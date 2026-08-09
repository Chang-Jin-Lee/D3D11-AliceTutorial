# Project 23 Rigid Startup and Korean Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore project 23's Korean text as UTF-8 and make it start with an automatically playing `BoxHuman.fbx` rigid animation.

**Architecture:** Treat encoding recovery and startup behavior as separate testable changes. Repair text from readable history while preserving the current non-comment token stream, then change only `App::OnInitialize()` for the default rigid scene; keep `FbxManager` as the classifier and animation engine.

**Tech Stack:** C++17, HLSL, Direct3D 11, Assimp, PowerShell contract tests, Git history, MSBuild/Visual Studio 2022

## Global Constraints

- Limit Korean recovery to `Dx11/23_Rigid_Animation`.
- Store repaired text as strict UTF-8 without replacement characters or mojibake.
- Preserve all current executable C++ and HLSL behavior except the approved `App::OnInitialize()` startup block.
- Load `..\Resource\fbx\Study\BoxHuman.fbx` at `(0,0,0)`, scale 1, rotation 0, with whole-model auto-rotation disabled.
- Start playback only after a successful rigid FBX load with an embedded animation.
- Use project-local camera position `(0,0,-8)` and zero rotation in normal and README-capture modes.
- Do not modify `BoxHuman.fbx`, the user's GLB files, or `Dx11/Common/Camera.cpp`.

---

### Task 1: UTF-8 recovery contract and project text repair

**Files:**
- Create: `tools/tests/test_project23_utf8.ps1`
- Modify: `Dx11/23_Rigid_Animation/App.cpp`
- Modify: `Dx11/23_Rigid_Animation/App.h`
- Modify: `Dx11/23_Rigid_Animation/WinMain.cpp`
- Modify: `Dx11/23_Rigid_Animation/23_BasicPS.hlsl`
- Modify: `Dx11/23_Rigid_Animation/23_BasicVS.hlsl`
- Modify: `Dx11/23_Rigid_Animation/23_LightingHelper.hlsli`
- Modify: `Dx11/23_Rigid_Animation/23_Shared.fxh`
- Modify: `Dx11/23_Rigid_Animation/23_SkyBoxVS.hlsl`
- Modify: `Dx11/23_Rigid_Animation/README.md`

**Interfaces:**
- Consumes: readable historical `App.cpp` at commit `3015652` and readable README lesson content at commit `123da1a`.
- Produces: strict UTF-8 project 23 text with current executable tokens preserved and readable Korean tutorial comments.

- [ ] **Step 1: Write the failing UTF-8 contract**

Create `tools/tests/test_project23_utf8.ps1`:

```powershell
$ErrorActionPreference = 'Stop'

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$relativeFiles = @(
    'Dx11\23_Rigid_Animation\App.cpp',
    'Dx11\23_Rigid_Animation\App.h',
    'Dx11\23_Rigid_Animation\WinMain.cpp',
    'Dx11\23_Rigid_Animation\23_BasicPS.hlsl',
    'Dx11\23_Rigid_Animation\23_BasicVS.hlsl',
    'Dx11\23_Rigid_Animation\23_LightingHelper.hlsli',
    'Dx11\23_Rigid_Animation\23_Shared.fxh',
    'Dx11\23_Rigid_Animation\23_SkyBoxVS.hlsl',
    'Dx11\23_Rigid_Animation\README.md'
)
$strictUtf8 = [Text.UTF8Encoding]::new($false, $true)
$mojibake = '(?:\uFFFD|Ã|Â|ï§|æ|ë‚|ë©|ì—|ìž|\?��|\?ë|\?ì)'

foreach ($relative in $relativeFiles) {
    $path = Join-Path $repoRoot $relative
    Assert-True (Test-Path -LiteralPath $path -PathType Leaf) "$relative missing"
    $bytes = [IO.File]::ReadAllBytes($path)
    try { $text = $strictUtf8.GetString($bytes) }
    catch { throw "$relative is not strict UTF-8: $($_.Exception.Message)" }
    Assert-True ($text -notmatch $mojibake) "$relative still contains mojibake"
}

$readme = [IO.File]::ReadAllText((Join-Path $repoRoot 'Dx11\23_Rigid_Animation\README.md'), $strictUtf8)
Assert-True ($readme -match '리지드 애니메이션|Rigid Animation') 'README rigid-animation explanation missing'
Assert-True ($readme -match '본의 개수') 'README Korean lesson text missing'

'project 23 UTF-8 contract tests passed'
```

- [ ] **Step 2: Run the contract and verify RED**

```powershell
pwsh -NoProfile -File .\tools\tests\test_project23_utf8.ps1
```

Expected: FAIL first on `App.cpp is not strict UTF-8`; after individual transcoding starts, it must continue failing until all listed files and README mojibake are repaired.

- [ ] **Step 3: Capture the pre-repair executable-token baseline**

Before editing, retain a review copy of the current `App.cpp` and shader blobs through Git object IDs, not workspace backup files:

```powershell
git rev-parse HEAD:Dx11/23_Rigid_Animation/App.cpp
git rev-parse HEAD:Dx11/23_Rigid_Animation/23_BasicVS.hlsl
git rev-parse HEAD:Dx11/23_Rigid_Animation/23_BasicPS.hlsl
```

During review, compare comment-stripped token streams before and after. Differences are allowed only in the later Task 2 `OnInitialize()` block.

- [ ] **Step 4: Recover `App.cpp` Korean without reverting current code**

Use the readable historical blob as the comment reference:

```powershell
git show 30156521088619a00237a33329f0927626b5c497:Dx11/23_Rigid_Animation/App.cpp
```

Patch the current file section-by-section. For matching code regions, restore the historical Korean comments. For comments introduced after that revision, replace damaged text with concise Korean describing the current code. Remove no executable statement and retain current includes, loader logic, capture integration, UI, and renderer state management.

Key clean section comments to preserve include:

```cpp
// ImGui에서 보여주기 위한 애니메이션 선택 및 재생 상태
// 현재 로드된 모든 모델의 변환과 애니메이션을 갱신합니다.
// FBX 애니메이션 또는 리지드 노드 팔레트를 갱신합니다.
// 모델 로더: FBX, OBJ, PMX 파일을 새 항목으로 추가합니다.
```

- [ ] **Step 5: Transcode the remaining CP949 files and repair README**

For files whose content is clean under code page 949, decode with `Encoding.GetEncoding(949)` and write UTF-8 without BOM as one mechanical conversion. For README, restore the lesson body from:

```powershell
git show 123da1a402583e740db68caa9ddc560233d485e6:23_Rigid_Animation/README.md
```

Retain the current README branding block, previous/main/next navigation, gallery images, and GIF. The repaired lesson must explicitly explain:

```markdown
- 본의 개수 > 0, 애니메이션 개수 > 0이면 Skinned Animation입니다.
- 본의 개수 == 0, 애니메이션 개수 > 0이면 Rigid Animation입니다.
- 기본 실행 예제는 `../Resource/fbx/Study/BoxHuman.fbx`의 리지드 애니메이션을 자동 재생합니다.
```

- [ ] **Step 6: Verify GREEN and review semantic preservation**

```powershell
pwsh -NoProfile -File .\tools\tests\test_project23_utf8.ps1
git diff --check
git diff --word-diff=porcelain -- Dx11/23_Rigid_Animation
```

Expected: UTF-8 contract passes, no whitespace errors, and the diff shows text/comment recovery only. If executable tokens changed, restore them before continuing.

- [ ] **Step 7: Build project 23 after encoding recovery**

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  '.\Dx11\23_Rigid_Animation\23_Rigid_Animation.vcxproj' `
  /t:Build '/p:Configuration=Debug;Platform=x64' /m /v:minimal /nologo
```

Expected: exit 0. Record pre-existing warnings; an encoding conversion must not introduce new compile or shader errors.

- [ ] **Step 8: Commit the Korean recovery**

```powershell
git add -- tools/tests/test_project23_utf8.ps1 Dx11/23_Rigid_Animation
git diff --cached --check
git commit -m "fix: restore project 23 Korean text"
```

### Task 2: BoxHuman rigid startup regression and implementation

**Files:**
- Create: `tools/tests/test_project23_rigid_startup.ps1`
- Modify: `tools/tests/test_default_character_startup.ps1:33-61`
- Modify: `tools/tests/test_portable_runtime.ps1:233-248`
- Modify: `Dx11/23_Rigid_Animation/App.cpp:299-327`
- Modify: `Dx11/23_Rigid_Animation/README.md`

**Interfaces:**
- Consumes: `FbxManager::GetCurrentAnimationType()`, `FbxManager::HasAnimations()`, and `FbxManager::SetAnimationPlaying(bool)`.
- Produces: a project 23 startup scene containing one `BoxHuman.fbx` entry at the origin with rigid playback active.

- [ ] **Step 1: Remove project 23 from the generic SampleModel contract**

In `test_default_character_startup.ps1`, remove `23_Rigid_Animation` from `$multiProjects` and `$expectedProjects`, then change the exact target count from 7 to 6 and its message accordingly.

In `test_portable_runtime.ps1`, remove `23_Rigid_Animation` from `$defaultModelProjects`. Project 23 will have its own fixture-specific contract.

- [ ] **Step 2: Write the failing rigid-startup contract**

Create `tools/tests/test_project23_rigid_startup.ps1`:

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
$sourcePath = Join-Path $repoRoot 'Dx11\23_Rigid_Animation\App.cpp'
$source = [IO.File]::ReadAllText($sourcePath, [Text.Encoding]::UTF8)
$anchor = $source.IndexOf('bool App::OnInitialize()', [StringComparison]::Ordinal)
Assert-True ($anchor -ge 0) 'project 23 OnInitialize missing'
$body = Get-BracedBlock $source $anchor 'project 23 OnInitialize'

$load = 'LoadModelFromFile(L"..\\Resource\\fbx\\Study\\BoxHuman.fbx")'
Assert-True ([regex]::Matches($body, [regex]::Escape($load)).Count -eq 1) `
    'project 23 must load BoxHuman.fbx exactly once'
Assert-True ($body -notmatch 'SampleModel\.glb') 'project 23 must not load SampleModel.glb at startup'
Assert-True ($body -match 'model\.pos\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\)') 'origin missing'
Assert-True ($body -match 'model\.rotDeg\s*=\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\)') 'zero rotation missing'
Assert-True ($body -match 'model\.scale\s*=\s*XMFLOAT3\(\s*1\.0f\s*,\s*1\.0f\s*,\s*1\.0f\s*\)') 'unit scale missing'
Assert-True ($body -match 'model\.autoRotate\s*=\s*false') 'whole-model auto rotation must be disabled'
Assert-True ($body -match 'GetCurrentAnimationType\(\)\s*==\s*FbxManager::AnimationType::Rigid') 'rigid classification guard missing'
Assert-True ($body -match 'model\.fbx\.HasAnimations\(\)') 'embedded-animation guard missing'
Assert-True ($body -match 'model\.uiAnimPlaying\s*=\s*true') 'UI playback state missing'
Assert-True ($body -match 'model\.fbx\.SetAnimationPlaying\(\s*true\s*\)') 'FbxManager playback state missing'
Assert-True ($body -match 'm_Camera\.SetPosition\(\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*-8\.0f\s*\)\s*\)') 'project camera position missing'
Assert-True ($body -match 'm_Camera\.SetRotation\(\s*XMFLOAT3\(\s*0\.0f\s*,\s*0\.0f\s*,\s*0\.0f\s*\)\s*\)') 'project camera rotation missing'

'project 23 rigid startup contract tests passed'
```

- [ ] **Step 3: Run startup contracts and verify RED**

```powershell
pwsh -NoProfile -File .\tools\tests\test_project23_rigid_startup.ps1
pwsh -NoProfile -File .\tools\tests\test_default_character_startup.ps1
pwsh -NoProfile -File .\tools\tests\test_portable_runtime.ps1
```

Expected: the new rigid test fails because `SampleModel.glb` is still loaded. The two adjusted generic contracts pass after project 23 is removed from their SampleModel lists.

- [ ] **Step 4: Implement the startup scene**

Replace project 23's current default model block with:

```cpp
if (LoadModelFromFile(L"..\\Resource\\fbx\\Study\\BoxHuman.fbx") && !m_->m_Models.empty())
{
    auto& model = *m_->m_Models.back();
    model.pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
    model.rotDeg = XMFLOAT3(0.0f, 0.0f, 0.0f);
    model.scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
    model.autoRotate = false;

    m_Camera.SetPosition(XMFLOAT3(0.0f, 0.0f, -8.0f));
    m_Camera.SetRotation(XMFLOAT3(0.0f, 0.0f, 0.0f));

    if (model.source == ModelSource::FBX &&
        model.fbx.GetCurrentAnimationType() == FbxManager::AnimationType::Rigid &&
        model.fbx.HasAnimations())
    {
        model.uiAnimPlaying = true;
        model.fbx.SetAnimationPlaying(true);
    }
}
```

Do not add a `ReadmeCapture::IsEnabled()` model substitution. Both normal and capture modes use BoxHuman and active rigid playback.

- [ ] **Step 5: Verify GREEN**

```powershell
pwsh -NoProfile -File .\tools\tests\test_project23_rigid_startup.ps1
pwsh -NoProfile -File .\tools\tests\test_project23_utf8.ps1
pwsh -NoProfile -File .\tools\tests\test_default_character_startup.ps1
pwsh -NoProfile -File .\tools\tests\test_portable_runtime.ps1
pwsh -NoProfile -File .\tools\tests\test_visual_capture_contracts.ps1
```

Expected: all five commands exit 0.

- [ ] **Step 6: Build project 23**

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  '.\Dx11\23_Rigid_Animation\23_Rigid_Animation.vcxproj' `
  /t:Rebuild '/p:Configuration=Debug;Platform=x64' /m /v:minimal /nologo
```

Expected: exit 0 and `Dx11/x64/Debug/23_Rigid_Animation.exe` is produced.

- [ ] **Step 7: Launch and verify rigid motion**

```powershell
Start-Process `
  -FilePath '.\Dx11\x64\Debug\23_Rigid_Animation.exe' `
  -WorkingDirectory '.\Dx11\23_Rigid_Animation'
```

Confirm BoxHuman is visible, the Play checkbox is active, component nodes move over time, model transform remains at origin/unit scale/zero rotation, and `Auto Rotate (Yaw)` remains off.

- [ ] **Step 8: Commit the rigid startup**

```powershell
git add -- `
  Dx11/23_Rigid_Animation/App.cpp `
  Dx11/23_Rigid_Animation/README.md `
  tools/tests/test_project23_rigid_startup.ps1 `
  tools/tests/test_default_character_startup.ps1 `
  tools/tests/test_portable_runtime.ps1
git diff --cached --check
git commit -m "feat: start project 23 rigid animation sample"
```

### Task 3: Combined verification and GitHub handoff

**Files:**
- Verify: all files changed by both approved plans

**Interfaces:**
- Consumes: completed bind-pose and project 23 commits.
- Produces: a reviewed, buildable feature branch ready for the user-approved GitHub integration path.

- [ ] **Step 1: Run the complete relevant test set**

```powershell
pwsh -NoProfile -File .\tools\tests\test_bind_pose_without_animation.ps1
pwsh -NoProfile -File .\tools\tests\test_project23_utf8.ps1
pwsh -NoProfile -File .\tools\tests\test_project23_rigid_startup.ps1
pwsh -NoProfile -File .\tools\tests\test_default_character_startup.ps1
pwsh -NoProfile -File .\tools\tests\test_portable_runtime.ps1
pwsh -NoProfile -File .\tools\tests\test_visual_capture_contracts.ps1
```

Expected: six passing commands and no PowerShell errors.

- [ ] **Step 2: Rebuild projects 18 and 23 together**

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  '.\Dx11\TutorialApp.sln' `
  '/t:18_fbx_Animation;23_Rigid_Animation' `
  '/p:Configuration=Debug;Platform=x64' /m /v:minimal /nologo
```

Expected: exit 0 and both executables are present.

- [ ] **Step 3: Review scope and protected files**

```powershell
git diff --check main..HEAD
git status --short
git diff --name-status main..HEAD
```

Confirm the feature branch contains the two specs, two plans, focused tests, `FbxManager.cpp`, and project 23 text/startup changes only. Recheck main-checkout hashes for the user's `SampleModel.glb` and `SampleModel2.glb`; do not stage them or `Camera.cpp`.

- [ ] **Step 4: Request final code review**

Use `superpowers:requesting-code-review` against the full `main..HEAD` diff. Address only findings that are reproducible and within the approved specs, using `superpowers:receiving-code-review` for any requested change.

- [ ] **Step 5: Run fresh verification after review**

Repeat Steps 1-3 after the final code change. Do not claim completion or push based on an earlier test run; use `superpowers:verification-before-completion`.

- [ ] **Step 6: Finish the branch**

Use `superpowers:finishing-a-development-branch`. Because the user requested GitHub publication, offer the standard integration choices and recommend pushing the feature branch and creating a pull request unless the user explicitly chooses a local merge followed by a `main` push.
