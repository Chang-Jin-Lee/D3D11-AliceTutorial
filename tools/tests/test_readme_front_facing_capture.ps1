$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-BracedBlock([string]$Text, [int]$Anchor, [string]$Label) {
    $openBrace = $Text.IndexOf('{', $Anchor)
    Assert-True ($openBrace -ge 0) "$Label opening brace missing"

    $depth = 0
    for ($index = $openBrace; $index -lt $Text.Length; ++$index) {
        if ($Text[$index] -eq '{') { ++$depth }
        elseif ($Text[$index] -eq '}') {
            --$depth
            if ($depth -eq 0) {
                return $Text.Substring($Anchor, $index - $Anchor + 1)
            }
        }
    }

    throw "$Label closing brace missing"
}

function Get-CaptureContract([string]$Source, [string]$Project) {
    $capturePattern = 'if\s*\(\s*ReadmeCapture::IsEnabled\(\)\s*\)'
    $captureConditions = [regex]::Matches($Source, $capturePattern)
    Assert-True ($captureConditions.Count -eq 1) "$Project must have exactly one positive README capture setup branch"

    $condition = $captureConditions[0]
    $block = Get-BracedBlock $Source $condition.Index "$Project README capture setup"
    $normalSource = $Source.Remove($condition.Index, $block.Length)

    [pscustomobject]@{
        Block = $block
        NormalSource = $normalSource
    }
}

function Get-LiteralYaws([string]$Block, [string[]]$Subjects) {
    $yaws = @()
    foreach ($subject in $Subjects) {
        $pattern = [regex]::Escape($subject) + '\s*=\s*XMFLOAT3\(\s*-?\d+(?:\.\d+)?f?\s*,\s*(?<yaw>-?\d+(?:\.\d+)?)f?\s*,\s*-?\d+(?:\.\d+)?f?\s*\)'
        $matches = [regex]::Matches($Block, $pattern)
        foreach ($match in $matches) {
            $yaws += [double]$match.Groups['yaw'].Value
        }
    }
    return $yaws
}

function Get-ArrayYaws([string]$Block, [string]$Project) {
    $array = [regex]::Match($Block, 'const\s+float\s+characterYaw\s*\[\s*8\s*\]\s*=\s*\{(?<values>[^}]*)\}')
    Assert-True $array.Success "$Project rear-facing crowd regression: capture characterYaw[8] array missing"

    $yaws = @()
    foreach ($number in [regex]::Matches($array.Groups['values'].Value, '-?\d+(?:\.\d+)?(?=f?\s*(?:,|$))')) {
        $yaws += [double]$number.Value
    }
    return $yaws
}

function Assert-YawThenRotationDisabled(
    [string]$Block,
    [string]$YawSubject,
    [string]$RotationSubject,
    [string]$Project,
    [string]$Regression
) {
    $yawPattern = [regex]::Escape($YawSubject) + '\s*=\s*XMFLOAT3\(\s*0(?:\.0)?f?\s*,\s*-?\d+(?:\.\d+)?f?\s*,\s*0(?:\.0)?f?\s*\)\s*;'
    $disablePattern = [regex]::Escape($RotationSubject) + '\s*=\s*false\s*;'
    $orderedPattern = '(?s)' + $yawPattern + '.*?' + $disablePattern
    Assert-True ($Block -match $orderedPattern) "$Project $Regression regression: fixed capture yaw must be followed by automatic-rotation disable"
}

function Assert-CaptureOnlyUiComposition(
    [string]$Source,
    [string]$Project,
    [string[]]$HiddenCalls,
    [string[]]$VisibleCalls
) {
    $guards = [regex]::Matches($Source, 'if\s*\(\s*!\s*ReadmeCapture::IsEnabled\(\)\s*\)')
    Assert-True ($guards.Count -gt 0) "$Project capture-only UI guard missing"
    $normalUi = ($guards | ForEach-Object {
        Get-BracedBlock $Source $_.Index "$Project normal-mode UI composition"
    }) -join "`n"

    foreach ($call in $HiddenCalls) {
        Assert-True ($normalUi -match ([regex]::Escape($call) + '\s*;')) `
            "$Project capture UI regression: $call must be hidden during README capture"
    }
    foreach ($call in $VisibleCalls) {
        Assert-True ($normalUi -notmatch ([regex]::Escape($call) + '\s*;')) `
            "$Project tutorial evidence regression: $call must remain visible during README capture"
        Assert-True ($Source -match ([regex]::Escape($call) + '\s*;')) `
            "$Project tutorial evidence regression: $call missing"
    }
}

function Assert-LegacyCaptureUiHidden([string]$Source, [string]$Project, [string[]]$WindowTitles) {
    $guards = [regex]::Matches($Source, 'if\s*\(\s*!\s*ReadmeCapture::IsEnabled\(\)\s*\)')
    $uiBlock = $null
    foreach ($guard in $guards) {
        $candidate = Get-BracedBlock $Source $guard.Index "$Project capture-only UI guard"
        if ($candidate -match 'ImGui::Begin\(\s*"Controls"') {
            $uiBlock = $candidate
            break
        }
    }
    Assert-True ($null -ne $uiBlock) "$Project capture-only legacy UI guard missing"
    foreach ($title in $WindowTitles) {
        Assert-True ($uiBlock -match ('ImGui::Begin\(\s*"' + [regex]::Escape($title) + '"')) `
            "$Project capture UI regression: $title must stay hidden from the primary subject"
    }
}

function Assert-LegacyCaptureEvidenceUi([string]$Source, [string]$Project) {
    $guards = [regex]::Matches($Source, 'if\s*\(\s*!\s*ReadmeCapture::IsEnabled\(\)')
    Assert-True ($guards.Count -ge 2) "$Project capture-only UI guards missing"
    $hiddenUi = ($guards | ForEach-Object {
        Get-BracedBlock $Source $_.Index "$Project capture-only UI guard"
    }) -join "`n"

    Assert-True ($hiddenUi -match 'm_SystemInfo\.RenderUI\(\)\s*;') `
        "$Project capture UI regression: System Info must stay hidden from the primary subject"
    Assert-True ($hiddenUi -match 'ImGui::Begin\(\s*"Skybox Face"') `
        "$Project capture UI regression: Skybox Face must stay hidden from the primary subject"
    Assert-True ($hiddenUi -notmatch 'ImGui::Begin\(\s*"Controls"') `
        "$Project tutorial evidence regression: Controls must remain visible during README capture"
    Assert-True ($Source -match 'ImGui::Begin\(\s*"Controls"') `
        "$Project tutorial evidence regression: Controls window missing"
}
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifestPath = Join-Path $repoRoot 'tools\readme_media_manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$legacyProjects = @(
    '07_pmxTexture',
    '15_pmxWithPhong',
    '16_NormalMapping'
)
$singleProjects = @(
    '17_fbx_pmx_obj_WithPhong',
    '18_fbx_Animation'
)
$perModelProjects = @(
    '19_MultiModels',
    '20_Depth_And_Alpha_Issue',
    '21_MultiModels_With_Animations',
    '22_VMD',
    '24_Skinned_With_Bone_Structure'
)
$crowdProjects = @(
    '26_ShadowMap_PCF',
    '27_DebugDraw'
)
$heroProjects = @(
    '28_Scene_Shared3DModel_Animation',
    '29_MousePicking'
)
$companionProjects = @(
    '30_PBR_BRDF',
    '31_IBL',
    '32_Sound_FMOD',
    '33_Sound_Animation_Camera_Motion'
)
$projects = @($singleProjects) + @($perModelProjects) + @($crowdProjects) + @($heroProjects) + @($companionProjects)

Assert-True ($projects.Count -eq 15) 'front-facing capture contract must contain exactly 15 projects'
Assert-True (($projects | Select-Object -Unique).Count -eq 15) 'front-facing capture contract project list contains duplicates'

foreach ($project in $legacyProjects) {
    $projectNumber = [int]$project.Substring(0, 2)
    $manifestEntry = @($manifest.projects | Where-Object { [int]$_.number -eq $projectNumber })
    Assert-True ($manifestEntry.Count -eq 1) "$project manifest entry missing or duplicated"
    Assert-True ($manifestEntry[0].readmeCaptureMode -eq $true) `
        "$project manifest must enable readmeCaptureMode"

    $sourcePath = Join-Path $repoRoot "Dx11\$project\App.cpp"
    Assert-True (Test-Path -LiteralPath $sourcePath -PathType Leaf) "$project App.cpp missing"
}

foreach ($projectNumber in @('05', '06')) {
    $manifestEntry = @($manifest.projects | Where-Object { $_.number -ceq $projectNumber })
    Assert-True ($manifestEntry.Count -eq 1) "$projectNumber manifest entry missing or duplicated"
    Assert-True ($manifestEntry[0].readmeCaptureMode -ne $true) `
        "$projectNumber must not promise the reverted README capture path"
}

foreach ($project in @('06_pmx', '07_pmxTexture')) {
    $source = [IO.File]::ReadAllText((Join-Path $repoRoot "Dx11\$project\App.cpp"))
    $contract = Get-CaptureContract $source $project
    Assert-True ($contract.Block -match 't0\s*=\s*XMConvertToRadians\(\s*-25\.0f\s*\)\s*;') `
        "$project capture branch must set a deterministic front three-quarter yaw"
    Assert-True ($source -match '(?s)if\s*\(\s*!\s*ReadmeCapture::IsEnabled\(\)\s*\).*?t0\s*\+=') `
        "$project capture mode must freeze automatic model rotation"
    Assert-True ($contract.NormalSource -match 't0\s*\+=\s*0\.6f\s*\*\s*dt\s*;') `
        "$project normal-mode automatic rotation changed"
    Assert-True ($contract.Block -notmatch 'm_CameraPos|m_CameraFovDeg') `
        "$project capture yaw fix must preserve the existing model auto-fit camera"
    Assert-True ($contract.NormalSource -match 'm_CameraPos\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*targetRadius\s*\*\s*0\.5f\s*,\s*-targetRadius\s*\*\s*2\.0f\s*\)') `
        "$project model auto-fit camera contract changed"
}

$project05 = [IO.File]::ReadAllText((Join-Path $repoRoot 'Dx11\05_Mesh\App.cpp'))
$project05Capture = Get-CaptureContract $project05 '05_Mesh'
Assert-True ($project05Capture.Block -match 'm_CameraPos\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*1\.0f\s*,\s*-4\.0f\s*\)') `
    '05_Mesh capture must frame the haniwa readably'

foreach ($project in @('15_pmxWithPhong', '16_NormalMapping')) {
    $source = [IO.File]::ReadAllText((Join-Path $repoRoot "Dx11\$project\App.cpp"))
    $capture = (Get-CaptureContract $source $project).Block
    Assert-True ($capture -match 'SetPosition\s*\(') `
        "$project capture must set full head-and-torso camera position"
    Assert-True ($capture -match 'm_cubeRotation\s*=\s*XMFLOAT3') `
        "$project capture must set a deterministic front three-quarter model yaw"
}

# Projects 15 and 16 share a scene whose reflective mirror cube sits 2.5 units to the
# right of the subject and resolves to a large amorphous dark shape. The approved
# stills push it out of frame with a capture-only camera/subject offset plus a narrower
# field of view, so lock those exact values in.
$legacySubjectFraming = @(
    [pscustomobject]@{
        Project = '15_pmxWithPhong'
        CameraPosition = '-3\.35f\s*,\s*1\.55f\s*,\s*-4\.5f'
        SubjectPosition = '-2\.1f\s*,\s*0\.0f\s*,\s*0\.0f'
        Framing = 'full-character framing that keeps the face, hands and feet in frame'
    },
    [pscustomobject]@{
        Project = '16_NormalMapping'
        CameraPosition = '-5\.5f\s*,\s*0\.0f\s*,\s*-11\.5f'
        SubjectPosition = '-2\.8f\s*,\s*0\.0f\s*,\s*0\.0f'
        Framing = 'framing that keeps the whole normal-mapped cube in frame'
    }
)
foreach ($framing in $legacySubjectFraming) {
    $source = [IO.File]::ReadAllText((Join-Path $repoRoot "Dx11\$($framing.Project)\App.cpp"))
    $capture = (Get-CaptureContract $source $framing.Project).Block
    Assert-True ($capture -match ('m_Camera\.SetPosition\s*\(\s*XMFLOAT3\(\s*' + $framing.CameraPosition + '\s*\)\s*\)')) `
        "$($framing.Project) capture must keep the approved $($framing.Framing)"
    Assert-True ($capture -match 'm_Camera\.SetFrustum\s*\(\s*XMConvertToRadians\(\s*45\.0f\s*\)') `
        "$($framing.Project) capture must narrow the field of view so the subject fills the still"
    Assert-True ($capture -match ('m_cubePos\s*=\s*XMFLOAT3\(\s*' + $framing.SubjectPosition + '\s*\)')) `
        "$($framing.Project) capture must offset the subject so the reflective mirror cube stays out of the still"
    Assert-LegacyCaptureEvidenceUi $source $framing.Project
}

foreach ($project in @('05_Mesh', '06_pmx')) {
    $source = [IO.File]::ReadAllText((Join-Path $repoRoot "Dx11\$project\App.cpp"))
    Assert-LegacyCaptureUiHidden $source $project @('Controls', 'System Info')
}
$project07 = [IO.File]::ReadAllText((Join-Path $repoRoot 'Dx11\07_pmxTexture\App.cpp'))
Assert-LegacyCaptureUiHidden $project07 '07_pmxTexture' @('Controls', 'System Info', 'Model Info')

$uiContracts = @(
    [pscustomobject]@{ Project = '26_ShadowMap_PCF'; Hidden = @('RenderSceneCollection()', 'RenderModelPannel()', 'RenderConsolPannel()', 'm_->m_SystemInfo.RenderUI()', 'RenderWidgetUI()'); Visible = @('RenderControlPannel()') },
    [pscustomobject]@{ Project = '27_DebugDraw'; Hidden = @('RenderSceneCollection()', 'RenderModelPannel()', 'RenderConsolPannel()', 'm_->m_SystemInfo.RenderUI()', 'RenderWidgetUI()'); Visible = @('RenderControlPannel()') },
    [pscustomobject]@{ Project = '29_MousePicking'; Hidden = @('RenderControlPannel()', 'RenderModelPannel()', 'RenderConsolPannel()', 'm_->m_SystemInfo.RenderUI()', 'RenderSceneImageWindow()'); Visible = @('RenderSceneCollection()') },
    [pscustomobject]@{ Project = '30_PBR_BRDF'; Hidden = @('RenderSceneCollection()', 'RenderModelPannel()', 'RenderConsolPannel()', 'm_->m_SystemInfo.RenderUI()', 'RenderSceneImageWindow()'); Visible = @('RenderControlPannel()') },
    [pscustomobject]@{ Project = '31_IBL'; Hidden = @('RenderSceneCollection()', 'RenderModelPannel()', 'RenderConsolPannel()', 'm_->m_SystemInfo.RenderUI()', 'RenderSceneImageWindow()'); Visible = @('RenderControlPannel()') },
    [pscustomobject]@{ Project = '32_Sound_FMOD'; Hidden = @('RenderSceneCollection()', 'RenderModelPannel()', 'RenderConsolPannel()', 'm_->m_SystemInfo.RenderUI()', 'RenderSceneImageWindow()'); Visible = @('RenderControlPannel()') },
    [pscustomobject]@{ Project = '34_ToneMapping'; Hidden = @('RenderSceneCollection()', 'RenderModelPannel()', 'RenderConsolPannel()', 'm_->m_SystemInfo.RenderUI()', 'RenderSceneImageWindow()'); Visible = @('RenderControlPannel()') },
    [pscustomobject]@{ Project = '35_DeferredRendering'; Hidden = @('RenderControlPannel()', 'RenderSceneCollection()', 'RenderModelPannel()', 'RenderConsolPannel()', 'm_->m_SystemInfo.RenderUI()', 'RenderSceneImageWindow()'); Visible = @('RenderDeferredUI()') }
)
foreach ($uiContract in $uiContracts) {
    $source = [IO.File]::ReadAllText((Join-Path $repoRoot "Dx11\$($uiContract.Project)\App.cpp"))
    Assert-CaptureOnlyUiComposition $source $uiContract.Project $uiContract.Hidden $uiContract.Visible
}

foreach ($project in @('29_MousePicking', '35_DeferredRendering')) {
    $source = [IO.File]::ReadAllText((Join-Path $repoRoot "Dx11\$project\App.cpp"))
    Assert-True ($source -match 'ReadmeCapture::IsEnabled\(\)\s*\?\s*ImGuiCond_Always\s*:\s*ImGuiCond_FirstUseEver') `
        "$project retained evidence window must use deterministic capture-only placement"
    Assert-True ($source -match 'ReadmeCapture::IsEnabled\(\)\s*\?\s*ImVec2\(\s*10\.0f\s*,\s*20\.0f\s*\)') `
        "$project retained evidence window must stay visible at the capture left edge"
}

$project31Source = [IO.File]::ReadAllText((Join-Path $repoRoot 'Dx11\31_IBL\App.cpp'))
$project31Capture = Get-CaptureContract $project31Source '31_IBL'
Assert-True ($project31Capture.Block -match 'player\.scale\s*=\s*XMFLOAT3\(\s*100\.0f\s*,\s*100\.0f\s*,\s*100\.0f\s*\)') `
    '31_IBL capture must restore a readable player scale alongside the PBR spheres'

foreach ($project in $projects) {
    $sourcePath = Join-Path $repoRoot "Dx11\$project\App.cpp"
    Assert-True (Test-Path -LiteralPath $sourcePath -PathType Leaf) "$project App.cpp missing"
    $source = [IO.File]::ReadAllText($sourcePath)
    $contract = Get-CaptureContract $source $project
    $capture = $contract.Block

    Assert-True ($capture -notmatch '(?:m_RotateModel|autoRotate)\s*=\s*true\s*;') `
        "$project capture branch must never enable rotation before the delayed still"

    $subjects = @()
    $expectedYawCount = 0
    $regression = ''
    if ($singleProjects -contains $project) {
        $regression = 'delayed single-hero back/side turn'
        Write-Host "Checking $project - $regression"
        $subjects = @('m_->m_modelRotation')
        $expectedYawCount = 1
        Assert-YawThenRotationDisabled $capture 'm_->m_modelRotation' 'm_->m_RotateModel' $project $regression
        Assert-True ($contract.NormalSource -match 'bool\s+m_RotateModel\s*=\s*false\s*;') `
            "$project normal-mode m_RotateModel default changed"
        Assert-True ($contract.NormalSource -match '(?s)if\s*\(\s*m_->m_RotateModel\s*\)\s*\{.*?m_->m_modelRotation\.y\s*\+=\s*45\.0f\s*\*\s*dt\s*;.*?m_->m_modelRotation\.y\s*=\s*std::fmod\(\s*m_->m_modelRotation\.y\s*\+\s*180\.0f\s*,\s*360\.0f\s*\)\s*-\s*180\.0f\s*;') `
            "$project normal-mode guarded 45-degree update loop changed"
    }
    else {
        Assert-True ($contract.NormalSource -match 'bool\s+autoRotate\s*=\s*false\s*;') `
            "$project normal-mode per-model autoRotate default changed"
        Assert-True ($contract.NormalSource -match '(?s)if\s*\(\s*mdl\.autoRotate\s*\)\s*\{.*?mdl\.rotDeg\.y\s*\+=\s*45\.0f\s*\*\s*dt\s*;.*?mdl\.rotDeg\.y\s*=\s*std::fmod\(\s*mdl\.rotDeg\.y\s*\+\s*180\.0f\s*,\s*360\.0f\s*\)\s*-\s*180\.0f\s*;') `
            "$project normal-mode guarded 45-degree update loop changed"

        if ($perModelProjects -contains $project) {
            $regression = 'delayed per-model back/side turn'
            Write-Host "Checking $project - $regression"
            if ($project -eq '22_VMD') {
                $subjects = @('m_->m_Models[0]->rotDeg')
                Assert-YawThenRotationDisabled $capture 'm_->m_Models[0]->rotDeg' 'm_->m_Models[0]->autoRotate' $project $regression
            }
            else {
                $subjects = @('model.rotDeg')
                Assert-YawThenRotationDisabled $capture 'model.rotDeg' 'model.autoRotate' $project $regression
            }
            $expectedYawCount = 1
        }
        elseif ($crowdProjects -contains $project) {
            $regression = 'rear-facing multi-character shadow/debug crowd'
            Write-Host "Checking $project - $regression"
            $yaws = @(Get-ArrayYaws $capture $project)
            $expectedYawCount = 8
            Assert-True ($capture -match '(?s)model\.rotDeg\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*characterYaw\[i\]\s*,\s*0\.0f\s*\)\s*;.*?model\.autoRotate\s*=\s*false\s*;') `
                "$project $regression regression: array-selected yaw must be followed by per-model rotation disable"
        }
        elseif ($heroProjects -contains $project) {
            $regression = 'drifting hero side-view'
            Write-Host "Checking $project - $regression"
            $subjects = @('player.rotDeg')
            $expectedYawCount = 1
            Assert-YawThenRotationDisabled $capture 'player.rotDeg' 'player.autoRotate' $project $regression
        }
        else {
            $regression = 'rear-facing visible companion'
            Write-Host "Checking $project - $regression"
            $subjects = @('player.rotDeg', 'enemy.rotDeg')
            $expectedYawCount = 2
            Assert-YawThenRotationDisabled $capture 'player.rotDeg' 'player.autoRotate' $project $regression
            Assert-YawThenRotationDisabled $capture 'enemy.rotDeg' 'enemy.autoRotate' $project $regression
        }
    }

    if ($crowdProjects -notcontains $project) {
        $yaws = @(Get-LiteralYaws $capture $subjects)
    }
    Assert-True ($yaws.Count -eq $expectedYawCount) `
        "$project $regression regression: expected $expectedYawCount visible capture yaw values, found $($yaws.Count)"
    Assert-True ([Math]::Abs($yaws[0] - -25.0) -lt 0.001) `
        "$project $regression regression: main hero capture yaw must remain the fixed -25-degree three-quarter view"
    foreach ($yaw in $yaws) {
        Assert-True ([Math]::Abs($yaw) -le 35.0) `
            "$project $regression regression: visible capture yaw $yaw is outside the front-facing +/-35-degree range"
    }
    for ($yawIndex = 1; $yawIndex -lt $yaws.Count; ++$yawIndex) {
        Assert-True ([Math]::Abs($yaws[$yawIndex]) -le 20.0) `
            "$project $regression regression: companion capture yaw $($yaws[$yawIndex]) is outside the -20..20-degree range"
    }
}

'README front-facing capture contract tests passed'
