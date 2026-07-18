$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifestPath = Join-Path $repoRoot 'tools\readme_media_manifest.json'
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json

function Get-Project([string]$Number) {
    $project = @($manifest.projects | Where-Object { $_.number -eq $Number })
    Assert-True ($project.Count -eq 1) "manifest project $Number missing or duplicated"
    return $project[0]
}

foreach ($number in @('11', '12', '13', '25', '30', '36')) {
    $project = Get-Project $number
    Assert-True ([bool]$project.readmeCaptureMode) "project $number must enable README capture mode"
}

foreach ($number in @('12', '13', '25', '30')) {
    $project = Get-Project $number
    $gifActions = @($project.gifActions | Where-Object { $null -ne $_ })
    Assert-True ($gifActions.Count -eq 0) "project $number must use intrinsic scene motion instead of camera input"
}

$project07 = Get-Project '07'
$project07Actions = @($project07.gifActions | Where-Object { $null -ne $_ })
Assert-True ($project07Actions.Count -eq 4) 'project 07 must keep one short right/left camera movement pair'
foreach ($key in @('D', 'A')) {
    $keyActions = @($project07Actions | Where-Object { $_.key -eq $key } | Sort-Object atMs)
    Assert-True ($keyActions.Count -eq 2) "project 07 must define one keyDown/keyUp pair for $key"
    Assert-True ($keyActions[0].type -eq 'keyDown' -and $keyActions[1].type -eq 'keyUp') "project 07 $key actions must remain ordered"
    Assert-True (([int]$keyActions[1].atMs - [int]$keyActions[0].atMs) -le 250) "project 07 $key camera movement must not exceed 250 ms at speed 15"
}

foreach ($captureProject in @(
    @{ Number = '12'; Source = 'Dx11\12_Lighting_BlinnPhong\App.cpp' },
    @{ Number = '13'; Source = 'Dx11\13_LineRenderer_AxisDebug\App.cpp' }
)) {
    $source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot $captureProject.Source)
    Assert-True ($source -match '#include\s+"\.\./Common/ReadmeCapture\.h"') "project $($captureProject.Number) is missing the shared README capture gate"
    Assert-True ($source -match 'if\s*\(!ReadmeCapture::IsEnabled\(\)\)') "project $($captureProject.Number) must suppress its high-entropy skybox only during capture"
}

$project13 = Get-Project '13'
Assert-True ([int]$project13.delayMs -ge 2500) 'project 13 capture delay must allow the scene to stabilize'
$project13Source = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\13_LineRenderer_AxisDebug\App.cpp')
Assert-True ($project13Source -match 'if\s*\(ReadmeCapture::IsEnabled\(\)\)\s*\{\s*m_RotateCube\s*=\s*true;') 'project 13 must use capture-only intrinsic cube rotation'

$project36 = Get-Project '36'
$project36ActionTypes = @($project36.preCaptureActions | ForEach-Object { $_.type })
Assert-True (($project36ActionTypes -join ',') -eq 'click,wait') 'project 36 fallback actions must remain ordered after the load delay'
Assert-True ([int]$project36.delayMs -ge 7000) 'project 36 capture delay must cover local public-model loading'

$live2dSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\11_Live2D\App.cpp')
Assert-True ($live2dSource -match '#include\s+"\.\./Common/ReadmeCapture\.h"') 'project 11 is missing the shared README capture gate'
Assert-True ($live2dSource -match 'ReadmeCapture::IsEnabled\(\)') 'project 11 is missing capture-only loader presentation'
Assert-True ($live2dSource -match 'No redistributable Live2D model is bundled') 'project 11 capture does not explain the safe distributable empty state'
Assert-True ($live2dSource -notmatch '(?i)Doro') 'project 11 must not reference removed Doro assets'

$advancedSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\36_AdvancedAnim_Sound_Click\App_UpdateInput.inl')
$loadedGate = $advancedSource.IndexOf('if (!m_bIsLoaded)', [System.StringComparison]::Ordinal)
$captureAutoStart = $advancedSource.IndexOf('if (IsReadmeCaptureMode())', [System.StringComparison]::Ordinal)
Assert-True ($loadedGate -ge 0) 'project 36 load gate missing'
Assert-True ($captureAutoStart -gt $loadedGate) 'project 36 capture auto-start must occur only after the load gate'
Assert-True ($advancedSource.Substring($captureAutoStart, [Math]::Min(180, $advancedSource.Length - $captureAutoStart)) -match 'm_bIsGameStarted\s*=\s*true') 'project 36 capture mode does not auto-start the loaded scene'

$cameraSource = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Dx11\Common\Camera.cpp')
Assert-True ($cameraSource -match 'SetFrustum\([^;]*0\.01f\s*,\s*1000\.0f\)') 'camera near-plane default changed from 0.01'
Assert-True ($cameraSource -match 'm_MoveSpeed\s*=\s*15\.0f') 'camera speed default changed from 15'

'visual capture contract tests passed'
