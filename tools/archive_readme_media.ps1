param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Manifest = 'tools/readme_media_manifest.json',
    [string]$DestinationRoot = 'C:\Users\k2503200021\Desktop\애셋\D3D11-AliceTutorial'
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'readme_media_common.ps1')

function Assert-UnderRoot {
    param(
        [Parameter(Mandatory)] [string]$Target,
        [Parameter(Mandatory)] [string]$Root,
        [Parameter(Mandatory)] [string]$Label
    )

    $targetFull = [System.IO.Path]::GetFullPath($Target)
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    )

    if ($targetFull.Equals($rootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $targetFull
    }

    $rootPrefix = $rootFull + [System.IO.Path]::DirectorySeparatorChar
    if (-not $targetFull.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label path escaped root: $targetFull (root: $rootFull)"
    }

    return $targetFull
}

function Copy-ArchiveFile {
    param(
        [Parameter(Mandatory)] [string]$RelativePath,
        [Parameter(Mandatory)] [string]$RepoRoot,
        [Parameter(Mandatory)] [string]$ArchiveRoot
    )

    $source = Assert-UnderRoot -Target (Join-Path $RepoRoot ($RelativePath -replace '/', '\')) -Root $RepoRoot -Label 'Archive source'
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Archive source file not found: $source"
    }

    $destination = Join-Path $ArchiveRoot ($RelativePath -replace '/', '\')
    $destinationParent = Split-Path -Parent $destination
    New-Item -ItemType Directory -Path $destinationParent -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $destination -Force
}

function Get-ArchiveRelativePath {
    param(
        [Parameter(Mandatory)] [string]$ArchiveRoot,
        [Parameter(Mandatory)] [string]$Path
    )

    return [System.IO.Path]::GetRelativePath($ArchiveRoot, $Path).Replace('\', '/')
}

$RepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)
$DestinationRoot = [System.IO.Path]::GetFullPath($DestinationRoot)
if (-not (Test-Path -LiteralPath $RepoRoot -PathType Container)) {
    throw "Repository root not found: $RepoRoot"
}

$manifestObject = Get-ReadmeMediaManifest -ManifestPath $Manifest -RepoRoot $RepoRoot
$manifestErrors = @(Test-ReadmeMediaManifest -Manifest $manifestObject -RepoRoot $RepoRoot)
if ($manifestErrors.Count -gt 0) {
    throw "README media manifest is invalid:`n$($manifestErrors -join "`n")"
}

New-Item -ItemType Directory -Path $DestinationRoot -Force | Out-Null
$archiveName = 'README_Media_{0}' -f (Get-Date -Format 'yyyyMMdd_HHmmss')
$archiveRoot = Join-Path $DestinationRoot $archiveName
if (Test-Path -LiteralPath $archiveRoot) {
    throw "Archive directory already exists: $archiveRoot"
}
New-Item -ItemType Directory -Path $archiveRoot | Out-Null

Copy-ArchiveFile -RelativePath 'README.md' -RepoRoot $RepoRoot -ArchiveRoot $archiveRoot
Copy-ArchiveFile -RelativePath 'README_old.md' -RepoRoot $RepoRoot -ArchiveRoot $archiveRoot

$mediaSource = Join-Path $RepoRoot 'docs\media\readme'
if (-not (Test-Path -LiteralPath $mediaSource -PathType Container)) {
    throw "README media directory not found: $mediaSource"
}
$mediaDestination = Join-Path $archiveRoot 'docs\media\readme'
New-Item -ItemType Directory -Path $mediaDestination -Force | Out-Null
foreach ($item in @(Get-ChildItem -LiteralPath $mediaSource -Force)) {
    Copy-Item -LiteralPath $item.FullName -Destination $mediaDestination -Recurse -Force
}

$projectRoot = $RepoRoot
if ($null -ne $manifestObject.PSObject.Properties['runtimeDir']) {
    $runtimePath = Resolve-ReadmeMediaPath -RepoRoot $RepoRoot -Path ([string]$manifestObject.runtimeDir)
    $projectRoot = Split-Path -Parent $runtimePath
}

foreach ($project in @($manifestObject.projects)) {
    $projectDirectory = Resolve-ReadmeMediaPath -RepoRoot $projectRoot -Path ([string]$project.directory)
    $projectReadme = Join-Path $projectDirectory 'README.md'
    $projectRelativePath = [System.IO.Path]::GetRelativePath($RepoRoot, $projectReadme).Replace('\', '/')
    Assert-UnderRoot -Target $projectReadme -Root $RepoRoot -Label "Project README $($project.number)" | Out-Null
    Copy-ArchiveFile -RelativePath $projectRelativePath -RepoRoot $RepoRoot -ArchiveRoot $archiveRoot
}

$sourceCommit = (& git -C $RepoRoot rev-parse HEAD 2>$null | Select-Object -First 1)
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace([string]$sourceCommit)) {
    $sourceCommit = $null
}
else {
    $sourceCommit = ([string]$sourceCommit).Trim()
}

$metadataPath = Join-Path $archiveRoot 'archive-manifest.json'
$archivedFiles = @(
    Get-ChildItem -LiteralPath $archiveRoot -Recurse -File -Force |
        Where-Object { $_.FullName -ne $metadataPath } |
        ForEach-Object {
            [pscustomobject]@{
                path = Get-ArchiveRelativePath -ArchiveRoot $archiveRoot -Path $_.FullName
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            }
        } |
        Sort-Object -Property path
)

$archiveMetadata = [ordered]@{
    createdAt = (Get-Date).ToUniversalTime().ToString('o')
    sourceRoot = $RepoRoot
    sourceCommit = $sourceCommit
    files = $archivedFiles
}
$archiveMetadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $metadataPath -Encoding utf8

Write-Host "Archived README media to $archiveRoot"
