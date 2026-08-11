$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Invoke-ProductionMarkdownRowParser([string]$Script, [string]$Line) {
    $source = Get-Content -Raw -LiteralPath $Script
    $start = $source.IndexOf('function Split-MarkdownRow {', [System.StringComparison]::Ordinal)
    $end = $source.IndexOf('function Get-CaptureReportRows {', [System.StringComparison]::Ordinal)
    Assert-True ($start -ge 0 -and $end -gt $start) 'could not locate Split-MarkdownRow in verifier source'
    Invoke-Expression $source.Substring($start, $end - $start)
    return @(Split-MarkdownRow $Line)
}

function Write-Utf8File([string]$Path, [string]$Content) {
    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        $null = New-Item -ItemType Directory -Path $parent -Force
    }
    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Write-FixtureManifest([string]$Path, [object]$Manifest) {
    Write-Utf8File -Path $Path -Content (($Manifest | ConvertTo-Json -Depth 10) + "`n")
}

function New-SolidPng([string]$Path, [int]$Width, [int]$Height) {
    $bitmap = $null
    try {
        $bitmap = [System.Drawing.Bitmap]::new($Width, $Height)
        for ($y = 0; $y -lt $Height; $y++) {
            for ($x = 0; $x -lt $Width; $x++) {
                $bitmap.SetPixel($x, $y, [System.Drawing.Color]::DarkSlateGray)
            }
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

function New-PatternPng([string]$Path, [int]$Width, [int]$Height, [int]$FrameIndex = 0) {
    $palette = @(
        [System.Drawing.Color]::FromArgb(235, 64, 52),
        [System.Drawing.Color]::FromArgb(49, 130, 206),
        [System.Drawing.Color]::FromArgb(56, 161, 105),
        [System.Drawing.Color]::FromArgb(214, 158, 46),
        [System.Drawing.Color]::FromArgb(128, 90, 213),
        [System.Drawing.Color]::FromArgb(221, 107, 32),
        [System.Drawing.Color]::FromArgb(0, 181, 216),
        [System.Drawing.Color]::FromArgb(236, 201, 75)
    )
    $bitmap = $null
    $graphics = $null
    $brushes = [System.Collections.Generic.List[System.Drawing.Brush]]::new()
    try {
        $bitmap = [System.Drawing.Bitmap]::new($Width, $Height)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        foreach ($color in $palette) { $brushes.Add([System.Drawing.SolidBrush]::new($color)) }
        for ($y = 0; $y -lt $Height; $y += 40) {
            for ($x = 0; $x -lt $Width; $x += 40) {
                $brush = $brushes[(($x / 40) + ($y / 40)) % $brushes.Count]
                $graphics.FillRectangle($brush, $x, $y, 40, 40)
            }
        }
        $movingX = 40 + (($FrameIndex * 83) % [math]::Max(1, ($Width - 180)))
        $graphics.FillRectangle([System.Drawing.Brushes]::White, $movingX, 120, 140, 100)
        $graphics.FillRectangle([System.Drawing.Brushes]::Black, $movingX + 20, 140, 100, 60)
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        foreach ($brush in $brushes) { $brush.Dispose() }
        if ($null -ne $graphics) { $graphics.Dispose() }
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

function New-TransparentHiddenColorPng([string]$Path, [int]$Width, [int]$Height, [int]$FrameIndex = 0) {
    $bitmap = $null
    try {
        $bitmap = [System.Drawing.Bitmap]::new($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        for ($row = 0; $row -lt 18; $row++) {
            $y = [math]::Min($Height - 1, [int][math]::Floor((($row + 0.5) * $Height) / 18))
            for ($column = 0; $column -lt 32; $column++) {
                $x = [math]::Min($Width - 1, [int][math]::Floor((($column + 0.5) * $Width) / 32))
                $value = $column + ($row * 32) + ($FrameIndex * 17) + 1
                $color = [System.Drawing.Color]::FromArgb(0, (37 * $value) % 256, (83 * $value) % 256, (149 * $value) % 256)
                $bitmap.SetPixel($x, $y, $color)
            }
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

function New-AlphaMotionFrame([string]$Path, [bool]$Opaque) {
    $bitmap = $null
    $graphics = $null
    try {
        $bitmap = [System.Drawing.Bitmap]::new(800, 450, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $alpha = if ($Opaque) { 255 } else { 0 }
        $graphics.Clear([System.Drawing.Color]::FromArgb($alpha, 0, 0, 0))
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($null -ne $graphics) { $graphics.Dispose() }
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

function New-OneFrameGif([string]$Path) {
    $bitmap = $null
    try {
        $bitmap = [System.Drawing.Bitmap]::new(800, 450)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try { $graphics.Clear([System.Drawing.Color]::Crimson) } finally { $graphics.Dispose() }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Gif)
    }
    finally {
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

function New-MovingGif([string]$Path, [string]$FramesDir, [double]$FrameRate = 8, [int]$FrameCount = 32) {
    if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
        throw 'ffmpeg is required to build the real multi-frame GIF fixture'
    }
    $null = New-Item -ItemType Directory -Path $FramesDir -Force
    for ($index = 0; $index -lt $FrameCount; $index++) {
        New-PatternPng -Path (Join-Path $FramesDir ("frame-{0:D2}.png" -f $index)) -Width 800 -Height 450 -FrameIndex $index
    }
    & ffmpeg -hide_banner -loglevel error -y -framerate $FrameRate -start_number 0 -i (Join-Path $FramesDir 'frame-%02d.png') -filter_complex '[0:v]split[s0][s1];[s0]palettegen=max_colors=64[p];[s1][p]paletteuse' -loop 0 $Path
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg fixture encoding failed with exit code $LASTEXITCODE" }
}

function New-TransparentHiddenColorGif([string]$Path, [string]$FramesDir) {
    $null = New-Item -ItemType Directory -Path $FramesDir -Force
    for ($index = 0; $index -lt 32; $index++) {
        New-TransparentHiddenColorPng -Path (Join-Path $FramesDir ("frame-{0:D2}.png" -f $index)) -Width 800 -Height 450 -FrameIndex $index
    }
    & ffmpeg -hide_banner -loglevel error -y -framerate 8 -start_number 0 -i (Join-Path $FramesDir 'frame-%02d.png') -filter_complex '[0:v]split[s0][s1];[s0]palettegen=reserve_transparent=1[p];[s1][p]paletteuse=alpha_threshold=128' -loop 0 $Path
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg transparent GIF fixture encoding failed with exit code $LASTEXITCODE" }
}

function New-AlphaMotionGif([string]$Path, [string]$FramesDir) {
    $null = New-Item -ItemType Directory -Path $FramesDir -Force
    for ($index = 0; $index -lt 32; $index++) {
        New-AlphaMotionFrame -Path (Join-Path $FramesDir ("frame-{0:D2}.png" -f $index)) -Opaque ($index -ge 4)
    }
    & ffmpeg -hide_banner -loglevel error -y -framerate 8 -start_number 0 -i (Join-Path $FramesDir 'frame-%02d.png') -filter_complex '[0:v]split[s0][s1];[s0]palettegen=reserve_transparent=1[p];[s1][p]paletteuse=alpha_threshold=128' -loop 0 $Path
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg alpha-motion GIF fixture encoding failed with exit code $LASTEXITCODE" }
}

function Invoke-Verifier([string]$Script, [string]$RepoRoot, [string]$Manifest) {
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = (Get-Command pwsh).Source
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    foreach ($argument in @('-NoProfile', '-File', $Script, '-RepoRoot', $RepoRoot, '-Manifest', $Manifest)) {
        $null = $startInfo.ArgumentList.Add($argument)
    }

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        $null = $process.Start()
        $standardOutputTask = $process.StandardOutput.ReadToEndAsync()
        $standardErrorTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $standardOutput = $standardOutputTask.GetAwaiter().GetResult()
        $standardError = $standardErrorTask.GetAwaiter().GetResult()
        return [pscustomobject]@{
            ExitCode = $process.ExitCode
            Output = ($standardOutput + $standardError).Trim()
        }
    }
    finally {
        $process.Dispose()
    }
}

function Assert-FailedWith([object]$Result, [string[]]$Patterns, [string]$Message) {
    Assert-True ($Result.ExitCode -ne 0) "$Message (exit code was 0)"
    foreach ($pattern in $Patterns) {
        Assert-True ($Result.Output -match $pattern) "$Message (missing '$pattern' in output: $($Result.Output))"
    }
}

function Assert-OutputExcludes([object]$Result, [string[]]$Patterns, [string]$Message) {
    foreach ($pattern in $Patterns) {
        Assert-True ($Result.Output -notmatch $pattern) "$Message (unexpected '$pattern' in output: $($Result.Output))"
    }
}

Add-Type -AssemblyName System.Drawing

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$verifier = Join-Path $repoRoot 'tools/verify_readme_media.ps1'
$trailingEscapedPipe = @(Invoke-ProductionMarkdownRowParser -Script $verifier -Line '| 01 | 3 | 01_Test.exe | docs/media/readme/01-Test.gif | Success | 800x450 | 2000 | trailing escaped pipe \|')
Assert-True ($trailingEscapedPipe.Count -eq 8) "trailing escaped-pipe row parsed as $($trailingEscapedPipe.Count) cells instead of 8"
Assert-True ($trailingEscapedPipe[-1] -ceq 'trailing escaped pipe |') "trailing escaped pipe was corrupted: '$($trailingEscapedPipe[-1])'"

$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("verify-readme-media-" + [guid]::NewGuid().ToString('N'))
$manifestPath = Join-Path $fixtureRoot 'tools/readme_media_manifest.json'
$mediaDir = Join-Path $fixtureRoot 'docs/media/readme'
$projectDir = Join-Path $fixtureRoot 'Dx11/01_Test'
$imagePath = Join-Path $mediaDir '01-Test.png'
$gifPath = Join-Path $mediaDir '01-Test.gif'
$infoPath = Join-Path $mediaDir 'info/01-Test-info.png'
$reportPath = Join-Path $mediaDir 'capture-report.md'
$projectReadmePath = Join-Path $projectDir 'README.md'
$rootReadmePath = Join-Path $fixtureRoot 'README.md'
$outsideRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("verify-readme-media-outside-" + [guid]::NewGuid().ToString('N'))
$junctionPath = Join-Path $fixtureRoot 'linked-media'

$project = [ordered]@{
    number = '01'
    name = 'Fixture'
    directory = '01_Test'
    exe = '01_Test.exe'
    image = '01-Test.png'
    gif = '01-Test.gif'
    infoImage = 'info/01-Test-info.png'
    gifPhase = 'runtime'
    title = 'Fixture'
    summary = 'Verifier fixture'
    tags = @('D3D11', 'Fixture', 'Verification')
    rootFeaturedGif = $true
}
$manifest = [ordered]@{
    runtimeDir = 'Dx11/bin'
    mediaDir = 'docs/media/readme'
    expectedProjectCount = 1
    captureWidth = 1600
    captureHeight = 900
    gifWidth = 800
    gifHeight = 450
    gifSeconds = 4
    gifFps = 8
    gifMaxBytes = 5242880
    infoWidth = 1600
    infoHeight = 640
    captureAttempts = 2
    projects = @($project)
}

try {
    $null = New-Item -ItemType Directory -Path (Split-Path -Parent $infoPath), $projectDir -Force
    Write-FixtureManifest -Path $manifestPath -Manifest $manifest
    Write-Utf8File -Path (Join-Path $fixtureRoot 'README_old.md') -Content "# Preserved fixture README`n"
    New-SolidPng -Path $imagePath -Width 320 -Height 180
    New-SolidPng -Path $infoPath -Width 320 -Height 128
    New-OneFrameGif -Path $gifPath
    Write-Utf8File -Path $projectReadmePath -Content "# Fixture`n`nThis body is intentionally long enough to survive generated block removal checks.`n"
    Write-Utf8File -Path $rootReadmePath -Content "# Fixture root`n"
    Write-Utf8File -Path $reportPath -Content @'
# README media capture report

| Project | Attempt | Exe | Output | Status | Dimensions | Bytes | Notes |
|---|---:|---|---|---|---|---:|---|
| 01 | 1 | 01_Test.exe | 01_Test.exe | Failure |  |  | intentional fixture failure |
'@

    $invalid = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $invalid -Patterns @('dimensions', 'variance', 'motion', 'README', 'report.*status') -Message 'invalid fixture was not fully rejected'

    [System.IO.File]::WriteAllBytes($imagePath, [byte[]](0x47, 0x49, 0x46, 0x38, 0x39, 0x61))
    [System.IO.File]::WriteAllBytes($gifPath, [byte[]](0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A))
    $badSignatures = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $badSignatures -Patterns @('PNG signature', 'GIF signature') -Message 'invalid media signatures were not rejected'

    New-PatternPng -Path $imagePath -Width 1600 -Height 900
    New-PatternPng -Path $infoPath -Width 1600 -Height 640
    New-MovingGif -Path $gifPath -FramesDir (Join-Path $fixtureRoot 'gif-frames')

    $top = @'
<!-- README-NAV-TOP:START -->
<div align="center">

이전 | [메인](../../README.md) | [상위](../) | 다음

</div>
<!-- README-NAV-TOP:END -->
'@
    $info = @'
<!-- README-INFO:START -->
<p align="center"><img src="../../docs/media/readme/info/01-Test-info.png" width="100%" /></p>
<!-- README-INFO:END -->
'@
    $runtime = @'
<!-- README-RUNTIME:START -->
## 실행 화면

| Screenshot | GIF |
|---|---|
| <img src="../../docs/media/readme/01-Test.png" width="100%" /> | <img src="../../docs/media/readme/01-Test.gif" width="100%" /> |
<!-- README-RUNTIME:END -->
'@
    $bottom = $top.Replace('README-NAV-TOP', 'README-NAV-BOTTOM')
    $body = '# Fixture' + "`n`n" + 'This preserved project documentation contains more than fifty meaningful characters outside generated blocks.'
    Write-Utf8File -Path $projectReadmePath -Content ($top + "`n`n" + $info + "`n`n" + $body + "`n`n" + $runtime + "`n`n" + $bottom + "`n")
    Write-Utf8File -Path $rootReadmePath -Content @'
# Fixture root

<img src="docs/media/readme/01-Test.png" />
<img src="docs/media/readme/01-Test.gif" />
'@
    Write-Utf8File -Path $reportPath -Content @'
# README media capture report

| Project | Attempt | Exe | Output | Status | Dimensions | Bytes | Notes |
|---|---:|---|---|---|---|---:|---|
| 01 | 1 | 01_Test.exe | 01_Test.exe | Failure |  |  | first attempt failed \| retrying |
| 01 | 2 | 01_Test.exe | 01_Test.exe | Failure | ends-with-\\|  | even backslash run before delimiter |
| 01 | 3 | 01_Test.exe | docs/media/readme/01-Test.png | Success | 1600x900 | 1000 | PNG captured |
| 01 | 3 | 01_Test.exe | docs/media/readme/01-Test.gif | Success | 800x450 | 2000 | GIF captured |
'@

    $validRootReadme = Get-Content -Raw -LiteralPath $rootReadmePath
    $validReport = Get-Content -Raw -LiteralPath $reportPath
    $validGifBackup = Join-Path $fixtureRoot 'valid-01-Test.gif'
    Copy-Item -LiteralPath $gifPath -Destination $validGifBackup

    Write-FixtureManifest -Path $manifestPath -Manifest $manifest
    $baseline = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-True ($baseline.ExitCode -eq 0) "odd/even escaped-pipe report with a later successful attempt failed: $($baseline.Output)"

    New-MovingGif -Path $gifPath -FramesDir (Join-Path $fixtureRoot 'above-window-gif-frames') -FrameRate 8 -FrameCount 40
    $aboveWindowDuration = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $aboveWindowDuration -Patterns @('GIF total decoded delay.*expected 3\.5-4\.5s') -Message 'five-second 8fps GIF outside the symmetric duration window was accepted'
    Copy-Item -LiteralPath $validGifBackup -Destination $gifPath -Force

    New-OneFrameGif -Path $gifPath
    $frameCountFailure = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $frameCountFailure -Patterns @('GIF frame count is 1; expected at least 2') -Message 'independent GIF frame-count diagnostic was not emitted'
    Copy-Item -LiteralPath $validGifBackup -Destination $gifPath -Force

    New-MovingGif -Path $gifPath -FramesDir (Join-Path $fixtureRoot 'short-gif-frames') -FrameRate 8 -FrameCount 8
    $durationFailure = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $durationFailure -Patterns @('GIF total decoded delay.*expected 3\.5-4\.5s') -Message 'independent GIF duration diagnostic was not emitted'
    Copy-Item -LiteralPath $validGifBackup -Destination $gifPath -Force

    $project.gifSeconds = 8
    Write-FixtureManifest -Path $manifestPath -Manifest $manifest
    New-MovingGif -Path $gifPath -FramesDir (Join-Path $fixtureRoot 'eight-second-gif-frames') -FrameRate 8 -FrameCount 64
    $eightSecondOverride = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-True ($eightSecondOverride.ExitCode -eq 0) "eight-second project override was rejected: $($eightSecondOverride.Output)"

    $project.Remove('gifSeconds')
    Write-FixtureManifest -Path $manifestPath -Manifest $manifest
    $eightSecondWithoutOverride = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $eightSecondWithoutOverride -Patterns @('GIF total decoded delay.*expected 3\.5-4\.5s') -Message 'eight-second GIF without a project override was accepted'
    Copy-Item -LiteralPath $validGifBackup -Destination $gifPath -Force

    New-MovingGif -Path $gifPath -FramesDir (Join-Path $fixtureRoot 'two-fps-gif-frames') -FrameRate 2 -FrameCount 8
    $cadenceFailure = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $cadenceFailure -Patterns @('GIF frame cadence is 2\.00fps; expected 8\.00fps') -Message 'independent GIF cadence diagnostic was not emitted'
    Copy-Item -LiteralPath $validGifBackup -Destination $gifPath -Force

    $oversizeStream = [System.IO.File]::Open($gifPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    try { $oversizeStream.SetLength(5242881) } finally { $oversizeStream.Dispose() }
    $sizeFailure = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $sizeFailure -Patterns @('GIF size is 5242881 bytes; expected at most 5242880 bytes') -Message 'independent GIF size-cap diagnostic was not emitted'
    Copy-Item -LiteralPath $validGifBackup -Destination $gifPath -Force

    New-TransparentHiddenColorPng -Path $imagePath -Width 1600 -Height 900
    $transparentPng = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $transparentPng -Patterns @('PNG sampled color count is 1; expected at least 8', 'PNG luminance variance is 0\.000; expected above 4\.0') -Message 'transparent hidden-color PNG affected visible sampling'
    New-PatternPng -Path $imagePath -Width 1600 -Height 900

    New-TransparentHiddenColorGif -Path $gifPath -FramesDir (Join-Path $fixtureRoot 'transparent-hidden-gif-frames')
    $transparentGif = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $transparentGif -Patterns @('GIF sampled motion is 0\.000%; expected above 0\.2%') -Message 'transparent hidden-color GIF affected visible motion sampling'
    Assert-OutputExcludes -Result $transparentGif -Patterns @('GIF frame count', 'GIF total decoded delay') -Message 'transparent hidden-color GIF regression was not isolated to motion'
    Copy-Item -LiteralPath $validGifBackup -Destination $gifPath -Force

    New-AlphaMotionGif -Path $gifPath -FramesDir (Join-Path $fixtureRoot 'alpha-motion-gif-frames')
    $alphaMotion = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-True ($alphaMotion.ExitCode -eq 0) "alpha-only visible GIF motion was not counted: $($alphaMotion.Output)"
    Copy-Item -LiteralPath $validGifBackup -Destination $gifPath -Force

    $allowedAttachmentReadme = $validRootReadme + @'

<img src="https://github.com/user-attachments/assets/3aafc53e-d6ae-492d-8680-b240c19f1f92" />
<img src="https://github.com/user-attachments/assets/64a50e8e-5580-4e76-97d1-b500f9c5a8a2" />
'@
    Write-Utf8File -Path $rootReadmePath -Content $allowedAttachmentReadme
    $allowedAttachments = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-True ($allowedAttachments.ExitCode -eq 0) "approved legacy root attachments were rejected: $($allowedAttachments.Output)"

    Write-Utf8File -Path $rootReadmePath -Content ($validRootReadme + "`n<img src=`"https://github.com/user-attachments/assets/00000000-0000-0000-0000-000000000000`" />`n")
    $unknownAttachment = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $unknownAttachment -Patterns @('unexpected root user-attachment') -Message 'unknown root attachment was not rejected'

    Write-Utf8File -Path $rootReadmePath -Content ($validRootReadme + "`n<img src=`"assets/local-other.gif`" />`n")
    $otherLocalGif = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $otherLocalGif -Patterns @('unexpected root GIF reference: assets/local-other\.gif') -Message 'local non-mediaDir root GIF embed was not rejected'
    Write-Utf8File -Path $rootReadmePath -Content ($validRootReadme + "`n![external demo](https://example.com/demo.gif)`n")
    $externalGif = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $externalGif -Patterns @('unexpected root GIF reference: https://example\.com/demo\.gif') -Message 'external root GIF embed was not rejected'
    Write-Utf8File -Path $rootReadmePath -Content ($validRootReadme + "`n![query demo](assets/query-demo.gif?raw=1)`n")
    $queryGif = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $queryGif -Patterns @('unexpected root GIF reference: assets/query-demo\.gif') -Message 'query-suffixed root GIF embed was not rejected'
    Write-Utf8File -Path $rootReadmePath -Content ($validRootReadme + "`n![fragment demo](assets/fragment-demo.gif#preview)`n")
    $fragmentGif = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $fragmentGif -Patterns @('unexpected root GIF reference: assets/fragment-demo\.gif') -Message 'fragment-suffixed root GIF embed was not rejected'
    Write-Utf8File -Path $rootReadmePath -Content ($validRootReadme + "`n![reference demo][reference-demo]`n`n[reference-demo]: assets/reference-demo.gif?raw=1`n")
    $referenceGif = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $referenceGif -Patterns @('unexpected root GIF reference: assets/reference-demo\.gif') -Message 'reference-style root GIF embed was not rejected'
    Write-Utf8File -Path $rootReadmePath -Content ($validRootReadme + "`n![collapsed demo][]`n`n[collapsed demo]: assets/collapsed-demo.gif`n")
    $collapsedReferenceGif = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $collapsedReferenceGif -Patterns @('unexpected root GIF reference: assets/collapsed-demo\.gif') -Message 'collapsed reference-style root GIF embed was not rejected'
    Write-Utf8File -Path $rootReadmePath -Content ($validRootReadme + "`n![shortcut demo]`n`n[shortcut demo]: assets/shortcut-demo.gif`n")
    $shortcutReferenceGif = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $shortcutReferenceGif -Patterns @('unexpected root GIF reference: assets/shortcut-demo\.gif') -Message 'shortcut reference-style root GIF embed was not rejected'
    Write-Utf8File -Path $rootReadmePath -Content @'
# Fixture root

<img src="docs/media/readme/01-Test.png" />
![featured][featured-demo]

[featured-demo]: docs/media/readme/01-Test.gif?raw=1
'@
    $featuredReference = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-True ($featuredReference.ExitCode -eq 0) "reference-style featured GIF embed was not accepted: $($featuredReference.Output)"
    Write-Utf8File -Path $rootReadmePath -Content @'
# Fixture root

<img src="docs/media/readme/01-Test.png" />
Featured path as text only: docs/media/readme/01-Test.gif
'@
    $featuredTextOnly = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $featuredTextOnly -Patterns @('missing featured GIF embed: docs/media/readme/01-Test\.gif') -Message 'plain text incorrectly satisfied the featured GIF embed requirement'
    Write-Utf8File -Path $rootReadmePath -Content $validRootReadme

    Remove-Item -LiteralPath $infoPath -Force
    $lockedStream = [System.IO.File]::Open($imagePath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::None)
    try {
        $lockedMedia = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    }
    finally {
        $lockedStream.Dispose()
    }
    Assert-FailedWith -Result $lockedMedia -Patterns @('PNG validation failed.*being used by another process', 'info PNG missing') -Message 'locked media I/O did not aggregate a second independent diagnostic'
    New-PatternPng -Path $infoPath -Width 1600 -Height 640

    $null = New-Item -ItemType Directory -Path $outsideRoot -Force
    Write-Utf8File -Path (Join-Path $outsideRoot 'sentinel.txt') -Content "outside target must survive junction cleanup`n"
    [System.IO.File]::WriteAllBytes((Join-Path $outsideRoot '01-Test.png'), [byte[]](0x47, 0x49, 0x46, 0x38, 0x39, 0x61))
    $null = New-Item -ItemType Junction -Path $junctionPath -Target $outsideRoot
    $junctionManifest = $manifest | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    $junctionManifest.mediaDir = 'linked-media'
    Write-FixtureManifest -Path $manifestPath -Manifest $junctionManifest
    $junctionResult = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $junctionResult -Patterns @('Manifest mediaDir.*reparse point') -Message 'junction traversal was not rejected canonically'
    Assert-OutputExcludes -Result $junctionResult -Patterns @('invalid PNG signature') -Message 'verifier read media through the outside junction'
    Remove-Item -LiteralPath $junctionPath -Force
    Assert-True (Test-Path -LiteralPath (Join-Path $outsideRoot 'sentinel.txt') -PathType Leaf) 'junction cleanup removed the outside target'
    Write-FixtureManifest -Path $manifestPath -Manifest $manifest

    $unsafeProjectManifest = $manifest | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    $unsafeProjectManifest.projects[0].image = '../outside.png'
    Write-FixtureManifest -Path $manifestPath -Manifest $unsafeProjectManifest
    $unsafeProject = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $unsafeProject -Patterns @('contained') -Message 'traversing project media path was not rejected'

    $unsafeMediaManifest = $manifest | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    $unsafeMediaManifest.mediaDir = '../outside-media'
    Write-FixtureManifest -Path $manifestPath -Manifest $unsafeMediaManifest
    $unsafeMedia = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $unsafeMedia -Patterns @('contained') -Message 'traversing media directory was not rejected'

    $unfeaturedManifest = $manifest | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    $unfeaturedManifest.projects[0].rootFeaturedGif = $false
    Write-FixtureManifest -Path $manifestPath -Manifest $unfeaturedManifest
    $unfeatured = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-FailedWith -Result $unfeatured -Patterns @('unexpected root GIF') -Message 'unfeatured root GIF was not rejected'

    Write-FixtureManifest -Path $manifestPath -Manifest $manifest
    Write-Utf8File -Path $rootReadmePath -Content $validRootReadme
    Write-Utf8File -Path $reportPath -Content $validReport
    Copy-Item -LiteralPath $validGifBackup -Destination $gifPath -Force
    $valid = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-True ($valid.ExitCode -eq 0) "valid fixture failed verification: $($valid.Output)"
    Assert-True ($valid.Output -match 'README media verification passed') 'valid fixture did not print the success message'

    'README media verifier tests passed'
}
finally {
    if (Test-Path -LiteralPath $junctionPath) {
        Remove-Item -LiteralPath $junctionPath -Force
    }
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
    if (Test-Path -LiteralPath $outsideRoot) {
        Remove-Item -LiteralPath $outsideRoot -Recurse -Force
    }
}
