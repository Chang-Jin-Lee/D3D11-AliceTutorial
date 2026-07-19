# F5 Default Scene QA Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make projects 11, 17-21, and 23-36 open their intended default content safely and visibly when launched with F5, with project 25 playing Idle and project 36 no longer asserting in ImGui.

**Architecture:** Keep the existing tutorial-specific startup scenes and correct only their broken contracts. Extend the portable PowerShell verifier first, let Cubism own its clipping-mask surfaces, add a non-owning external-animation clip path to `FbxAnimation`, apply explicit character-only unit conversions, and balance the project 36 ImGui window lifecycle.

**Tech Stack:** C++17, Direct3D 11, Assimp, Cubism Native SDK, Dear ImGui, PowerShell 7, Visual Studio 2022/MSBuild.

## Global Constraints

- Work from `C:\Github\D3D11-AliceTutorial` and preserve unrelated user changes.
- Treat `Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb` as a meter-scale character and leave ground, sphere, camera, shadow, picking, and sound-distance values in their existing scene units.
- Keep `ExternalAnimationClipLibrary` as the owner of imported `aiAnimation` memory. `FbxAnimation` stores non-owning pointers only.
- Apply external Idle only to project 25's bundled startup models; do not force it onto arbitrary files loaded later through the picker.
- Preserve the existing encoding of legacy sources, especially `Dx11/11_Live2D/App.cpp`, and limit edits to the named blocks.
- Do not change Assimp exception policy for project 36. The four first-chance `DeadlyImportError` messages are handled internally; the terminating defect is the ImGui lifecycle assertion.
- Do not claim visual QA on another computer. The user performs that final check; local completion requires static verification plus x64 Debug and Release builds.
- After every task, run `git diff --check` before committing. Never stage unrelated files.

---

### Task 1: Add default-scene regression contracts

**Files:**

- Modify: `tools/tests/test_portable_runtime.ps1`

- [ ] **Step 1: Add a negative string assertion helper**

Place this directly after `Assert-Contains`:

```powershell
function Assert-NotContains {
    param(
        [string]$Text,
        [string]$Unexpected,
        [string]$Message
    )

    Assert-True -Condition (-not $Text.Contains($Unexpected)) -Message $Message
}
```

- [ ] **Step 2: Add project 11 mask-lifecycle contracts**

After the existing Live2D load-helper assertions, reject application-owned mask-buffer access:

```powershell
Assert-NotContains -Text $live2DSource -Unexpected 'GetRenderTextureCount()' `
    -Message '11_Live2D still queries a mask renderer that is absent for no-mask models.'
Assert-NotContains -Text $live2DSource -Unexpected 'GetMaskBuffer(' `
    -Message '11_Live2D still creates Cubism mask surfaces manually.'
```

- [ ] **Step 3: Characterize the already-correct default loaders in projects 17-21, 23, and 24**

Add a loop that reads each `App.cpp` and requires the exact bundled player call:

```powershell
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
```

These checks are characterization tests and should already pass; do not add duplicate production calls.

- [ ] **Step 4: Add project 25 external-Idle contracts**

Require the library, tracked Idle path, coordinate conversion, startup-only opt-in, and animator injection:

```powershell
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
```

- [ ] **Step 5: Add final scale contracts for projects 26-35**

Use exact snippets so the test covers only characters, not ground or PBR spheres:

```powershell
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

foreach ($project in $characterScaleContracts.Keys) {
    $source = Read-RepoText -RelativePath "Dx11/$project/App.cpp"
    foreach ($expectedScale in $characterScaleContracts[$project]) {
        Assert-Contains -Text $source -Expected $expectedScale `
            -Message "$project is missing character scale contract: $expectedScale"
    }
}
```

- [ ] **Step 6: Add a project 36 balanced-window contract**

```powershell
$advancedPanels = Read-RepoText -RelativePath 'Dx11/36_AdvancedAnim_Sound_Click/App_ImGuiPanels.inl'
Assert-Contains -Text $advancedPanels `
    -Expected 'const bool showSoundDebug = ImGui::Begin("Sound Debug");' `
    -Message '36_AdvancedAnim_Sound_Click does not record the Sound Debug Begin result.'
Assert-Contains -Text $advancedPanels -Expected 'if (showSoundDebug)' `
    -Message '36_AdvancedAnim_Sound_Click does not conditionally draw Sound Debug contents.'
Assert-NotContains -Text $advancedPanels -Unexpected 'if (ImGui::Begin("Sound Debug"))' `
    -Message '36_AdvancedAnim_Sound_Click still has the unsafe inline Begin/End pattern.'
```

- [ ] **Step 7: Run the verifier and confirm the RED state**

Run:

```powershell
pwsh -NoProfile -File .\tools\tests\test_portable_runtime.ps1
```

Expected: exit code `1`. The existing 17-21/23/24 checks pass, while failures mention project 11 mask access, project 25 Idle, project 26-35 final scales, and project 36 Sound Debug. If any unrelated Assimp or Live2D asset contract fails, stop and diagnose that regression before continuing.

- [ ] **Step 8: Commit the contracts**

```powershell
git add tools/tests/test_portable_runtime.ps1
git diff --cached --check
git commit -m "test: capture F5 default scene QA contracts"
```

---

### Task 2: Let Cubism own project 11 mask surfaces

**Files:**

- Modify: `Dx11/11_Live2D/App.cpp`
- Test: `tools/tests/test_portable_runtime.ps1`

- [ ] **Step 1: Remove the unsafe manual mask setup**

In `App::LoadLive2DModel`, replace the renderer block that calls `SetClippingMaskBufferSize`, `GetRenderTextureCount`, `GetMaskBuffer`, and `CreateOffscreenSurface` with:

```cpp
if (auto* renderer = m_L2D->GetRenderer<Rendering::CubismRenderer_D3D11>())
{
    renderer->UseHighPrecisionMask(false);
}
```

Do not add a model-side mask test. `CubismRenderer_D3D11::Initialize` already allocates the clipping manager and its surfaces only when the model uses masking.

- [ ] **Step 2: Run the focused verifier**

```powershell
pwsh -NoProfile -File .\tools\tests\test_portable_runtime.ps1
```

Expected: project 11 mask failures disappear. The command still exits `1` because tasks 3-6 are intentionally incomplete.

- [ ] **Step 3: Build project 11 in both configurations**

Resolve MSBuild once in the shell, then build:

```powershell
$msbuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
& $msbuild .\Dx11\11_Live2D\11_Live2D.vcxproj /m /p:Configuration=Debug /p:Platform=x64
& $msbuild .\Dx11\11_Live2D\11_Live2D.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

Expected: both commands exit `0` with `0 Error(s)`.

- [ ] **Step 4: Commit**

```powershell
git add Dx11/11_Live2D/App.cpp
git diff --cached --check
git commit -m "fix: let Cubism manage Live2D mask surfaces"
```

---

### Task 3: Give project 25 a real external Idle default

**Files:**

- Modify: `Dx11/Common/Mesh/FbxAnimation.h`
- Modify: `Dx11/Common/Mesh/FbxAnimation.cpp`
- Modify: `Dx11/25_ToonShading_Outline/App.h`
- Modify: `Dx11/25_ToonShading_Outline/App.cpp`
- Test: `tools/tests/test_portable_runtime.ps1`

- [ ] **Step 1: Add the non-owning external clip interface**

Forward-declare `struct aiAnimation;` in `FbxAnimation.h`. Add this public method beside `InitMetadata`:

```cpp
void SetExternalClip(const aiAnimation* clip, const std::string& name);
```

Add the active clip storage beside metadata vectors:

```cpp
std::vector<const aiAnimation*> m_Clips;
```

Document in the header that the caller owns each pointer and must keep it alive while the animator uses it.

- [ ] **Step 2: Make embedded and external clips share one metadata path**

In `FbxAnimation.cpp`:

1. Clear `m_Clips` in `Clear()`.
2. Have `InitMetadata(const aiScene*)` populate `m_Clips` from each non-null `scene->mAnimations[i]`, then populate `m_Names`, `m_TicksPerSec`, and `m_DurationSec` from `m_Clips`.
3. Implement `SetExternalClip` as a one-clip replacement:

```cpp
void FbxAnimation::SetExternalClip(const aiAnimation* clip, const std::string& name)
{
    m_Clips.clear();
    m_Names.clear();
    m_DurationSec.clear();
    m_TicksPerSec.clear();
    m_Precomputed.clear();
    m_Current = -1;
    m_TimeSec = 0.0;
    m_ChannelDirty = true;

    if (!clip) return;

    const double ticksPerSec = clip->mTicksPerSecond != 0.0 ? clip->mTicksPerSecond : 25.0;
    m_Clips.push_back(clip);
    m_Names.push_back(name.empty() ? "External" : name);
    m_TicksPerSec.push_back(ticksPerSec);
    m_DurationSec.push_back(ticksPerSec != 0.0 ? clip->mDuration / ticksPerSec : 0.0);
    m_Current = 0;

    if (m_Scene && m_BoneNames && m_BoneOffsets && m_GlobalInverse)
    {
        PrecomputeAll(m_Scene, m_NodeIndexOfName, *m_BoneNames, *m_BoneOffsets, *m_GlobalInverse, 30);
    }
}
```

- [ ] **Step 3: Route channel mapping and precomputation through `m_Clips`**

Change the helper to accept the selected clip directly:

```cpp
static void RebuildChannelMapIfNeeded(
    const aiAnimation* animation,
    const std::unordered_map<std::string, int>& nodeIndexOfName,
    std::vector<const aiNodeAnim*>& out)
```

The helper fills `out` from `animation->mChannels`. In `PrecomputeAll`, size `m_Precomputed` from `m_Clips.size()` and use `m_Clips[clipIdx]`. In the on-the-fly fallback inside `UpdateAndUpload`, pass `m_Clips[m_Current]` after bounds checking. Continue passing the model `aiScene` to hierarchy evaluation, because bind pose and node traversal still come from the GLB.

- [ ] **Step 4: Make project 25 distinguish startup models from file-picker models**

In `App.h`, change the loader declaration to:

```cpp
bool LoadModelFromFile(const std::wstring& pathW, bool useDefaultIdle = false);
```

In `App.cpp`:

- Include `../Common/Animation/ExternalAnimationClipLibrary.h`.
- Add `bool useDefaultIdle = false;` to `ModelEntry`.
- Add these fields to `App::Impl` near the model cache:

```cpp
ExternalAnimationClipLibrary m_ExternalAnimations;
const aiAnimation* m_DefaultIdleClip = nullptr;
```

- Update the loader definition to take `bool useDefaultIdle` and copy it to the new `ModelEntry` field.
- Leave file-picker calls unchanged so the default argument remains `false`.

- [ ] **Step 5: Load Idle before creating the 36 startup models**

At the start of the model section in `OnInitialize`, add constants and fail with the exact importer error when the required clip cannot load:

```cpp
constexpr const wchar_t* kDefaultModelPath = L"..\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb";
constexpr const wchar_t* kDefaultIdlePath = L"..\\Resource\\fbx\\Public\\MyAlice\\Animations\\anim_Idle.fbx";

std::string idleError;
if (!m_->m_ExternalAnimations.LoadClip(
        "Idle",
        kDefaultIdlePath,
        &idleError,
        ExternalAnimationClipTransform::UnrealCmZUpToGlbMeters))
{
    m_->PushLog("[ERR] Failed to load default Idle animation: " + idleError);
    return false;
}
m_->m_DefaultIdleClip = m_->m_ExternalAnimations.Get("Idle");
if (!m_->m_DefaultIdleClip)
{
    m_->PushLog("[ERR] Default Idle animation was loaded without a clip.");
    return false;
}
```

Change the 36-instance loop call to `LoadModelFromFile(kDefaultModelPath, true)` and return `false` immediately if any load fails.

- [ ] **Step 6: Attach and play Idle after animator type initialization**

In both animator initialization paths—the eager path in `LoadModelFromFile` and the lazy fallback in `OnUpdate`—run this after `SetType(...)`:

```cpp
if (entry->useDefaultIdle && m_->m_DefaultIdleClip)
{
    entry->animator.SetExternalClip(m_->m_DefaultIdleClip, "Idle");
    entry->animator.SetCurrentIndex(0);
    entry->uiAnimPlaying = true;
}
```

Use `mdl` instead of `entry` in the lazy path. The call must occur after `SetType` so the recomputed palette uses the correct skinned/rigid mode.

- [ ] **Step 7: Remove the invalid row-specific embedded animation selection**

Replace the 36 repeated `SetCurrentIndex(0/1/2)` assignments with one bounded loop that only starts the already-attached Idle clip:

```cpp
for (int i = 0; i < kToonSampleModelCount; ++i)
{
    m_->m_Models[(size_t)i]->animator.SetCurrentIndex(0);
    m_->m_Models[(size_t)i]->uiAnimPlaying = true;
}
```

Update the surrounding comment so the three rows are described as material/outline comparisons, not animation-index comparisons.

- [ ] **Step 8: Run the verifier and build Common plus project 25**

```powershell
pwsh -NoProfile -File .\tools\tests\test_portable_runtime.ps1
& $msbuild .\Dx11\Common\Common.vcxproj /m /p:Configuration=Debug /p:Platform=x64
& $msbuild .\Dx11\25_ToonShading_Outline\25_ToonShading_Outline.vcxproj /m /p:Configuration=Debug /p:Platform=x64
& $msbuild .\Dx11\25_ToonShading_Outline\25_ToonShading_Outline.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

Expected: the project 25 verifier failures disappear; scale/project 36 failures remain until later tasks. All three builds exit `0` with `0 Error(s)`.

- [ ] **Step 9: Commit**

```powershell
git add Dx11/Common/Mesh/FbxAnimation.h Dx11/Common/Mesh/FbxAnimation.cpp Dx11/25_ToonShading_Outline/App.h Dx11/25_ToonShading_Outline/App.cpp
git diff --cached --check
git commit -m "fix: play external Idle in toon sample"
```

---

### Task 4: Normalize character scale in projects 26-30

**Files:**

- Modify: `Dx11/26_ShadowMap_PCF/App.cpp`
- Modify: `Dx11/27_DebugDraw/App.cpp`
- Modify: `Dx11/28_Scene_Shared3DModel_Animation/App.cpp`
- Modify: `Dx11/29_MousePicking/App.cpp`
- Modify: `Dx11/30_PBR_BRDF/App.cpp`
- Test: `tools/tests/test_portable_runtime.ps1`

- [ ] **Step 1: Update startup character scales only**

Make these exact replacements in the existing post-load framing blocks:

- Projects 26 and 27: `characterScale(95.0f, 95.0f, 95.0f)` becomes `characterScale(100.0f, 100.0f, 100.0f)`; models 0-7 receive it, model 8 ground does not.
- Projects 28 and 29: `player.scale = XMFLOAT3(80.0f, 80.0f, 80.0f)` becomes `100.0f` on all axes; model 1 ground stays unchanged.
- Project 30: player model 0 and enemy model 1 become `100.0f` on all axes; ground models 2 and 3 stay unchanged.

- [ ] **Step 2: Update runtime spawn scales**

In projects 28, 29, and 30, replace the spawned character assignment:

```cpp
mdl.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
```

with:

```cpp
mdl.scale = XMFLOAT3(100.0f, 100.0f, 100.0f);
```

Do not change spawn positions, row spacing, animation choice, or material randomization.

- [ ] **Step 3: Run the verifier**

```powershell
pwsh -NoProfile -File .\tools\tests\test_portable_runtime.ps1
```

Expected: scale failures for projects 26-30 disappear; projects 31-35 and project 36 remain red.

- [ ] **Step 4: Build the five Debug projects**

```powershell
foreach ($project in @('26_ShadowMap_PCF','27_DebugDraw','28_Scene_Shared3DModel_Animation','29_MousePicking','30_PBR_BRDF')) {
    & $msbuild ".\Dx11\$project\$project.vcxproj" /m /p:Configuration=Debug /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { throw "$project Debug build failed" }
}
```

Expected: each build exits `0`.

- [ ] **Step 5: Commit**

```powershell
git add Dx11/26_ShadowMap_PCF/App.cpp Dx11/27_DebugDraw/App.cpp Dx11/28_Scene_Shared3DModel_Animation/App.cpp Dx11/29_MousePicking/App.cpp Dx11/30_PBR_BRDF/App.cpp
git diff --cached --check
git commit -m "fix: scale default characters in projects 26 to 30"
```

---

### Task 5: Normalize character scale in projects 31-35

**Files:**

- Modify: `Dx11/31_IBL/App.cpp`
- Modify: `Dx11/32_Sound_FMOD/App.cpp`
- Modify: `Dx11/33_Sound_Animation_Camera_Motion/App.cpp`
- Modify: `Dx11/34_ToneMapping/App.cpp`
- Modify: `Dx11/35_DeferredRendering/App.cpp`
- Test: `tools/tests/test_portable_runtime.ps1`

- [ ] **Step 1: Apply the approved character-only scale conversion**

In the existing post-load framing blocks:

- Projects 31, 32, and 33: set `player.scale` to `(100,100,100)` and the intentionally half-size `enemy.scale` to `(50,50,50)`.
- Projects 34 and 35: set `player.scale` to `(100,100,100)`.
- Leave PBR sphere scales around `0.45`, ground scales around `(1.5,1,5)`, their transforms, and all cameras unchanged.

- [ ] **Step 2: Run the verifier**

```powershell
pwsh -NoProfile -File .\tools\tests\test_portable_runtime.ps1
```

Expected: all project 26-35 scale failures disappear. Only project 36 remains red if previous tasks passed.

- [ ] **Step 3: Build the five Debug projects**

```powershell
foreach ($project in @('31_IBL','32_Sound_FMOD','33_Sound_Animation_Camera_Motion','34_ToneMapping','35_DeferredRendering')) {
    & $msbuild ".\Dx11\$project\$project.vcxproj" /m /p:Configuration=Debug /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { throw "$project Debug build failed" }
}
```

Expected: each build exits `0`.

- [ ] **Step 4: Commit**

```powershell
git add Dx11/31_IBL/App.cpp Dx11/32_Sound_FMOD/App.cpp Dx11/33_Sound_Animation_Camera_Motion/App.cpp Dx11/34_ToneMapping/App.cpp Dx11/35_DeferredRendering/App.cpp
git diff --cached --check
git commit -m "fix: scale default characters in projects 31 to 35"
```

---

### Task 6: Balance project 36 Sound Debug Begin/End

**Files:**

- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_ImGuiPanels.inl`
- Test: `tools/tests/test_portable_runtime.ps1`

- [ ] **Step 1: Reproduce the static lifecycle failure**

Confirm the existing source contains `if (ImGui::Begin("Sound Debug"))` and places `ImGui::End()` inside that conditional. This is the direct cause of the attached `imgui.cpp` line 11048 `Missing End()` assertion when the window is collapsed.

- [ ] **Step 2: Balance every Begin call**

Replace the existing opening line with these lines:

```cpp
const bool showSoundDebug = ImGui::Begin("Sound Debug");
if (showSoundDebug)
{
```

Keep the complete existing widget body after that opening brace. Move the existing `ImGui::End();` past the brace that closes `if (showSoundDebug)`, producing this exact tail:

```cpp
}
ImGui::End();
```

Do not move, remove, or alter any debug control statement.

- [ ] **Step 3: Run the full static verifier**

```powershell
pwsh -NoProfile -File .\tools\tests\test_portable_runtime.ps1
```

Expected: exit code `0` and final line `Portable runtime verification passed.`

- [ ] **Step 4: Build project 36 in both configurations**

```powershell
& $msbuild .\Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj /m /p:Configuration=Debug /p:Platform=x64
& $msbuild .\Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

Expected: both commands exit `0` with `0 Error(s)`.

- [ ] **Step 5: Commit**

```powershell
git add Dx11/36_AdvancedAnim_Sound_Click/App_ImGuiPanels.inl
git diff --cached --check
git commit -m "fix: balance Sound Debug ImGui window"
```

---

### Task 7: Final Debug/Release verification and delivery

**Files:**

- Verify: `Dx11/TutorialApp.sln`
- Verify: `tools/tests/test_portable_runtime.ps1`
- Review: all files changed since design commit `0887494`

- [ ] **Step 1: Run the portable/default-scene verifier from a clean shell**

```powershell
pwsh -NoProfile -File .\tools\tests\test_portable_runtime.ps1
```

Expected: exit code `0` and `Portable runtime verification passed.`

- [ ] **Step 2: Build the full solution in x64 Debug**

```powershell
& $msbuild .\Dx11\TutorialApp.sln /m /p:Configuration=Debug /p:Platform=x64
```

Expected: exit code `0` and `0 Error(s)`.

- [ ] **Step 3: Build the full solution in x64 Release**

```powershell
& $msbuild .\Dx11\TutorialApp.sln /m /p:Configuration=Release /p:Platform=x64
```

Expected: exit code `0` and `0 Error(s)`.

If an unrelated tutorial prevents a full-solution build, record the exact pre-existing error, then individually build projects 11, 17-21, 23-36 in both configurations. Do not label an affected-project error as unrelated.

- [ ] **Step 4: Review scope and repository cleanliness**

```powershell
git diff 0887494..HEAD --check
git diff 0887494..HEAD --stat
git status --short --branch
```

Expected: no whitespace errors, only the planned verifier/common/project files changed, and no generated `.obj`, `.pdb`, `.ilk`, or untracked runtime files are staged.

- [ ] **Step 5: Push the completed commit series**

```powershell
git push origin main
```

Expected: `main` is updated on `origin` without a non-fast-forward error. Report the pushed commit range, verifier result, build result, and explicitly state that final F5 visual verification remains for the user's other computer.
