$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Test-MultipleColors([System.Drawing.Image]$Image, [System.Drawing.Rectangle]$Region) {
    $colors = [System.Collections.Generic.HashSet[int]]::new()
    for ($y = $Region.Top; $y -lt $Region.Bottom; $y += 16) {
        for ($x = $Region.Left; $x -lt $Region.Right; $x += 16) {
            $null = $colors.Add($Image.GetPixel($x, $y).ToArgb())
        }
    }
    return $colors.Count -gt 1
}

Add-Type -AssemblyName System.Drawing

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$infoImageScript = Join-Path $repoRoot 'tools\generate_readme_info_images.ps1'
$reviewSheetScript = Join-Path $repoRoot 'tools\generate_readme_review_sheets.ps1'
$tempRoot = Join-Path $env:TEMP ('D3D11-readme-info-image-test-' + [Guid]::NewGuid().ToString('N'))
$mediaDir = Join-Path $tempRoot 'media'
$infoDir = Join-Path $mediaDir 'info'
$manifestPath = Join-Path $tempRoot 'manifest.json'
$sourcePng = Join-Path $mediaDir '01-fixture.png'
$sourceGif = Join-Path $mediaDir '01-fixture.gif'
$posterPath = Join-Path $infoDir '01-fixture-info.png'
$reviewDir = Join-Path $tempRoot 'review'
$pngSheet = Join-Path $reviewDir 'readme-png-review.png'
$gifSheet = Join-Path $reviewDir 'readme-gif-review.png'
$fixtureBitmap = $null
$fixtureGraphics = $null

New-Item -ItemType Directory -Path $infoDir -Force | Out-Null
try {
    $fixtureBitmap = [System.Drawing.Bitmap]::new(1600, 900)
    $fixtureGraphics = [System.Drawing.Graphics]::FromImage($fixtureBitmap)
    $fixtureGraphics.Clear([System.Drawing.Color]::Black)
    $fixtureGraphics.FillRectangle([System.Drawing.Brushes]::Crimson, 0, 0, 800, 450)
    $fixtureGraphics.FillRectangle([System.Drawing.Brushes]::ForestGreen, 800, 0, 800, 450)
    $fixtureGraphics.FillRectangle([System.Drawing.Brushes]::RoyalBlue, 0, 450, 800, 450)
    $fixtureGraphics.FillRectangle([System.Drawing.Brushes]::Gold, 800, 450, 800, 450)
    $fixtureBitmap.Save($sourcePng, [System.Drawing.Imaging.ImageFormat]::Png)
    $fixtureBitmap.Save($sourceGif, [System.Drawing.Imaging.ImageFormat]::Gif)

    @{
        runtimeDir = 'Dx11/bin'
        mediaDir = $mediaDir
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
        projects = @(
            @{
                number = '01'
                name = 'Fixture'
                directory = '01_RenderingQuadangle'
                exe = 'fixture.exe'
                image = '01-fixture.png'
                gif = '01-fixture.gif'
                infoImage = 'info/01-fixture-info.png'
                title = 'Fixture Poster Title With Enough Words To Fit'
                summary = '한국어 요약 텍스트로 D3D11 이미지 생성의 글꼴 대체와 줄바꿈을 검증합니다.'
                tags = @('D3D11', 'Fixture', 'Image Test')
                gifPhase = 'startup'
            }
        )
    } | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

    & pwsh -NoProfile -File $infoImageScript -Manifest $manifestPath -ProjectNumber '01'
    Assert-True (Test-Path -LiteralPath $posterPath -PathType Leaf) 'poster missing'

    $poster = [System.Drawing.Bitmap]::new($posterPath)
    try {
        Assert-True ($poster.Width -eq 1600 -and $poster.Height -eq 640) 'poster dimensions mismatch'
        Assert-True (Test-MultipleColors -Image $poster -Region ([System.Drawing.Rectangle]::new(40, 40, 940, 560))) 'screenshot region is blank or monochrome'
        Assert-True (Test-MultipleColors -Image $poster -Region ([System.Drawing.Rectangle]::new(1030, 64, 520, 512))) 'summary region is blank or monochrome'
    }
    finally {
        $poster.Dispose()
    }

    & pwsh -NoProfile -File $reviewSheetScript -Manifest $manifestPath -OutputDir $reviewDir
    Assert-True ((Test-Path -LiteralPath $pngSheet -PathType Leaf) -and (Test-Path -LiteralPath $gifSheet -PathType Leaf)) 'review sheets missing'
}
finally {
    if ($null -ne $fixtureGraphics) { $fixtureGraphics.Dispose() }
    if ($null -ne $fixtureBitmap) { $fixtureBitmap.Dispose() }
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

'info image tests passed'
