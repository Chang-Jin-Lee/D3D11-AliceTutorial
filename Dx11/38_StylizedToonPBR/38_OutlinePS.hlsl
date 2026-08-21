#include "38_Shared.fxh"

Texture2D normalProfileTexture : register(t0);
Texture2D depthTexture : register(t1);
SamplerState linearSampler : register(s0);

float ReconstructViewDepth(float deviceDepth)
{
    float nearPlane = depthReconstructionParameters.x;
    float farPlane = depthReconstructionParameters.y;
    return nearPlane * farPlane / max(farPlane - deviceDepth * (farPlane - nearPlane), 0.0001f);
}

float MeasureEdge(float2 uv, float2 offset, float centerDepth, float3 centerNormal, float depthThreshold)
{
    float sampleDepth = depthTexture.SampleLevel(linearSampler, uv + offset, 0).r;
    bool centerHasGeometry = centerDepth < 0.9999f;
    bool sampleHasGeometry = sampleDepth < 0.9999f;
    if (centerHasGeometry != sampleHasGeometry)
        return 1.0f;
    if (!centerHasGeometry)
        return 0.0f;

    float centerViewDepth = ReconstructViewDepth(centerDepth);
    float sampleViewDepth = ReconstructViewDepth(sampleDepth);
    float relativeDepthDifference = abs(centerViewDepth - sampleViewDepth)
        / max(min(centerViewDepth, sampleViewDepth), depthReconstructionParameters.x);
    float3 sampleNormal = normalProfileTexture.SampleLevel(linearSampler, uv + offset, 0).rgb * 2.0f - 1.0f;
    float normalEdge = 1.0f - saturate(dot(centerNormal, normalize(sampleNormal)));
    float depthEdge = smoothstep(depthThreshold, depthThreshold * 2.5f, relativeDepthDifference);
    float normalSignal = smoothstep(outlineParameters.z, outlineParameters.z + outlineParameters.w, normalEdge);
    return max(depthEdge, normalSignal);
}

float PSMain(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float centerDepth = depthTexture.SampleLevel(linearSampler, uv, 0).r;
    bool centerHasGeometry = centerDepth < 0.9999f;
    float3 centerNormal = centerHasGeometry
        ? normalize(normalProfileTexture.SampleLevel(linearSampler, uv, 0).rgb * 2.0f - 1.0f)
        : float3(0.0f, 0.0f, 1.0f);
    float outlineWidth = outlineParameters.x;
    float outlineQuality = outlineParameters.y;
    float2 pixelOffset = inverseResolution.xy * outlineWidth;
    float resolutionScale = depthReconstructionParameters.w * inverseResolution.y;
    float depthThreshold = depthReconstructionParameters.z
        * max(resolutionScale, 0.35f)
        * max(outlineWidth, 0.5f);
    float edge = 0.0f;
    edge = max(edge, MeasureEdge(uv, float2(pixelOffset.x, 0.0f), centerDepth, centerNormal, depthThreshold));
    edge = max(edge, MeasureEdge(uv, float2(-pixelOffset.x, 0.0f), centerDepth, centerNormal, depthThreshold));
    edge = max(edge, MeasureEdge(uv, float2(0.0f, pixelOffset.y), centerDepth, centerNormal, depthThreshold));
    edge = max(edge, MeasureEdge(uv, float2(0.0f, -pixelOffset.y), centerDepth, centerNormal, depthThreshold));
    if (outlineQuality > 1.5f)
    {
        edge = max(edge, MeasureEdge(uv, pixelOffset, centerDepth, centerNormal, depthThreshold));
        edge = max(edge, MeasureEdge(uv, -pixelOffset, centerDepth, centerNormal, depthThreshold));
        edge = max(edge, MeasureEdge(uv, float2(pixelOffset.x, -pixelOffset.y), centerDepth, centerNormal, depthThreshold));
        edge = max(edge, MeasureEdge(uv, float2(-pixelOffset.x, pixelOffset.y), centerDepth, centerNormal, depthThreshold));
    }
    return saturate(edge);
}
