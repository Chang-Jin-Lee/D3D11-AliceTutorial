#include "14_Shared.fxh"

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

    float3 normal = normalize(pIn.normalW);
    float3 light = normalize(-g_DirLight.direction);
    float3 eye = normalize(g_EyePosW - pIn.posW);
    // Phong: 반사벡터 기반 스펙큘러
    float3 reflectDir = reflect(-light, normal);

    /*
        @brief :
            낮은 shininess(?1)에서 specular lobe이 넓어져 뒤/엣지 하이라이트 꼬리가 보일 수 있음
        @details :
            - 억제: N·L > 0, N·V > 0 게이팅으로 구조적으로 차단
            - 나중에 배울 BSDF에서는 GGX(Tr/Trowbridge-Reitz) NDF + Smith G + Schlick F로 각도 의존 억제와 에너지 보존을 하기 때문에 구조적으로 해결 가능
    */
    float NdotL = dot(normal, light);
    float NdotV = dot(normal, eye);
    float theta = saturate(NdotL);
    float specGate = saturate(sign(theta)) * saturate(sign(NdotV));
    float specularScalar = pow(max(dot(reflectDir, eye), 0.0f), g_Material.specular.w) * specGate;

    // ambient/diffuse/specular
    float4 ambient  = g_Material.ambient * g_DirLight.ambient;
    float4 diffuse  = theta * g_DirLight.diffuse;
    float4 specular = specularScalar * g_Material.specular * g_DirLight.specular;

    // kd = texture * material.diffuse
    float4 kd = textureColor * g_Material.diffuse;

    float4 litColor = kd * (ambient + diffuse) + specular;

    // 환경 반사: 큐브맵 샘플 후 머티리얼 reflect와 곱해 합성
    /*
        @details :
            PBR 경로에선 반사 기여는 Fresnel(F_schlick)과 에너지 보존 하에 가중
    */
    float4 reflectionColor = g_TexCube.Sample(g_Sam, reflect(-eye, normal));
    litColor += g_Material.reflect * reflectionColor;
    litColor.a = kd.a * g_Material.diffuse.a;

    return litColor;
}