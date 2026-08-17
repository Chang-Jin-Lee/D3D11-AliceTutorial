# Project 36 PBR ORM Misread — Design

## Why

Project 36's showcase characters shimmer. Their outfits carry a moving speckle — white flecks on the black bodysuit, a mirror-like gradient sweeping the black skirt — that tracks the animation. The author asked for it to stop.

## Root cause

`Dx11/36_AdvancedAnim_Sound_Click/36_BasicPS.hlsl:46-52` treats the bound albedo texture as a packed metallic-roughness map:

```hlsl
float roughnessTex = 1.0f;
float metalnessTex = 0.0f;
if (g_UseDiffuseMap != 0)
{
    roughnessTex = textureColor.g;
    metalnessTex = textureColor.b;
    ...
}
```

and `:107-109` folds those into the material:

```hlsl
metalness = saturate(metalness * metalnessTex);
roughness = saturate(roughness * roughnessTex);
```

`g_UseDiffuseMap` means "an albedo map is bound", not "an ORM map is bound". There is no separate ORM texture slot. The in-code comment — *"Roughness/Metalness는 선형 텍스처이므로 그대로 사용"* — records the assumption: the author expected a packed convention where `.g` is roughness and `.b` is metalness. VRoid character textures do not follow it. They are plain colour maps.

So the albedo drives the material:

| Outfit | Texture `.g` / `.b` | Resulting material |
|---|---|---|
| White | ≈ 1 / ≈ 1 | metalness survives at full strength — fully metallic |
| Black | ≈ 0 / ≈ 0 | roughness → 0, clamped to 0.04 at `:117` — near-mirror |

Both are highly reflective, and `:149-152` reflects the IBL specular probe — the skybox. As a character animates, the reflected sky sweeps across the outfit, which is the shimmer. A mirror surface also blows highlights to saturation, which is the white speckle.

The bug is not in the skinning path: `36_BasicVS.hlsl:52` and `:60` renormalise the skinned normal correctly, and `36_BasicPS.hlsl:65` renormalises the interpolated normal. Animation only makes an existing material error visible by moving the reflection.

## Fix

Stop deriving roughness and metalness from the albedo texture. With no ORM map bound, the scalar material parameters `g_PBRRoughness` and `g_PBRMetalness` govern the surface, as they already do when no texture is bound at all (`:112-115`).

This is a two-line deletion plus the comments that assert the wrong convention.

If a genuine ORM map is added later, it belongs in its own texture slot behind its own flag — reusing the albedo slot is what caused this.

## Blast radius

`36_BasicPS.hlsl` is referenced only by `Dx11/36_AdvancedAnim_Sound_Click/36_AdvancedAnim_Sound_Click.vcxproj` and `App_Lifecycle.inl`. Every other project compiles its own pixel shader, so no other project changes.

Within Project 36 the change affects any model rendered through the PBR path with a diffuse map — the four characters and the ground. The ground sets `useInstancePbrMaterial` with an explicit `metalness 0.01 / roughness 1.0`, so it was already dominated by its scalar values; removing the albedo modulation moves it slightly toward those stated values, which is the intent.

## Verification

A shimmer bug is easy to declare fixed by eye and easy to reintroduce silently. Two checks:

1. **Automated.** Add an assertion to `tools/tests/test_project36_portfolio_showcase.ps1` measuring the fraction of near-saturated pixels inside the character bands. A metallic or mirror outfit blows highlights to white; a dielectric one does not. Measure the fraction **before and after** the shader change and set the threshold from the gap, with the margin stated. If the two do not separate, do not fit a threshold — find a different observable and say so.

2. **Visual.** The controller opens captured frames at original resolution and confirms the outfits read as cloth rather than metal, with no moving speckle across sampled frames.

## Out of scope

- Switching Project 36 to toon shading. It exists as `g_ShadingMode == 5` with the banding already implemented at `36_BasicPS.hlsl:244-249`, and the author chose to keep the PBR look after being shown the option.
- The camera, character placement, animation windows, and HUD. All settled and frozen.
- Adding an ORM texture slot. No asset needs one today.

## Constraints

- Do not modify `Dx11/Common/Animation`.
- Do not touch the frozen composition: vertical FOV 40°, camera `(0, 73, -285)` pitch 2°, positions x `45 / -155 / -54 / 155`.
- Leave `tools/tests/test_project36_portfolio_media.ps1` and `tools/verify_readme_media.ps1` red by design — they are the acceptance gate for media the author captures by hand.
- No junctions, symlinks, or reparse points.
