param(
    [Parameter(Mandatory = $true)][string]$SkyboxRoot,
    [ValidateSet('All', 'Sample', 'Bridge', 'Indoor')][string]$SetName = 'All',
    [string]$ManifestPath = (Join-Path $PSScriptRoot 'skybox_asset_manifest.json')
)

$ErrorActionPreference = 'Stop'

function Resolve-ContainedAssetPath([string]$Root, [string]$RelativePath) {
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $candidate = [IO.Path]::GetFullPath((Join-Path $rootFull $RelativePath))
    $prefix = $rootFull + [IO.Path]::DirectorySeparatorChar
    if (-not $candidate.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Skybox manifest path escapes its root: $RelativePath"
    }
    return $candidate
}

if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Skybox asset manifest not found: $ManifestPath"
}

$manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
$assets = @($manifest.assets | Where-Object { $SetName -eq 'All' -or $_.set -eq $SetName })
if ($assets.Count -eq 0) {
    throw "Skybox manifest has no assets for set '$SetName'."
}

foreach ($asset in $assets) {
    $path = Resolve-ContainedAssetPath -Root $SkyboxRoot -RelativePath ([string]$asset.relativePath)
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Missing skybox asset: $path"
    }

    $actualSize = (Get-Item -LiteralPath $path).Length
    if ($actualSize -ne [int64]$asset.size) {
        throw "Skybox asset size mismatch: $path (expected $($asset.size), got $actualSize)"
    }

    $actualHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    $expectedHash = ([string]$asset.sha256).ToLowerInvariant()
    if ($actualHash -cne $expectedHash) {
        throw "Skybox asset SHA-256 mismatch: $path"
    }
}

"Skybox asset preflight passed: $SetName ($($assets.Count) files)"
