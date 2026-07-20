$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script:Failures = [System.Collections.Generic.List[string]]::new()

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        $script:Failures.Add($Message)
    }
}

function Assert-Contains {
    param(
        [string]$Text,
        [string]$Expected,
        [string]$Message
    )

    Assert-True -Condition $Text.Contains($Expected) -Message $Message
}

function Assert-NotContains {
    param(
        [string]$Text,
        [string]$Unexpected,
        [string]$Message
    )

    Assert-True -Condition (-not $Text.Contains($Unexpected)) -Message $Message
}

function Assert-Matches {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    Assert-True -Condition ([regex]::IsMatch($Text, $Pattern)) -Message $Message
}

function Assert-ContainsInOrderBefore {
    param(
        [string]$Text,
        [string]$Anchor,
        [string[]]$Expected,
        [string]$Boundary,
        [string]$Message
    )

    $anchorIndex = $Text.IndexOf($Anchor, [System.StringComparison]::Ordinal)
    Assert-True -Condition ($anchorIndex -ge 0) `
        -Message "$Message (missing anchor: $Anchor)"
    if ($anchorIndex -lt 0) {
        return
    }

    $boundaryIndex = $Text.Length
    if (-not [string]::IsNullOrEmpty($Boundary)) {
        $boundaryIndex = $Text.IndexOf($Boundary, $anchorIndex, [System.StringComparison]::Ordinal)
        Assert-True -Condition ($boundaryIndex -ge 0) `
            -Message "$Message (missing boundary after anchor: $Boundary)"
        if ($boundaryIndex -lt 0) {
            return
        }
    }

    $searchIndex = $anchorIndex + $Anchor.Length
    foreach ($item in $Expected) {
        $itemIndex = $Text.IndexOf($item, $searchIndex, [System.StringComparison]::Ordinal)
        $isInOrderBeforeBoundary = $itemIndex -ge 0 -and $itemIndex -lt $boundaryIndex
        Assert-True -Condition $isInOrderBeforeBoundary `
            -Message "$Message (missing or out of order before boundary: $item)"
        if (-not $isInOrderBeforeBoundary) {
            return
        }
        $searchIndex = $itemIndex + $item.Length
    }
}

function Read-RepoText {
    param([string]$RelativePath)

    return Get-Content -Raw -LiteralPath (Join-Path $repoRoot $RelativePath)
}

$assimpRelative = 'Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll'
$assimpDll = Join-Path $repoRoot $assimpRelative
$targetsPath = Join-Path $repoRoot 'Dx11/Directory.Build.targets'

Assert-True -Condition (Test-Path -LiteralPath $assimpDll -PathType Leaf) `
    -Message "Assimp runtime DLL is missing: $assimpRelative"
if (Test-Path -LiteralPath $assimpDll -PathType Leaf) {
    Assert-True -Condition ((Get-Item -LiteralPath $assimpDll).Length -gt 0) `
        -Message 'Assimp runtime DLL is empty.'
}

& git -C $repoRoot check-ignore --quiet --no-index -- $assimpRelative
$ignoredExitCode = $LASTEXITCODE
Assert-True -Condition ($ignoredExitCode -ne 0) `
    -Message 'Assimp runtime DLL is still ignored by Git.'

$trackedFiles = @(& git -C $repoRoot ls-files -- $assimpRelative)
Assert-True -Condition ($trackedFiles -contains $assimpRelative) `
    -Message 'Assimp runtime DLL is not tracked by Git.'

$assimpOutputRelatives = @(
    'Dx11/bin/assimp-vc143-mt.dll',
    'Dx11/x64/Debug/assimp-vc143-mt.dll',
    'Dx11/x64/Release/assimp-vc143-mt.dll'
)

$canonicalAssimpHash = $null
if (Test-Path -LiteralPath $assimpDll -PathType Leaf) {
    $canonicalAssimpHash = (Get-FileHash -LiteralPath $assimpDll -Algorithm SHA256).Hash
}

foreach ($outputRelative in $assimpOutputRelatives) {
    $outputDll = Join-Path $repoRoot $outputRelative
    Assert-True -Condition (Test-Path -LiteralPath $outputDll -PathType Leaf) `
        -Message "Assimp output runtime DLL is missing: $outputRelative"

    & git -C $repoRoot check-ignore --quiet --no-index -- $outputRelative
    $outputIgnoredExitCode = $LASTEXITCODE
    Assert-True -Condition ($outputIgnoredExitCode -ne 0) `
        -Message "Assimp output runtime DLL is still ignored by Git: $outputRelative"

    $trackedOutputFiles = @(& git -C $repoRoot ls-files -- $outputRelative)
    Assert-True -Condition ($trackedOutputFiles -contains $outputRelative) `
        -Message "Assimp output runtime DLL is not tracked by Git: $outputRelative"

    if (Test-Path -LiteralPath $outputDll -PathType Leaf) {
        Assert-True -Condition ((Get-Item -LiteralPath $outputDll).Length -gt 0) `
            -Message "Assimp output runtime DLL is empty: $outputRelative"
        if ($canonicalAssimpHash) {
            $outputHash = (Get-FileHash -LiteralPath $outputDll -Algorithm SHA256).Hash
            Assert-True -Condition ($outputHash -eq $canonicalAssimpHash) `
                -Message "Assimp output runtime DLL differs from the canonical DLL: $outputRelative"
        }
    }
}

$targets = Get-Content -Raw -LiteralPath $targetsPath
Assert-Contains -Text $targets -Expected 'assimp\bin\msvc\assimp-vc143-mt.dll' `
    -Message 'Directory.Build.targets does not name the Assimp runtime DLL.'
Assert-Contains -Text $targets -Expected 'DestinationFolder="$(TargetDir)"' `
    -Message 'Directory.Build.targets does not copy runtime DLLs to TargetDir.'
Assert-Contains -Text $targets -Expected 'DestinationFolder="$(CommonBinDir)"' `
    -Message 'Directory.Build.targets does not copy runtime DLLs to the common bin directory.'

$live2DRelative = 'Dx11/Resource/Live2D/Skeleton_Model'
$live2DDir = Join-Path $repoRoot $live2DRelative
$expectedLive2DFiles = @(
    'Skeleton_Model.model3.json',
    'Skeleton_Model.moc3',
    'Skeleton_Model.cdi3.json',
    'Skeleton_Model.2048/texture_00.png',
    'README.md'
)

foreach ($relativeFile in $expectedLive2DFiles) {
    $assetPath = Join-Path $live2DDir $relativeFile
    Assert-True -Condition (Test-Path -LiteralPath $assetPath -PathType Leaf) `
        -Message "Live2D sample asset is missing: $live2DRelative/$relativeFile"
    if (Test-Path -LiteralPath $assetPath -PathType Leaf) {
        Assert-True -Condition ((Get-Item -LiteralPath $assetPath).Length -gt 0) `
            -Message "Live2D sample asset is empty: $live2DRelative/$relativeFile"
    }
}

$modelJsonPath = Join-Path $live2DDir 'Skeleton_Model.model3.json'
if (Test-Path -LiteralPath $modelJsonPath -PathType Leaf) {
    $modelSettings = Get-Content -Raw -LiteralPath $modelJsonPath | ConvertFrom-Json
    $modelReferences = [System.Collections.Generic.List[string]]::new()
    $modelReferences.Add([string]$modelSettings.FileReferences.Moc)
    foreach ($texture in @($modelSettings.FileReferences.Textures)) {
        $modelReferences.Add([string]$texture)
    }
    if ($modelSettings.FileReferences.DisplayInfo) {
        $modelReferences.Add([string]$modelSettings.FileReferences.DisplayInfo)
    }

    foreach ($modelReference in $modelReferences) {
        Assert-True -Condition (-not [string]::IsNullOrWhiteSpace($modelReference)) `
            -Message 'Live2D model3.json contains an empty file reference.'
        if (-not [string]::IsNullOrWhiteSpace($modelReference)) {
            $referencedPath = Join-Path $live2DDir $modelReference
            Assert-True -Condition (Test-Path -LiteralPath $referencedPath -PathType Leaf) `
                -Message "Live2D model3.json reference is missing: $modelReference"
        }
    }
}

$provenancePath = Join-Path $live2DDir 'README.md'
if (Test-Path -LiteralPath $provenancePath -PathType Leaf) {
    $provenance = Get-Content -Raw -LiteralPath $provenancePath
    Assert-Contains -Text $provenance `
        -Expected 'https://github.com/BluePengcho/Open_Source_Hand_Tracking_Live2D_Model' `
        -Message 'Live2D provenance README does not name the upstream repository.'
    Assert-Contains -Text $provenance `
        -Expected '994c4719f081a3f219b62abbeb4a4b43543a48b8' `
        -Message 'Live2D provenance README does not pin the upstream commit.'
}

$live2DHeader = Read-RepoText -RelativePath 'Dx11/11_Live2D/App.h'
$live2DSource = Read-RepoText -RelativePath 'Dx11/11_Live2D/App.cpp'

Assert-Contains -Text $live2DHeader `
    -Expected 'bool LoadLive2DModel(const std::wstring& model3Path);' `
    -Message 'App.h does not declare the shared Live2D load helper.'
Assert-Contains -Text $live2DSource `
    -Expected 'L"..\\Resource\\Live2D\\Skeleton_Model\\Skeleton_Model.model3.json"' `
    -Message 'App.cpp does not name the bundled Live2D startup model.'
Assert-Matches -Text $live2DSource `
    -Pattern 'bool\s+App::LoadLive2DModel\s*\(const\s+std::wstring&\s+model3Path\)' `
    -Message 'App.cpp does not define App::LoadLive2DModel.'
Assert-Contains -Text $live2DSource `
    -Expected 'LoadLive2DModel(kDefaultLive2DModelPath);' `
    -Message 'OnInitialize does not load the bundled Live2D model.'
Assert-Contains -Text $live2DSource `
    -Expected 'LoadLive2DModel(file);' `
    -Message 'The model file picker does not use the shared load helper.'
Assert-NotContains -Text $live2DSource -Unexpected 'GetRenderTextureCount()' `
    -Message '11_Live2D still queries a mask renderer that is absent for no-mask models.'
Assert-NotContains -Text $live2DSource -Unexpected 'GetMaskBuffer(' `
    -Message '11_Live2D still creates Cubism mask surfaces manually.'

$defaultModelProjects = @(
    '17_fbx_pmx_obj_WithPhong',
    '18_fbx_Animation',
    '19_MultiModels',
    '20_Depth_And_Alpha_Issue',
    '21_MultiModels_With_Animations',
    '23_Rigid_Animation',
    '24_Skinned_With_Bone_Structure'
)
$defaultModelLoad = 'LoadModelFromFile(L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb")'

foreach ($project in $defaultModelProjects) {
    $source = Read-RepoText -RelativePath "Dx11/$project/App.cpp"
    Assert-Contains -Text $source -Expected $defaultModelLoad `
        -Message "$project does not load the bundled public player on startup."
}

$toonSource = Read-RepoText -RelativePath 'Dx11/25_ToonShading_Outline/App.cpp'
$animationHeader = Read-RepoText -RelativePath 'Dx11/Common/Mesh/FbxAnimation.h'
$animationSource = Read-RepoText -RelativePath 'Dx11/Common/Mesh/FbxAnimation.cpp'

Assert-Contains -Text $animationHeader -Expected 'void SetExternalClip(const aiAnimation* clip, const std::string& name);' `
    -Message 'FbxAnimation does not expose the external clip entry point.'
Assert-Contains -Text $animationSource -Expected 'm_Clips.push_back(clip);' `
    -Message 'FbxAnimation does not retain the caller-owned external clip pointer.'
Assert-Contains -Text $toonSource -Expected '../Common/Animation/ExternalAnimationClipLibrary.h' `
    -Message '25_ToonShading_Outline does not include the external animation library.'
Assert-Contains -Text $toonSource -Expected 'Animations\\anim_Idle.fbx' `
    -Message '25_ToonShading_Outline does not name the tracked Idle animation.'
Assert-Contains -Text $toonSource -Expected 'ExternalAnimationClipTransform::UnrealCmZUpToGlbMeters' `
    -Message '25_ToonShading_Outline does not convert the Unreal FBX clip to GLB units.'
Assert-Contains -Text $toonSource -Expected 'LoadModelFromFile(kDefaultModelPath, true)' `
    -Message '25_ToonShading_Outline does not opt bundled startup models into Idle.'
Assert-Contains -Text $toonSource -Expected 'SetExternalClip(m_->m_DefaultIdleClip, "Idle")' `
    -Message '25_ToonShading_Outline does not attach Idle to startup animators.'

$characterScaleContracts = @{
    '26_ShadowMap_PCF' = @('const XMFLOAT3 characterScale(100.0f, 100.0f, 100.0f)')
    '27_DebugDraw' = @('const XMFLOAT3 characterScale(100.0f, 100.0f, 100.0f)')
    '28_Scene_Shared3DModel_Animation' = @('player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)', 'mdl.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)')
    '29_MousePicking' = @('player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)', 'mdl.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)')
    '30_PBR_BRDF' = @('player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)', 'enemy.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)', 'mdl.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)')
    '31_IBL' = @('player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)', 'enemy.scale = XMFLOAT3(50.0f, 50.0f, 50.0f)')
    '32_Sound_FMOD' = @('player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)', 'enemy.scale = XMFLOAT3(50.0f, 50.0f, 50.0f)')
    '33_Sound_Animation_Camera_Motion' = @('player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)', 'enemy.scale = XMFLOAT3(50.0f, 50.0f, 50.0f)')
    '34_ToneMapping' = @('player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)')
    '35_DeferredRendering' = @('player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f)')
}

$startupCharacterScaleContracts = @{
    '26_ShadowMap_PCF' = @(
        'const XMFLOAT3 characterScale(100.0f, 100.0f, 100.0f);',
        'for (int i = 0; i < 8 && i < (int)m_->m_Models.size(); ++i)',
        'model.scale = characterScale;'
    )
    '27_DebugDraw' = @(
        'const XMFLOAT3 characterScale(100.0f, 100.0f, 100.0f);',
        'for (int i = 0; i < 8 && i < (int)m_->m_Models.size(); ++i)',
        'model.scale = characterScale;'
    )
    '28_Scene_Shared3DModel_Animation' = @(
        'if (m_->m_Models.size() > 0)',
        'player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f);'
    )
    '29_MousePicking' = @(
        'if (m_->m_Models.size() > 0)',
        'player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f);'
    )
    '30_PBR_BRDF' = @(
        'if (m_->m_Models.size() > 0)',
        'player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f);',
        'if (m_->m_Models.size() > 1)',
        'enemy.scale = XMFLOAT3(100.0f, 100.0f, 100.0f);'
    )
    '31_IBL' = @(
        'if (m_->m_Models.size() > 0)',
        'player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f);',
        'if (m_->m_Models.size() > 3)',
        'enemy.scale = XMFLOAT3(50.0f, 50.0f, 50.0f);'
    )
    '32_Sound_FMOD' = @(
        'if (m_->m_Models.size() > 0)',
        'player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f);',
        'if (m_->m_Models.size() > 3)',
        'enemy.scale = XMFLOAT3(50.0f, 50.0f, 50.0f);'
    )
    '33_Sound_Animation_Camera_Motion' = @(
        'if (m_->m_Models.size() > 0)',
        'player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f);',
        'if (m_->m_Models.size() > 3)',
        'enemy.scale = XMFLOAT3(50.0f, 50.0f, 50.0f);'
    )
    '34_ToneMapping' = @(
        'if (m_->m_Models.size() > 0)',
        'player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f);'
    )
    '35_DeferredRendering' = @(
        'if (m_->m_Models.size() > 0)',
        'player.scale = XMFLOAT3(100.0f, 100.0f, 100.0f);'
    )
}

foreach ($project in $characterScaleContracts.Keys) {
    $source = Read-RepoText -RelativePath "Dx11/$project/App.cpp"
    foreach ($expectedScale in $characterScaleContracts[$project]) {
        Assert-Contains -Text $source -Expected $expectedScale `
            -Message "$project is missing character scale contract: $expectedScale"
    }

    Assert-ContainsInOrderBefore -Text $source `
        -Anchor 'bool App::OnInitialize' `
        -Expected $startupCharacterScaleContracts[$project] `
        -Boundary 'if (ReadmeCapture::IsEnabled())' `
        -Message "$project does not apply startup character scales before the first ReadmeCapture guard."
}

$advancedPanels = Read-RepoText -RelativePath 'Dx11/36_AdvancedAnim_Sound_Click/App_ImGuiPanels.inl'
Assert-Contains -Text $advancedPanels `
    -Expected 'const bool showSoundDebug = ImGui::Begin("Sound Debug");' `
    -Message '36_AdvancedAnim_Sound_Click does not record the Sound Debug Begin result.'
Assert-Contains -Text $advancedPanels -Expected 'if (showSoundDebug)' `
    -Message '36_AdvancedAnim_Sound_Click does not conditionally draw Sound Debug contents.'
Assert-NotContains -Text $advancedPanels -Unexpected 'if (ImGui::Begin("Sound Debug"))' `
    -Message '36_AdvancedAnim_Sound_Click still has the unsafe inline Begin/End pattern.'
Assert-ContainsInOrderBefore -Text $advancedPanels `
    -Anchor 'void App::RenderSoundDebugUI()' `
    -Expected @(
        'const bool showSoundDebug = ImGui::Begin("Sound Debug");',
        'if (showSoundDebug)',
        'ImGui::PopID();',
        'ImGui::End();'
    ) `
    -Boundary '' `
    -Message '36_AdvancedAnim_Sound_Click does not end Sound Debug after its conditional body.'

$live2DReadme = Read-RepoText -RelativePath 'Dx11/11_Live2D/README.md'
$thirdPartyReadme = Read-RepoText -RelativePath 'Dx11/third_party/README.md'

Assert-Contains -Text $live2DReadme -Expected 'Skeleton_Model.model3.json' `
    -Message '11_Live2D README does not document the bundled startup model.'
Assert-Contains -Text $live2DReadme `
    -Expected 'https://github.com/BluePengcho/Open_Source_Hand_Tracking_Live2D_Model' `
    -Message '11_Live2D README does not document the sample model source.'
Assert-Contains -Text $thirdPartyReadme -Expected 'assimp-vc143-mt.dll' `
    -Message 'third_party README does not document the tracked Assimp runtime DLL.'
Assert-Contains -Text $thirdPartyReadme -Expected 'Directory.Build.targets' `
    -Message 'third_party README does not document the shared DLL copy target.'
Assert-Contains -Text $thirdPartyReadme -Expected 'Dx11/x64/Debug' `
    -Message 'third_party README does not document the tracked Debug runtime copy.'
Assert-Contains -Text $thirdPartyReadme -Expected 'Dx11/x64/Release' `
    -Message 'third_party README does not document the tracked Release runtime copy.'
Assert-Contains -Text $thirdPartyReadme -Expected 'Visual Studio skips a rebuild' `
    -Message 'third_party README does not explain the pull-and-F5 runtime contract.'

if ($script:Failures.Count -gt 0) {
    Write-Host 'Portable runtime verification failed:'
    foreach ($failure in $script:Failures) {
        Write-Host " - $failure"
    }
    exit 1
}

Write-Host 'Portable runtime verification passed.'
