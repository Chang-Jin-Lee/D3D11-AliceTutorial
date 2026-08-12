# Project 36 representative media contract.
#
# Project 36 is the portfolio showcase: an eight-second Dance -> BlendLayer -> IK
# -> Finish cycle rendered by four front-facing characters. The published PNG and
# GIF are the only evidence a reader of the README ever sees, so this test checks
# the media itself rather than the runtime that produced it.
#
# The -PngPath/-GifPath parameters let the same contract run against staged
# capture output before publication and against the published files afterwards.

[CmdletBinding()]
param(
    [string]$PngPath = 'docs/media/readme/36-AdvancedAnim-Sound-Click.png',
    [string]$GifPath = 'docs/media/readme/36-advanced-anim-sound-click.gif'
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

# --------------------------------------------------------------------------
# Contract constants. These mirror tools/readme_media_manifest.json and
# tools/verify_readme_media.ps1; they are restated rather than read so that a
# manifest edit cannot silently relax what the published media must look like.
# --------------------------------------------------------------------------
$expectedPngWidth = 1600
$expectedPngHeight = 900
$expectedGifWidth = 800
$expectedGifHeight = 450
$expectedGifFrameCount = 64
$expectedGifSeconds = 8.0
$gifSecondsTolerance = 0.5
$gifMaxBytes = 5242880

# verify_readme_media.ps1 rejects a frame whose 32x18 sampled luminance variance
# is at or below 4.0, and whose sampled colour count is below 8. A blank or
# single-colour capture must fail here for the same reason and by the same
# measure.
$minimumLuminanceVariance = 4.0
$minimumSampledColors = 8

# The deterministic phase cycle, in GIF frame indices at 8 fps. Frames 60..63 are
# the Finish beat and are deliberately unconstrained: the cycle ends there.
$phases = @(
    [pscustomobject]@{ Label = 'dance';       Start = 0;  End = 23 }
    [pscustomobject]@{ Label = 'blend-layer'; Start = 24; End = 41 }
    [pscustomobject]@{ Label = 'ccd-ik';      Start = 42; End = 59 }
)

# At least this many distinct central-region hashes per phase. A frozen render, a
# dropped animation update, or a capture that repeated one frame collapses to a
# single hash and fails.
$minimumDistinctPhaseHashes = 4

$failures = [System.Collections.Generic.List[string]]::new()

function Assert-True {
    param([bool]$Condition, [string]$Message)

    if ($Condition) {
        Write-Host "  ok   $Message"
    }
    else {
        Write-Host "  FAIL $Message"
        $null = $script:failures.Add($Message)
    }
}

function Resolve-MediaPath {
    param([string]$Path, [string]$Label)

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "$Label path is empty"
    }
    $candidate = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    $candidate = [System.IO.Path]::GetFullPath($candidate)
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "$Label not found: $candidate"
    }
    return $candidate
}

function Get-SampledRgb {
    # Same sampling grid verify_readme_media.ps1 uses, so "non-blank" means the
    # same thing in both places.
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [int]$Columns = 32,
        [int]$Rows = 18
    )

    $samples = [int[]]::new($Columns * $Rows)
    $sampleIndex = 0
    for ($row = 0; $row -lt $Rows; $row++) {
        $y = [Math]::Min($Bitmap.Height - 1, [int][Math]::Floor((($row + 0.5) * $Bitmap.Height) / $Rows))
        for ($column = 0; $column -lt $Columns; $column++) {
            $x = [Math]::Min($Bitmap.Width - 1, [int][Math]::Floor((($column + 0.5) * $Bitmap.Width) / $Columns))
            $color = $Bitmap.GetPixel($x, $y)
            $inverseAlpha = 255 - $color.A
            $red = [int][Math]::Round((($color.R * $color.A) + (255 * $inverseAlpha)) / 255.0)
            $green = [int][Math]::Round((($color.G * $color.A) + (255 * $inverseAlpha)) / 255.0)
            $blue = [int][Math]::Round((($color.B * $color.A) + (255 * $inverseAlpha)) / 255.0)
            $samples[$sampleIndex++] = ($red -shl 16) -bor ($green -shl 8) -bor $blue
        }
    }
    return $samples
}

function Get-LuminanceVariance {
    param([int[]]$RgbSamples)

    if ($RgbSamples.Count -eq 0) { return 0.0 }
    $luminances = [double[]]::new($RgbSamples.Count)
    $sum = 0.0
    for ($index = 0; $index -lt $RgbSamples.Count; $index++) {
        $rgb = $RgbSamples[$index]
        $red = ($rgb -shr 16) -band 0xFF
        $green = ($rgb -shr 8) -band 0xFF
        $blue = $rgb -band 0xFF
        $luminance = (0.2126 * $red) + (0.7152 * $green) + (0.0722 * $blue)
        $luminances[$index] = $luminance
        $sum += $luminance
    }
    $mean = $sum / $RgbSamples.Count
    $squaredDifference = 0.0
    foreach ($luminance in $luminances) {
        $difference = $luminance - $mean
        $squaredDifference += $difference * $difference
    }
    return $squaredDifference / $RgbSamples.Count
}

function Get-UniqueSampledColorCount {
    param([int[]]$RgbSamples)

    $unique = [System.Collections.Generic.HashSet[int]]::new()
    foreach ($sample in $RgbSamples) { $null = $unique.Add($sample) }
    return $unique.Count
}

function Assert-NonBlankFrame {
    param([System.Drawing.Bitmap]$Bitmap, [string]$Label)

    $samples = @(Get-SampledRgb -Bitmap $Bitmap)
    $uniqueColors = Get-UniqueSampledColorCount -RgbSamples $samples
    $variance = Get-LuminanceVariance -RgbSamples $samples
    Assert-True ($uniqueColors -ge $minimumSampledColors) `
        ("$Label is non-blank: sampled colour count $uniqueColors (need >= $minimumSampledColors)")
    Assert-True ($variance -gt $minimumLuminanceVariance) `
        ("$Label carries image content: sampled luminance variance {0:N3} (need > {1:N1})" -f $variance, $minimumLuminanceVariance)
}

function Assert-ImageDimensions {
    param([System.Drawing.Image]$Image, [int]$ExpectedWidth, [int]$ExpectedHeight, [string]$Label)

    Assert-True ($Image.Width -eq $ExpectedWidth -and $Image.Height -eq $ExpectedHeight) `
        ("$Label is ${ExpectedWidth}x${ExpectedHeight} (found $($Image.Width)x$($Image.Height))")
}

function Get-CentralModelRegion {
    # The characters stand centred and fill roughly 70% of the frame height. This
    # box brackets them while excluding the HUD panel, which the runtime anchors
    # at client (24,24) and which is phase-constant anyway: motion found here is
    # motion of the models, not of the caption.
    param([int]$Width, [int]$Height)

    $x = [int][Math]::Round($Width * 0.25)
    $y = [int][Math]::Round($Height * 0.15)
    $regionWidth = [int][Math]::Round($Width * 0.50)
    $regionHeight = [int][Math]::Round($Height * 0.75)
    if (($x + $regionWidth) -gt $Width -or ($y + $regionHeight) -gt $Height) {
        throw "central model region ($x,$y) ${regionWidth}x${regionHeight} does not fit a ${Width}x${Height} frame"
    }
    return [pscustomobject]@{ X = $x; Y = $y; Width = $regionWidth; Height = $regionHeight }
}

function Get-RegionSampleHash {
    # Hash of a 64x36 sample grid over the central model region, each channel
    # composited over white and quantised to five bits. Quantisation keeps GIF
    # dither noise from masquerading as animation, so a distinct hash means the
    # rendered pose actually changed.
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [object]$Region,
        [System.Security.Cryptography.SHA256]$Hasher,
        [int]$Columns = 64,
        [int]$Rows = 36
    )

    $rectangle = [System.Drawing.Rectangle]::new($Region.X, $Region.Y, $Region.Width, $Region.Height)
    $locked = $Bitmap.LockBits($rectangle, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $stride = $locked.Stride
        if ($stride -le 0) {
            throw "unsupported bottom-up bitmap stride $stride"
        }
        $pixels = [byte[]]::new($stride * $Region.Height)
        [System.Runtime.InteropServices.Marshal]::Copy($locked.Scan0, $pixels, 0, $pixels.Length)
    }
    finally {
        $Bitmap.UnlockBits($locked)
    }

    $samples = [byte[]]::new($Columns * $Rows * 3)
    $sampleIndex = 0
    for ($row = 0; $row -lt $Rows; $row++) {
        $sourceY = [Math]::Min($Region.Height - 1, [int][Math]::Floor((($row + 0.5) * $Region.Height) / $Rows))
        $rowOffset = $sourceY * $stride
        for ($column = 0; $column -lt $Columns; $column++) {
            $sourceX = [Math]::Min($Region.Width - 1, [int][Math]::Floor((($column + 0.5) * $Region.Width) / $Columns))
            $offset = $rowOffset + ($sourceX * 4)
            $alpha = $pixels[$offset + 3]
            $inverseAlpha = 255 - $alpha
            $red = (($pixels[$offset + 2] * $alpha) + (255 * $inverseAlpha)) / 255
            $green = (($pixels[$offset + 1] * $alpha) + (255 * $inverseAlpha)) / 255
            $blue = (($pixels[$offset] * $alpha) + (255 * $inverseAlpha)) / 255
            $samples[$sampleIndex++] = [byte]([int]$red -shr 3)
            $samples[$sampleIndex++] = [byte]([int]$green -shr 3)
            $samples[$sampleIndex++] = [byte]([int]$blue -shr 3)
        }
    }

    return [System.Convert]::ToBase64String($Hasher.ComputeHash($samples))
}

function Assert-PhaseMotion {
    param(
        [string[]]$Frames,
        [int]$Start,
        [int]$End,
        [string]$Label
    )

    if ($Frames.Count -le $End) {
        Assert-True $false ("$Label phase frames $Start..$End exist (GIF has only $($Frames.Count) frames)")
        return
    }

    $distinct = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    for ($index = $Start; $index -le $End; $index++) {
        $null = $distinct.Add($Frames[$index])
    }
    Assert-True ($distinct.Count -ge $minimumDistinctPhaseHashes) `
        ("$Label phase animates across frames $Start..$End`: $($distinct.Count) distinct central-region hashes (need >= $minimumDistinctPhaseHashes)")
}

# --------------------------------------------------------------------------
# PNG
# --------------------------------------------------------------------------
$resolvedPng = Resolve-MediaPath -Path $PngPath -Label 'Project 36 PNG'
$resolvedGif = Resolve-MediaPath -Path $GifPath -Label 'Project 36 GIF'

Write-Host "Project 36 media contract"
Write-Host "  PNG: $resolvedPng"
Write-Host "  GIF: $resolvedGif"

$png = $null
try {
    $png = [System.Drawing.Bitmap]::new($resolvedPng)
    Assert-ImageDimensions -Image $png -ExpectedWidth $expectedPngWidth -ExpectedHeight $expectedPngHeight -Label 'PNG'
    Assert-NonBlankFrame -Bitmap $png -Label 'PNG'
}
finally {
    if ($null -ne $png) { $png.Dispose() }
}

# --------------------------------------------------------------------------
# GIF
# --------------------------------------------------------------------------
$gifBytes = (Get-Item -LiteralPath $resolvedGif).Length
Assert-True ($gifBytes -le $gifMaxBytes) `
    ("GIF is within the 5 MiB limit: $gifBytes bytes (limit $gifMaxBytes)")

$gif = $null
$hasher = $null
try {
    $gif = [System.Drawing.Image]::FromFile($resolvedGif)
    Assert-ImageDimensions -Image $gif -ExpectedWidth $expectedGifWidth -ExpectedHeight $expectedGifHeight -Label 'GIF'

    $frameDimension = [System.Drawing.Imaging.FrameDimension]::new($gif.FrameDimensionsList[0])
    $gifFrameCount = $gif.GetFrameCount($frameDimension)
    Assert-True ($gifFrameCount -eq $expectedGifFrameCount) `
        ("GIF holds $expectedGifFrameCount frames (found $gifFrameCount)")

    $gifDurationSeconds = 0.0
    $delayBytes = $gif.GetPropertyItem(0x5100).Value
    if ($delayBytes.Length -lt ($gifFrameCount * 4)) {
        Assert-True $false "GIF decoded delay table is complete (found $($delayBytes.Length) bytes for $gifFrameCount frames)"
    }
    else {
        $totalDelayHundredths = [uint64]0
        for ($index = 0; $index -lt $gifFrameCount; $index++) {
            $totalDelayHundredths += [System.BitConverter]::ToUInt32($delayBytes, $index * 4)
        }
        $gifDurationSeconds = $totalDelayHundredths / 100.0
        Assert-True ([Math]::Abs($gifDurationSeconds - $expectedGifSeconds) -le $gifSecondsTolerance) `
            ("GIF decodes to {0:N1}s +/- {1:N1}s (found {2:N2}s)" -f $expectedGifSeconds, $gifSecondsTolerance, $gifDurationSeconds)
    }

    $region = Get-CentralModelRegion -Width $gif.Width -Height $gif.Height
    Write-Host ("  central model region: ({0},{1}) {2}x{3}" -f $region.X, $region.Y, $region.Width, $region.Height)

    $hasher = [System.Security.Cryptography.SHA256]::Create()
    $decodedFrames = [string[]]::new($gifFrameCount)
    for ($index = 0; $index -lt $gifFrameCount; $index++) {
        $null = $gif.SelectActiveFrame($frameDimension, $index)
        $frameBitmap = [System.Drawing.Bitmap]$gif
        if ($index -eq 0) {
            Assert-NonBlankFrame -Bitmap $frameBitmap -Label 'GIF frame 0'
        }
        $decodedFrames[$index] = Get-RegionSampleHash -Bitmap $frameBitmap -Region $region -Hasher $hasher
    }

    $distinctOverall = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
    foreach ($frameHash in $decodedFrames) { $null = $distinctOverall.Add($frameHash) }
    Write-Host "  distinct central-region hashes across the whole GIF: $($distinctOverall.Count) of $gifFrameCount"

    foreach ($phase in $phases) {
        Assert-PhaseMotion -Frames $decodedFrames -Start $phase.Start -End $phase.End -Label $phase.Label
    }
}
finally {
    if ($null -ne $hasher) { $hasher.Dispose() }
    if ($null -ne $gif) { $gif.Dispose() }
}

if ($failures.Count -gt 0) {
    Write-Host ''
    Write-Host "Project 36 media contract FAILED ($($failures.Count) assertion(s)):"
    foreach ($failure in $failures) {
        Write-Host "  - $failure"
    }
    exit 1
}

Write-Host ''
Write-Host 'project 36 portfolio media tests passed'
