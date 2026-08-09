# Project 23 Scale and README Media Refresh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Scale Project 23's startup BoxHuman to `0.01` and regenerate all 37 projects' runtime PNGs, GIFs, and information images with the user's latest character resources.

**Architecture:** Keep the code change local to Project 23 and use its focused startup contract for RED/GREEN verification. Build in the isolated worktree, then capture sequentially from an ignored staged runtime whose `Resource` junction points read-only at the main checkout's updated assets; use the existing manifest pipeline for media generation and validation.

**Tech Stack:** C++17, Direct3D 11, PowerShell 7, MSBuild/Visual Studio 2022, ffmpeg, System.Drawing, Git worktrees

## Global Constraints

- Set only Project 23's automatically loaded BoxHuman startup scale to `(0.01, 0.01, 0.01)`.
- Preserve its origin, zero rotation, camera `(0,0,-8)`, disabled whole-model rotation, rigid classification, and automatic playback.
- Models loaded interactively after startup retain their existing default scale.
- Recapture all 37 manifest projects sequentially as PNG and GIF files; do not run GUI captures in parallel.
- Regenerate all 37 information images from the new PNG files.
- Preserve README markup, navigation, branding, descriptions, and media paths.
- Do not modify or commit `BoxHuman.fbx`, `SampleModel.glb`, its variants, or `Dx11/Common/Camera.cpp`.
- Preserve the UTF-8 BOM on `Dx11/23_Rigid_Animation/App.cpp`.
- Use the main checkout's `Dx11/Resource` only through the ignored staging junction; never copy user assets into tracked worktree paths.

---

### Task 1: Repair the information-image baseline contract

**Files:**
- Modify: `tools/tests/test_readme_info_images.ps1:126-166`
- Verify: `tools/readme_media_common.ps1:14-29`

**Interfaces:**
- Consumes: `Test-ReadmeMediaManifest()` rejection messages from `tools/readme_media_common.ps1`.
- Produces: a passing information-image contract that still requires unsafe paths to exit nonzero and create no outside files.

- [ ] **Step 1: Reproduce the existing RED baseline**

Run:

```powershell
pwsh -NoProfile -File .\tools\tests\test_readme_info_images.ps1
```

Expected: exit 1 with `rooted image manifest path was accepted did not report the expected rejection`. The nested generator already exits nonzero with `invalid safe relative path 'image'`; only the test's expected diagnostic is stale.

- [ ] **Step 2: Update only the expected diagnostics**

Keep `Assert-GeneratorRejected()` and every outside-output assertion. Add explicit `-ExpectedPattern` arguments to the five unsafe-path calls:

```powershell
Assert-GeneratorRejected -Script $infoImageScript `
    -ScriptArguments @('-Manifest', $rootedImageManifestPath, '-ProjectNumber', '01') `
    -Message 'rooted image manifest path was accepted' `
    -ExpectedPattern "invalid safe relative path 'image'"
Assert-GeneratorRejected -Script $infoImageScript `
    -ScriptArguments @('-Manifest', $traversalImageManifestPath, '-ProjectNumber', '01') `
    -Message 'traversal image manifest path was accepted' `
    -ExpectedPattern "invalid safe relative path 'image'"
Assert-GeneratorRejected -Script $infoImageScript `
    -ScriptArguments @('-Manifest', $rootedInfoManifestPath, '-ProjectNumber', '01') `
    -Message 'rooted info image manifest path was accepted' `
    -ExpectedPattern "invalid safe relative path 'infoImage'"
Assert-GeneratorRejected -Script $infoImageScript `
    -ScriptArguments @('-Manifest', $traversalInfoManifestPath, '-ProjectNumber', '01') `
    -Message 'traversal info image manifest path was accepted' `
    -ExpectedPattern "invalid safe relative path 'infoImage'"
Assert-GeneratorRejected -Script $reviewSheetScript `
    -ScriptArguments @('-Manifest', $rootedGifManifestPath, '-OutputDir', (Join-Path $tempRoot 'rooted-gif-review')) `
    -Message 'rooted GIF manifest path was accepted' `
    -ExpectedPattern "invalid safe relative path 'gif'"
```

Do not change production path validation or broaden the regex to accept success output.

- [ ] **Step 3: Verify GREEN and related path contracts**

Run:

```powershell
pwsh -NoProfile -File .\tools\tests\test_readme_info_images.ps1
pwsh -NoProfile -File .\tools\tests\test_readme_media_manifest.ps1
pwsh -NoProfile -File .\tools\tests\test_verify_readme_media.ps1
```

Expected: all three commands exit 0.

- [ ] **Step 4: Commit the baseline repair**

```powershell
git add -- tools/tests/test_readme_info_images.ps1
git diff --cached --check
git commit -m "test: align README info path diagnostics"
```

### Task 2: Scale the Project 23 startup BoxHuman

**Files:**
- Modify: `tools/tests/test_project23_rigid_startup.ps1:34,51`
- Modify: `Dx11/23_Rigid_Animation/App.cpp:319`

**Interfaces:**
- Consumes: the successful BoxHuman load block in `App::OnInitialize()`.
- Produces: startup `model.scale == XMFLOAT3(0.01f, 0.01f, 0.01f)` without changing other model defaults.

- [ ] **Step 1: Change the focused contract first**

Replace both unit-scale assertions with literal `0.01f` assertions:

```powershell
Assert-True ($body -match 'model\.scale\s*=\s*XMFLOAT3\(\s*0\.01f\s*,\s*0\.01f\s*,\s*0\.01f\s*\)') `
    '0.01 startup scale missing'
Assert-True ($loadBlock -match 'model\.scale\s*=\s*XMFLOAT3\(\s*0\.01f\s*,\s*0\.01f\s*,\s*0\.01f\s*\)') `
    '0.01 startup scale must be inside successful load block'
```

- [ ] **Step 2: Run the focused contract and verify RED**

```powershell
pwsh -NoProfile -File .\tools\tests\test_project23_rigid_startup.ps1
```

Expected: exit 1 with `0.01 startup scale missing` because production still uses `1.0f`.

- [ ] **Step 3: Make the minimal production change**

In the successful BoxHuman load block, change only:

```cpp
model.scale = XMFLOAT3(0.01f, 0.01f, 0.01f);
```

Confirm the first three bytes of `App.cpp` remain `EF BB BF`.

- [ ] **Step 4: Verify GREEN, encoding, and Project 23 build**

```powershell
pwsh -NoProfile -File .\tools\tests\test_project23_rigid_startup.ps1
pwsh -NoProfile -File .\tools\tests\test_project23_utf8.ps1
pwsh -NoProfile -File .\tools\tests\test_visual_capture_contracts.ps1
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  '.\Dx11\23_Rigid_Animation\23_Rigid_Animation.vcxproj' `
  /t:Build '/p:Configuration=Debug;Platform=x64' /m /v:minimal /nologo
```

Expected: three contracts and the build exit 0. Record pre-existing shared-code warnings without changing their sources.

- [ ] **Step 5: Commit the scale change**

```powershell
git add -- Dx11/23_Rigid_Animation/App.cpp tools/tests/test_project23_rigid_startup.ps1
git diff --cached --check
git commit -m "fix: scale project 23 BoxHuman startup"
```

### Task 3: Build and recapture all runtime PNG/GIF media

**Files:**
- Modify: `docs/media/readme/*.png` (37 manifest outputs)
- Modify: `docs/media/readme/*.gif` (37 manifest outputs)
- Modify: `docs/media/readme/capture-report.md`
- Temporary ignored files: `Dx11/x64/ReadmeCapture/**`

**Interfaces:**
- Consumes: `tools/readme_media_manifest.json`, the worktree's current `Dx11/bin` binaries, and the main checkout's current `Dx11/Resource` directory.
- Produces: validated runtime PNG/GIF outputs and a capture report for all 37 projects.

- [ ] **Step 1: Record protected state and build all projects**

Record `git -C C:\Github\D3D11-AliceTutorial status --short` and SHA-256 for the modified/untracked character GLBs and `Dx11/Common/Camera.cpp`.

Run:

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' `
  '.\Dx11\TutorialApp.sln' `
  /t:Build '/p:Configuration=Debug;Platform=x64' /m /v:minimal /nologo
```

Expected: exit 0 and every manifest executable exists under `Dx11/bin`.

- [ ] **Step 2: Create the ignored staged runtime**

Run from the feature worktree:

```powershell
$stageRoot = Join-Path (Get-Location) 'Dx11\x64\ReadmeCapture'
$stageBin = Join-Path $stageRoot 'bin'
$stageResource = Join-Path $stageRoot 'Resource'
$stageManifest = Join-Path $stageRoot 'readme_media_manifest.json'

New-Item -ItemType Directory -Path $stageBin -Force | Out-Null
Copy-Item -Path '.\Dx11\bin\*' -Destination $stageBin -Recurse -Force
New-Item -ItemType Junction -Path $stageResource `
  -Target 'C:\Github\D3D11-AliceTutorial\Dx11\Resource' | Out-Null

$manifest = Get-Content -Raw '.\tools\readme_media_manifest.json' | ConvertFrom-Json
$manifest.runtimeDir = 'Dx11/x64/ReadmeCapture/bin'
$json = $manifest | ConvertTo-Json -Depth 20
[IO.File]::WriteAllText($stageManifest, $json, [Text.UTF8Encoding]::new($false))
```

Verify `git check-ignore -q Dx11/x64/ReadmeCapture` exits 0 and `git status --short` does not include staged runtime files.

- [ ] **Step 3: Validate the temporary manifest**

```powershell
pwsh -NoProfile -File .\tools\capture_readme_media.ps1 `
  -Manifest 'Dx11/x64/ReadmeCapture/readme_media_manifest.json' -ValidateOnly
```

Expected: exit 0 with `capture manifest validation passed`.

- [ ] **Step 4: Capture all 37 projects sequentially**

```powershell
pwsh -NoProfile -File .\tools\capture_readme_media.ps1 `
  -Manifest 'Dx11/x64/ReadmeCapture/readme_media_manifest.json' -All
```

Expected: exit 0; the final report contains successful PNG and GIF rows for every project. Keep captures sequential because window focus and synthetic input are shared.

- [ ] **Step 5: Verify all runtime media**

```powershell
pwsh -NoProfile -File .\tools\verify_readme_media.ps1 `
  -Manifest 'Dx11/x64/ReadmeCapture/readme_media_manifest.json'
pwsh -NoProfile -File .\tools\tests\test_public_scene_media.ps1
```

Expected: both commands exit 0. Confirm 37 PNG and 37 GIF manifest paths exist and Project 23's captured UI shows `Model #0: BoxHuman`, scale `0.010`, Play checked, and visible rigid motion.

- [ ] **Step 6: Recheck protected hashes and commit runtime media**

Confirm main-checkout hashes and status match Step 1 exactly.

```powershell
git add -- docs/media/readme/*.png docs/media/readme/*.gif docs/media/readme/capture-report.md
git diff --cached --check
git commit -m "docs: refresh all README runtime captures"
```

### Task 4: Regenerate information images and perform final review

**Files:**
- Modify: `docs/media/readme/info/*-info.png` (37 manifest outputs)
- Verify: all files changed by Tasks 1-3
- Temporary ignored files: `Dx11/x64/ReadmeCapture/review/**`

**Interfaces:**
- Consumes: the Task 3 PNG/GIF outputs and temporary manifest.
- Produces: 37 updated information images plus aggregate PNG/GIF review sheets for visual QA.

- [ ] **Step 1: Generate all information images**

```powershell
pwsh -NoProfile -File .\tools\generate_readme_info_images.ps1 `
  -Manifest 'Dx11/x64/ReadmeCapture/readme_media_manifest.json' -All
```

Expected: exit 0 and every manifest `infoImage` path exists at `1600x640`.

- [ ] **Step 2: Generate aggregate review sheets**

```powershell
pwsh -NoProfile -File .\tools\generate_readme_review_sheets.ps1 `
  -Manifest 'Dx11/x64/ReadmeCapture/readme_media_manifest.json' `
  -OutputDir '.\Dx11\x64\ReadmeCapture\review'
```

Inspect both `readme-png-review.png` and `readme-gif-review.png`. Reject blank frames, old/wrong characters, capture tool windows, unusable framing, or missing animation. If any project fails visual QA, stop this task and report its exact two-digit manifest number; do not commit information images until that project has been recaptured and both review sheets regenerated.

- [ ] **Step 3: Run the complete relevant verification set**

```powershell
$tests = @(
  '.\tools\tests\test_project23_rigid_startup.ps1',
  '.\tools\tests\test_project23_utf8.ps1',
  '.\tools\tests\test_visual_capture_contracts.ps1',
  '.\tools\tests\test_capture_manifest_actions.ps1',
  '.\tools\tests\test_readme_media_manifest.ps1',
  '.\tools\tests\test_readme_info_images.ps1',
  '.\tools\tests\test_verify_readme_media.ps1',
  '.\tools\tests\test_public_scene_media.ps1',
  '.\tools\tests\test_project_readme_updater.ps1'
)
foreach ($test in $tests) {
  & pwsh -NoProfile -File $test
  if ($LASTEXITCODE -ne 0) { throw "$test failed with exit code $LASTEXITCODE" }
}
```

Expected: nine commands exit 0.

- [ ] **Step 4: Verify scope and commit information images**

```powershell
git diff --check
git status --short
git diff --name-status main..HEAD
git add -- docs/media/readme/info/*-info.png
git diff --cached --check
git commit -m "docs: regenerate all README information images"
```

Confirm no README markdown, branding logo, source outside Project 23, protected user asset, or ignored staging path is staged.

- [ ] **Step 5: Final branch verification**

Rerun Task 4 Step 3, verify the full Debug x64 solution is already built at the current HEAD, recheck protected hashes/status, and request a full `main..HEAD` code/media review. Use `superpowers:verification-before-completion` before integration and `superpowers:finishing-a-development-branch` for the user's selected merge/push path.
