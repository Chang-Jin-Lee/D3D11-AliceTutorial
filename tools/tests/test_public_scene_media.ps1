$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$legacyImages = @(
    'Dx11\Resource\Image\SceneA.png',
    'Dx11\Resource\Image\SceneB.png'
)
$publicImages = @(
    'Dx11\Resource\Image\Public\Comic\01.png',
    'Dx11\Resource\Image\Public\Comic\02.png'
)

foreach ($image in $publicImages) {
    Assert-True (Test-Path -LiteralPath (Join-Path $repoRoot $image) -PathType Leaf) "missing public scene image: $image"
}
foreach ($image in $legacyImages) {
    Assert-True (-not (Test-Path -LiteralPath (Join-Path $repoRoot $image))) "legacy scene image remains in repository: $image"
}

$legacyReferences = @(& git -C $repoRoot grep -n -E 'Resource\\\\Image\\\\Scene[AB]\.png' -- 'Dx11/*.cpp' 'Dx11/*.h' 'Dx11/*.inl' 2>$null)
Assert-True ($legacyReferences.Count -eq 0) "legacy scene image references remain:`n$($legacyReferences -join "`n")"

$publicReferences = @(& git -C $repoRoot grep -n -E 'Resource\\\\Image\\\\Public\\\\Comic\\\\0[12]\.png' -- 'Dx11/*.cpp' 'Dx11/*.h' 'Dx11/*.inl' 2>$null)
Assert-True ($publicReferences.Count -ge 10) 'public scene image references were not applied to the shared late projects'

$skyboxManagerPath = Join-Path $repoRoot 'Dx11\Common\SkyboxAssetManager.cpp'
$skyboxManager = Get-Content -Raw -LiteralPath $skyboxManagerPath
Assert-True ($skyboxManager -match '#include\s+"ReadmeCapture\.h"') 'skybox status UI is missing the README capture dependency'
Assert-True ($skyboxManager -match 'if\s*\(ReadmeCapture::IsEnabled\(\)\)\s*\{\s*return;\s*\}') 'skybox status UI remains visible during README capture'
Assert-True ($skyboxManager -match 'void\s+SkyboxAssetManager::EnsureSkyboxAssetsAsync\(\)\s*\{\s*if\s*\(ReadmeCapture::IsEnabled\(\)\)\s*\{\s*return;\s*\}') 'skybox downloader starts during README capture'

$project33Source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\33_Sound_Animation_Camera_Motion\App.cpp')
$project33CaptureUiPattern = 'RenderControlPannel\(\);\s*if\s*\(!ReadmeCapture::IsEnabled\(\)\)\s*\{\s*RenderSceneCollection\(\);\s*RenderModelPannel\(\);\s*RenderConsolPannel\(\);\s*m_->m_SystemInfo\.RenderUI\(\);\s*RenderSceneImageWindow\(\);\s*\}'
Assert-True ($project33Source -match $project33CaptureUiPattern) 'project 33 debug windows obscure the README capture subject'

'public scene media tests passed'
