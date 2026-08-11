[CmdletBinding()]
param(
    [string]$Manifest = 'tools/readme_media_manifest.json',
    [string]$ProjectNumber,
    [switch]$All,
    [switch]$SkipGif,
    [switch]$KeepWindows,
    [string]$OutputDir,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'readme_media_common.ps1')

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
if (-not ('ReadmeCaptureWin32' -as [type])) {
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class ReadmeCaptureWin32 {
  public const uint WM_KEYDOWN = 0x0100;
  public const uint WM_KEYUP = 0x0101;
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
$WM_LBUTTONDOWN = 0x0201
$WM_LBUTTONUP = 0x0202
$MK_LBUTTON = 0x0001
$PngSignature = [byte[]](0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A)
$VirtualKeyCodes = @{ W = 0x57; A = 0x41; S = 0x53; D = 0x44 }

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

function Get-CaptureOutputDetails {
    param(
        [string]$Path,
        [int]$ExpectedWidth,
        [int]$ExpectedHeight
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Capture output was not created: $Path"
    }

    $image = $null
    try {
        $image = [System.Drawing.Image]::FromFile($Path)
        if ($image.Width -ne $ExpectedWidth -or $image.Height -ne $ExpectedHeight) {
            throw "Capture output has unexpected dimensions $($image.Width)x$($image.Height): $Path"
        }

        return [pscustomobject]@{
            Dimensions = "$($image.Width)x$($image.Height)"
            Bytes = (Get-Item -LiteralPath $Path).Length
        }
    }
    finally {
        if ($null -ne $image) {
            $image.Dispose()
        }
    }
}

function Copy-ReadmeBackbufferPng {
    param(
        [string]$SourcePath,
        [string]$OutputPath,
        [int]$Width,
        [int]$Height,
        [int]$TimeoutSeconds = 3
    )

    # The application republishes this path continuously through an atomic
    # replace, so every open either finds nothing yet or finds a whole PNG. Retry
    # until the first frame lands, share the file with the writer, and copy the
    # decoded image into a detached bitmap so the tool's handle is gone - and the
    # publisher's next replace unblocked - before this returns.
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $lastError = 'no attempt was made'
    while ($true) {
        $stream = $null
        $image = $null
        $detached = $null
        $copied = $false
        try {
            $stream = [System.IO.File]::Open($SourcePath, [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
            $image = [System.Drawing.Image]::FromStream($stream, $false, $true)
            if ($image.Width -ne $Width -or $image.Height -ne $Height) {
                throw "backbuffer PNG has unexpected dimensions $($image.Width)x$($image.Height)"
            }
            $detached = New-Object System.Drawing.Bitmap($image)
            $detached.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
            $copied = $true
        }
        catch {
            $lastError = $_.Exception.Message
        }
        finally {
            if ($null -ne $detached) { $detached.Dispose() }
            if ($null -ne $image) { $image.Dispose() }
            if ($null -ne $stream) { $stream.Dispose() }
        }

        if ($copied) {
            return
        }
        if ((Get-Date) -ge $deadline) {
            throw "Backbuffer PNG was not published within $TimeoutSeconds seconds: $SourcePath ($lastError)"
        }
        Start-Sleep -Milliseconds 100
    }
}

function Remove-ReadmeBackbufferFile {
    param(
        [string]$Path,
        [string]$MediaDir
    )

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return
    }

    $resolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $resolvedMedia = (Resolve-Path -LiteralPath $MediaDir).Path
    $mediaWithSlash = $resolvedMedia.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolvedPath.StartsWith($mediaWithSlash, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove backbuffer output outside mediaDir: $resolvedPath"
    }

    Remove-Item -LiteralPath $resolvedPath -Force
}

function Get-CaptureProjectSelection {
    param(
        [object]$Manifest,
        [string]$ProjectNumber,
        [switch]$All
    )

    if ($All) {
        if (-not [string]::IsNullOrWhiteSpace($ProjectNumber)) {
            throw 'Specify either -All or -ProjectNumber, not both.'
        }
        return @($Manifest.projects)
    }

    if ([string]::IsNullOrWhiteSpace($ProjectNumber)) {
        throw 'Specify -All to capture every manifest project or -ProjectNumber to capture one project.'
    }

    $normalizedProjectNumber = if ($ProjectNumber -match '^\d+$') { ([int]$ProjectNumber).ToString('00') } else { $ProjectNumber }
    $selectedProjects = @($Manifest.projects | Where-Object { $_.number -eq $normalizedProjectNumber })
    if ($selectedProjects.Count -eq 0) {
        throw "Project number not found in manifest: $ProjectNumber"
    }

    return $selectedProjects
}

function Get-GifActionSchedule {
    param(
        [object[]]$Actions,
        [int]$FrameCount,
        [int]$FrameIntervalMs
    )

    $actionList = @($Actions | Where-Object { $null -ne $_ })
    $pending = @(
        for ($index = 0; $index -lt $actionList.Count; $index++) {
            [pscustomobject]@{
                Order = $index
                Action = $actionList[$index]
            }
        }
    ) | Sort-Object -Property @{ Expression = { [double]$_.Action.atMs }; Ascending = $true }, @{ Expression = { $_.Order }; Ascending = $true }

    $schedule = [System.Collections.Generic.List[object]]::new()
    $nextAction = 0
    for ($frameIndex = 0; $frameIndex -lt $FrameCount; $frameIndex++) {
        $frameTimeMs = $frameIndex * $FrameIntervalMs
        while ($nextAction -lt $pending.Count -and [double]$pending[$nextAction].Action.atMs -le $frameTimeMs) {
            $schedule.Add([pscustomobject]@{
                FrameIndex = $frameIndex
                FrameTimeMs = $frameTimeMs
                Action = $pending[$nextAction].Action
            })
            $nextAction++
        }
    }

    return $schedule.ToArray()
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
        param([IntPtr]$CandidateHandle)

        if ($CandidateHandle -eq [IntPtr]::Zero -or -not [ReadmeCaptureWin32]::IsWindowVisible($CandidateHandle)) {
            return
        }

        $candidateTitle = Get-WindowTitle -Handle $CandidateHandle
        if ($candidateTitle -eq 'Exception') {
            throw "Process main window is an exception dialog: $(Get-ProcessCaptureLabel -Process $Process)"
        }

        $candidateRect = New-Object ReadmeCaptureWin32+RECT
        if (-not [ReadmeCaptureWin32]::GetWindowRect($CandidateHandle, [ref]$candidateRect)) {
            return
        }

        $candidateWidth = $candidateRect.Right - $candidateRect.Left
        $candidateHeight = $candidateRect.Bottom - $candidateRect.Top
        if ($candidateWidth -le 32 -or $candidateHeight -le 32) {
            return
        }

        $candidates.Add([pscustomobject]@{
            Handle = $CandidateHandle
            Title = $candidateTitle
            Rect = $candidateRect
        })
    }

    $Process.Refresh()
    if ($Process.HasExited) {
        throw "Process exited before capture: $(Get-ProcessCaptureLabel -Process $Process)"
    }

    if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
        & $addCandidate ([IntPtr]$Process.MainWindowHandle)
    }

    $callback = [ReadmeCaptureWin32+EnumWindowsProc]{
        param([IntPtr]$WindowHandle, [IntPtr]$LParam)

        $windowProcessId = 0
        [void][ReadmeCaptureWin32]::GetWindowThreadProcessId($WindowHandle, [ref]$windowProcessId)
        if ($windowProcessId -eq $Process.Id) {
            & $addCandidate $WindowHandle
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

function Resize-CaptureWindowClient {
    param([IntPtr]$Handle, [int]$ClientWidth, [int]$ClientHeight)

    $window = New-Object ReadmeCaptureWin32+RECT
    $client = New-Object ReadmeCaptureWin32+RECT
    if (-not [ReadmeCaptureWin32]::GetWindowRect($Handle, [ref]$window) -or
        -not [ReadmeCaptureWin32]::GetClientRect($Handle, [ref]$client)) {
        throw 'unable to measure capture window'
    }
    $outerWidth = $ClientWidth + (($window.Right - $window.Left) - ($client.Right - $client.Left))
    $outerHeight = $ClientHeight + (($window.Bottom - $window.Top) - ($client.Bottom - $client.Top))
    if (-not [ReadmeCaptureWin32]::SetWindowPos($Handle, [IntPtr]::Zero, 40, 40, $outerWidth, $outerHeight, 0x0040)) {
        throw 'unable to resize capture window'
    }

    Start-Sleep -Milliseconds 100
    $resizedClient = New-Object ReadmeCaptureWin32+RECT
    if (-not [ReadmeCaptureWin32]::GetClientRect($Handle, [ref]$resizedClient)) {
        throw 'unable to remeasure capture window'
    }
    $actualWidth = $resizedClient.Right - $resizedClient.Left
    $actualHeight = $resizedClient.Bottom - $resizedClient.Top
    if ($actualWidth -ne $ClientWidth -or $actualHeight -ne $ClientHeight) {
        throw "capture client size mismatch: expected ${ClientWidth}x${ClientHeight}, found ${actualWidth}x${actualHeight}"
    }
}

function Focus-CaptureWindow {
    param([IntPtr]$Handle)

    [void][ReadmeCaptureWin32]::ShowWindow($Handle, 9)
    [void][ReadmeCaptureWin32]::SetForegroundWindow($Handle)
    [void][ReadmeCaptureWin32]::BringWindowToTop($Handle)
}

function Test-PreparedCaptureWindow {
    param(
        [System.Diagnostics.Process]$Process,
        [object]$CaptureSession
    )

    $Process.Refresh()
    if ($Process.HasExited) {
        throw "Process exited before capture: $(Get-ProcessCaptureLabel -Process $Process)"
    }

    $captureWindow = Resolve-CaptureWindow -Process $Process
    if ($captureWindow.Handle -ne $CaptureSession.Handle) {
        throw 'capture window changed during prepared capture sequence'
    }

    $client = New-Object ReadmeCaptureWin32+RECT
    if (-not [ReadmeCaptureWin32]::GetClientRect($CaptureSession.Handle, [ref]$client)) {
        throw 'unable to remeasure prepared capture client window'
    }
    $width = $client.Right - $client.Left
    $height = $client.Bottom - $client.Top
    if ($width -ne $CaptureSession.ClientWidth -or $height -ne $CaptureSession.ClientHeight) {
        throw "prepared client size changed: expected $($CaptureSession.ClientWidth)x$($CaptureSession.ClientHeight), found ${width}x${height}"
    }

    return $CaptureSession.Rect
}

function Prepare-CaptureWindow {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$ClientWidth,
        [int]$ClientHeight,
        [string]$BackbufferPath
    )

    $session = $null
    try {
        $captureWindow = Resolve-CaptureWindow -Process $Process
        Resize-CaptureWindowClient -Handle $captureWindow.Handle -ClientWidth $ClientWidth -ClientHeight $ClientHeight
        $captureWindow = Resolve-CaptureWindow -Process $Process
        $rect = Get-CaptureClientRect -Handle $captureWindow.Handle -FallbackRect $captureWindow.Rect
        $width = $rect.Right - $rect.Left
        $height = $rect.Bottom - $rect.Top
        if ($width -ne $ClientWidth -or $height -ne $ClientHeight) {
            throw "prepared client size mismatch: expected ${ClientWidth}x${ClientHeight}, found ${width}x${height}"
        }

        $topmostFlags = [uint32]($SWP_NOMOVE -bor $SWP_NOSIZE -bor $SWP_SHOWWINDOW)
        if (-not [ReadmeCaptureWin32]::SetWindowPos($captureWindow.Handle, $HWND_TOPMOST, 0, 0, 0, 0, $topmostFlags)) {
            throw 'SetWindowPos failed while making window topmost'
        }
        $session = [pscustomobject]@{
            Handle = $captureWindow.Handle
            ClientWidth = $ClientWidth
            ClientHeight = $ClientHeight
            Rect = $rect
            TopmostFlags = $topmostFlags
            BackbufferPath = $BackbufferPath
        }
        Focus-CaptureWindow -Handle $captureWindow.Handle
        Start-Sleep -Milliseconds 300
        $session.Rect = Test-PreparedCaptureWindow -Process $Process -CaptureSession $session
        return $session
    }
    catch {
        if ($null -ne $session) {
            Restore-CaptureWindow -CaptureSession $session
        }
        throw
    }
}

function Restore-CaptureWindow {
    param([object]$CaptureSession)

    if ($null -ne $CaptureSession -and $CaptureSession.Handle -ne [IntPtr]::Zero) {
        [void][ReadmeCaptureWin32]::SetWindowPos($CaptureSession.Handle, $HWND_NOTOPMOST, 0, 0, 0, 0, [uint32]$CaptureSession.TopmostFlags)
    }
}

function Capture-PreparedWindowPng {
    param(
        [System.Diagnostics.Process]$Process,
        [object]$CaptureSession,
        [string]$OutputPath
    )

    $rect = Test-PreparedCaptureWindow -Process $Process -CaptureSession $CaptureSession

    if (-not [string]::IsNullOrWhiteSpace($CaptureSession.BackbufferPath)) {
        # Opted-in projects publish their own swap-chain frames, so the capture is
        # the frame the application actually rendered - immune to an overlapping
        # window, a screensaver, or a DPI quirk.
        Copy-ReadmeBackbufferPng -SourcePath $CaptureSession.BackbufferPath -OutputPath $OutputPath `
            -Width $CaptureSession.ClientWidth -Height $CaptureSession.ClientHeight
    }
    else {
        # Every project without the opt-in flag keeps the screen-copy path.
        $bitmap = $null
        $graphics = $null
        try {
            $bitmap = New-Object System.Drawing.Bitmap($CaptureSession.ClientWidth, $CaptureSession.ClientHeight)
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
            $bitmap.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            if ($null -ne $graphics) {
                $graphics.Dispose()
            }
            if ($null -ne $bitmap) {
                $bitmap.Dispose()
            }
        }
    }

    Test-PngOutput -Path $OutputPath
    [void](Get-CaptureOutputDetails -Path $OutputPath -ExpectedWidth $CaptureSession.ClientWidth -ExpectedHeight $CaptureSession.ClientHeight)
}

function Send-CaptureWindowMessage {
    param([IntPtr]$Handle, [uint32]$Message, [IntPtr]$WParam, [IntPtr]$LParam)

    [void][ReadmeCaptureWin32]::PostMessage($Handle, $Message, $WParam, $LParam)
}

function Invoke-CaptureAction {
    param(
        [object]$CaptureSession,
        [object]$Action,
        [System.Collections.Generic.HashSet[string]]$PressedKeys,
        [switch]$AllowWait,
        [scriptblock]$MessageSink
    )

    if ($null -eq $MessageSink) {
        $MessageSink = ${function:Send-CaptureWindowMessage}
    }

    switch ([string]$Action.type) {
        'wait' {
            if ($AllowWait) {
                Start-Sleep -Milliseconds ([int]$Action.durationMs)
            }
        }
        'click' {
            $clientX = [int][Math]::Round(([double]$Action.x) * ($CaptureSession.ClientWidth - 1))
            $clientY = [int][Math]::Round(([double]$Action.y) * ($CaptureSession.ClientHeight - 1))
            $lParam = [IntPtr]((($clientY -band 0xffff) -shl 16) -bor ($clientX -band 0xffff))
            & $MessageSink $CaptureSession.Handle ([uint32]$WM_LBUTTONDOWN) ([IntPtr]$MK_LBUTTON) $lParam
            & $MessageSink $CaptureSession.Handle ([uint32]$WM_LBUTTONUP) ([IntPtr]::Zero) $lParam
        }
        'keyDown' {
            $key = [string]$Action.key
            $keyCode = [IntPtr]$VirtualKeyCodes[$key]
            & $MessageSink $CaptureSession.Handle ([ReadmeCaptureWin32]::WM_KEYDOWN) $keyCode ([IntPtr]::Zero)
            [void]$PressedKeys.Add($key)
        }
        'keyUp' {
            $key = [string]$Action.key
            $keyCode = [IntPtr]$VirtualKeyCodes[$key]
            & $MessageSink $CaptureSession.Handle ([ReadmeCaptureWin32]::WM_KEYUP) $keyCode ([IntPtr]::Zero)
            [void]$PressedKeys.Remove($key)
        }
        default {
            throw "unsupported capture action: $($Action.type)"
        }
    }
}

function Release-CaptureKeys {
    param(
        [object]$CaptureSession,
        [System.Collections.Generic.HashSet[string]]$PressedKeys,
        [scriptblock]$MessageSink
    )

    if ($null -eq $CaptureSession -or $CaptureSession.Handle -eq [IntPtr]::Zero) {
        return
    }
    if ($null -eq $MessageSink) {
        $MessageSink = ${function:Send-CaptureWindowMessage}
    }

    foreach ($key in @($PressedKeys)) {
        $keyCode = [IntPtr]$VirtualKeyCodes[$key]
        & $MessageSink $CaptureSession.Handle ([ReadmeCaptureWin32]::WM_KEYUP) $keyCode ([IntPtr]::Zero)
    }
    $PressedKeys.Clear()
}

function Invoke-PreCaptureActions {
    param(
        [object]$CaptureSession,
        [object]$Project,
        [System.Collections.Generic.HashSet[string]]$PressedKeys
    )

    foreach ($action in @($Project.preCaptureActions)) {
        if ($null -eq $action) {
            continue
        }
        Invoke-CaptureAction -CaptureSession $CaptureSession -Action $action -PressedKeys $PressedKeys -AllowWait
    }
}

function Wait-MainWindow {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$TimeoutMs = 15000
    )

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    do {
        $Process.Refresh()
        if ($Process.HasExited) {
            throw "Process exited before a main window appeared: $(Get-ProcessCaptureLabel -Process $Process)"
        }

        if ($Process.MainWindowHandle -ne [IntPtr]::Zero) {
            return
        }

        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)

    throw "Timed out waiting for a main window: $(Get-ProcessCaptureLabel -Process $Process)"
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
        [int]$Attempt,
        [string]$Output,
        [string]$Status,
        [string]$Dimensions,
        [object]$Bytes,
        [string]$Notes
    )

    $Rows.Add([pscustomobject]@{
        Project = $Project.number
        Attempt = $Attempt
        Exe = $Project.exe
        Output = $Output
        Status = $Status
        Dimensions = $Dimensions
        Bytes = $Bytes
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
    $lines.Add('| Project | Attempt | Exe | Output | Status | Dimensions | Bytes | Notes |')
    $lines.Add('|---|---:|---|---|---|---|---:|---|')

    foreach ($row in $Rows) {
        $line = '| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} |' -f `
            (Format-MarkdownCell $row.Project),
            (Format-MarkdownCell $row.Attempt),
            (Format-MarkdownCell $row.Exe),
            (Format-MarkdownCell $row.Output),
            (Format-MarkdownCell $row.Status),
            (Format-MarkdownCell $row.Dimensions),
            (Format-MarkdownCell $row.Bytes),
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

function Invoke-GifEncode {
    param(
        [string]$FfmpegPath,
        [string]$InputPattern,
        [string]$GifPath,
        [int]$GifFps,
        [int]$GifWidth,
        [int]$GifHeight,
        [int]$MaxColors
    )

    $filter = "fps=$GifFps,scale=$GifWidth`:$GifHeight`:flags=lanczos,split[s0][s1];" +
        "[s0]palettegen=max_colors=${MaxColors}:stats_mode=diff[p];" +
        '[s1][p]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle'
    $arguments = @(
        '-y',
        '-framerate', $GifFps,
        '-i', $InputPattern,
        '-filter_complex', $filter,
        '-loop', '0',
        $GifPath
    )

    $ffmpegOutput = & $FfmpegPath @arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        $joinedOutput = ($ffmpegOutput | ForEach-Object { [string]$_ }) -join ' '
        throw "ffmpeg failed with exit code ${LASTEXITCODE}: $joinedOutput"
    }
}

function Invoke-PresentationPanGif {
    param(
        [string]$FfmpegPath,
        [string]$PngPath,
        [string]$GifPath,
        [int]$GifFps,
        [int]$GifWidth,
        [int]$GifHeight,
        [double]$GifSeconds,
        [int]$MaxColors,
        [int64]$GifMaxBytes = 5242880
    )

    Test-PngOutput -Path $PngPath
    $frameCount = [Math]::Max(1, [int][Math]::Ceiling($GifSeconds * $GifFps))
    $durationText = $GifSeconds.ToString([System.Globalization.CultureInfo]::InvariantCulture)
    $scaledWidth = $GifWidth + 8
    $scaledHeight = $GifHeight + 4
    $colorAttempts = @($MaxColors, 128, 96 | Select-Object -Unique)
    $details = $null

    foreach ($colorCount in $colorAttempts) {
        $filter = "scale=$scaledWidth`:$scaledHeight`:flags=lanczos," +
            "crop=$GifWidth`:$GifHeight`:x='4+4*sin(2*PI*n/$frameCount)':y='2+2*cos(2*PI*n/$frameCount)'," +
            "fps=$GifFps,split[s0][s1];" +
            "[s0]palettegen=max_colors=${colorCount}:stats_mode=full[p];" +
            '[s1][p]paletteuse=dither=sierra2_4a'
        $arguments = @(
            '-y',
            '-loop', '1',
            '-framerate', $GifFps,
            '-t', $durationText,
            '-i', $PngPath,
            '-frames:v', $frameCount,
            '-filter_complex', $filter,
            '-loop', '0',
            '-gifflags', '0',
            $GifPath
        )

        $ffmpegOutput = & $FfmpegPath @arguments 2>&1
        if ($LASTEXITCODE -ne 0) {
            $joinedOutput = ($ffmpegOutput | ForEach-Object { [string]$_ }) -join ' '
            throw "ffmpeg presentation pan failed with exit code ${LASTEXITCODE}: $joinedOutput"
        }

        $details = Get-CaptureOutputDetails -Path $GifPath -ExpectedWidth $GifWidth -ExpectedHeight $GifHeight
        if ($details.Bytes -le $GifMaxBytes) {
            break
        }
    }

    if ($details.Bytes -gt $GifMaxBytes) {
        throw "Presentation-pan GIF exceeds $GifMaxBytes byte limit: $($details.Bytes) bytes"
    }

    $details | Add-Member -NotePropertyName SourceFrameDurationMs -NotePropertyValue 0
    $details | Add-Member -NotePropertyName PresentationPan -NotePropertyValue $true
    return $details
}

function Invoke-GifCapture {
    param(
        [System.Diagnostics.Process]$Process,
        [object]$Project,
        [object]$ManifestData,
        [string]$MediaDir,
        [string]$GifPath,
        [string]$RepoRoot,
        [object]$CaptureSession,
        [System.Collections.Generic.HashSet[string]]$PressedKeys
    )

    $ffmpegPath = 'C:\ffmpeg\bin\ffmpeg.exe'
    if (-not (Test-Path -LiteralPath $ffmpegPath)) {
        throw "ffmpeg not found: $ffmpegPath"
    }

    $gifSeconds = Get-ReadmeMediaEffectivePositiveNumber -Manifest $ManifestData -Project $Project -Name 'gifSeconds'
    $gifFps = [int](Get-ReadmeMediaEffectivePositiveNumber -Manifest $ManifestData -Project $Project -Name 'gifFps')
    $gifWidth = [int]$ManifestData.gifWidth
    $gifHeight = [int]$ManifestData.gifHeight
    $gifMaxBytes = [int64](Get-ReadmeMediaEffectivePositiveNumber -Manifest $ManifestData -Project $Project -Name 'gifMaxBytes')
    $frameCount = [Math]::Max(1, [int][Math]::Ceiling($gifSeconds * $gifFps))
    $frameIntervalMs = [int][Math]::Round(1000.0 / $gifFps)
    $framesDir = Join-Path $MediaDir ('frames-{0}-{1}' -f $Project.number, [Guid]::NewGuid().ToString('N'))
    $scheduledActions = @(Get-GifActionSchedule -Actions @($Project.gifActions) -FrameCount $frameCount -FrameIntervalMs $frameIntervalMs)
    $nextScheduledAction = 0

    New-Item -ItemType Directory -Path $framesDir -Force | Out-Null
    try {
        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        for ($index = 0; $index -lt $frameCount; $index++) {
            $frameTargetMs = $index * $frameIntervalMs
            while ($stopwatch.ElapsedMilliseconds -lt $frameTargetMs) {
                Start-Sleep -Milliseconds ([Math]::Min(10, $frameTargetMs - $stopwatch.ElapsedMilliseconds))
            }

            while ($nextScheduledAction -lt $scheduledActions.Count -and $scheduledActions[$nextScheduledAction].FrameIndex -eq $index) {
                Invoke-CaptureAction -CaptureSession $CaptureSession -Action $scheduledActions[$nextScheduledAction].Action -PressedKeys $PressedKeys
                $nextScheduledAction++
            }

            $framePath = Join-Path $framesDir ('frame_{0:D4}.png' -f $index)
            Capture-PreparedWindowPng -Process $Process -CaptureSession $CaptureSession -OutputPath $framePath
        }
        $sourceFrameDurationMs = $stopwatch.ElapsedMilliseconds

        $inputPattern = Join-Path $framesDir 'frame_%04d.png'
        Invoke-GifEncode -FfmpegPath $ffmpegPath -InputPattern $inputPattern -GifPath $GifPath -GifFps $gifFps -GifWidth $gifWidth -GifHeight $gifHeight -MaxColors 128
        $details = Get-CaptureOutputDetails -Path $GifPath -ExpectedWidth $gifWidth -ExpectedHeight $gifHeight
        if ($details.Bytes -gt $gifMaxBytes) {
            Invoke-GifEncode -FfmpegPath $ffmpegPath -InputPattern $inputPattern -GifPath $GifPath -GifFps $gifFps -GifWidth $gifWidth -GifHeight $gifHeight -MaxColors 96
            $details = Get-CaptureOutputDetails -Path $GifPath -ExpectedWidth $gifWidth -ExpectedHeight $gifHeight
        }
        if ($details.Bytes -gt $gifMaxBytes) {
            throw "GIF exceeds $gifMaxBytes byte limit after palette retry: $($details.Bytes) bytes"
        }

        $details | Add-Member -NotePropertyName SourceFrameDurationMs -NotePropertyValue $sourceFrameDurationMs
        return $details
    }
    finally {
        Remove-FrameDirectory -FramesDir $framesDir -MediaDir $MediaDir
    }
}

function Stop-CaptureProcess {
    param([System.Diagnostics.Process]$Process)

    if ($null -eq $Process) {
        return
    }

    try {
        $Process.Refresh()
        if (-not $Process.HasExited) {
            Stop-Process -Id $Process.Id -Force
            [void]$Process.WaitForExit(5000)
        }
    }
    finally {
        $Process.Dispose()
    }
}

function Invoke-ProjectCapture {
    param(
        [object]$Project,
        [object]$ManifestData,
        [string]$RuntimeDir,
        [string]$MediaDir,
        [string]$RepoRoot,
        [int]$Attempt,
        [switch]$SkipGif,
        [switch]$KeepWindows
    )

    $process = $null
    $captureCompleted = $false
    $lastCaptureSession = $null
    $pressedKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $previousReadmeCaptureEnv = $env:DX11_README_CAPTURE
    $previousReadmeBackbufferEnv = $env:DX11_README_BACKBUFFER_PNG
    $readmeCaptureEnvChanged = $false
    $readmeBackbufferEnvChanged = $false
    $backbufferPath = $null
    $usePresentationPan = [bool]$Project.gifPresentationPan
    $imagePath = Resolve-ReadmeMediaContainedPath -BasePath $MediaDir -Path $Project.image -Description "project $($Project.number) image output"
    $gifPath = Resolve-ReadmeMediaContainedPath -BasePath $MediaDir -Path $Project.gif -Description "project $($Project.number) GIF output"
    try {
        if (-not (Test-Path -LiteralPath $RuntimeDir -PathType Container)) {
            throw "Runtime directory not found: $(Convert-ToReportPath $RuntimeDir $RepoRoot)"
        }

        $exePath = Resolve-ReadmeMediaContainedPath -BasePath $RuntimeDir -Path $Project.exe -Description "project $($Project.number) executable"
        if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
            throw "Executable not found: $(Convert-ToReportPath $exePath $RepoRoot)"
        }

        if ([bool]$Project.readmeCaptureMode) {
            $env:DX11_README_CAPTURE = '1'
            $readmeCaptureEnvChanged = $true
        }

        if ([bool]$Project.readmeBackbufferCapture) {
            # A unique name per attempt, contained inside the chosen media
            # directory so the cleanup below can prove it is removing a path this
            # run created.
            $backbufferPath = Resolve-ReadmeMediaContainedPath -BasePath $MediaDir `
                -Path ('backbuffer-{0}-{1}.png' -f $Project.number, [Guid]::NewGuid().ToString('N')) `
                -Description "project $($Project.number) backbuffer output"
            $env:DX11_README_BACKBUFFER_PNG = $backbufferPath
            $readmeBackbufferEnvChanged = $true
        }

        $process = Start-Process -FilePath $exePath -WorkingDirectory $RuntimeDir -PassThru
        if ($readmeCaptureEnvChanged) {
            if ($null -eq $previousReadmeCaptureEnv) {
                Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue
            }
            else {
                $env:DX11_README_CAPTURE = $previousReadmeCaptureEnv
            }
            $readmeCaptureEnvChanged = $false
        }
        if ($readmeBackbufferEnvChanged) {
            if ($null -eq $previousReadmeBackbufferEnv) {
                Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue
            }
            else {
                $env:DX11_README_BACKBUFFER_PNG = $previousReadmeBackbufferEnv
            }
            $readmeBackbufferEnvChanged = $false
        }

        Wait-MainWindow -Process $process

        if ($Project.gifPhase -eq 'startup' -and -not $SkipGif -and -not $usePresentationPan) {
            $startupSession = Prepare-CaptureWindow -Process $process -ClientWidth ([int]$ManifestData.captureWidth) -ClientHeight ([int]$ManifestData.captureHeight) -BackbufferPath $backbufferPath
            $lastCaptureSession = $startupSession
            try {
                $gifDetails = Invoke-GifCapture -Process $process -Project $Project -ManifestData $ManifestData -MediaDir $MediaDir -GifPath $gifPath -RepoRoot $RepoRoot -CaptureSession $startupSession -PressedKeys $pressedKeys
            }
            finally {
                Restore-CaptureWindow -CaptureSession $startupSession
            }
        }

        $delayMs = [int]$Project.delayMs
        if ($delayMs -gt 0) {
            Start-Sleep -Milliseconds $delayMs
        }

        $captureSession = Prepare-CaptureWindow -Process $process -ClientWidth ([int]$ManifestData.captureWidth) -ClientHeight ([int]$ManifestData.captureHeight) -BackbufferPath $backbufferPath
        $lastCaptureSession = $captureSession
        try {
            Invoke-PreCaptureActions -CaptureSession $captureSession -Project $Project -PressedKeys $pressedKeys
            Capture-PreparedWindowPng -Process $process -CaptureSession $captureSession -OutputPath $imagePath
            $imageDetails = Get-CaptureOutputDetails -Path $imagePath -ExpectedWidth ([int]$ManifestData.captureWidth) -ExpectedHeight ([int]$ManifestData.captureHeight)

            if ($usePresentationPan -and -not $SkipGif) {
                $effectiveGifFps = [int](Get-ReadmeMediaEffectivePositiveNumber -Manifest $ManifestData -Project $Project -Name 'gifFps')
                $effectiveGifSeconds = Get-ReadmeMediaEffectivePositiveNumber -Manifest $ManifestData -Project $Project -Name 'gifSeconds'
                $effectiveGifMaxBytes = [int64](Get-ReadmeMediaEffectivePositiveNumber -Manifest $ManifestData -Project $Project -Name 'gifMaxBytes')
                $gifDetails = Invoke-PresentationPanGif -FfmpegPath 'C:\ffmpeg\bin\ffmpeg.exe' -PngPath $imagePath -GifPath $gifPath -GifFps $effectiveGifFps -GifWidth ([int]$ManifestData.gifWidth) -GifHeight ([int]$ManifestData.gifHeight) -GifSeconds $effectiveGifSeconds -MaxColors 256 -GifMaxBytes $effectiveGifMaxBytes
            }
            elseif ($Project.gifPhase -eq 'runtime' -and -not $SkipGif) {
                $gifDetails = Invoke-GifCapture -Process $process -Project $Project -ManifestData $ManifestData -MediaDir $MediaDir -GifPath $gifPath -RepoRoot $RepoRoot -CaptureSession $captureSession -PressedKeys $pressedKeys
            }
        }
        finally {
            Restore-CaptureWindow -CaptureSession $captureSession
        }

        $captureCompleted = $true
        return [pscustomobject]@{
            ImagePath = $imagePath
            ImageDetails = $imageDetails
            GifPath = $gifPath
            GifDetails = $gifDetails
        }
    }
    finally {
        Release-CaptureKeys -CaptureSession $lastCaptureSession -PressedKeys $pressedKeys
        if ($readmeCaptureEnvChanged) {
            if ($null -eq $previousReadmeCaptureEnv) {
                Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue
            }
            else {
                $env:DX11_README_CAPTURE = $previousReadmeCaptureEnv
            }
        }
        if ($readmeBackbufferEnvChanged) {
            if ($null -eq $previousReadmeBackbufferEnv) {
                Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue
            }
            else {
                $env:DX11_README_BACKBUFFER_PNG = $previousReadmeBackbufferEnv
            }
        }
        if ($null -ne $process -and (-not $KeepWindows -or -not $captureCompleted)) {
            Stop-CaptureProcess -Process $process
        }
        # After the publisher is down, so it cannot recreate what is removed here.
        # The application clears its own temporary sibling when it shuts down, but
        # this run may have killed it mid-publication, and nothing but the media
        # directory's own capture output may be left behind.
        if (-not [string]::IsNullOrWhiteSpace($backbufferPath)) {
            Remove-ReadmeBackbufferFile -Path $backbufferPath -MediaDir $MediaDir
            Remove-ReadmeBackbufferFile -Path ($backbufferPath + '.tmp.png') -MediaDir $MediaDir
        }
    }
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$manifestData = Get-ReadmeMediaManifest -ManifestPath $Manifest -RepoRoot $repoRoot
$manifestErrors = @(Test-ReadmeMediaManifest -Manifest $manifestData -RepoRoot $repoRoot)
if ($manifestErrors.Count -gt 0) {
    throw "Capture manifest validation failed: $($manifestErrors -join '; ')"
}
if ($ValidateOnly) {
    'capture manifest validation passed'
    return
}

$runtimeDir = Resolve-ReadmeMediaPath -RepoRoot $repoRoot -Path $manifestData.runtimeDir
$mediaDir = if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    Resolve-ReadmeMediaPath -RepoRoot $repoRoot -Path $manifestData.mediaDir
}
else {
    Resolve-ReadmeMediaPath -RepoRoot $repoRoot -Path $OutputDir
}
New-Item -ItemType Directory -Path $mediaDir -Force | Out-Null

$selectedProjects = @(Get-CaptureProjectSelection -Manifest $manifestData -ProjectNumber $ProjectNumber -All:$All)

$reportRows = New-Object System.Collections.Generic.List[object]
foreach ($project in $selectedProjects) {
    $projectSucceeded = $false
    $attemptLimit = [int]$manifestData.captureAttempts
    for ($attempt = 1; $attempt -le $attemptLimit -and -not $projectSucceeded; $attempt++) {
        Write-Host ("Capturing project {0}, attempt {1}/{2}: {3}" -f $project.number, $attempt, $attemptLimit, $project.exe)
        try {
            $result = Invoke-ProjectCapture -Project $project -ManifestData $manifestData -RuntimeDir $runtimeDir -MediaDir $mediaDir -RepoRoot $repoRoot -Attempt $attempt -SkipGif:$SkipGif -KeepWindows:$KeepWindows
            Add-ReportRow -Rows $reportRows -Project $project -Attempt $attempt -Output (Convert-ToReportPath $result.ImagePath $repoRoot) -Status 'Success' -Dimensions $result.ImageDetails.Dimensions -Bytes $result.ImageDetails.Bytes -Notes 'PNG captured'
            if (-not $SkipGif) {
                $gifNotes = if ([bool]$result.GifDetails.PresentationPan) {
                    'GIF generated from the captured PNG with the reproducible presentation-pan stage'
                }
                else {
                    "GIF captured; source frames collected in $($result.GifDetails.SourceFrameDurationMs) ms"
                }
                Add-ReportRow -Rows $reportRows -Project $project -Attempt $attempt -Output (Convert-ToReportPath $result.GifPath $repoRoot) -Status 'Success' -Dimensions $result.GifDetails.Dimensions -Bytes $result.GifDetails.Bytes -Notes $gifNotes
            }
            else {
                Add-ReportRow -Rows $reportRows -Project $project -Attempt $attempt -Output (Convert-ToReportPath $result.GifPath $repoRoot) -Status 'Skipped' -Dimensions '' -Bytes '' -Notes 'GIF skipped by -SkipGif'
            }
            $projectSucceeded = $true
        }
        catch {
            Add-ReportRow -Rows $reportRows -Project $project -Attempt $attempt -Output $project.exe -Status 'Failure' -Dimensions '' -Bytes '' -Notes $_.Exception.Message
        }
    }
}

$reportPath = Join-Path $mediaDir 'capture-report.md'
Write-CaptureReport -Rows $reportRows -ReportPath $reportPath
Write-Host "Capture report written to $(Convert-ToReportPath -Path $reportPath -RepoRoot $repoRoot)"

$failedProjects = @(
    foreach ($project in $selectedProjects) {
        $projectSuccessRows = @($reportRows | Where-Object { $_.Project -eq $project.number -and $_.Status -eq 'Success' })
        if ($projectSuccessRows.Count -eq 0) {
            $project
        }
    }
)
if ($failedProjects.Count -gt 0) {
    throw "README media capture failed for project(s): $($failedProjects.number -join ', ')"
}
