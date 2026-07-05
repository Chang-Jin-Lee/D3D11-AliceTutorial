# Public Character Asset Replacement Design

## Context

The D3D11 tutorial currently references NIKKE/Alice-derived character, image, manga, Live2D, and sound assets. The goal is to replace those public-facing assets with user-owned VRoid characters and neutral replacement media, while preserving the interactive animation demo value of the project.

The replacement character source is the user-provided Drive asset set:

- Player: `SampleModel.glb`, `SampleModel.fbx`, `SampleModel.vrm`
- Enemies: `AliceEnemy1.glb`, `AliceEnemy2.glb`, `AliceEnemy3.glb`

The user confirmed they are `ChangJinLee`, the creator of the VRoid assets. That author confirmation allows this project to bundle the character GLB/FBX/VRM assets under project-specific permission even though the embedded VRM metadata currently says redistribution is disabled. To avoid ambiguity, the implementation must add an attribution/permission note in the repository.

The Unreal reference project at `C:\Perforce_Main\AliceTopView_` contains retargeted `MyAlice` animation assets. A local Unreal export probe successfully exported animation-only FBX files for `anim_Idle`, `Walk_Loop_F_0_Seq`, `Run_Combat_Loop_F_0_Seq`, and `Roll_F_0_Seq`. These exported FBX files contain animation stacks and `J_Bip` tracks that match the replacement GLB skeletons. Because the original animation source rights are not yet explicitly confirmed, the default design is to generate/use these animation FBXs locally and document them separately. If the user later confirms they also own or can redistribute the animation source, those exported FBXs can be bundled as project assets.

## Goals

- Move current NIKKE/Alice-derived assets out of the repository resource paths into `C:\Users\k2503200021\Desktop\assets` or the Korean-named asset archive path requested by the user, preserving relative paths for personal study.
- Replace all currently loaded player and enemy character assets with the user-owned VRoid GLB/FBX assets.
- Replace NIKKE manga, Live2D, and sound references with neutral placeholder or generated/free project assets.
- Add a lightweight external animation path that can apply Unreal-exported animation-only FBX clips to the replacement GLB skeletons.
- Keep the app buildable and runnable without requiring the old copyrighted assets.
- Document all replacement asset provenance and any local-only animation export requirements.

## Non-Goals

- Do not build a full retargeting editor.
- Do not rewrite the renderer, animation state machine, or Assimp model loader beyond the minimal external clip support needed.
- Do not preserve the old NIKKE songs, manga pages, Live2D assets, or character filenames in public-facing runtime paths.
- Do not modify the user-authored VRoid model geometry or metadata unless the user later asks for that explicitly.

## Asset Policy

### Character Assets

Use bundled replacement character assets under project-specific permission from the author:

- Author: `ChangJinLee`
- Permission basis: the user confirmed they created the VRoid assets and approved their use in this project.
- Required repository note: add an attribution/permission document next to resources.

### Animation Assets

Use a conservative default:

- Export animation-only FBX files from `C:\Perforce_Main\AliceTopView_` into a local generated asset folder.
- Load those FBX files at runtime if present.
- Do not treat the exported animation FBXs as generally redistributable unless the user confirms the animation source rights.

If a bundled playable demo is required before rights are confirmed, the implementation may include simple generated fallback clips or static pose fallback instead of committing animation FBXs.

### Old Assets

Move risky current assets to a personal archive path. The implementation must not delete them permanently.

Archive root:

`C:\Users\k2503200021\Desktop\assets\D3D11-AliceTutorial-2026-07-05`

If the Korean path `C:\Users\k2503200021\Desktop\애셋` exists or can be created safely, use that exact path instead:

`C:\Users\k2503200021\Desktop\애셋\D3D11-AliceTutorial-2026-07-05`

## Runtime Architecture

### Replacement Character Loading

Update the advanced animation sample to load:

- Player: `Dx11\Resource\fbx\Public\MyAlice\Player\SampleModel.glb`
- Enemy 1: `Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy1.glb`
- Enemy 2: `Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy2.glb`
- Enemy 3: `Dx11\Resource\fbx\Public\MyAlice\Enemy\AliceEnemy3.glb`

Keep the existing `FbxModel`/Assimp path because it already supports `.glb`, `.gltf`, and `.fbx`.

### External Animation Clip Loading

Add a focused animation clip library that owns Assimp importers/scenes for animation-only FBX files and exposes stable clip keys:

- `Idle`
- `Walk`
- `Run`
- `Roll`

The library must preserve importer lifetime for the returned `aiAnimation*`. The existing animation update code can already evaluate an `aiAnimation` against the mesh scene if channel node names match the model skeleton. The new path should therefore resolve animation clips in this order:

1. External animation library clip by key.
2. Existing same-scene animation index map fallback.
3. Bind pose/static fallback if neither exists.

### Bone Matching

Use `J_Bip` skeleton naming. Confirmed shared core names include hips, spine, chest, neck, head, arms, legs, hands, fingers, and toe bases. Extra GLB-only hair, skirt, and accessory bones can remain in bind pose when a clip has no channel for them.

The weapon socket should move away from the old Alice-specific bone name and use a `J_Bip` hand bone such as:

`J_Bip_R_Hand`

If the old rifle asset is also NIKKE-derived or unclear, replace it with a neutral simple generated placeholder weapon or temporarily disable weapon attachment.

### Media Replacement

Replace old manga/image/sound runtime references with neutral project assets:

- Loading/comic images: neutral generated PNG placeholders or a simple project-branded UI image.
- SFX: short neutral generated or free placeholder sounds.
- BGM/demo sound boxes: remove copyrighted song names and map boxes to neutral demo audio labels, or disable audio-trigger boxes until replacement audio exists.

## Error Handling

- If a replacement GLB is missing, log the exact path and skip that actor instead of crashing.
- If external animation FBX files are missing, keep the model in bind pose or use any embedded clip if present.
- If a requested clip key is not found, log the missing key once per initialization path and fall back to idle/static behavior.
- If old asset archive move fails, stop before deleting or replacing the corresponding file so no personal-study asset is lost.

## Testing

- Run an asset probe to confirm replacement GLBs load through Assimp and expose a skin/skeleton.
- Run an animation probe to confirm exported FBX clips contain `J_Bip` animation channels.
- Build the Visual Studio solution or the specific D3D11 sample project.
- Launch or smoke-test the app enough to verify:
  - the player model appears,
  - enemies use the three replacement assets,
  - old NIKKE manga/sound paths are no longer required,
  - animation clip resolution does not crash when clips are absent or present.

## Implementation Sequence

1. Archive current risky assets outside the repository resource paths.
2. Copy user-owned replacement character assets into the public resource tree.
3. Add attribution/permission documentation.
4. Add local Unreal animation export script or instructions.
5. Add external animation clip loading support.
6. Update sample runtime asset paths, bone socket names, and media references.
7. Add neutral media replacements.
8. Build and smoke-test.

## Self-Review

- No placeholder requirements remain.
- The character asset permission path is explicit and tied to the user being the asset author.
- The animation asset path is conservative because source redistribution rights are not yet confirmed.
- The design is scoped to asset replacement and lightweight animation reuse only.
- Runtime fallback behavior is defined for missing models, clips, and media.
