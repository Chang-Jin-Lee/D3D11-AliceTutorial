# Observable-behavior test for the Project 36 capture-only portfolio showcase.
#
# This test launches the built executable and inspects the frames it actually
# renders. It deliberately does NOT grep the C++ sources: source text asserts the
# shape of the code, not the behaviour, and passes whether or not the feature
# works. Every requirement below is verified through rendered pixels, process
# liveness, or file content.
[CmdletBinding()]
param(
    [switch]$KeepFrames
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script:Failures = [System.Collections.Generic.List[string]]::new()

function Assert-True {
    param([bool]$Condition, [string]$Message)

    if (-not $Condition) {
        $script:Failures.Add($Message)
    }
    else {
        Write-Host "  ok   $Message"
    }
}

Add-Type -AssemblyName System.Drawing

if (-not ('ShowcaseWin32' -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class ShowcaseWin32 {
  public const uint WM_LBUTTONDOWN = 0x0201;
  public const uint WM_LBUTTONUP = 0x0202;
  public const int MK_LBUTTON = 0x0001;
  public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out int lpdwProcessId);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
  [DllImport("user32.dll")] public static extern int GetWindowTextLength(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
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

if (-not ('ShowcaseFrame' -as [type])) {
    # Pure array math, deliberately free of any System.Drawing dependency: the
    # caller hands over the raw 32bpp scan lines, so this compiles the same way
    # on Windows PowerShell and on PowerShell 7 (where System.Drawing lives in
    # System.Drawing.Common and drags in extra reference assemblies).
    Add-Type @"
using System;

// A captured client-area frame reduced to 8-bit luminance so comparisons stay
// fast enough to run inside a PowerShell test.
public class ShowcaseFrame {
  public int Width;
  public int Height;
  public byte[] Pixels;

  public static ShowcaseFrame FromBgra(byte[] buffer, int stride, int width, int height) {
    ShowcaseFrame frame = new ShowcaseFrame();
    frame.Width = width;
    frame.Height = height;
    frame.Pixels = new byte[width * height];
    for (int y = 0; y < height; ++y) {
      int rowBase = y * stride;
      int outBase = y * width;
      for (int x = 0; x < width; ++x) {
        int b = buffer[rowBase + x * 4 + 0];
        int g = buffer[rowBase + x * 4 + 1];
        int r = buffer[rowBase + x * 4 + 2];
        frame.Pixels[outBase + x] = (byte)((r * 77 + g * 151 + b * 28) >> 8);
      }
    }
    return frame;
  }

  private static void Clip(ShowcaseFrame f, ref int x, ref int y, ref int w, ref int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > f.Width) { w = f.Width - x; }
    if (y + h > f.Height) { h = f.Height - y; }
    if (w < 0) { w = 0; }
    if (h < 0) { h = 0; }
  }

  // Fraction of pixels in the region whose luminance differs by more than
  // 'tolerance'. 0.0 means the two frames render the region identically.
  public static double DiffRatio(ShowcaseFrame a, ShowcaseFrame b, int x, int y, int w, int h, int tolerance) {
    if (a.Width != b.Width || a.Height != b.Height) { return 1.0; }
    Clip(a, ref x, ref y, ref w, ref h);
    if (w == 0 || h == 0) { return 0.0; }
    long changed = 0;
    for (int yy = y; yy < y + h; ++yy) {
      int rowBase = yy * a.Width;
      for (int xx = x; xx < x + w; ++xx) {
        int delta = a.Pixels[rowBase + xx] - b.Pixels[rowBase + xx];
        if (delta < 0) { delta = -delta; }
        if (delta > tolerance) { ++changed; }
      }
    }
    return (double)changed / (double)(w * h);
  }

  // Fraction of pixels in the region at or above 'threshold' luminance. Used to
  // prove that legible light-on-dark HUD text was actually rasterised.
  public static double BrightRatio(ShowcaseFrame a, int x, int y, int w, int h, int threshold) {
    Clip(a, ref x, ref y, ref w, ref h);
    if (w == 0 || h == 0) { return 0.0; }
    long bright = 0;
    for (int yy = y; yy < y + h; ++yy) {
      int rowBase = yy * a.Width;
      for (int xx = x; xx < x + w; ++xx) {
        if (a.Pixels[rowBase + xx] >= threshold) { ++bright; }
      }
    }
    return (double)bright / (double)(w * h);
  }

  // Splits the band [y0, y1) into fixed-width columns, marks a column as moving
  // when enough of its pixels change across any pair of the supplied frames,
  // then returns the number of separated clusters of moving columns.
  public static int MovingColumnClusters(ShowcaseFrame[] frames, int y0, int y1,
      int columnWidth, int tolerance, double minChangedFraction, int minGapColumns) {
    if (frames == null || frames.Length < 2) { return 0; }
    int width = frames[0].Width;
    int columns = width / columnWidth;
    bool[] moving = new bool[columns];
    int bandHeight = y1 - y0;
    for (int c = 0; c < columns; ++c) {
      int x = c * columnWidth;
      double best = 0.0;
      for (int i = 0; i + 1 < frames.Length; ++i) {
        for (int j = i + 1; j < frames.Length; ++j) {
          double ratio = DiffRatio(frames[i], frames[j], x, y0, columnWidth, bandHeight, tolerance);
          if (ratio > best) { best = ratio; }
        }
      }
      moving[c] = best >= minChangedFraction;
    }

    int clusters = 0;
    int gap = minGapColumns;
    for (int c = 0; c < columns; ++c) {
      if (moving[c]) {
        if (gap >= minGapColumns) { ++clusters; }
        gap = 0;
      }
      else {
        ++gap;
      }
    }
    return clusters;
  }
}
"@
}

try { [void][ShowcaseWin32]::SetProcessDPIAware() } catch { }

$HWND_TOPMOST = [IntPtr]::new(-1)
$HWND_NOTOPMOST = [IntPtr]::new(-2)
$SWP_NOSIZE = 0x0001
$SWP_NOMOVE = 0x0002
$SWP_SHOWWINDOW = 0x0040

$manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools/readme_media_manifest.json') | ConvertFrom-Json
$project36 = @($manifest.projects | Where-Object { $_.number -eq '36' })[0]
if ($null -eq $project36) { throw 'project 36 is missing from tools/readme_media_manifest.json' }

$clientWidth = [int]$manifest.captureWidth
$clientHeight = [int]$manifest.captureHeight
$runtimeDir = Join-Path $repoRoot ([string]$manifest.runtimeDir)
$exePath = Join-Path $runtimeDir ([string]$project36.exe)
$projectDir = Join-Path $repoRoot 'Dx11/36_AdvancedAnim_Sound_Click'
$msbuildCommand = "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1"

# The eight-second showcase cycle, as the runtime defines it.
$phaseDanceSampleA = 1.0
$phaseDanceSampleB = 2.3
$phaseBlendSample = 4.0
$phaseIkSample = 6.3

# The HUD is a fixed-geometry window anchored at client (24,24).
$hudX = 28
$hudY = 28
$hudW = 312
$hudH = 56

# Second HUD line ("4 CHARACTERS / LIVE PALETTES") is phase invariant, so it is
# the sharpest capture-gate probe: identical in every capture frame, and a leak
# into normal mode would reproduce it pixel for pixel.
$castX = 28
$castY = 52
$castW = 290
$castH = 18
$pixelTolerance = 12

# ---------------------------------------------------------------------------
# 1. The built binary must exist and must be newer than the sources it is built
#    from. Never silently skip.
# ---------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "Built executable not found: $exePath`nBuild it first:`n$msbuildCommand"
}

$sourceFiles = @()
$sourceFiles += Get-ChildItem -LiteralPath $projectDir -File |
    Where-Object { $_.Extension -in @('.cpp', '.h', '.inl', '.vcxproj', '.filters', '.hlsl', '.hlsli', '.fxh') }
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Dx11/Common/Animation') -File |
    Where-Object { $_.Extension -in @('.cpp', '.h') }

$exeWriteTime = (Get-Item -LiteralPath $exePath).LastWriteTimeUtc
$staleSources = @($sourceFiles | Where-Object { $_.LastWriteTimeUtc -gt $exeWriteTime })
if ($staleSources.Count -gt 0) {
    $names = ($staleSources | ForEach-Object { $_.Name }) -join ', '
    throw "Built executable is older than its sources ($names).`nRebuild it first:`n$msbuildCommand"
}

Write-Host "  ok   built binary is present and newer than its sources"

# ---------------------------------------------------------------------------
# 2. Rights boundary. A provenance assertion about the bytes that ship, not a
#    change detector: the Project 36 deliverables must contain no trace of the
#    legacy licensed model, its named dance clips, or its audio.
# ---------------------------------------------------------------------------
$forbiddenTokens = @(
    'NIKKE',
    'Alice_.fbx',
    'CaramellaDansen',
    'RabbitHole',
    'Specialist',
    'CaliforniaGirls'
)

$deliverables = @()
$deliverables += Get-ChildItem -LiteralPath $projectDir -File -Recurse |
    Where-Object { $_.Extension -in @('.cpp', '.h', '.inl', '.md', '.vcxproj', '.filters', '.hlsl', '.hlsli', '.fxh') }
$deliverables += Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Dx11/Resource/fbx/Public/MyAlice/Animations') -File |
    Where-Object { $_.Name -like 'anim_Portfolio*' }

foreach ($deliverable in $deliverables) {
    $bytes = [System.IO.File]::ReadAllBytes($deliverable.FullName)
    $asAscii = [System.Text.Encoding]::ASCII.GetString($bytes)
    foreach ($token in $forbiddenTokens) {
        if ($asAscii.IndexOf($token, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
            $script:Failures.Add("legacy rights boundary violated: '$token' found in $($deliverable.Name)")
        }
    }
}
Write-Host "  ok   rights boundary scan covered $($deliverables.Count) Project 36 deliverables"

# ---------------------------------------------------------------------------
# Runtime helpers
# ---------------------------------------------------------------------------
function Get-ShowcaseWindowTitle {
    param([IntPtr]$Handle)

    $length = [ShowcaseWin32]::GetWindowTextLength($Handle)
    if ($length -le 0) { return '' }
    $builder = New-Object System.Text.StringBuilder($length + 1)
    [void][ShowcaseWin32]::GetWindowText($Handle, $builder, $builder.Capacity)
    return $builder.ToString()
}

function Resolve-ShowcaseWindow {
    param([System.Diagnostics.Process]$Process)

    $candidates = New-Object System.Collections.Generic.List[object]
    $addCandidate = {
        param([IntPtr]$CandidateHandle)

        if ($CandidateHandle -eq [IntPtr]::Zero -or -not [ShowcaseWin32]::IsWindowVisible($CandidateHandle)) { return }
        $rect = New-Object ShowcaseWin32+RECT
        if (-not [ShowcaseWin32]::GetWindowRect($CandidateHandle, [ref]$rect)) { return }
        if (($rect.Right - $rect.Left) -le 32 -or ($rect.Bottom - $rect.Top) -le 32) { return }
        $candidates.Add([pscustomobject]@{ Handle = $CandidateHandle; Title = (Get-ShowcaseWindowTitle -Handle $CandidateHandle) })
    }

    $Process.Refresh()
    if ($Process.MainWindowHandle -ne [IntPtr]::Zero) { & $addCandidate ([IntPtr]$Process.MainWindowHandle) }

    $callback = [ShowcaseWin32+EnumWindowsProc]{
        param([IntPtr]$WindowHandle, [IntPtr]$LParam)

        $windowProcessId = 0
        [void][ShowcaseWin32]::GetWindowThreadProcessId($WindowHandle, [ref]$windowProcessId)
        if ($windowProcessId -eq $Process.Id) { & $addCandidate $WindowHandle }
        return $true
    }
    [void][ShowcaseWin32]::EnumWindows($callback, [IntPtr]::Zero)

    if ($candidates.Count -eq 0) { return $null }
    $preferred = @($candidates | Where-Object { $_.Title -eq 'GameApp' } | Select-Object -First 1)
    if ($preferred.Count -eq 0) { $preferred = @($candidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_.Title) } | Select-Object -First 1) }
    if ($preferred.Count -eq 0) { return $candidates[0] }
    return $preferred[0]
}

function Wait-ShowcaseWindow {
    param([System.Diagnostics.Process]$Process, [int]$TimeoutMs = 30000)

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    do {
        $Process.Refresh()
        if ($Process.HasExited) { throw "process exited before a main window appeared (exit $($Process.ExitCode))" }
        $window = Resolve-ShowcaseWindow -Process $Process
        if ($null -ne $window) { return $window }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)

    throw 'timed out waiting for the application main window'
}

function Set-ShowcaseClientSize {
    param([IntPtr]$Handle, [int]$Width, [int]$Height)

    for ($attempt = 0; $attempt -lt 5; ++$attempt) {
        $window = New-Object ShowcaseWin32+RECT
        $client = New-Object ShowcaseWin32+RECT
        if ([ShowcaseWin32]::GetWindowRect($Handle, [ref]$window) -and [ShowcaseWin32]::GetClientRect($Handle, [ref]$client)) {
            $outerWidth = $Width + (($window.Right - $window.Left) - ($client.Right - $client.Left))
            $outerHeight = $Height + (($window.Bottom - $window.Top) - ($client.Bottom - $client.Top))
            [void][ShowcaseWin32]::SetWindowPos($Handle, [IntPtr]::Zero, 40, 40, $outerWidth, $outerHeight, [uint32]$SWP_SHOWWINDOW)
            Start-Sleep -Milliseconds 200
            $resized = New-Object ShowcaseWin32+RECT
            if ([ShowcaseWin32]::GetClientRect($Handle, [ref]$resized)) {
                if (($resized.Right - $resized.Left) -eq $Width -and ($resized.Bottom - $resized.Top) -eq $Height) { return }
            }
        }
        Start-Sleep -Milliseconds 250
    }

    throw "unable to resize the application client area to ${Width}x${Height}"
}

function Set-ShowcaseTopmost {
    param([IntPtr]$Handle)

    $flags = [uint32]($SWP_NOMOVE -bor $SWP_NOSIZE -bor $SWP_SHOWWINDOW)
    [void][ShowcaseWin32]::SetWindowPos($Handle, $HWND_TOPMOST, 0, 0, 0, 0, $flags)
    [void][ShowcaseWin32]::ShowWindow($Handle, 9)
    [void][ShowcaseWin32]::SetForegroundWindow($Handle)
    [void][ShowcaseWin32]::BringWindowToTop($Handle)
}

function Reset-ShowcaseTopmost {
    param([IntPtr]$Handle)

    if ($Handle -eq [IntPtr]::Zero) { return }
    $flags = [uint32]($SWP_NOMOVE -bor $SWP_NOSIZE -bor $SWP_SHOWWINDOW)
    [void][ShowcaseWin32]::SetWindowPos($Handle, $HWND_NOTOPMOST, 0, 0, 0, 0, $flags)
}

function Get-ShowcaseClientOrigin {
    param([IntPtr]$Handle)

    $origin = New-Object ShowcaseWin32+POINT
    if (-not [ShowcaseWin32]::ClientToScreen($Handle, [ref]$origin)) { throw 'unable to map the client origin to screen coordinates' }
    return $origin
}

function Save-ShowcaseFrame {
    param([IntPtr]$Handle, [int]$Width, [int]$Height, [string]$Path)

    $origin = Get-ShowcaseClientOrigin -Handle $Handle
    $bitmap = $null
    $graphics = $null
    $bitmapData = $null
    try {
        $bitmap = New-Object System.Drawing.Bitmap($Width, $Height)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, $bitmap.Size)
        if (-not [string]::IsNullOrEmpty($Path)) {
            $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        }

        $rect = New-Object System.Drawing.Rectangle(0, 0, $Width, $Height)
        $bitmapData = $bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $byteCount = $bitmapData.Stride * $Height
        $buffer = New-Object byte[] $byteCount
        [System.Runtime.InteropServices.Marshal]::Copy($bitmapData.Scan0, $buffer, 0, $byteCount)
        return [ShowcaseFrame]::FromBgra($buffer, $bitmapData.Stride, $Width, $Height)
    }
    finally {
        if ($null -ne $bitmapData -and $null -ne $bitmap) { $bitmap.UnlockBits($bitmapData) }
        if ($null -ne $graphics) { $graphics.Dispose() }
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

function Send-ShowcaseClick {
    param([IntPtr]$Handle, [int]$ClientX, [int]$ClientY)

    $lParam = [IntPtr]((($ClientY -band 0xffff) -shl 16) -bor ($ClientX -band 0xffff))
    [void][ShowcaseWin32]::PostMessage($Handle, [ShowcaseWin32]::WM_LBUTTONDOWN, [IntPtr][ShowcaseWin32]::MK_LBUTTON, $lParam)
    Start-Sleep -Milliseconds 40
    [void][ShowcaseWin32]::PostMessage($Handle, [ShowcaseWin32]::WM_LBUTTONUP, [IntPtr]::Zero, $lParam)
}

# The loading screen clears the whole back buffer to pure black; the live scene
# fills the corners with the skybox. That makes corner brightness an independent
# readiness signal that does not depend on the feature under test.
function Wait-ShowcaseScene {
    param(
        [System.Diagnostics.Process]$Process,
        [IntPtr]$Handle,
        [int]$Width,
        [int]$Height,
        [string]$ProbePath,
        [switch]$ClickToStart,
        [int]$TimeoutMs = 120000
    )

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    do {
        $Process.Refresh()
        if ($Process.HasExited) { throw "process exited while waiting for the scene (exit $($Process.ExitCode))" }
        if ($ClickToStart) { Send-ShowcaseClick -Handle $Handle -ClientX ([int]($Width / 2)) -ClientY ([int]($Height / 2)) }
        Start-Sleep -Milliseconds 700
        $frame = Save-ShowcaseFrame -Handle $Handle -Width $Width -Height $Height -Path $ProbePath
        # Bottom-left corner: outside the centred loading window in every state.
        $lit = [ShowcaseFrame]::BrightRatio($frame, 0, $Height - 140, 160, 120, 24)
        if ($lit -ge 0.5) { return }
    } while ((Get-Date) -lt $deadline)

    throw 'timed out waiting for the live scene to replace the loading screen'
}

function Stop-ShowcaseProcess {
    param([System.Diagnostics.Process]$Process)

    if ($null -eq $Process) { return }
    try {
        $Process.Refresh()
        if (-not $Process.HasExited) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
            [void]$Process.WaitForExit(8000)
        }
    }
    catch { }
}

$frameDir = Join-Path ([System.IO.Path]::GetTempPath()) ("dx11-project36-showcase-{0}" -f $PID)
New-Item -ItemType Directory -Path $frameDir -Force | Out-Null

$previousCaptureEnv = $env:DX11_README_CAPTURE
$captureProcess = $null
$normalProcess = $null
$captureHandle = [IntPtr]::Zero
$normalHandle = [IntPtr]::Zero

try {
    # -----------------------------------------------------------------------
    # 3. Capture-mode run: showcase HUD plus the deterministic phase cycle.
    # -----------------------------------------------------------------------
    $env:DX11_README_CAPTURE = '1'
    $captureProcess = Start-Process -FilePath $exePath -WorkingDirectory $runtimeDir -PassThru
    if ($null -eq $previousCaptureEnv) { Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue }
    else { $env:DX11_README_CAPTURE = $previousCaptureEnv }

    $captureWindow = Wait-ShowcaseWindow -Process $captureProcess
    $captureHandle = $captureWindow.Handle
    Set-ShowcaseClientSize -Handle $captureHandle -Width $clientWidth -Height $clientHeight
    Set-ShowcaseTopmost -Handle $captureHandle
    Start-Sleep -Milliseconds 400
    Wait-ShowcaseScene -Process $captureProcess -Handle $captureHandle -Width $clientWidth -Height $clientHeight `
        -ProbePath (Join-Path $frameDir 'capture-probe.png')

    # Frame-zero reset click, exactly as tools/readme_media_manifest.json schedules it.
    Send-ShowcaseClick -Handle $captureHandle -ClientX ([int]($clientWidth / 2)) -ClientY ([int]($clientHeight / 2))
    $cycleClock = [System.Diagnostics.Stopwatch]::StartNew()

    $samples = [ordered]@{}
    foreach ($sample in @(
            @{ Name = 'dance-a'; At = $phaseDanceSampleA },
            @{ Name = 'dance-b'; At = $phaseDanceSampleB },
            @{ Name = 'blend'; At = $phaseBlendSample },
            @{ Name = 'ik'; At = $phaseIkSample })) {
        $waitMs = [int](($sample.At * 1000.0) - $cycleClock.Elapsed.TotalMilliseconds)
        if ($waitMs -gt 0) { Start-Sleep -Milliseconds $waitMs }
        $samples[$sample.Name] = Save-ShowcaseFrame -Handle $captureHandle -Width $clientWidth -Height $clientHeight `
            -Path (Join-Path $frameDir "capture-$($sample.Name).png")
    }
    $cycleClock.Stop()

    $captureProcess.Refresh()
    Assert-True (-not $captureProcess.HasExited) 'capture-mode process is still running after the eight-second cycle'
    Assert-True ($captureProcess.Responding) 'capture-mode process stays responsive while the showcase drives four palettes'
    Assert-True ($captureProcess.MainWindowHandle -ne [IntPtr]::Zero) 'capture-mode process owns a visible main window'

    $danceA = $samples['dance-a']
    $danceB = $samples['dance-b']
    $blend = $samples['blend']
    $ik = $samples['ik']

    # The HUD is light text on an opaque dark panel: without it this region is
    # scene background and carries almost no near-white pixels.
    $hudBright = [ShowcaseFrame]::BrightRatio($danceA, $hudX, $hudY, $hudW, $hudH, 200)
    Assert-True ($hudBright -ge 0.01) `
        ("capture mode renders showcase HUD text at client (24,24) (bright pixel ratio {0:N4}, need >= 0.0100)" -f $hudBright)

    # The cast line never changes, so it must be pixel stable across the whole
    # cycle. This is what makes the normal-mode comparison below conclusive.
    $castStableAcrossPhases = @(
        [ShowcaseFrame]::DiffRatio($danceA, $blend, $castX, $castY, $castW, $castH, $pixelTolerance),
        [ShowcaseFrame]::DiffRatio($danceA, $ik, $castX, $castY, $castW, $castH, $pixelTolerance)
    ) | Measure-Object -Maximum | Select-Object -ExpandProperty Maximum
    Assert-True ($castStableAcrossPhases -le 0.005) `
        ("the '4 CHARACTERS / LIVE PALETTES' line is pixel stable across phases (max diff {0:N4}, need <= 0.0050)" -f $castStableAcrossPhases)

    # Distinct phase labels rasterise to distinct pixels.
    $hudDanceVsBlend = [ShowcaseFrame]::DiffRatio($danceA, $blend, $hudX, $hudY, $hudW, $hudH, 40)
    $hudDanceVsIk = [ShowcaseFrame]::DiffRatio($danceA, $ik, $hudX, $hudY, $hudW, $hudH, 40)
    $hudBlendVsIk = [ShowcaseFrame]::DiffRatio($blend, $ik, $hudX, $hudY, $hudW, $hudH, 40)
    Assert-True ($hudDanceVsBlend -ge 0.02) `
        ("Dance and BlendLayer phases show different HUD content (diff {0:N4}, need >= 0.0200)" -f $hudDanceVsBlend)
    Assert-True ($hudDanceVsIk -ge 0.02) `
        ("Dance and IK phases show different HUD content (diff {0:N4}, need >= 0.0200)" -f $hudDanceVsIk)
    Assert-True ($hudBlendVsIk -ge 0.02) `
        ("BlendLayer and IK phases show different HUD content (diff {0:N4}, need >= 0.0200)" -f $hudBlendVsIk)

    # Two samples inside the same phase: the characters must still be moving.
    $characterBandTop = 200
    $characterBandHeight = $clientHeight - 260
    $sameDanceMotion = [ShowcaseFrame]::DiffRatio($danceA, $danceB, 0, $characterBandTop, $clientWidth, $characterBandHeight, 24)
    Assert-True ($sameDanceMotion -ge 0.005) `
        ("characters keep animating inside a single phase (diff {0:N4}, need >= 0.0050)" -f $sameDanceMotion)

    # The slots run with distinct time offsets, so separated moving column
    # clusters prove that several characters are live rather than one animating
    # while the rest are frozen.
    #
    # NOTE: the mandated capture layout places slot 2 at (0,0,62) directly behind
    # slot 0 at (0,0,5), so only three of the four characters are ever visible.
    # Three separated clusters is therefore the maximum this composition can
    # show. See the Task 3 report: the layout needs fixing before Task 5
    # publishes media, and this threshold should rise to 4 at that point.
    $frames = @($danceA, $danceB, $blend, $ik)
    $clusters = [ShowcaseFrame]::MovingColumnClusters($frames, 240, 480, 25, 24, 0.10, 2)
    Assert-True ($clusters -ge 3) `
        ("separated character regions animate independently (found $clusters, need >= 3)")

    Reset-ShowcaseTopmost -Handle $captureHandle
    Stop-ShowcaseProcess -Process $captureProcess

    # -----------------------------------------------------------------------
    # 4. Normal (non-capture) run: the showcase must be completely inert.
    # -----------------------------------------------------------------------
    Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue
    $normalProcess = Start-Process -FilePath $exePath -WorkingDirectory $runtimeDir -PassThru
    $normalWindow = Wait-ShowcaseWindow -Process $normalProcess
    $normalHandle = $normalWindow.Handle
    Set-ShowcaseClientSize -Handle $normalHandle -Width $clientWidth -Height $clientHeight
    Set-ShowcaseTopmost -Handle $normalHandle
    Start-Sleep -Milliseconds 400
    Wait-ShowcaseScene -Process $normalProcess -Handle $normalHandle -Width $clientWidth -Height $clientHeight `
        -ProbePath (Join-Path $frameDir 'normal-probe.png') -ClickToStart
    Start-Sleep -Milliseconds 1200
    $normalFrame = Save-ShowcaseFrame -Handle $normalHandle -Width $clientWidth -Height $clientHeight `
        -Path (Join-Path $frameDir 'normal.png')

    $normalProcess.Refresh()
    Assert-True (-not $normalProcess.HasExited) 'normal-mode process is still running'
    Assert-True ($normalProcess.Responding) 'normal-mode process stays responsive'
    Assert-True ($normalProcess.MainWindowHandle -ne [IntPtr]::Zero) 'normal-mode process owns a visible main window'

    # If the showcase HUD leaked outside capture mode, its fixed-geometry opaque
    # panel would land on exactly these pixels and reproduce the phase-invariant
    # cast line, driving this diff to the 0.0000 measured above.
    $hudLeak = [ShowcaseFrame]::DiffRatio($danceA, $normalFrame, $castX, $castY, $castW, $castH, $pixelTolerance)
    Assert-True ($hudLeak -ge 0.10) `
        ("normal mode shows the editor UI, not the showcase HUD, at client (24,24) (diff {0:N4}, need >= 0.1000)" -f $hudLeak)

    $normalContent = [ShowcaseFrame]::BrightRatio($normalFrame, $hudX, $hudY, $hudW, $hudH, 60)
    Assert-True ($normalContent -ge 0.05) `
        ("normal mode still draws its own UI in that region (lit pixel ratio {0:N4}, need >= 0.0500)" -f $normalContent)

    Reset-ShowcaseTopmost -Handle $normalHandle
}
finally {
    Reset-ShowcaseTopmost -Handle $captureHandle
    Reset-ShowcaseTopmost -Handle $normalHandle
    Stop-ShowcaseProcess -Process $captureProcess
    Stop-ShowcaseProcess -Process $normalProcess
    if ($null -eq $previousCaptureEnv) { Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue }
    else { $env:DX11_README_CAPTURE = $previousCaptureEnv }
    if (-not $KeepFrames -and (Test-Path -LiteralPath $frameDir)) {
        Remove-Item -LiteralPath $frameDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    elseif ($KeepFrames) {
        Write-Host "  frames kept in $frameDir"
    }
}

if ($script:Failures.Count -gt 0) {
    Write-Host 'Project 36 portfolio showcase verification failed:'
    foreach ($failure in $script:Failures) { Write-Host " - $failure" }
    exit 1
}

Write-Host 'Project 36 portfolio showcase verification passed.'
