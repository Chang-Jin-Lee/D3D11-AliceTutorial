#include "39_Transparency.fxh"

cbuffer Surface : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 projection;
    float4 baseColor;
    float4 material; // mode, cutoff, opacity, skinning
    float4 surface;  // near, far, texture present, manual sRGB decode
};
// FbxModel already transposes its Assimp palette when uploading. These bytes are row matrices.
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
    float viewDepth : TEXCOORD1;
};
VertexOutput VSMain(VertexInput input)
{
    float4 p = float4(input.position, 1);
    float3 n = input.normal;
    if (material.w > 0.5)
    {
        float4x4 skin = input.weights.x * bonePalette[input.indices.x]
                     + input.weights.y * bonePalette[input.indices.y]
                     + input.weights.z * bonePalette[input.indices.z]
                     + input.weights.w * bonePalette[input.indices.w];
        p = mul(p, skin);
        n = mul(n, (float3x3)skin);
    }
    VertexOutput output;
    float4 viewPosition = mul(mul(p, world), view);
    output.position = mul(viewPosition, projection);
    output.viewDepth = viewPosition.z;
    output.normal = normalize(mul(n, (float3x3)world));
    output.uv = input.uv;
    output.color = input.color;
    return output;
}
float3 DecodeSrgb(float3 color)
{
    return float3(color.r <= 0.04045 ? color.r / 12.92 : pow(max((color.r + 0.055) / 1.055, 0), 2.4),
                  color.g <= 0.04045 ? color.g / 12.92 : pow(max((color.g + 0.055) / 1.055, 0), 2.4),
                  color.b <= 0.04045 ? color.b / 12.92 : pow(max((color.b + 0.055) / 1.055, 0), 2.4));
}
float4 Shade(VertexOutput input, bool frontFace)
{
    float4 texel = 1;
    if (surface.z > 0.5)
    {
        texel = baseTexture.Sample(linearSampler, input.uv);
        if (surface.w > 0.5) texel.rgb = DecodeSrgb(texel.rgb);
    }
    float4 color = texel * baseColor * input.color;
    color.a = saturate(color.a * material.z);
    // OPAQUE ignores texture alpha; MASK uses its selected cutoff and writes solid coverage.
    if (material.x > 0.5 && material.x < 1.5) clip(color.a - material.y);
    if (material.x < 1.5) color.a = 1;
    else if (color.a == 0) discard;
    float3 normal = normalize(input.normal) * (frontFace ? 1 : -1);
    float diffuse = saturate(dot(normal, normalize(float3(-0.35, 0.7, -0.6))));
    color.rgb *= 0.48 + 0.72 * diffuse;
    return color;
}
float4 PSMain(VertexOutput input, bool frontFace : SV_IsFrontFace) : SV_Target
{
    return Shade(input, frontFace);
}
OitOutput PSOit(VertexOutput input, bool frontFace : SV_IsFrontFace)
{
    float4 color = Shade(input, frontFace);
    float normalizedLinearDepth = saturate((input.viewDepth - surface.x) / (surface.y - surface.x));
    return MakeOitOutput(color.rgb, color.a, normalizedLinearDepth);
}
