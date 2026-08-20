# Project 38 Stylized Toon-PBR Showcase Design

Date: 2026-08-21
Status: Approved for implementation planning

## Goal

Add an independent `38_StylizedToonPBR` tutorial project that presents the existing public Alice `SampleModel` as a polished character-rendering showcase. The project demonstrates material-aware Hybrid Toon-PBR shading, restrained anime-style outlines, two art-directed lighting presets, direct PBR comparison, and measured rendering cost without turning Project 36 into a catch-all sample.

The same delivery removes the large logo block from every detailed project README and corrects README still images in Projects 01-35 wherever a loaded character currently appears from the side or rear instead of a readable front or three-quarter-front view.

## Visual direction and identity boundary

- Use the repository's existing public `Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb`; do not add or download a new character.
- Target the broad visual language of premium anime games: deliberate warm/cool separation, readable tone planes, polished hair response, soft material distinction, and clean silhouettes.
- Do not copy a specific game's shader values, UI, branding, logos, names, textures, or screenshots.
- Expose two original presets:
  - `Neon Contrast`: stronger cool-shadow/warm-key separation and brighter edge accents.
  - `Industrial Soft`: lower saturation, softer band transitions, and more restrained highlights.
- Use a fixed front three-quarter hero composition for the README still and keep the face, hair, hands, and feet unobstructed.

## Project boundary

Project 38 is a standalone executable and Visual Studio project added after Project 37 in `Dx11/TutorialApp.sln`. It may reuse the repository's `Common` utilities and proven loading/rendering patterns from Projects 30, 31, and 36, but it does not include Project 36's gameplay, FMOD, multi-character showcase, or general animation editor complexity.

Project 36 retains its current integrated animation example and its in-progress Toon-PBR refinement. Project 38 becomes the focused explanation and comparison surface for stylized character shading and rendering optimization.

## User-visible experience

Project 38 starts directly in a readable hero view of `SampleModel`. The normal runtime offers:

- `PBR`, `Hybrid Toon-PBR`, and same-camera split comparison modes;
- `Neon Contrast` and `Industrial Soft` presets;
- controls for diffuse bands, shadow softness, shadow hue, hair highlight strength, rim strength, outline width, and exposure;
- a fixed or slowly animated model pose that never obscures the comparison;
- a compact project-description HUD and a small performance readout.

The description HUD remains intentionally simple and does not resemble a full editor:

> Hybrid Toon-PBR Character Showcase
> Material-aware toon shading, hair highlights, rim lighting, stable outlines, and GPU cost comparison.

The HUD may also show the active mode/preset and control hints, but it must not cover the character's face or body.

## Rendering architecture

The frame is organized into explicit measured passes:

1. Render the character shadow map.
2. Render the baseline or Hybrid Toon-PBR character pass.
3. Composite a stable outline from normal/depth discontinuities.
4. Apply the selected stylized tone-mapping preset.
5. Render the compact description and performance HUD.

The Hybrid Toon-PBR shader keeps physically useful inputs while art-directing their response:

- a two- or three-region diffuse ramp with controllable soft transitions;
- cool shadow tint and warm key-light tint applied without crushing texture detail;
- separate skin, hair, and cloth response profiles derived from the model's material/subset metadata and explicit project-side overrides;
- quantized/specular-lobe shaping so cloth remains broad and quiet while hair receives a narrow band highlight;
- view-dependent rim light limited by the light-facing term so it does not flatten the silhouette;
- tone mapping that preserves highlight color instead of clipping to white.

The baseline and Toon modes use the same camera, pose, lights, render targets, and exposure contract. Split comparison changes only the shading path, making the visual and timing comparison meaningful.

## Outline design

The default outline is a screen-space normal/depth edge pass because it provides a consistent pixel width as the camera changes. It uses resolution-scaled sampling and threshold controls, excludes background-only depth, and blends a dark material-aware edge color instead of unconditional pure black.

Outline quality has at least two levels so its cost is demonstrable. If the normal buffer or outline shader cannot be created, the application continues with outlines disabled and reports that state in the HUD rather than failing the whole project.

## Performance and optimization evidence

Project 38 demonstrates optimizations in running code instead of README-only claims:

- cache shaders, samplers, rasterizer/blend/depth states, and render targets;
- reuse the shadow and intermediate targets until the window size changes;
- update immutable or infrequently changing data only when values change;
- pack frequently read toon parameters into aligned constant buffers;
- avoid per-pixel dynamic material branches where a material profile or feature mask can select a cheaper path;
- provide outline quality levels with measurable cost differences;
- use D3D11 disjoint/timestamp queries in a small delayed ring so reading GPU results does not stall the current frame.

The HUD shows CPU frame time, total GPU frame time, and available pass timings for the character, outline, and tone-mapping work. Results warm up asynchronously. Unsupported or not-yet-ready queries display `warming up` or `unavailable`; the renderer must not block waiting for a measurement.

## README front-view capture investigation

The current rear/side-view defect is reproducible and not yet fixed. In several Projects 17-24, the README capture branch sets a useful initial character yaw and then enables automatic rotation. The capture manifest waits roughly 2-3 seconds while those applications add approximately 45 degrees of yaw per second, so the still is taken after the character has turned an additional 55-100 degrees. Projects 26-29 use capture-specific multi-character transforms that also leave important models facing sideways or away.

The correction is capture-only:

- normal application defaults and interactive auto-rotation remain unchanged;
- when `DX11_README_CAPTURE` is active, the primary character uses a deterministic front or three-quarter-front yaw and does not auto-rotate before the PNG is taken;
- multi-character demonstrations orient their readable hero character toward the capture camera, while secondary characters are adjusted only as required to avoid obvious rear views;
- screenshot timing is not shortened as a substitute for a deterministic pose;
- GIF motion may remain dynamic when it is part of the tutorial, but the representative PNG must come from a stable front-facing state.

Every Project 01-35 still is visually audited. Only projects that load and visibly present a character need pose changes or media replacement. Projects with geometry, spheres, editors, or no visible character remain unchanged. The known problem set is expanded if full-resolution review finds another rear-facing still.

Verification combines a focused source contract for deterministic README poses with original-resolution visual review of every recaptured PNG. Pixel-only heuristics are not treated as proof of body orientation.

## README logo removal

Remove the `README-BRAND` marker block and mascot logo image from every detailed project README, including the new Project 38 README. The root README remains free of that block as it is today.

The existing branding updater and tests are changed so the repository contract becomes removal/prevention rather than insertion/canonicalization:

- no detailed project README may contain `README-BRAND:START`, `README-BRAND:END`, or the detailed-README mascot image reference;
- running the branding maintenance tool removes legacy README logo blocks idempotently;
- app icon and other non-README uses of the branding asset remain in scope and are not deleted.

## Documentation and media integration

- Increase the manifest and test project count from 37 to 38.
- Add Project 38 to the solution, root README navigation/table/gallery, detailed README chain, media manifest, app branding metadata, and relevant static contracts.
- Update Project 37's next-project navigation to point to Project 38.
- Add Project 38 PNG, GIF, information image, and successful capture-report entries.
- Refresh Project 36's already prepared Toon-PBR PNG/GIF and capture-report evidence as part of the same branch delivery.
- Replace only verified Project 01-35 rear/side-facing character stills and their derived information images/report entries.
- Preserve unrelated media and detailed README content except for the approved logo-block removal and navigation changes.

## Testing and verification

Implementation follows test-driven development. Required evidence includes:

1. A failing-then-passing project-structure contract for Project 38, solution registration, manifest count, navigation, and expected media paths.
2. A failing-then-passing shader/source contract for the approved Hybrid Toon-PBR features, comparison modes, compact HUD, and nonblocking GPU query ring.
3. A failing-then-passing branding contract proving all detailed README logo blocks are absent and the removal tool is idempotent.
4. A failing-then-passing README capture-pose contract for every project source changed by the front-view repair.
5. Debug x64 rebuild of Project 38 and every legacy project whose capture-only pose code changes.
6. Normal-mode smoke run of Project 38 and capture-mode runs for all recaptured projects.
7. Exact media dimensions, GIF frame count/duration/size, nonblank output, manifest, README reference, and capture-report checks.
8. Original-resolution visual inspection confirming Project 38's polished front-facing character and front-facing representative stills in Projects 01-35.
9. `git diff --check`, focused regression tests, and review of the exact staged file set.

## Failure behavior

- A missing `SampleModel` or shader compile failure produces a clear initialization error for Project 38 instead of a blank successful-looking window.
- Missing optional outline resources disable only the outline and report the degraded mode.
- GPU query results that are delayed or unsupported never stall rendering and are labeled honestly.
- Capture failures preserve the previous public image until a staged replacement passes automated and visual checks.
- Logo removal never deletes the shared branding asset or alters app-icon resources.

## Git delivery

Keep unrelated dirty files and pre-existing untracked data untouched. Commit approved work in logical units, run fresh verification and code review, then push the existing `codex/project36-portfolio-showcase` branch to its configured GitHub remote with a normal non-force push. Do not merge or push directly to `main` unless separately requested.
