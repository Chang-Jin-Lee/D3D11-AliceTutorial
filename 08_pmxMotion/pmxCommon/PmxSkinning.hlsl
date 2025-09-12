cbuffer CameraCB : register(b0)
{
	float4x4 world;
	float4x4 view;
	float4x4 proj;
}
cbuffer BoneCB : register(b1)
{
	float4x4 gBones[60];
}
struct VSIn
{
	float3 pos : POSITION;
	float2 uv  : TEXCOORD0;
	uint4  bi  : BLENDINDICES;
	float4 bw  : BLENDWEIGHT;
};
struct VSOut
{
	float4 svpos : SV_POSITION;
	float2 uv    : TEXCOORD0;
};
VSOut VSMain(VSIn i)
{
	VSOut o;
	float4 p=float4(i.pos,1);
	float4 sk=0;
	[unroll] for(int k=0;k<4;++k){ if(i.bw[k]>1e-6 && i.bi[k]<60) sk += mul(p, gBones[i.bi[k]]) * i.bw[k]; }
	if (all(sk==0)) sk=p;
	float4 pw = mul(sk, world);
	float4 pv = mul(pw, view);
	o.svpos = mul(pv, proj);
	o.uv = i.uv;
	return o;
} 