// 디퍼드 라이트 패스 Pixel Shader
// G-Buffer를 읽어서 단일 디렉션 라이트로 PBR 조명 계산

#include "35_DeferredShared.fxh"

float4 main(PS_INPUT_QUAD pIn) : SV_Target
{
    // G-Buffer 샘플링. 데이터 읽기
    float4 positionWS = g_PositionWS.Sample(g_Sam, pIn.uv);
    float4 normalWS_packed = g_NormalWS.Sample(g_Sam, pIn.uv);
    float4 metalness_packed = g_Metalness.Sample(g_Sam, pIn.uv);
    float4 roughness_packed = g_Roughness.Sample(g_Sam, pIn.uv);
    float4 baseColor = g_BaseColor.Sample(g_Sam, pIn.uv);
    
    // Position의 w값이 0이면 초기화된 배경색이거나 빈 공간임
    // 혹은 깊이 버퍼를 샘플링해서 1.0인지 확인하는 방법도 있음
    // 여기선 간단히 Normal 길이로 체크하거나 BaseColor Alpha로 체크
    if (length(normalWS_packed.xyz) < 0.1f) discard; // 노말이 없으면 배경으로 간주

    // 언패킹
    float3 posW = positionWS.xyz;
    float3 N = normalize(normalWS_packed.xyz * 2.0f - 1.0f); // 0~1을 -1~1로 변환
    float metalness = metalness_packed.r;
    float roughness = max(roughness_packed.r, 0.04f); // 최소 거칠기 보장
    float3 albedo = baseColor.rgb;
    float3 albedoLinear = pow(max(albedo, 0.0f), 2.2f);
    
    // 라이팅 벡터 계산
    float3 L = normalize(-g_LightDirection.xyz);
    float3 V = normalize(g_EyePosW - posW);
    float3 H = normalize(L + V);
    
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));
    
    // PBR 계산 (Cook-Torrance BRDF)
    // 1. Fresnel (F0 계산)
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedoLinear, metalness);
    
    // 2. Distribution (GGX)
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = (NdotH * NdotH * (a2 - 1.0f) + 1.0f);
    float D = a2 / (3.14159265f * denom * denom);
    
    // 3. Geometry (Smith)
    float k = (roughness + 1.0f) * (roughness + 1.0f) * 0.125f;
    float G1_L = NdotL / (NdotL * (1.0f - k) + k);
    float G1_V = NdotV / (NdotV * (1.0f - k) + k);
    float G = G1_L * G1_V;
    
    // 4. Fresnel (Schlick)
    float3 F = F0 + (1.0f - F0) * pow(1.0f - VdotH, 5.0f);
    
    // Cook-Torrance BRDF
    float3 specular = (D * G * F) / max(4.0f * NdotV * NdotL, 0.001f);
    
    // Diffuse (Lambert)
    float3 kS = F; // Specular contribution
    float3 kD = (1.0f - kS) * (1.0f - metalness); // Diffuse contribution
    float3 diffuse = kD * albedoLinear * INV_PI;
    
    // 최종 조명 계산
    float shadowVis = CalcShadowFactorDeferred(posW);
    
    float3 lightColor = g_LightColor.rgb * g_LightDirection.w;
    float3 color = (diffuse + specular) * lightColor * NdotL * shadowVis;
    
    // IBL (Image-Based Lighting) 추가
    {
        // Diffuse IBL (Irradiance)
        float3 diffuseIBL = kD * g_IBL_Diffuse.Sample(g_SamplerLinear, N).rgb * albedoLinear;
        
        // Specular IBL (Prefiltered + BRDF LUT)
        float3 R = reflect(-V, N);
        float3 prefilteredColor = g_IBL_Specular.SampleLevel(g_SamplerLinear, R, roughness * 8.0f).rgb;
        float2 BRDF = g_IBL_BRDF_LUT.Sample(g_SamplerLinear, float2(NdotV, roughness)).rg;
        float3 specularIBL = prefilteredColor * (F0 * BRDF.x + BRDF.y);
        
        float ao = saturate(g_PBRAmbientOcclusion);

        color += (diffuseIBL + specularIBL) * ao;
    }
    
    return float4(color, 1.0f);
}