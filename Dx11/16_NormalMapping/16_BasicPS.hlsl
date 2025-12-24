#include "16_Shared.fxh"

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
    float alphaTex = textureColor.a * g_Material.diffuse.a;
    clip(alphaTex - 0.1f);

    // Normal mapping: normal map is in tangent space (DXT5nm style, assume [0,1]->[-1,1])
    float3 N_ts = g_NormalMap.Sample(g_Sam, pIn.tex).xyz * 2.0f - 1.0f;
    N_ts = normalize(N_ts);

    // Build TBN matrix from interpolated world-space T,B,N
    float3x3 TBN = float3x3(normalize(pIn.tangentW), normalize(pIn.bitanW), normalize(pIn.normalW));
    float3 normal = normalize(mul(N_ts, TBN));

    float3 light = normalize(-g_DirLight.direction);
    float3 eye = normalize(g_EyePosW - pIn.posW);
    float3 reflectDir = reflect(-light, normal);

    float NdotL = dot(normal, light);
    float NdotV = dot(normal, eye);
    float theta = saturate(NdotL);
    float specGate = saturate(sign(theta)) * saturate(sign(NdotV));

    // Per-face specular intensity from texture (gray-scale expected)
    float specTex = g_SpecularMap.Sample(g_Sam, pIn.tex).r;
    float shininess = max(g_Material.specular.w, 1.0f);
    float specularScalar = pow(max(dot(reflectDir, eye), 0.0f), shininess) * specGate * specTex;

    // ambient/diffuse/specular
    float4 ambient  = g_Material.ambient * g_DirLight.ambient;
    float4 diffuse  = theta * g_DirLight.diffuse;
    float4 specular = specularScalar * g_Material.specular * g_DirLight.specular;

    // kd = texture * material.diffuse
    float4 kd = textureColor * g_Material.diffuse;

    float4 litColor = kd * (ambient + diffuse) + specular;

    // 환경 반사(러프니스)
    float roughness = saturate(g_Material.reflect.a);
    float3 rdir = reflect(-eye, normal);
    const float kMaxMip = 8.0f;
    float mipBias = roughness * roughness * kMaxMip;
    float4 reflectionColor = g_TexCube.SampleBias(g_Sam, rdir, mipBias);
    // 반사 게이팅 적용. 조명 없는 면(N·L==0)에서는 거울처럼 뒤에서 보이는 현상을 제거하기 위해서 사용합니다
    float reflectGate = theta;
    litColor += (g_Material.reflect * reflectGate) * reflectionColor;
    // 마지막 색상에서의 알파 값은 텍스처 알파 값으로 덮어 씁니다
    litColor.a = alphaTex;

    return litColor;
}