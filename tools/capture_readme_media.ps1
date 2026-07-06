[CmdletBinding()]
param(
    [string]$Manifest = 'tools/readme_media_manifest.json',
    [string]$ProjectNumber,
    [switch]$All,
    [switch]$SkipGif,
    [switch]$KeepWindows
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
if (-not ('ReadmeCaptureWin32' -as [type])) {
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class ReadmeCaptureWin32 {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
}
"@
}

function Capture-WindowPng {
  param([System.Diagnostics.Process]$Process, [string]$OutputPath)
  $handle = $Process.MainWindowHandle
  if ($handle -eq [IntPtr]::Zero) { throw "Process has no main window: $($Process.ProcessName)" }
  [ReadmeCaptureWin32]::ShowWindow($handle, 5) | Out-Null
  [ReadmeCaptureWin32]::SetForegroundWindow($handle) | Out-Null
  Start-Sleep -Milliseconds 300
  $rect = New-Object ReadmeCaptureWin32+RECT
  if (-not [ReadmeCaptureWin32]::GetWindowRect($handle, [ref]$rect)) { throw "GetWindowRect failed" }
  $width = $rect.Right - $rect.Left
  $height = $rect.Bottom - $rect.Top
  if ($width -le 32 -or $height -le 32) { throw "Window rectangle too small: ${width}x${height}" }
  $bitmap = New-Object System.Drawing.Bitmap($width, $height)
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
  $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
  $graphics.Dispose()
  $bitmap.Dispose()
}

function Resolve-ExistingInputPath {
    param(
        [string]$Path,
        [string]$RepoRoot
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return (Resolve-Path -LiteralPath $Path).Path
    }

    $repoCandidate = Join-Path $RepoRoot $Path
    if (Test-Path -LiteralPath $repoCandidate) {
        return (Resolve-Path -LiteralPath $repoCandidate).Path
    }

    $cwdCandidate = Join-Path (Get-Location).Path $Path
    if (Test-Path -LiteralPath $cwdCandidate) {
        return (Resolve-Path -LiteralPath $cwdCandidate).Path
    }

    throw "Path not found: $Path"
}

function Resolve-RepoPath {
    param(
        [string]$Path,
        [string]$RepoRoot
    )

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Path))
}

function Get-JsonPropertyValue {
    param(
        [object]$Object,
        [string]$Name,
        [object]$DefaultValue
    )

    $property = $Object.PSObject.Properties[$Name]
    if ($null -ne $property -and $null -ne $property.Value) {
        return $property.Value
    }

    return $DefaultValue
}

function Normalize-ProjectNumber {
    param([string]$Value)

    $parsed = 0
    if ([int]::TryParse($Value, [ref]$parsed)) {
        return $parsed.ToString('00')
    }

    return $Value
}

function Wait-MainWindow {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$TimeoutMs = 15000
    )

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    do {
        if ($Process.HasExited) {
            throw "Process exited before a main window appeared: $($Process.ProcessName)"
        }

        $Process.Refresh()
        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            return
        }

        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)

    throw "Timed out waiting for a main window: $($Process.ProcessName)"
}

function Convert-ToReportPath {
    param(
        [string]$Path,
        [string]$RepoRoot
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $rootWithSlash = $RepoRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if ($fullPath.StartsWith($rootWithSlash, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath.Substring($rootWithSlash.Length).Replace('\', '/')
    }

    return $fullPath.Replace('\', '/')
}

function Format-MarkdownCell {
    param([object]$Value)

    if ($null -eq $Value) {
        return ''
    }

    return ([string]$Value).Replace("`r", ' ').Replace("`n", ' ').Replace('|', '\|')
}

function Add-ReportRow {
    param(
        [System.Collections.Generic.List[object]]$Rows,
        [object]$Project,
        [string]$Output,
        [string]$Status,
        [string]$Notes
    )

    $Rows.Add([pscustomobject]@{
        Project = $Project.number
        Exe = $Project.exe
        Output = $Output
        Status = $Status
        Notes = $Notes
    })
}

function Write-CaptureReport {
    param(
        [System.Collections.Generic.List[object]]$Rows,
        [string]$ReportPath
    )

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add('# README media capture report')
    $lines.Add('')
    $lines.Add("Generated: $((Get-Date).ToString('yyyy-MM-dd HH:mm:ss'))")
    $lines.Add('')
    $lines.Add('| Project | Exe | Output | Status | Notes |')
    $lines.Add('|---|---|---|---|---|')

    foreach ($row in $Rows) {
        $lines.Add('| {0} | {1} | {2} | {3} | {4} |' -f `
            (Format-MarkdownCell $row.Project),
            (Format-MarkdownCell $row.Exe),
            (Format-MarkdownCell $row.Output),
            (Format-MarkdownCell $row.Status),
            (Format-MarkdownCell $row.Notes))
    }

    Set-Content -LiteralPath $ReportPath -Value $lines -Encoding UTF8
}

function Remove-FrameDirectory {
    param(
        [string]$FramesDir,
        [string]$MediaDir
    )

    if (-not (Test-Path -LiteralPath $FramesDir)) {
        return
    }

    $resolvedFrames = (Resolve-Path -LiteralPath $FramesDir).Path
    $resolvedMedia = (Resolve-Path -LiteralPath $MediaDir).Path
    $mediaWithSlash = $resolvedMedia.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $frameLeaf = Split-Path -Leaf $resolvedFrames

    if (-not $resolvedFrames.StartsWith($mediaWithSlash, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove frames directory outside mediaDir: $resolvedFrames"
    }

    if (-not $frameLeaf.StartsWith('frames-', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove unexpected frames directory: $resolvedFrames"
    }

    Remove-Item -LiteralPath $resolvedFrames -Recurse -Force
}

function Invoke-GifCapture {
    param(
        [System.Diagnostics.Process]$Process,
        [object]$Project,
        [string]$MediaDir,
        [string]$GifPath,
        [string]$RepoRoot
    )

    $ffmpegPath = 'C:\ffmpeg\bin\ffmpeg.exe'
    if (-not (Test-Path -LiteralPath $ffmpegPath)) {
        throw "ffmpeg not found: $ffmpegPath"
    }

    $gifSeconds = [double](Get-JsonPropertyValue $Project 'gifSeconds' 4)
    $gifFps = [int](Get-JsonPropertyValue $Project 'gifFps' 10)
    if ($gifSeconds -le 0) { throw "gifSeconds must be greater than zero for project $($Project.number)" }
    if ($gifFps -le 0) { throw "gifFps must be greater than zero for project $($Project.number)" }

    $frameCount = [Math]::Max(1, [int][Math]::Ceiling($gifSeconds * $gifFps))
    $frameIntervalMs = [Math]::Max(1, [int][Math]::Round(1000 / $gifFps))
    $framesDir = Join-Path $MediaDir ('frames-{0}-{1}' -f $Project.number, [Guid]::NewGuid().ToString('N'))

    New-Item -ItemType Directory -Path $framesDir -Force | Out-Null
    try {
        for ($index = 0; $index -lt $frameCount; $index++) {
            $framePath = Join-Path $framesDir ('frame_{0:D4}.png' -f $index)
            Capture-WindowPng -Process $Process -OutputPath $framePath
            if ($index -lt ($frameCount - 1)) {
                Start-Sleep -Milliseconds $frameIntervalMs
            }
        }

        $inputPattern = Join-Path $framesDir 'frame_%04d.png'
        $filter = 'fps={0},scale=960:-1:flags=lanczos' -f $gifFps
        $arguments = @(
            '-y',
            '-framerate', $gifFps,
            '-i', $inputPattern,
            '-vf', $filter,
            '-loop', '0',
            $GifPath
        )

        $ffmpegOutput = & $ffmpegPath @arguments 2>&1
        $exitCode = $LASTEXITCODE
        if ($exitCode -ne 0) {
            $joinedOutput = ($ffmpegOutput | ForEach-Object { [string]$_ }) -join ' '
            throw "ffmpeg failed with exit code ${exitCode}: $joinedOutput"
        }

        if (-not (Test-Path -LiteralPath $GifPath)) {
            throw "ffmpeg did not create GIF: $(Convert-ToReportPath $GifPath $RepoRoot)"
        }

        if ((Get-Item -LiteralPath $GifPath).Length -le 0) {
            throw "ffmpeg created an empty GIF: $(Convert-ToReportPath $GifPath $RepoRoot)"
        }
    }
    finally {
        Remove-FrameDirectory -FramesDir $framesDir -MediaDir $MediaDir
    }
}

if (-not $All -and [string]::IsNullOrWhiteSpace($ProjectNumber)) {
    throw "Specify -All to capture every manifest project or -ProjectNumber to capture one project."
}

if ($All -and -not [string]::IsNullOrWhiteSpace($ProjectNumber)) {
    throw "Specify either -All or -ProjectNumber, not both."
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$manifestPath = Resolve-ExistingInputPath -Path $Manifest -RepoRoot $repoRoot
$manifestData = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$runtimeDir = Resolve-RepoPath -Path $manifestData.runtimeDir -RepoRoot $repoRoot
$mediaDir = Resolve-RepoPath -Path $manifestData.mediaDir -RepoRoot $repoRoot

New-Item -ItemType Directory -Path $mediaDir -Force | Out-Null

$projects = @($manifestData.projects)
if ($All) {
    $selectedProjects = $projects
}
else {
    $normalizedProjectNumber = Normalize-ProjectNumber -Value $ProjectNumber
    $selectedProjects = @($projects | Where-Object { $_.number -eq $normalizedProjectNumber })
    if ($selectedProjects.Count -eq 0) {
        throw "Project number not found in manifest: $ProjectNumber"
    }
}

$reportRows = New-Object System.Collections.Generic.List[object]

foreach ($project in $selectedProjects) {
    $process = $null
    $imagePath = Join-Path $mediaDir $project.image
    $imageReportPath = Convert-ToReportPath -Path $imagePath -RepoRoot $repoRoot
    $exePath = Join-Path $runtimeDir $project.exe

    Write-Host ("Capturing project {0}: {1}" -f $project.number, $project.exe)

    try {
        if (-not (Test-Path -LiteralPath $runtimeDir)) {
            throw "Runtime directory not found: $(Convert-ToReportPath $runtimeDir $repoRoot)"
        }

        if (-not (Test-Path -LiteralPath $exePath)) {
            throw "Executable not found: $(Convert-ToReportPath $exePath $repoRoot)"
        }

        $process = Start-Process -FilePath $exePath -WorkingDirectory $runtimeDir -PassThru
        Wait-MainWindow -Process $process

        $delayMs = [int](Get-JsonPropertyValue -Object $project -Name 'delayMs' -DefaultValue $manifestData.defaultDelayMs)
        if ($delayMs -gt 0) {
            Start-Sleep -Milliseconds $delayMs
        }

        $process.Refresh()
        Capture-WindowPng -Process $process -OutputPath $imagePath
        Add-ReportRow -Rows $reportRows -Project $project -Output $imageReportPath -Status 'Success' -Notes 'PNG captured'

        $gifName = Get-JsonPropertyValue -Object $project -Name 'gif' -DefaultValue $null
        if ($gifName -and -not $SkipGif) {
            $gifPath = Join-Path $mediaDir $gifName
            $gifReportPath = Convert-ToReportPath -Path $gifPath -RepoRoot $repoRoot
            try {
                Invoke-GifCapture -Process $process -Project $project -MediaDir $mediaDir -GifPath $gifPath -RepoRoot $repoRoot
                Add-ReportRow -Rows $reportRows -Project $project -Output $gifReportPath -Status 'Success' -Notes 'GIF captured'
            }
            catch {
                Add-ReportRow -Rows $reportRows -Project $project -Output $gifReportPath -Status 'Failure' -Notes "GIF failed: $($_.Exception.Message)"
            }
        }
        elseif ($gifName -and $SkipGif) {
            Add-ReportRow -Rows $reportRows -Project $project -Output (Convert-ToReportPath -Path (Join-Path $mediaDir $gifName) -RepoRoot $repoRoot) -Status 'Skipped' -Notes 'GIF skipped by -SkipGif'
        }
    }
    catch {
        Add-ReportRow -Rows $reportRows -Project $project -Output $imageReportPath -Status 'Failure' -Notes $_.Exception.Message
    }
    finally {
        if ($null -ne $process -and -not $KeepWindows) {
            try {
                if (-not $process.HasExited) {
                    Stop-Process -Id $process.Id -Force
                }
            }
            catch {
                Add-ReportRow -Rows $reportRows -Project $project -Output $project.exe -Status 'Failure' -Notes "Failed to stop launched process $($process.Id): $($_.Exception.Message)"
            }
        }
    }
}

$reportPath = Join-Path $mediaDir 'capture-report.md'
Write-CaptureReport -Rows $reportRows -ReportPath $reportPath
Write-Host "Capture report written to $(Convert-ToReportPath -Path $reportPath -RepoRoot $repoRoot)"
