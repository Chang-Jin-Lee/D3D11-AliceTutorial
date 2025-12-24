#include "04_shared.fxh"

Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

struct PSInput
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
	return tex0.Sample(samp0, input.texcoord);
} 