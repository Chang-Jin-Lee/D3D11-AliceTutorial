$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$updater = Join-Path $repoRoot 'tools\update_readme_branding.ps1'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('readme-brand-' + [guid]::NewGuid().ToString('N'))
try {
    $projectDirectories = @(1..37 | ForEach-Object { '{0:D2}_Test' -f $_ })
    New-Item -ItemType Directory -Force -Path (Join-Path $fixture 'tools'), (Join-Path $fixture 'Dx11'), (Join-Path $fixture 'Dx11\99_Unselected') | Out-Null
    foreach ($directory in $projectDirectories) {
        New-Item -ItemType Directory -Force -Path (Join-Path $fixture "Dx11\$directory") | Out-Null
    }
    $solutionLines = @('Project("{TYPE}") = "Common", "Common\Common.vcxproj", "{COMMON}"')
    $solutionLines += @($projectDirectories | ForEach-Object { "Project(`"{TYPE}`") = `"$_`", `"$_\$_.vcxproj`", `"{PROJECT}`"" })
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\TutorialApp.sln'), ($solutionLines -join "`r`n") + "`r`n", [Text.UTF8Encoding]::new($false))
    @{ expectedProjectCount = 37; projects = @($projectDirectories | ForEach-Object { @{ directory = $_ } }) } |
        ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $fixture 'tools\manifest.json') -Encoding utf8NoBOM
    [IO.File]::WriteAllText((Join-Path $fixture 'README.md'), "# Root`r`n`r`n<!-- README-BRAND:START -->`r`nold root block`r`n<!-- README-BRAND:END -->`r`n> Root quote`r`n", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\01_Test\README.md'), "<!-- README-NAV-TOP:START -->`nnav`n<!-- README-NAV-TOP:END -->`n`n## One`n`n<!-- README-BRAND:START -->`nold project block`n<!-- README-BRAND:END -->`n- Project list`n", [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\02_Test\README.md'), "### Two`n`n본문 two`n", [Text.Encoding]::GetEncoding(949))
    foreach ($directory in $projectDirectories | Select-Object -Skip 2) {
        [IO.File]::WriteAllText((Join-Path $fixture "Dx11\$directory\README.md"), "# $directory`n`nBody`n", [Text.UTF8Encoding]::new($false))
    }
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\99_Unselected\README.md'), "# Unselected`n", [Text.UTF8Encoding]::new($false))
    $unselectedBefore = [IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\99_Unselected\README.md'))

    & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json'
    $rootFirst = Get-Content -Raw -LiteralPath (Join-Path $fixture 'README.md')
    $oneFirst = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\01_Test\README.md')
    $twoFirst = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\02_Test\README.md')
    & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json'
    $rootSecond = Get-Content -Raw -LiteralPath (Join-Path $fixture 'README.md')
    $oneSecond = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\01_Test\README.md')

    $newBehaviorFailures = [Collections.Generic.List[string]]::new()

    Assert-True ($rootFirst -match 'width="720"') 'root width must be 720'
    Assert-True ($rootFirst.Contains("`r`n")) 'CRLF root README newline style changed'
    Assert-True ($rootFirst -match "# Root`r`n`r`n<!-- README-BRAND:START -->") 'CRLF root brand block placement or surrounding content changed'
    if ($rootFirst -notmatch '<!-- README-BRAND:END -->\r\n\r\n> Root quote') { $newBehaviorFailures.Add('root blockquote must have one blank line after README-BRAND:END') }
    if ($rootFirst -match '<!-- README-BRAND:END -->\r\n\r\n\r\n') { $newBehaviorFailures.Add('root block must not gain extra blank lines') }
    if ($rootFirst -cne $rootSecond) { $newBehaviorFailures.Add('second run must keep the root README byte-idempotent') }
    Assert-True ($oneFirst -match 'width="520"') 'project width must be 520'
    if ($oneFirst -notmatch '<!-- README-BRAND:END -->\n\n- Project list') { $newBehaviorFailures.Add('project list must have one blank line after README-BRAND:END') }
    if ($oneFirst -match '<!-- README-BRAND:END -->\n\n\n') { $newBehaviorFailures.Add('project block must not gain extra blank lines') }
    Assert-True (($oneFirst | Select-String 'README-BRAND:START' -AllMatches).Matches.Count -eq 1) 'brand block duplicated'
    Assert-True ($oneFirst -ceq $oneSecond) 'second run must be idempotent'
    Assert-True ($oneFirst.IndexOf('README-BRAND:START') -gt $oneFirst.IndexOf('## One')) 'brand block must follow first Markdown heading'
    Assert-True ($twoFirst.IndexOf('README-BRAND:START') -gt $twoFirst.IndexOf('### Two')) 'brand block must follow H3 heading'
    Assert-True ([BitConverter]::ToString(([IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\02_Test\README.md'))[0..2])) -ne 'EF-BB-BF') 'updated README must not have a UTF-8 BOM'
    Assert-True ($oneFirst -match 'README-NAV-TOP:START') 'existing navigation marker changed'
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($unselectedBefore, [IO.File]::ReadAllBytes((Join-Path $fixture 'Dx11\99_Unselected\README.md')))) 'unselected README changed'

    $rootBeforeMalformed = [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md'))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\01_Test\README.md'), "## One`n<!-- README-BRAND:END -->`n<!-- README-BRAND:START -->`n", [Text.UTF8Encoding]::new($false))
    $failed = $false
    try { & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json' } catch { $failed = $_.Exception.Message -match 'malformed README-BRAND markers' }
    Assert-True $failed 'END-before-START must fail'
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($rootBeforeMalformed, [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md')))) 'malformed markers partially updated root README'

    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\01_Test\README.md'), "## One`n`nBody one`n`n<!-- README-BRAND:START -->`nold block`n<!-- README-BRAND:END -->`n", [Text.UTF8Encoding]::new($false))
    & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json'
    $movedBlock = Get-Content -Raw -LiteralPath (Join-Path $fixture 'Dx11\01_Test\README.md')
    Assert-True ($movedBlock -match "## One`n`n<!-- README-BRAND:START -->") 'existing brand block must move immediately after first Markdown heading'
    Assert-True (($movedBlock | Select-String 'README-BRAND:START' -AllMatches).Matches.Count -eq 1) 'moved brand block duplicated'

    $beforeMissingH1 = [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md'))
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\02_Test\README.md'), "No heading`n", [Text.UTF8Encoding]::new($false))
    $failed = $false
    try { & $updater -RepoRoot $fixture -Manifest 'tools/manifest.json' } catch { $failed = $_.Exception.Message -match 'first Markdown heading' }
    Assert-True $failed 'missing Markdown heading must fail'
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($beforeMissingH1, [IO.File]::ReadAllBytes((Join-Path $fixture 'README.md')))) 'validation failure partially updated root README'

    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\02_Test\README.md'), "### Two`n`nBody two`n", [Text.UTF8Encoding]::new($false))
    $manifestFailures = [Collections.Generic.List[string]]::new()
    $negativeCases = @(
        [pscustomobject]@{
            Name = 'duplicate'
            Message = 'manifest project directories must be unique'
            Directories = @($projectDirectories[0..35]) + @($projectDirectories[0])
        },
        [pscustomobject]@{
            Name = 'solution-external substitution'
            Message = 'manifest project directories must exactly match TutorialApp.sln application projects'
            Directories = @($projectDirectories[0..35]) + @('99_External')
        }
    )
    New-Item -ItemType Directory -Force -Path (Join-Path $fixture 'Dx11\99_External') | Out-Null
    [IO.File]::WriteAllText((Join-Path $fixture 'Dx11\99_External\README.md'), "# External`n", [Text.UTF8Encoding]::new($false))
    foreach ($case in $negativeCases) {
        @{ expectedProjectCount = 37; projects = @($case.Directories | ForEach-Object { @{ directory = $_ } }) } |
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
    foreach ($failure in $manifestFailures) { $newBehaviorFailures.Add($failure) }
    Assert-True ($newBehaviorFailures.Count -eq 0) ($newBehaviorFailures -join '; ')

    'README branding updater tests passed'
}
finally {
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}
