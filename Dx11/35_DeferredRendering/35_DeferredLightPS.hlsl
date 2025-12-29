// 디퍼드 라이트 패스 Pixel Shader
// G-Buffer에 있는 색상 정보를 이용해서 PBR 조명 연산
// 16bit Float Normal G-Buffer 사용으로 압축 해제 제거됨

#include "35_DeferredShared.fxh"

float4 main(PS_INPUT_QUAD pIn) : SV_Target
{
    // G-Buffer 가져오기. 샘플러 필요
    float4 positionWS = g_PositionWS.Sample(g_Sam, pIn.uv);
    float4 normalWS_packed = g_NormalWS.Sample(g_Sam, pIn.uv);
    float4 metalness_packed = g_Metalness.Sample(g_Sam, pIn.uv);
    float4 roughness_packed = g_Roughness.Sample(g_Sam, pIn.uv);
    float4 baseColor = g_BaseColor.Sample(g_Sam, pIn.uv);
    
    // Position의 w값이 0이면 물체가 그려지지 않은 배경임
    // 미리 클리어 컬러를 검정색이 아닌 1.0으로 밀어두고 이를 활용
    // 혹은 디퍼드에서는 Normal 길이로 판별하거나 BaseColor Alpha로 뺌
    if (length(normalWS_packed.xyz) < 0.1f) discard; // 노말 길이가 너무 작으면 배경

    // 데이터 복원
    float3 posW = positionWS.xyz;
    float3 N = normalize(normalWS_packed.xyz); // 16bit Float이므로 압축 해제 불필요
    float metalness = metalness_packed.r;
    float roughness = max(roughness_packed.r, 0.04f); // 최소 거칠기 보정
    float3 albedo = baseColor.rgb;
    float3 albedoLinear = pow(max(albedo, 0.0f), 2.2f);
    
    // 라이팅을 위한 벡터들 계산 (Forward와 동일)
    float3 L = normalize(-g_LightDirection.xyz);
    float3 V = normalize(g_EyePosW - posW);
    float3 H = normalize(L + V);
    
    float NdotL = dot(N, L);
    float theta = saturate(NdotL);
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));
    
    // PBR 연산 (Forward와 코드가 같아야 함을 유의)
    // 1. Albedo & Material Setup
    float3 albedoPBR = albedoLinear;
    roughness = max(roughness, 0.04f); // Forward와 맞춤: 최소 거칠기 보정
    float ao = saturate(g_PBRAmbientOcclusion);
    
    // 2. Direct Light Calculation (Forward와 동일)
    // F0 : 비금속 반사율, 보통 0.04 사용
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedoPBR, metalness);
    float D = DistributionGGX(NdotH, roughness);
    float G = GeometrySmith(NdotV, theta, roughness);
    float3 F = FresnelSchlick(F0, VdotH);
    
    // 쿡-토런스 BRDF
    float3 numerator = D * G * F;
    float denomSpec = max(4.0f * NdotV * theta, 1e-4f);
    float3 specular = numerator / denomSpec;
    
    // 에너지 보존: 반사된만큼 굴절(diffuse) 감소
    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metalness);
    float3 diffuse = kD * albedoPBR * INV_PI;
    
    // 그림자 팩터 계산 (Forward와 함수명 다를 수 있음)
    float shadowVis = CalcShadowFactorDeferred(posW);
    
    // 직접광 계산 (Forward에서는 g_DirLight.diffuse.rgb * PI * g_DirLight.intensity)
    // Deferred에서는 g_LightColor에 g_intensity가 녹아있는지, Forward와 로직 같은지 확인 필수
    float3 radiance = g_LightColor.rgb * PI;
    float3 directLighting = (diffuse + specular) * radiance * theta * ao * shadowVis * g_intensity;
    
    // 3. Indirect Light (IBL) - Forward와 동일
    // Diffuse IBL : Irradiance map 사용 난반사조명 근사
    float3 diffuseIBL = kD * g_IBL_Diffuse.Sample(g_Sam, N).rgb * albedoPBR;
    
    // Specular IBL : 미리 굽힌 환경맵 참조 + BRDF LUT (split-sum 근사)
    float3 Renv = reflect(-V, N);
    const float kMaxSpecularMip = 8.0f;
    float3 prefilteredColor = g_IBL_Specular.SampleLevel(g_Sam, Renv, roughness * kMaxSpecularMip).rgb;
    float2 specBRDF = g_IBL_BRDF_LUT.Sample(g_ShadowSamp, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefilteredColor * (F0 * specBRDF.x + specBRDF.y);
    
    // AO는 통상 전체에 곱해줌
    float3 iblColor = (diffuseIBL + specularIBL) * ao;
    
    // 4. Final Combination
    float3 color = directLighting + iblColor;
    
    return float4(color, 1.0f);
}