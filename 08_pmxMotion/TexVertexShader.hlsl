cbuffer ConstantBuffer : register(b0)
{
	float4x4 world;
	float4x4 view;
	float4x4 proj;
};

cbuffer BoneBuffer : register(b1)
{
	float4x4 gBones[60];
};

struct VSInput
{
	float3 position    : POSITION;
	float2 texcoord    : TEXCOORD0;
	uint4  blendIndex  : BLENDINDICES;
	float4 blendWeight : BLENDWEIGHT;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

VSOutput main(VSInput input)
{
	VSOutput o;
	float4 p = float4(input.position, 1.0f);
	// 4-º» ½ºÅ°´×
	float4 skinned = float4(0,0,0,0);
	[unroll]
	for (int i = 0; i < 4; ++i)
	{
		uint bi = input.blendIndex[i];
		float w = input.blendWeight[i];
		if (w > 0.00001f)
		{
			float4x4 M = gBones[bi];
			skinned += mul(p, M) * w;
		}
	}
	float4 posW = mul(skinned, world);
	float4 posV = mul(posW, view);
	o.position = mul(posV, proj);
	o.texcoord = input.texcoord;
	return o;
} 