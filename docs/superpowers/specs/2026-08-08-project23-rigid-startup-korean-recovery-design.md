# Project 23 Rigid Startup and Korean Recovery

## Problem

Project 23 no longer demonstrates its intended rigid-animation lesson at startup. It currently loads `SampleModel.glb`, while the repository's rigid sample is `Dx11/Resource/fbx/Study/BoxHuman.fbx`.

The project text also has two forms of Korean corruption:

- `App.cpp` was damaged by an incorrect historical encoding conversion, so transcoding the current bytes alone cannot recover every comment.
- Other C++/HLSL files are readable as CP949 but are not consistently UTF-8, while the README contains mojibake stored inside an otherwise valid UTF-8 file.

## Goals

1. Start project 23 with `BoxHuman.fbx` at world position `(0,0,0)`.
2. Classify it through the existing `FbxManager` rigid-animation path and play its first embedded animation automatically.
3. Restore readable Korean in the project README and all Korean source/shader comments.
4. Store the repaired project text as UTF-8 so GitHub, Visual Studio, and command-line tools agree on the encoding.
5. Preserve current rendering, loader, icon, and README-capture functionality.

## Evidence

`BoxHuman.fbx` is a binary FBX containing animation stacks and animation curves, but no Skin, Cluster, or Deformer records. This matches the existing `FbxManager` rule: no mesh bones plus one or more animations selects `AnimationType::Rigid`.

The last broadly readable `App.cpp` revision is commit `3015652`. Later commits contain required functional changes, so the whole file must not simply be reverted.

## Design

### Korean recovery

- Recover Korean comments in `App.cpp` from the last readable revision where the surrounding code still matches, while retaining every current executable statement.
- Decode the remaining CP949 project source and shader files and save them as UTF-8 without changing executable shader/C++ content.
- Restore the README's Korean lesson text from its readable history, retaining the current branding, navigation, screenshots, and GIF sections.
- Limit recovery to `Dx11/23_Rigid_Animation`; unrelated projects and common files remain untouched.

The relevant text files are `App.cpp`, `App.h`, `WinMain.cpp`, `23_BasicPS.hlsl`, `23_BasicVS.hlsl`, `23_LightingHelper.hlsli`, `23_Shared.fxh`, `23_SkyBoxVS.hlsl`, and `README.md`. Files already valid and containing no damaged Korean do not need rewriting.

### Default rigid scene

`App::OnInitialize()` will load:

```text
..\Resource\fbx\Study\BoxHuman.fbx
```

After a successful load, the new model entry will use:

- position `(0,0,0)`;
- rotation `(0,0,0)`;
- scale `(1,1,1)`;
- model auto-rotation disabled;
- project camera position `(0,0,-8)` and zero rotation.

When the loaded entry is FBX, has animations, and reports `AnimationType::Rigid`, initialization sets both the UI playback state and `FbxManager` playback state to `true`. The existing per-frame `UpdateAnimation()` call advances the clip and uploads the rigid node palette.

README capture mode uses the same rigid model, active animation, and `(0,0,-8)` camera framing. It must not substitute `SampleModel.glb` or disable animation.

## Alternatives Rejected

- **Transcode the current files only:** This preserves already-corrupted characters in `App.cpp` and the README.
- **Revert project 23 to the old readable commit:** This would discard later rendering, capture, icon, and loader fixes.
- **Use `SampleModel.glb` and simulate rotation:** That demonstrates skinned/static rendering or whole-model rotation, not rigid node animation.
- **Enable auto-rotation instead of animation playback:** This hides whether the embedded rigid animation is actually advancing.

## Failure Handling

- A failed default load remains non-fatal; the application opens with no model rather than dereferencing an empty model list.
- Playback flags are set only after successful load and only for a rigid FBX with an embedded clip.
- Manual model loading retains the existing UI-controlled playback behavior.
- Encoding verification fails if any targeted text file is invalid UTF-8 or retains known mojibake/replacement markers.

## Verification

1. Add a regression test that asserts the project 23 startup path is `BoxHuman.fbx`, the new entry is placed at the origin, and rigid playback is enabled after successful loading.
2. Add an encoding contract for the targeted project 23 text files and verify the test fails before recovery.
3. Confirm `BoxHuman.fbx` still contains animation data and no skin/deformer data.
4. Run the new tests through RED and GREEN, followed by the existing startup and README-capture contracts.
5. Build project 23 in Debug/x64.
6. Launch project 23 normally and confirm that BoxHuman is visible and its component nodes move over time without whole-model auto-rotation.
7. Review the final diff to ensure no executable C++/HLSL logic changed as a side effect of encoding recovery.
