float4 VSFullscreen(uint vertexId : SV_VertexID) : SV_Position
{
    float2 positions[3] = { float2(-1.0, -1.0), float2(-1.0, 3.0), float2(3.0, -1.0) };
    return float4(positions[vertexId], 0.0, 1.0);
}

Texture2D<float4> opaqueTexture : register(t0);
Texture2D<float4> accumulationTexture : register(t1);
Texture2D<float> revealageTexture : register(t2);

cbuffer PresentConstants : register(b0)
{
    float exposure;
    float3 presentPadding;
};

float3 LoadNormalizedAccumulation(uint2 pixel)
{
    const float4 accumulation = accumulationTexture.Load(int3(pixel, 0));
    if (accumulation.a <= 1.0e-5 || !all(isfinite(accumulation)))
        return 0.0;
    const float3 normalized = accumulation.rgb / accumulation.a;
    return all(isfinite(normalized)) ? normalized : 0.0;
}

float4 PSCopy(float4 position : SV_Position) : SV_Target
{
    return opaqueTexture.Load(int3(uint2(position.xy), 0));
}

float4 PSResolve(float4 position : SV_Position) : SV_Target
{
    const uint2 pixel = uint2(position.xy);
    const float4 opaque = opaqueTexture.Load(int3(pixel, 0));
    const float4 accumulation = accumulationTexture.Load(int3(pixel, 0));
    const float revealage = saturate(revealageTexture.Load(int3(pixel, 0)));
    if (accumulation.a <= 1.0e-5)
        return opaque;
    const float3 weightedColor = LoadNormalizedAccumulation(pixel);
    const float3 resolved = weightedColor * (1.0 - revealage) + opaque.rgb * revealage;
    return float4(all(isfinite(resolved)) ? resolved : opaque.rgb, 1.0);
}

float3 LinearToSrgb(float3 linearColor)
{
    const float3 low = linearColor * 12.92;
    const float3 high = 1.055 * pow(max(linearColor, 0.0), 1.0 / 2.4) - 0.055;
    return lerp(high, low, linearColor <= 0.0031308);
}

float4 PrepareForDisplay(float3 linearColor)
{
    const float3 exposed = max(linearColor * max(exposure, 0.0), 0.0);
    const float3 toneMapped = exposed / (1.0 + exposed);
    return float4(LinearToSrgb(toneMapped), 1.0);
}

float4 PSPresentComposite(float4 position : SV_Position) : SV_Target
{
    return PrepareForDisplay(opaqueTexture.Load(int3(uint2(position.xy), 0)).rgb);
}

float4 PSPresentAccumulation(float4 position : SV_Position) : SV_Target
{
    return PrepareForDisplay(LoadNormalizedAccumulation(uint2(position.xy)));
}

float4 PSPresentRevealage(float4 position : SV_Position) : SV_Target
{
    const float revealage = saturate(revealageTexture.Load(int3(uint2(position.xy), 0)));
    return PrepareForDisplay(revealage.xxx);
}
