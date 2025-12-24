// G-Buffer 패스 Vertex Shader
// 월드 좌표, 월드 노말, 텍스처 좌표를 출력

#include "35_Shared.fxh"

// VertexIn과 VertexOut은 35_Shared.fxh에 정의되어 있음
// VertexIn에는 COLOR가 포함되어 있지만 G-Buffer 패스에서는 사용하지 않음
// VertexOut에는 posShadowH가 포함되어 있지만 G-Buffer 패스에서는 사용하지 않음

VertexOut main(VertexIn vIn)
{
    VertexOut vOut;
    
    // 월드 공간 위치 계산
    float4 posW = mul(float4(vIn.posL, 1.0f), g_World);
    vOut.posH = mul(posW, g_View);
    vOut.posH = mul(vOut.posH, g_Proj);
    vOut.posW = posW.xyz;
    
    // 월드 공간 노말, 탄젠트, 비탄젠트 변환
    vOut.normalW = normalize(mul(vIn.normalL, (float3x3)g_WorldInvTranspose));
    vOut.tangentW = normalize(mul(vIn.tangentL, (float3x3)g_World));
    vOut.bitanW = normalize(mul(vIn.bitanL, (float3x3)g_World));
    
    vOut.tex = vIn.tex;
    
    // G-Buffer 패스에서는 사용하지 않지만 초기화 필요 (경고 방지)
    vOut.color = vIn.color;
    vOut.posShadowH = float4(0, 0, 0, 1);  // G-Buffer 패스에서는 섀도우 사용 안 함
    
    return vOut;
}
