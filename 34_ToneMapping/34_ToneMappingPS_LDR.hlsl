#include "34_Shared.fxh"


float3 LinearToSRGB(float3 linearColor)
{
    return pow(linearColor, 1.0f / 2.2f);
}

float4 main(PS_INPUT_QUAD input) : SV_Target
{
     // 1. 선형 HDR 값 로드 (Nits 값으로 간주)
    float3 C_linear709 = g_SceneHDR.Sample(g_SamplerLinear, input.uv).rgb;
    
    float exposureFactor = pow(2.0f, g_Exposure);
    C_linear709 *= exposureFactor;

    float3 C_tonemapped;
    C_tonemapped = ACESFilm(C_linear709);
   
    float3 C_final;
    C_final = LinearToSRGB(C_tonemapped);    
    return float4(C_final, 1.0) * g_Intensity;
}
