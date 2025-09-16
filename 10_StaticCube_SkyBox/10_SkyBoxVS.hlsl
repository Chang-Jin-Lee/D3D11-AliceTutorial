#include "10_SkyBox.hlsli"

SkyBoxVertexPosHL VS(SkyBoxVertexPos vIn)
{
    SkyBoxVertexPosHL vOut;

    // z/w = 1.0f 로 고정하여 원근 투영 변환의 영향을 받지 않도록 합니다.
    // 왜 이렇게 하냐면 스카이 박스는 아주 먼 플랜에 있다고 가정하기 때문입니다
    float4 posH = mul(float4(vIn.posL, 1.0f), g_WorldViewProj);
    vOut.posH = posH.xyww;
    vOut.posL = vIn.posL;
    return vOut;
}