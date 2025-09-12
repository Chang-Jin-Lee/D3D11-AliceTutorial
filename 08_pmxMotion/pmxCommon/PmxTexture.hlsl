Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

struct PSIn
{
	float4 svpos : SV_POSITION;
	float2 uv    : TEXCOORD0;
};

float4 PSMain(PSIn i) : SV_TARGET
{
	return tex0.Sample(samp0, i.uv);
} 