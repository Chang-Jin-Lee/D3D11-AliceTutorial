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
    for($i=0;$i -lt $frameCount;$i++) {
        $null=$gif.SelectActiveFrame($dimension,$i)
        $frame=[System.Drawing.Bitmap]::new($gif)
        try { $null=$hashes.Add([Convert]::ToBase64String($hasher.ComputeHash((Get-CharacterFrameBytes $frame)))) }
        finally { $frame.Dispose() }
    }
    Assert-True ($hashes.Count -ge 4) `
        "character region genuinely animates independently of HUD digits ($($hashes.Count) distinct quantized hashes, need >= 4)"
}
finally { $hasher.Dispose(); $gif.Dispose() }

if($failures.Count -gt 0) {
    Write-Host "Project 38 media contract FAILED ($($failures.Count) assertion(s))"
    exit 1
}
'project 38 observable media tests passed'
