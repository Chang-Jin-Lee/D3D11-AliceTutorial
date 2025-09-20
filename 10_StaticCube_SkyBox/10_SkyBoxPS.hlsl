#include "10_Skybox.hlsli"

// 여기서 z축 한번 더 반전
float4 PS(SkyBoxVertexPosHL pIn) : SV_Target
{
    return g_TexCube.Sample(g_Sam, float3(pIn.posL.x, pIn.posL.y, -pIn.posL.z));
}