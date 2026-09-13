#include "40_AlphaCoverage.fxh"

cbuffer Surface : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4 baseColor;
    float4 material; // SurfaceRule, cutoff, opacity, skinning
    float4 surface;  // texture present, manual sRGB decode, 0, 0
};

// FbxModel stores the uploaded Assimp palette as row matrices.
cbuffer Bones : register(b1) { row_major float4x4 bonePalette[1023]; };

Texture2D baseTexture : register(t0);
SamplerState linearSampler : register(s0);

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    uint4 indices : BLENDINDICES;
    float4 weights : BLENDWEIGHT;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(VertexInput input)
{
    float4 position = float4(input.position, 1.0f);
    float3 normal = input.normal;
    if (material.w > 0.5f)
    {
        float4x4 skin = input.weights.x * bonePalette[input.indices.x]
                      + input.weights.y * bonePalette[input.indices.y]
                      + input.weights.z * bonePalette[input.indices.z]
                      + input.weights.w * bonePalette[input.indices.w];
        position = mul(position, skin);
        normal = mul(normal, (float3x3)skin);
    }

    VertexOutput output;
    output.position = mul(mul(mul(position, world), view), projection);
    output.normal = normalize(mul(normal, (float3x3)world));
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

float3 DecodeSrgb(float3 color)
{
    return float3(
        color.r <= 0.04045f ? color.r / 12.92f : pow(max((color.r + 0.055f) / 1.055f, 0.0f), 2.4f),
        color.g <= 0.04045f ? color.g / 12.92f : pow(max((color.g + 0.055f) / 1.055f, 0.0f), 2.4f),
        color.b <= 0.04045f ? color.b / 12.92f : pow(max((color.b + 0.055f) / 1.055f, 0.0f), 2.4f));
}

float4 PSMain(VertexOutput input, bool frontFace : SV_IsFrontFace) : SV_Target
{
    float4 texel = 1.0f;
    if (surface.x > 0.5f)
    {
        texel = baseTexture.Sample(linearSampler, input.uv);
        if (surface.y > 0.5f)
            texel.rgb = DecodeSrgb(texel.rgb);
    }

    float4 color = texel * baseColor * input.color;
    color.a = ApplyCoverageAlpha(color.a * material.z,
        (uint)(material.x + 0.5f), material.y);

    float3 normal = normalize(input.normal) * (frontFace ? 1.0f : -1.0f);
    const float diffuse = saturate(dot(normal,
        normalize(float3(-0.35f, 0.7f, -0.6f))));
    color.rgb *= 0.48f + 0.72f * diffuse;
    return color;
}
