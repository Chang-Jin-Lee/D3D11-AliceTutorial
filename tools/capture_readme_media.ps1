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
using System.Text;
public static class ReadmeCaptureWin32 {
  public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out int lpdwProcessId);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
  [DllImport("user32.dll")] public static extern int GetWindowTextLength(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int X, int Y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, UIntPtr dwExtraInfo);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
  public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public struct POINT { public int X; public int Y; }
}
"@
}
try {
  [void][ReadmeCaptureWin32]::SetProcessDPIAware()
}
catch {
}

$HWND_TOPMOST = [IntPtr]::new(-1)
$HWND_NOTOPMOST = [IntPtr]::new(-2)
$SWP_NOSIZE = 0x0001
$SWP_NOMOVE = 0x0002
$SWP_SHOWWINDOW = 0x0040
$MOUSEEVENTF_LEFTDOWN = 0x0002
$MOUSEEVENTF_LEFTUP = 0x0004
$WM_LBUTTONDOWN = 0x0201
$WM_LBUTTONUP = 0x0202
$MK_LBUTTON = 0x0001
$PngSignature = [byte[]](0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A)

function Test-PngOutput {
  param([string]$Path)

  if (-not (Test-Path -LiteralPath $Path)) {
    throw "PNG capture did not create output: $Path"
  }

  $item = Get-Item -LiteralPath $Path
  if ($item.Length -le 4096) {
    throw "PNG capture output is too small to be valid: $($item.Length) bytes"
  }

  $stream = [System.IO.File]::OpenRead($Path)
  try {
    if ($stream.Length -lt $PngSignature.Length) {
      throw "PNG capture output is too small to contain a PNG signature: $($stream.Length) bytes"
    }

    $header = New-Object byte[] $PngSignature.Length
    $bytesRead = $stream.Read($header, 0, $header.Length)
    if ($bytesRead -ne $PngSignature.Length) {
      throw "PNG capture output could not be read: $Path"
    }

    for ($index = 0; $index -lt $PngSignature.Length; $index++) {
      if ($header[$index] -ne $PngSignature[$index]) {
        throw "PNG capture output has an invalid PNG signature: $Path"
      }
    }
  }
  finally {
    $stream.Dispose()
  }
}

function Get-WindowTitle {
  param([IntPtr]$Handle)

  $length = [ReadmeCaptureWin32]::GetWindowTextLength($Handle)
  if ($length -le 0) {
    return ''
  }

  $builder = New-Object System.Text.StringBuilder($length + 1)
  [void][ReadmeCaptureWin32]::GetWindowText($Handle, $builder, $builder.Capacity)
  return $builder.ToString()
}

function Get-ProcessCaptureLabel {
  param([System.Diagnostics.Process]$Process)

  if ($null -eq $Process) {
    return 'unknown process'
  }

  try {
    if (-not [string]::IsNullOrWhiteSpace($Process.ProcessName)) {
      return $Process.ProcessName
    }
  }
  catch {
  }

  try {
    return "pid $($Process.Id)"
  }
  catch {
    return 'unknown process'
  }
}

function Resolve-CaptureWindow {
  param([System.Diagnostics.Process]$Process)

  $candidates = New-Object System.Collections.Generic.List[object]
  $addCandidate = {
    param([IntPtr]$candidateHandle)

    if ($candidateHandle -eq [IntPtr]::Zero) {
      return
    }

    if (-not [ReadmeCaptureWin32]::IsWindowVisible($candidateHandle)) {
      return
    }

    $candidateTitle = Get-WindowTitle -Handle $candidateHandle
    if ($candidateTitle -eq 'Exception') {
      throw "Process main window is an exception dialog: $(Get-ProcessCaptureLabel -Process $Process)"
    }

    $candidateRect = New-Object ReadmeCaptureWin32+RECT
    if (-not [ReadmeCaptureWin32]::GetWindowRect($candidateHandle, [ref]$candidateRect)) {
      return
    }

    $candidateWidth = $candidateRect.Right - $candidateRect.Left
    $candidateHeight = $candidateRect.Bottom - $candidateRect.Top
    if ($candidateWidth -le 32 -or $candidateHeight -le 32) {
      return
    }

    $candidates.Add([pscustomobject]@{
      Handle = $candidateHandle
      Title = $candidateTitle
      Rect = $candidateRect
      Width = $candidateWidth
      Height = $candidateHeight
    })
  }

  $Process.Refresh()
  if ($Process.HasExited) {
    throw "Process exited before capture: $(Get-ProcessCaptureLabel -Process $Process)"
  }

  if ($null -ne $Process.MainWindowHandle) {
    $mainWindowHandle = [IntPtr]$Process.MainWindowHandle
    & $addCandidate $mainWindowHandle
  }

  $callback = [ReadmeCaptureWin32+EnumWindowsProc]{
    param([IntPtr]$windowHandle, [IntPtr]$lParam)

    $windowProcessId = 0
    [void][ReadmeCaptureWin32]::GetWindowThreadProcessId($windowHandle, [ref]$windowProcessId)
    if ($windowProcessId -eq $Process.Id) {
      & $addCandidate $windowHandle
    }

    return $true
  }

  [void][ReadmeCaptureWin32]::EnumWindows($callback, [IntPtr]::Zero)

  if ($candidates.Count -eq 0) {
    throw "Process has no capturable main window: $(Get-ProcessCaptureLabel -Process $Process)"
  }

  $preferred = @($candidates | Where-Object { $_.Title -eq 'GameApp' } | Select-Object -First 1)
  if ($preferred.Count -eq 0) {
    $preferred = @($candidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_.Title) } | Select-Object -First 1)
  }

  if ($preferred.Count -eq 0) {
    return $candidates[0]
  }

  return $preferred[0]
}

function Get-CaptureClientRect {
  param(
    [IntPtr]$Handle,
    [object]$FallbackRect
  )

  $clientRect = New-Object ReadmeCaptureWin32+RECT
  if ([ReadmeCaptureWin32]::GetClientRect($Handle, [ref]$clientRect)) {
    $clientWidth = $clientRect.Right - $clientRect.Left
    $clientHeight = $clientRect.Bottom - $clientRect.Top
    if ($clientWidth -gt 32 -and $clientHeight -gt 32) {
      $origin = New-Object ReadmeCaptureWin32+POINT
      $origin.X = 0
      $origin.Y = 0
      if ([ReadmeCaptureWin32]::ClientToScreen($Handle, [ref]$origin)) {
        return [pscustomobject]@{
          Left = $origin.X
          Top = $origin.Y
          Right = $origin.X + $clientWidth
          Bottom = $origin.Y + $clientHeight
        }
      }
    }
  }

  return $FallbackRect
}

function Capture-WindowPng {
  param([System.Diagnostics.Process]$Process, [string]$OutputPath)
  $Process.Refresh()
  if ($Process.HasExited) {
    throw "Process exited before capture: $(Get-ProcessCaptureLabel -Process $Process)"
  }

  if ($Process.MainWindowTitle -eq 'Exception') {
    throw "Process main window is an exception dialog: $(Get-ProcessCaptureLabel -Process $Process)"
  }

  $captureWindow = Resolve-CaptureWindow -Process $Process
  $handle = $captureWindow.Handle
  $rect = Get-CaptureClientRect -Handle $handle -FallbackRect $captureWindow.Rect
  $width = $rect.Right - $rect.Left
  $height = $rect.Bottom - $rect.Top
  if ($width -le 32 -or $height -le 32) { throw "Window rectangle too small: ${width}x${height}" }
  $bitmap = $null
  $graphics = $null
  $topmostFlags = [uint32]($SWP_NOMOVE -bor $SWP_NOSIZE -bor $SWP_SHOWWINDOW)
  try {
    if (-not [ReadmeCaptureWin32]::SetWindowPos($handle, $HWND_TOPMOST, 0, 0, 0, 0, $topmostFlags)) {
      throw "SetWindowPos failed while making window topmost"
    }

    $showWindowResult = [ReadmeCaptureWin32]::ShowWindow($handle, 9)
    $lastSetForegroundResult = $false
    $foregrounded = $false
    $foregroundDeadline = (Get-Date).AddMilliseconds(500)
    do {
      $lastSetForegroundResult = [ReadmeCaptureWin32]::SetForegroundWindow($handle)
      [void][ReadmeCaptureWin32]::BringWindowToTop($handle)
      Start-Sleep -Milliseconds 100
      if ([ReadmeCaptureWin32]::GetForegroundWindow() -eq $handle) {
        $foregrounded = $true
        break
      }
    } while ((Get-Date) -lt $foregroundDeadline)

    if (-not $foregrounded) {
      $foregroundHandle = [ReadmeCaptureWin32]::GetForegroundWindow()
      Write-Warning ("Continuing after foreground request was denied for process window: {0} (ShowWindow returned {1}; SetForegroundWindow returned {2}; target handle 0x{3}; foreground handle 0x{4})" -f `
        (Get-ProcessCaptureLabel -Process $Process),
        $showWindowResult,
        $lastSetForegroundResult,
        $handle.ToInt64().ToString('X'),
        $foregroundHandle.ToInt64().ToString('X'))
    }

    [void][ReadmeCaptureWin32]::BringWindowToTop($handle)
    Start-Sleep -Milliseconds 300

    $bitmap = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
    $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    Test-PngOutput -Path $OutputPath
  }
  finally {
    if ($handle -ne [IntPtr]::Zero) {
      [void][ReadmeCaptureWin32]::SetWindowPos($handle, $HWND_NOTOPMOST, 0, 0, 0, 0, $topmostFlags)
    }
    if ($null -ne $graphics) {
      $graphics.Dispose()
    }
    if ($null -ne $bitmap) {
      $bitmap.Dispose()
    }
  }
}

function Invoke-ProjectStartInteraction {
  param(
    [System.Diagnostics.Process]$Process,
    [object]$Project
  )

  $startClick = [bool](Get-JsonPropertyValue -Object $Project -Name 'startClick' -DefaultValue $false)
  if (-not $startClick) {
    return
  }

  $Process.Refresh()
  if ($Process.HasExited) {
    throw "Process exited before start interaction: $(Get-ProcessCaptureLabel -Process $Process)"
  }

  $captureWindow = Resolve-CaptureWindow -Process $Process
  $handle = $captureWindow.Handle
  $rect = Get-CaptureClientRect -Handle $handle -FallbackRect $captureWindow.Rect
  $clientRect = New-Object ReadmeCaptureWin32+RECT
  $clientX = [int](($rect.Right - $rect.Left) / 2)
  $clientY = [int](($rect.Bottom - $rect.Top) / 2)
  if ([ReadmeCaptureWin32]::GetClientRect($handle, [ref]$clientRect)) {
    $clientX = [int](($clientRect.Right - $clientRect.Left) / 2)
    $clientY = [int](($clientRect.Bottom - $clientRect.Top) / 2)
  }
  $x = [int](($rect.Left + $rect.Right) / 2)
  $y = [int](($rect.Top + $rect.Bottom) / 2)

  [void][ReadmeCaptureWin32]::ShowWindow($handle, 9)
  [void][ReadmeCaptureWin32]::SetForegroundWindow($handle)
  [void][ReadmeCaptureWin32]::BringWindowToTop($handle)
  Start-Sleep -Milliseconds 200

  [void][ReadmeCaptureWin32]::SetCursorPos($x, $y)
  Start-Sleep -Milliseconds 100
  [ReadmeCaptureWin32]::mouse_event([uint32]$MOUSEEVENTF_LEFTDOWN, [uint32]$x, [uint32]$y, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 60
  [ReadmeCaptureWin32]::mouse_event([uint32]$MOUSEEVENTF_LEFTUP, [uint32]$x, [uint32]$y, 0, [UIntPtr]::Zero)

  $lParam = [IntPtr]((($clientY -band 0xffff) -shl 16) -bor ($clientX -band 0xffff))
  [void][ReadmeCaptureWin32]::PostMessage($handle, [uint32]$WM_LBUTTONDOWN, [IntPtr]$MK_LBUTTON, $lParam)
  Start-Sleep -Milliseconds 60
  [void][ReadmeCaptureWin32]::PostMessage($handle, [uint32]$WM_LBUTTONUP, [IntPtr]::Zero, $lParam)

  $postStartDelayMs = [int](Get-JsonPropertyValue -Object $Project -Name 'postStartDelayMs' -DefaultValue 1000)
  if ($postStartDelayMs -gt 0) {
    Start-Sleep -Milliseconds $postStartDelayMs
  }
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
        $line = '| {0} | {1} | {2} | {3} | {4} |' -f `
            (Format-MarkdownCell $row.Project),
            (Format-MarkdownCell $row.Exe),
            (Format-MarkdownCell $row.Output),
            (Format-MarkdownCell $row.Status),
            (Format-MarkdownCell $row.Notes)
        $lines.Add($line)
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
    $previousReadmeCaptureEnv = $env:DX11_README_CAPTURE
    $readmeCaptureEnvChanged = $false
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

        $readmeCaptureMode = [bool](Get-JsonPropertyValue -Object $project -Name 'readmeCaptureMode' -DefaultValue $false)
        if ($readmeCaptureMode) {
            $env:DX11_README_CAPTURE = '1'
            $readmeCaptureEnvChanged = $true
        }

        $process = Start-Process -FilePath $exePath -WorkingDirectory $runtimeDir -PassThru
        if ($readmeCaptureEnvChanged) {
            if ($null -eq $previousReadmeCaptureEnv) {
                Remove-Item Env:\\DX11_README_CAPTURE -ErrorAction SilentlyContinue
            }
            else {
                $env:DX11_README_CAPTURE = $previousReadmeCaptureEnv
            }
            $readmeCaptureEnvChanged = $false
        }
        Wait-MainWindow -Process $process

        $delayMs = [int](Get-JsonPropertyValue -Object $project -Name 'delayMs' -DefaultValue $manifestData.defaultDelayMs)
        if ($delayMs -gt 0) {
            Start-Sleep -Milliseconds $delayMs
        }

        Invoke-ProjectStartInteraction -Process $process -Project $project

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
        if ($readmeCaptureEnvChanged) {
            if ($null -eq $previousReadmeCaptureEnv) {
                Remove-Item Env:\\DX11_README_CAPTURE -ErrorAction SilentlyContinue
            }
            else {
                $env:DX11_README_CAPTURE = $previousReadmeCaptureEnv
            }
        }

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

$failedRows = @($reportRows | Where-Object { $_.Status -eq 'Failure' })
if ($failedRows.Count -gt 0) {
    $failedProjects = @($failedRows | ForEach-Object { $_.Project } | Select-Object -Unique) -join ', '
    throw "README media capture completed with $($failedRows.Count) failure row(s) for project(s): $failedProjects"
}
