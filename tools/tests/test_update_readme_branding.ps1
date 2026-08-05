$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$updater = Join-Path $repoRoot 'tools\update_readme_branding.ps1'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('readme-brand-' + [guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Force -Path (Join-Path $fixture 'tools'), (Join-Path $fixture 'Dx11\01_Test'), (Join-Path $fixture 'Dx11\02_Test'), (Join-Path $fixture 'Dx11\03_Unselected') | Out-Null
    @{ expectedProjectCount = 2; projects = @(@{ directory = '01_Test' }, @{ directory = '02_Test' }) } |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $fixture 'tools\manifest.json') -Encoding utf8NoBOM
    [IO.File]::WriteAllText((Join-Path $fixture 'README.md'), "# Root`r`n`r`nRoot body`r`n", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\01_Test\README.md'), "<!-- README-NAV-TOP:START -->`nnav`n<!-- README-NAV-TOP:END -->`n`n## One`n`nBody one`n", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\02_Test\README.md'), "### Two`n`n본문 two`n", [Text.Encoding]::GetEncoding(949))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\03_Unselected\README.md'), "# Three`n", [Text.UTF8Encoding]::new($false))
    $unselectedBefore = [IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\03_Unselected\README.md'))

    & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json'
    $rootFirst = Get-Content -Raw -LiteralPath (Join-Path $fixture 'README.md')
    $oneFirst = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\01_Test\README.md')
    $twoFirst = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\02_Test\README.md')
    & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json'
    $oneSecond = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\01_Test\README.md')

    Assert-True ($rootFirst -match 'width="720"') 'root width must be 720'
    Assert-True ($oneFirst -match 'width="520"') 'project width must be 520'
    Assert-True (($oneFirst | Select-String 'README-BRAND:START' -AllMatches).Matches.Count -eq 1) 'brand block duplicated'
    Assert-True ($oneFirst -ceq $oneSecond) 'second run must be idempotent'
    Assert-True ($oneFirst.IndexOf('README-BRAND:START') -gt $oneFirst.IndexOf('## One')) 'brand block must follow first Markdown heading'
    Assert-True ($twoFirst.IndexOf('README-BRAND:START') -gt $twoFirst.IndexOf('### Two')) 'brand block must follow H3 heading'
    Assert-True ([BitConverter]::ToString(([IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\02_Test\README.md'))[0..2])) -ne 'EF-BB-BF') 'updated README must not have a UTF-8 BOM'
    Assert-True ($oneFirst -match 'README-NAV-TOP:START') 'existing navigation marker changed'
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($unselectedBefore, [IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\03_Unselected\README.md')))) 'unselected README changed'

    $beforeMissingH1 = [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md'))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\02_Test\README.md'), "No heading`n", [Text.UTF8Encoding]::new($false))
    $failed = $false
    try { & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json' } catch { $failed = $_.Exception.Message -match 'first Markdown heading' }
    Assert-True $failed 'missing Markdown heading must fail'
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($beforeMissingH1, [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md')))) 'validation failure partially updated root README'

    'README branding updater tests passed'
}
finally {
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}
