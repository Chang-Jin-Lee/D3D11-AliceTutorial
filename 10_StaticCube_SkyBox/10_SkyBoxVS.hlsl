#include "10_SkyBox.hlsli"

SkyBoxVertexPosHL VS(SkyBoxVertexPos vIn)
{
    SkyBoxVertexPosHL vOut;

    // z/w = 1.0f �� �����Ͽ� ���� ���� ��ȯ�� ������ ���� �ʵ��� �մϴ�.
    // �� �̷��� �ϳĸ� ��ī�� �ڽ��� ���� �� �÷��� �ִٰ� �����ϱ� �����Դϴ�
    float4 posH = mul(float4(vIn.posL, 1.0f), g_WorldViewProj);
    vOut.posH = posH.xyww;
    vOut.posL = vIn.posL;
    return vOut;
}