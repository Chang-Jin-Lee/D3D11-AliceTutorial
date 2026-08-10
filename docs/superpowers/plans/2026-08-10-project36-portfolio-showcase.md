# Project 36 Portfolio Showcase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Project 36's representative PNG and GIF with a reproducible eight-second runtime showcase of four large current VRoid characters performing an original dance, animation blending/layering, and CCD IK.

**Architecture:** Add two deterministic animation-only GLB assets generated from the current VRoid bind transforms, then drive them through a Project 36-only `PortfolioShowcaseRuntime` gated by `IsReadmeCaptureMode()`. Extend the README media tooling with per-project GIF duration and an opt-in atomic backbuffer PNG provider so Project 36 can be captured reliably without changing the other 36 projects or normal application behavior.

**Tech Stack:** C++20, Direct3D 11, Assimp, existing `CharacterAnimator`/`FbxAnimation`, DirectXTK `ScreenGrab`, ImGui, Python 3 standard library, PowerShell 7, ffmpeg, MSBuild.

## Global Constraints

- Never copy, commit, render, trace, retarget, or derive output from the legacy NIKKE Alice model, its embedded named dance clips, or its audio.
- The new animation assets contain exactly one animation each and no mesh, skin, material, image, texture, or audio payload.
- Activate the showcase and backbuffer writer only when `IsReadmeCaptureMode()` is true; normal Project 36 behavior remains unchanged.
- Keep the existing root and Project 36 README markup and media paths unchanged.
- Capture PNG at 1600x900 and GIF at 800x450, 8 fps, approximately 8 seconds, within the existing 5 MiB GIF limit.
- Show exactly four current public `MyAlice` characters, front-facing and collectively occupying roughly 70% of the frame height.
- Capture into an ignored staging directory first. Publish public media only after automated and visual checks pass.
- Do not create junctions, symlinks, or reparse points. Do not touch the pre-existing untracked `.superpowers/` content except for ignored execution evidence.
- Do not modify `Dx11/Common/Animation`; Project 36 composes the existing public animation interfaces.

## File Structure

### New files

- `tools/generate_project36_portfolio_animations.py` — deterministic animation-only GLB generator.
- `tools/tests/test_project36_portfolio_assets.py` — structural, provenance, and byte-reproducibility tests for both GLBs.
- `tools/tests/test_project36_portfolio_showcase.ps1` — source contract for capture-only runtime wiring and rights exclusions.
- `tools/tests/test_project36_portfolio_media.ps1` — duration, dimensions, cadence, phase-motion, and PNG/GIF validation.
- `Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl` — isolated showcase state, animation update, HUD, IK debug lines, and backbuffer output.
- `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioDance.glb` — original full-body animation only.
- `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioUpperWave.glb` — original upper-body animation only.

### Modified files

- `tools/readme_media_common.ps1` — validate and resolve optional per-project GIF/backbuffer settings.
- `tools/readme_media_manifest.json` — Project 36 duration, reset action, and backbuffer provider flags.
- `tools/capture_readme_media.ps1` — use effective Project 36 duration and opt-in backbuffer PNG source.
- `tools/verify_readme_media.ps1` — validate each GIF against its effective duration/fps/max bytes.
- `tools/tests/test_readme_media_manifest.ps1` — override validation and default-preservation tests.
- `tools/tests/test_capture_manifest_actions.ps1` — Project 36 reset schedule and backbuffer-copy behavior tests.
- `tools/tests/test_verify_readme_media.ps1` — eight-second per-project verifier fixture.
- `tools/tests/test_visual_capture_contracts.ps1` — Project 36 showcase gate and reset-action contract.
- `Dx11/36_AdvancedAnim_Sound_Click/App.cpp` — include the new focused implementation file and DirectXTK ScreenGrab.
- `Dx11/36_AdvancedAnim_Sound_Click/App.h` — private showcase method declarations.
- `Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl` — `PortfolioShowcaseRuntime` state owned by `App::Impl`.
- `Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl` — load the two clips and initialize the capture-only composition.
- `Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl` — delegate capture-mode animation updates and reset clicks.
- `Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl` — render capture HUD/debug evidence and publish a backbuffer frame before `Present`.
- `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj` — list the new `.inl` in the project.
- `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj.filters` — list the new `.inl` under Header Files.
- `docs/media/readme/36-AdvancedAnim-Sound-Click.png` — refreshed representative still.
- `docs/media/readme/36-advanced-anim-sound-click.gif` — refreshed continuous showcase.
- `docs/media/readme/info/36-AdvancedAnim-Sound-Click-info.png` — regenerated information image.
- `docs/media/readme/capture-report.md` — replace only the two Project 36 success rows with the new measured output rows.

---

### Task 1: Per-project eight-second GIF contract

**Files:**
- Modify: `tools/readme_media_common.ps1:186-286`
- Modify: `tools/readme_media_manifest.json` (Project 36 entry)
- Modify: `tools/capture_readme_media.ps1:780-835, 869-960`
- Modify: `tools/verify_readme_media.ps1:222-315, 830-850`
- Modify: `tools/tests/test_readme_media_manifest.ps1`
- Modify: `tools/tests/test_capture_manifest_actions.ps1`
- Modify: `tools/tests/test_verify_readme_media.ps1`
- Modify: `tools/tests/test_visual_capture_contracts.ps1`

**Interfaces:**
- Produces: `Get-ReadmeMediaEffectivePositiveNumber([object]$Manifest, [object]$Project, [string]$Name)` returning the project override or global value.
- Produces: optional manifest properties `gifSeconds: 8` and `readmeBackbufferCapture: true` for Project 36.
- Produces: Project 36 `gifActions` containing one `{ atMs: 0, type: "click", x: 0.5, y: 0.5 }` reset action.
- Preserves: every project without an override uses global `gifSeconds=4`, `gifFps=8`, and `gifMaxBytes=5242880`.

- [ ] **Step 1: Write failing manifest and capture tests**

Add these assertions to `test_readme_media_manifest.ps1` and `test_capture_manifest_actions.ps1`:

```powershell
$project36 = @($manifest.projects | Where-Object number -eq '36')[0]
Assert-True ((Get-ReadmeMediaEffectivePositiveNumber $manifest $project36 'gifSeconds') -eq 8) 'project 36 must capture eight seconds'
Assert-True ([bool]$project36.readmeBackbufferCapture) 'project 36 must opt into backbuffer capture'

foreach ($project in @($manifest.projects | Where-Object number -ne '36')) {
    Assert-True ((Get-ReadmeMediaEffectivePositiveNumber $manifest $project 'gifSeconds') -eq 4) "project $($project.number) changed from four seconds"
}

$invalid = Copy-ReadmeMediaManifest $manifest
$invalid.projects[35].gifSeconds = 0
Assert-True ((Test-ReadmeMediaManifest $invalid $repoRoot) -contains 'invalid gifSeconds override: 36') 'zero override was accepted'

$resetActions = @($project36.gifActions)
Assert-True ($resetActions.Count -eq 1) 'project 36 needs exactly one GIF reset action'
Assert-True ($resetActions[0].type -eq 'click' -and [int]$resetActions[0].atMs -eq 0) 'project 36 reset must be a click at frame zero'
```

In `test_verify_readme_media.ps1`, create a fixture project with `gifSeconds = 8`, create 64 moving frames at 8 fps, and assert that the verifier accepts it while still rejecting the same GIF when the override is removed.

- [ ] **Step 2: Run the focused tests and verify RED**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
pwsh -NoProfile -File tools/tests/test_capture_manifest_actions.ps1
pwsh -NoProfile -File tools/tests/test_verify_readme_media.ps1
```

Expected: failure because the effective-setting helper and Project 36 overrides do not exist and the verifier still enforces 3.5-5.5 seconds.

- [ ] **Step 3: Implement effective project settings**

Add this helper to `readme_media_common.ps1`:

```powershell
function Get-ReadmeMediaEffectivePositiveNumber {
    param(
        [Parameter(Mandatory)] [object] $Manifest,
        [Parameter(Mandatory)] [object] $Project,
        [Parameter(Mandatory)] [string] $Name
    )
    if (Test-ReadmeMediaManifestProperty -Object $Project -Name $Name) {
        return [double]$Project.$Name
    }
    return [double]$Manifest.$Name
}
```

Validate optional project `gifSeconds`, `gifFps`, and `gifMaxBytes` with the same positive-number rules as global values. Validate `readmeBackbufferCapture` as Boolean. Add to Project 36:

```json
"gifSeconds": 8,
"readmeBackbufferCapture": true,
"gifActions": [
  { "atMs": 0, "type": "click", "x": 0.5, "y": 0.5 }
]
```

Use the helper in `Invoke-GifCapture`, the presentation-pan call, and `Test-GifMedia`. Change the verifier signature to:

```powershell
function Test-GifMedia {
    param(
        [string]$Path,
        [string]$Label,
        [int]$ExpectedFps,
        [double]$ExpectedSeconds,
        [int64]$ExpectedMaxBytes,
        [System.Collections.Generic.List[string]]$Errors
    )
}
```

Accept decoded duration only when `Abs(actual - ExpectedSeconds) -le 0.5`; use `ExpectedMaxBytes` instead of the hard-coded size.

- [ ] **Step 4: Run focused and visual capture contracts**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_readme_media_manifest.ps1
pwsh -NoProfile -File tools/tests/test_capture_manifest_actions.ps1
pwsh -NoProfile -File tools/tests/test_verify_readme_media.ps1
pwsh -NoProfile -File tools/tests/test_visual_capture_contracts.ps1
pwsh -NoProfile -File tools/capture_readme_media.ps1 -ValidateOnly
```

Expected: all five commands exit 0 and Project 36 resolves to 8 seconds/8 fps while Project 01 resolves to 4 seconds/8 fps.

- [ ] **Step 5: Commit Task 1**

```powershell
git add -- tools/readme_media_common.ps1 tools/readme_media_manifest.json tools/capture_readme_media.ps1 tools/verify_readme_media.ps1 tools/tests/test_readme_media_manifest.ps1 tools/tests/test_capture_manifest_actions.ps1 tools/tests/test_verify_readme_media.ps1 tools/tests/test_visual_capture_contracts.ps1
git diff --cached --check
git commit -m "feat: support project-specific README GIF duration"
```

---

### Task 2: Original animation-only GLB assets

**Files:**
- Create: `tools/generate_project36_portfolio_animations.py`
- Create: `tools/tests/test_project36_portfolio_assets.py`
- Create: `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioDance.glb`
- Create: `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioUpperWave.glb`

**Interfaces:**
- Consumes: `SampleModel.glb` node names and bind-local translation/rotation values only.
- Produces: CLI `python tools/generate_project36_portfolio_animations.py --source <glb> --output-dir <dir>`.
- Produces: animation names `PortfolioDance` and `PortfolioUpperWave`, duration 4.0s and 2.0s respectively.
- Produces: byte-identical output for identical input.

- [ ] **Step 1: Write the failing asset test**

Create `test_project36_portfolio_assets.py` with a standard-library GLB parser and these checks:

```python
EXPECTED = {
    "anim_PortfolioDance.glb": ("PortfolioDance", 4.0),
    "anim_PortfolioUpperWave.glb": ("PortfolioUpperWave", 2.0),
}
BANNED = (b"NIKKE", b"Alice_.fbx", b"CaramellaDansen", b"RabbitHole", b"Specialist", b"CaliforniaGirls")

for filename, (animation_name, duration) in EXPECTED.items():
    doc, binary = read_glb(ANIMATION_DIR / filename)
    assert len(doc.get("animations", [])) == 1
    assert doc["animations"][0]["name"] == animation_name
    for forbidden in ("meshes", "skins", "materials", "textures", "images", "audio"):
        assert not doc.get(forbidden)
    assert {node["name"] for node in doc["nodes"]} == current_skeleton_closure_names
    assert all(child < len(doc["nodes"]) for node in doc["nodes"] for child in node.get("children", []))
    assert not any(token.lower() in (json.dumps(doc).encode() + binary).lower() for token in BANNED)

with tempfile.TemporaryDirectory() as tmp:
    subprocess.run([sys.executable, str(GENERATOR), "--source", str(SOURCE), "--output-dir", tmp], check=True)
    for filename in EXPECTED:
        assert (Path(tmp) / filename).read_bytes() == (ANIMATION_DIR / filename).read_bytes()
```

Also decode the time accessor and assert the first time is 0, the last time equals the declared duration, and times are strictly increasing.

- [ ] **Step 2: Run the test and verify RED**

Run:

```powershell
python tools/tests/test_project36_portfolio_assets.py
```

Expected: failure because the generator and animation GLBs do not exist.

- [ ] **Step 3: Implement the deterministic generator**

Implement GLB read/write helpers using `json`, `struct`, and `pathlib`. Read `skins[0].joints`, add every ancestor needed to reach the source scene root, and copy that complete skeleton closure into each output while preserving child relationships and bind-local translation/rotation/scale. Do not copy mesh references or skin objects. Multiply each animated target node's bind quaternion by authored delta quaternions and emit LINEAR rotation/translation channels; unanimated skeleton nodes retain their copied bind-local transforms.

Use these exact keyframe definitions as the original motion source:

```python
DANCE_TIMES = [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0]
DANCE_EULER_DEG = {
    "J_Bip_C_Hips":       [(0,0,0), (0,10,7), (0,0,0), (0,-10,-7), (0,0,0), (0,10,7), (0,0,0), (0,-10,-7), (0,0,0)],
    "J_Bip_C_Spine":      [(0,0,0), (4,-8,-5), (0,0,0), (4,8,5), (0,0,0), (4,-8,-5), (0,0,0), (4,8,5), (0,0,0)],
    "J_Bip_C_Chest":      [(0,0,0), (-3,-10,-8), (0,0,0), (-3,10,8), (0,0,0), (-3,-10,-8), (0,0,0), (-3,10,8), (0,0,0)],
    "J_Bip_L_UpperArm":   [(0,0,0), (-20,0,-35), (-110,0,-70), (-20,0,-35), (0,0,0), (-20,0,-35), (-110,0,-70), (-20,0,-35), (0,0,0)],
    "J_Bip_R_UpperArm":   [(0,0,0), (-110,0,70), (-20,0,35), (-110,0,70), (0,0,0), (-110,0,70), (-20,0,35), (-110,0,70), (0,0,0)],
    "J_Bip_L_LowerArm":   [(0,0,0), (0,0,-25), (0,0,-55), (0,0,-25), (0,0,0), (0,0,-25), (0,0,-55), (0,0,-25), (0,0,0)],
    "J_Bip_R_LowerArm":   [(0,0,0), (0,0,55), (0,0,25), (0,0,55), (0,0,0), (0,0,55), (0,0,25), (0,0,55), (0,0,0)],
    "J_Bip_L_UpperLeg":   [(0,0,0), (0,0,8), (0,0,-5), (0,0,2), (0,0,0), (0,0,8), (0,0,-5), (0,0,2), (0,0,0)],
    "J_Bip_R_UpperLeg":   [(0,0,0), (0,0,-2), (0,0,5), (0,0,-8), (0,0,0), (0,0,-2), (0,0,5), (0,0,-8), (0,0,0)],
}
DANCE_HIPS_X = [0.0, 0.08, 0.0, -0.08, 0.0, 0.08, 0.0, -0.08, 0.0]

UPPER_TIMES = [0.0, 0.5, 1.0, 1.5, 2.0]
UPPER_EULER_DEG = {
    "J_Bip_C_Chest":     [(0,0,0), (-5,-12,-5), (0,0,0), (-5,12,5), (0,0,0)],
    "J_Bip_L_Shoulder":  [(0,0,0), (0,0,-12), (0,0,-20), (0,0,-12), (0,0,0)],
    "J_Bip_R_Shoulder":  [(0,0,0), (0,0,12), (0,0,20), (0,0,12), (0,0,0)],
    "J_Bip_L_UpperArm":  [(0,0,0), (-45,0,-55), (-75,0,-90), (-45,0,-55), (0,0,0)],
    "J_Bip_R_UpperArm":  [(0,0,0), (-75,0,90), (-45,0,55), (-75,0,90), (0,0,0)],
    "J_Bip_L_LowerArm":  [(0,0,0), (0,0,-35), (0,0,-65), (0,0,-35), (0,0,0)],
    "J_Bip_R_LowerArm":  [(0,0,0), (0,0,65), (0,0,35), (0,0,65), (0,0,0)],
}
```

Write accessors as little-endian float32, align JSON and BIN chunks to four bytes, set `asset.version` to `2.0`, and set each animation sampler's interpolation to `LINEAR`.

- [ ] **Step 4: Generate assets and run reproducibility tests**

Run:

```powershell
python tools/generate_project36_portfolio_animations.py --source Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb --output-dir Dx11/Resource/fbx/Public/MyAlice/Animations
python tools/tests/test_project36_portfolio_assets.py
```

Expected: the test reports both clips, one animation per file, no render payload, exact node-name subset, and byte-identical regeneration.

- [ ] **Step 5: Commit Task 2**

```powershell
git add -- tools/generate_project36_portfolio_animations.py tools/tests/test_project36_portfolio_assets.py Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioDance.glb Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioUpperWave.glb
git diff --cached --check
git commit -m "feat: add original portfolio animation clips"
```

---

### Task 3: Capture-only showcase controller

**Files:**
- Create: `tools/tests/test_project36_portfolio_showcase.ps1`
- Create: `Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App.cpp`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App.h`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj.filters`

**Interfaces:**
- Produces: `bool App::InitializePortfolioShowcase()`.
- Produces: `void App::ResetPortfolioShowcase()`.
- Produces: `bool App::UpdatePortfolioShowcase(float dt)`; returns true only when it owns capture-mode character palettes.
- Produces: `void App::RenderPortfolioShowcaseDebug()` and `void App::RenderPortfolioShowcaseHud()`.
- Consumes: external clip keys `PortfolioDance`, `PortfolioUpperWave`, `Idle`, and `Walk`.
- Consumes: four model indices: player followed by the three existing enemy indices.

- [ ] **Step 1: Write the failing showcase source contract**

Create `test_project36_portfolio_showcase.ps1` and assert exact ownership/gating:

```powershell
$app = Get-Content -Raw "$projectDir/App.cpp"
$lifecycle = Get-Content -Raw "$projectDir/App_Lifecycle.inl"
$update = Get-Content -Raw "$projectDir/App_UpdateInput.inl"
$render = Get-Content -Raw "$projectDir/App_RenderPasses.inl"
$showcase = Get-Content -Raw "$projectDir/App_PortfolioShowcase.inl"

Assert-True ($app -match '#include\s+"App_PortfolioShowcase\.inl"') 'showcase implementation not included'
Assert-True ($lifecycle -match 'InitializePortfolioShowcase\(\)') 'showcase not initialized after model loading'
Assert-True ($update -match 'UpdatePortfolioShowcase\(dt\)') 'showcase does not own capture animation updates'
Assert-True ($showcase -match 'IsReadmeCaptureMode\(\)') 'showcase lacks capture gate'
Assert-True ($showcase -match 'PortfolioDance' -and $showcase -match 'PortfolioUpperWave') 'original clips not used'
Assert-True ($showcase -match 'J_Bip_L_Hand' -and $showcase -match 'chainLen\s*=\s*3') 'CCD IK contract missing'
Assert-True ($showcase -match 'DANCE / SKINNED ANIMATION' -and $showcase -match 'ANIMATION BLEND \+ LAYER' -and $showcase -match 'CCD IK') 'HUD phases missing'
Assert-True ($showcase -notmatch '(?i)NIKKE|Alice_\.fbx|CaramellaDansen|RabbitHole|Specialist|CaliforniaGirls') 'legacy rights boundary violated'
```

Also assert that `App_UpdateInput.inl` still contains the existing normal advanced-rig branch and that the showcase branch does not execute unless capture mode is enabled.

- [ ] **Step 2: Run the contract and verify RED**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_portfolio_showcase.ps1
```

Expected: failure because `App_PortfolioShowcase.inl` and its wiring do not exist.

- [ ] **Step 3: Add focused runtime state and declarations**

Add these shapes to `App_InternalTypes.inl` inside `App::Impl`:

```cpp
enum class PortfolioShowcasePhase { Dance, BlendLayer, Ik, Finish };

struct PortfolioAnimatorSlot {
    int modelIndex = -1;
    CharacterAnimator animator;
    float phaseOffsetSec = 0.0f;
    bool initialized = false;
};

struct PortfolioShowcaseRuntime {
    bool initialized = false;
    bool fallbackToIdle = false;
    float timeSec = 0.0f;
    PortfolioShowcasePhase phase = PortfolioShowcasePhase::Dance;
    std::array<PortfolioAnimatorSlot, 4> slots{};
    DirectX::XMFLOAT3 ikTargetMS{ -0.35f, 1.15f, 0.18f };
    DirectX::XMFLOAT3 ikShoulderWS{};
    DirectX::XMFLOAT3 ikElbowWS{};
    DirectX::XMFLOAT3 ikHandWS{};
    DirectX::XMFLOAT3 ikTargetWS{};
    bool ikDebugValid = false;
};

PortfolioShowcaseRuntime m_PortfolioShowcase;
```

Declare the five App methods listed in Interfaces. Include `App_PortfolioShowcase.inl` after `App_Utilities.inl`, and list it in the vcxproj/filter beside the other App `.inl` files.

- [ ] **Step 4: Initialize only the capture composition**

In `App_Lifecycle.inl`, load the two GLBs with `ExternalAnimationClipTransform::None`, then call `InitializePortfolioShowcase()` after all four model indices and external clips are ready.

For capture mode only, set:

```cpp
const std::array<XMFLOAT3, 4> positions = {
    XMFLOAT3{ 0.0f, 0.0f, 5.0f },
    XMFLOAT3{ -92.0f, 0.0f, 48.0f },
    XMFLOAT3{ 0.0f, 0.0f, 62.0f },
    XMFLOAT3{ 92.0f, 0.0f, 48.0f }
};
for (size_t i = 0; i < slots.size(); ++i) {
    model->pos = positions[i];
    model->rotDeg = XMFLOAT3(0.0f, 180.0f, 0.0f);
    model->scale = XMFLOAT3(80.0f, 80.0f, 80.0f);
    model->autoRotate = false;
}
m_Camera.SetPosition(XMFLOAT3(0.0f, 72.0f, -165.0f));
m_Camera.SetRotation(XMFLOAT3(5.0f, 0.0f, 0.0f));
```

Do not create the two decorative cube objects in capture mode. Initialize one `CharacterAnimator` per valid FBX model using that model's scene, node map, global inverse, bone names, and bone offsets. If either portfolio clip fails, set `fallbackToIdle=true`, log a warning, and keep the four-character scene alive.

- [ ] **Step 5: Implement the deterministic eight-second update**

Use `fmod(timeSec, 8.0f)` and these phase boundaries:

```cpp
if (t < 3.0f)      phase = PortfolioShowcasePhase::Dance;
else if (t < 5.2f) phase = PortfolioShowcasePhase::BlendLayer;
else if (t < 7.5f) phase = PortfolioShowcasePhase::Ik;
else               phase = PortfolioShowcasePhase::Finish;
```

For each slot, build `CharacterAnimator::UpdateDesc` with its own time offset `{0.0f, 0.17f, 0.31f, 0.46f}`. The main slot uses:

```cpp
desc.base.enabled = true;
desc.base.animA = dance;
desc.base.animB = dance;
desc.base.timeA = desc.base.timeB = danceTime;

// 3.0-4.1: Dance -> Walk, 4.1-5.2: Walk -> Dance.
desc.base.animA = firstHalf ? dance : walk;
desc.base.animB = firstHalf ? walk : dance;
desc.base.blend01 = SmoothStep(localHalfTime);

desc.upper.enabled = phase == PortfolioShowcasePhase::BlendLayer;
desc.upper.animA = upperWave;
desc.upper.animB = upperWave;
desc.upper.timeA = desc.upper.timeB = upperTime;
desc.upper.layerAlpha = std::sin(layer01 * XM_PI);

desc.ik.enabled = phase == PortfolioShowcasePhase::Ik;
desc.ik.tipBone = "J_Bip_L_Hand";
desc.ik.chainLen = 3;
desc.ik.targetMS = XMVectorSet(
    -0.32f + 0.18f * std::cos(ikTime * XM_2PI),
     1.08f + 0.16f * std::sin(ikTime * XM_2PI),
     0.20f, 1.0f);
desc.ik.weight = SmoothStep(ikWeight);
```

Call `slot.animator.Update(desc)`, then upload `slot.animator.finalTransforms` through the corresponding `ModelEntry::fbxBaseAnimator`. Companion slots keep the dance base during blend and IK phases.

Reset `timeSec` to zero when capture mode receives the manifest's synthetic left-click press. Return true so only the existing general animation loop is skipped; camera matrices, rendering, and application responsiveness continue normally.

- [ ] **Step 6: Render verifiable HUD and IK evidence**

In `PassUI`, call `RenderPortfolioShowcaseHud()` instead of the full editor panels during capture mode. Use an input-free, title-free ImGui window at `(24,24)` with the exact phase labels from Step 1 and `4 CHARACTERS / LIVE PALETTES` as a second line.

Recover shoulder/elbow/hand node globals inside Project 36 from the palette equation:

```cpp
nodeGlobal = XMMatrixInverse(nullptr, XMLoadFloat4x4(&globalInverse))
           * animator.finalTransforms[boneIndex]
           * XMMatrixInverse(nullptr, XMLoadFloat4x4(&boneOffsets[boneIndex]));
worldPosition = XMVector3TransformCoord(nodeGlobal.r[3], modelWorld);
```

During the IK phase, draw shoulder-to-elbow, elbow-to-hand, and hand-to-target lines plus a three-axis target cross with `LineRenderer`. Set `ikDebugValid` only when all required bones resolve.

- [ ] **Step 7: Run contract and Debug x64 build**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_portfolio_showcase.ps1
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
```

Expected: contract exits 0; build exits 0 and refreshes both `Dx11/x64/Debug/36_AdvancedAnim_Sound_Click.exe` and the normal post-build `Dx11/bin` executable.

- [ ] **Step 8: Commit Task 3**

```powershell
git add -- tools/tests/test_project36_portfolio_showcase.ps1 Dx11/36_AdvancedAnim_Sound_Click/App.cpp Dx11/36_AdvancedAnim_Sound_Click/App.h Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj.filters
git diff --cached --check
git commit -m "feat: stage project 36 portfolio showcase"
```

---

### Task 4: Reliable opt-in backbuffer capture

**Files:**
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App.cpp`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App.h`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl`
- Modify: `tools/capture_readme_media.ps1`
- Modify: `tools/tests/test_capture_manifest_actions.ps1`
- Modify: `tools/tests/test_project36_portfolio_showcase.ps1`

**Interfaces:**
- Consumes: `DX11_README_BACKBUFFER_PNG`, set only for manifest projects with `readmeBackbufferCapture=true`.
- Produces: `void App::WritePortfolioBackbufferPng()` called after `PassUI()` and before `Present()`.
- Produces: `Copy-ReadmeBackbufferPng([string]$SourcePath, [string]$OutputPath, [int]$Width, [int]$Height)` in the capture tool.
- Preserves: `CopyFromScreen` for all projects without the opt-in flag.

- [ ] **Step 1: Write failing provider tests**

Extend `test_capture_manifest_actions.ps1` with a temporary 1600x900 PNG and call the new helper:

```powershell
$source = Join-Path $tempRoot 'backbuffer.png'
$destination = Join-Path $tempRoot 'copied.png'
New-TestPng -Path $source -Width 1600 -Height 900
Copy-ReadmeBackbufferPng -SourcePath $source -OutputPath $destination -Width 1600 -Height 900
$details = Get-CaptureOutputDetails $destination 1600 900
Assert-True ($details.Dimensions -eq '1600x900') 'backbuffer PNG provider changed dimensions'
```

Extend the showcase contract to require `DX11_README_BACKBUFFER_PNG`, `SaveWICTextureToFile`, a temporary sibling file, and `MoveFileExW(...MOVEFILE_REPLACE_EXISTING...)`.

- [ ] **Step 2: Run provider tests and verify RED**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_capture_manifest_actions.ps1
pwsh -NoProfile -File tools/tests/test_project36_portfolio_showcase.ps1
```

Expected: failure because neither provider implementation exists.

- [ ] **Step 3: Implement atomic Project 36 backbuffer output**

Include `<directxtk/ScreenGrab.h>` in `App.cpp`. On capture-only initialization, read `DX11_README_BACKBUFFER_PNG` once into a runtime `std::wstring`; leave it empty if the environment variable is missing.

Throttle writes to at most 12 fps. `WritePortfolioBackbufferPng()` gets swap-chain buffer 0, writes `requestedPath + L".tmp.png"` with `DirectX::SaveWICTextureToFile`, then atomically replaces the requested path:

```cpp
ComPtr<ID3D11Texture2D> backBuffer;
if (FAILED(m_->m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())))) return;
const std::wstring temporary = m_->m_PortfolioBackbufferPath + L".tmp.png";
if (SUCCEEDED(DirectX::SaveWICTextureToFile(
        m_->m_pDeviceContext, backBuffer.Get(), GUID_ContainerFormatPng, temporary.c_str()))) {
    MoveFileExW(temporary.c_str(), m_->m_PortfolioBackbufferPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
```

Call it after `PassUI()` and before `Present()`. Never call it outside README capture mode.

- [ ] **Step 4: Teach the capture tool to consume the provider**

For an opted-in project, create a unique contained path under the selected output/media directory before launch, set both `DX11_README_CAPTURE=1` and `DX11_README_BACKBUFFER_PNG=<path>` for `Start-Process`, then restore both parent environment values immediately after launch.

Add `BackbufferPath` to the capture session. `Capture-PreparedWindowPng` calls `Copy-ReadmeBackbufferPng` when the path is present; otherwise it retains `Graphics.CopyFromScreen`. The copy helper retries for up to three seconds, opens the atomically published PNG with `FileShare.ReadWrite`, clones it into a detached bitmap, validates exact dimensions, and saves the requested output as PNG. Remove only the exact generated temporary backbuffer path in `finally` after verifying it stays under the chosen media directory.

- [ ] **Step 5: Run tests, validation, and rebuild**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_capture_manifest_actions.ps1
pwsh -NoProfile -File tools/tests/test_project36_portfolio_showcase.ps1
pwsh -NoProfile -File tools/capture_readme_media.ps1 -ValidateOnly
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
```

Expected: all tests and validation exit 0; the project rebuild exits 0.

- [ ] **Step 6: Commit Task 4**

```powershell
git add -- Dx11/36_AdvancedAnim_Sound_Click/App.cpp Dx11/36_AdvancedAnim_Sound_Click/App.h Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl tools/capture_readme_media.ps1 tools/tests/test_capture_manifest_actions.ps1 tools/tests/test_project36_portfolio_showcase.ps1
git diff --cached --check
git commit -m "feat: capture project 36 from the backbuffer"
```

---

### Task 5: Capture, inspect, and publish the representative media

**Files:**
- Create: `tools/tests/test_project36_portfolio_media.ps1`
- Modify: `docs/media/readme/36-AdvancedAnim-Sound-Click.png`
- Modify: `docs/media/readme/36-advanced-anim-sound-click.gif`
- Modify: `docs/media/readme/info/36-AdvancedAnim-Sound-Click-info.png`
- Modify: `docs/media/readme/capture-report.md` (Project 36 rows only)

**Interfaces:**
- Consumes: Project 36 click-at-zero reset and eight-second capture contract.
- Produces: public PNG/GIF at their existing README paths only after staged media passes.
- Produces: `test_project36_portfolio_media.ps1 -PngPath <path> -GifPath <path>`.

- [ ] **Step 1: Write the failing media test against the current four-second GIF**

The test must use `System.Drawing` and ffprobe/ffmpeg where needed and assert:

```powershell
param(
    [string]$PngPath = 'docs/media/readme/36-AdvancedAnim-Sound-Click.png',
    [string]$GifPath = 'docs/media/readme/36-advanced-anim-sound-click.gif'
)

Assert-ImageDimensions $PngPath 1600 900
Assert-ImageDimensions $GifPath 800 450
Assert-True ($gifFrameCount -eq 64) "expected 64 frames, found $gifFrameCount"
Assert-True ([Math]::Abs($gifDurationSeconds - 8.0) -le 0.5) "expected eight-second GIF, found $gifDurationSeconds"
Assert-True ($gifBytes -le 5242880) 'GIF exceeds 5 MiB'
Assert-PhaseMotion -Frames $decodedFrames -Start 0  -End 23 -Label 'dance'
Assert-PhaseMotion -Frames $decodedFrames -Start 24 -End 41 -Label 'blend-layer'
Assert-PhaseMotion -Frames $decodedFrames -Start 42 -End 59 -Label 'ccd-ik'
```

`Assert-PhaseMotion` hashes the central model region of every frame and requires at least four distinct hashes per phase. Require the PNG and first GIF frame to be nonblank with sampled luminance variance above the existing verifier threshold.

- [ ] **Step 2: Run against current public media and verify RED**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_portfolio_media.ps1
```

Expected: failure because the current GIF has 32 frames and approximately four seconds.

- [ ] **Step 3: Capture to an ignored staging directory**

Run:

```powershell
$artifactDir = '.superpowers/artifacts/project36-portfolio-showcase'
pwsh -NoProfile -File tools/capture_readme_media.ps1 -ProjectNumber 36 -OutputDir $artifactDir
pwsh -NoProfile -File tools/tests/test_project36_portfolio_media.ps1 -PngPath "$artifactDir/36-AdvancedAnim-Sound-Click.png" -GifPath "$artifactDir/36-advanced-anim-sound-click.gif"
```

Expected: capture report contains two Project 36 Success rows, the media test exits 0, the PNG is 1600x900, and the GIF is 800x450/64 frames/about 8 seconds.

- [ ] **Step 4: Perform visual QA before public replacement**

Generate an eight-tile contact sheet in the ignored artifact directory:

```powershell
& 'C:\ffmpeg\bin\ffmpeg.exe' -y -i "$artifactDir/36-advanced-anim-sound-click.gif" -vf 'fps=1,scale=800:450,tile=4x2' "$artifactDir/contact-sheet.png"
```

Inspect the staged PNG, GIF, and contact sheet at original resolution. Require all of the following:

- all four characters are visible in every phase;
- the main character and companions face the camera;
- no ear, hand, foot, or hair is cropped;
- the characters collectively fill about 70% of the frame height;
- `DANCE / SKINNED ANIMATION`, `ANIMATION BLEND + LAYER`, and `CCD IK` are readable;
- the IK target and chain move with the main character's hand;
- no editor, terminal, capture window, NIKKE asset, or legacy character is visible.

If any requirement fails, do not copy staged media to `docs/media`; add a failing assertion or measured layout correction to Task 3, rebuild, and repeat Steps 3-4.

- [ ] **Step 5: Publish verified media and regenerate the information image**

After visual approval, copy only the staged PNG/GIF into their existing public paths, then run:

```powershell
Copy-Item -LiteralPath "$artifactDir/36-AdvancedAnim-Sound-Click.png" -Destination 'docs/media/readme/36-AdvancedAnim-Sound-Click.png' -Force
Copy-Item -LiteralPath "$artifactDir/36-advanced-anim-sound-click.gif" -Destination 'docs/media/readme/36-advanced-anim-sound-click.gif' -Force
pwsh -NoProfile -File tools/generate_readme_info_images.ps1 -ProjectNumber 36
```

Replace only the two `| 36 |` rows in `capture-report.md` with the exact successful rows from the staged report, rewriting the output prefix to `docs/media/readme/`. Preserve all other project rows byte-for-byte.

- [ ] **Step 6: Verify published media and README references**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_portfolio_media.ps1
pwsh -NoProfile -File tools/tests/test_public_scene_media.ps1
pwsh -NoProfile -File tools/verify_readme_media.ps1
git diff --exit-code -- README.md Dx11/36_AdvancedAnim_Sound_Click/README.md
```

Expected: all media/verifier commands exit 0 and both README files have no text diff.

- [ ] **Step 7: Commit Task 5**

```powershell
git add -- tools/tests/test_project36_portfolio_media.ps1 docs/media/readme/36-AdvancedAnim-Sound-Click.png docs/media/readme/36-advanced-anim-sound-click.gif docs/media/readme/info/36-AdvancedAnim-Sound-Click-info.png docs/media/readme/capture-report.md
git diff --cached --check
git commit -m "docs: showcase project 36 animation systems"
```

---

### Task 6: Final regression, provenance audit, and handoff

**Files:**
- Verify only; no expected production edits.
- Write ignored evidence to `.superpowers/artifacts/project36-portfolio-showcase/final-verification.txt`.

**Interfaces:**
- Consumes: all prior task commits.
- Produces: evidence that normal mode, capture mode, rights boundary, media, and repository scope pass together.

- [ ] **Step 1: Run the focused and media regression suite**

Run in a fail-fast PowerShell loop:

```powershell
$tests = @(
  'tools/tests/test_project36_portfolio_assets.py',
  'tools/tests/test_readme_media_manifest.ps1',
  'tools/tests/test_capture_manifest_actions.ps1',
  'tools/tests/test_verify_readme_media.ps1',
  'tools/tests/test_visual_capture_contracts.ps1',
  'tools/tests/test_project36_portfolio_showcase.ps1',
  'tools/tests/test_project36_portfolio_media.ps1',
  'tools/tests/test_public_scene_media.ps1',
  'tools/tests/test_readme_info_images.ps1'
)
python $tests[0]
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
foreach ($test in $tests[1..($tests.Count-1)]) {
  pwsh -NoProfile -File $test
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
pwsh -NoProfile -File tools/verify_readme_media.ps1
```

Expected: every command exits 0.

- [ ] **Step 2: Rebuild and run a normal-mode responsiveness smoke test**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
$oldCapture = $env:DX11_README_CAPTURE
$oldBackbuffer = $env:DX11_README_BACKBUFFER_PNG
$process = $null
try {
    Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue
    Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue
    $process = Start-Process -FilePath 'C:\Github\D3D11-AliceTutorial\Dx11\bin\36_AdvancedAnim_Sound_Click.exe' -WorkingDirectory 'C:\Github\D3D11-AliceTutorial\Dx11\bin' -PassThru
    Start-Sleep -Seconds 8
    $process.Refresh()
    if ($process.HasExited -or -not $process.Responding -or $process.MainWindowHandle -eq 0) {
        throw 'normal-mode smoke run failed'
    }
}
finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
    }
    if ($null -eq $oldCapture) { Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue }
    else { $env:DX11_README_CAPTURE = $oldCapture }
    if ($null -eq $oldBackbuffer) { Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue }
    else { $env:DX11_README_BACKBUFFER_PNG = $oldBackbuffer }
}
```

Expected: build exits 0; normal-mode application owns a responsive visible main window after eight seconds.

- [ ] **Step 3: Audit rights boundary and exact repository scope**

Run:

```powershell
$rightsMatches = rg -a -n -i 'NIKKE|Alice_\.fbx|CaramellaDansen|RabbitHole|Specialist|CaliforniaGirls|Caramel_Dance|MeniShuki' Dx11/36_AdvancedAnim_Sound_Click tools/generate_project36_portfolio_animations.py Dx11/Resource/fbx/Public/MyAlice/Animations/anim_Portfolio*.glb
if ($LASTEXITCODE -eq 0) { $rightsMatches; throw 'legacy/NIKKE provenance token found in Project 36 deliverables' }
if ($LASTEXITCODE -ne 1) { throw "rights scan failed with exit code $LASTEXITCODE" }
git diff --check a955686..HEAD
git status --short
git log -6 --oneline
```

Expected: the rights scan returns no matches; `git diff --check` is silent; the only untracked path is the pre-existing `.superpowers/`; commits contain only the files enumerated in this plan.

- [ ] **Step 4: Record hashes and inspect final media once more**

Record SHA-256 for `SampleModel.glb`, the three enemy GLBs, both new animation GLBs, the public PNG/GIF, and the information PNG in the ignored final verification file. Inspect the final public PNG and the one-second contact sheet at original resolution and confirm they are identical to the approved staged media.

- [ ] **Step 5: Use the finishing skill for integration and push**

Invoke `superpowers:verification-before-completion`, then `superpowers:requesting-code-review`, then `superpowers:finishing-a-development-branch`. Push only after fresh verification and review pass; report the resulting commit IDs and remote branch.
