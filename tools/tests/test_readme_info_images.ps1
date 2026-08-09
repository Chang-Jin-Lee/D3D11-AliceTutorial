param([switch]$KeepFixture)

$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Test-MultipleColors([System.Drawing.Bitmap]$Image, [System.Drawing.Rectangle]$Region) {
    $colors = [System.Collections.Generic.HashSet[int]]::new()
    for ($y = $Region.Top; $y -lt $Region.Bottom; $y += 12) {
        for ($x = $Region.Left; $x -lt $Region.Right; $x += 12) {
            $null = $colors.Add($Image.GetPixel($x, $y).ToArgb())
        }
    }
    return $colors.Count -gt 1
}

function Test-ContainsColor([System.Drawing.Bitmap]$Image, [System.Drawing.Rectangle]$Region, [System.Drawing.Color]$Color) {
    for ($y = $Region.Top; $y -lt $Region.Bottom; $y += 3) {
        for ($x = $Region.Left; $x -lt $Region.Right; $x += 3) {
            if ($Image.GetPixel($x, $y).ToArgb() -eq $Color.ToArgb()) { return $true }
        }
    }
    return $false
}

function Test-GreenDominant([System.Drawing.Color]$Color) {
    return $Color.G -gt ($Color.R + 40) -and $Color.G -gt ($Color.B + 40)
}

function New-FixturePng([string]$Path, [System.Drawing.Color]$Color, [bool]$FourColor) {
    $bitmap = $null
    $graphics = $null
    try {
        $bitmap = [System.Drawing.Bitmap]::new(1600, 900)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        if ($FourColor) {
            $graphics.FillRectangle([System.Drawing.Brushes]::Crimson, 0, 0, 800, 450)
            $graphics.FillRectangle([System.Drawing.Brushes]::ForestGreen, 800, 0, 800, 450)
            $graphics.FillRectangle([System.Drawing.Brushes]::RoyalBlue, 0, 450, 800, 450)
            $graphics.FillRectangle([System.Drawing.Brushes]::Gold, 800, 450, 800, 450)
        }
        else {
            $graphics.Clear($Color)
        }
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        if ($null -ne $graphics) { $graphics.Dispose() }
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

function Copy-FixtureManifest([object]$Manifest) {
    return $Manifest | ConvertTo-Json -Depth 12 | ConvertFrom-Json
}

function Write-FixtureManifest([string]$Path, [object]$Manifest) {
    $Manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Assert-GeneratorRejected([string]$Script, [object[]]$ScriptArguments, [string]$Message, [string]$ExpectedPattern = 'Manifest media path') {
    $output = @(& pwsh -NoProfile -File $Script @ScriptArguments 2>&1)
    Assert-True ($LASTEXITCODE -ne 0) $Message
    Assert-True (($output | Out-String) -match $ExpectedPattern) "$Message did not report the expected rejection"
}

Add-Type -AssemblyName System.Drawing

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$infoImageScript = Join-Path $repoRoot 'tools\generate_readme_info_images.ps1'
$reviewSheetScript = Join-Path $repoRoot 'tools\generate_readme_review_sheets.ps1'
$tempRoot = Join-Path $env:TEMP ('D3D11-readme-info-image-test-' + [Guid]::NewGuid().ToString('N'))
$mediaDir = Join-Path $tempRoot 'media'
$infoDir = Join-Path $mediaDir 'info'
$framesDir = Join-Path $tempRoot 'gif-frames'
$manifestPath = Join-Path $tempRoot 'manifest.json'
$reviewDir = Join-Path $tempRoot 'review'
$pngSheet = Join-Path $reviewDir 'readme-png-review.png'
$gifSheet = Join-Path $reviewDir 'readme-gif-review.png'
$projectDirectories = @('01_RenderingQuadangle', '02_RenderingCube', '03_RenderingMeshAndSceneGraph', '04_RenderingMeshWithTexture', '05_Mesh', '06_pmx')
$projectColors = @([System.Drawing.Color]::DarkOrange, [System.Drawing.Color]::MediumPurple, [System.Drawing.Color]::Teal, [System.Drawing.Color]::Sienna, [System.Drawing.Color]::SteelBlue, [System.Drawing.Color]::DeepPink)

New-Item -ItemType Directory -Path $infoDir, $framesDir -Force | Out-Null
try {
    $projects = [System.Collections.Generic.List[object]]::new()
    for ($index = 0; $index -lt 6; $index++) {
        $number = ($index + 1).ToString('00')
        $pngName = "$number-fixture.png"
        $gifName = "$number-fixture.gif"
        New-FixturePng -Path (Join-Path $mediaDir $pngName) -Color $projectColors[$index] -FourColor ($index -eq 0)
        $tags = if ($index -eq 0) {
            @(
                'Direct3D11 Vertex Buffer Rendering Pipeline',
                'High Fidelity Texture Sampling Workflow',
                'Hierarchical Transform Composition System',
                'Korean Typography Fallback Verification',
                'High Quality Bicubic Screenshot Scaling'
            )
        }
        else {
            @('D3D11', 'Fixture', "Project $number")
        }
        $projects.Add([pscustomobject]@{
            number = $number
            name = "Fixture $number"
            directory = $projectDirectories[$index]
            exe = "fixture-$number.exe"
            image = $pngName
            gif = $gifName
            infoImage = "info/$number-fixture-info.png"
            title = 'Fixture Poster Title With Enough Words To Fit'
            summary = '한국어 요약 텍스트로 D3D11 이미지 생성의 글꼴 대체와 줄바꿈을 검증합니다.'
            tags = $tags
            gifPhase = 'startup'
        })
    }

    New-FixturePng -Path (Join-Path $framesDir 'frame-00.png') -Color ([System.Drawing.Color]::Crimson) -FourColor $false
    New-FixturePng -Path (Join-Path $framesDir 'frame-01.png') -Color ([System.Drawing.Color]::LimeGreen) -FourColor $false
    New-FixturePng -Path (Join-Path $framesDir 'frame-02.png') -Color ([System.Drawing.Color]::RoyalBlue) -FourColor $false
    $gifTemplate = Join-Path $mediaDir '01-fixture.gif'
    & ffmpeg -hide_banner -loglevel error -y -framerate 1 -start_number 0 -i (Join-Path $framesDir 'frame-%02d.png') -loop 0 $gifTemplate
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg GIF fixture encoding failed with exit code $LASTEXITCODE" }
    for ($index = 1; $index -lt 6; $index++) {
        Copy-Item -LiteralPath $gifTemplate -Destination (Join-Path $mediaDir ("{0:D2}-fixture.gif" -f ($index + 1)))
    }

    $manifest = [pscustomobject]@{
        runtimeDir = 'Dx11/bin'
        mediaDir = $mediaDir
        expectedProjectCount = 6
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
        projects = $projects.ToArray()
    }
    Write-FixtureManifest -Path $manifestPath -Manifest $manifest

    $outsideRootedImage = Join-Path $tempRoot 'outside-rooted-image.png'
    $outsideTraversalImage = Join-Path $tempRoot 'outside-traversal-image.png'
    $outsideRootedInfo = Join-Path $tempRoot 'outside-rooted-info.png'
    $outsideTraversalInfo = Join-Path $tempRoot 'outside-traversal-info.png'
    $outsideRootedGif = Join-Path $tempRoot 'outside-rooted.gif'
    Copy-Item -LiteralPath (Join-Path $mediaDir '01-fixture.png') -Destination $outsideRootedImage
    Copy-Item -LiteralPath (Join-Path $mediaDir '01-fixture.png') -Destination $outsideTraversalImage
    Copy-Item -LiteralPath $gifTemplate -Destination $outsideRootedGif

    $rootedImageManifest = Copy-FixtureManifest $manifest
    $rootedImageManifest.projects[0].image = $outsideRootedImage
    $rootedImageManifestPath = Join-Path $tempRoot 'rooted-image.json'
    Write-FixtureManifest -Path $rootedImageManifestPath -Manifest $rootedImageManifest
    Assert-GeneratorRejected -Script $infoImageScript -ScriptArguments @('-Manifest', $rootedImageManifestPath, '-ProjectNumber', '01') -Message 'rooted image manifest path was accepted' -ExpectedPattern "invalid safe relative path 'image'"

    $traversalImageManifest = Copy-FixtureManifest $manifest
    $traversalImageManifest.projects[0].image = '..\outside-traversal-image.png'
    $traversalImageManifestPath = Join-Path $tempRoot 'traversal-image.json'
    Write-FixtureManifest -Path $traversalImageManifestPath -Manifest $traversalImageManifest
    Assert-GeneratorRejected -Script $infoImageScript -ScriptArguments @('-Manifest', $traversalImageManifestPath, '-ProjectNumber', '01') -Message 'traversal image manifest path was accepted' -ExpectedPattern "invalid safe relative path 'image'"

    $rootedInfoManifest = Copy-FixtureManifest $manifest
    $rootedInfoManifest.projects[0].infoImage = $outsideRootedInfo
    $rootedInfoManifestPath = Join-Path $tempRoot 'rooted-info.json'
    Write-FixtureManifest -Path $rootedInfoManifestPath -Manifest $rootedInfoManifest
    Assert-GeneratorRejected -Script $infoImageScript -ScriptArguments @('-Manifest', $rootedInfoManifestPath, '-ProjectNumber', '01') -Message 'rooted info image manifest path was accepted' -ExpectedPattern "invalid safe relative path 'infoImage'"
    Assert-True (-not (Test-Path -LiteralPath $outsideRootedInfo)) 'rooted info image created an outside output'

    $traversalInfoManifest = Copy-FixtureManifest $manifest
    $traversalInfoManifest.projects[0].infoImage = '..\outside-traversal-info.png'
    $traversalInfoManifestPath = Join-Path $tempRoot 'traversal-info.json'
    Write-FixtureManifest -Path $traversalInfoManifestPath -Manifest $traversalInfoManifest
    Assert-GeneratorRejected -Script $infoImageScript -ScriptArguments @('-Manifest', $traversalInfoManifestPath, '-ProjectNumber', '01') -Message 'traversal info image manifest path was accepted' -ExpectedPattern "invalid safe relative path 'infoImage'"
    Assert-True (-not (Test-Path -LiteralPath $outsideTraversalInfo)) 'traversal info image created an outside output'

    $wrongDimensionManifest = Copy-FixtureManifest $manifest
    $wrongDimensionManifest.infoWidth = 1599
    $wrongDimensionManifest.projects[0].infoImage = 'info/wrong-dimension-info.png'
    $wrongDimensionManifestPath = Join-Path $tempRoot 'wrong-dimension.json'
    Write-FixtureManifest -Path $wrongDimensionManifestPath -Manifest $wrongDimensionManifest
    $wrongDimensionOutput = Join-Path $infoDir 'wrong-dimension-info.png'
    Assert-GeneratorRejected -Script $infoImageScript -ScriptArguments @('-Manifest', $wrongDimensionManifestPath, '-ProjectNumber', '01') -Message 'wrong information image dimensions were accepted' -ExpectedPattern '1600x640'
    Assert-True (-not (Test-Path -LiteralPath $wrongDimensionOutput)) 'wrong dimensions created an information image'

    $rootedGifManifest = Copy-FixtureManifest $manifest
    $rootedGifManifest.projects[0].gif = $outsideRootedGif
    $rootedGifManifestPath = Join-Path $tempRoot 'rooted-gif.json'
    Write-FixtureManifest -Path $rootedGifManifestPath -Manifest $rootedGifManifest
    Assert-GeneratorRejected -Script $reviewSheetScript -ScriptArguments @('-Manifest', $rootedGifManifestPath, '-OutputDir', (Join-Path $tempRoot 'rooted-gif-review')) -Message 'rooted GIF manifest path was accepted' -ExpectedPattern "invalid safe relative path 'gif'"

    & pwsh -NoProfile -File $infoImageScript -Manifest $manifestPath -ProjectNumber '01'
    $posterPath = Join-Path $infoDir '01-fixture-info.png'
    Assert-True (Test-Path -LiteralPath $posterPath -PathType Leaf) 'poster missing'

    $poster = [System.Drawing.Bitmap]::new($posterPath)
    try {
        Assert-True ($poster.Width -eq 1600 -and $poster.Height -eq 640) 'poster dimensions mismatch'
        Assert-True (Test-MultipleColors -Image $poster -Region ([System.Drawing.Rectangle]::new(40, 40, 940, 560))) 'screenshot region is blank or monochrome'
        Assert-True (Test-MultipleColors -Image $poster -Region ([System.Drawing.Rectangle]::new(1030, 64, 520, 512))) 'summary region is blank or monochrome'
        Assert-True (Test-ContainsColor -Image $poster -Region ([System.Drawing.Rectangle]::new(1062, 522, 460, 34)) -Color ([System.Drawing.Color]::FromArgb(46, 70, 82))) 'lowest tag row was not rendered inside the summary panel'
    }
    finally {
        $poster.Dispose()
    }

    & pwsh -NoProfile -File $reviewSheetScript -Manifest $manifestPath -OutputDir $reviewDir
    Assert-True ((Test-Path -LiteralPath $pngSheet -PathType Leaf) -and (Test-Path -LiteralPath $gifSheet -PathType Leaf)) 'review sheets missing'

    $pngReview = [System.Drawing.Bitmap]::new($pngSheet)
    $gifReview = [System.Drawing.Bitmap]::new($gifSheet)
    try {
        Assert-True (Test-MultipleColors -Image $pngReview -Region ([System.Drawing.Rectangle]::new(16, 242, 300, 210))) 'second review-sheet row is blank'
        Assert-True (Test-GreenDominant -Color $gifReview.GetPixel(166, 365)) 'GIF review sheet did not use the midpoint frame color'
    }
    finally {
        $gifReview.Dispose()
        $pngReview.Dispose()
    }
}
finally {
    if ($KeepFixture) {
        Write-Output "fixture preserved at $tempRoot"
    }
    else {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

'info image tests passed'
