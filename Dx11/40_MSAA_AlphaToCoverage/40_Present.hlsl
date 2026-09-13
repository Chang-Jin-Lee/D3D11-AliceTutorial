Texture2D<float4> LinearTexture : register(t0);

cbuffer PresentConstants : register(b0)
{
    float Exposure;
    float3 PresentPadding;
};

struct FullscreenOutput
{
    float4 position : SV_POSITION;
};

FullscreenOutput VSFullscreen(uint vertexId : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };
    FullscreenOutput output;
    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    return output;
}

float3 LinearToDisplay(float3 hdr)
{
    const float3 x = max(hdr * Exposure, 0.0f);
    const float3 y = x / (1.0f + x);
    return float3(
        y.r <= 0.0031308f ? 12.92f * y.r : 1.055f * pow(y.r, 1.0f / 2.4f) - 0.055f,
        y.g <= 0.0031308f ? 12.92f * y.g : 1.055f * pow(y.g, 1.0f / 2.4f) - 0.055f,
        y.b <= 0.0031308f ? 12.92f * y.b : 1.055f * pow(y.b, 1.0f / 2.4f) - 0.055f);
}

float4 PSPresent(FullscreenOutput input) : SV_TARGET
{
    const float3 hdr = LinearTexture.Load(int3(input.position.xy, 0)).rgb;
    return float4(LinearToDisplay(hdr), 1.0f);
}
