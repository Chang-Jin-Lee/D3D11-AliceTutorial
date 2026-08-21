$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$updater = Join-Path $repoRoot 'tools\update_readme_branding.ps1'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('readme-brand-' + [guid]::NewGuid().ToString('N'))
try {
    $projectDirectories = @(1..38 | ForEach-Object { '{0:D2}_Test' -f $_ })
    New-Item -ItemType Directory -Force -Path (Join-Path $fixture 'tools'), (Join-Path $fixture 'Dx11'), (Join-Path $fixture 'Dx11\99_Unselected') | Out-Null
    foreach ($directory in $projectDirectories) {
        New-Item -ItemType Directory -Force -Path (Join-Path $fixture "Dx11\$directory") | Out-Null
    }
    $solutionLines = @('Project("{TYPE}") = "Common", "Common\Common.vcxproj", "{COMMON}"')
    $solutionLines += @($projectDirectories | ForEach-Object { "Project(`"{TYPE}`") = `"$_`", `"$_\$_.vcxproj`", `"{PROJECT}`"" })
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\TutorialApp.sln'), ($solutionLines -join "`r`n") + "`r`n", [Text.UTF8Encoding]::new($false))
    @{ expectedProjectCount = 38; projects = @($projectDirectories | ForEach-Object { @{ directory = $_ } }) } |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $fixture 'tools\manifest.json') -Encoding utf8NoBOM

    [IO.File]::WriteAllText((Join-Path $fixture 'README.md'), "# Root`r`n`r`n> Root quote`r`n", [Text.UTF8Encoding]::new($false))
    $rootBefore = [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md'))

    # 01_Test still carries a legacy README-BRAND block, as a project README
    # would have held the mascot logo before it was removed. It also carries
    # an unrelated navigation marker that must survive untouched.
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\01_Test\README.md'), "<!-- README-NAV-TOP:START -->`nnav`n<!-- README-NAV-TOP:END -->`n`n## One`n`n<!-- README-BRAND:START -->`nold project block`n<!-- README-BRAND:END -->`n- Project list`n", [Text.UTF8Encoding]::new($false))

    # 02_Test is CP949-encoded (as some legacy READMEs are) and also carries
    # a legacy brand block, exercising the encoding-fallback read/rewrite path.
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\02_Test\README.md'), "### Two`n`n본문 two`n`n<!-- README-BRAND:START -->`nold logo`n<!-- README-BRAND:END -->`n`n마지막 문단`n", [Text.Encoding]::GetEncoding(949))

    # Every other project README already reflects the new contract: no
    # markers, no logo. The updater must leave these completely untouched.
    foreach ($directory in $projectDirectories | Select-Object -Skip 2) {
        [IO.File]::WriteAllText((Join-Path $fixture "Dx11\$directory\README.md"), "# $directory`n`nBody`n", [Text.UTF8Encoding]::new($false))
    }
    $threeBefore = [IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\03_Test\README.md'))

    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\99_Unselected\README.md'), "# Unselected`n", [Text.UTF8Encoding]::new($false))
    $unselectedBefore = [IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\99_Unselected\README.md'))

    & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json'
    $rootFirst = Get-Content -Raw -LiteralPath (Join-Path $fixture 'README.md')
    $oneFirst = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\01_Test\README.md')
    $twoFirstBytes = [IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\02_Test\README.md'))
    $twoFirst = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\02_Test\README.md')
    & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json'
    $rootSecond = Get-Content -Raw -LiteralPath (Join-Path $fixture 'README.md')
    $oneSecond = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\01_Test\README.md')
    $twoSecond = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\02_Test\README.md')

    # Root README (never had a brand block) must be completely unaffected.
    Assert-True ($rootFirst.Contains("`r`n")) 'CRLF root README newline style changed'
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($rootBefore, [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md')))) 'root README must remain byte-identical'
    Assert-True ($rootFirst -notmatch 'README-BRAND:(?:START|END)') 'root README brand block must remain absent'
    Assert-True ($rootFirst -ceq $rootSecond) 'second run must keep the root README byte-idempotent'

    # The legacy block -- including its content, not just its markers -- must
    # be gone, and the rest of the file must be preserved exactly.
    Assert-True ($oneFirst -notmatch 'README-BRAND:(?:START|END)') 'legacy brand markers must be removed from 01_Test'
    Assert-True ($oneFirst -notmatch 'old project block') 'legacy brand block content must be removed from 01_Test'
    Assert-True ($oneFirst -match '## One\n\n- Project list') 'exactly one blank line must remain where the brand block was'
    Assert-True ($oneFirst -notmatch '## One\n\n\n') 'removing the brand block must not leave an extra blank line'
    Assert-True ($oneFirst -match 'README-NAV-TOP:START') 'existing navigation marker changed'
    Assert-True ($oneFirst -ceq $oneSecond) 'second run must be idempotent'

    # Same removal guarantee through the CP949 decode/UTF-8 re-encode path,
    # with the surrounding Korean text preserved on both sides of the block.
    Assert-True ($twoFirst -notmatch 'README-BRAND:(?:START|END)') 'legacy brand markers must be removed from 02_Test'
    Assert-True ($twoFirst -notmatch 'old logo') 'legacy brand block content must be removed from 02_Test'
    Assert-True ($twoFirst -match '본문 two') 'Korean body text before the block must survive re-encoding'
    Assert-True ($twoFirst -match '마지막 문단') 'Korean body text after the block must survive re-encoding'
    Assert-True ([BitConverter]::ToString($twoFirstBytes[0..2]) -ne 'EF-BB-BF') 'updated README must not have a UTF-8 BOM'
    Assert-True ($twoFirst -ceq $twoSecond) 'second run must be idempotent for the re-encoded README'

    # Durability: a README that never had a brand block must not be
    # rewritten at all, proving the generator no longer touches clean files.
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($threeBefore, [IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\03_Test\README.md')))) 'README with no brand markers must be left byte-identical'
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($unselectedBefore, [IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\99_Unselected\README.md')))) 'unselected README changed'

    # Malformed markers (END before START) must fail loudly and must not
    # partially update any file, including unrelated READMEs.
    $rootBeforeMalformed = [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md'))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\01_Test\README.md'), "## One`n<!-- README-BRAND:END -->`n<!-- README-BRAND:START -->`n", [Text.UTF8Encoding]::new($false))
    $failed = $false
    try { & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json' } catch { $failed = $_.Exception.Message -match 'malformed README-BRAND markers' }
    Assert-True $failed 'END-before-START must fail'
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($rootBeforeMalformed, [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md')))) 'malformed markers partially updated root README'

    # Restore 01_Test and 02_Test to clean, marker-free content before the
    # manifest-validation negative cases below.
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\01_Test\README.md'), "# 01_Test`n`nBody`n", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\02_Test\README.md'), "### Two`n`nBody two`n", [Text.UTF8Encoding]::new($false))

    $manifestFailures = [Collections.Generic.List[string]]::new()
    $negativeCases = @(
        [pscustomobject]@{
            Name = 'duplicate'
            Message = 'manifest project directories must be unique'
            Directories = @($projectDirectories[0..36]) + @($projectDirectories[0])
        },
        [pscustomobject]@{
            Name = 'solution-external substitution'
            Message = 'manifest project directories must exactly match TutorialApp.sln application projects'
            Directories = @($projectDirectories[0..36]) + @('99_External')
        }
    )
    New-Item -ItemType Directory -Force -Path (Join-Path $fixture 'Dx11\99_External') | Out-Null
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\99_External\README.md'), "# External`n", [Text.UTF8Encoding]::new($false))
    foreach ($case in $negativeCases) {
        @{ expectedProjectCount = 38; projects = @($case.Directories | ForEach-Object { @{ directory = $_ } }) } |
            ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $fixture 'tools\manifest.json') -Encoding utf8NoBOM
        [IO.File]::WriteAllText((Join-Path $fixture 'README.md'), "# Root`n`nRoot body`n", [Text.UTF8Encoding]::new($false))
        $beforeInvalidManifest = [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md'))
        $rejected = $false
        try { & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json' } catch { $rejected = $_.Exception.Message -match [regex]::Escape($case.Message) }
        if (-not $rejected) { $manifestFailures.Add("$($case.Name) manifest was not rejected at the expected validator") }
        if (-not [System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($beforeInvalidManifest, [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md')))) {
            $manifestFailures.Add("$($case.Name) manifest caused a write before rejection")
        }
    }
    Assert-True ($manifestFailures.Count -eq 0) ($manifestFailures -join '; ')

    'README branding updater tests passed'
}
finally {
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}
