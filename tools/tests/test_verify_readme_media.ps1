$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
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

function New-OneFrameGif([string]$Path) {
    $bitmap = $null
    try {
        $bitmap = [System.Drawing.Bitmap]::new(640, 360)
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Gif)
    }
    finally {
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

function New-MovingGif([string]$Path, [string]$FramesDir) {
    if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
        throw 'ffmpeg is required to build the real multi-frame GIF fixture'
    }
    $null = New-Item -ItemType Directory -Path $FramesDir -Force
    for ($index = 0; $index -lt 8; $index++) {
        New-PatternPng -Path (Join-Path $FramesDir ("frame-{0:D2}.png" -f $index)) -Width 800 -Height 450 -FrameIndex $index
    }
    & ffmpeg -hide_banner -loglevel error -y -framerate 2 -start_number 0 -i (Join-Path $FramesDir 'frame-%02d.png') -loop 0 $Path
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg fixture encoding failed with exit code $LASTEXITCODE" }
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

Add-Type -AssemblyName System.Drawing

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$verifier = Join-Path $repoRoot 'tools/verify_readme_media.ps1'
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
| 01 | 2 | 01_Test.exe | docs/media/readme/01-Test.png | Success | 1600x900 | 1000 | PNG captured |
| 01 | 2 | 01_Test.exe | docs/media/readme/01-Test.gif | Success | 800x450 | 2000 | GIF captured |
'@

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
    $valid = Invoke-Verifier -Script $verifier -RepoRoot $fixtureRoot -Manifest $manifestPath
    Assert-True ($valid.ExitCode -eq 0) "valid fixture failed verification: $($valid.Output)"
    Assert-True ($valid.Output -match 'README media verification passed') 'valid fixture did not print the success message'

    'README media verifier tests passed'
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}
