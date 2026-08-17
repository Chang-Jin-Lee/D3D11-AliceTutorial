# Project 36 representative media contract.
#
# Project 36 is the portfolio showcase: four front-facing characters rotating
# through the VRM_* clips in twelve-second sets, with three technique windows cut
# from each set's own clock (App_PortfolioShowcase.inl):
#
#   set time  0.0 - 0.6    cross-fade out of the previous set's line-up, and only
#                          when cycle > 0 - set 0 has no predecessor to fade from
#   set time  4.0 - 7.0    upper-body layer on slot 0
#   set time  8.0 - 11.4   CCD IK on slot 0's left hand
#
# The published PNG and GIF are the only evidence a reader of the README ever
# sees, so this test checks the media itself rather than the runtime that produced
# it.
#
# ---------------------------------------------------------------------------
# The capture window, and why it is thirteen seconds
# ---------------------------------------------------------------------------
# tools/readme_media_manifest.json drives the GIF with a left click at frame zero,
# which calls ResetPortfolioShowcase() and puts showcase time 0 on GIF frame 0. So
# the only window the capture can address deterministically is [0, gifSeconds), and
# GIF frame k is showcase time k/8 s.
#
# The layer and the IK both run in set 0, so both are already inside [0, 12).
# The cross-fade is not: it is guarded on cycle > 0, so its first occurrence is the
# set boundary at t = 12.0 and it closes at t = 12.6. The last frame that can carry
# it is frame 100 (t = 12.500; frame 101 is t = 12.625, already past the close), so
# the capture needs at least 101 frames, i.e. at least 12.625 s. gifSeconds must be
# a whole number (Test-ReadmeMediaManifest requires a positive integer), so thirteen
# seconds - 104 frames at 8 fps - is the SHORTEST capture that can contain all three
# techniques at once.
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
$expectedGifFrameCount = 104
$expectedGifSeconds = 13.0
$gifSecondsTolerance = 0.5
$gifMaxBytes = 5242880

# verify_readme_media.ps1 rejects a frame whose 32x18 sampled luminance variance
# is at or below 4.0, and whose sampled colour count is below 8. A blank or
# single-colour capture must fail here for the same reason and by the same
# measure.
$minimumLuminanceVariance = 4.0
$minimumSampledColors = 8

# The three technique windows, in GIF frame indices at 8 fps, derived from the
# runtime constants above with frame k at showcase time k/8 s:
#
#   upper-body layer  frames 34..53   t 4.250 .. 6.625   set 0's 4.0-7.0 window
#   ccd-ik            frames 66..89   t 8.250 .. 11.125  set 0's 8.0-11.4 window
#   cross-fade        frames 98..100  t 12.250 .. 12.500 set 1's 0.0-0.6 window,
#                                                        which is t 12.0-12.6
#
# Every range is inset from its window's edges rather than filling it, because a
# captured frame's CONTENT is not from the instant the frame was sampled. The two
# error sources do NOT push the same way:
#
#   EARLIER (the dominant one). The showcase throttles its back-buffer publication
#   with kPortfolioBackbufferMinIntervalMs = 1000ull / 12ull, which is integer
#   division - 83 ms, not 83.33 - and compares it against GetTickCount64()
#   (App_PortfolioShowcase.inl), whose ~15.6 ms tick means the first tick at or
#   after the deadline is ~94 ms out. So the real publication period is ~94 ms, and
#   the WIC encode and the MoveFileExW that follow are on top of that. A sample
#   therefore reads a render that is at least one encode old and up to ~94 ms +
#   encode old: call the budget up to ~0.12 s. A publication dropped on a sharing
#   violation (see Copy-ReadmeBackbufferPng in tools/capture_readme_media.ps1)
#   doubles that for one interval.
#
#   LATER. The capture paces its frames with
#   Start-Sleep -Milliseconds ([Math]::Min(10, remaining)) and only re-reads the
#   clock after each chunk, so the last sleep of a frame can overshoot by the
#   ~15 ms system timer granularity and sample the frame that much AFTER its
#   nominal k/8 mark, pushing its content later.
#
# Budget, then: content from ~0.12 s earlier to ~0.02 s later than k/8. All three
# opening edges are inset by 0.25 s, which covers the earlier error with room for
# one dropped publication; all three closing edges keep at least 0.1 s, which
# covers the later error several times over. The cross-fade's opening inset used to
# be 0.125 s, i.e. frame 97 - which does not cover the ~0.12 s earlier budget at
# all, so frame 97 can carry a render from before the fade even opened. The range
# now starts at 98.
#
# The cross-fade range is three frames because that is all a 0.6 s window inset by
# 0.25 s can hold - frames 98, 99 and 100, with 101 already past the close. Frames
# unclaimed by any range - the base-clip gaps at frames 0..33, 54..65 and 90..97,
# and the tail at frames 101..103 - are deliberately unconstrained: no technique is
# running there, so there is nothing to assert about them.
#
# Nothing here names a "dance" or "finish" beat. The showcase has no such phases:
# every slot plays a VRM_* clip continuously for the whole capture, and the three
# ranges above are the only intervals in which a NAMED technique is on.
#
# MinimumDistinctHashes is the floor on distinct central-region hashes inside the
# range. A frozen render, a dropped animation update, or a capture that repeated
# one frame collapses to a single hash and fails. The floor is PER PHASE rather
# than shared because the ranges are not the same length and duplicates are not
# impossible:
#
#   Copy-ReadmeBackbufferPng opens the published PNG with FileShare.ReadWrite,
#   which does not include FILE_SHARE_DELETE, so while that handle is open the
#   publisher's MoveFileExW replace fails and the frame is dropped - and the
#   publisher's throttle has already advanced, so the next publication is a further
#   ~94 ms out. The resulting ~190 ms gap is longer than the 125 ms sample
#   interval, and two consecutive samples then read the identical file and hash
#   identically. Moving the PNG encode out of that handle's scope cut the hold from
#   ~58 ms to ~14 ms of each 125 ms period - measured on a real 1600x900 published
#   frame - which makes a duplicate uncommon, not impossible. The tool's 100 ms
#   retry after a transient open failure can collapse a pair the same way.
#
#   upper-body layer  20 frames, floor 4 - a duplicate or two costs nothing
#   ccd-ik            24 frames, floor 4 - likewise
#   cross-fade         3 frames, floor 2 - one duplicate has to be survivable
#
# On the cross-fade, a ~14 ms hold in a 125 ms period is roughly a one-in-ten
# chance of a duplicate per frame boundary, and the range has two boundaries, so
# demanding all 3 hashes distinct would redden about one good capture in five.
# Demanding 2 fires only when BOTH boundaries duplicate, about one run in a
# hundred, and it still catches what this assertion exists to catch: a render that
# stopped moving during the fade collapses all three frames onto one hash.
$phases = @(
    [pscustomobject]@{ Label = 'upper-body layer'; Start = 34; End = 53;  MinimumDistinctHashes = 4 }
    [pscustomobject]@{ Label = 'ccd-ik';           Start = 66; End = 89;  MinimumDistinctHashes = 4 }
    [pscustomobject]@{ Label = 'cross-fade';       Start = 98; End = 100; MinimumDistinctHashes = 2 }
)

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
    # The box that brackets the whole cast.
    #
    # HORIZONTAL. The four characters are spread across the frame, not clustered in
    # the middle: App_Lifecycle.inl places them at ndc x -0.719 / -0.241 / +0.240 /
    # +0.719, which is 14.1% / 38.0% / 62.0% / 86.0% of the frame width. A typical
    # pose spans ~0.43 ndc, so the outer two reach out to roughly 3% and 97%. The
    # old 25%-75% box therefore sampled only the inner two characters and missed
    # half the cast, so half the animation could freeze without this test noticing.
    #
    # VERTICAL. The cast fills ~60% of the frame height (see the camera derivation
    # in App_Lifecycle.inl: character height 126.5 world units at scale 80, visible
    # height 0.72794 * 289.6). With the 2 degree pitch the bodies run from about 20%
    # to 80% of the frame, so 15%-90% brackets them with margin at both ends.
    #
    # It is that vertical bound, not the horizontal one, that excludes the HUD
    # panel: the runtime anchors it at client (24,24) at 320x84, which is the top
    # 12% of the frame. Keeping it out still matters for the same reason as before -
    # motion found in this box must be motion of the models, not of the caption -
    # but the caption is phase-constant within a range anyway.
    param([int]$Width, [int]$Height)

    $x = [int][Math]::Round($Width * 0.03)
    $y = [int][Math]::Round($Height * 0.15)
    $regionWidth = [int][Math]::Round($Width * 0.94)
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
        [int]$MinimumDistinctHashes,
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
    Assert-True ($distinct.Count -ge $MinimumDistinctHashes) `
        ("$Label phase animates across frames $Start..$End`: $($distinct.Count) distinct central-region hashes (need >= $MinimumDistinctHashes)")
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
        Assert-PhaseMotion -Frames $decodedFrames -Start $phase.Start -End $phase.End `
            -MinimumDistinctHashes $phase.MinimumDistinctHashes -Label $phase.Label
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
