#include "28_Shared.fxh"
// 정점 셰이더
VertexOut main(VertexIn vIn)
{
	VertexOut vOut;
	
	float4 posW = mul(float4(vIn.posL, 1.0f), g_World);
	vOut.posH = mul(posW, g_View);
	vOut.posH = mul(vOut.posH, g_Proj);
	vOut.posW = posW.xyz;
    // 라이트 공간 투영 좌표 저장(PS에서 사용)
    vOut.posShadowH = mul(posW, g_LightViewProj);

	// 노말 매핑을 위한 월드 공간 변환
	float3 nW = normalize(mul(vIn.normalL, (float3x3) g_WorldInvTranspose));
	float3 tW = mul(vIn.tangentL, (float3x3) g_World);
	float3 bW = mul(vIn.bitanL,   (float3x3) g_World);

	// 최종 할당 Right-handed는 b = n x t
	vOut.normalW  = nW;
	vOut.tangentW = tW;
	vOut.bitanW   = bW;

	vOut.tex = vIn.tex;
	vOut.color = vIn.color;
	
	return vOut;
}

// 본 팔레트(g_BonePalette)로 로컬을 스키닝 후 월드/뷰/프로젝션을 적용
VertexOut VSSkinned(VertexInSkinned vIn)
{
	VertexOut vOut;

	// 스키닝 행렬 합
	uint4 bi = vIn.boneIdx;
	float4 bw = vIn.boneW;
	// DirectX11(행벡터) 기준: v' = v * (Σ w_i * M_i)
	matrix M = bw.x * g_BonePalette[bi.x]
		     + bw.y * g_BonePalette[bi.y]
		     + bw.z * g_BonePalette[bi.z]
		     + bw.w * g_BonePalette[bi.w];

	float4 posL = float4(vIn.posL, 1.0f);
	float3 nL = vIn.normalL;
	float3 tL = vIn.tangentL;
	float3 bL = vIn.bitanL;

	float4 skinnedPos = mul(posL, M);
	// 법선/탄젠트/비탄젠트는 3x3 부분을 사용 (M은 CPU에서 전치 업로드됨)
	float3x3 M3 = (float3x3)M;
	float3 skinnedN   = normalize(mul(nL, M3));
	float3 skinnedT   = normalize(mul(tL, M3));
	float3 skinnedB   = normalize(mul(bL, M3));

	float4 posW = mul(skinnedPos, g_World);
	vOut.posH = mul(mul(posW, g_View), g_Proj);
	vOut.posW = posW.xyz;
    vOut.posShadowH = mul(posW, g_LightViewProj);
	vOut.normalW = normalize(mul(skinnedN, (float3x3)g_WorldInvTranspose));
	vOut.tangentW = mul(skinnedT, (float3x3)g_World);
	vOut.bitanW   = mul(skinnedB, (float3x3)g_World);
	vOut.tex = vIn.tex;
	vOut.color = vIn.color;
	return vOut;
}

// PMX(노말맵 미사용) 전용: TBN 없는 입력 레이아웃용 VS
struct VertexInNoTBN
{
	float3 posL   : POSITION;
	float3 normalL: NORMAL;
	float2 tex    : TEXCOORD;
	float4 color  : COLOR;
};

VertexOut VSNoTBN(VertexInNoTBN vIn)
{
	VertexOut vOut;
	float4 posW = mul(float4(vIn.posL, 1.0f), g_World);
	vOut.posH = mul(posW, g_View);
	vOut.posH = mul(vOut.posH, g_Proj);
	vOut.posW = posW.xyz;
    vOut.posShadowH = mul(posW, g_LightViewProj);

	vOut.normalW = normalize(mul(vIn.normalL, (float3x3) g_WorldInvTranspose));
	vOut.tangentW = float3(1,0,0);
	vOut.bitanW   = float3(0,1,0);

	vOut.tex = vIn.tex;
	vOut.color = vIn.color;

	return vOut;
}

// 라인/축 전용 VS (POSITION, NORMAL, COLOR 전용)
struct VertexInLine
{
	float3 posL    : POSITION;
	float3 normalL : NORMAL;
	float4 color   : COLOR;
};

VertexOut VSLine(VertexInLine vIn)
{
	VertexOut vOut;
    float4 p = float4(vIn.posL, 1.0f);
    // 루트 본 인덱스가 지정되면 팔레트 행렬을 먼저 적용해 애니메이션 루트 변환을 반영
    if (g_BoundsBoneIndex >= 0)
    {
        p = mul(p, g_BonePalette[g_BoundsBoneIndex]);
    }
    float4 posW = mul(p, g_World);
	vOut.posH = mul(posW, g_View);
	vOut.posH = mul(vOut.posH, g_Proj);
	vOut.posW = posW.xyz;
	vOut.normalW = normalize(mul(vIn.normalL, (float3x3)g_WorldInvTranspose));
	vOut.tangentW = float3(1,0,0);
	vOut.bitanW   = float3(0,1,0);
	vOut.tex = float2(0,0);
	vOut.color = vIn.color;
    vOut.posShadowH = mul(posW, g_LightViewProj);
	return vOut;
}

// ---------------- Outline Pass (Geometry-based) ----------------
struct VSInOutline
{
    float3 posL   : POSITION;
    float3 normalL: NORMAL;
};

struct VSOutOutline
{
    float4 posH : SV_POSITION;
};

VSOutOutline VSOutline(VSInOutline vIn)
{
    VSOutOutline o;
    // View-space XY 팽창: 깊이는 유지하여 실루엣만 보이도록
    float4 posW = mul(float4(vIn.posL, 1.0f), g_World);
    float4 posV = mul(posW, g_View);
    float3 nW = normalize(mul(vIn.normalL, (float3x3) g_WorldInvTranspose));
    float3 nV = normalize(mul(nW, (float3x3) g_View));
    float2 dir = nV.xy;
    float len = max(length(dir), 1e-5);
    dir /= len;
    posV.xy += dir * g_OutlineThickness;
    o.posH = mul(posV, g_Proj);
    return o;
}


// 아웃라인 전용(스키닝)
VertexOut VSSkinnedOutline(VertexInSkinned vIn)
{
	VertexOut vOut;
	uint4 bi = vIn.boneIdx; float4 bw = vIn.boneW;
	matrix M = bw.x * g_BonePalette[bi.x]
			 + bw.y * g_BonePalette[bi.y]
			 + bw.z * g_BonePalette[bi.z]
			 + bw.w * g_BonePalette[bi.w];
	float4 skinnedPos = mul(float4(vIn.posL,1.0f), M);
	float3x3 M3 = (float3x3)M;
	float3 skinnedN = normalize(mul(vIn.normalL, M3));
	// 월드→뷰 변환
	float4 posW = mul(skinnedPos, g_World);
	float4 posV = mul(posW, g_View);
	float3 nW = normalize(mul(skinnedN, (float3x3)g_WorldInvTranspose));
    float3 nV = normalize(mul(nW, (float3x3)g_View));
    // 화면 두께가 보이도록 z 성분 제거 후 XY 평면으로만 팽창
    float2 nVP = normalize(max(abs(nV.x) + abs(nV.y), 1e-5) * (nV.xy / max(length(nV.xy), 1e-5)));
    posV.xy += nVP * g_OutlineThickness;
    // 프로젝션
    vOut.posH = mul(posV, g_Proj);
    vOut.posW = posW.xyz;
	vOut.normalW = nW;
	vOut.tangentW = float3(1,0,0);
	vOut.bitanW   = float3(0,1,0);
	vOut.tex = vIn.tex;
	vOut.color = float4(0,0,0,1);
	return vOut;
}

// ---------------- Shadow Depth Pass ----------------
struct VSShadowOut
{
    float4 posH : SV_POSITION;
};

VSShadowOut VSShadow(VertexIn vIn)
{
    VSShadowOut o;
    float4 posW = mul(float4(vIn.posL, 1.0f), g_World);
    o.posH = mul(posW, g_LightViewProj);
    return o;
}

VSShadowOut VSSkinnedShadow(VertexInSkinned vIn)
{
    VSShadowOut o;
    uint4 bi = vIn.boneIdx; float4 bw = vIn.boneW;
    matrix M = bw.x * g_BonePalette[bi.x]
             + bw.y * g_BonePalette[bi.y]
             + bw.z * g_BonePalette[bi.z]
             + bw.w * g_BonePalette[bi.w];
    float4 skinnedPos = mul(float4(vIn.posL,1.0f), M);
    float4 posW = mul(skinnedPos, g_World);
    o.posH = mul(posW, g_LightViewProj);
    return o;
}