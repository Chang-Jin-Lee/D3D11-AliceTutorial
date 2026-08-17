# Project 36 VRM Animation Showcase — Design

Supersedes the animation-source half of `2026-08-10-project36-portfolio-showcase-design.md`. The camera composition, the per-project GIF contract, and the backbuffer capture provider from that design remain in force — except for that design's requirement that the GIF demonstrate CCD IK, which is withdrawn; see [Removed: the CCD IK window](#removed-the-ccd-ik-window).

## Why

`Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb` was replaced with a new model that embeds eight animations: `T-Pose` plus `VRM_1` … `VRM_7`. These are the motions the project should show off. The previous design generated two synthetic clips (`PortfolioDance`, `PortfolioUpperWave`) because no suitable motion existed; that reason is gone.

Two further problems with the current state:

- The showcase only runs when `DX11_README_CAPTURE=1`, so running the project normally shows the tutorial scene instead. The author captures screenshots by hand and wants the showcase to be what the project looks like when launched.
- The published README media shows characters from the side and back.

## Goal

Launching Project 36 shows four current public `MyAlice` characters, front-facing, each playing a different one of the seven VRM animations, cycling so all seven appear. Animation blending and upper-body layering are demonstrated on top of that motion.

> **Amended 2026-08-17.** This goal originally also demanded CCD IK. That window shipped, was reviewed by the author, and was removed; see [Removed: the CCD IK window](#removed-the-ccd-ik-window). What ships is the cross-fade, the upper-body layer, and the seven-clip rotation.

The author captures the PNG and GIF by hand. This work delivers the runtime only.

## Source of the animations

The seven animations live in the player model's own `aiScene`. `FbxManager` already enumerates `scene->mNumAnimations` and records each clip's name, duration, and ticks-per-second, so they are reachable through the existing public interface with no new asset pipeline.

`mAnimations[0]` is `T-Pose` (0.042 s) and is skipped. `mAnimations[1..7]` are the showcase clips:

| Clip | Duration |
|---|---|
| `VRM_1` | 11.875 s |
| `VRM_2` | 7.333 s |
| `VRM_3` | 11.750 s |
| `VRM_4` | 9.667 s |
| `VRM_5` | 9.375 s |
| `VRM_6` | 7.583 s |
| `VRM_7` | 11.583 s |

Resolve clips by **name**, not index, so a re-export that reorders or adds animations fails loudly instead of silently playing the wrong motion.

### Cross-model compatibility

Each animation drives 219 nodes. The three enemy models carry every one of the 52 core humanoid (`J_Bip_*`) bones the animations target:

| Model | `J_Bip` | `J_Sec` |
|---|---|---|
| `AliceEnemy1.glb` | 52/52 | 80/164 |
| `AliceEnemy2.glb` | 52/52 | 80/164 |
| `AliceEnemy3.glb` | 52/52 | 152/164 |

The unmatched bones are all `J_Sec_*` cloth bones — sleeves and skirts belonging to outfits the other characters do not wear. Leaving them in their own bind pose is correct, not a defect. `CharacterAnimator` already matches channels by bone name and ignores unmatched ones.

## Runtime

### Assignment

Four slots, seven clips, advancing every `kSetSeconds = 12.0`:

```
slot i plays clip index ((cycle * 4 + i) % 7) + 1
```

| Cycle | Slot 0 | Slot 1 | Slot 2 | Slot 3 |
|---|---|---|---|---|
| 0 | VRM_1 | VRM_2 | VRM_3 | VRM_4 |
| 1 | VRM_5 | VRM_6 | VRM_7 | VRM_1 |
| 2 | VRM_2 | VRM_3 | VRM_4 | VRM_5 |

Two cycles expose all seven. Twelve seconds is the shortest interval that lets the longest clip (`VRM_1`, 11.875 s) play through once.

### Technique windows inside each 12-second set

Two techniques serve the animation showcase rather than replacing it. Times are relative to the start of a set:

| Window | Technique | What happens |
|---|---|---|
| 0.0 – 0.6 s | **Blend** | Cross-fade every slot from its previous clip into its new one, `blend01 = SmoothStep(t / 0.6)`. Makes the cycle seamless and demonstrates blending as a working transition, not a set piece. |
| 4.0 – 7.0 s | **Layer** | On slot 0 only, layer the *next* clip's upper body over the current clip's lower body. `layerAlpha = sin(pi * (t - 4.0) / 3.0)`, so it eases in and out. |
| 7.0 – 12.0 s | *(none)* | Base clip on every slot. |

Windows do not overlap, so each technique reads clearly in a screenshot. Slots 1–3 play their base clip throughout.

### Removed: the CCD IK window

A third window ran CCD IK on slot 0 over set time 8.0 – 11.4 s (`tipBone = "J_Bip_L_Hand"`, `chainLen = 3`, a target orbiting in model space, `weight = SmoothStep` ramped over the first and last 0.4 s), with a `LineRenderer` debug pass drawing the solved chain, the reach line, and a target cross. It shipped, and the author removed it on 2026-08-17 after looking at it. **Do not re-add it in that shape.**

Why it did not work:

- The target traced a *fixed* orbit in model space while the same arm was simultaneously playing a baked VRM clip. The solve and the clip fought each other on every frame, and the arm read as broken rather than as reaching for something.
- The CCD solver applies no joint limits, so what absorbed the conflict was the elbow and the wrist, twisting through poses no clip authored.
- Mirroring the target's X sign — the obvious first fix, since the tip bone is the *left* hand — only moved the identical breakage onto the other arm. The sign was never the problem.
- It was also the one technique with no honest automated proof. Its runtime assertion measured the solver's own residual, so it passed while the solve was dead.

What a future attempt would need: an IK target derived from the clip's own hand path (so the goal and the animation agree), or joint limits on the chain plus the arm masked out of the base clip so only one thing drives it. A different target *position* is not a fix.

Removed with it: `RenderPortfolioShowcaseDebug()` and the palette-inversion recovery that fed it, the `ccd-ik` phase in `tools/tests/test_project36_portfolio_media.ps1`, and the reach-line assertion in `tools/tests/test_project36_portfolio_showcase.ps1`.

Unchanged by the removal, deliberately: the 12-second set length, both surviving windows, the frozen composition, and the media gate's 13-second / 104-frame capture window. That capture length was derived from where the cross-fade first occurs (its `cycle > 0` guard puts the first fade at t 12.0 – 12.6), not from the IK, so dropping the IK does not shorten it.

### Gating

The showcase becomes Project 36's normal appearance: launching the executable with no environment variable shows it. `IsReadmeCaptureMode()` no longer gates the showcase.

The backbuffer PNG writer keeps its own gate — it still requires both README capture mode and `DX11_README_BACKBUFFER_PNG` — so the automated capture path is unchanged for anyone who uses it.

Consequence, accepted deliberately: Project 36 no longer opens on its tutorial scene. The editor panels, decorative cubes, and free camera are replaced by the showcase composition.

### Composition

Unchanged from the measured values already on this branch: camera `(0, 73, -85)` with 5° pitch, characters at world x `19 / -177 / -89 / 166` and z `5 / 48 / 62 / 48`, `rotDeg.y = 0` (front-facing), scale 80. That composition was derived by inverting the 90° vertical frustum and confirmed against captured frames at 71.6 % of frame height with four separated silhouettes.

If the new model's proportions differ enough to break that, re-derive rather than nudge.

### HUD

One input-free, title-free panel at client `(24, 24)`:

```
VRM_1 · VRM_2 · VRM_3 · VRM_4
BLEND | UPPER-BODY LAYER | BASE   <- one label, naming the active window
```

Kept small so it does not intrude on hand-taken screenshots.

## Removals

| Path | Why |
|---|---|
| `tools/generate_project36_portfolio_animations.py` | The motion it synthesised is superseded by the embedded VRM clips. |
| `tools/tests/test_project36_portfolio_assets.py` | Tests only the generator's output. |
| `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioDance.glb` | Unused. |
| `Dx11/Resource/fbx/Public/MyAlice/Animations/anim_PortfolioUpperWave.glb` | Unused. |

The published media reverts to its pre-branch state: `docs/media/readme/36-AdvancedAnim-Sound-Click.png`, `36-advanced-anim-sound-click.gif`, `info/36-AdvancedAnim-Sound-Click-info.png`, and the two Project 36 rows in `capture-report.md`. The author supplies replacements.

`tools/tests/test_project36_portfolio_media.ps1` is retained but will fail against the reverted 4-second media. It is the contract the author's hand-captured GIF must satisfy, so it stays as the acceptance gate rather than being deleted or loosened.

## New asset

The replacement `SampleModel.glb` (13.98 MB, 251 nodes, 219 joints, 8 animations) currently exists only as an uncommitted change in the main checkout. It must be brought into the worktree and committed — it is the source of every motion in this design.

## Verification

The existing runtime test harness is reused; its assertions are replaced to match the new requirements. All checks are observable behaviour against the launched executable — no source-text assertions.

| Requirement | Assertion |
|---|---|
| Launching with no environment variable shows the showcase | HUD region is non-empty on a plain launch |
| Four characters animate independently | ≥ 4 separated moving column clusters across the frame |
| Slots play *different* clips | The four character bands differ from one another within a single frame, and their motion is not in lock-step |
| The set cycles | Frames sampled in cycle 0 and cycle 1 differ in the HUD region |
| All seven clips appear | Over two cycles the HUD region shows seven distinct clip-name renderings |
| Blend window | Cross-fade produces intermediate poses at the set boundary rather than a pose jump |
| Layer window | Slot 0's upper body diverges from its lower body during 4.0 – 7.0 s |
| Rights boundary | Project 36 deliverables contain none of the banned legacy tokens |

Plus: a Debug x64 rebuild at the existing ~30-warning baseline with no new warning, and confirmation that the clips resolve by name with the expected durations.

The runtime test needs an interactive unlocked desktop, as before.

## Out of scope

- Capturing the PNG or GIF. The author does that by hand.
- Any change to `Dx11/Common/Animation`.

## Tracked separately

Removing the mascot logo from the 37 project READMEs is unrelated to the showcase but was requested alongside it. The implementation plan carries it as its own task and its own commit so it can be reviewed and reverted independently.

## Constraints

- Never copy, commit, render, trace, retarget, or derive output from the legacy NIKKE Alice model, its named dance clips, or its audio.

## Animation provenance

`VRM_1` … `VRM_7` are VRMA motions from BOOTH, distributed for use: <https://vroid.booth.pm/items/5512385>. Confirmed by the repository owner on 2026-08-17. They are unrelated to the legacy NIKKE Alice assets the constraint above excludes.

A rights scan over the Project 36 deliverables and `SampleModel.glb` for the legacy tokens (`NIKKE`, `Alice_.fbx`, `CaramellaDansen`, `RabbitHole`, `Specialist`, `CaliforniaGirls`) returns no matches.
- Keep the root and Project 36 README markup and media paths unchanged.
- No junctions, symlinks, or reparse points.
- Do not touch the other worktrees (`project-readme-visual-gallery`, `project36-fbx-showcase`) or `.superpowers/` workspaces belonging to other plans.
