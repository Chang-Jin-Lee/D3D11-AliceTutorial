$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Read-BigEndianUInt32([byte[]]$Bytes, [int]$Offset) {
    return [uint32]((([uint32]$Bytes[$Offset]) -shl 24) -bor (([uint32]$Bytes[$Offset + 1]) -shl 16) -bor (([uint32]$Bytes[$Offset + 2]) -shl 8) -bor ([uint32]$Bytes[$Offset + 3]))
}

function Read-PngHeader([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $signature = [byte[]](137, 80, 78, 71, 13, 10, 26, 10)
    $validSignature = $bytes.Length -ge 26
    for ($index = 0; $validSignature -and $index -lt $signature.Length; $index++) {
        $validSignature = $bytes[$index] -eq $signature[$index]
    }
    Assert-True $validSignature "invalid PNG signature or header: $Path"
    Assert-True ([Text.Encoding]::ASCII.GetString($bytes, 12, 4) -ceq 'IHDR') "PNG IHDR missing: $Path"
    return [pscustomobject]@{
        Width = Read-BigEndianUInt32 $bytes 16
        Height = Read-BigEndianUInt32 $bytes 20
        BitDepth = [int]$bytes[24]
        ColorType = [int]$bytes[25]
    }
}

function Test-RequiredIcoDirectory([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 6) { return $false }
    $reserved = [BitConverter]::ToUInt16($bytes, 0)
    $type = [BitConverter]::ToUInt16($bytes, 2)
    $count = [BitConverter]::ToUInt16($bytes, 4)
    if ($reserved -ne 0 -or $type -ne 1 -or $count -ne 9 -or $bytes.Length -lt (6 + (16 * $count))) { return $false }
    $sizes = [Collections.Generic.List[int]]::new()
    for ($index = 0; $index -lt $count; $index++) {
        $offset = 6 + (16 * $index)
        $width = if ($bytes[$offset] -eq 0) { 256 } else { [int]$bytes[$offset] }
        $height = if ($bytes[$offset + 1] -eq 0) { 256 } else { [int]$bytes[$offset + 1] }
        $imageSize = [BitConverter]::ToUInt32($bytes, $offset + 8)
        $imageOffset = [BitConverter]::ToUInt32($bytes, $offset + 12)
        if ($width -ne $height -or $imageSize -eq 0 -or ([uint64]$imageOffset + [uint64]$imageSize) -gt [uint64]$bytes.Length) { return $false }
        $sizes.Add($width)
    }
    return (($sizes | Sort-Object) -join ',') -ceq '16,20,24,32,40,48,64,128,256'
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifestPath = Join-Path $repoRoot 'tools\readme_media_manifest.json'
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$solutionText = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\TutorialApp.sln')
$solutionProjectNames = @(
    [regex]::Matches($solutionText, '(?m)^Project\("[^"]+"\) = "([^"]+)", "([^"]+\.vcxproj)"') |
        ForEach-Object { $_.Groups[1].Value } |
        Where-Object { $_ -ne 'Common' }
)

Assert-True ([int]$manifest.expectedProjectCount -eq 37) 'expectedProjectCount must be 37'
Assert-True (@($manifest.projects).Count -eq 37) 'manifest must contain 37 projects'
Assert-True ($solutionProjectNames.Count -eq 37) "solution must contain exactly 37 application projects, got $($solutionProjectNames.Count)"
Assert-True (@($solutionProjectNames | Sort-Object -Unique).Count -eq 37) 'solution application projects must be unique'
foreach ($solutionProjectName in $solutionProjectNames) {
    Assert-True ($solutionProjectName -match '^\d{2}_[A-Za-z0-9_]+$') "invalid solution application project name: $solutionProjectName"
}
$manifestDirectories = @($manifest.projects | ForEach-Object { [string]$_.directory })
foreach ($directory in $manifestDirectories) {
    Assert-True ($directory -match '^\d{2}_[A-Za-z0-9_]+$') "unsafe project directory: $directory"
}
Assert-True (@($manifestDirectories | Sort-Object -Unique).Count -eq 37) 'manifest project directories must be unique'
$sortedManifestDirectories = @($manifestDirectories | Sort-Object -CaseSensitive)
$sortedSolutionProjectNames = @($solutionProjectNames | Sort-Object -CaseSensitive)
Assert-True (($sortedManifestDirectories -join "`n") -ceq ($sortedSolutionProjectNames -join "`n")) 'manifest project directories must exactly match TutorialApp.sln application projects'

$logo = Join-Path $repoRoot 'docs\media\branding\alice-tutorial-logo.png'
Assert-True (Test-Path -LiteralPath $logo -PathType Leaf) 'shared README logo missing'
$masterPath = Join-Path $repoRoot 'Dx11\Resource\Icon\AliceTutorialIcon.png'
$icoPath = Join-Path $repoRoot 'Dx11\Resource\Icon\AliceTutorial.ico'
Assert-True (Test-Path -LiteralPath $masterPath -PathType Leaf) 'checked-in icon master missing'
Assert-True (Test-Path -LiteralPath $icoPath -PathType Leaf) 'checked-in ICO missing'

$masterHeader = Read-PngHeader $masterPath
Assert-True ($masterHeader.Width -eq 1024 -and $masterHeader.Height -eq 1024 -and $masterHeader.BitDepth -eq 8 -and $masterHeader.ColorType -eq 6) 'master PNG must be 1024x1024 RGBA'
$bannerHeader = Read-PngHeader $logo
Assert-True ($bannerHeader.Width -eq 1536 -and $bannerHeader.Height -eq 640 -and $bannerHeader.BitDepth -eq 8 -and $bannerHeader.ColorType -eq 2) 'README banner must be 1536x640 RGB'

Add-Type -AssemblyName System.Drawing
$masterBitmap = [Drawing.Bitmap]::new($masterPath)
try {
    Assert-True ($masterBitmap.GetPixel(0, 0).A -eq 0 -and $masterBitmap.GetPixel(1023, 0).A -eq 0) 'master PNG top corners must be transparent'
    $foregroundOpaque = $true
    for ($y = 971; $foregroundOpaque -and $y -le 1023; $y++) {
        for ($x = 465; $x -le 551; $x++) {
            if ($masterBitmap.GetPixel($x, $y).A -ne 255) {
                $foregroundOpaque = $false
                break
            }
        }
    }
    Assert-True $foregroundOpaque 'master PNG foreground clothing must be fully opaque'
}
finally {
    $masterBitmap.Dispose()
}
Assert-True (Test-RequiredIcoDirectory $icoPath) 'ICO must contain exactly the nine required square entries'

$rootReadme = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'README.md')
Assert-True ($rootReadme -notmatch 'README-BRAND:(?:START|END)') 'root README brand markers must be absent'
Assert-True ($rootReadme -notmatch 'alice-tutorial-logo\.png') 'root README logo reference must be absent'

$readmes = @($manifest.projects | ForEach-Object {
    [pscustomobject]@{
        Path = Join-Path $repoRoot "Dx11/$($_.directory)/README.md"
        Relative = '../../docs/media/branding/alice-tutorial-logo.png'
        Width = 520
    }
})

Assert-True ($readmes.Count -eq 37) 'expected 37 project READMEs'
foreach ($entry in $readmes) {
    $content = Get-Content -Raw -LiteralPath $entry.Path
    $brandStart = '<!-- README-BRAND:START -->'
    $brandEnd = '<!-- README-BRAND:END -->'

    Assert-True (([regex]::Matches($content, [regex]::Escape($brandStart))).Count -eq 1) "brand start count invalid: $($entry.Path)"
    Assert-True (([regex]::Matches($content, [regex]::Escape($brandEnd))).Count -eq 1) "brand end count invalid: $($entry.Path)"

    $brandIndex = $content.IndexOf($brandStart, [StringComparison]::Ordinal)
    $brandEndIndex = $content.IndexOf($brandEnd, [StringComparison]::Ordinal)
    Assert-True ($brandIndex -lt $brandEndIndex) "brand markers out of order: $($entry.Path)"
    $brandBlockLength = $brandEndIndex + $brandEnd.Length - $brandIndex
    $brandBlock = $content.Substring($brandIndex, $brandBlockLength)
    $expectedImage = "<p align=`"center`"><img src=`"$($entry.Relative)`" width=`"$($entry.Width)`" alt=`"D3D11 Alice Tutorial mascot logo`" /></p>"
    $expectedBlockPattern = '\A' + [regex]::Escape($brandStart) + '\r?\n' + [regex]::Escape($expectedImage) + '\r?\n' + [regex]::Escape($brandEnd) + '\z'
    Assert-True ([regex]::IsMatch($brandBlock, $expectedBlockPattern)) "brand block content invalid: $($entry.Path)"
    $afterBrandBlock = $content.Substring($brandEndIndex + $brandEnd.Length)
    Assert-True ([regex]::IsMatch($afterBrandBlock, '\A(?<newline>\r?\n)\k<newline>(?!\r?\n)')) "brand block must have exactly one trailing blank line: $($entry.Path)"

    $heading = [regex]::Match($content, '(?m)^#{1,6}\s+.+$')
    Assert-True $heading.Success "Markdown heading missing: $($entry.Path)"
    Assert-True ($brandIndex -gt ($heading.Index + $heading.Length)) "brand block must follow first Markdown heading: $($entry.Path)"
    $betweenHeadingAndBrand = $content.Substring($heading.Index + $heading.Length, $brandIndex - ($heading.Index + $heading.Length))
    Assert-True ($betweenHeadingAndBrand -match '^\s*$') "brand block must be immediately after first Markdown heading: $($entry.Path)"
}

$sourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Dx11') -Recurse -File -Include *.cpp, *.h, *.rc, *.targets)
$allText = ($sourceFiles | Get-Content -Raw) -join "`n"
Assert-True ($allText -notmatch 'Resource\\Icon\\Alice\.ico') 'old icon reference remains'
Assert-True (-not (Test-Path -LiteralPath (Join-Path $repoRoot 'Dx11\Resource\Icon\Alice.ico'))) 'old icon file remains'
Assert-True ($allText -notmatch 'ChatGPT_Icon|ChatGPT_TwoTone_LOGO') 'source filenames leaked into public files'

'app branding acceptance tests passed'
