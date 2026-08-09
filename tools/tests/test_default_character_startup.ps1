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

function Assert-CaptureOnlySetting(
    [string]$InitializeBody,
    [string]$CaptureBlock,
    [string]$Pattern,
    [string]$Project,
    [string]$Description
) {
    $allMatches = [regex]::Matches($InitializeBody, $Pattern)
    Assert-True ($allMatches.Count -eq 1) "$Project must set $Description exactly once during startup"
    Assert-True ($CaptureBlock -match $Pattern) "$Project must keep $Description inside the README capture block"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$singleProjects = @(
    '17_fbx_pmx_obj_WithPhong',
    '18_fbx_Animation'
)
$multiProjects = @(
    '19_MultiModels',
    '20_Depth_And_Alpha_Issue',
    '21_MultiModels_With_Animations',
    '24_Skinned_With_Bone_Structure'
)
$projects = @($singleProjects) + @($multiProjects)
$expectedProjects = @(
    '17_fbx_pmx_obj_WithPhong',
    '18_fbx_Animation',
    '19_MultiModels',
    '20_Depth_And_Alpha_Issue',
    '21_MultiModels_With_Animations',
    '24_Skinned_With_Bone_Structure'
)

Assert-True ($projects.Count -eq 6) 'startup contract must contain exactly six projects'
Assert-True (@(Compare-Object ($projects | Sort-Object) ($expectedProjects | Sort-Object)).Count -eq 0) 'startup contract target list changed'

$defaultModelCall = 'LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb")'
$capturePattern = 'if\s*\(\s*ReadmeCapture::IsEnabled\(\)\s*\)'
$cameraPositionPattern = 'm_Camera\.SetPosition\(\s*XMFLOAT3\(\s*20\.0f\s*,\s*70\.0f\s*,\s*-150\.0f\s*\)\s*\)'
$cameraRotationPattern = 'm_Camera\.SetRotation\(\s*XMFLOAT3\(\s*10\.0f\s*,\s*-6\.0f\s*,\s*0\.0f\s*\)\s*\)'

foreach ($project in $projects) {
    $sourcePath = Join-Path $repoRoot "Dx11\$project\App.cpp"
    Assert-True (Test-Path -LiteralPath $sourcePath -PathType Leaf) "$project App.cpp missing"
    $source = [IO.File]::ReadAllText($sourcePath)

    $initializeAnchor = $source.IndexOf('bool App::OnInitialize()', [StringComparison]::Ordinal)
    Assert-True ($initializeAnchor -ge 0) "$project App::OnInitialize missing"
    $initializeBody = Get-BracedBlock $source $initializeAnchor "$project App::OnInitialize"

    $loadCallCount = [regex]::Matches($initializeBody, [regex]::Escape($defaultModelCall)).Count
    Assert-True ($loadCallCount -eq 1) "$project must load SampleModel.glb exactly once during startup"

    $guardSuffix = if ($multiProjects -contains $project) {
        '\s*&&\s*!m_->m_Models\.empty\(\)'
    }
    else {
        ''
    }
    $loadConditionPattern = 'if\s*\(\s*' + [regex]::Escape($defaultModelCall) + $guardSuffix + '\s*\)'
    $loadCondition = [regex]::Match($initializeBody, $loadConditionPattern)
    Assert-True $loadCondition.Success "$project default load success guard missing"

    $captureCondition = [regex]::Match($initializeBody, $capturePattern)
    Assert-True $captureCondition.Success "$project README capture gate missing"
    Assert-True ($loadCondition.Index -lt $captureCondition.Index) "$project default model load must happen before the README capture gate"

    $loadBlock = Get-BracedBlock $initializeBody $loadCondition.Index "$project default load block"
    $nestedCaptureCondition = [regex]::Match($loadBlock, $capturePattern)
    Assert-True $nestedCaptureCondition.Success "$project README capture gate must be nested inside the successful default load block"
    $captureBlock = Get-BracedBlock $loadBlock $nestedCaptureCondition.Index "$project README capture block"

    if ($singleProjects -contains $project) {
        $loaderAnchor = $source.IndexOf('bool App::LoadModelFromFile(const std::wstring& pathW)', [StringComparison]::Ordinal)
        Assert-True ($loaderAnchor -ge 0) "$project LoadModelFromFile implementation missing"
        $loaderBody = Get-BracedBlock $source $loaderAnchor "$project LoadModelFromFile"
        Assert-True ($loaderBody -match 'm_->m_modelPos\s*=\s*\{\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\}') "$project loader must reset the single model to the origin"
        Assert-True ($initializeBody -notmatch 'm_->m_modelPos\s*=') "$project startup must not override the loader-provided origin"

        Assert-CaptureOnlySetting $initializeBody $captureBlock 'm_->m_modelScale\s*=\s*XMFLOAT3\(\s*80\.0f\s*,\s*80\.0f\s*,\s*80\.0f\s*\)' $project 'capture model scale'
        Assert-CaptureOnlySetting $initializeBody $captureBlock 'm_->m_modelRotation\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*-35\.0f\s*,\s*0\.0f\s*\)' $project 'capture model rotation'
        Assert-CaptureOnlySetting $initializeBody $captureBlock 'm_->m_RotateModel\s*=\s*true' $project 'capture automatic rotation'
    }
    else {
        $originPattern = 'model\.pos\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\)'
        $originAssignment = [regex]::Match($loadBlock, $originPattern)
        Assert-True $originAssignment.Success "$project must place the loaded model at the origin"
        Assert-True ($originAssignment.Index -lt $nestedCaptureCondition.Index) "$project origin assignment must run outside the README capture gate"

        Assert-CaptureOnlySetting $initializeBody $captureBlock 'model\.scale\s*=\s*XMFLOAT3\(\s*80\.0f\s*,\s*80\.0f\s*,\s*80\.0f\s*\)' $project 'capture model scale'
        Assert-CaptureOnlySetting $initializeBody $captureBlock 'model\.rotDeg\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*-35\.0f\s*,\s*0\.0f\s*\)' $project 'capture model rotation'
        Assert-CaptureOnlySetting $initializeBody $captureBlock 'model\.autoRotate\s*=\s*true' $project 'capture automatic rotation'
    }

    Assert-CaptureOnlySetting $initializeBody $captureBlock $cameraPositionPattern $project 'capture camera position'
    Assert-CaptureOnlySetting $initializeBody $captureBlock $cameraRotationPattern $project 'capture camera rotation'
}

'default character startup contract tests passed'
