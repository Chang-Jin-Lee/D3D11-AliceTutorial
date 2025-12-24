#include "15_Shared.fxh"

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
        // Use vertex color to allow RGB axes via LineRenderer
        return pIn.color;
    }

    float4 textureColor = g_DiffuseMap.Sample(g_Sam, pIn.tex);
    float alphaTex = textureColor.a * g_Material.diffuse.a;
    clip(alphaTex - 0.1f);

    float3 N = normalize(pIn.normalW);
    float3 L = normalize(-g_DirLight.direction);
    float3 V = normalize(g_EyePosW - pIn.posW);

    float NdotL = dot(N, L);
    float theta = saturate(NdotL);

    float4 ambientTerm  = g_Material.ambient * g_DirLight.ambient;
    float4 diffuseTerm  = theta * g_DirLight.diffuse;
    float4 specularTerm = 0;

    // Phong vs Blinn-Phong
    if (g_ShadingMode == 0)
    {
        // Phong
        float3 R = reflect(-L, N);
        float NdotV = saturate(dot(N, V));
        float specGate = step(0.0f, NdotL) * step(0.0f, NdotV);
        float s = pow(max(dot(R, V), 0.0f), g_Material.specular.w) * specGate;
        specularTerm = s * g_Material.specular * g_DirLight.specular;
    }
    else if (g_ShadingMode == 1)
    {
        // Blinn-Phong
        float3 H = normalize(L + V);
        float NdotH = saturate(dot(N, H));
        float NdotV = saturate(dot(N, V));
        float specGate = step(0.0f, NdotL) * step(0.0f, NdotV);
        float s = pow(NdotH, g_Material.specular.w) * specGate;
        specularTerm = s * g_Material.specular * g_DirLight.specular;
    }
    else if (g_ShadingMode == 2)
    {
        // Lambert only
        specularTerm = 0;
    }
    else // Unlit
    {
        ambientTerm = 0;
        diffuseTerm = 0;
        specularTerm = 0;
    }

    // TextureOnly: 텍스처 색상 그대로 (라이팅 없음)
    if (g_ShadingMode == 4)
    {
        float4 only = textureColor * g_Material.diffuse;
        only.a = alphaTex;
        return only;
    }

    // kd = texture * material.diffuse
    float4 kd = textureColor * g_Material.diffuse;
    float4 litColor = kd * (ambientTerm + diffuseTerm) + specularTerm;

    // 환경 반사 (Unlit/Lambert에는 적용하지 않음)
    if (g_ShadingMode == 0 || g_ShadingMode == 1)
    {
        float3 Renv = reflect(-V, N);
        float roughness = saturate(g_Material.reflect.a);
        const float kMaxMip = 8.0f;
        float mipBias = roughness * roughness * kMaxMip;
        float4 reflectionColor = g_TexCube.SampleBias(g_Sam, Renv, mipBias);
        float reflectGate = theta;
        litColor += (g_Material.reflect * reflectGate) * reflectionColor;
    }

    litColor.a = alphaTex;
    return litColor;
}