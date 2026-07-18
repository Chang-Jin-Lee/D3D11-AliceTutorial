$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-DarkSpan {
    param([System.Drawing.Bitmap]$Bitmap)

    $minX = $Bitmap.Width
    $maxX = -1
    $darkPixels = 0
    foreach ($y in @(195, 210, 225, 240)) {
        for ($x = 0; $x -lt $Bitmap.Width; $x++) {
            $color = $Bitmap.GetPixel($x, $y)
            if ($color.R -lt 40 -and $color.G -lt 40 -and $color.B -lt 40) {
                $minX = [Math]::Min($minX, $x)
                $maxX = [Math]::Max($maxX, $x)
                $darkPixels++
            }
        }
    }

    return [pscustomobject]@{ MinX = $minX; MaxX = $maxX; DarkPixels = $darkPixels }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$captureScript = Join-Path $repoRoot 'tools\capture_readme_media.ps1'
$manifestPath = Join-Path $repoRoot 'tools\readme_media_manifest.json'
$ffmpegPath = (Get-Command ffmpeg -ErrorAction Stop).Source
$captureSource = Get-Content -Raw -LiteralPath $captureScript
Assert-True ($captureSource -match "'-gifflags',\s*'0'") 'presentation-pan GIFs must disable transparent frame differencing'
Assert-True ($captureSource -match 'palettegen=max_colors=\$\{colorCount\}:stats_mode=full') 'presentation-pan GIF palette must sample complete frames'
Assert-True ($captureSource -match 'paletteuse=dither=sierra2_4a') 'presentation-pan GIFs must use error-diffusion dithering'
Assert-True ($captureSource -match 'Invoke-PresentationPanGif[^\r\n]+-MaxColors 256') 'production presentation-pan GIFs must start with a 256-color palette'
$tempRoot = Join-Path $env:TEMP ('D3D11-presentation-pan-test-' + [Guid]::NewGuid().ToString('N'))
$sourcePath = Join-Path $tempRoot 'source.png'
$gifPath = Join-Path $tempRoot 'presentation-pan.gif'

New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
try {
    . $captureScript -Manifest $manifestPath -ValidateOnly | Out-Null

    $source = [System.Drawing.Bitmap]::new(804, 454)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($source)
        try {
            $graphics.Clear([System.Drawing.Color]::White)
            for ($y = 0; $y -lt $source.Height; $y += 16) {
                for ($x = 0; $x -lt $source.Width; $x += 16) {
                    $shade = 180 + (($x + $y) % 70)
                    $brush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb($shade, 220, 245))
                    try {
                        $graphics.FillRectangle($brush, $x, $y, 16, 16)
                    }
                    finally {
                        $brush.Dispose()
                    }
                }
            }
            $graphics.FillRectangle([System.Drawing.Brushes]::Black, 300, 180, 80, 80)
        }
        finally {
            $graphics.Dispose()
        }
        $source.Save($sourcePath, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $source.Dispose()
    }

    $details = Invoke-PresentationPanGif -FfmpegPath $ffmpegPath -PngPath $sourcePath -GifPath $gifPath -GifFps 8 -GifWidth 800 -GifHeight 450 -GifSeconds 4 -MaxColors 128
    Assert-True (Test-Path -LiteralPath $gifPath -PathType Leaf) 'presentation-pan GIF was not created'
    Assert-True ($details.Dimensions -eq '800x450') 'presentation-pan GIF dimensions mismatch'

    $gif = [System.Drawing.Image]::FromFile($gifPath)
    try {
        $dimension = [System.Drawing.Imaging.FrameDimension]::new($gif.FrameDimensionsList[0])
        $frameCount = $gif.GetFrameCount($dimension)
        Assert-True ($frameCount -eq 32) "presentation-pan GIF must contain 32 frames, found $frameCount"

        $bounds = @()
        foreach ($frameIndex in @(0, 8, 16, 24)) {
            [void]$gif.SelectActiveFrame($dimension, $frameIndex)
            $frame = [System.Drawing.Bitmap]::new($gif)
            try {
                $bounds += Get-DarkSpan -Bitmap $frame
            }
            finally {
                $frame.Dispose()
            }
        }

        Assert-True ((@($bounds.MinX | Select-Object -Unique)).Count -gt 1) 'presentation pan did not move the source image'
        foreach ($bound in $bounds) {
            Assert-True (($bound.MaxX - $bound.MinX) -lt 90) 'presentation-pan frame contains a horizontal ghost trail'
            Assert-True ($bound.DarkPixels -lt 340) 'presentation-pan frame accumulated ghost pixels'
        }
    }
    finally {
        $gif.Dispose()
    }

    $mediaManifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    $panProjects = @($mediaManifest.projects | Where-Object { $_.gifPresentationPan } | ForEach-Object { $_.number })
    Assert-True (($panProjects -join ',') -eq '01,28,33,37') 'presentation-pan project selection mismatch'
}
finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

'presentation pan GIF tests passed'
