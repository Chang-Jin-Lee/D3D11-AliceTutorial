# Project 36 Embedded FBX Showcase Implementation Plan

> ## ABANDONED — DO NOT EXECUTE
>
> This plan was abandoned on 2026-09-02 at the author's instruction. It is kept for its
> design reasoning only.
>
> The branch it ran on, `codex/project36-fbx-showcase`, was deleted along with its worktree.
> Tasks 1–4 had been completed on that branch (tip `5a625c2`, never pushed); Task 5 was
> never started. None of that work is on `main`, and the worktree path this plan tells you
> to work in no longer exists.
>
> Do not resume it without the author explicitly reopening the work. If they do, treat this
> as a starting point to be re-planned rather than a live checklist: `main` has moved 56+
> commits since 2026-08-12 and Project 36 changed substantially (skybox asset bootstrap,
> model transparency, UI restoration).
>
> See `docs/superpowers/handoffs/2026-09-02-main-state-handoff.md` for what was discarded.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Project 36's awkward retargeted README capture with two large instances of the user's FBX, directly playing `Armature|Humanoid` and `Armature|Humanoid.001` against the Bridge release skybox.

**Architecture:** Commit the supplied FBX byte-for-byte and load it twice only in README capture mode, relying on the existing path cache for shared geometry and each `ModelEntry::fbxBaseAnimator` for an independent palette. Prepare the ignored Bridge DDS set before process launch, apply it after `Skybox` creation, retain the proven backbuffer publisher, and publish new media only after staged automated and visual verification.

**Tech Stack:** C++20, Direct3D 11, Assimp, existing `FbxModel`/`FbxAnimation`, ImGui, PowerShell 7, GitHub Release assets, ffmpeg/ffprobe, MSBuild.

## Global Constraints

- Work only in `C:\Github\D3D11-AliceTutorial\.worktrees\project36-fbx-showcase` on `codex/project36-fbx-showcase`.
- Copy `Z:\Alice_Swimsuit_white.fbx` verbatim to `Dx11/Resource/fbx/Public/MyAlice/Portfolio/Alice_Swimsuit_white.fbx`.
- The committed FBX must remain 13,912,812 bytes with SHA-256 `9E77F9CC872F2BB6802D1E2D9C984C30D6C986CF68FDE5CA82CA3CDB595289F7`.
- Select `Armature|Humanoid` and `Armature|Humanoid.001` by exact name; never play `Armature|T-Pose`.
- Do not extract, resample, re-export, or retarget animation data.
- Use the `Skybox_2` release's four Bridge DDS files; never use `Hanako.dds` or a flat-gray fallback for the capture.
- Do not commit `Skybox.7z` or extracted Bridge DDS files, and do not create junctions, symlinks, or reparse points.
- Activate the two-character scene only in README capture mode; preserve normal Project 36 behavior.
- Keep root and Project 36 README text/markup and their existing media paths unchanged.
- Capture PNG at 1600x900 and GIF at 800x450, 8 fps, approximately 12 seconds, at most 5 MiB.
- Capture into `.superpowers/artifacts/project36-embedded-fbx-showcase` first. Do not overwrite public media until all staged checks and visual review pass.
- Preserve the existing Project 36 atomic backbuffer writer and the capture tool's per-project GIF settings.
- Each task follows RED -> GREEN -> refactor, runs `git diff --check`, and ends in a focused commit.

## File Structure

### New files

- `Dx11/Resource/fbx/Public/MyAlice/Portfolio/Alice_Swimsuit_white.fbx` — exact user-authored model, textures, skin, and three embedded animation records.
- `tools/project36_bridge_skybox.ps1` — deterministic Bridge release preflight and ignored extraction/install functions.
- `tools/tests/test_project36_embedded_fbx_asset.ps1` — exact source hash/size and obsolete-generated-asset removal contract.
- `tools/tests/test_project36_bridge_skybox.ps1` — four-file, incomplete-set, contained-install, and no-debug-fallback contract.
- `tools/tests/test_project36_embedded_showcase.ps1` — capture-only source contract for two instances, exact clips, independent animators, framing, and Bridge application order.
- `tools/tests/test_project36_embedded_media.ps1` — final PNG/GIF dimensions, duration, left/right motion, environment, and obsolete-HUD checks.

### Modified files

- `Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl` — replace the generated four-character timeline with direct embedded-FBX playback; retain backbuffer publication.
- `Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl` — replace four `CharacterAnimator` slots and IK state with two embedded-FBX slot descriptors and Bridge readiness.
- `Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl` — capture-only pair loading/composition and post-`Skybox` Bridge activation; remove generated external-clip loads.
- `Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl` — retain showcase-owned palette skipping for the two direct animator slots and remove IK debug update dependencies.
- `Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl` — remove capture-only IK line calls while keeping the compact HUD and backbuffer write.
- `Dx11/36_AdvancedAnim_Sound_Click/App_Utilities.inl` — return explicit success from DDS/IBL changes so capture readiness cannot claim a missing environment.
- `Dx11/36_AdvancedAnim_Sound_Click/App.h` — update helper return types and remove obsolete IK-debug method declaration.
- `tools/capture_readme_media.ps1` — invoke Bridge preflight before launching Project 36.
- `tools/readme_media_manifest.json` — change only Project 36 `gifSeconds` from 8 to 12.
- `tools/tests/test_capture_manifest_actions.ps1` — require Project 36 Bridge preflight before `Start-Process`.
- `tools/tests/test_readme_media_manifest.ps1` — require the 12-second Project 36 override and unchanged defaults elsewhere.
- `tools/tests/test_verify_readme_media.ps1` — cover a 12-second project override.
- `tools/tests/test_visual_capture_contracts.ps1` — replace old four-phase Project 36 expectations with the embedded-pair contract.
- `tools/verify_readme_media.ps1` — no expected production logic change; verified against the new 12-second manifest value.
- `docs/media/readme/36-AdvancedAnim-Sound-Click.png` — approved representative still.
- `docs/media/readme/36-advanced-anim-sound-click.gif` — approved two-clip animation capture.
- `docs/media/readme/info/36-AdvancedAnim-Sound-Click-info.png` — regenerated from the approved PNG.
- `docs/media/readme/capture-report.md` — replace only Project 36 success rows.

### Removed files

- `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioDance.glb`
- `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioUpperWave.glb`
- `tools/generate_project36_portfolio_animations.py`
- `tools/tests/test_project36_portfolio_assets.py`
- `tools/tests/test_project36_portfolio_media.ps1`
- `tools/tests/test_project36_portfolio_showcase.ps1`

---

### Task 1: Preserve the authored FBX and remove obsolete generated motion

**Files:**
- Create: `Dx11/Resource/fbx/Public/MyAlice/Portfolio/Alice_Swimsuit_white.fbx`
- Create: `tools/tests/test_project36_embedded_fbx_asset.ps1`
- Remove: `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioDance.glb`
- Remove: `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioUpperWave.glb`
- Remove: `tools/generate_project36_portfolio_animations.py`
- Remove: `tools/tests/test_project36_portfolio_assets.py`

**Interfaces:**
- Produces: one immutable FBX at `..\Resource\fbx\Public\MyAlice\Portfolio\Alice_Swimsuit_white.fbx` as seen from `Dx11/bin`.
- Produces: `tools/tests/test_project36_embedded_fbx_asset.ps1` with source/destination hash, size, expected metadata constants, and obsolete-file absence checks.
- Preserves: the source file on `Z:`; it is read but never modified or moved.

- [ ] **Step 1: Write the failing asset contract**

Create `tools/tests/test_project36_embedded_fbx_asset.ps1` with these exact assertions:

```powershell
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$asset = Join-Path $repo 'Dx11\Resource\fbx\Public\MyAlice\Portfolio\Alice_Swimsuit_white.fbx'
$expectedHash = '9E77F9CC872F2BB6802D1E2D9C984C30D6C986CF68FDE5CA82CA3CDB595289F7'
$expectedBytes = 13912812
$expectedScene = [pscustomobject]@{ Meshes=13; Materials=13; EmbeddedTextures=20; Animations=3 }
$expectedClips = @(
    [pscustomobject]@{ Name='Armature|Humanoid'; Ticks=284.0; Fps=24.0; Seconds=11.833333; Channels=220 },
    [pscustomobject]@{ Name='Armature|Humanoid.001'; Ticks=175.0; Fps=24.0; Seconds=7.291667; Channels=220 },
    [pscustomobject]@{ Name='Armature|T-Pose'; Ticks=0.0; Fps=24.0; Seconds=0.0; Channels=50 }
)

if (-not (Test-Path -LiteralPath $asset -PathType Leaf)) { throw 'portfolio FBX is missing' }
if ((Get-Item -LiteralPath $asset).Length -ne $expectedBytes) { throw 'portfolio FBX size changed' }
if ((Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash -ne $expectedHash) { throw 'portfolio FBX hash changed' }
if ($expectedScene.Meshes -ne 13 -or $expectedScene.Materials -ne 13 -or $expectedScene.EmbeddedTextures -ne 20 -or $expectedScene.Animations -ne 3) { throw 'recorded scene metadata is invalid' }
if ($expectedClips[0].Seconds -le 0 -or $expectedClips[1].Seconds -le 0 -or $expectedClips[2].Seconds -ne 0) { throw 'recorded clip metadata is invalid' }

$obsolete = @(
  'Dx11\Resource\fbx\Public\MyAlice\Animations\anim_PortfolioDance.glb',
  'Dx11\Resource\fbx\Public\MyAlice\Animations\anim_PortfolioUpperWave.glb',
  'tools\generate_project36_portfolio_animations.py',
  'tools\tests\test_project36_portfolio_assets.py'
)
foreach ($relative in $obsolete) {
    if (Test-Path -LiteralPath (Join-Path $repo $relative)) { throw "obsolete generated asset remains: $relative" }
}
'project 36 embedded FBX asset contract passed'
```

The scene/clip constants document the Assimp inspection tied to the immutable expected hash. Task 3 independently asks the production Assimp scene for the exact names and positive durations before it permits capture playback.

- [ ] **Step 2: Run the contract and verify RED**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_embedded_fbx_asset.ps1
```

Expected: exit 1 with `portfolio FBX is missing`.

- [ ] **Step 3: Copy the binary verbatim and remove only the obsolete generated files**

Run:

```powershell
New-Item -ItemType Directory -Force -Path 'Dx11/Resource/fbx/Public/MyAlice/Portfolio' | Out-Null
Copy-Item -LiteralPath 'Z:\Alice_Swimsuit_white.fbx' -Destination 'Dx11/Resource/fbx/Public/MyAlice/Portfolio/Alice_Swimsuit_white.fbx'
Remove-Item -LiteralPath 'Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioDance.glb'
Remove-Item -LiteralPath 'Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioUpperWave.glb'
Remove-Item -LiteralPath 'tools/generate_project36_portfolio_animations.py'
Remove-Item -LiteralPath 'tools/tests/test_project36_portfolio_assets.py'
```

Immediately compare the `Z:` and repository SHA-256 values; abort if either differs from the expected hash.

- [ ] **Step 4: Run GREEN and inspect the binary scope**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_embedded_fbx_asset.ps1
git diff --check
git status --short
```

Expected: contract exits 0; only the new test/FBX and four intended deletions are shown for this task.

- [ ] **Step 5: Commit Task 1**

```powershell
git add -- Dx11/Resource/fbx/Public/MyAlice/Portfolio/Alice_Swimsuit_white.fbx tools/tests/test_project36_embedded_fbx_asset.ps1 Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioDance.glb Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioUpperWave.glb tools/generate_project36_portfolio_animations.py tools/tests/test_project36_portfolio_assets.py
git diff --cached --check
git commit -m "feat: add authored project 36 FBX"
```

---

### Task 2: Prepare the Bridge release assets before capture

**Files:**
- Create: `tools/project36_bridge_skybox.ps1`
- Create: `tools/tests/test_project36_bridge_skybox.ps1`
- Modify: `tools/capture_readme_media.ps1`
- Modify: `tools/tests/test_capture_manifest_actions.ps1`

**Interfaces:**
- Produces: `Test-Project36BridgeSkybox([string]$BridgeDir) -> bool`.
- Produces: `Install-Project36BridgeSkybox([string]$ExtractRoot, [string]$BridgeDir)`, copying exactly four nonempty DDS files through temporary siblings.
- Produces: `Ensure-Project36BridgeSkybox([string]$RepoRoot, [string]$RuntimeDir)`, using release URL `https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/releases/download/Skybox_2/Skybox.7z`.
- Installs ignored runtime files under `<parent of RuntimeDir>/Resource/Skybox/Bridge`.
- Preserves: capture of projects other than 36; they never call this preflight.

- [ ] **Step 1: Write the failing Bridge helper and wiring tests**

Create a fixture directory in `test_project36_bridge_skybox.ps1`, write four distinct nonempty byte arrays using the exact names below, and assert the helper installs them into a contained destination:

```powershell
$BridgeNames = @(
  'bridgeDiffuseHDR.dds',
  'bridgeSpecularHDR.dds',
  'bridgeBrdf.dds',
  'bridgeEnvHDR.dds'
)
```

Then delete `bridgeEnvHDR.dds` from the fixture and assert installation throws without replacing an already valid destination. Assert the helper source contains the exact release URL and no `Hanako.dds`. Extend `test_capture_manifest_actions.ps1` to require this ordering in `Invoke-ProjectCapture`:

```powershell
Ensure-Project36BridgeSkybox -RepoRoot $RepoRoot -RuntimeDir $RuntimeDir
$process = Start-Process
```

and require the preflight to be guarded by `$Project.number -eq '36'`.

- [ ] **Step 2: Run both tests and verify RED**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_bridge_skybox.ps1
pwsh -NoProfile -File tools/tests/test_capture_manifest_actions.ps1
```

Expected: both fail because the helper and pre-launch call do not exist.

- [ ] **Step 3: Implement contained Bridge preparation**

In `tools/project36_bridge_skybox.ps1`, define the exact four-name array and functions from the interface. `Ensure-Project36BridgeSkybox` must:

1. Resolve `RepoRoot`, `RuntimeDir`, cache root `.superpowers/artifacts/project36-embedded-fbx-showcase/skybox-cache`, and target Bridge directory to absolute paths.
2. Prove the cache is inside `.superpowers/artifacts/project36-embedded-fbx-showcase` and the target is inside `<parent of RuntimeDir>/Resource/Skybox` before any recursive cleanup.
3. Return without network access when all four target files exist and have positive length.
4. Download `Skybox.7z` only when the cache lacks it, using `Invoke-WebRequest` with `SilentlyContinue` progress.
5. Extract to a unique cache child using `tar.exe -xf`; if tar fails, try the first available `7z.exe` or `7za.exe`; throw when neither succeeds.
6. Recursively locate exactly one source for each required filename, reject missing/empty files, copy each to `<name>.incoming`, then `Move-Item -Force` onto its target.
7. Re-run `Test-Project36BridgeSkybox` and throw if the final set is incomplete.
8. Remove only the unique extraction child in `finally`; retain the ignored archive cache for later captures.

Dot-source the helper near the top of `capture_readme_media.ps1`. In `Invoke-ProjectCapture`, after executable/path validation and before setting environment variables or starting the process, call it only for Project 36.

- [ ] **Step 4: Run GREEN without downloading the release**

The focused helper test must use its local fixture and never invoke the network:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_bridge_skybox.ps1
pwsh -NoProfile -File tools/tests/test_capture_manifest_actions.ps1
pwsh -NoProfile -File tools/capture_readme_media.ps1 -ValidateOnly
git diff --check
```

Expected: all four commands exit 0.

- [ ] **Step 5: Commit Task 2**

```powershell
git add -- tools/project36_bridge_skybox.ps1 tools/tests/test_project36_bridge_skybox.ps1 tools/capture_readme_media.ps1 tools/tests/test_capture_manifest_actions.ps1
git diff --cached --check
git commit -m "feat: prepare Bridge skybox for project 36 capture"
```

---

### Task 3: Replace retargeting with two direct embedded-animation instances

**Files:**
- Create: `tools/tests/test_project36_embedded_showcase.ps1`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_Utilities.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App.h`

**Interfaces:**
- `Impl::PortfolioAnimatorSlot` contains `int modelIndex`, `int animationIndex`, `const char* clipName`, and `bool initialized`.
- `Impl::PortfolioShowcaseRuntime` contains `bool initialized`, `bool bridgeReady`, `float timeSec`, and `std::array<PortfolioAnimatorSlot, 2> slots`.
- `Impl::m_PortfolioModelIndices` stores the two capture-only model indices and stays `{ -1, -1 }` in normal mode.
- `InitializePortfolioShowcase()` resolves exact names from each instance's `fbxBaseAnimator.GetNames()` and proves both entries share `SharedModelData` but have distinct `GetBoneCB()` buffers.
- `UpdatePortfolioShowcase(float dt)` advances each slot's own `ModelEntry::fbxBaseAnimator` directly and returns true only when both palettes are owned.
- `ChangeSkyboxDDS` and `ChangeIBLSkyBox` return `bool`; normal callers may ignore the result.

- [ ] **Step 1: Write the failing runtime source contract**

Create `test_project36_embedded_showcase.ps1` and assert all of the following:

```powershell
$fbxPathToken = 'L"..\\Resource\\fbx\\Public\\MyAlice\\Portfolio\\Alice_Swimsuit_white.fbx"'
$leftClip = '"Armature|Humanoid"'
$rightClip = '"Armature|Humanoid.001"'
$excludedClip = '"Armature|T-Pose"'
```

- capture mode contains exactly two `loadModel` calls using `$fbxPathToken`, while the normal-mode block retains the original player/enemy loads;
- generated clip keys/paths and `CharacterAnimator` do not occur in `App_PortfolioShowcase.inl` or the capture block of `App_Lifecycle.inl`;
- two slots exist and exact-name lookup assigns the two selected clips;
- initialization rejects zero duration and explicitly rejects `$excludedClip`;
- both entries compare equal `shared.get()` and unequal `fbxBaseAnimator.GetBoneCB()` after `EnsureBoneCB`;
- `UpdateAndUpload` is called on each slot entry's `fbxBaseAnimator`;
- HUD contains `EMBEDDED FBX ANIMATION`, `LEFT  Armature|Humanoid`, and `RIGHT Armature|Humanoid.001`, with no `CCD IK`, `BLEND`, or `4 CHARACTERS`;
- capture path names `..\Resource\Skybox\Bridge\bridge`, applies it after `m_->m_Skybox = new Skybox()`, and never names `Hanako.dds`;
- `WritePortfolioBackbufferPng` requires both `showcase.initialized` and `showcase.bridgeReady`;
- capture composition calls `ComputeLocalAABB`, derives scale from a 132.0f target height, uses two opposite x positions, and normal mode remains outside that branch.

- [ ] **Step 2: Run the contract and verify RED**

```powershell
pwsh -NoProfile -File tools/tests/test_project36_embedded_showcase.ps1
```

Expected: exit 1 because the current source still contains four generated-motion slots and IK/blend labels.

- [ ] **Step 3: Replace capture-only model loading and frame the pair**

In `App_Lifecycle.inl`, keep the existing normal-mode five-model load sequence unchanged. In README capture mode instead load:

```cpp
const std::wstring portfolioFbx =
    L"..\\Resource\\fbx\\Public\\MyAlice\\Portfolio\\Alice_Swimsuit_white.fbx";
const int leftIndex = loadModel(portfolioFbx, "portfolio left");
const int rightIndex = loadModel(portfolioFbx, "portfolio right");
const int groundIndex = loadModel(L"..\\Resource\\fbx\\Study\\Ground.fbx", "ground");
```

Do not load player/enemy GLBs in capture mode. Build `m_Objects` from the resulting entries as before. For the two FBX entries:

```cpp
XMFLOAT3 localMin{}, localMax{};
if (!ComputeLocalAABB(device, context, shared->vb, shared->stride, localMin, localMax))
    fail capture initialization;
const float localHeight = localMax.y - localMin.y;
const float targetHeight = 132.0f;
const float uniformScale = targetHeight / localHeight;
const float groundY = -2.0f;
const float x = targetHeight * 0.42f;
left.pos  = XMFLOAT3(-x, groundY - localMin.y * uniformScale, 0.0f);
right.pos = XMFLOAT3( x, groundY - localMin.y * uniformScale, 0.0f);
left.scale = right.scale = XMFLOAT3(uniformScale, uniformScale, uniformScale);
left.rotDeg = right.rotDeg = XMFLOAT3(0.0f, 0.0f, 0.0f);
```

Set the capture camera to `(0, groundY + targetHeight * 0.52f, -92)` with zero rotation and the existing 90-degree vertical FOV. Keep the ground and capture lighting; keep editor positions/camera in the normal branch.

Store `{ leftIndex, rightIndex }` in `m_PortfolioModelIndices`, set `m_CharModelIndex = -1`, and leave all three enemy indices at `-1` for capture mode. If either load fails, both entries do not share one cached model, `localHeight <= 0`, or `ComputeLocalAABB` fails, log `[ERR] Portfolio capture composition unavailable`, reset `m_PortfolioModelIndices` to `{ -1, -1 }`, and let `InitializePortfolioShowcase` remain false so the backbuffer writer cannot publish. Normal mode retains its current character/enemy indices and initialization.

- [ ] **Step 4: Replace the showcase state and initialize exact embedded clips**

Replace the four `CharacterAnimator` slots and phase/IK fields in `App_InternalTypes.inl` with the two-slot interfaces above.

In `InitializePortfolioShowcase`, after the existing backbuffer path initialization:

1. Return false outside README capture mode.
2. Bind left/right model indices from `m_PortfolioModelIndices` created by the capture lifecycle.
3. Require both `ModelEntry`s to be FBX, to share the same non-null `SharedModelData`, and to expose the expected source scene.
4. Find each exact clip name in its own `fbxBaseAnimator.GetNames()`; reject missing names, a selected name equal to `Armature|T-Pose`, or `GetClipDurationSec(index) <= 0`.
5. Call `SetCurrentIndex`, `SetTimeSec(0)`, `SetPlaying(true)`, and `EnsureBoneCB(device, 1023)` on each entry.
6. Require both bone buffers non-null and unequal.
7. Mark both slots initialized and log the exact clip names and durations.

`ResetPortfolioShowcase` sets `timeSec` and both animator times to zero.

- [ ] **Step 5: Advance the two per-instance FBX animators directly**

Rewrite `UpdatePortfolioShowcase(float dt)` to return false unless capture mode, both slots, and Bridge are ready. Handle the frame-zero synthetic click with `ResetPortfolioShowcase`, then for each slot call:

```cpp
entry.fbxBaseAnimator.SetPlaying(true);
entry.fbxBaseAnimator.UpdateAndUpload(
    m_->m_pDeviceContext, dt,
    entry.shared->fbx->GetScenePtr(),
    entry.shared->fbx->GetNodeIndexOfName(),
    entry.shared->fbx->GetBoneNames(),
    entry.shared->fbx->GetBoneOffsets(),
    entry.shared->fbx->GetGlobalInverse());
```

Increment `showcase.timeSec` only after both updates. Keep `showcaseOwnsPalettes` in `App_UpdateInput.inl`, but make its ownership loop cover the two new slots. Remove capture-only `RenderPortfolioShowcaseDebug` declarations/calls and all IK debug state.

- [ ] **Step 6: Apply Bridge only after creating the Skybox**

Change `ChangeSkyboxDDS` to return true only when `Skybox::ChangeDDS` succeeds. Change `ChangeIBLSkyBox` to return true only when all three IBL SRVs load and the exact `<prefix>EnvHDR.dds` loads; do not substitute `cubemap.dds` in README capture mode.

Remove the early unconditional Baker `ChangeIBLSkyBox` call. Immediately after `m_->m_Skybox = new Skybox()`, choose the initial prefix and pass its exact `EnvHDR.dds` path to `Skybox::Initialize`:

```cpp
const std::wstring initialIbl = IsReadmeCaptureMode()
    ? L"..\\Resource\\Skybox\\Bridge\\bridge"
    : L"..\\Resource\\Skybox\\Sample\\BakerSample";
const std::wstring initialEnv = initialIbl + L"EnvHDR.dds";
m_->m_Skybox->Initialize(device, initialEnv, skyVs, skyPs, skyLayout, constantBuffer);
const bool initialIblReady = ChangeIBLSkyBox(initialIbl);
```

Set `SkyBoxChoice::bridge` in capture mode and `SkyBoxChoice::Baker` otherwise. Store `initialIblReady` in `m_PortfolioShowcase.bridgeReady`; `InitializePortfolioShowcase` must preserve this field while resetting its other state later in `LoadDataAsync`. Keep normal asynchronous asset behavior and the normal-mode `cubemap.dds` fallback unchanged; only README capture refuses that fallback. The backbuffer writer returns without publishing when either `initialized` or `bridgeReady` is false, forcing the capture tool to time out instead of accepting gray output.

- [ ] **Step 7: Render the compact truthful HUD**

Keep the existing fixed-position translucent ImGui window and replace its content with exactly:

```text
EMBEDDED FBX ANIMATION
LEFT  Armature|Humanoid
RIGHT Armature|Humanoid.001
DIRECT PLAYBACK / NO RETARGETING
```

Do not render editor panels, IK lines, blend labels, or a phase timeline in capture mode.

- [ ] **Step 8: Run GREEN and rebuild Project 36**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_embedded_fbx_asset.ps1
pwsh -NoProfile -File tools/tests/test_project36_embedded_showcase.ps1
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
git diff --check
```

Expected: contracts exit 0; MSBuild exits 0 and refreshes `Dx11/bin/36_AdvancedAnim_Sound_Click.exe`.

- [ ] **Step 9: Run a normal-mode responsiveness smoke test**

Clear `DX11_README_CAPTURE` and `DX11_README_BACKBUFFER_PNG`, launch the `Dx11/bin` executable with `Dx11/bin` as working directory, wait eight seconds, and assert `HasExited=$false`, `Responding=$true`, and `MainWindowHandle -ne 0`. Stop only that process afterward.

- [ ] **Step 10: Commit Task 3**

```powershell
git add -- tools/tests/test_project36_embedded_showcase.ps1 Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl Dx11/36_AdvancedAnim_Sound_Click/App_Utilities.inl Dx11/36_AdvancedAnim_Sound_Click/App.h
git diff --cached --check
git commit -m "feat: play embedded FBX clips in project 36 showcase"
```

---

### Task 4: Define the 12-second two-character media contract

**Files:**
- Create: `tools/tests/test_project36_embedded_media.ps1`
- Remove: `tools/tests/test_project36_portfolio_media.ps1`
- Remove: `tools/tests/test_project36_portfolio_showcase.ps1`
- Modify: `tools/readme_media_manifest.json`
- Modify: `tools/tests/test_readme_media_manifest.ps1`
- Modify: `tools/tests/test_verify_readme_media.ps1`
- Modify: `tools/tests/test_visual_capture_contracts.ps1`

**Interfaces:**
- Project 36 manifest: `gifSeconds: 12`, `gifFps: 8` inherited, `readmeBackbufferCapture: true` retained.
- Produces: `test_project36_embedded_media.ps1 -PngPath <path> -GifPath <path>`.
- Preserves: all other projects' effective duration and every Project 36 media path/action.

- [ ] **Step 1: Write failing manifest and media assertions**

Update the three general tests to require Project 36 effective duration 12 seconds and every other project unchanged. Replace old phase assertions in `test_visual_capture_contracts.ps1` with calls to the new embedded showcase contract.

Create `test_project36_embedded_media.ps1` with these checks:

```powershell
Assert-ImageDimensions $PngPath 1600 900
Assert-ImageDimensions $GifPath 800 450
Assert-True ($gifFrameCount -ge 92 -and $gifFrameCount -le 100) 'expected about 96 frames'
Assert-True ([Math]::Abs($gifDurationSeconds - 12.0) -le 0.5) 'expected twelve-second GIF'
Assert-True ($gifBytes -le 5242880) 'GIF exceeds 5 MiB'
Assert-RegionMotion -Frames $frames -Rect @(80,45,320,390) -MinimumDistinct 16 -Label 'left Humanoid'
Assert-RegionMotion -Frames $frames -Rect @(400,45,320,390) -MinimumDistinct 16 -Label 'right Humanoid.001'
Assert-True ((Get-BackgroundVariance $PngPath) -ge 12.0) 'background is flat gray or missing'
```

Decode frames with ffmpeg to an ignored temporary directory and use SHA-256 of cropped pixel data for region motion. Sample four corner/upper-background rectangles for variance, excluding the HUD. OCR is not required; the runtime source contract owns exact HUD text. Delete the temporary frame directory in `finally` after proving it is contained under the test artifact root.

- [ ] **Step 2: Run tests and verify RED**

```powershell
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
pwsh -NoProfile -File tools/tests/test_verify_readme_media.ps1
pwsh -NoProfile -File tools/tests/test_visual_capture_contracts.ps1
pwsh -NoProfile -File tools/tests/test_project36_embedded_media.ps1
```

Expected: failure because the manifest/public GIF still describe the old eight-second showcase.

- [ ] **Step 3: Apply the minimal manifest/test change**

Change only Project 36 `"gifSeconds":8` to `"gifSeconds":12`. Keep its click reset, pre-capture click/wait, 1600x900 PNG path, 800x450 GIF path, and backbuffer flag. Remove the two obsolete portfolio PowerShell tests after their replacements cover every retained behavior.

- [ ] **Step 4: Run GREEN except for not-yet-published media**

```powershell
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
pwsh -NoProfile -File tools/tests/test_verify_readme_media.ps1
pwsh -NoProfile -File tools/tests/test_visual_capture_contracts.ps1
pwsh -NoProfile -File tools/capture_readme_media.ps1 -ValidateOnly
```

Expected: all four commands exit 0. `test_project36_embedded_media.ps1` remains intentionally RED against old public media until Task 5.

- [ ] **Step 5: Commit Task 4**

```powershell
git add -- tools/tests/test_project36_embedded_media.ps1 tools/tests/test_project36_portfolio_media.ps1 tools/tests/test_project36_portfolio_showcase.ps1 tools/readme_media_manifest.json tools/tests/test_readme_media_manifest.ps1 tools/tests/test_verify_readme_media.ps1 tools/tests/test_visual_capture_contracts.ps1
git diff --cached --check
git commit -m "test: define embedded FBX showcase media"
```

---

### Task 5: Capture, visually inspect, and publish Project 36 media

**Files:**
- Modify: `docs/media/readme/36-AdvancedAnim-Sound-Click.png`
- Modify: `docs/media/readme/36-advanced-anim-sound-click.gif`
- Modify: `docs/media/readme/info/36-AdvancedAnim-Sound-Click-info.png`
- Modify: `docs/media/readme/capture-report.md` (Project 36 rows only)

**Interfaces:**
- Consumes: the ignored Bridge preflight, two direct FBX animators, 12-second manifest override, and atomic backbuffer provider.
- Produces: the existing public Project 36 media paths only after staged verification passes.
- Preserves: `README.md` and `Dx11/36_AdvancedAnim_Sound_Click/README.md` byte-for-byte.

- [ ] **Step 1: Capture into the ignored staging directory**

Run:

```powershell
$artifactDir = '.superpowers/artifacts/project36-embedded-fbx-showcase'
pwsh -NoProfile -File tools/capture_readme_media.ps1 -ProjectNumber 36 -OutputDir $artifactDir
pwsh -NoProfile -File tools/tests/test_project36_embedded_media.ps1 -PngPath "$artifactDir/36-AdvancedAnim-Sound-Click.png" -GifPath "$artifactDir/36-advanced-anim-sound-click.gif"
```

Expected: Bridge is downloaded/extracted only if absent; the staged report contains two Success rows; PNG is 1600x900; GIF is 800x450, about 96 frames/12 seconds, and no more than 5 MiB.

- [ ] **Step 2: Generate deterministic review evidence**

Run:

```powershell
& 'C:\ffmpeg\bin\ffmpeg.exe' -y -i "$artifactDir/36-advanced-anim-sound-click.gif" -vf 'fps=1,scale=800:450,tile=4x3' "$artifactDir/contact-sheet.png"
```

Also extract the first, middle, and last GIF frames at 800x450. Keep all review evidence ignored.

- [ ] **Step 3: Inspect the PNG, GIF, and contact sheet at original resolution**

Require all of the following before publication:

- exactly two copies of the supplied character are visible;
- both are textured, front-facing, and naturally animated;
- left and right poses evolve independently throughout the GIF;
- ears, hair, hands, and feet remain inside the frame;
- silhouettes do not overlap at their widest poses;
- the pair occupies approximately 75% of frame height;
- the Bridge environment is clearly visible and not flat gray;
- the four exact HUD lines are readable and do not cover either character;
- no old enemy variant, IK line, blend label, editor panel, terminal, capture window, or debug Hanako image is visible.

If any item fails, leave public media untouched, add a measured failing assertion or adjust only the capture transforms/camera/HUD in Task 3, rebuild, and repeat Tasks 3 and 5 verification.

- [ ] **Step 4: Publish only approved staged outputs**

After visual approval, copy the staged PNG/GIF to their existing public paths and regenerate only Project 36's information image:

```powershell
Copy-Item -LiteralPath "$artifactDir/36-AdvancedAnim-Sound-Click.png" -Destination 'docs/media/readme/36-AdvancedAnim-Sound-Click.png' -Force
Copy-Item -LiteralPath "$artifactDir/36-advanced-anim-sound-click.gif" -Destination 'docs/media/readme/36-advanced-anim-sound-click.gif' -Force
pwsh -NoProfile -File tools/generate_readme_info_images.ps1 -ProjectNumber 36
```

Replace only the two `| 36 |` rows in `docs/media/readme/capture-report.md` with the staged Success rows, rewriting the ignored artifact prefix to `docs/media/readme/`. Preserve every other row byte-for-byte.

- [ ] **Step 5: Verify the published result**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_embedded_media.ps1
pwsh -NoProfile -File tools/tests/test_public_scene_media.ps1
pwsh -NoProfile -File tools/tests/test_readme_info_images.ps1
pwsh -NoProfile -File tools/verify_readme_media.ps1
git diff --exit-code -- README.md Dx11/36_AdvancedAnim_Sound_Click/README.md
git diff --check
```

Expected: every command exits 0 and neither README has a text diff.

- [ ] **Step 6: Commit Task 5**

```powershell
git add -- docs/media/readme/36-AdvancedAnim-Sound-Click.png docs/media/readme/36-advanced-anim-sound-click.gif docs/media/readme/info/36-AdvancedAnim-Sound-Click-info.png docs/media/readme/capture-report.md
git diff --cached --check
git commit -m "docs: show embedded FBX animation pair"
```

---

### Task 6: Fresh verification, review, and remote handoff

**Files:**
- Verify only; no expected tracked edits.
- Write ignored evidence to `.superpowers/artifacts/project36-embedded-fbx-showcase/final-verification.txt`.

**Interfaces:**
- Produces: clean, reviewed `codex/project36-fbx-showcase` pushed to `origin` without merging into `main`.

- [ ] **Step 1: Run the complete focused suite fail-fast**

```powershell
$tests = @(
  'tools/tests/test_project36_embedded_fbx_asset.ps1',
  'tools/tests/test_project36_bridge_skybox.ps1',
  'tools/tests/test_project36_embedded_showcase.ps1',
  'tools/tests/test_readme_media_manifest.ps1',
  'tools/tests/test_capture_manifest_actions.ps1',
  'tools/tests/test_verify_readme_media.ps1',
  'tools/tests/test_visual_capture_contracts.ps1',
  'tools/tests/test_project36_embedded_media.ps1',
  'tools/tests/test_public_scene_media.ps1',
  'tools/tests/test_readme_info_images.ps1'
)
foreach ($test in $tests) {
    pwsh -NoProfile -File $test
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
pwsh -NoProfile -File tools/verify_readme_media.ps1
```

Expected: every command exits 0.

- [ ] **Step 2: Rebuild and repeat the normal-mode smoke test**

Rebuild Project 36 Debug x64 with `/m:1`, then run the exact normal-mode responsiveness check from Task 3 Step 9. Record command, exit code, window title, responsiveness, and elapsed time.

- [ ] **Step 3: Audit scope, hashes, and prohibited paths**

Run:

```powershell
git diff --check a259e99..HEAD
git diff --name-status a259e99..HEAD
git status --short
Get-FileHash -Algorithm SHA256 'Z:\Alice_Swimsuit_white.fbx'
Get-FileHash -Algorithm SHA256 'Dx11/Resource/fbx/Public/MyAlice/Portfolio/Alice_Swimsuit_white.fbx'
rg -n -i 'anim_PortfolioDance|anim_PortfolioUpperWave|Hanako\.dds|CCD IK / LEFT HAND TARGET|4 CHARACTERS / LIVE PALETTES' Dx11/36_AdvancedAnim_Sound_Click tools/capture_readme_media.ps1 tools/project36_bridge_skybox.ps1
```

Expected: hashes are identical to the approved value; prohibited capture tokens have no active source references; only files enumerated by this plan changed; the working tree is clean.

Also run `Get-Item -Force` on the committed FBX and all four installed Bridge files and assert `Attributes` does not contain `ReparsePoint`.

- [ ] **Step 4: Perform one final media inspection**

Open the public 1600x900 PNG, public GIF, 4x3 contact sheet, and Project 36 information image. Confirm they match the approved staged media and the Bridge background/two independent clips remain visible.

- [ ] **Step 5: Request review and push the branch**

Invoke `superpowers:verification-before-completion`, then `superpowers:requesting-code-review`, then `superpowers:finishing-a-development-branch`. Push only after the review has no unresolved Critical or Important findings:

```powershell
git push -u origin codex/project36-fbx-showcase
```

Report the design, plan, implementation/media commit IDs, exact FBX hash, test/build results, visual evidence paths, and remote branch. Do not merge into `main`.
