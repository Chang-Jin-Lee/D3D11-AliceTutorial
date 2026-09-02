# Project 36 Embedded FBX Showcase Design

Date: 2026-08-12
Status: **Abandoned 2026-09-02** at the author's instruction. Kept for its design reasoning
only. Superseded status, not a design defect — the implementation branch
`codex/project36-fbx-showcase` was discarded with Tasks 1–4 done and Task 5 unstarted, and
none of it reached `main`. Its implementation plan is
`docs/superpowers/plans/2026-08-12-project36-embedded-fbx-showcase.md`, which carries the
same notice. Note that the source asset described below (`Alice_Swimsuit_white.fbx`) is not
in the repository; discarding the branch removed it from the working set.

## Goal

Replace the awkward retargeted Project 36 README showcase with a direct playback of the two authored animations embedded in the user's own FBX. The representative PNG and GIF will show two large instances of the same character at once, each playing a different embedded clip, against the Bridge release skybox.

This is a capture-only presentation change. Normal Project 36 behavior remains unchanged.

## Verified source asset

Source file: `Z:\Alice_Swimsuit_white.fbx`

- Size: 13,912,812 bytes
- SHA-256: `9E77F9CC872F2BB6802D1E2D9C984C30D6C986CF68FDE5CA82CA3CDB595289F7`
- Meshes: 13
- Materials: 13
- Embedded textures: 20
- `Armature|Humanoid`: 284 ticks at 24 fps, about 11.833 seconds, 220 channels
- `Armature|Humanoid.001`: 175 ticks at 24 fps, about 7.292 seconds, 220 channels
- `Armature|T-Pose`: zero seconds; excluded from playback and capture

The source is a user-created VRoid character with animations authored through the user's VRMA workflow. The implementation copies this FBX verbatim into:

`Dx11/Resource/fbx/Public/MyAlice/Portfolio/Alice_Swimsuit_white.fbx`

It does not extract, retarget, resample, re-export, or otherwise modify its animation data. A focused asset contract records the expected hash, scene counts, exact animation names, positive durations for the two selected clips, and the excluded zero-duration T-pose.

## Why the current showcase is replaced

The current showcase branch applies generated sparse GLB clips across several character variants. Exact node comparison found different skeleton overlap between those variants, so the same external clip does not address every target skeleton consistently. The generated motion is also too sparse to present natural character animation.

The supplied FBX contains the mesh, skin, bind hierarchy, and both animations as one authored scene. Playing those embedded animations directly removes the cross-asset retargeting step and its visible mismatch.

## Runtime architecture

README capture mode loads the same FBX path twice and creates two `ModelEntry` instances:

- Left character: `Armature|Humanoid`
- Right character: `Armature|Humanoid.001`

The existing model cache shares the immutable `FbxModel` geometry, materials, textures, and Assimp scene for both instances. Each `ModelEntry` retains its own `FbxAnimation fbxBaseAnimator` and bone palette, so the two clips advance independently without duplicating the source model in memory.

Clip selection is by exact animation name, not array index. Both animators start at time zero, loop independently at their own authored durations, and never select `Armature|T-Pose`. Because each clip is evaluated against the exact scene and hierarchy it was authored with, this path performs no retargeting.

The capture-only controller replaces the previous four-character generated-motion timeline. When README capture mode is off, the existing Project 36 scene, controls, animation behavior, and UI continue to run as before.

## Skybox and lighting

`Hanako.dds` is a debug asset and must not be used or accepted as a fallback.

The showcase uses the Bridge IBL set distributed in the repository's `Skybox_2` GitHub Release:

- `bridgeDiffuseHDR.dds`
- `bridgeSpecularHDR.dds`
- `bridgeBrdf.dds`
- `bridgeEnvHDR.dds`

Release page: https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/releases/tag/Skybox_2

Archive URL: https://github.com/Chang-Jin-Lee/D3D11-AliceTutorial/releases/download/Skybox_2/Skybox.7z

The capture preparation step checks for all four Bridge files before launching Project 36. If they are missing, it downloads the release archive to an ignored temporary location and extracts only the required Bridge files into the worktree's ignored runtime resource tree. The approximately 365 MB archive and extracted DDS files are not committed.

The application applies the Bridge IBL only after the skybox object has been created. README capture must fail before replacing any public media when the four-file set cannot be prepared or loaded. It must not silently produce a flat gray background or substitute `Hanako.dds`. Normal-mode asynchronous release-asset behavior remains unchanged.

No junction, symbolic link, or other reparse-point alias is introduced for the FBX or skybox assets.

## Visual composition

- Source capture: 1600 x 900
- Final GIF: 800 x 450 at 8 fps
- GIF duration: approximately 12 seconds, covering one full `Armature|Humanoid` cycle and more than one `Armature|Humanoid.001` cycle
- Two characters stand side by side, face the camera, and occupy about 75% of the frame height
- The camera includes ears, hair, hands, and feet throughout both loops
- Character spacing prevents intersecting silhouettes even at the widest authored poses
- Bridge IBL and the existing ground provide depth without obscuring the characters
- A small translucent HUD identifies `EMBEDDED FBX ANIMATION` and labels the left and right clip names
- The HUD does not cover either character and makes no IK, blending, or retargeting claim

Both characters begin at deterministic transforms and animation times. The left clip is the primary loop boundary for the GIF; the right clip is intentionally seen looping independently. The PNG is selected from a time where both poses are visibly animated and distinct.

## Media and README scope

The existing README markup and public media paths remain unchanged:

- `docs/media/readme/36-AdvancedAnim-Sound-Click.png`
- `docs/media/readme/36-advanced-anim-sound-click.gif`
- `docs/media/readme/info/36-AdvancedAnim-Sound-Click-info.png`

Only the contents of those media files and the Project 36 capture-report rows are refreshed. The root README and Project 36 README continue referencing the same paths.

The reliable Project 36 backbuffer capture path and project-specific GIF-duration support from the parent branch are retained. The generated `anim_PortfolioDance.glb`, `anim_PortfolioUpperWave.glb`, their exclusive generator, and exclusive generated-clip tests are removed because the new capture no longer uses them. Focused tests are replaced with contracts for the embedded FBX clips, two independent animation instances, Bridge preparation, and the new capture presentation.

## Failure behavior

- A missing, changed, or unreadable FBX fails the capture before public media replacement.
- A missing exact animation name, a zero-duration selected animation, or accidental T-pose selection fails the capture.
- Failure to create two independent animators or to observe motion from both instances fails verification.
- Missing or unloadable Bridge assets fail the capture; gray or debug-skybox fallback is not accepted.
- Cropped, overlapping, textureless, motionless, or visibly malformed characters fail visual review.
- On any capture or verification failure, the existing public PNG, GIF, information image, and report remain untouched.
- Normal Project 36 remains usable even when capture-only assets are unavailable.

## Verification

Implementation follows test-driven development: each focused behavioral contract is demonstrated failing before the production change and passing afterward.

Required checks:

1. The committed FBX byte hash equals the verified source hash, and its Assimp metadata contains the two exact positive-duration clips plus the excluded zero-duration T-pose.
2. Capture mode loads exactly two instances from the same committed FBX path and selects the clips by exact name.
3. The two instances share model data but own separate animators and bone palettes; sampled matrices from both palettes change over time.
4. No capture path references the old generated GLB clips, the legacy NIKKE model, or `Hanako.dds`.
5. Bridge preflight requires all four exact release files and aborts safely on missing or failed preparation.
6. Bridge application occurs after skybox creation, and a captured frame contains a non-gray environment.
7. Project 36 builds in Debug x64 and remains responsive in normal-mode smoke testing.
8. README capture produces the existing 1600 x 900 PNG path and an approximately 12-second 800 x 450 GIF at 8 fps.
9. Automated media checks confirm meaningful model-region motion from both character areas, multiple distinct frames, the expected dimensions/duration, and a size within the existing 5 MiB limit.
10. Original-resolution visual review confirms two large textured characters, distinct natural poses, complete silhouettes, exact clip labels, and the Bridge environment.
11. The Project 36 information image is regenerated from the approved PNG, and README links remain valid.
12. `git diff --check` passes, unrelated projects and protected assets remain untouched, and the working tree is clean after commit.

## Delivery

The work is implemented only on `codex/project36-fbx-showcase`, committed in reviewable steps, verified from a clean state, and pushed to the matching remote branch for user review. It is not merged into `main` as part of this task.
