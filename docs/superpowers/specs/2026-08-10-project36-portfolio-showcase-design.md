# Project 36 Portfolio Showcase Design

Date: 2026-08-10
Status: Approved for implementation planning

## Goal

Replace the representative Project 36 PNG and GIF with a clear, reproducible runtime showcase. The characters must be large enough to read at GitHub README size, and one continuous GIF must demonstrate original dance motion, animation blending/layering, CCD IK, and multiple animated characters.

## Rights and provenance boundary

- Do not copy, commit, render, trace, retarget, or otherwise derive output from the legacy NIKKE Alice model, its embedded dance clips, or its audio files.
- Do not add `Alice_.fbx`, NIKKE PMX data, textures, extracted animation curves, or the legacy named dance clips to the current repository.
- Create a new dance motion against the current VRoid `J_Bip_*` skeleton. The motion must be authored for this repository and must not reproduce a recognizable third-party routine.
- The checked-in dance asset must contain animation and skeleton-node data only: no mesh, skin, material, image, texture, or audio payloads.
- Continue using only the current repository's public `MyAlice` player and enemy models for the rendered characters.

## Scope

The implementation is limited to Project 36's README capture path and its derived media:

- `Dx11/36_AdvancedAnim_Sound_Click`
- two original animation-only assets under `Dx11/Resource/fbx/Public/MyAlice/Animations`
- `docs/media/readme/36-AdvancedAnim-Sound-Click.png`
- `docs/media/readme/36-advanced-anim-sound-click.gif`
- `docs/media/readme/info/36-AdvancedAnim-Sound-Click-info.png`
- the Project 36 entry in the capture report if the capture tooling records it

The root README and Project 36 README keep their existing media paths and markup. Normal Project 36 execution must retain its current scene, input, animation, and UI behavior.

## Capture-only runtime architecture

Project 36 gains a small capture-only showcase controller owned by the Project 36 application code. It activates only when `IsReadmeCaptureMode()` is true. It must not change shared animation code unless a narrowly scoped fix is required and separately justified by a failing test.

The controller has three responsibilities:

1. Arrange the current player and three enemy characters in a compact, front-facing group.
2. Drive a deterministic eight-second animation timeline using the existing animation system.
3. Render a compact runtime HUD and IK debug markers that identify what is actually running.

The normal application update path remains the fallback whenever README capture mode is off.

## Original animation assets

Create two animation-only glTF/GLB clips for the current VRoid skeleton. Build both from the current model's skeleton hierarchy and bind-local transforms so that unanimated bones retain a valid pose.

- `PortfolioDance` provides the full-body dance loop.
- `PortfolioUpperWave` provides an upper-body-only arm and torso phrase used by the existing masked animation-layer path.

The motion should be a short original loop with clearly visible whole-body movement:

- alternating side steps
- hip and torso turns
- coordinated arm swings
- a short overhead arm pose
- a clean loop back to the starting pose

Use sparse, smooth keyframes rather than dense sampled noise. Keep the root close to its starting position so all four characters stay inside the camera composition. Small phase offsets may be applied per character so the group remains readable rather than moving in perfect visual overlap.

The existing external animation loader loads the clips under new `PortfolioDance` and `PortfolioUpperWave` keys. Loading failure must leave Project 36 usable and fall back to the existing idle animation while recording a clear warning.

## Visual composition

- Capture source resolution: 1600 x 900.
- Final GIF resolution: 800 x 450, optimized for GitHub README playback.
- The four characters should collectively occupy roughly 70% of the frame height and most of the central horizontal area.
- All characters face generally toward the camera. The current player is centered slightly forward; the three enemy variants form a shallow arc behind it.
- Retain the existing lit environment and ground, but keep unrelated cubes and debug panels outside the capture composition.
- Avoid clipped ears, feet, hair, and hands in both the PNG and all GIF phases.
- Use a fixed camera and deterministic scene positions so repeated captures have the same framing.

## Eight-second GIF timeline

1. **0.0-3.0 seconds — `DANCE / SKINNED ANIMATION`**
   - All four characters play the original dance loop.
   - The main character is the visual focus; companion characters use small time offsets.

2. **3.0-5.2 seconds — `ANIMATION BLEND + LAYER`**
   - The main character first crossfades from dance to locomotion and back on the base layer.
   - `PortfolioUpperWave` then fades in and out through the existing upper-body bone mask while the lower body continues its base motion.
   - Companion characters continue dancing so multiple live palettes remain visible.

3. **5.2-7.5 seconds — `CCD IK`**
   - The main character's hand follows a slowly moving target while a stable base animation continues.
   - A small target marker and unobtrusive shoulder-elbow-hand chain lines make the IK result visually verifiable.
   - Companion characters continue animating in the background.

4. **7.5-8.0 seconds — group finish**
   - Return to a readable group pose so the GIF loop does not cut abruptly.

The compact capture HUD uses a translucent background and short technical labels only. It must not cover character faces or bodies and must not resemble an editor or capture-tool overlay.

## PNG selection

Capture the PNG during the dance section at a frame where:

- the main character faces the camera;
- the arms and legs show an unmistakable animated pose;
- all four character variants are visible and separated;
- no limb or ear is cropped;
- the HUD reads `DANCE / SKINNED ANIMATION`.

The Project 36 information image is regenerated from this new PNG so the gallery and project README remain visually consistent.

## Failure behavior

- A missing or invalid `PortfolioDance` asset falls back to the current idle clip and writes a warning; it must not crash the application.
- A missing IK debug primitive disables only the marker, not animation playback; media verification still fails and preserves the existing public media because the required IK evidence is absent.
- Capture mode must not enable copyrighted legacy paths as a fallback.
- If an automated capture cannot prove all four characters are visible or the GIF contains meaningful motion, the existing public media remains untouched and the capture is reported as failed.

## Verification

Implementation follows test-driven development. Focused contracts must first fail for the missing showcase and then pass after implementation.

Required checks:

1. Each new animation asset has exactly one animation, zero meshes, zero materials, zero textures/images, and only expected current-VRoid node names.
2. No new tracked file or source reference contains a legacy NIKKE asset path, model name, named legacy dance clip, or legacy audio path.
3. The showcase controller is gated by `IsReadmeCaptureMode()` and normal-mode defaults remain unchanged.
4. Project 36 builds in Debug x64.
5. A normal-mode smoke run remains responsive.
6. The README capture completes at 1600 x 900 and produces the existing Project 36 PNG/GIF paths.
7. The GIF is eight seconds within normal encoding tolerance, has multiple distinct frames in every feature phase, and loops cleanly.
8. Visual review confirms four large front-facing characters, readable HUD labels, a visible blend transition, and the hand following the IK target.
9. The root README, Project 36 README, and information image all resolve to the refreshed media without changing their existing layout.
10. `git diff --check` passes and unrelated files remain untouched.

## Expected repository result

The final implementation commit contains only the Project 36 capture-specific code, the original animation-only assets, refreshed Project 36 evidence media and derived information image/report, and focused tests or capture metadata required to reproduce and verify the result.

## Rights references

- Official NIKKE MMD distribution page: https://nikke-kr.com/mmd.html
- SHIFT UP secondary creation guideline: https://policy.shiftup.co.kr/ip/kr/index.html

The official Alice archive was inspected during design. It contains the PMX model and textures but no dance motion or separate license document. The legacy repository likewise contains no provenance or redistribution permission for its embedded named dance clips. Those materials are therefore excluded from this design.
