$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '..\readme_media_common.ps1')

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Assert-Path([string]$Path, [string]$Message) {
    Assert-True (Test-Path -LiteralPath $Path) $Message
}

$archiveScript = Join-Path $PSScriptRoot '..\archive_readme_media.ps1'
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('archive-readme-media-' + [guid]::NewGuid().ToString('N'))
$fixtureRoot = Join-Path $tempRoot 'fixture'
$destinationRoot = Join-Path $tempRoot 'destination'
$nonGitDestinationRoot = Join-Path $tempRoot 'non-git-destination'

try {
    New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $nonGitDestinationRoot -Force | Out-Null

    $sourceFiles = [ordered]@{
        'README.md' = '# Fixture README'
        'README_old.md' = '# Older Fixture README'
        'Dx11/01_Fixture/README.md' = '# Project Fixture README'
        'docs/media/readme/nested/02.gif' = 'fixture gif bytes'
    }

    foreach ($relativePath in $sourceFiles.Keys) {
        $path = Join-Path $fixtureRoot ($relativePath -replace '/', '\')
        New-Item -ItemType Directory -Path (Split-Path -Parent $path) -Force | Out-Null
        Set-Content -LiteralPath $path -Value $sourceFiles[$relativePath] -NoNewline
    }

    $excludedProjectReadme = Join-Path $fixtureRoot 'Dx11\16_pmxWithMotion\README.md'
    New-Item -ItemType Directory -Path (Split-Path -Parent $excludedProjectReadme) -Force | Out-Null
    Set-Content -LiteralPath $excludedProjectReadme -Value '# Excluded project README' -NoNewline

    $sourcePng = Join-Path $fixtureRoot 'docs\media\readme\01.png'
    New-Item -ItemType Directory -Path (Split-Path -Parent $sourcePng) -Force | Out-Null
    [System.IO.File]::WriteAllBytes($sourcePng, [byte[]](0, 1, 2, 3, 255, 254, 253))

    $manifestPath = Join-Path $fixtureRoot 'fixture-manifest.json'
    $fixtureManifest = [ordered]@{
        expectedProjectCount = 1
        captureWidth = 1
        captureHeight = 1
        gifWidth = 1
        gifHeight = 1
        infoWidth = 1
        infoHeight = 1
        captureAttempts = 1
        gifSeconds = 1
        gifFps = 1
        gifMaxBytes = 1
        projects = @(
            [ordered]@{
                number = '01'
                name = 'Fixture'
                directory = 'Dx11/01_Fixture'
                exe = 'Fixture.exe'
                image = '01.png'
                gif = '01.gif'
                infoImage = 'info/01-info.png'
                gifPhase = 'runtime'
                title = 'Fixture'
                summary = 'Archive fixture'
                tags = @('fixture', 'archive', 'test')
            }
        )
    }
    $fixtureManifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $manifestPath -NoNewline

    $LASTEXITCODE = $null
    & $archiveScript -RepoRoot $fixtureRoot -Manifest $manifestPath -DestinationRoot $nonGitDestinationRoot
    $nonGitArchive = @(Get-ChildItem -LiteralPath $nonGitDestinationRoot -Directory) | Select-Object -First 1
    $nonGitMetadata = Get-Content -LiteralPath (Join-Path $nonGitArchive.FullName 'archive-manifest.json') -Raw | ConvertFrom-Json
    Assert-True ($null -eq $nonGitMetadata.sourceCommit) 'non-Git sourceCommit metadata should be null'

    & git init --quiet $fixtureRoot
    Assert-True ($LASTEXITCODE -eq 0) 'fixture Git repository initialization failed'
    & git -C $fixtureRoot -c user.name='Archive Test' -c user.email='archive-test@example.invalid' add -- .
    Assert-True ($LASTEXITCODE -eq 0) 'fixture Git add failed'
    & git -C $fixtureRoot -c user.name='Archive Test' -c user.email='archive-test@example.invalid' commit --quiet -m 'test fixture'
    Assert-True ($LASTEXITCODE -eq 0) 'fixture Git commit failed'
    $expectedSourceCommitOutput = & git -C $fixtureRoot rev-parse HEAD
    $expectedSourceCommitExitCode = $LASTEXITCODE
    Assert-True ($expectedSourceCommitExitCode -eq 0) 'fixture Git rev-parse failed'
    $expectedSourceCommit = ([string]($expectedSourceCommitOutput | Select-Object -First 1)).Trim()

    $beforeHashes = @{}
    $trackedSourcePaths = @($sourceFiles.Keys + 'docs/media/readme/01.png' + 'Dx11/16_pmxWithMotion/README.md')
    foreach ($relativePath in $trackedSourcePaths) {
        $sourcePath = Join-Path $fixtureRoot ($relativePath -replace '/', '\')
        $beforeHashes[$relativePath] = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
    }
    $before = $beforeHashes['docs/media/readme/01.png']

    $LASTEXITCODE = $null
    & $archiveScript -RepoRoot $fixtureRoot -Manifest $manifestPath -DestinationRoot $destinationRoot

    $archives = @(Get-ChildItem -LiteralPath $destinationRoot -Directory)
    Assert-True ($archives.Count -eq 1) 'exactly one archive directory should be created'
    $archive = $archives[0]
    Assert-True ($archive.Name -match '^README_Media_\d{8}_\d{6}$') 'archive directory name is invalid'

    $copy = Join-Path $archive.FullName 'docs\media\readme\01.png'
    Assert-Path $copy 'archived PNG is missing'
    if ((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -ne $before) { throw 'archived PNG hash mismatch' }
    if ((Get-FileHash -LiteralPath $sourcePng -Algorithm SHA256).Hash -ne $before) { throw 'source PNG was modified' }

    Assert-Path (Join-Path $archive.FullName 'docs\media\readme\nested\02.gif') 'nested media was not archived'
    Assert-Path (Join-Path $archive.FullName 'README.md') 'root README was not archived'
    Assert-Path (Join-Path $archive.FullName 'README_old.md') 'old root README was not archived'
    Assert-Path (Join-Path $archive.FullName 'Dx11\01_Fixture\README.md') 'project README path was not preserved'
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $archive.FullName 'Dx11\16_pmxWithMotion\README.md'))) 'unlisted project README was archived'

    $metadataPath = Join-Path $archive.FullName 'archive-manifest.json'
    Assert-Path $metadataPath 'archive metadata missing'
    $metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
    Assert-True ($metadata.PSObject.Properties.Name -contains 'createdAt') 'createdAt metadata missing'
    Assert-True ($metadata.PSObject.Properties.Name -contains 'sourceRoot') 'sourceRoot metadata missing'
    Assert-True ($metadata.PSObject.Properties.Name -contains 'sourceCommit') 'sourceCommit metadata missing'
    $createdAt = [DateTimeOffset]::MinValue
    Assert-True ([DateTimeOffset]::TryParse([string]$metadata.createdAt, [ref]$createdAt)) 'createdAt metadata is invalid'
    Assert-True ([System.IO.Path]::GetFullPath([string]$metadata.sourceRoot) -eq [System.IO.Path]::GetFullPath($fixtureRoot)) 'sourceRoot metadata mismatch'
    Assert-True ([string]$metadata.sourceCommit -eq $expectedSourceCommit) "sourceCommit metadata mismatch: expected $expectedSourceCommit, got $($metadata.sourceCommit)"

    $expectedPaths = @($sourceFiles.Keys + 'docs/media/readme/01.png' | Sort-Object)
    $actualPaths = @(Get-ChildItem -LiteralPath $archive.FullName -Recurse -File |
        Where-Object { $_.Name -ne 'archive-manifest.json' } |
        ForEach-Object { [System.IO.Path]::GetRelativePath($archive.FullName, $_.FullName).Replace('\', '/') } |
        Sort-Object)
    Assert-True (($actualPaths -join '|') -eq ($expectedPaths -join '|')) 'archived relative paths mismatch'

    $metadataFiles = @($metadata.files)
    Assert-True ($metadataFiles.Count -eq $expectedPaths.Count) 'archive file metadata count mismatch'
    $metadataPaths = @($metadataFiles | ForEach-Object { $_.path })
    Assert-True (($metadataPaths -join '|') -eq ($expectedPaths -join '|')) 'archive metadata paths are not sorted'
    foreach ($relativePath in $expectedPaths) {
        $archivePath = Join-Path $archive.FullName ($relativePath -replace '/', '\')
        $metadataFile = @($metadataFiles | Where-Object { $_.path -eq $relativePath }) | Select-Object -First 1
        $actualHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
        Assert-True ($metadataFile.sha256 -eq $actualHash) "archive metadata hash mismatch: $relativePath"
    }

    foreach ($relativePath in $beforeHashes.Keys) {
        $sourcePath = Join-Path $fixtureRoot ($relativePath -replace '/', '\')
        $after = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
        Assert-True ($after -eq $beforeHashes[$relativePath]) "source file was modified: $relativePath"
    }

    $inRepoDestination = Join-Path $fixtureRoot 'docs\media\readme\archive-target'
    $inRepoThrew = $false
    try {
        & $archiveScript -RepoRoot $fixtureRoot -Manifest $manifestPath -DestinationRoot $inRepoDestination
    }
    catch {
        $inRepoThrew = $true
    }
    Assert-True $inRepoThrew 'in-repository destination should be rejected'
    Assert-True (-not (Test-Path -LiteralPath $inRepoDestination)) 'rejected destination directory was created'
    foreach ($relativePath in $beforeHashes.Keys) {
        $sourcePath = Join-Path $fixtureRoot ($relativePath -replace '/', '\')
        $after = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
        Assert-True ($after -eq $beforeHashes[$relativePath]) "source file changed after rejected destination: $relativePath"
    }

    $rootDirectoriesBefore = @(Get-ChildItem -LiteralPath $fixtureRoot -Directory |
        ForEach-Object { [System.IO.Path]::GetFullPath($_.FullName) } |
        Sort-Object)
    $trailingRepoRoot = $fixtureRoot + [System.IO.Path]::DirectorySeparatorChar
    $sameRootDestination = $fixtureRoot
    $sameRootThrew = $false
    try {
        & $archiveScript -RepoRoot $trailingRepoRoot -Manifest $manifestPath -DestinationRoot $sameRootDestination
    }
    catch {
        $sameRootThrew = $true
    }
    Assert-True $sameRootThrew 'equivalent repository destination should be rejected'
    $rootDirectoriesAfter = @(Get-ChildItem -LiteralPath $fixtureRoot -Directory |
        ForEach-Object { [System.IO.Path]::GetFullPath($_.FullName) } |
        Sort-Object)
    Assert-True (($rootDirectoriesAfter -join '|') -eq ($rootDirectoriesBefore -join '|')) 'equivalent destination created a directory'
    foreach ($relativePath in $beforeHashes.Keys) {
        $sourcePath = Join-Path $fixtureRoot ($relativePath -replace '/', '\')
        $after = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
        Assert-True ($after -eq $beforeHashes[$relativePath]) "source file changed after equivalent destination: $relativePath"
    }

    'archive tests passed'
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
