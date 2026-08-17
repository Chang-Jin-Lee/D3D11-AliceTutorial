#include "36_Shared.fxh"

// ---------------------------------------------------------------------------
// 툰 셰이딩 - AliceEngine-Optimization의 ForwardShader.h ToonStepEditable 방식
//
// PBR을 버리지 않는다. 스페큘러/환경 반사/IBL/앰비언트는 그대로 두고, 디렉셔널
// 라이트의 디퓨즈 NdotL 하나만 밴딩한다. g_ToonEnabled가 1인 드로우콜에만
// 적용되므로 쇼케이스의 네 캐릭터만 툰이 되고 바닥과 스카이박스는 그대로다.
//
// 만질 노브는 셋이다. 값은 실제로 프레임을 찍어 보고 고른 것이다.
//
//  1) kToonSelfShadowStrength - NdotL이 표면을 얼마나 어둡게 할 수 있는지.
//     0이면 NdotL로는 전혀 어두워지지 않고, 1이면 평범한 램버트 그대로다.
//     가슴/턱 밑/치마 안쪽이 뭉개지는 문제를 여기서 잡는다.
//  2) kToonRampIntensity - 가장 어두운 밴드의 바닥값. 밴딩 결과가 이 아래로는
//     내려가지 않는다.
//  3) kToonCuts / kToonLevels - 밴드 경계와 각 밴드가 가지는 NdotL 값.
//     kToonCuts.w는 밴딩 전체 강도, kToonLevels.w는 fwidth 소프트 엣지 스위치.
//
// 측정값 (1600x900 프레임, 허벅지처럼 매끄러운 면을 가로질러 읽은 sRGB 휘도):
//   밴드 네 개가 144 / 163 / 187 / 212 / 231 근처로 평평하게 앉는다.
//   손대기 전에는 같은 자리가 194~233의 매끄러운 그라데이션이었고, 자기
//   그림자가 걸린 곳은 0(순수 검정)이었다.
// ---------------------------------------------------------------------------
static const float4 kToonCuts   = float4(0.10f, 0.26f, 0.42f, 0.95f); // (c1, c2, c3, strength)
static const float4 kToonLevels = float4(0.00f, 0.20f, 0.50f, 1.00f); // (l0, l1, l2, blur)
static const float3 kToonAlphas = float3(1.0f, 1.0f, 1.0f);           // 밴드별 적용 비율
static const float  kToonRampIntensity      = 0.04f;
static const float  kToonSelfShadowStrength = 0.96f;

float ToonStepEditable(float n, float3 cuts, float3 levels, float3 alphas,
                       float strength, float blur, float rampIntensity)
{
    float c1 = saturate(cuts.x);
    float c2 = max(saturate(cuts.y), c1 + 1e-4f);
    float c3 = max(saturate(cuts.z), c2 + 1e-4f);

    float l0 = saturate(levels.x);
    float l1 = saturate(levels.y);
    float l2 = saturate(levels.z);
    const float l3 = 1.0f;

    float a0 = saturate(alphas.x);
    float a1 = saturate(alphas.y);
    float a2 = saturate(alphas.z);
    const float a3 = 1.0f;

    float t = saturate(strength);
    float ramp = saturate(rampIntensity);

    float level;
    float alpha;
    float darkMask;
    if (blur > 0.5f)
    {
        // fwidth 소프트 엣지: 화면 공간에서 NdotL이 변하는 속도만큼만 경계를
        // 흐린다. 멀리 있는 슬롯(한 픽셀이 넓은 쪽)에서도 계단이 지지 않는다.
        float w = max(fwidth(n) * 1.0f, 0.010f);
        float s1 = smoothstep(c1 - w, c1 + w, n);
        float s2 = smoothstep(c2 - w, c2 + w, n);
        float s3 = smoothstep(c3 - w, c3 + w, n);

        level = lerp(lerp(lerp(l0, l1, s1), l2, s2), l3, s3);
        alpha = lerp(lerp(lerp(a0, a1, s1), a2, s2), a3, s3);
        darkMask = 1.0f - s1;
    }
    else
    {
        level = (n > c3) ? l3 : (n > c2) ? l2 : (n > c1) ? l1 : l0;
        alpha = (n > c3) ? a3 : (n > c2) ? a2 : (n > c1) ? a1 : a0;
        darkMask = (n > c1) ? 0.0f : 1.0f;
    }

    // 램프: 가장 어두운 밴드의 바닥을 올린다.
    //
    // 참고 구현은 여기서 어두운 밴드의 alpha를 깎아 원래 NdotL 쪽으로 되돌린다.
    // 이 씬에서는 그게 반대로 더 뭉갠다 - 어두운 밴드의 원래 NdotL이 이미 0에
    // 가깝기 때문이다. 그래서 이름 그대로 바닥을 올리는 쪽으로 바꿨다.
    // max()라 밴드 경계에서도 연속이다.
    level = lerp(level, max(level, ramp), darkMask);

    // lerp 꼬리: strength/alpha가 1이 아니면 원래 NdotL과 연속으로 이어진다.
    return lerp(n, level, t * alpha);
}

// 드로우콜 단위 스위치. 툰이 꺼진 오브젝트는 NdotL을 손대지 않고 그대로 돌려준다.
//
// 참고 구현은 NdotL > 0 일 때만 밴딩을 태운다. 하지만 saturate(NdotL)은 빛을 등진
// 반구 전체를 정확히 0으로 눕히므로, 그 분기는 터미네이터에 램프 바닥만큼 값이 툭
// 끊기는 계단을 하나 더 만든다 - 어느 노브로도 조절되지 않는 계단이다. 항상 태우면
// ToonStepEditable(0)이 곧 램프 바닥이라 함수가 연속이고, 터미네이터의 선명한
// 경계는 kToonCuts.x가 그대로 만들어 준다.
float ShadeToonDiffuse(float n)
{
    if (g_ToonEnabled == 0)
        return n;

    float banded = ToonStepEditable(n, kToonCuts.xyz, kToonLevels.xyz, kToonAlphas,
                                    kToonCuts.w, kToonLevels.w, kToonRampIntensity);

    // 자기 그림자 강도: 1이면 평범한 셰이딩, 0이면 NdotL로 전혀 어두워지지 않는다.
    return lerp(1.0f, banded, saturate(kToonSelfShadowStrength));
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
	//
	// g_UseDiffuseMap은 "알베도 맵이 있다"는 뜻이지 "ORM 맵이 있다"는 뜻이 아니다.
	// 예전에는 여기서 알베도의 .g/.b를 러프니스/메탈니스로 읽었다. 그 규약을 쓰는
	// 텍스처가 이 프로젝트에는 없다. VRoid 캐릭터 텍스처는 색상 맵이라 흰 옷은
	// 메탈니스가 살아 금속이 되고 검은 옷은 러프니스가 0이 되어 거울면이 됐다.
	// 둘 다 IBL 스카이박스를 반사해, 애니메이션을 따라 옷 위에서 반짝였다.
	// 재질은 스칼라 머티리얼 값이 정한다. 전용 ORM 맵이 생기면 자기 텍스처
	// 슬롯과 자기 플래그를 가져야 한다 - 알베도 슬롯 재사용이 이 버그였다.
	float4 kd;
	if (g_UseDiffuseMap != 0)
	{
		
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
	// 툰 오브젝트는 여기서 디퓨즈 NdotL만 밴딩된다. 스페큘러(Blinn/Cook-Torrance),
	// 환경 반사, IBL, 앰비언트는 전부 원래 theta를 그대로 쓴다.
	float thetaDiffuse = ShadeToonDiffuse(theta);

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
		float3 directLighting = (diffuse + specular) * radiance * thetaDiffuse * ao * shadowVis * g_DirLight.intensity;


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
    float4 diffuseTerm  = thetaDiffuse * g_DirLight.diffuse;
    float4 specularTerm = 0;

	// 분기별 조명 계산
	if (g_ShadingMode == 0)
	{
		// Phong
		float3 R = reflect(-L, N);
		float NdotV = saturate(dot(N, V));
		float specGate = step(0.0f, NdotL) * step(0.0f, NdotV);
		float s = pow(max(dot(R, V), 0.0f), max(g_Material.specular.w, 1.0f)) * specGate * g_DirLight.intensity;
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
        specularTerm = s * g_Material.specular * g_DirLight.specular * g_DirLight.intensity;
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
        return only * g_DirLight.intensity;
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
        litColor += (g_Material.reflect * reflectGate) * reflectionColor * g_DirLight.intensity;
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
        return outCol * g_DirLight.intensity;
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