# Project 36 Outfit Shimmer — Design

> **Correction.** An earlier revision of this document named the wrong root cause. It claimed the shimmer came from `36_BasicPS.hlsl` reading the albedo texture as a packed metallic-roughness map. That misread is real, but fixing it changed the rendering by essentially nothing — measured local contrast on the worst outfit went 31.75 → 31.87 — so it is not what the author is seeing. The real cause is missing mipmaps. Both are recorded below; only the second explains the symptom.

## Why

Project 36's showcase characters shimmer. Their outfits carry a moving speckle that tracks the animation, worst on the high-frequency patterned ones — the black splatter bodysuit and the lace skirt. The author asked for it to stop.

## Root cause: model textures have no mipmaps

`Dx11/Common/FbxManager.cpp` creates every model texture with a single mip level and never generates a chain. The raw-pixel paths hard-code `td.MipLevels = 1` and `srvd.Texture2D.MipLevels = 1` (around `:637` and `:667`), and the dominant path for VRoid GLB assets — embedded PNG/JPG, taken when `at->mHeight == 0` — calls the `CreateWICTextureFromMemory` overload that has no device context, which also produces a single level. `GenerateMips` appears nowhere in the repository outside `third_party`.

Meanwhile the samplers ask for mip filtering: `D3D11_FILTER_MIN_MAG_MIP_LINEAR` with `MaxLOD = D3D11_FLOAT32_MAX`. The intent was always a mip chain; the textures simply never had one.

The showcase characters are minified — four of them across a 1600x900 frame at ~60% height. With no mip chain every screen pixel samples a nearly arbitrary texel of a high-resolution outfit texture. A sub-pixel change in pose re-rolls which texel each pixel lands on, so the outfit boils. That is the shimmer, it is worst where the texture has the most high-frequency detail, and it only appears in motion — matching every part of the report.

## Fix

Give model textures a full mip chain.

- For the embedded compressed path, pass the device context to `CreateWICTextureFromMemory`. DirectXTK's context-taking overload (`WICTextureLoader.h:94-103`) generates the chain itself.
- For the raw BGRA paths, request a full chain (`MipLevels = 0`), add `D3D11_BIND_RENDER_TARGET` and `D3D11_RESOURCE_MISC_GENERATE_MIPS`, drop `D3D11_USAGE_IMMUTABLE` (incompatible with generated mips), upload level 0, and call `GenerateMips`. Set the SRV's `MipLevels` to `-1` so it sees the whole chain.
- Apply the same to the external-file path if it uses the same single-level overload.

Cost: about 33% more texture memory, the standard mip-chain overhead.

## Blast radius

`FbxManager.cpp` is shared. Every project that loads an FBX or GLB model gets the same improvement — which is the point, since the samplers everywhere already assume mips exist. The author approved the shared fix over a Project 36-local workaround.

## The separate latent bug

`36_BasicPS.hlsl` derived roughness from the albedo texture's `.g` and metalness from its `.b`, gated on `g_UseDiffuseMap` — a flag meaning "an albedo map is bound", not "an ORM map is bound". No asset in this project uses that packed convention, and there is no ORM texture slot.

This is wrong by inspection, and it is fixed. But it is **not** the shimmer: the scalar material values currently in use neutralise it, which is why removing it left the image measurably unchanged. It ships as its own commit, described as what it is — a latent correctness fix with no visible effect today.

If a real ORM map is added later it needs its own texture slot and its own flag. Reusing the albedo slot is what created the confusion.

## Verification

A shimmer bug is easy to declare fixed by eye and easy to reintroduce silently.

1. **Automated.** Assert in `tools/tests/test_project36_portfolio_showcase.ps1` that the model textures carry a mip chain, or measure the artifact directly if a metric can be shown to discriminate. The first attempt at a metric failed honestly: near-saturated-pixel fraction read 0.00% everywhere, and local contrast measured the outfit's own printed pattern rather than the artifact. Any metric adopted must be demonstrated to separate the before and after states with the numbers shown; if none does, say so rather than fitting a threshold.

2. **Visual.** The controller opens captured frames at original resolution and compares the patterned outfits before and after.

## Out of scope

- Switching Project 36 to toon shading. It exists as `g_ShadingMode == 5` with banding already implemented at `36_BasicPS.hlsl:244-249`; the author chose to keep the PBR look after being shown the option.
- The camera, character placement, animation windows, and HUD — all settled and frozen.
- Adding an ORM texture slot. No asset needs one.

## Constraints

- Do not modify `Dx11/Common/Animation`.
- Do not touch the frozen composition: vertical FOV 40°, camera `(0, 73, -285)` pitch 2°, positions x `45 / -155 / -54 / 155`.
- Leave `tools/tests/test_project36_portfolio_media.ps1` and `tools/verify_readme_media.ps1` red by design — they are the acceptance gate for media the author captures by hand.
- `FbxManager.cpp` is CP949-encoded with Korean comments. Preserve the encoding.
- No junctions, symlinks, or reparse points.
