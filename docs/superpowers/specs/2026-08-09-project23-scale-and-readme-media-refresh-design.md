# Project 23 Scale and README Media Refresh Design

## Context

Project 23 currently starts `BoxHuman.fbx` at scale `(1.0, 1.0, 1.0)`. The user wants this startup model reduced to `(0.01, 0.01, 0.01)` while models loaded later keep their existing default scale.

The tutorial has a manifest-driven media pipeline for 37 projects. Each project has a runtime PNG, runtime GIF, and derived information image. The character resources in the main checkout have user-owned updates that must be used for capture without being copied into Git or modified.

One relevant baseline test currently fails because manifest path validation was strengthened to reject unsafe paths earlier. The generator is secure, but `test_readme_info_images.ps1` still expects the older downstream error wording.

## Goals

- Set only Project 23's automatically loaded BoxHuman startup scale to `(0.01, 0.01, 0.01)`.
- Preserve its origin, zero rotation, fixed camera, disabled whole-model rotation, rigid classification, and automatic playback.
- Recapture all 37 manifest projects as runtime PNG and GIF files using the user's latest character resources.
- Regenerate all 37 information images from the new PNG files.
- Preserve README markup, navigation, branding, descriptions, and media paths.
- Preserve the user's modified and untracked model assets byte-for-byte and keep them out of commits.

## Non-Goals

- Do not change global FBX import units or `FbxManager` scaling.
- Do not modify `BoxHuman.fbx`, `SampleModel.glb`, its variants, or `Dx11/Common/Camera.cpp`.
- Do not change the default scale for models loaded interactively after startup.
- Do not replace the common Alice branding logo or rewrite README prose.
- Do not manually stage screenshots one project at a time when the manifest pipeline can produce them reproducibly.

## Approaches Considered

### 1. Manifest batch capture with a staged runtime overlay — selected

Build from the isolated feature worktree, stage its runtime binaries in a Git-ignored directory, and link the staged `Dx11/Resource` directory to the main checkout's current resource directory. Use a temporary manifest whose `runtimeDir` points at the staged binaries, while media output remains `docs/media/readme` in the feature worktree.

This keeps source changes isolated, consumes the user's newest character resources read-only, and uses the repository's existing capture behavior for all projects.

### 2. Capture directly in the dirty main checkout

This avoids a resource overlay, but mixes generated media and code changes with user-owned source and GLB modifications. It makes review, staging, and rollback less reliable.

### 3. Manual per-project screenshots

This gives individual control but is slow, inconsistent, and bypasses manifest actions, dimensions, retries, GIF timing, and verification.

## Design

### Project 23 startup scale

Update `App::OnInitialize()` so the successfully loaded BoxHuman receives:

```cpp
model.scale = XMFLOAT3(0.01f, 0.01f, 0.01f);
```

No other model initialization or camera value changes. Preserve the UTF-8 BOM already required for `App.cpp`.

Update the Project 23 focused startup contract to require `0.01f` on all axes both in `OnInitialize()` and inside the successful BoxHuman load block. The changed contract must fail against the current `1.0f` code before production is edited.

### Baseline information-image test recovery

Keep the existing unsafe-path fixtures and rejection assertions. Update only their expected diagnostic pattern so they accept the current, earlier manifest-validation rejection. Do not relax the requirement that the generator exits nonzero and produces no outside output.

### Build and runtime staging

Build the Debug x64 solution so all 37 application executables and dependencies are current. Create a Git-ignored staging runtime inside the feature worktree:

- Copy the built runtime binaries into the staging `Dx11/bin` directory.
- Create a directory junction from the staging `Dx11/Resource` path to `C:/Github/D3D11-AliceTutorial/Dx11/Resource`.
- Create a temporary manifest derived from `tools/readme_media_manifest.json` with only `runtimeDir` changed to the staged relative path.

Record SHA-256 hashes for the main checkout's modified character assets before and after capture. Never stage the temporary manifest, staged runtime, junction, or user assets.

### Sequential media capture

Run `tools/capture_readme_media.ps1` with the temporary manifest and `-All`. Capture projects sequentially because they share desktop focus and synthetic input. Use the manifest's existing delays, actions, GIF timing, dimensions, byte limits, and two-attempt retry policy.

If the batch reports an individual failure, diagnose that project and rerun only that manifest project after fixing the concrete capture issue. Do not silently retain its old media.

The resulting tracked outputs are:

- 37 runtime PNG files in `docs/media/readme`.
- 37 runtime GIF files in `docs/media/readme`.
- `docs/media/readme/capture-report.md`.

### Information images and visual review

Run `tools/generate_readme_info_images.ps1 -All` with the same temporary manifest to regenerate all 37 `info/*-info.png` files.

Run the repository media verifier and relevant contracts. Generate PNG and GIF review sheets in a Git-ignored output directory and inspect both sheets for blank frames, wrong characters, missing animation, unexpected windows, or unusable framing.

Project 23 must visibly show BoxHuman, `Scale 0.010`, checked playback, and rigid motion. README markup stays unchanged because all media paths remain stable.

## Error Handling

- Stop the batch if any project lacks an executable or valid capture output.
- Treat missing, undersized, wrong-dimension, oversized, or stale project media as failure.
- Preserve the previous tracked media until a replacement file passes validation.
- Keep capture windows sequential and always close them after each project.
- If resource staging or the junction cannot be created safely, stop rather than copying user assets into tracked paths.

## Verification

- Observe the Project 23 startup contract fail for the old `1.0f` scale and pass after the `0.01f` change.
- Pass the Project 23 UTF-8, rigid-startup, and visual-capture contracts.
- Pass manifest, capture-action, information-image, and media-verifier tests.
- Build all 37 Debug x64 projects successfully.
- Produce and validate 37 PNGs, 37 GIFs, 37 information images, and a successful capture report.
- Inspect aggregate PNG and GIF review sheets.
- Confirm the main checkout's protected file hashes and Git status are unchanged.
- Confirm the feature diff contains only the approved code/test/spec/plan changes and regenerated README media.
