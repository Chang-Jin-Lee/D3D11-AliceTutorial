$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$updater = Join-Path $repoRoot 'tools/update_project_readmes.ps1'
$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("project-readme-updater-" + [guid]::NewGuid().ToString('N'))

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        throw $Message
    }
}

try {
    [System.Text.Encoding]::RegisterProvider([System.Text.CodePagesEncodingProvider]::Instance)
    $cp949 = [System.Text.Encoding]::GetEncoding(949, [System.Text.EncoderExceptionFallback]::new(), [System.Text.DecoderExceptionFallback]::new())
    $null = New-Item -ItemType Directory -Force -Path (Join-Path $fixtureRoot 'Dx11/01_Test'), (Join-Path $fixtureRoot 'Dx11/02_Test')

    $manifest = @{
        expectedProjectCount = 2
        projects = @(
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
    } | ConvertTo-Json -Depth 4
    $fixtureManifest = Join-Path $fixtureRoot 'tools/readme_media_manifest.json'
    $null = New-Item -ItemType Directory -Force -Path (Split-Path -Parent $fixtureManifest)
    [System.IO.File]::WriteAllText($fixtureManifest, $manifest + "`n", [System.Text.UTF8Encoding]::new($false))

    $readme01 = Join-Path $fixtureRoot 'Dx11/01_Test/README.md'
    $readme02 = Join-Path $fixtureRoot 'Dx11/02_Test/README.md'
    $body01 = "`n# 01 Test`n`n기존 기술 설명 01`n`n| left | right |`r`n|---|---|`n"
    [System.IO.File]::WriteAllText($readme01, $body01, [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText($readme02, "# 02 Test`n`n기존 기술 설명 02`n", $cp949)

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

    'project README updater tests passed'
}
finally {
    if (Test-Path -LiteralPath $fixtureRoot) {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}
