$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifestPath = Join-Path $repoRoot 'tools\readme_media_manifest.json'
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json

Assert-True ([int]$manifest.expectedProjectCount -eq 37) 'expectedProjectCount must be 37'
Assert-True (@($manifest.projects).Count -eq 37) 'manifest must contain 37 projects'

$logo = Join-Path $repoRoot 'docs\media\branding\alice-tutorial-logo.png'
Assert-True (Test-Path -LiteralPath $logo -PathType Leaf) 'shared README logo missing'

$readmes = @(
    [pscustomobject]@{
        Path = Join-Path $repoRoot 'README.md'
        Relative = 'docs/media/branding/alice-tutorial-logo.png'
        Width = 720
    }
)
$readmes += @($manifest.projects | ForEach-Object {
    [pscustomobject]@{
        Path = Join-Path $repoRoot "Dx11/$($_.directory)/README.md"
        Relative = '../../docs/media/branding/alice-tutorial-logo.png'
        Width = 520
    }
})

Assert-True ($readmes.Count -eq 38) 'expected root plus 37 project READMEs'
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
