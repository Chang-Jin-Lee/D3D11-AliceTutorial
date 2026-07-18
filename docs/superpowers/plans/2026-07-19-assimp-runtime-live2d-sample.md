# Assimp Runtime and Live2D Sample Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make fresh clones contain and deploy the Assimp runtime DLL, and make `11_Live2D` automatically load a bundled redistributable Cubism model.

**Architecture:** Keep the existing Assimp import library and shared MSBuild copy target, but make the matching DLL an explicit tracked exception to the build-output ignore rules. Add a pinned, permissively redistributable Live2D runtime bundle under `Dx11/Resource`, centralize model loading in one `App` helper, and call that helper at startup and from the existing file picker. A dependency-free PowerShell contract test verifies packaging and wiring without launching any tutorial executable.

**Tech Stack:** Visual Studio/MSBuild, C++17/C++20, Direct3D 11, Assimp 6.0.5 VC143 x64, Live2D Cubism Native SDK 5-r.4.1, PowerShell 5.1+, Git.

## Global Constraints

- Do not launch tutorial executables; the user will perform runtime validation on another computer.
- Do not replace or rebuild Assimp; retain `assimp-vc143-mt.lib` and `assimp-vc143-mt.dll`.
- Keep one Assimp DLL under `Dx11/third_party/assimp/bin/msvc`; do not duplicate it in tutorial project directories.
- Builds and runtime must not require network access.
- Pin the Live2D asset source to commit `994c4719f081a3f219b62abbeb4a4b43543a48b8` from `https://github.com/BluePengcho/Open_Source_Hand_Tracking_Live2D_Model`.
- Preserve the existing **Open model3.json** workflow while adding automatic startup loading.
- Commit on the current `main` branch and push `main` to `origin`; this includes the pre-existing local commits already ahead of `origin/main`.

---

### Task 1: Make the Assimp runtime a tracked deployment input

**Files:**
- Create: `tools/tests/test_portable_runtime.ps1`
- Modify: `.gitignore:29-33`
- Add: `Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll`
- Test: `tools/tests/test_portable_runtime.ps1`

**Interfaces:**
- Consumes: `Dx11/Directory.Build.targets` target `CopyThirdPartyRuntimeDlls` and the existing local VC143 x64 DLL.
- Produces: a tracked DLL at `Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll` and a reusable static contract test for later tasks.

- [ ] **Step 1: Write the failing Assimp packaging test**

Create `tools/tests/test_portable_runtime.ps1` with this complete content:

```powershell
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

function Assert-Matches {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )

    Assert-True -Condition ([regex]::IsMatch($Text, $Pattern)) -Message $Message
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

& git -C $repoRoot check-ignore --quiet -- $assimpRelative
$ignoredExitCode = $LASTEXITCODE
Assert-True -Condition ($ignoredExitCode -ne 0) `
    -Message 'Assimp runtime DLL is still ignored by Git.'

& git -C $repoRoot ls-files --error-unmatch -- $assimpRelative *> $null
$trackedExitCode = $LASTEXITCODE
Assert-True -Condition ($trackedExitCode -eq 0) `
    -Message 'Assimp runtime DLL is not tracked by Git.'

$targets = Get-Content -Raw -LiteralPath $targetsPath
Assert-Contains -Text $targets -Expected 'assimp\bin\msvc\assimp-vc143-mt.dll' `
    -Message 'Directory.Build.targets does not name the Assimp runtime DLL.'
Assert-Contains -Text $targets -Expected 'DestinationFolder="$(TargetDir)"' `
    -Message 'Directory.Build.targets does not copy runtime DLLs to TargetDir.'
Assert-Contains -Text $targets -Expected 'DestinationFolder="$(CommonBinDir)"' `
    -Message 'Directory.Build.targets does not copy runtime DLLs to the common bin directory.'

if ($script:Failures.Count -gt 0) {
    Write-Host 'Portable runtime verification failed:'
    foreach ($failure in $script:Failures) {
        Write-Host " - $failure"
    }
    exit 1
}

Write-Host 'Portable runtime verification passed.'
```

- [ ] **Step 2: Run the test and verify the packaging contract fails**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
```

Expected: exit code `1`, including both `Assimp runtime DLL is still ignored by Git.` and `Assimp runtime DLL is not tracked by Git.`

- [ ] **Step 3: Unignore and stage the existing DLL**

Insert this exact block immediately after `[Bb]in/` in `.gitignore`:

```gitignore

# Repo-local Assimp runtime required by model-loading samples.
!Dx11/third_party/assimp/bin/
!Dx11/third_party/assimp/bin/msvc/
!Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll
```

Then stage the DLL so the contract test can distinguish a deliverable repository input from an untracked local file:

```powershell
git add -- .gitignore tools/tests/test_portable_runtime.ps1 Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll
```

- [ ] **Step 4: Run the test and verify the Assimp packaging contract passes**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
```

Expected: exit code `0` and `Portable runtime verification passed.`

- [ ] **Step 5: Commit the Assimp packaging fix**

```powershell
git commit -m "fix: distribute Assimp runtime DLL"
```

Expected: the commit includes only `.gitignore`, the new PowerShell test, and `assimp-vc143-mt.dll`.

---

### Task 2: Bundle the redistributable Live2D runtime model

**Files:**
- Modify: `tools/tests/test_portable_runtime.ps1`
- Create: `Dx11/Resource/Live2D/Skeleton_Model/README.md`
- Add: `Dx11/Resource/Live2D/Skeleton_Model/Skeleton_Model.model3.json`
- Add: `Dx11/Resource/Live2D/Skeleton_Model/Skeleton_Model.moc3`
- Add: `Dx11/Resource/Live2D/Skeleton_Model/Skeleton_Model.cdi3.json`
- Add: `Dx11/Resource/Live2D/Skeleton_Model/Skeleton_Model.2048/texture_00.png`
- Test: `tools/tests/test_portable_runtime.ps1`

**Interfaces:**
- Consumes: upstream model bundle at pinned commit `994c4719f081a3f219b62abbeb4a4b43543a48b8`.
- Produces: a self-contained Cubism `model3.json` bundle rooted at `Dx11/Resource/Live2D/Skeleton_Model`.

- [ ] **Step 1: Extend the contract test with the Live2D asset manifest**

Insert this exact block before the final `if ($script:Failures.Count -gt 0)` in `tools/tests/test_portable_runtime.ps1`:

```powershell
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
```

- [ ] **Step 2: Run the test and verify the Live2D asset contract fails**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
```

Expected: exit code `1` with missing paths under `Dx11/Resource/Live2D/Skeleton_Model`.

- [ ] **Step 3: Copy the pinned upstream runtime files**

Run these PowerShell statements from the repository root:

```powershell
$assetTemp = Join-Path ([System.IO.Path]::GetTempPath()) ('alice-live2d-' + [guid]::NewGuid().ToString('N'))
$assetSource = Join-Path $assetTemp 'source'
$assetDestination = Join-Path (Get-Location) 'Dx11\Resource\Live2D\Skeleton_Model'
New-Item -ItemType Directory -Path $assetTemp | Out-Null
git clone --depth 1 https://github.com/BluePengcho/Open_Source_Hand_Tracking_Live2D_Model.git $assetSource
$actualCommit = git -C $assetSource rev-parse HEAD
if ($actualCommit -ne '994c4719f081a3f219b62abbeb4a4b43543a48b8') { throw "Unexpected Live2D source commit: $actualCommit" }
New-Item -ItemType Directory -Force -Path (Join-Path $assetDestination 'Skeleton_Model.2048') | Out-Null
Copy-Item -LiteralPath (Join-Path $assetSource 'Skeleton_Model\Skeleton_Model.model3.json') -Destination $assetDestination
Copy-Item -LiteralPath (Join-Path $assetSource 'Skeleton_Model\Skeleton_Model.moc3') -Destination $assetDestination
Copy-Item -LiteralPath (Join-Path $assetSource 'Skeleton_Model\Skeleton_Model.cdi3.json') -Destination $assetDestination
Copy-Item -LiteralPath (Join-Path $assetSource 'Skeleton_Model\Skeleton_Model.2048\texture_00.png') -Destination (Join-Path $assetDestination 'Skeleton_Model.2048')
```

Create `Dx11/Resource/Live2D/Skeleton_Model/README.md` with this complete content:

```markdown
# Skeleton_Model Live2D sample

This runtime model is copied from:

https://github.com/BluePengcho/Open_Source_Hand_Tracking_Live2D_Model

Pinned upstream commit: `994c4719f081a3f219b62abbeb4a4b43543a48b8`

The upstream author states that the model and its parts may be freely used, copied, and edited, and that attribution is not required. This repository retains the source and commit information for provenance.

Copied runtime files:

- `Skeleton_Model.model3.json`
- `Skeleton_Model.moc3`
- `Skeleton_Model.cdi3.json`
- `Skeleton_Model.2048/texture_00.png`
```

- [ ] **Step 4: Run the test and verify the bundled model is self-contained**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
```

Expected: exit code `0` and `Portable runtime verification passed.`

- [ ] **Step 5: Commit the Live2D asset bundle**

```powershell
git add -- tools/tests/test_portable_runtime.ps1 Dx11/Resource/Live2D/Skeleton_Model
git commit -m "feat: bundle Live2D sample model"
```

Expected: the commit contains the four upstream runtime files, the provenance README, and the expanded test.

---

### Task 3: Automatically load the bundled model through one helper

**Files:**
- Modify: `tools/tests/test_portable_runtime.ps1`
- Modify: `Dx11/11_Live2D/App.h:129-134`
- Modify: `Dx11/11_Live2D/App.cpp:43-50,419-520,646-683`
- Test: `tools/tests/test_portable_runtime.ps1`

**Interfaces:**
- Consumes: `Dx11/Resource/Live2D/Skeleton_Model/Skeleton_Model.model3.json` from Task 2 and the existing `MinimalUserModel` methods.
- Produces: `bool App::LoadLive2DModel(const std::wstring& model3Path)` used by startup and the file picker.

- [ ] **Step 1: Extend the contract test with startup and helper wiring**

Insert this exact block before the final failure-reporting `if` in `tools/tests/test_portable_runtime.ps1`:

```powershell
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
```

- [ ] **Step 2: Run the test and verify the loading contract fails**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
```

Expected: exit code `1` with failures for the helper declaration, helper definition, startup model path, and helper calls.

- [ ] **Step 3: Declare the shared loader**

Add this exact declaration to the private section of `App` in `Dx11/11_Live2D/App.h`, immediately above `InitEffect()`:

```cpp
bool LoadLive2DModel(const std::wstring& model3Path);
```

- [ ] **Step 4: Add the default path and helper implementation**

Add this constant inside the existing anonymous namespace in `Dx11/11_Live2D/App.cpp`, above `ToUtf8Bytes`:

```cpp
constexpr wchar_t kDefaultLive2DModelPath[] =
    L"..\\Resource\\Live2D\\Skeleton_Model\\Skeleton_Model.model3.json";
```

Add this complete method between `OnInitialize` and `OnUninitialize`:

```cpp
bool App::LoadLive2DModel(const std::wstring& model3Path)
{
    m_L2DModelJsonPath = model3Path;
    m_L2DRequested = true;

    for (auto* texture : m_L2DTexSRVs)
    {
        SAFE_RELEASE(texture);
    }
    m_L2DTexSRVs.clear();
    m_L2DTexSizes.clear();
    m_L2DMotionGroups.clear();
    m_L2DMotionGroupIndex = 0;
    m_L2DMotionIndex = 0;

    delete m_L2D;
    m_L2D = nullptr;
    m_L2DLoaded = false;

    if (!m_L2DReady)
    {
        m_L2DStatus = "Live2D is not initialized";
        return false;
    }

    m_L2D = new MinimalUserModel();
    if (!m_L2D->LoadFromModel3(model3Path) ||
        !m_L2D->LoadAndBindTextures(m_pDevice))
    {
        delete m_L2D;
        m_L2D = nullptr;
        m_L2DStatus = "Model load failed";
        return false;
    }

    m_L2DLoaded = true;
    m_L2DStatus = "Model loaded";
    m_L2DMotionGroups = m_L2D->motionGroups;

    if (auto* renderer = m_L2D->GetRenderer<Rendering::CubismRenderer_D3D11>())
    {
        renderer->UseHighPrecisionMask(false);
        renderer->SetClippingMaskBufferSize(256.0f, 256.0f);
        const int renderTextureCount = renderer->GetRenderTextureCount();
        for (int index = 0; index < renderTextureCount; ++index)
        {
            renderer->GetMaskBuffer(0, index)->CreateOffscreenSurface(
                m_pDevice,
                256,
                256);
        }
    }

    return true;
}
```

After the Cubism initialization block in `OnInitialize`, before system-information initialization, add:

```cpp
LoadLive2DModel(kDefaultLive2DModelPath);
```

- [ ] **Step 5: Route the file picker through the helper**

Replace the existing block inside `if (GetOpenFileNameW(&ofn))` with:

```cpp
LoadLive2DModel(file);
```

Do not change the file dialog filter or remove the button.

- [ ] **Step 6: Run the test and verify the loading contract passes**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
git diff --check
```

Expected: the PowerShell test exits `0`, and `git diff --check` prints no errors.

- [ ] **Step 7: Commit the startup loading change**

```powershell
git add -- tools/tests/test_portable_runtime.ps1 Dx11/11_Live2D/App.h Dx11/11_Live2D/App.cpp
git commit -m "feat: load bundled Live2D model on startup"
```

Expected: the commit includes only the test and `11_Live2D` code changes.

---

### Task 4: Document portable runtime behavior

**Files:**
- Modify: `tools/tests/test_portable_runtime.ps1`
- Modify: `Dx11/11_Live2D/README.md:9-40`
- Modify: `Dx11/third_party/README.md:7-15`
- Test: `tools/tests/test_portable_runtime.ps1`

**Interfaces:**
- Consumes: the tracked DLL contract and startup model path from Tasks 1-3.
- Produces: clone/build/runtime instructions that match the repository state.

- [ ] **Step 1: Add documentation assertions to the contract test**

Insert this exact block before the final failure-reporting `if` in `tools/tests/test_portable_runtime.ps1`:

```powershell
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
```

- [ ] **Step 2: Run the test and verify documentation checks fail**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
```

Expected: exit code `1` with the four README documentation failures.

- [ ] **Step 3: Update the Live2D README**

Add this text after the introductory `내용` bullet in `Dx11/11_Live2D/README.md`:

```markdown
- 기본 예제 모델: 실행 시 `../Resource/Live2D/Skeleton_Model/Skeleton_Model.model3.json`을 자동으로 로드합니다.
  - 원본: https://github.com/BluePengcho/Open_Source_Hand_Tracking_Live2D_Model
  - 라이선스/출처 기록: `Dx11/Resource/Live2D/Skeleton_Model/README.md`
  - “Open model3.json” 버튼으로 다른 Cubism 모델을 계속 선택할 수 있습니다.
```

Replace the first usage step with:

```markdown
  1) 앱 실행 시 번들된 `Skeleton_Model.model3.json`이 자동 로드됩니다. 다른 모델은 “Open model3.json”으로 선택합니다.
```

- [ ] **Step 4: Update the third-party README**

Change the Assimp table row to:

```markdown
| Assimp | `v6.0.5`, assimp/assimp | Windows x64 VC143 build with only `OBJ`, `FBX`, `GLTF`, and `MMD` importers enabled. The matching `assimp-vc143-mt.dll` is tracked with the import library. |
```

Add this paragraph below the dependency table:

```markdown
`Directory.Build.targets` copies `assimp-vc143-mt.dll` from `third_party/assimp/bin/msvc` to each executable output directory and to `Dx11/bin` after a successful build.
```

- [ ] **Step 5: Run the full static verification and commit documentation**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
git diff --check
git add -- tools/tests/test_portable_runtime.ps1 Dx11/11_Live2D/README.md Dx11/third_party/README.md
git commit -m "docs: explain portable runtime assets"
```

Expected: verification passes, whitespace checking prints no errors, and the commit contains only the verifier and documentation.

---

### Task 5: Verify repository state and push `main`

**Files:**
- Verify: `.gitignore`
- Verify: `Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll`
- Verify: `Dx11/Directory.Build.targets`
- Verify: `Dx11/Resource/Live2D/Skeleton_Model/**`
- Verify: `Dx11/11_Live2D/App.h`
- Verify: `Dx11/11_Live2D/App.cpp`
- Verify: `tools/tests/test_portable_runtime.ps1`

**Interfaces:**
- Consumes: all prior task outputs.
- Produces: a clean, pushed `main` branch containing the portable runtime fix.

- [ ] **Step 1: Run the final non-runtime verification**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\tests\test_portable_runtime.ps1
git diff --check HEAD^ HEAD
git status --short --branch
```

Expected: the contract test passes, the diff check prints no errors, and the worktree is clean with `main` ahead of `origin/main`.

- [ ] **Step 2: Inspect the committed delivery set**

Run:

```powershell
git log --oneline --decorate origin/main..HEAD
git ls-files --stage -- Dx11/third_party/assimp/bin/msvc/assimp-vc143-mt.dll Dx11/Resource/Live2D/Skeleton_Model
```

Expected: the log contains the design, plan, Assimp packaging, Live2D asset, startup loading, and documentation commits; `git ls-files` lists the Assimp DLL and every bundled Live2D file.

- [ ] **Step 3: Refresh remote state and push**

Run:

```powershell
git fetch origin
git status --short --branch
git push origin main
```

Expected: after fetch, `main` is not behind or diverged; push exits `0` and updates `origin/main`. If the fetch shows remote commits not present locally, stop before pushing and request direction instead of rebasing or force-pushing.

- [ ] **Step 4: Confirm the pushed state**

Run:

```powershell
git status --short --branch
git log -1 --oneline --decorate
```

Expected: `main...origin/main` has no ahead/behind marker, the worktree is clean, and `HEAD` is decorated with both `main` and `origin/main`.
