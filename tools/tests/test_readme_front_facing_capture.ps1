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

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
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
