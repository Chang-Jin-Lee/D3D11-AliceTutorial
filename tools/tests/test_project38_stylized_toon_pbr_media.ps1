[CmdletBinding()]
param(
    [string]$PngPath = 'docs/media/readme/38-StylizedToonPBR.png',
    [string]$GifPath = 'docs/media/readme/38-StylizedToonPBR.gif'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$failures = [System.Collections.Generic.List[string]]::new()

function Assert-True([bool]$Condition, [string]$Message) {
    if ($Condition) { Write-Host "  ok   $Message" }
    else { Write-Host "  FAIL $Message"; $null = $script:failures.Add($Message) }
}

function Resolve-MediaPath([string]$Path, [string]$Label) {
    $candidate = if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    $candidate = [IO.Path]::GetFullPath($candidate)
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) { throw "$Label not found: $candidate" }
    return $candidate
}

function Get-RegionPixels([System.Drawing.Bitmap]$Bitmap, [System.Drawing.Rectangle]$Region) {
    $values = [System.Collections.Generic.List[object]]::new()
    for ($y = $Region.Top; $y -lt $Region.Bottom; $y += 3) {
        for ($x = $Region.Left; $x -lt $Region.Right; $x += 3) {
            $color = $Bitmap.GetPixel($x, $y)
            $values.Add([pscustomobject]@{ R = [int]$color.R; G = [int]$color.G; B = [int]$color.B })
        }
    }
    return $values
}

function Get-VisibleWarmthStatistics(
    [System.Drawing.Bitmap]$Bitmap,
    [System.Drawing.Rectangle]$Region,
    [double]$MinimumLuminance = 100.0
) {
    $count = 0
    $redBlueDelta = 0.0
    $saturation = 0.0
    for ($y = $Region.Top; $y -lt $Region.Bottom; $y += 2) {
        for ($x = $Region.Left; $x -lt $Region.Right; $x += 2) {
            $color = $Bitmap.GetPixel($x, $y)
            $luminance = 0.2126 * $color.R + 0.7152 * $color.G + 0.0722 * $color.B
            if ($luminance -le $MinimumLuminance -or $color.R -lt $color.B) { continue }

            $maximum = [Math]::Max($color.R, [Math]::Max($color.G, $color.B))
            $minimum = [Math]::Min($color.R, [Math]::Min($color.G, $color.B))
            $redBlueDelta += $color.R - $color.B
            if ($maximum -gt 0) { $saturation += ($maximum - $minimum) / $maximum }
            ++$count
        }
    }

    return [pscustomobject]@{
        Count = $count
        MeanRedBlueDelta = if ($count -gt 0) { $redBlueDelta / $count } else { 0.0 }
        MeanSaturation = if ($count -gt 0) { $saturation / $count } else { 0.0 }
    }
}

function Get-CharacterFrameBytes([System.Drawing.Bitmap]$Bitmap) {
    # The HUD ends left of x=440 in the 800x450 GIF. This literal right-side box
    # brackets the published character while excluding every changing timing digit.
    $region = [System.Drawing.Rectangle]::new(440, 55, 250, 355)
    $bytes = [byte[]]::new(50 * 71 * 3)
    $offset = 0
    for ($row = 0; $row -lt 71; ++$row) {
        $y = $region.Top + [Math]::Min($region.Height - 1, $row * 5)
        for ($column = 0; $column -lt 50; ++$column) {
            $x = $region.Left + [Math]::Min($region.Width - 1, $column * 5)
            $color = $Bitmap.GetPixel($x, $y)
            $bytes[$offset++] = [byte]([int]$color.R -shr 3)
            $bytes[$offset++] = [byte]([int]$color.G -shr 3)
            $bytes[$offset++] = [byte]([int]$color.B -shr 3)
        }
    }
    return $bytes
}

$resolvedPng = Resolve-MediaPath $PngPath 'Project 38 PNG'
$resolvedGif = Resolve-MediaPath $GifPath 'Project 38 GIF'
Write-Host 'Project 38 observable media contract'

$png = [System.Drawing.Bitmap]::new($resolvedPng)
try {
    Assert-True ($png.Width -eq 1600 -and $png.Height -eq 900) "PNG is 1600x900"

    $background = @(Get-RegionPixels $png ([System.Drawing.Rectangle]::new(850, 20, 700, 130)))
    $brightBackground = @($background | Where-Object { [Math]::Max($_.R, [Math]::Max($_.G, $_.B)) -gt 12 }).Count
    Assert-True ($brightBackground -le 5) "top-right background remains black (bright samples: $brightBackground)"

    $subjectPixels = @(Get-RegionPixels $png ([System.Drawing.Rectangle]::new(900, 150, 450, 650)) |
        Where-Object { (0.2126 * $_.R + 0.7152 * $_.G + 0.0722 * $_.B) -gt 35 })
    Assert-True ($subjectPixels.Count -gt 1000) "right-side character is visibly framed (bright samples: $($subjectPixels.Count))"
    if ($subjectPixels.Count -gt 0) {
        $meanR = ($subjectPixels | Measure-Object R -Average).Average
        $meanG = ($subjectPixels | Measure-Object G -Average).Average
        $meanB = ($subjectPixels | Measure-Object B -Average).Average
        $spread = [Math]::Max($meanR, [Math]::Max($meanG, $meanB)) - [Math]::Min($meanR, [Math]::Min($meanG, $meanB))
        Assert-True ($spread -lt 24.0) ("character lighting stays neutral/clean: mean RGB {0:N1}/{1:N1}/{2:N1}, spread {3:N1}" -f $meanR,$meanG,$meanB,$spread)
    }

    # These hand-measured boxes isolate the exposed lower legs and the white lace skirt in the
    # deterministic README pose. The authored textures are already different: true skin is warm
    # peach while the garment is neutral white. A material-aware toon response must preserve that
    # separation after ACES instead of compressing both surfaces into the same near-white value.
    $skinWarmth = Get-VisibleWarmthStatistics $png ([System.Drawing.Rectangle]::new(1040, 520, 130, 210))
    # Sample the centre panel of the white skirt. The wider character rectangle also contains both
    # forearms and the exposed thigh, which would make a skin improvement look like cloth tinting.
    # The lace is partially transparent over warm skin. Restrict the cloth measurement to the
    # bright opaque weave so the test measures the garment rather than the skin seen through it.
    $clothWarmth = Get-VisibleWarmthStatistics $png ([System.Drawing.Rectangle]::new(1080, 435, 70, 105)) 200.0
    Assert-True ($skinWarmth.Count -gt 1500) "lower-leg skin region contains enough visible samples ($($skinWarmth.Count))"
    Assert-True ($clothWarmth.Count -gt 1200) "white-skirt region contains enough visible samples ($($clothWarmth.Count))"
    Assert-True ($skinWarmth.MeanRedBlueDelta -ge 12.0 -and $skinWarmth.MeanSaturation -ge 0.06) `
        ("true skin retains a warm readable tone after ACES (R-B {0:N1}, saturation {1:P1})" -f $skinWarmth.MeanRedBlueDelta,$skinWarmth.MeanSaturation)
    Assert-True ($clothWarmth.MeanRedBlueDelta -le 7.0 -and $clothWarmth.MeanSaturation -le 0.04) `
        ("white clothing remains neutral while skin is warmed (R-B {0:N1}, saturation {1:P1})" -f $clothWarmth.MeanRedBlueDelta,$clothWarmth.MeanSaturation)
    Assert-True (($skinWarmth.MeanRedBlueDelta - $clothWarmth.MeanRedBlueDelta) -ge 7.0) `
        ("skin and white clothing stay visibly separated (R-B gap {0:N1})" -f ($skinWarmth.MeanRedBlueDelta - $clothWarmth.MeanRedBlueDelta))

    $minX=1600; $maxX=-1; $minY=900; $maxY=-1
    for($y=5;$y -lt 880;$y+=2) { for($x=820;$x -lt 1500;$x+=2) {
        $c=$png.GetPixel($x,$y); if((0.2126*$c.R+0.7152*$c.G+0.0722*$c.B)-gt 28) {
            $minX=[Math]::Min($minX,$x); $maxX=[Math]::Max($maxX,$x); $minY=[Math]::Min($minY,$y); $maxY=[Math]::Max($maxY,$y)
        }
    } }
    $height = $maxY - $minY + 1
    $centerX = ($minX + $maxX) / 2.0
    Assert-True ($height -ge 480 -and $height -le 650 -and $centerX -ge 1000 -and $centerX -le 1250) `
        "character framing stays clear of the HUD (bounds x=$minX..$maxX y=$minY..$maxY)"
}
finally { $png.Dispose() }

$gif = [System.Drawing.Image]::FromFile($resolvedGif)
$hasher = [Security.Cryptography.SHA256]::Create()
try {
    Assert-True ($gif.Width -eq 800 -and $gif.Height -eq 450) "GIF is 800x450"
    $dimension = [System.Drawing.Imaging.FrameDimension]::new($gif.FrameDimensionsList[0])
    $frameCount = $gif.GetFrameCount($dimension)
    Assert-True ($frameCount -eq 32) "GIF contains 32 frames (found $frameCount)"

    $delayBytes = $gif.GetPropertyItem(0x5100).Value
    $duration = 0.0
    for($i=0;$i -lt $frameCount;$i++) { $duration += [BitConverter]::ToUInt32($delayBytes,$i*4) / 100.0 }
    Assert-True ([Math]::Abs($duration - 4.0) -le 0.5) ("GIF duration is about four seconds (found {0:N2}s)" -f $duration)

    $hashes = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $appearanceFrames = [Collections.Generic.List[object]]::new()
    for($i=0;$i -lt $frameCount;$i++) {
        $null=$gif.SelectActiveFrame($dimension,$i)
        $frame=[System.Drawing.Bitmap]::new($gif)
        try {
            $null=$hashes.Add([Convert]::ToBase64String($hasher.ComputeHash((Get-CharacterFrameBytes $frame))))
            if ($i -eq 0 -or $i -eq ($frameCount - 1)) {
                # GIF output is half-resolution. Checking both ends of the animation prevents a
                # freshly captured PNG from masking a stale or differently graded motion asset.
                $appearanceFrames.Add([pscustomobject]@{
                    Frame = $i
                    Skin = Get-VisibleWarmthStatistics $frame ([System.Drawing.Rectangle]::new(520, 260, 65, 105))
                    Cloth = Get-VisibleWarmthStatistics $frame ([System.Drawing.Rectangle]::new(540, 217, 35, 53)) 200.0
                })
            }
        }
        finally { $frame.Dispose() }
    }
    Assert-True ($hashes.Count -ge 4) `
        "character region genuinely animates independently of HUD digits ($($hashes.Count) distinct quantized hashes, need >= 4)"

    foreach ($appearance in $appearanceFrames) {
        $skin = $appearance.Skin
        $cloth = $appearance.Cloth
        Assert-True ($skin.Count -gt 450 -and $cloth.Count -gt 250) `
            "GIF frame $($appearance.Frame) contains measurable skin/cloth samples ($($skin.Count)/$($cloth.Count))"
        Assert-True ($skin.MeanRedBlueDelta -ge 12.0 -and $skin.MeanSaturation -ge 0.06) `
            ("GIF frame {0} retains warm skin (R-B {1:N1}, saturation {2:P1})" -f $appearance.Frame,$skin.MeanRedBlueDelta,$skin.MeanSaturation)
        Assert-True ($cloth.MeanRedBlueDelta -le 8.0 -and $cloth.MeanSaturation -le 0.045) `
            ("GIF frame {0} retains neutral white clothing (R-B {1:N1}, saturation {2:P1})" -f $appearance.Frame,$cloth.MeanRedBlueDelta,$cloth.MeanSaturation)
        Assert-True (($skin.MeanRedBlueDelta - $cloth.MeanRedBlueDelta) -ge 7.0) `
            ("GIF frame {0} separates skin and clothing (R-B gap {1:N1})" -f $appearance.Frame,($skin.MeanRedBlueDelta - $cloth.MeanRedBlueDelta))
    }
}
finally { $hasher.Dispose(); $gif.Dispose() }

if($failures.Count -gt 0) {
    Write-Host "Project 38 media contract FAILED ($($failures.Count) assertion(s))"
    exit 1
}
'project 38 observable media tests passed'
