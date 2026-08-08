# Bind Pose Rendering Without Animation Clips

## Problem

Project 18 successfully loads `SampleModel.glb`, but the character is not visible. The model is a valid skinned GLB with one skin and 135 joints, but it contains no animation clips.

`App::OnRender()` selects the skinned vertex shader whenever `FbxManager::HasSkeleton()` is true. However, `FbxManager::UpdateAnimation()` returns early when no animation clip exists. That prevents the bind-pose bone palette from ever being uploaded, so the skinned shader reads an uninitialized bone constant buffer.

## Goal

Render skinned models in their bind pose even when they contain no animation clips, while preserving existing animated and rigid-model behavior.

## Design

`FbxManager::UpdateAnimation()` remains the single place that prepares and uploads the active bone palette.

1. Preserve the existing rigid-animation branch unchanged.
2. Continue only when the current model type is `AnimationType::Skinned`.
3. Require a loaded Assimp scene, but do not require an animation clip.
4. When a valid clip exists, build the channel map and evaluate the animated pose exactly as before.
5. When no clip exists, use an empty channel map. `EvaluateGlobalMatrices()` then evaluates the imported node hierarchy at its original transforms.
6. Build and upload the normal skinning palette using `GlobalInverse * BoneGlobal * InverseBind`.

This reuses the established bind-pose math instead of uploading identity matrices or switching to a static shader.

## Alternatives Rejected

- **Upload identity matrices:** This is not generally equivalent to the bind pose because inverse-bind and node transforms can be non-identity.
- **Use the static vertex shader when no clips exist:** This bypasses skinning semantics and would make the project behave incorrectly when animation data is attached later.
- **Initialize the palette inside `Load()`:** `Load()` receives a device but not the render context used for dynamic constant-buffer upload. Keeping palette upload in `UpdateAnimation()` preserves the current responsibility boundary.

## Scope

Production changes are limited to `Dx11/Common/FbxManager.cpp`. Project 18 benefits through its existing `UpdateAnimation()` call. Other projects using the same manager receive the same correct bind-pose behavior.

No model files, camera defaults, shader layouts, animation playback controls, or README-capture settings will be changed.

## Failure Handling

- Rigid animation continues to use `UploadRigidNodePalette()` and returns.
- Static models return before palette evaluation because their animation type is not skinned.
- A missing Assimp scene still returns safely.
- A skinned scene with no clips evaluates imported node transforms and uploads a valid bind-pose palette.

## Verification

1. Add a regression contract that fails while `UpdateAnimation()` rejects a skinned model solely because `HasAnimations` is false.
2. Verify the regression test fails before the production change and passes afterward.
3. Run the existing startup and visual-capture contract tests.
4. Build project 18 in Debug/x64.
5. Run project 18 with the current `SampleModel.glb` and confirm the character is visible at the origin.
