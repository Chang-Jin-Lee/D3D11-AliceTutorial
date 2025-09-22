#include "12_Shared.fxh"

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

    float3 lightVec = -g_DirLight.direction;
    float3 eyeVec = normalize(g_EyePosW - pIn.posW);
    float4 ambient, diffuse, specular;
    ambient = diffuse = specular = float4(0.0f, 0.0f, 0.0f, 0.0f); // 초기화 

    // ambient
    ambient = g_Material.ambient * g_DirLight.ambient; // 환경광

    // diffuse
    float3 H = normalize(lightVec + eyeVec); // 반사 벡터, 일단은 중간 벡터로 대체
    float specularScalar = diffuse = pow(saturate(dot(pIn.normalW, H)), g_Material.specular.w); // 광택 계산
    diffuse = specularScalar * g_DirLight.diffuse;

    // specular
    specular = specularScalar * g_Material.specular * g_DirLight.specular;
    
    float4 litColor = pIn.color * (ambient + diffuse) + specular;
    litColor.a = g_Material.diffuse.a * pIn.color.a;
    
    return litColor;
}