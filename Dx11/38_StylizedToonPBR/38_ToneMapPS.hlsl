#include "38_Shared.fxh"

Texture2D hdrTexture : register(t0);
Texture2D outlineMaskTexture : register(t1);
Texture2D normalProfileTexture : register(t2);
SamplerState linearSampler : register(s0);

static const uint NeonContrast = 0;
static const uint IndustrialSoft = 1;

float3 AcesFitted(float3 color)
{
    return saturate((color * (2.51f * color + 0.03f)) / (color * (2.43f * color + 0.59f) + 0.14f));
}

float3 LinearToSrgb(float3 linearColor)
{
    linearColor = saturate(linearColor);
    float3 lowerSegment = linearColor * 12.92f;
    float3 upperSegment = 1.055f * pow(linearColor, (1.0f / 2.4f).xxx) - 0.055f;
    float3 useUpperSegment = step(0.0031308f.xxx, linearColor);
    return lerp(lowerSegment, upperSegment, useUpperSegment);
}

float3 SrgbToLinear(float3 srgbColor)
{
    srgbColor = saturate(srgbColor);
    float3 lowerSegment = srgbColor / 12.92f;
    float3 upperSegment = pow((srgbColor + 0.055f) / 1.055f, 2.4f.xxx);
    float3 useUpperSegment = step(0.04045f.xxx, srgbColor);
    return lerp(lowerSegment, upperSegment, useUpperSegment);
}

float4 PSMain(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float exposure = toneMapParameters.x;
    uint lightingPreset = (uint)(toneMapParameters.y + 0.5f);
    float3 hdr = hdrTexture.SampleLevel(linearSampler, uv, 0).rgb * exposure;
    float3 mapped = AcesFitted(hdr);
    if (lightingPreset == NeonContrast)
    {
        float luminance = dot(mapped, float3(0.2126f, 0.7152f, 0.0722f));
        mapped = lerp(luminance.xxx, mapped, 1.12f);
        mapped = pow(saturate(mapped), 0.94f.xxx);
    }
    else if (lightingPreset == IndustrialSoft)
    {
        float luminance = dot(mapped, float3(0.2126f, 0.7152f, 0.0722f));
        mapped = lerp(luminance.xxx, mapped, 0.82f);
        mapped = pow(saturate(mapped), 1.06f.xxx);
    }

    float outlineMask = outlineMaskTexture.SampleLevel(linearSampler, uv, 0).r;
    float profile = normalProfileTexture.SampleLevel(linearSampler, uv, 0).a * 3.0f - 0.5f;
    float3 skinOutline = SrgbToLinear(float3(0.18f, 0.075f, 0.09f));
    float3 hairOutline = SrgbToLinear(float3(0.035f, 0.045f, 0.085f));
    float3 clothOutline = SrgbToLinear(float3(0.055f, 0.065f, 0.085f));
    float3 materialAwareOutlineColor = profile < 0.5f ? skinOutline : (profile < 1.5f ? hairOutline : clothOutline);
    mapped = lerp(mapped, materialAwareOutlineColor, outlineMask * toneMapParameters.z);
    return float4(LinearToSrgb(mapped), 1.0f);
}
