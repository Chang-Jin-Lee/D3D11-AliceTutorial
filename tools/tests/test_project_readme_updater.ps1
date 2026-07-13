$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$updater = Join-Path $repoRoot 'tools/update_project_readmes.ps1'
$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("project-readme-updater-" + [guid]::NewGuid().ToString('N'))

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw $Message
    }
}

function Assert-BytesEqual([byte[]]$Expected, [byte[]]$Actual, [string]$Message) {
    Assert-True ([System.Collections.StructuralComparisons]::StructuralEqualityComparer.Equals($Expected, $Actual)) $Message
}

function Invoke-ExpectFailure([scriptblock]$Action, [string]$Message, [string]$ExpectedError) {
    $failed = $false
    $errorMessage = ''
    try {
        & $Action
    }
    catch {
        $failed = $true
        $errorMessage = $_.Exception.Message
    }

    Assert-True $failed $Message
    if (-not [string]::IsNullOrWhiteSpace($ExpectedError)) {
        Assert-True ($errorMessage -like "*$ExpectedError*") "$Message (expected error containing '$ExpectedError', got '$errorMessage')"
    }
}

function Write-FixtureManifest([string]$Path, [object[]]$Projects) {
    $manifest = @{
        expectedProjectCount = $Projects.Count
        projects = $Projects
    } | ConvertTo-Json -Depth 4
    [System.IO.File]::WriteAllText($Path, $manifest + "`n", [System.Text.UTF8Encoding]::new($false))
}

try {
    [System.Text.Encoding]::RegisterProvider([System.Text.CodePagesEncodingProvider]::Instance)
    $cp949 = [System.Text.Encoding]::GetEncoding(949, [System.Text.EncoderExceptionFallback]::new(), [System.Text.DecoderExceptionFallback]::new())
    $null = New-Item -ItemType Directory -Force -Path (Join-Path $fixtureRoot 'Dx11/01_Test'), (Join-Path $fixtureRoot 'Dx11/02_Test')

    $projects = @(
        @{
            number = '01'
            directory = '01_Test'
            image = '01-Test.png'
            gif = '01-Test.gif'
            infoImage = 'info/01-Test-info.png'
        },
        @{
            number = '02'
            directory = '02_Test'
            image = '02-Test.png'
            gif = '02-Test.gif'
            infoImage = 'info/02-Test-info.png'
        }
    )
    $fixtureManifest = Join-Path $fixtureRoot 'tools/readme_media_manifest.json'
    $null = New-Item -ItemType Directory -Force -Path (Split-Path -Parent $fixtureManifest)
    Write-FixtureManifest $fixtureManifest $projects

    $readme01 = Join-Path $fixtureRoot 'Dx11/01_Test/README.md'
    $readme02 = Join-Path $fixtureRoot 'Dx11/02_Test/README.md'
    $rootReadme = Join-Path $fixtureRoot 'README.md'
    $unselectedReadme = Join-Path $fixtureRoot 'Dx11/03_Unselected/README.md'
    $null = New-Item -ItemType Directory -Force -Path (Split-Path -Parent $unselectedReadme)
    $body01 = "`n# 01 Test`n`n기존 기술 설명 01`n`n| left | right |`r`n|---|---|`n"
    [System.IO.File]::WriteAllText($readme01, $body01, [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText($readme02, "# 02 Test`n`n기존 기술 설명 02`n", $cp949)
    [System.IO.File]::WriteAllText($rootReadme, "# Root must not change`n", [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText($unselectedReadme, "# Unselected must not change`n", [System.Text.UTF8Encoding]::new($false))
    $rootBytes = [System.IO.File]::ReadAllBytes($rootReadme)
    $unselectedBytes = [System.IO.File]::ReadAllBytes($unselectedReadme)

    & $updater -RepoRoot $fixtureRoot -Manifest $fixtureManifest
    $first = Get-Content -Raw $readme01
    & $updater -RepoRoot $fixtureRoot -Manifest $fixtureManifest
    $second = Get-Content -Raw $readme01
    $last = [System.IO.File]::ReadAllText($readme02, $cp949)

    Assert-True ($first -ceq $second) 'README update is not idempotent'
    Assert-True ($first -match '기존 기술 설명 01') 'existing body was not preserved'
    Assert-True ($first.Contains($body01.TrimEnd([char[]]"`r`n"))) 'existing body bytes were not preserved'
    Assert-True ($first -match '\.\./\.\./README\.md') 'main link missing'
    Assert-True ($first -match '\.\./02_Test/README\.md') 'next link missing'
    Assert-True ($first -match 'docs/media/readme/info/01-Test-info\.png') 'info image missing'
    Assert-True ($first -match 'docs/media/readme/01-Test\.gif') 'GIF missing'
    Assert-True ($first -match '(?m)^이전 \| \[메인\]') 'first Previous must be non-linked text'
    Assert-True ($last -match '(?m)\| 다음$') 'last Next must be non-linked text'
    Assert-True ($last -match '기존 기술 설명 02') 'legacy CP949 body was not preserved'
    Assert-True ($first.IndexOf('<!-- README-NAV-TOP:START -->') -lt $first.IndexOf('기존 기술 설명 01')) 'top navigation must precede existing body'
    Assert-True ($first.IndexOf('<!-- README-INFO:START -->') -lt $first.IndexOf('기존 기술 설명 01')) 'info block must precede existing body'
    Assert-True ($first.IndexOf('<!-- README-RUNTIME:START -->') -gt $first.IndexOf('기존 기술 설명 01')) 'runtime block must follow existing body'
    Assert-True ($first.IndexOf('<!-- README-NAV-BOTTOM:START -->') -gt $first.IndexOf('<!-- README-RUNTIME:START -->')) 'bottom navigation must follow runtime block'
    Assert-BytesEqual $rootBytes ([System.IO.File]::ReadAllBytes($rootReadme)) 'root README changed'
    Assert-BytesEqual $unselectedBytes ([System.IO.File]::ReadAllBytes($unselectedReadme)) 'unselected README changed'

    $topStart = '<!-- README-NAV-TOP:START -->'
    $topEnd = '<!-- README-NAV-TOP:END -->'
    $infoStart = '<!-- README-INFO:START -->'
    $infoEnd = '<!-- README-INFO:END -->'
    $runtimeStart = '<!-- README-RUNTIME:START -->'
    $runtimeEnd = '<!-- README-RUNTIME:END -->'
    $bottomStart = '<!-- README-NAV-BOTTOM:START -->'
    $bottomEnd = '<!-- README-NAV-BOTTOM:END -->'
    $markerCases = @(
        @{ Name = 'nested'; Content = "$topStart`n$infoStart`n$infoEnd`n$topEnd`n$runtimeStart`n$runtimeEnd`n$bottomStart`n$bottomEnd`n" },
        @{ Name = 'overlapping'; Content = "$topStart`n$infoStart`n$topEnd`n$infoEnd`n$runtimeStart`n$runtimeEnd`n$bottomStart`n$bottomEnd`n" },
        @{ Name = 'reversed-pair'; Content = "$topEnd`n$topStart`n$infoStart`n$infoEnd`n$runtimeStart`n$runtimeEnd`n$bottomStart`n$bottomEnd`n" },
        @{ Name = 'partial'; Content = "$topStart`n$infoStart`n$infoEnd`n$runtimeStart`n$runtimeEnd`n$bottomStart`n$bottomEnd`n" },
        @{ Name = 'duplicate'; Content = "$topStart`n$topStart`n$topEnd`n$infoStart`n$infoEnd`n$runtimeStart`n$runtimeEnd`n$bottomStart`n$bottomEnd`n" },
        @{ Name = 'wrong-order'; Content = "$infoStart`n$infoEnd`n$topStart`n$topEnd`n$runtimeStart`n$runtimeEnd`n$bottomStart`n$bottomEnd`n" }
    )

    foreach ($case in $markerCases) {
        [System.IO.File]::WriteAllText($readme01, $case.Content, [System.Text.UTF8Encoding]::new($false))
        $caseBefore = [System.IO.File]::ReadAllBytes($readme01)
        $selectedBefore = [System.IO.File]::ReadAllBytes($readme02)
        Invoke-ExpectFailure { & $updater -RepoRoot $fixtureRoot -Manifest $fixtureManifest } "Malformed $($case.Name) marker topology did not fail" 'README'
        Assert-BytesEqual $caseBefore ([System.IO.File]::ReadAllBytes($readme01)) "Malformed $($case.Name) marker topology changed its README"
        Assert-BytesEqual $selectedBefore ([System.IO.File]::ReadAllBytes($readme02)) "Malformed $($case.Name) marker topology created a partial update"
        Assert-BytesEqual $rootBytes ([System.IO.File]::ReadAllBytes($rootReadme)) "Malformed $($case.Name) marker topology changed root README"
        Assert-BytesEqual $unselectedBytes ([System.IO.File]::ReadAllBytes($unselectedReadme)) "Malformed $($case.Name) marker topology changed unselected README"
    }

    [System.IO.File]::WriteAllText($readme01, $first, [System.Text.UTF8Encoding]::new($false))
    foreach ($invalidPath in @('../01_Test', '.', '01 Test', '01_Test"', ("01_Test" + "`n"), ("01_Test" + "`r`n"))) {
        $invalidProjects = @($projects | ForEach-Object { $_.Clone() })
        $invalidProjects[0].directory = $invalidPath
        Write-FixtureManifest $fixtureManifest $invalidProjects
        $selectedBefore = [System.IO.File]::ReadAllBytes($readme02)
        Invoke-ExpectFailure { & $updater -RepoRoot $fixtureRoot -Manifest $fixtureManifest } "Unsafe directory '$invalidPath' did not fail" 'safe path segment'
        Assert-BytesEqual $selectedBefore ([System.IO.File]::ReadAllBytes($readme02)) "Unsafe directory '$invalidPath' changed a selected README"
        Assert-BytesEqual $rootBytes ([System.IO.File]::ReadAllBytes($rootReadme)) "Unsafe directory '$invalidPath' changed root README"
        Assert-BytesEqual $unselectedBytes ([System.IO.File]::ReadAllBytes($unselectedReadme)) "Unsafe directory '$invalidPath' changed unselected README"
    }

    foreach ($property in @('image', 'gif', 'infoImage')) {
        foreach ($invalidPath in @('../outside.png', '/root.png', 'folder\\file.png', 'folder/./image.png', 'folder/../image.png', 'folder/.../image.png', 'has space.png', 'has#fragment.png', 'has"quote.png', ("trailing-lf.png" + "`n"), ("trailing-crlf.png" + "`r`n"))) {
            $invalidProjects = @($projects | ForEach-Object { $_.Clone() })
            $invalidProjects[0].$property = $invalidPath
            Write-FixtureManifest $fixtureManifest $invalidProjects
            $selectedBefore = [System.IO.File]::ReadAllBytes($readme02)
            Invoke-ExpectFailure { & $updater -RepoRoot $fixtureRoot -Manifest $fixtureManifest } "Unsafe $property path '$invalidPath' did not fail" 'safe relative path'
            Assert-BytesEqual $selectedBefore ([System.IO.File]::ReadAllBytes($readme02)) "Unsafe $property path '$invalidPath' changed a selected README"
            Assert-BytesEqual $rootBytes ([System.IO.File]::ReadAllBytes($rootReadme)) "Unsafe $property path '$invalidPath' changed root README"
            Assert-BytesEqual $unselectedBytes ([System.IO.File]::ReadAllBytes($unselectedReadme)) "Unsafe $property path '$invalidPath' changed unselected README"
        }
    }

    'project README updater tests passed'
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}
