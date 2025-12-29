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
    // 노말 언패킹 및 정규화 (Forward와 동일한 정규화 보장)
    float3 N_unpacked = normalWS_packed.xyz * 2.0f - 1.0f; // 0~1을 -1~1로 변환
    float3 N = normalize(N_unpacked); // Forward와 동일하게 정규화
    float metalness = metalness_packed.r;
    float roughness = max(roughness_packed.r, 0.04f); // 최소 거칠기 보장
    float3 albedo = baseColor.rgb;
    float3 albedoLinear = pow(max(albedo, 0.0f), 2.2f);
    
    // 공통으로 쓰일 라이팅 벡터들 (Forward와 동일)
    float3 L = normalize(-g_LightDirection.xyz);
    float3 V = normalize(g_EyePosW - posW);
    float3 H = normalize(L + V);
    
    float NdotL = dot(N, L);
    float theta = saturate(NdotL);
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));
    
    // PBR 계산 (Forward와 동일한 헬퍼 함수 사용)
    // 1. Albedo & Material Setup
    float3 albedoPBR = albedoLinear;
    roughness = max(roughness, 0.04f); // Forward와 동일: 최소 거칠기 보정
    float ao = saturate(g_PBRAmbientOcclusion);
    
    // 2. Direct Light Calculation (Forward와 동일)
    // F0 : 금속은 알베도, 비도체는 0.04 근처
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedoPBR, metalness);
    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, theta, roughness);
    float3 F = FresnelSchlick(F0, VdotH);
    
    // 스펙큘러 BRDF
    float3 numerator = D * G * F;
    float denomSpec = max(4.0f * NdotV * theta, 1e-4f);
    float3 specular = numerator / denomSpec;
    
    // 에너지 보존: 금속일수록 디퓨즈 감소
    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metalness);
    float3 diffuse = kD * albedoPBR * INV_PI;
    
    // 그림자 가시성 계산 (Forward와 동일한 함수 사용)
    float shadowVis = CalcShadowFactorDeferred(posW);
    
    // 라이트 계산 (Forward와 동일: g_DirLight.diffuse.rgb * PI * g_DirLight.intensity)
    // Deferred에서는 g_LightColor와 g_intensity를 사용하지만, Forward와 동일한 결과를 위해 변환
    float3 radiance = g_LightColor.rgb * PI;
    float3 directLighting = (diffuse + specular) * radiance * theta * ao * shadowVis * g_intensity;
    
    // 3. Indirect Light (IBL) - Forward와 동일
    // Diffuse IBL : Irradiance map을 법선 방향으로 샘플
    float3 diffuseIBL = kD * g_IBL_Diffuse.Sample(g_Sam, N).rgb * albedoPBR;
    
    // Specular IBL : 사전 필터된 스펙큘러 큐브맵 + BRDF LUT (split-sum 근사)
    float3 Renv = reflect(-V, N);
    const float kMaxSpecularMip = 8.0f;
    float3 prefilteredColor = g_IBL_Specular.SampleLevel(g_Sam, Renv, roughness * kMaxSpecularMip).rgb;
    float2 specBRDF = g_IBL_BRDF_LUT.Sample(g_ShadowSamp, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefilteredColor * (F0 * specBRDF.x + specBRDF.y);
    
    // AO로 환경광 전체를 감쇠
    float3 iblColor = (diffuseIBL + specularIBL) * ao;
    
    // 4. Final Combination
    float3 color = directLighting + iblColor;
    
    return float4(color, 1.0f);
}