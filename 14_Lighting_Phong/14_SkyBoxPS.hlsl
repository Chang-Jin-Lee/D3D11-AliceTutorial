#include "14_SkyBox.hlsli"

float4 PS(SkyBoxVertexPosHL pIn) : SV_Target
{
    return g_TexCube.Sample(g_Sam, pIn.posL);
}