# Bind Pose Without Animation Clips Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make project 18 render a skinned GLB in bind pose when the file has a skeleton but no animation clips.

**Architecture:** Keep palette generation inside `FbxManager::UpdateAnimation()`. Rigid models retain their existing branch; static models return; skinned models evaluate either the selected animation channels or an empty channel map representing the imported bind pose, then upload `GlobalInverse * BoneGlobal * InverseBind`.

**Tech Stack:** C++17, Direct3D 11, Assimp, PowerShell contract tests, MSBuild/Visual Studio 2022

## Global Constraints

- Do not modify either `SampleModel.glb` file or `Dx11/Common/Camera.cpp`.
- Preserve animated-skinned and rigid-animation behavior.
- Do not use an identity palette or a static-shader fallback for skinned models.
- Keep load failure non-fatal.
- Production changes for this plan are limited to `Dx11/Common/FbxManager.cpp`.

---

### Task 1: Bind-pose palette regression

**Files:**
- Create: `tools/tests/test_bind_pose_without_animation.ps1`
- Modify: `Dx11/Common/FbxManager.cpp:801-837`

**Interfaces:**
- Consumes: `FbxManager::AnimationType`, `EvaluateGlobalMatrices(...)`, `BuildBonePalette(...)`, and `UploadBonePalette(...)`.
- Produces: `FbxManager::UpdateAnimation(ID3D11DeviceContext*, double)` behavior that uploads a bind-pose palette for `AnimationType::Skinned` even when `HasAnimations == false`.

- [ ] **Step 1: Write the failing source contract**

Create `tools/tests/test_bind_pose_without_animation.ps1` with this contract:

```powershell
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Get-BracedBlock([string]$Text, [int]$Anchor, [string]$Label) {
    $open = $Text.IndexOf('{', $Anchor)
    Assert-True ($open -ge 0) "$Label opening brace missing"
    $depth = 0
    for ($i = $open; $i -lt $Text.Length; ++$i) {
        if ($Text[$i] -eq '{') { ++$depth }
        elseif ($Text[$i] -eq '}') {
            --$depth
            if ($depth -eq 0) { return $Text.Substring($Anchor, $i - $Anchor + 1) }
        }
    }
    throw "$Label closing brace missing"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourcePath = Join-Path $repoRoot 'Dx11\Common\FbxManager.cpp'
$source = [IO.File]::ReadAllText($sourcePath)
$anchor = $source.IndexOf('void FbxManager::UpdateAnimation(', [StringComparison]::Ordinal)
Assert-True ($anchor -ge 0) 'FbxManager::UpdateAnimation missing'
$body = Get-BracedBlock $source $anchor 'FbxManager::UpdateAnimation'

Assert-True ($body -match 'if\s*\(\s*m_CurrentType\s*!=\s*AnimationType::Skinned\s*\)\s*return\s*;') `
    'static FBX models must return before skinning palette evaluation'
Assert-True ($body -notmatch 'if\s*\(\s*!m_->HasAnimations\s*\|\|\s*m_->CurrentClip\s*<\s*0\s*\)\s*return\s*;') `
    'skinned models without clips must not return before bind-pose evaluation'
Assert-True ($body -match 'const\s+aiAnimation\s*\*\s*anim\s*=.*m_->HasAnimations.*nullptr') `
    'animation channel selection must allow a null clip'
Assert-True ($body -match 'EvaluateGlobalMatrices\s*\(\s*scene\s*,\s*channelOf\s*,\s*global\s*\)') `
    'bind-pose hierarchy evaluation missing'
Assert-True ($body -match 'BuildBonePalette\s*\(\s*global\s*,\s*palette\s*\)') `
    'bind-pose palette construction missing'
Assert-True ($body -match 'UploadBonePalette\s*\(\s*ctx\s*,\s*palette\s*\)') `
    'bind-pose palette upload missing'

'bind pose without animation contract tests passed'
```

- [ ] **Step 2: Run the contract and verify RED**

Run:

```powershell
pwsh -NoProfile -File .\tools\tests\test_bind_pose_without_animation.ps1
```

Expected: FAIL with `static FBX models must return before skinning palette evaluation` or `skinned models without clips must not return before bind-pose evaluation` because the current implementation exits solely on missing clips.

- [ ] **Step 3: Make the minimal production change**

In `FbxManager::UpdateAnimation()`, preserve the rigid block and replace the clip-required guard with a model-type guard. The resulting control flow must be:

```cpp
if (m_CurrentType == AnimationType::Rigid)
{
    UploadRigidNodePalette(ctx);
    return;
}

if (m_CurrentType != AnimationType::Skinned) return;

const aiScene* scene = reinterpret_cast<const aiScene*>(m_->SceneMutable);
if (!scene) return;

const aiAnimation* anim =
    (m_->HasAnimations && m_->CurrentClip >= 0)
    ? scene->mAnimations[m_->CurrentClip]
    : nullptr;
```

Leave the existing empty `channelOf` map, hierarchy evaluation, palette construction, and upload below this block unchanged. With `anim == nullptr`, the map stays empty and `EvaluateGlobalMatrices()` uses imported node transforms.

- [ ] **Step 4: Run the focused test and existing contracts**

Run:

```powershell
pwsh -NoProfile -File .\tools\tests\test_bind_pose_without_animation.ps1
pwsh -NoProfile -File .\tools\tests\test_default_character_startup.ps1
pwsh -NoProfile -File .\tools\tests\test_visual_capture_contracts.ps1
```

Expected: all three commands exit 0. Existing compiler warnings are not part of these script outputs.

- [ ] **Step 5: Build projects 18 and 23**

Run:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  '.\Dx11\TutorialApp.sln' `
  '/t:18_fbx_Animation;23_Rigid_Animation' `
  '/p:Configuration=Debug;Platform=x64' /m /v:minimal /nologo
```

Expected: exit 0 and both executables are produced. Record, but do not broaden scope to fix, pre-existing warnings.

- [ ] **Step 6: Commit the bind-pose fix**

```powershell
git add -- tools/tests/test_bind_pose_without_animation.ps1 Dx11/Common/FbxManager.cpp
git diff --cached --check
git commit -m "fix: render skinned bind pose without animation clips"
```

### Task 2: Runtime verification with the user's no-clip model

**Files:**
- Verify only: `Dx11/x64/Debug/18_fbx_Animation.exe`
- Verify only: main-checkout `Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb`

**Interfaces:**
- Consumes: the Task 1 executable and the user's modified no-animation `SampleModel.glb` in the main checkout.
- Produces: visual evidence that project 18 renders the model without requiring an animation clip.

- [ ] **Step 1: Reconfirm the main-checkout model shape**

Read the GLB JSON chunk without editing the file and confirm `skins >= 1` and `animations == 0`. Record the SHA-256 hash before launching.

- [ ] **Step 2: Launch the worktree executable against main-checkout resources**

Use the worktree-built executable but set the working directory to the main checkout's `Dx11/18_fbx_Animation` directory. This causes `..\Resource` to resolve to the user's unchanged main-checkout model without copying it into the worktree.

```powershell
Start-Process `
  -FilePath 'C:\Github\D3D11-AliceTutorial\.worktrees\fix-bind-pose-without-animation\Dx11\x64\Debug\18_fbx_Animation.exe' `
  -WorkingDirectory 'C:\Github\D3D11-AliceTutorial\Dx11\18_fbx_Animation'
```

- [ ] **Step 3: Verify the visible result**

Confirm the character is visible at position `(0,0,0)` and the application remains responsive. If automated Direct3D capture is blank, use the visible application window and request one user confirmation rather than changing production code based on capture failure.

- [ ] **Step 4: Recheck the protected model hash**

Confirm the SHA-256 hash matches Step 1 and `git status --short` in the main checkout still lists the user's pre-existing `Camera.cpp`, `SampleModel.glb`, `SampleModel2.glb`, and `.superpowers/` state only.
