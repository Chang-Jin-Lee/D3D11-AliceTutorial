[CmdletBinding()]
param(
    [string]$Project24Png = 'docs/media/readme/24-Skinned-With-Bone-Structure.png',
    [string]$Project31Png = 'docs/media/readme/31-IBL.png',
    [string]$Project32Png = 'docs/media/readme/32-Sound-FMOD.png'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$failures = [Collections.Generic.List[string]]::new()

function Assert-True([bool]$Condition,[string]$Message) {
    if($Condition){ Write-Host "  ok   $Message" }
    else { Write-Host "  FAIL $Message"; $null=$script:failures.Add($Message) }
}
function Resolve-Png([string]$Path) {
    $candidate=if([IO.Path]::IsPathRooted($Path)){$Path}else{Join-Path $repoRoot $Path}
    return [IO.Path]::GetFullPath($candidate)
}
function Get-RegionStats([Drawing.Bitmap]$Bitmap,[Drawing.Rectangle]$Region,[int]$Step=3) {
    $dark=0; $bright=0; $count=0
    $colors=[Collections.Generic.HashSet[int]]::new()
    for($y=$Region.Top;$y -lt $Region.Bottom;$y+=$Step){for($x=$Region.Left;$x -lt $Region.Right;$x+=$Step){
        $c=$Bitmap.GetPixel($x,$y); $l=0.2126*$c.R+0.7152*$c.G+0.0722*$c.B
        if($l -lt 25){$dark++}; if($l -gt 120){$bright++}; $count++
        $null=$colors.Add((([int]$c.R -shr 4)-shl 8)-bor(([int]$c.G -shr 4)-shl 4)-bor([int]$c.B -shr 4))
    }}
    [pscustomobject]@{Dark=$dark;Bright=$bright;Count=$count;Colors=$colors.Count}
}
function Get-DifferenceFraction([Drawing.Bitmap]$A,[Drawing.Bitmap]$B,[Drawing.Rectangle]$Region,[int]$Step=3) {
    $different=0; $samples=0
    for($y=$Region.Top;$y -lt $Region.Bottom;$y+=$Step){for($x=$Region.Left;$x -lt $Region.Right;$x+=$Step){
        $aPixel=$A.GetPixel($x,$y); $bPixel=$B.GetPixel($x,$y)
        if(([Math]::Abs([int]$aPixel.R-[int]$bPixel.R)+[Math]::Abs([int]$aPixel.G-[int]$bPixel.G)+[Math]::Abs([int]$aPixel.B-[int]$bPixel.B))-gt 24){$different++}
        $samples++
    }}
    return $different/[double]$samples
}

Write-Host 'README capture evidence media contract'
$p24=[Drawing.Bitmap]::new((Resolve-Png $Project24Png))
$p31=[Drawing.Bitmap]::new((Resolve-Png $Project31Png))
$p32=[Drawing.Bitmap]::new((Resolve-Png $Project32Png))
try {
    foreach($pair in @(@('24',$p24),@('31',$p31),@('32',$p32))){
        Assert-True ($pair[1].Width -eq 1600 -and $pair[1].Height -eq 900) "Project $($pair[0]) PNG is 1600x900"
    }

    $different=0; $samples=0
    for($y=0;$y -lt 900;$y+=6){for($x=0;$x -lt 1600;$x+=6){
        $a=$p31.GetPixel($x,$y); $b=$p32.GetPixel($x,$y)
        if(([Math]::Abs([int]$a.R-[int]$b.R)+[Math]::Abs([int]$a.G-[int]$b.G)+[Math]::Abs([int]$a.B-[int]$b.B))-gt 24){$different++}
        $samples++
    }}
    $differenceFraction=$different/[double]$samples
    Assert-True ($differenceFraction -gt 0.005) `
        ("Project 32 visibly differs from Project 31 ({0:P2} changed samples, need > 0.5%)" -f $differenceFraction)

    # Project 32 owns a compact capture-only FMOD card at the top of Controls.
    # Compare exactly that visible UI region against Project 31 rather than
    # trusting source text or unrelated motion elsewhere in the frame.
    $audioRegion=[Drawing.Rectangle]::new(10,20,300,190)
    $audioDifference=Get-DifferenceFraction $p31 $p32 $audioRegion
    $audioStats=Get-RegionStats $p32 $audioRegion
    Assert-True ($audioDifference -gt 0.03) `
        ("Project 32 exposes distinct FMOD controls in the visible left UI ({0:P2} changed samples, need > 3%)" -f $audioDifference)
    Assert-True ($audioStats.Dark -gt ($audioStats.Count*0.20) -and $audioStats.Bright -gt 20 -and $audioStats.Colors -ge 8) `
        'Project 32 FMOD evidence region retains structured text, buttons, and status colour'

    # With the bone card docked at the far left, the character's outstretched
    # left arm remains observable in this literal scene-space corridor.
    $leftArm=Get-RegionStats $p24 ([Drawing.Rectangle]::new(540,190,200,200))
    Assert-True ($leftArm.Bright -gt 250) `
        "Project 24 keeps the front-facing subject and left arm clear of the bone panel (bright samples: $($leftArm.Bright), need > 250)"

    $bonePanel=Get-RegionStats $p24 ([Drawing.Rectangle]::new(20,20,400,430))
    Assert-True ($bonePanel.Dark -gt ($bonePanel.Count*0.20) -and $bonePanel.Bright -gt 40 -and $bonePanel.Colors -ge 8) `
        'Project 24 retains a structured left-side bone evidence panel'
}
finally { $p24.Dispose(); $p31.Dispose(); $p32.Dispose() }

if($failures.Count -gt 0){ Write-Host "README capture evidence media contract FAILED ($($failures.Count) assertion(s))"; exit 1 }
'README capture evidence media tests passed'
