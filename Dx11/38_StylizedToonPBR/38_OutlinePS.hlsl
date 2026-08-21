#include "38_Shared.fxh"

Texture2D normalProfileTexture : register(t0);
Texture2D depthTexture : register(t1);
SamplerState linearSampler : register(s0);

float MeasureEdge(float2 uv, float2 offset, float centerDepth, float3 centerNormal)
{
    float sampleDepth = depthTexture.SampleLevel(linearSampler, uv + offset, 0).r;
    float3 sampleNormal = normalProfileTexture.SampleLevel(linearSampler, uv + offset, 0).rgb * 2.0f - 1.0f;
    float depthEdge = centerDepth < 0.9999f && sampleDepth < 0.9999f ? abs(centerDepth - sampleDepth) * 45.0f : 0.0f;
    float normalEdge = 1.0f - saturate(dot(centerNormal, normalize(sampleNormal)));
    float depthNormalEdge = depthEdge + normalEdge * 2.25f;
    return depthNormalEdge;
}

float PSMain(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float centerDepth = depthTexture.SampleLevel(linearSampler, uv, 0).r;
    if (centerDepth >= 0.9999f)
        return 0.0f;

    float3 centerNormal = normalize(normalProfileTexture.SampleLevel(linearSampler, uv, 0).rgb * 2.0f - 1.0f);
    float outlineWidth = outlineParameters.x;
    float outlineQuality = outlineParameters.y;
    float2 pixelOffset = inverseResolution.xy * outlineWidth;
    float edge = 0.0f;
    edge = max(edge, MeasureEdge(uv, float2(pixelOffset.x, 0.0f), centerDepth, centerNormal));
    edge = max(edge, MeasureEdge(uv, float2(-pixelOffset.x, 0.0f), centerDepth, centerNormal));
    edge = max(edge, MeasureEdge(uv, float2(0.0f, pixelOffset.y), centerDepth, centerNormal));
    edge = max(edge, MeasureEdge(uv, float2(0.0f, -pixelOffset.y), centerDepth, centerNormal));
    if (outlineQuality > 1.5f)
    {
        edge = max(edge, MeasureEdge(uv, pixelOffset, centerDepth, centerNormal));
        edge = max(edge, MeasureEdge(uv, -pixelOffset, centerDepth, centerNormal));
        edge = max(edge, MeasureEdge(uv, float2(pixelOffset.x, -pixelOffset.y), centerDepth, centerNormal));
        edge = max(edge, MeasureEdge(uv, float2(-pixelOffset.x, pixelOffset.y), centerDepth, centerNormal));
    }
    return smoothstep(outlineParameters.z, outlineParameters.z + outlineParameters.w, edge);
}
