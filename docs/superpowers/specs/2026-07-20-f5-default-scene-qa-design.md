# F5 Default Scene QA Design

**Date:** 2026-07-20
**Scope:** Projects 11, 17-21, 23-36
**Goal:** Pressing F5 in Visual Studio should immediately show the intended sample content without a required file-picker action, invisible meter-scale characters, a T-pose-only default, or an ImGui assertion.

## Context and confirmed causes

The QA report covers several independent symptoms that share one user-facing requirement: each tutorial must present a useful default scene immediately after launch.

- Project 11 loads the bundled `Skeleton_Model.model3.json`, which does not use clipping masks. The application then calls `GetRenderTextureCount()` even though Cubism did not create a clipping manager. The SDK already creates mask surfaces when a model actually needs them.
- Projects 17, 18, 19, 20, 21, 23, and 24 already contain startup calls that load `..\Resource\fbx\Public\MyAlice\Player\SampleModel.glb`. The required change is therefore regression protection, not another copy of the same startup code.
- `SampleModel.glb` contains one embedded animation named `T-Pose`. Project 25 can never reach Idle by selecting a different embedded index. The tracked `anim_Idle.fbx` contains the required compatible external animation.
- The public player GLB has an approximately 1.61-unit mesh height, while projects 26-35 retain scenes, cameras, spawn spacing, sound distances, and ground placement authored in an approximately centimeter-scale world. This unit mismatch makes the replacement characters effectively invisible.
- Project 36 calls `ImGui::End()` only when `ImGui::Begin("Sound Debug")` returns true. ImGui requires `End()` after every `Begin()` call, including when a window is collapsed. This causes the reported `Missing End()` assertion.
- The four `DeadlyImportError` first-chance messages in the project 36 debugger log correspond to the four external FBX animation imports. Direct Assimp probes using the same post-process flags successfully loaded all four files with one animation each. They are handled importer-internal exceptions and are not the terminating failure.

## Chosen approach

Use targeted, explicit corrections at each failure point.

This preserves the tutorial scenes and avoids changing shared model-import scale semantics for arbitrary files opened through the UI. Automatic AABB normalization was rejected because it would also affect user-selected models and earlier tutorials. Re-authoring every camera and scene in meter units was rejected because it would require coordinated changes to ground geometry, shadow ranges, picking, spawning, and sound attenuation.

## Component design

### Project 11: Cubism mask lifecycle

Remove the application-owned clipping-mask size query and offscreen-surface creation loop from the successful Live2D load path. Keep renderer options that are safe without a clipping manager, but let `CubismRenderer_D3D11` create and resize mask surfaces when `CubismModel::IsUsingMasking()` is true.

This fixes the no-mask bundled model and retains support for future masked models through the SDK's normal renderer lifecycle.

### Projects 17-21, 23, and 24: default asset contract

Keep the existing startup load of `SampleModel.glb` and its camera framing. Add these projects to the portable runtime/default-scene verifier so removal of the call, a path typo, or omission of a project fails QA automatically.

No duplicate loading code or unrelated scene refactor is required.

### Project 25: external Idle as the default animation

Extend `FbxAnimation` with a narrow external-clip entry point. The animator continues to use the GLB scene for hierarchy, bind pose, bone offsets, and global inverse, while channel evaluation and clip timing can come from a caller-owned `aiAnimation`.

The implementation will:

1. Store the active clip pointers independently from `aiScene::mAnimations`.
2. Build animation names, duration, channel maps, and precomputed palettes from those pointers.
3. Preserve the existing embedded-animation behavior when no external clip is supplied.
4. Let project 25 own one `ExternalAnimationClipLibrary`, load `anim_Idle.fbx` with `UnrealCmZUpToGlbMeters`, and share its `Idle` pointer across all 36 model instances.
5. Select and play Idle for every instance. The different toon/outline rows remain visual-material comparisons rather than animation-index comparisons.

The external library owns its Assimp importer for the entire lifetime of the model instances, so the `aiAnimation` pointer remains valid. If the required Idle clip cannot be loaded, project initialization logs the importer error and fails instead of silently presenting T-pose as a successful default.

### Projects 26-35: character-only unit conversion

Apply a factor of `100.0f` to public character instance scales, while leaving non-character assets unchanged.

- A character currently at scale `1` becomes `100`.
- A character intentionally authored at scale `0.5` becomes `50`, preserving its relative size.
- Ground and sphere scales remain unchanged.
- Runtime-spawned enemy instances in projects that support spawning receive the same conversion.
- Camera position, ground placement, shadow configuration, picking coordinates, spawn spacing, and sound distance settings remain unchanged.

The conversion is written explicitly at the default-character creation sites. It does not alter `FbxModel`, `AssetManager`, or file-picker behavior for arbitrary user assets.

### Project 36: balanced ImGui window lifecycle

Keep the Sound Debug contents conditional on the return value of `ImGui::Begin()`, but move `ImGui::End()` outside the conditional block. This matches the pattern already used by the other project 36 windows.

No Assimp exception policy change is included because all four referenced animation files were confirmed to import successfully and the actual terminating assertion is independent of those first-chance messages.

## Error handling

- Required startup model and Idle paths remain repository-relative and are checked by the verifier.
- Project 25 reports the exact external animation import error and does not claim a successful default scene when Idle is unavailable.
- Existing load functions continue to return failure without dereferencing missing model data.
- Project 11 relies on the Cubism renderer's mask-presence check rather than adding a second application-side mask state.

## Test and verification design

Implementation follows test-driven development.

1. Extend the PowerShell portable/default-scene verifier with failing contracts for:
   - project 11 having no manual `GetRenderTextureCount()`/`GetMaskBuffer()` startup path;
   - default model startup calls in projects 17, 18, 19, 20, 21, 23, and 24;
   - project 25 loading and selecting external Idle;
   - character scale conversion at initial and dynamic-spawn sites in projects 26-35;
   - project 36 placing `ImGui::End()` outside the Sound Debug `Begin()` condition.
2. Add focused unit-level coverage for `FbxAnimation` external clip metadata/channel selection where the existing build structure permits it.
3. Run the verifier before production changes and record the expected failures.
4. Implement one correction group at a time and rerun the focused checks.
5. Build affected projects in x64 Debug and x64 Release, including `Common` because `FbxAnimation` changes there.
6. Run the full portable runtime verifier and inspect the final Git diff for unintended asset or configuration changes.

The user will perform final visual execution testing on another computer. Local verification therefore proves build integrity and static/default-scene contracts without claiming remote visual confirmation.

## Out of scope

- Changing Assimp's internal exception mechanism or Visual Studio first-chance exception display.
- Automatically normalizing arbitrary user-loaded models.
- Re-authoring tutorial scenes into a different unit system.
- Changing tutorial-specific material, lighting, shadow, picking, or sound behavior beyond what is required to make default characters visible.
