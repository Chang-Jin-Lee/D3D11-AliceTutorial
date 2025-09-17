#include "09_shared.fxh"

// 픽셀 셰이더(쉐이더/셰이더)
float4 main(VertexOut pIn) : SV_Target
{
    // 디버그: g_Pad == 1 -> 보라색, g_Pad == 2 -> 흰색 마커, g_Pad == 3 -> 빨간색 라인
    if (abs(g_Pad - 1.0f) < 1e-3)
    {
        return float4(1.0f, 0.0f, 1.0f, 1.0f);
    }
    if (abs(g_Pad - 2.0f) < 1e-3)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (abs(g_Pad - 3.0f) < 1e-3)
    {
        return float4(1.0f, 0.0f, 0.0f, 1.0f);
    }

    float3 baseColor = float3(1.0, 0.7, 1.0);
    // 월드 기준 램버트: L은 광원→표면 방향의 반대이므로 -direction 사용
    // diffuse specular 계산시 필요
    //float3 eyeVec = normalize(g_EyePosW - pIn.posW);

    float3 lightVec = -g_DirLight.direction;
    float diffuseFactor = saturate(dot(lightVec, pIn.normalW));
    
    float3 lit = 0.1 * g_DirLight.color.rgb + diffuseFactor * g_DirLight.color.rgb;
    float3 litColor = baseColor * lit;
    
    return float4(litColor, 1.0f);
}