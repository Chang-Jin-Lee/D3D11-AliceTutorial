# Project 36 VRM Animation Showcase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Launching Project 36 shows four front-facing public `MyAlice` characters each playing a different one of the seven `VRM_*` animations embedded in the new `SampleModel.glb`, cycling so all seven appear, with animation blending, upper-body layering, and CCD IK demonstrated on top.

**Architecture:** Replace the synthetic two-clip source with the player model's own `aiScene` animations, resolved by name through `FbxManager`'s existing public interface. Replace the fixed Dance/BlendLayer/IK phase machine with a rotating four-slot clip assignment whose 12-second set contains non-overlapping blend, layer, and IK windows. Drop the `IsReadmeCaptureMode()` gate so the showcase is Project 36's normal appearance.

**Tech Stack:** C++20, Direct3D 11, Assimp, existing `CharacterAnimator`/`FbxManager`, ImGui, PowerShell 7, MSBuild.

## Global Constraints

- Never copy, commit, render, trace, retarget, or derive output from the legacy NIKKE Alice model, its embedded named dance clips, or its audio. The Project 36 deliverables must contain none of: `NIKKE`, `Alice_.fbx`, `CaramellaDansen`, `RabbitHole`, `Specialist`, `CaliforniaGirls`.
- Show exactly four current public `MyAlice` characters, front-facing, collectively occupying roughly 70% of the frame height.
- Keep the existing root and Project 36 README markup and media paths unchanged.
- Resolve animation clips by **name**, never by index.
- Do not modify `Dx11/Common/Animation` — compose its existing public interfaces.
- Do not create junctions, symlinks, or reparse points.
- Do not touch the other worktrees (`project-readme-visual-gallery`, `project36-fbx-showcase`) or `.superpowers/` workspaces belonging to other plans.
- The backbuffer PNG writer keeps its own gate: README capture mode **and** `DX11_README_BACKBUFFER_PNG`.
- Work only in `C:\Github\D3D11-AliceTutorial\.worktrees\project36-portfolio-showcase`. Never run `git reset`, `git stash`, `git clean`, or any branch-switching checkout. The single path-scoped `git restore --source=…` in Task 1 Step 6 is the one authorized exception; it restores four named tracked files and touches nothing else.

## Environment

- MSBuild: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe`. Baseline is ~30 pre-existing warnings, 0 errors.
- `python` is NOT on PATH. Use `C:\Users\k2503200021\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe`.
- The runtime test needs an interactive, unlocked desktop.

## File Structure

### Deleted

- `tools/generate_project36_portfolio_animations.py` — synthesised motion, superseded.
- `tools/tests/test_project36_portfolio_assets.py` — tests only that generator.
- `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioDance.glb`
- `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioUpperWave.glb`

### Modified

- `Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb` — replaced with the 8-animation model.
- `Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl` — clip resolution, set rotation, technique windows, HUD.
- `Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl` — runtime state.
- `Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl` — remove capture-mode gating of the composition and clip loading.
- `Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl` — showcase owns palettes unconditionally.
- `Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl` — HUD and debug draw no longer capture-gated.
- `tools/tests/test_project36_portfolio_showcase.ps1` — assertions replaced.
- `docs/media/readme/*` — reverted to pre-branch state.

---

### Task 1: Adopt the new model and drop the superseded assets

**Files:**
- Modify: `Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb`
- Delete: `tools/generate_project36_portfolio_animations.py`
- Delete: `tools/tests/test_project36_portfolio_assets.py`
- Delete: `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioDance.glb`
- Delete: `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioUpperWave.glb`
- Create: `tools/tests/test_project36_vrm_clips.ps1`
- Modify: `docs/media/readme/36-AdvancedAnim-Sound-Click.png`
- Modify: `docs/media/readme/36-advanced-anim-sound-click.gif`
- Modify: `docs/media/readme/info/36-AdvancedAnim-Sound-Click-info.png`
- Modify: `docs/media/readme/capture-report.md`

**Interfaces:**
- Produces: `tools/tests/test_project36_vrm_clips.ps1` — asserts the player model carries `VRM_1`…`VRM_7` with their expected durations.
- Produces: the seven clip names and durations every later task depends on.

- [ ] **Step 1: Write the failing clip-inventory test**

Create `tools/tests/test_project36_vrm_clips.ps1`. It parses the GLB's JSON chunk with .NET only — no Python, no Assimp — so it runs anywhere:

```powershell
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$modelPath = Join-Path $repoRoot 'Dx11\Resource\fbx\Public\MyAlice\Player\SampleModel.glb'

$failures = [System.Collections.Generic.List[string]]::new()
function Assert-True([bool]$Condition, [string]$Message) {
    if ($Condition) { Write-Host "  ok   $Message" } else { $failures.Add($Message); Write-Host "  FAIL $Message" }
}

function Read-GlbJson([string]$Path) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 20) { throw "not a GLB: $Path" }
    if ([System.BitConverter]::ToUInt32($bytes, 0) -ne 0x46546C67) { throw "bad GLB magic: $Path" }
    $total = [System.BitConverter]::ToUInt32($bytes, 8)
    $offset = 12
    while ($offset -lt $total) {
        $len  = [System.BitConverter]::ToUInt32($bytes, $offset)
        $type = [System.BitConverter]::ToUInt32($bytes, $offset + 4)
        if ($type -eq 0x4E4F534A) {
            return [System.Text.Encoding]::UTF8.GetString($bytes, $offset + 8, $len) | ConvertFrom-Json
        }
        $pad = if ($len % 4) { 4 - ($len % 4) } else { 0 }
        $offset += 8 + $len + $pad
    }
    throw "no JSON chunk in $Path"
}

Assert-True (Test-Path -LiteralPath $modelPath) "player model exists: $modelPath"
$doc = Read-GlbJson $modelPath

$expected = [ordered]@{
    'VRM_1' = 11.875; 'VRM_2' = 7.333; 'VRM_3' = 11.750; 'VRM_4' = 9.667
    'VRM_5' = 9.375;  'VRM_6' = 7.583; 'VRM_7' = 11.583
}

$byName = @{}
foreach ($a in $doc.animations) { $byName[$a.name] = $a }

foreach ($name in $expected.Keys) {
    if (-not $byName.ContainsKey($name)) {
        Assert-True $false "clip '$name' is present in the player model"
        continue
    }
    $maxTime = 0.0
    foreach ($s in $byName[$name].samplers) {
        $acc = $doc.accessors[$s.input]
        if ($null -ne $acc.max -and $acc.max.Count -gt 0) {
            $maxTime = [Math]::Max($maxTime, [double]$acc.max[0])
        }
    }
    Assert-True ([Math]::Abs($maxTime - $expected[$name]) -le 0.05) `
        ("clip '$name' runs {0:F3}s (expected {1:F3}s)" -f $maxTime, $expected[$name])
}

Assert-True ($byName.ContainsKey('T-Pose')) 'T-Pose is present and will be skipped by the showcase'

# The showcase drives every character from these clips, so the core humanoid
# bones they target must exist on the enemy models too.
$targeted = [System.Collections.Generic.HashSet[string]]::new()
foreach ($name in $expected.Keys) {
    if (-not $byName.ContainsKey($name)) { continue }
    foreach ($c in $byName[$name].channels) {
        $n = $doc.nodes[$c.target.node].name
        if ($n -like 'J_Bip*') { [void]$targeted.Add($n) }
    }
}
Assert-True ($targeted.Count -ge 50) "VRM clips drive $($targeted.Count) core humanoid bones"

foreach ($enemy in @('AliceEnemy1.glb', 'AliceEnemy2.glb', 'AliceEnemy3.glb')) {
    $enemyDoc = Read-GlbJson (Join-Path $repoRoot "Dx11\Resource\fbx\Public\MyAlice\Enemy\$enemy")
    $have = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($n in $enemyDoc.nodes) { [void]$have.Add($n.name) }
    $missing = @($targeted | Where-Object { -not $have.Contains($_) })
    Assert-True ($missing.Count -eq 0) "$enemy carries every core humanoid bone the VRM clips drive"
}

if ($failures.Count -gt 0) {
    Write-Host "Project 36 VRM clip inventory FAILED ($($failures.Count)):"
    foreach ($f in $failures) { Write-Host " - $f" }
    exit 1
}
Write-Host 'project 36 VRM clip inventory passed'
```

- [ ] **Step 2: Run it and verify RED**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_vrm_clips.ps1
```

Expected: exit 1, with every `VRM_*` clip reported absent — the worktree still holds the old 11.5 MB model.

- [ ] **Step 3: Bring in the new model**

The replacement lives as an uncommitted change in the main checkout. Copy the file (never link it):

```powershell
Copy-Item -LiteralPath 'C:\Github\D3D11-AliceTutorial\Dx11\Resource\fbx\Public\MyAlice\Player\SampleModel.glb' `
          -Destination  'Dx11\Resource\fbx\Public\MyAlice\Player\SampleModel.glb' -Force
```

Confirm the copy is the 8-animation model and not the old one:

```powershell
(Get-Item 'Dx11\Resource\fbx\Public\MyAlice\Player\SampleModel.glb').Length
```

Expected: `13979960`.

- [ ] **Step 4: Run the test and verify GREEN**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_vrm_clips.ps1
```

Expected: exit 0, all seven clips present with the listed durations, and all three enemy models reported as carrying every core humanoid bone.

- [ ] **Step 5: Delete the superseded assets**

```powershell
git rm -- tools/generate_project36_portfolio_animations.py `
          tools/tests/test_project36_portfolio_assets.py `
          Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioDance.glb `
          Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioUpperWave.glb
```

- [ ] **Step 6: Revert the published media**

The author supplies replacements by hand, so restore the pre-branch files:

```powershell
git restore --source=07ae26b --staged --worktree -- `
    docs/media/readme/36-AdvancedAnim-Sound-Click.png `
    docs/media/readme/36-advanced-anim-sound-click.gif `
    docs/media/readme/info/36-AdvancedAnim-Sound-Click-info.png `
    docs/media/readme/capture-report.md
```

This is the plan's one authorized restore. It is path-scoped to four tracked files from the pre-branch commit and touches nothing else — it cannot move HEAD or discard other work. Verify:

```powershell
git status --short -- docs/media
```

Expected: the four paths staged as modifications, nothing else.

- [ ] **Step 7: Commit Task 1**

```powershell
git add -- Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb tools/tests/test_project36_vrm_clips.ps1
git diff --cached --check
git commit -m "feat: adopt the VRM animation model"
```

---

### Task 2: Play the VRM clips on all four characters, ungated

**Files:**
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl`
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl`
- Modify: `tools/tests/test_project36_portfolio_showcase.ps1`

**Interfaces:**
- Consumes: clip names `VRM_1`…`VRM_7` from Task 1.
- Produces: `const aiAnimation* App::FindPortfolioClip(const std::string& name) const` — resolves a clip by name from the player model's scene, `nullptr` if absent.
- Produces: `PortfolioShowcaseRuntime::clips` — a fixed array of seven `const aiAnimation*`.
- Produces: launching the executable with no environment variable shows the showcase.

- [ ] **Step 1: Replace the showcase assertions with the new contract**

In `tools/tests/test_project36_portfolio_showcase.ps1`, the harness (window acquisition, `ShowcaseFrame`, frame capture, `finally` cleanup, imgui.ini stashing) is correct and stays. Replace the capture-mode launch so it passes **no** `DX11_README_CAPTURE`, and replace the phase assertions with:

```powershell
Assert-True ($hudBright -ge 0.01) `
    "a plain launch renders the showcase HUD at client (24,24) (bright pixel ratio $([Math]::Round($hudBright,4)), need >= 0.0100)"

Assert-True ($clusters -ge 4) `
    "four separated character regions animate independently (found $clusters, need >= 4)"

Assert-True ($slotPairsDiffering -eq 6) `
    "all four characters show different motion (differing slot pairs $slotPairsDiffering of 6)"
```

`$slotPairsDiffering` compares the four character column bands pairwise **across the same two timestamps**: for each band, hash it at t and again at t+0.5 s, then count how many of the six band pairs have a different (hash-at-t, hash-at-t+0.5) transition. Four characters running the same clip in lock-step would move identically and score 0; four different clips score 6.

Delete the three phase-label diff assertions and the `4 CHARACTERS / LIVE PALETTES` cast-line assertion — the HUD's contents change in Task 3.

- [ ] **Step 2: Run it and verify RED**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_portfolio_showcase.ps1
```

Expected: exit 1. A plain launch currently shows the tutorial scene, so the HUD assertion fails first.

- [ ] **Step 3: Resolve clips by name from the player scene**

In `App_PortfolioShowcase.inl`, replace the two `m_ExternalAnimClips.Get(...)` lookups. Add near the top of the file:

```cpp
static constexpr int   kPortfolioClipCount    = 7;
static constexpr int   kPortfolioSlotCount    = 4;

static const char* const kPortfolioClipNames[kPortfolioClipCount] = {
    "VRM_1", "VRM_2", "VRM_3", "VRM_4", "VRM_5", "VRM_6", "VRM_7"
};
```

Add the resolver. It walks `GetAnimationNames()` — resolving by name, never index — and returns the matching `aiAnimation`:

```cpp
const aiAnimation* App::FindPortfolioClip(const std::string& name) const
{
    for (const auto& entry : m_->m_Models)
    {
        if (!entry || entry->source != ModelSource::FBX || !entry->shared || !entry->shared->fbx)
            continue;

        const auto& fbx = *entry->shared->fbx;
        const aiScene* scene = fbx.GetScenePtr();
        if (!scene)
            continue;

        const auto& names = fbx.GetAnimationNames();
        for (size_t i = 0; i < names.size(); ++i)
        {
            if (names[i] == name && i < scene->mNumAnimations)
                return scene->mAnimations[i];
        }
    }
    return nullptr;
}
```

Declare it in `App.h` beside the other showcase methods.

- [ ] **Step 4: Store the resolved clips**

In `App_InternalTypes.inl`, inside `PortfolioShowcaseRuntime`, replace the fallback flag with the clip table:

```cpp
std::array<const aiAnimation*, 7> clips{};
int resolvedClipCount = 0;
```

Keep `initialized`, `timeSec`, `slots`, and the IK debug members. Remove `fallbackToIdle` and `phase`.

In `InitializePortfolioShowcase()`, resolve every clip and log the outcome:

```cpp
showcase.resolvedClipCount = 0;
for (int i = 0; i < kPortfolioClipCount; ++i)
{
    showcase.clips[(size_t)i] = FindPortfolioClip(kPortfolioClipNames[i]);
    if (showcase.clips[(size_t)i])
        ++showcase.resolvedClipCount;
    else
        m_->PushLog(std::string("[WARN] Portfolio clip missing: ") + kPortfolioClipNames[i]);
}
m_->PushLog("[OK] Portfolio clips resolved: " + std::to_string(showcase.resolvedClipCount) +
            "/" + std::to_string(kPortfolioClipCount));

if (showcase.resolvedClipCount == 0)
{
    m_->PushLog("[WARN] Portfolio showcase disabled: no VRM clips resolved");
    return false;
}
```

- [ ] **Step 5: Drive each slot with its own clip**

In `UpdatePortfolioShowcase(float dt)`, delete the phase computation and the `Idle`/`Walk`/`PortfolioDance` lookups. Assign slot `i` the clip at index `i % resolvedClipCount` for now — rotation arrives in Task 3:

```cpp
showcase.timeSec += dt;

for (size_t slotIndex = 0; slotIndex < showcase.slots.size(); ++slotIndex)
{
    auto& slot = showcase.slots[slotIndex];
    if (!slot.initialized)
        continue;

    const aiAnimation* clip = showcase.clips[slotIndex % (size_t)showcase.resolvedClipCount];
    if (!clip)
        continue;

    const float clipTime = showcase.timeSec + slot.phaseOffsetSec;

    CharacterAnimator::UpdateDesc desc{};
    desc.dt = dt;
    desc.base.enabled = true;
    desc.base.animA = clip;
    desc.base.animB = clip;
    desc.base.timeA = clipTime;
    desc.base.timeB = clipTime;
    desc.base.blend01 = 0.0f;
    desc.base.layerAlpha = 1.0f;

    slot.animator.Update(desc);
    // existing palette upload, unchanged
}
return true;
```

- [ ] **Step 6: Remove the capture-mode gate**

Four edits, each removing a gate rather than adding a branch:

1. `App_PortfolioShowcase.inl`, `InitializePortfolioShowcase()` — delete the early `if (!IsReadmeCaptureMode()) return false;`.
2. `App_PortfolioShowcase.inl`, `UpdatePortfolioShowcase()` — change the guard to `if (!showcase.initialized) return false;`.
3. `App_Lifecycle.inl` — apply the capture composition (`capturePositions`, `rotDeg.y = 0`, camera `(0,73,-85)` pitch 5°) unconditionally, and skip the two decorative cubes unconditionally. Delete `editorPositions` and `editorYawDeg`; they no longer have a caller.
4. `App_RenderPasses.inl` — call `RenderPortfolioShowcaseHud()` and `RenderPortfolioShowcaseDebug()` without the `IsReadmeCaptureMode()` condition, in both the forward and deferred branches.

Leave `WritePortfolioBackbufferPng()` exactly as it is. Its gate on README capture mode plus `DX11_README_BACKBUFFER_PNG` is what keeps the automated capture path working, and it is not part of this change.

In `App_UpdateInput.inl`, `showcaseOwnsPalettes` now returns true on every normal frame, so the general animation loop is skipped for showcase-owned models always. Delete the now-dead `!showcaseOwnsPalettes &&` term on the AdvancedRig branch and its explanatory comment.

- [ ] **Step 7: Build and run the test**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
pwsh -NoProfile -File tools/tests/test_project36_portfolio_showcase.ps1
```

Expected: build exits 0 at the ~30-warning baseline with no new warning; the test exits 0 with four independently animating characters on a plain launch.

- [ ] **Step 8: Commit Task 2**

```powershell
git add -- Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl Dx11/36_AdvancedAnim_Sound_Click/App_InternalTypes.inl Dx11/36_AdvancedAnim_Sound_Click/App.h Dx11/36_AdvancedAnim_Sound_Click/App_Lifecycle.inl Dx11/36_AdvancedAnim_Sound_Click/App_UpdateInput.inl Dx11/36_AdvancedAnim_Sound_Click/App_RenderPasses.inl tools/tests/test_project36_portfolio_showcase.ps1
git diff --cached --check
git commit -m "feat: play VRM clips on every showcase character"
```

---

### Task 3: Rotate the clip assignment so all seven appear

**Files:**
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl`
- Modify: `tools/tests/test_project36_portfolio_showcase.ps1`

**Interfaces:**
- Consumes: `PortfolioShowcaseRuntime::clips` from Task 2.
- Produces: `int PortfolioClipIndexForSlot(int cycle, int slot)` returning `(cycle * 4 + slot) % 7`.
- Produces: a HUD line naming the four clips currently playing.

- [ ] **Step 1: Add the rotation assertions**

Append to `tools/tests/test_project36_portfolio_showcase.ps1`. Sample the HUD region once inside cycle 0 (t ≈ 6 s) and once inside cycle 1 (t ≈ 18 s):

```powershell
$cycle0Hud = [ShowcaseFrame]::Signature($frameCycle0, $hudX, $hudY, $hudW, $hudH)
$cycle1Hud = [ShowcaseFrame]::Signature($frameCycle1, $hudX, $hudY, $hudW, $hudH)
Assert-True ($cycle0Hud -ne $cycle1Hud) `
    'the clip assignment rotates between cycles (HUD names differ at t=6s and t=18s)'

Assert-True ($distinctHudSignatures -ge 2) `
    "at least two distinct clip line-ups appear over 24s (found $distinctHudSignatures)"
```

- [ ] **Step 2: Run it and verify RED**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_portfolio_showcase.ps1
```

Expected: exit 1 — the assignment is currently static, so both HUD samples are identical.

- [ ] **Step 3: Implement the rotation**

Add beside the other constants in `App_PortfolioShowcase.inl`:

```cpp
static constexpr float kPortfolioSetSeconds = 12.0f;

// Slot i plays clip ((cycle * 4 + i) % 7). Two cycles expose all seven.
static int PortfolioClipIndexForSlot(int cycle, int slot)
{
    const int raw = (cycle * kPortfolioSlotCount + slot) % kPortfolioClipCount;
    return raw < 0 ? raw + kPortfolioClipCount : raw;
}
```

In `UpdatePortfolioShowcase`, derive the cycle and use it:

```cpp
const int   cycle   = static_cast<int>(showcase.timeSec / kPortfolioSetSeconds);
const float setTime = showcase.timeSec - static_cast<float>(cycle) * kPortfolioSetSeconds;
```

Replace the Task 2 placeholder assignment with:

```cpp
const int clipIndex = PortfolioClipIndexForSlot(cycle, static_cast<int>(slotIndex));
const aiAnimation* clip = showcase.clips[(size_t)clipIndex];
if (!clip)
    continue;
```

Keep `setTime` unused for now; Task 4 consumes it.

- [ ] **Step 4: Show the line-up in the HUD**

Replace the HUD body in `RenderPortfolioShowcaseHud()` with the current clip names:

```cpp
std::string lineUp;
for (int slot = 0; slot < kPortfolioSlotCount; ++slot)
{
    if (slot > 0)
        lineUp += "  ";
    lineUp += kPortfolioClipNames[PortfolioClipIndexForSlot(cycle, slot)];
}
ImGui::TextUnformatted(lineUp.c_str());
```

`cycle` must be recomputed here from `m_->m_PortfolioShowcase.timeSec` the same way `UpdatePortfolioShowcase` does, so the HUD never disagrees with the motion.

- [ ] **Step 5: Build and run the test**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
pwsh -NoProfile -File tools/tests/test_project36_portfolio_showcase.ps1
```

Expected: both exit 0; the HUD line-up differs between the two cycle samples.

- [ ] **Step 6: Commit Task 3**

```powershell
git add -- Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl tools/tests/test_project36_portfolio_showcase.ps1
git diff --cached --check
git commit -m "feat: rotate showcase clips across all seven"
```

---

### Task 4: Blend, layer, and IK windows inside each set

**Files:**
- Modify: `Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl`
- Modify: `tools/tests/test_project36_portfolio_showcase.ps1`

**Interfaces:**
- Consumes: `setTime`, `cycle`, and `PortfolioClipIndexForSlot` from Task 3.
- Produces: non-overlapping technique windows at 0.0–0.6 s (blend, all slots), 4.0–7.0 s (upper-body layer, slot 0), and 8.0–11.4 s (CCD IK, slot 0).

- [ ] **Step 1: Add the technique assertions**

Append to `tools/tests/test_project36_portfolio_showcase.ps1`:

```powershell
Assert-True ($boundaryStep -le ($medianStep * 2.0)) `
    ("the set boundary cross-fades rather than snapping (boundary step $([Math]::Round($boundaryStep,2)) vs median $([Math]::Round($medianStep,2)))")

Assert-True ($ikYellowInWindow -ge 40 -and $ikYellowInWindow -ge (4 * $ikYellowOutsideWindow + 20)) `
    ("CCD IK runs only in its window (yellow px in $ikYellowInWindow, out $ikYellowOutsideWindow)")

Assert-True ($layerUpperDelta -ge (2.0 * $layerBaselineDelta)) `
    ("slot 0's upper body diverges during the layer window (delta $([Math]::Round($layerUpperDelta,4)) vs baseline $([Math]::Round($layerBaselineDelta,4)))")
```

`$boundaryStep` is the mean absolute pixel difference between the frames straddling t = 12.0 s; `$medianStep` is the median of that difference sampled at ten mid-set times. A hard clip switch produces a step far above the median; a 0.6 s cross-fade keeps it comparable.

`$layerUpperDelta` compares slot 0's upper band (y 150–320) against its lower band (y 320–560) change rate during 4.0–7.0 s, versus the same ratio at 2.0 s.

- [ ] **Step 2: Run it and verify RED**

Run:

```powershell
pwsh -NoProfile -File tools/tests/test_project36_portfolio_showcase.ps1
```

Expected: exit 1 — the clip switch currently snaps, and neither the layer nor the IK path runs.

- [ ] **Step 3: Cross-fade at the set boundary**

Add the window constants:

```cpp
static constexpr float kPortfolioBlendSeconds  = 0.6f;
static constexpr float kPortfolioLayerStartSec = 4.0f;
static constexpr float kPortfolioLayerEndSec   = 7.0f;
static constexpr float kPortfolioIkStartSec    = 8.0f;
static constexpr float kPortfolioIkEndSec      = 11.4f;
static constexpr float kPortfolioIkRampSec     = 0.4f;
```

In the per-slot loop, blend from the clip the slot played in the previous cycle:

```cpp
const int previousClipIndex = PortfolioClipIndexForSlot(cycle - 1, static_cast<int>(slotIndex));
const aiAnimation* previousClip = showcase.clips[(size_t)previousClipIndex];

desc.base.enabled = true;
if (cycle > 0 && previousClip && setTime < kPortfolioBlendSeconds)
{
    desc.base.animA   = previousClip;
    desc.base.timeA   = clipTime + kPortfolioSetSeconds;
    desc.base.animB   = clip;
    desc.base.timeB   = clipTime;
    desc.base.blend01 = SmoothStep(setTime / kPortfolioBlendSeconds);
}
else
{
    desc.base.animA   = clip;
    desc.base.animB   = clip;
    desc.base.timeA   = clipTime;
    desc.base.timeB   = clipTime;
    desc.base.blend01 = 0.0f;
}
desc.base.layerAlpha = 1.0f;
```

`timeA` adds `kPortfolioSetSeconds` so the outgoing clip keeps advancing from where it was rather than restarting.

- [ ] **Step 4: Layer the next clip's upper body on slot 0**

Still inside the loop, for slot 0 only:

```cpp
const bool isMainSlot = (slotIndex == 0);
if (isMainSlot && setTime >= kPortfolioLayerStartSec && setTime < kPortfolioLayerEndSec)
{
    const int nextClipIndex = PortfolioClipIndexForSlot(cycle, kPortfolioSlotCount);
    const aiAnimation* layerClip = showcase.clips[(size_t)nextClipIndex];
    if (layerClip)
    {
        const float layer01 = (setTime - kPortfolioLayerStartSec) /
                              (kPortfolioLayerEndSec - kPortfolioLayerStartSec);
        desc.upper.enabled    = true;
        desc.upper.animA      = layerClip;
        desc.upper.animB      = layerClip;
        desc.upper.timeA      = clipTime;
        desc.upper.timeB      = clipTime;
        desc.upper.blend01    = 0.0f;
        desc.upper.layerAlpha = std::sin(layer01 * XM_PI);
    }
}
```

`PortfolioClipIndexForSlot(cycle, kPortfolioSlotCount)` is the clip that follows the four on screen, so the layered upper body is always a motion the viewer has not just seen on another character.

- [ ] **Step 5: Run CCD IK on slot 0's left hand**

```cpp
if (isMainSlot && setTime >= kPortfolioIkStartSec && setTime < kPortfolioIkEndSec)
{
    const float ikSpan   = kPortfolioIkEndSec - kPortfolioIkStartSec;
    const float ikLocal  = setTime - kPortfolioIkStartSec;
    const float rampIn   = std::min(ikLocal / kPortfolioIkRampSec, 1.0f);
    const float rampOut  = std::min((ikSpan - ikLocal) / kPortfolioIkRampSec, 1.0f);
    const float ikTime   = ikLocal / ikSpan;

    desc.ik.enabled  = true;
    desc.ik.tipBone  = "J_Bip_L_Hand";
    desc.ik.chainLen = 3;
    desc.ik.targetMS = XMVectorSet(
        -0.32f + 0.18f * std::cos(ikTime * XM_2PI),
         1.08f + 0.16f * std::sin(ikTime * XM_2PI),
         0.20f, 1.0f);
    desc.ik.weight = SmoothStep(std::min(rampIn, rampOut));
}
```

Keep the existing IK debug-line recovery and `ikDebugValid` guarding exactly as it is; only its activation condition moves from the old phase test to this window.

- [ ] **Step 6: Mark the active technique in the HUD**

Add a second HUD line, so a screenshot says which technique it caught:

```cpp
const char* technique = "BASE";
if (setTime < kPortfolioBlendSeconds)                                             technique = "BLEND";
else if (setTime >= kPortfolioLayerStartSec && setTime < kPortfolioLayerEndSec)   technique = "UPPER-BODY LAYER";
else if (setTime >= kPortfolioIkStartSec   && setTime < kPortfolioIkEndSec)       technique = "CCD IK";
ImGui::TextUnformatted(technique);
```

- [ ] **Step 7: Build and run the test**

Run:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
pwsh -NoProfile -File tools/tests/test_project36_portfolio_showcase.ps1
```

Expected: both exit 0, with the boundary cross-fade, the layer divergence, and the windowed IK all measured.

- [ ] **Step 8: Commit Task 4**

```powershell
git add -- Dx11/36_AdvancedAnim_Sound_Click/App_PortfolioShowcase.inl tools/tests/test_project36_portfolio_showcase.ps1
git diff --cached --check
git commit -m "feat: window blend, layer, and IK inside each set"
```

---

### Task 5: Remove the mascot logo from the project READMEs

This task is independent of the showcase and shares only the repository. Keep it in its own commit.

**Files:**
- Modify: 37 `Dx11/*/README.md` files
- Modify: `README.md` if it carries the same block

**Interfaces:**
- Produces: no `alice-tutorial-logo.png` reference anywhere outside `Dx11/third_party`.

- [ ] **Step 1: Record the current count**

```powershell
(Select-String -Path 'Dx11\*\README.md','README.md' -Pattern 'alice-tutorial-logo' -SimpleMatch).Count
```

Expected: 37. Note the exact number — Step 4 asserts it reaches 0.

- [ ] **Step 2: Inspect one instance so the removal is exact**

```powershell
Select-String -Path 'Dx11\01_RenderingQuadangle\README.md' -Pattern 'alice-tutorial-logo' -Context 2,2
```

The line has the shape:

```html
<p align="center"><img src="../../docs/media/branding/alice-tutorial-logo.png" width="520" alt="D3D11 Alice Tutorial mascot logo" /></p>
```

Remove the whole `<p>` line. If it sits inside a marker block (`<!-- README-...:START -->` … `:END`), remove only the `<p>` line and leave the markers — the README tooling keys off them.

- [ ] **Step 3: Remove every instance**

```powershell
$files = Get-ChildItem -Path 'Dx11' -Filter 'README.md' -Recurse |
         Where-Object { $_.FullName -notmatch '\\third_party\\' }
$files += Get-Item 'README.md'
foreach ($f in $files) {
    $lines = Get-Content -LiteralPath $f.FullName
    $kept  = $lines | Where-Object { $_ -notmatch 'alice-tutorial-logo' }
    if ($kept.Count -ne $lines.Count) {
        Set-Content -LiteralPath $f.FullName -Value $kept -Encoding UTF8
    }
}
```

- [ ] **Step 4: Verify none remain and nothing else changed**

```powershell
(Select-String -Path 'Dx11\*\README.md','README.md' -Pattern 'alice-tutorial-logo' -SimpleMatch).Count
git diff --stat
```

Expected: the count is 0, and `git diff --stat` shows one deleted line per affected README and no other change. Inspect one file's diff to confirm only the `<p>` line went:

```powershell
git diff -- Dx11/01_RenderingQuadangle/README.md
```

- [ ] **Step 5: Confirm the README contract tests still pass**

```powershell
pwsh -NoProfile -File tools/tests/test_readme_info_images.ps1
pwsh -NoProfile -File tools/tests/test_public_scene_media.ps1
pwsh -NoProfile -File tools/verify_readme_media.ps1
```

`verify_readme_media.ps1` is expected to FAIL on Project 36 — the media was reverted in Task 1 while the manifest still declares an 8-second GIF. That failure is expected until the author supplies the new capture. Every other check must pass, and no failure may mention a logo or a missing image.

- [ ] **Step 6: Commit Task 5**

```powershell
git add -- Dx11 README.md
git diff --cached --check
git commit -m "docs: drop the mascot logo from project READMEs"
```

---

### Task 6: Final regression and provenance audit

**Files:**
- Verify only; no expected production edits.
- Write ignored evidence to `.superpowers/artifacts/project36-vrm-showcase/final-verification.txt`.

**Interfaces:**
- Consumes: all prior task commits.

- [ ] **Step 1: Run the suite**

```powershell
$tests = @(
  'tools/tests/test_project36_vrm_clips.ps1',
  'tools/tests/test_readme_media_manifest.ps1',
  'tools/tests/test_capture_manifest_actions.ps1',
  'tools/tests/test_verify_readme_media.ps1',
  'tools/tests/test_visual_capture_contracts.ps1',
  'tools/tests/test_project36_portfolio_showcase.ps1',
  'tools/tests/test_public_scene_media.ps1',
  'tools/tests/test_readme_info_images.ps1'
)
foreach ($test in $tests) {
  pwsh -NoProfile -File $test
  if ($LASTEXITCODE -ne 0) { throw "FAILED: $test" }
}
```

Expected: every command exits 0.

`tools/tests/test_project36_portfolio_media.ps1` and `tools/verify_readme_media.ps1` are **expected to fail** against the reverted 4-second media. Run both, record their diagnostics, and state in the report that they are the acceptance gate for the author's hand-captured replacement. Do not modify, loosen, or delete either.

- [ ] **Step 2: Rebuild and smoke-test the normal launch**

```powershell
& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1
$oldCapture = $env:DX11_README_CAPTURE
$oldBackbuffer = $env:DX11_README_BACKBUFFER_PNG
$process = $null
try {
    Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue
    Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue
    $process = Start-Process -FilePath 'C:\Github\D3D11-AliceTutorial\.worktrees\project36-portfolio-showcase\Dx11\bin\36_AdvancedAnim_Sound_Click.exe' -WorkingDirectory 'C:\Github\D3D11-AliceTutorial\.worktrees\project36-portfolio-showcase\Dx11\bin' -PassThru
    Start-Sleep -Seconds 8
    $process.Refresh()
    if ($process.HasExited -or -not $process.Responding -or $process.MainWindowHandle -eq 0) {
        throw 'normal-launch smoke run failed'
    }
}
finally {
    if ($null -ne $process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
    if ($null -eq $oldCapture) { Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue } else { $env:DX11_README_CAPTURE = $oldCapture }
    if ($null -eq $oldBackbuffer) { Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue } else { $env:DX11_README_BACKBUFFER_PNG = $oldBackbuffer }
}
```

Expected: build exits 0 at the ~30-warning baseline; the application owns a responsive visible window after eight seconds.

- [ ] **Step 3: Audit rights and repository scope**

```powershell
$rightsMatches = rg -a -n -i 'NIKKE|Alice_\.fbx|CaramellaDansen|RabbitHole|Specialist|CaliforniaGirls|Caramel_Dance|MeniShuki' Dx11/36_AdvancedAnim_Sound_Click Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb
if ($LASTEXITCODE -eq 0) { $rightsMatches; throw 'legacy/NIKKE provenance token found' }
if ($LASTEXITCODE -ne 1) { throw "rights scan failed with exit code $LASTEXITCODE" }
git diff --check 57fa7f4..HEAD
git status --short
git log --oneline 57fa7f4..HEAD
```

Expected: no matches; `git diff --check` silent; the only untracked paths are the gitignored `.superpowers/` and `artifacts/`.

Scanning `SampleModel.glb` itself is deliberate — it is a new binary asset whose provenance was flagged as an open item in the design.

- [ ] **Step 4: Record hashes and confirm the seven clips**

Record SHA-256 for `SampleModel.glb`, the three enemy GLBs, and the Project 36 executable into `.superpowers/artifacts/project36-vrm-showcase/final-verification.txt`, together with the clip-inventory output from `test_project36_vrm_clips.ps1`.

- [ ] **Step 5: Hand off**

Report to the author: the branch state, that `test_project36_portfolio_media.ps1` and `verify_readme_media.ps1` remain red pending their hand-captured PNG and GIF, and the unresolved provenance question for `VRM_1`…`VRM_7`.

Do not invoke the branch-finishing skill — integration is the controller's call once the author supplies the media.
