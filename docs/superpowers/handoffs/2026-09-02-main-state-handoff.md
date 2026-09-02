# Handoff — repository state on `main`, 2026-09-02

Written by Claude Code at the author's request so any agent (Codex CLI or otherwise) can
pick the work up from `main` without reconstructing context.

This is a **state-of-the-repository** handoff, not a plan. It records what is on `main`,
what still verifies, what is unmerged, and what is stale. Every number below was measured
on 2026-09-02, not carried over from an earlier document.

An earlier handoff was written on 2026-08-23 into an untracked `.superpowers/` directory
inside a git worktree. That worktree was later removed and the document was lost with it.
This one lives in `docs/` and is committed for that reason.

---

## Resume prompt

> Work in `C:\Github\D3D11-AliceTutorial` on `main`. Read
> `docs/superpowers/handoffs/2026-09-02-main-state-handoff.md` first. `main` is clean, in
> sync with `origin/main`, and its full regression passes on an unlocked interactive
> desktop. There is no in-flight task and no unmerged branch: the one that existed,
> `codex/project36-fbx-showcase`, was discarded on the author's instruction on 2026-09-02.
> Never copy, extract, retarget, or derive data from NIKKE or legacy Alice models, motions,
> or audio. Never create junctions, symlinks, or reparse points.

---

## Repository state

| | |
|---|---|
| Checkout | `C:\Github\D3D11-AliceTutorial` |
| Branch | `main` |
| HEAD | `2742058` — `fix: preserve project 36 eye transparency`, 2026-09-01 17:44 |
| Remote | in sync with `origin/main` (0 ahead, 0 behind) |
| Working tree | clean apart from an untracked `.superpowers/` — see "Unmerged and stale" |
| Solution | `Dx11/TutorialApp.sln`, 38 applications plus `Common` |
| Submodule | `Dx11/third_party/imgui-node-editor` |

---

## What landed since 2026-08-23

The `codex/project36-portfolio-showcase` branch (Project 38 Stylized Toon PBR, plus the
front-facing legacy capture work) was **merged into `main`** and its worktree removed.
Twelve further commits landed on top:

```
2742058  09-01 17:44  fix: preserve project 36 eye transparency
8e8ac0a  09-01 12:47  feat: bootstrap verified skybox assets
c2d2e05  08-26 07:26  test: support CRLF in project 38 contracts
872b8f4  08-26 07:04  test: normalize wrapped PowerShell errors
3d60e27  08-25 00:49  fix: restore project 36 UI and warm project 38 skin
f938ed8  08-24 16:04  fix: retain capture lock after restore failure
974042f  08-24 15:43  fix: publish front-facing project 06 media
7be6e38  08-24 15:42  fix: serialize retained README captures
1441e7d  08-24 14:44  fix: reject unsafe retained capture batch
b1f33f0  08-24 14:32  fix: preserve retained capture state
49fa6c0  08-24 13:56  docs: synchronize readme capture media
21b18c4  08-24 12:22  fix: close final capture and shading findings
```

Two substantial pieces of new work:

- **`8e8ac0a` — verified skybox assets.** Adds `Dx11/Common/SkyboxAssetValidation.{h,cpp}`,
  `tools/skybox_asset_manifest.json` (sha256 + size per asset, plus a release URL for the
  364 MB `Skybox.7z`), `tools/verify_skybox_assets.ps1`, and
  `tools/tests/test_skybox_asset_bootstrap.ps1`. Projects 31–36 consume it.
- **`2742058` — model transparency.** Adds `Dx11/Common/Mesh/ModelTransparency.{h,cpp}`
  and threads transparency classification through `FbxMaterial` / `FbxModel` and Project
  36's render passes, so Project 36 eye materials keep their transparency.

**New test infrastructure: native C++ tests.** `tools/tests/native/` now holds
`MaterialTransparencyTests` and `SkyboxAssetValidationTests` (`.cpp` + `.vcxproj`), driven
from PowerShell wrappers that locate MSBuild via `vswhere` and build the test project.
This did not exist before 2026-09-01.

### The six review findings from the 2026-08-23 branch review were all closed

That review found no Critical issues but six Important ones. Verified on `main` today:

| Finding | Status on `main` |
|---|---|
| `32-Sound-FMOD.png` byte-identical to `31-IBL.png` | **Closed.** Hashes now differ (`02ef2454…` vs `d73d4043…`). |
| Promoted lace material wrote depth/MRT1 over its full quad | **Closed.** `App.cpp` now sets `colorAlphaCutoff = kBlendCoverageCutoff` (0.02) instead of `0.0f`, with a comment naming the depth/MRT1 consequence. |
| `readmeCaptureMode` left enabled for reverted 05/06 media | **Closed.** `21b18c4` removed 05's flag (it was `true` at `7b96452`); 06 keeps its flag because `974042f` properly published its media. |
| GIFs contradicted their recaptured PNGs | **Closed.** GIFs resynchronised in `49fa6c0`. |
| Stills depended on an untracked `Dx11/bin/imgui.ini` | **Closed at the tooling level.** `tools/capture_readme_media.ps1` now handles the ini, there is a dedicated `tools/restore_readme_capture_imgui_ini.ps1`, and `test_project36_portfolio_showcase.ps1` stashes and restores it around the run. See the open item below for what is still not addressed. |
| Project 38 README documented a cool shadow / warm key | **Closed.** Now reads "중립 `Shadow tint`와 순백색 `Key tint`". |

Findings 1 and 5 were additionally turned into a regression guard:
`tools/tests/test_readme_capture_evidence_media.ps1` asserts that Project 24 keeps its bone
panel evidence and that the 24 / 31 / 32 stills are distinct from one another.

---

## Verification state, measured 2026-09-02

Full sweep of `tools/tests/*.ps1` plus both verifiers:

**28 of 28 PowerShell suites pass, plus `verify_readme_media.ps1` and
`verify_skybox_assets.ps1`.** One suite could not complete for an environmental reason:

- `test_project36_portfolio_showcase.ps1` — every source-level assertion passed, then the
  run died at `CopyFromScreen` with an invalid-handle error. **The desktop was locked.** I
  confirmed this is not a code defect by attempting a bare 100×100 screen grab outside the
  suite; it failed the same way. Re-run it on an unlocked interactive desktop. It also
  restores `Dx11/bin/imgui.ini` correctly even when it dies mid-run (verified: 729 bytes,
  present afterwards).

### Four invocation gotchas — these look like failures but are not

1. **Use `pwsh` (PowerShell 7), not `powershell` (5.1).** 5.1 cannot *parse* several suites
   and the parse error reads like a test failure.
2. **`tools/tests/test_built_app_icons.ps1` is not a standalone suite.** It takes mandatory
   `-BinRoot` and `-NotOlderThan`. Bare invocation reports missing-parameter errors.
   ```
   pwsh -NoProfile -Command "$t=[datetime]'2026-08-01'; & 'tools/tests/test_built_app_icons.ps1' -BinRoot 'Dx11' -NotOlderThan $t"
   ```
3. **`tools/verify_skybox_assets.ps1` requires `-SkyboxRoot`.** The correct root is
   `Dx11/Resource/Skybox` (not `Dx11/10_StaticCube_SkyBox`).
   ```
   pwsh -NoProfile -File tools/verify_skybox_assets.ps1 -SkyboxRoot "Dx11/Resource/Skybox"
   ```
   Passes with "Skybox asset preflight passed: All (12 files)".
4. **Kill the running executable before building.** Otherwise the post-build copy fails with
   `MSB3021`/`MSB3027` and the build silently leaves the previous binary in `Dx11/bin`, so
   any measurement taken afterwards describes the old build.

Binaries in `Dx11/bin` are current as of 2026-09-01 17:40, newer than the newest source
(17:38), so no rebuild is required to run the runtime suites.

---

## Unmerged and stale

### `codex/project36-fbx-showcase` — discarded on the author's instruction, 2026-09-02

This section previously said the branch held real unmerged work and must not be deleted
without a decision. The author made that decision on 2026-09-02: **discard.** The worktree
was removed, the branch deleted, and the directory cleared.

What it held, recorded because none of it survives anywhere else:

| | |
|---|---|
| Branch tip | `5a625c2c77572f2c913f9d2c5d5876ad01b081e1`, 2026-08-12 17:57 |
| Position | 12 commits ahead of `main`, 56 behind |
| Uncommitted | 4 modified files, +61/−2 |
| Plan | `2026-08-12-project36-embedded-fbx-showcase.md`, 653 lines — **recovered onto `main`** |
| Design | `2026-08-12-project36-embedded-fbx-showcase-design.md`, 128 lines — **recovered onto `main`** |
| Ledger state | Tasks 1–4 complete, review PASS/PASS, Task 5 pending |
| Assets | added `Alice_Swimsuit_white.fbx` (13.9 MB); replaced `SampleModel.glb` with an 11.5 MB variant |

The branch was never pushed to `origin`, so there is no remote copy. Git keeps unreachable
objects for a grace period, so `git show 5a625c2` or `git reflog` can still reach the tip
for roughly 90 days from 2026-09-02 if anything needs recovering. After that it is gone.

The plan and design documents were rescued from that tip before the window closed, at the
author's instruction, and now live at their original paths under
`docs/superpowers/plans/` and `docs/superpowers/specs/`. Both are byte-identical to the
discarded originals apart from an added status notice: the plan carries an
"ABANDONED — DO NOT EXECUTE" banner and the design's `Status:` line records the
abandonment. That notice matters because the plan opens with an instruction to agentic
workers to implement it task-by-task, and tells them to work in a worktree path that no
longer exists.

**The code those documents describe was not recovered and is not on `main`** — only the
reasoning. Anything else from the branch (the 12 commits' source changes, the 4 uncommitted
files, the assets) is reachable solely through `5a625c2` until the grace period expires.

One further consequence worth knowing rather than rediscovering: the branch introduced an
`Alice_Swimsuit_white.fbx` asset, and the recovered design still describes it as its source
asset. Discarding the branch removed that asset from the working set, which sits on the
right side of the standing constraint about legacy Alice material.

### Safe to remove

- `.worktrees/project-readme-visual-gallery` — 0 commits ahead, 130 behind, clean working
  tree. Fully superseded.
- `.worktrees/skybox-asset-bootstrap` — 0 ahead, 1 behind, clean. Its work is `8e8ac0a`,
  already on `main`.
- `.worktrees/project36-portfolio-showcase` — **not a git worktree any more.** It was
  deregistered; the directory has no `.git`, so git commands run from inside it silently
  operate on the main checkout. What remains is stale 2026-08-25 leftovers (`Dx11`,
  `README.md`, `README_old.md`, `tools`). Its branch is merged. Deleting the directory
  loses nothing.
- `.superpowers/` at the repository root — untracked, and its `sdd/progress.md` belongs to
  `docs/superpowers/plans/2026-07-19-assimp-f5-runtime-placement.md`, which is complete and
  pushed. It is a July leftover, not current state.
- `tools/tests/native/Material.28758653/` and `tools/tests/native/x64/` — untracked native
  test build output from 2026-09-01, not source.

---

## Open items

None of these block anything; they are recorded so they are not rediscovered from scratch.

1. **ImGui panel placement is still not deterministic at the application level.** No project
   sets `io.IniFilename = nullptr`, so window placement still comes from `Dx11/bin/imgui.ini`
   when one exists. The capture tooling and the affected suites now stash and restore it,
   which is why the published media is reproducible, but a developer running an app by hand
   still gets whatever their local ini says. Closing this properly means either pinning
   `IniFilename` or making every capture-relevant window `ImGuiCond_Always`.
2. **Project 38's published GIF has 32 frames but no motion.** Capture mode pins the pose at
   `kCapturePoseTimeSeconds` so the still is deterministic, and the PNG and GIF come from a
   single process launch, so no environment gate separates them. Project 38 is the only
   project that pins a pose; peers animate. Fixing it means holding the pose past the
   capture delay and then running, which couples the application to the capture script's
   timing — a deliberate design decision, not an oversight to silently patch.
3. **A one-pixel line at row `y=1`** appears in Project 38 captures (`43,43,43` on black).
   It predates the Project 38 work — an earlier navy-background capture shows the same line
   in its own background colour — and row `y=0` is correct, so it is not a capture-rect
   offset. Cosmetically negligible.
4. **Deferred test-precision Minors from the Project 38 run**, all still open and all
   requiring a device-backed or rendered-frame harness rather than a source-regex check
   (the author's standing ruling bans source-regex change detectors):
   - `test_project36_portfolio_showcase.ps1` uses raw-text regexes for Toon-PBR wiring.
   - The Project 38 GPU-profiler contract does not behaviourally prove slot arithmetic,
     same-slot pass indexing, summation, or filter categories.
   - The Project 38 static contract does not prove the shadow branch binds
     `m_shadowPixelShader` rather than null, nor that double-sided materials select
     `D3D11_CULL_NONE`.
   - There is no observable-behaviour guard for the Project 38 *render*. The colour cast,
     the contrast collapse, and the HUD occlusion found in August were all caught by ad-hoc
     measurement. Discriminating data exists — blue-dominant pixels went 39.9% → 7.3% across
     the fix — so a threshold near 20% would separate cleanly.
     `test_readme_capture_evidence_media.ps1` is the working precedent to copy.

---

## Corrections carried forward

Recorded so nobody acts on the wrong number.

- The commit message for `d905ed2` ("fix: frame the project 38 capture clear of its HUD")
  states the subject is "758 of 900 rows tall", i.e. 84% of frame height. **That is wrong.**
  The measuring script counted the one-pixel artifact line at `y=1` as the top of the
  subject. Re-measured while skipping the top rows, the subject spans `y 194..758` = 565 of
  900 rows = **62.8%**, against a 65% target. The frustum derivation was sound. Do not
  re-aim the Project 38 camera on the basis of the 84% figure.
- During the Project 38 work, the sheer panel below the skirt was initially read as the
  authored overskirt and passed as correct. It was not — the knife-straight edges with a
  hard outline stroke were a mesh quad boundary being stroked because alpha-zero texels were
  writing depth. That is what `21b18c4` fixed. When judging this asset visually, straight
  outlined edges mean a coverage bug, not a garment.

---

## Standing constraints

- Never copy, commit, render, trace, retarget, or derive output from the legacy NIKKE Alice
  model, its named dance clips, or its audio.
- No junctions, symlinks, or reparse points.
- Do not weaken, delete, or loosen a test assertion to make something pass.
- Do not write source-regex change-detector tests; verify the same requirement with an
  executable observable-behaviour test.
- Project 36's showcase composition is frozen: vertical FOV 40°, camera `(0, 73, -285)`
  pitch 2°, character x positions `45 / -155 / -54 / 155`.
- `Dx11/Common/Animation` is not to be modified.
- Several sources are CP949-encoded with Korean comments — notably
  `Dx11/Common/FbxManager.cpp` and `Dx11/36_AdvancedAnim_Sound_Click/36_BasicPS.hlsl`.
  Preserve the encoding. `Dx11/38_StylizedToonPBR/` sources are plain ASCII with LF endings.
- `VRM_1`…`VRM_7` provenance: BOOTH <https://vroid.booth.pm/items/5512385>, confirmed by the
  repository owner on 2026-08-17.
