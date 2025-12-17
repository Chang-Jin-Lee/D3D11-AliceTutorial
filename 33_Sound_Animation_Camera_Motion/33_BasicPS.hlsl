#include "33_Shared.fxh"

static const float PI			= 3.14159265f;
static const float TWO_PI		= 6.28318530718f;
static const float INV_PI		= 0.31830988618f; // 1 / PI
static const float INV_TWO_PI	= 0.15915494309f; // 1 / (2 * PI)

// -----------------------------------------------------------------------------
// PBR용 간단한 헬퍼 함수들. GGX, Schlick를 위한 것
// 각 함수가 의미하는 수식은 아래와 같습니다.
// -----------------------------------------------------------------------------

// NDF: GGX / Trowbridge-Reitz
//  D_ggx(n·h, α) = α² / ( π * ((n·h)²(α² - 1) + 1)² ),  α = roughness²
float DistributionGGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float denom = max(NdotH * NdotH * (a2 - 1.0f) + 1.0f, 1e-4f);
    return a2 / (PI * denom * denom);
}

// 단일 방향에 대한 Schlick-GGX Geometry term
//  G₁_schlick(n·x) = (n·x) / ( (n·x)(1 - k) + k ),  k = (r²) / 8,  r = roughness + 1
float GeometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) * 0.125f; // (r^2)/8, Epic에서 제안하는 형태
    return NdotX / (NdotX * (1.0f - k) + k);
}

// 양방향(뷰 + 라이트)에 대한 Smith Geometry term
//  G_smith(n·v, n·l) = G₁_schlick(n·v) * G₁_schlick(n·l)
float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float gv = GeometrySchlickGGX(NdotV, roughness);
    float gl = GeometrySchlickGGX(NdotL, roughness);
    return gv * gl;
}

// Schlick 근사 Fresnel
//  F_schlick(F?, cosθ) = F? + (1 - F?) * (1 - cosθ)?
float3 FresnelSchlick(float3 F0, float cosTheta)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

// -----------------------------------------------------------------------------
// 그림자 계산 함수
// -----------------------------------------------------------------------------
float CalcShadowFactor(float4 posShadowH)
{
    // 그림자가 꺼져 있으면 그림자 없음(1.0) 반환
    if (g_ShadowEnabled == 0)
        return 1.0f;

    // 동차 좌표계 나눗셈 (NDC 변환)
    float3 sh = posShadowH.xyz / posShadowH.w;
    
    // NDC [-1, 1] -> UV [0, 1] 변환 (Y축 반전)
    float2 uv = sh.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    float currentDepth = sh.z;

    // 쉐도우 맵 범위를 벗어났다면 그림자 없음 처리
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
    {
        return 1.0f;
    }

    // PCF (Percentage Closer Filtering)
    float texelSize = 1.0f / max(g_ShadowMapSize, 1.0f);
    float r = max(g_ShadowPCFRadius, 0.0f) * texelSize;
    float sum = 0.0f;
    int taps = 0;

    [unroll]
    for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx)
        {
            float2 uvOff = uv + float2(dx, dy) * r;
            float mapDepth = g_ShadowMap.Sample(g_ShadowSamp, uvOff).r;

            // 현재 깊이(bias 적용)가 맵의 깊이보다 작거나 같으면 빛을 받음(1.0)
            if (currentDepth - g_ShadowBias <= mapDepth)
            {
                sum += 1.0f;
            }
            // 아니면 그림자(0.0) -> sum에 더하지 않음
            
            taps++;
        }
    }

    return sum / max(taps, 1);
}

// 픽셀 셰이더
float4 main(VertexOut pIn) : SV_Target
{
	// 디버그 단축 경로들 (g_Pad)
	if (abs(g_Pad - 1.0f) < 1e-3) { return float4(1,0,1,1); }
	if (abs(g_Pad - 2.0f) < 1e-3) { return float4(1,1,1,1); }
	if (abs(g_Pad - 3.0f) < 1e-3) { return pIn.color; }

	// 아웃라인(멀티패스) 단색 출력: Strength로 강도 스케일
	if (abs(g_Pad - 6.0f) < 1e-3)
	{
		float s = saturate(g_OutlineStrength);
		return float4(g_OutlineColor.rgb * s, 1.0f);
	}

	// 알파 컷아웃: 텍스처 유무에 따라 분기
	float4 textureColor;
	if (g_UseDiffuseMap != 0)
	{
		textureColor = g_DiffuseMap.Sample(g_Sam, pIn.tex);
	}
	else
	{
		textureColor = float4(1,1,1,1);
	}

	float alphaBase;
	if (g_UseDiffuseMap != 0)
	{
		alphaBase = textureColor.a;
	}
	else
	{
		alphaBase = 1.0f;
	}

	float alphaTex = alphaBase * g_Material.diffuse.a;
	clip(alphaTex - 0.1f);

	// PBR을 사용하기 위한 베이스 알베도
	// BaseColor 텍스처는 sRGB로 저장되어 있으므로 선형 공간으로 변환 필요
	// Roughness/Metalness는 데이터 텍스처이므로 선형 그대로 사용
	float4 kd;
	float roughnessTex = 1.0f;
	float metalnessTex = 0.0f;
	if (g_UseDiffuseMap != 0)
	{
		// Roughness/Metalness는 데이터 텍스처이므로 선형 그대로 (감마 디코딩 전에 미리 저장)
		roughnessTex = textureColor.g;
		metalnessTex = textureColor.b;
		
		// BaseColor는 sRGB 텍스처를 선형 공간으로 디코딩: Linear = pow(sRGB, 2.2)
		float3 linearColor = pow(max(textureColor.rgb, 0.0f), 2.2f);
		kd = float4(linearColor, textureColor.a) * g_Material.diffuse;
	}
	else
	{
		kd = float4(1, 1, 1, 1) * g_Material.diffuse;
	}
	float3 albedo = kd.rgb;

	// 월드 노말 계산(N) - 기본은 정점 노멀, 필요 시 노말맵에서 덮어쓰기
	float3 N = normalize(pIn.normalW);
	
	if (g_EnableNormalMap != 0)
	{
		float3 T = normalize(pIn.tangentW);
		float3 B = normalize(pIn.bitanW);
		float handed = dot(cross(T, B), N);
		if (handed < 0.0f) B = -B;
		float3x3 TBN = float3x3(T, B, N);
		float3 N_ts = g_NormalMap.Sample(g_Sam, pIn.tex).xyz * 2.0f - 1.0f;
		N_ts.y = -N_ts.y; // 그린 채널 반전 보정
		N_ts = normalize(N_ts);
		N = normalize(mul(N_ts, TBN));
	}

	// 공통으로 쓰일 라이팅 벡터들
	float3 L = normalize(-g_DirLight.direction);
	float3 V = normalize(g_EyePosW - pIn.posW);
	float NdotL = dot(N, L);
	float theta = saturate(NdotL);

	// PBR Shading (Cook-Torrance, 단일 디렉션 라이트)
	if (g_ShadingMode == 6)
	{
		float3 H     = normalize(L + V);
		float  NdotV = saturate(dot(N, V));
		float  NdotH = saturate(dot(N, H));
		float  VdotH = saturate(dot(V, H));

		// 1. Albedo & Material Setup
		// 텍스처 색상을 Linear로 변환한 값을 미리 준비 (최적화: pow 중복 제거)
		float3 texLinear = (g_UseDiffuseMap != 0) 
			? pow(max(textureColor.rgb, 0.0f), 2.2f) 
			: float3(1, 1, 1);

		// =================================== 1. Albedo & Material Setup ==================================
		float3 albedoPBR;
		float metalness = saturate(g_PBRMetalness);
		float roughness = saturate(g_PBRRoughness);

		if (g_UseTextureColor != 0 && g_UseDiffuseMap != 0)
		{
			albedoPBR = texLinear * g_PBRBaseColor.rgb;
			metalness = saturate(metalness * metalnessTex);
			roughness = saturate(roughness * roughnessTex);
		}
		else
		{
			// 텍스처 색 사용 안 함: 고정 회색
			albedoPBR = g_PBRBaseColor.rgb;
		}
		// roughness = 0 이면 완전 거울이어야 하므로 아주 작은 값만 남기고 그대로 사용
		roughness = max(roughness, 0.04f);
		float ao = saturate(g_PBRAmbientOcclusion);

		// =================================== 2. Direct Light Calculation ==================================
		// F0 : 금속은 알베도, 비도체는 0.04 근처
		float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedoPBR, metalness);
		float D   = DistributionGGX(NdotH, roughness);
		float G   = GeometrySmith(NdotV, theta, roughness);
		float3 F  = FresnelSchlick(F0, VdotH);

		// 스펙큘러 BRDF
		float3 numerator = D * G * F;
		float denomSpec = max(4.0f * NdotV * theta, 1e-4f);
		float3 specular = numerator / denomSpec;

		// 에너지 보존: 금속일수록 디퓨즈 감소
		float3 kS = F;
		float3 kD = (1.0f - kS) * (1.0f - metalness);
		float3 diffuse = kD * albedoPBR * INV_PI;

		float shadowVis = CalcShadowFactor(pIn.posShadowH);
		float3 radiance = g_DirLight.diffuse.rgb * PI;
		float3 directLighting = (diffuse + specular) * radiance * theta * ao * shadowVis;


		// =================================== 3. Indirect Light (IBL) ==================================
		// --- 환경광(IBL) : Diffuse + Specular ------------------------------------
		// Diffuse IBL : Irradiance map 를 법선 방향으로 샘플
		float3 diffuseIBL = kD * g_IBL_Diffuse.Sample(g_Sam, N).rgb * albedoPBR;

		// Specular IBL : 사전 필터된 스펙큘러 큐브맵 + BRDF LUT (split-sum 근사)
		float3 Renv = reflect(-V, N);
		const float kMaxSpecularMip = 8.0f;
		float3 prefilteredColor = g_IBL_Specular.SampleLevel(g_Sam, Renv, roughness * kMaxSpecularMip).rgb;
		float2 specBRDF = g_IBL_BRDF_LUT.Sample(g_ShadowSamp, float2(NdotV, roughness)).rg;
		float3 specularIBL = prefilteredColor * (F0 * specBRDF.x + specBRDF.y);

		// AO로 환경광 전체를 감쇠
		float3 iblColor = (diffuseIBL + specularIBL) * ao;

		// =================================== 4. Final Combination ==================================
		float3 finalColor = directLighting + iblColor;
		return float4(finalColor, alphaTex);
	}

	float4 ambientTerm  = g_Material.ambient * g_DirLight.ambient;
    float4 diffuseTerm  = theta * g_DirLight.diffuse;
    float4 specularTerm = 0;

	// 분기별 조명 계산
	if (g_ShadingMode == 0)
	{
		// Phong
		float3 R = reflect(-L, N);
		float NdotV = saturate(dot(N, V));
		float specGate = step(0.0f, NdotL) * step(0.0f, NdotV);
		float s = pow(max(dot(R, V), 0.0f), max(g_Material.specular.w, 1.0f)) * specGate;
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
		// Lambert
		specularTerm = 0;
	}
	// Unlit (3): 조명 없이 텍스처*diffuse만
	else if (g_ShadingMode == 3)
	{
		ambientTerm = 0;
        diffuseTerm = 0;
        specularTerm = 0;
	}
	// TextureOnly (4): 텍스처만 출력(없으면 머티리얼로 대체)
	else if (g_ShadingMode == 4)
	{
		float4 only;
		if (g_UseDiffuseMap != 0)
		{
			only = textureColor * g_Material.diffuse;
		}
		else
		{
			only = g_Material.diffuse;
		}
        only.a = alphaTex;
        return only;
	}
	
	// kd = (useTex ? texture : 1) * material.diffuse
    float4 litColor = kd * (ambientTerm + diffuseTerm) + specularTerm;
	float legacyShadowVis = CalcShadowFactor(pIn.posShadowH);
    litColor *= legacyShadowVis;

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

    // ToonShading (5): 램프 셰이딩만 적용 (림/외곽선 제거)
    if (g_ShadingMode == 5)
    {
		const int kSteps = 5;
		float q;
		if (kSteps > 1)
		{
			q = floor(theta * (kSteps - 1) + 0.5f) / (kSteps - 1);
		}
		else
		{
			q = theta;
		}
        float4 toonDiffuse = kd * (ambientTerm + q * g_DirLight.diffuse);
        float3 H = normalize(L + V);	
        float NdotH = saturate(dot(N, H));
        float specBand = smoothstep(0.85f, 0.95f, NdotH) * step(0.0f, NdotL);
        float4 toonSpec = specBand * g_Material.specular * g_DirLight.specular;
        float4 outCol = toonDiffuse + toonSpec;
        outCol.a = alphaTex;
        return outCol;
    }

    litColor.a = alphaTex;
    return litColor;
}

// ---------------- Outline Pass (Geometry-based) ----------------
struct PSInOutline
{
    float4 posH : SV_POSITION;
};

float4 PSOutline(PSInOutline i) : SV_Target
{
    float s = saturate(g_OutlineStrength);
    return float4(g_OutlineColor.rgb * s, 1.0f);
}