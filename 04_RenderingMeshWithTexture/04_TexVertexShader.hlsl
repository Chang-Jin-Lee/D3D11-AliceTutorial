#include "04_shared.fxh"
cbuffer ConstantBuffer : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 proj;
};

struct VSInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    float4 posW = mul(float4(input.position, 1.0f), world);
    float4 posV = mul(posW, view);
    o.position = mul(posV, proj);
    o.texcoord = input.texcoord;
    return o;
}