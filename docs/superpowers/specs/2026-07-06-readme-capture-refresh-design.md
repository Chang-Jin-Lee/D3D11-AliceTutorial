# README Capture Refresh Design

## Goal

Refresh the repository presentation so every tutorial project has a current, redistributable screenshot in the root README, while preserving the existing README as legacy documentation.

## Approved Scope

- Preserve the current root `README.md` as `README_old.md`.
- Create a new root `README.md` with the same general content and project table structure, but replace existing GitHub attachment thumbnails with repository-local media.
- Capture one new screenshot for each project from `01_RenderingQuadangle` through `37_Blueprint`.
- Capture one short GIF for the representative demo, `36_AdvancedAnim_Sound_Click`.
- Store new README media under `docs/media/readme/`.
- Use only the public/repo-safe assets that are now present in the repository.
- Adjust startup camera transforms and cube/object positions where needed so projects open to a readable portfolio screenshot.

## Approach

Use repository-local media instead of GitHub attachment URLs. This keeps README rendering independent of external uploaded attachments and makes it easier to audit that the images show only assets that can be distributed with the repository.

The README should stay close to the existing style: Korean description, build instructions, dependency notes, and a grid of project links. The main visible change is that the images and the representative GIF come from `docs/media/readme/`.

## Camera And Scene Defaults

The default scene should be adjusted only where the first frame is hard to read in a screenshot. The target framing is:

- Cubes and basic mesh samples: object centered, not clipped, visible depth, camera slightly above or angled enough to show shape.
- Character/model samples: public character visible near center with enough ground/context to understand scale.
- Rendering technique samples: main effect visible without requiring manual camera movement.
- Debug/tool samples: the rendered object or debug primitive visible even if ImGui panels are open.

Changes should prefer existing per-sample camera variables and object transform defaults. Shared camera code should not be refactored unless a tiny helper is already present and directly useful.

## Capture Workflow

Build the relevant projects in `Debug|x64`, then run each executable from the expected `Dx11/bin` runtime layout. For each sample:

1. Launch the executable.
2. Wait for initialization and asynchronous loading where applicable.
3. Capture a stable screenshot.
4. Save the image with a two-digit project number and the project directory name, for example `docs/media/readme/02-RenderingCube.png`.
5. Close the executable before moving to the next sample.

For `36_AdvancedAnim_Sound_Click`, also record a short GIF:

- File: `docs/media/readme/36-advanced-anim-sound-click.gif`
- Content: the representative scene with the public character/cubes visible.
- Duration: short enough for README use, roughly 3 to 6 seconds if tooling allows.
- Size target: keep it practical for GitHub README loading.

If a sample cannot be captured automatically because it blocks on external UI or runtime behavior, document the reason and use a manually captured screenshot from the same repo-safe runtime scene.

## README Structure

`README_old.md` should be an exact legacy copy of the current root README at the time this work starts.

The new root README should keep:

- Introductory repository description.
- Representative demo section.
- Build instructions.
- Dependency and model format notes.
- Project shortcut grid.
- License/reference notes.

The new root README should update:

- Representative demo media to use the new `36` GIF and screenshot.
- Project grid media to use `docs/media/readme/*.png`.
- Any text that still implies restricted/private assets are required.

The README should not link to media that shows restricted NIKKE, MMD, Live2D, manga, or commercial music assets.

## Verification

Before completion:

- Confirm `README_old.md` exists and preserves the previous README content.
- Confirm every README media path exists on disk.
- Confirm the new README references `docs/media/readme/` media instead of old GitHub attachment thumbnails for project screenshots.
- Run a focused search for removed restricted asset names and paths.
- Build the projects touched by camera/object changes in `Debug|x64`.
- Run `git diff --check`.

## Risks And Mitigations

- Some samples may show blank frames if captured too early. Mitigation: wait for a visible frame and add per-sample delay in the capture script.
- Some samples may not exit cleanly after automated capture. Mitigation: launch one executable at a time and force-close only the launched process after capture.
- GIF size can become too large. Mitigation: keep it short, use modest resolution, and optimize if tooling is available.
- Adjusting every project aggressively could create regressions. Mitigation: only change startup transforms where screenshots are hard to read, and verify builds afterward.
