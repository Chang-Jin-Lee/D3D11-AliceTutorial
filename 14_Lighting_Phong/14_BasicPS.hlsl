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
    // 알파 값을 반영합니다.
    // 텍스처 알파가 낮으면 픽셀을 제거해서 투명하게 만듭니다
    // 깊이를 미기록해서 뒤 배경/스카이박스가 보이도록 합니다
    float alphaTex = textureColor.a * g_Material.diffuse.a;
    clip(alphaTex - 0.1f);

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

    /*
        @details :
            환경 반사(러프니스): reflect.a를 러프니스로 보고 mip 바이어스로 변환해서 단일 샘플로 만듭니다
            - prefiltered env + LOD (여기선 SampleBias로 간단화)로 접근합니다
    */
    float roughness = saturate(g_Material.reflect.a);
    float3 rdir = reflect(-eye, normal);
    const float kMaxMip = 8.0f;                 // 필요 시 큐브맵 mip 수에 맞춰 조정해야합니다
    float mipBias = roughness * roughness * kMaxMip; // perceptual mapping
    float4 reflectionColor = g_TexCube.SampleBias(g_Sam, rdir, mipBias);
    /*
        @details :
            반사 게이팅: 조명 없는 면(N·L==0)에서는 반사도 0 → 거울처럼 뒤에서 보이는 현상 억제
    */
    float reflectGate = theta;                  // 필요시 pow(theta,2) 등으로 부드러운 롤오프
    litColor += (g_Material.reflect * reflectGate) * reflectionColor;
    // 마지막 색상에서의 알파 값은 텍스처 알파 값으로 덮어 씁니다
    litColor.a = alphaTex;

    return litColor;
}