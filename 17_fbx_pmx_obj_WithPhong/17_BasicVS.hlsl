#include "17_Shared.fxh"
// 정점 셰이더
VertexOut main(VertexIn vIn)
{
    VertexOut vOut;
    
    float4 posW = mul(float4(vIn.posL, 1.0f), g_World);
    vOut.posH = mul(posW, g_View);
    vOut.posH = mul(vOut.posH, g_Proj);
    vOut.posW = posW.xyz;

    // 노말 매핑을 위한 월드 공간 변환
    float3 nW = normalize(mul(vIn.normalL, (float3x3) g_WorldInvTranspose));
    float3 tW = normalize(mul(vIn.tangentL, (float3x3) g_World));
    float3 bW = normalize(mul(vIn.bitanL, (float3x3) g_World));
    // Ensure right-handed TBN (tangent, bitangent, normal)
    if (dot(cross(tW, bW), nW) < 0.0f)
    {
        bW = -bW;
    }

    vOut.normalW  = nW;
    vOut.tangentW = tW;
    vOut.bitanW   = bW;
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
    float4 posW = mul(float4(vIn.posL, 1.0f), g_World);
    vOut.posH = mul(posW, g_View);
    vOut.posH = mul(vOut.posH, g_Proj);
    vOut.posW = posW.xyz;
    vOut.normalW = normalize(mul(vIn.normalL, (float3x3)g_WorldInvTranspose));
    vOut.tangentW = float3(1,0,0);
    vOut.bitanW   = float3(0,1,0);
    vOut.tex = float2(0,0);
    vOut.color = vIn.color;
    return vOut;
}