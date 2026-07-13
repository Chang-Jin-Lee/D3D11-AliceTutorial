$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '..\readme_media_common.ps1')

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$captureScript = Join-Path $repoRoot 'tools\capture_readme_media.ps1'

$validation = & $captureScript -Manifest 'tools/readme_media_manifest.json' -ValidateOnly
if ($validation -notcontains 'capture manifest validation passed') {
    throw 'capture manifest validation failed'
}

$manifest = Get-ReadmeMediaManifest -ManifestPath 'tools/readme_media_manifest.json' -RepoRoot $repoRoot
$project36 = Get-ReadmeMediaProject -Manifest $manifest -Number '36'
if (@($project36.preCaptureActions).Count -ne 2) {
    throw 'project 36 start actions missing'
}

$sway = @($manifest.projects | Where-Object { @($_.gifActions).Count -eq 4 })
if ($sway.Count -lt 1) {
    throw 'camera sway actions missing'
}

'capture action tests passed'
