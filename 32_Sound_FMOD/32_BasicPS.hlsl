#include "32_Shared.fxh"

static const float PI = 3.14159265f;

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

		// PBR 머티리얼:
		//  - UseTextureColor = 0 : 텍스처 색을 무시하고 고정 회색(0.5,0.5,0.5) 사용 (BaseColor UI 영향 없음)
		//  - UseTextureColor = 1 : 텍스처 색을 선형으로 디코딩 후 BaseColor로 틴트 (Blender 스타일)
		float3 albedoPBR;
		float metalness = saturate(g_PBRMetalness);
		float roughness = saturate(g_PBRRoughness);
		if (g_UseTextureColor != 0 && g_UseDiffuseMap != 0)
		{
			// 텍스처 색 + BaseColor
			float3 texLinear = pow(max(textureColor.rgb, 0.0f), 2.2f);
			albedoPBR = texLinear * g_PBRBaseColor.rgb;
			// Roughness/Metalness는 데이터 텍스처이므로 선형 그대로 사용
			metalness = saturate(metalness * metalnessTex);
			roughness = saturate(roughness * roughnessTex);
		}
		else
		{
			// 텍스처 색 사용 안 함: 고정 회색
			albedoPBR = g_PBRBaseColor.rgb;
		}
		// roughness = 0 이면 완전 거울이어야 하므로 아주 작은 값만 남기고 그대로 사용
		roughness = max(roughness, 0.001f);
		float ao = saturate(g_PBRAmbientOcclusion);

		// F0 : 금속은 알베도, 비도체는 0.04 근처
		float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedoPBR, metalness);

		// Cook-Torrance GGX (헬퍼 함수 사용: D, G, F)
		float D   = DistributionGGX(NdotH, roughness);
		float G   = GeometrySmith(NdotV, theta, roughness);
		float3 F  = FresnelSchlick(F0, VdotH);

		// 스펙큘러 BRDF
		float3 numerator = D * G * F;
		float denomSpec = max(4.0f * NdotV * theta, 1e-4f);
		float3 specular = numerator / denomSpec;

		// 에너지 보존: 금속일수록 디퓨즈 감소
		float3 kS = F;
		// 금속(metalness=1)일 때는 디퓨즈(난반사) 성분이 0이 되도록 kD 를 정의
		float3 kD = (1.0f - kS) * (1.0f - metalness);
		float3 diffuse = kD * albedoPBR * (1.0f / PI);

		// 단일 디렉션 라이트 강도
		// PBR diffuse가 1/PI로 나뉘므로 라이트 강도에 PI를 곱해서 보정
		// 추가로 밝기 보정을 위해 약간 더 강하게
		float3 radiance = g_DirLight.diffuse.rgb * PI * 1.1f;
		float3 color    = (diffuse + specular) * radiance * theta * ao;

		// --- 환경광(IBL) : Diffuse + Specular ------------------------------------
		// Diffuse IBL : Irradiance map 를 법선 방향으로 샘플
		//   → 직접광과 마찬가지로 kD 를 곱해서, 금속(metalness=1)일 때는 난반사를 0으로 만든다.
		float3 diffuseIBL = kD * g_IBL_Diffuse.Sample(g_Sam, N).rgb * albedoPBR;

		// Specular IBL : 사전 필터된 스펙큘러 큐브맵 + BRDF LUT (split-sum 근사)
		float3 Renv = reflect(-V, N);
		const float kMaxSpecularMip = 8.0f;       // BakerSampleSpecularHDR.dds 기준 예제 값
		float3 prefilteredColor = g_IBL_Specular.SampleLevel(
			g_Sam,
			Renv,
			roughness * kMaxSpecularMip).rgb;

		// BRDF LUT 에서 (NdotV, roughness)에 대한 평균 계수 A,B 를 가져온다.
		float2 specBRDF = g_IBL_BRDF_LUT.Sample(g_ShadowSamp, float2(NdotV, roughness)).rg;
		float3 specularIBL = prefilteredColor * (F0 * specBRDF.x + specBRDF.y);

		// AO로 환경광 전체를 감쇠
		float3 iblColor = (diffuseIBL + specularIBL) * ao;

		// 기존 단순 ambient 도 kD 를 곱해서 금속에는 거의 영향이 없도록 한다.
		float3 simpleAmbient = kD * albedoPBR * g_DirLight.ambient.rgb * ao * 0.1f;
		color += iblColor + simpleAmbient;

		// 감마 인코딩: 선형 공간 색상을 sRGB 공간으로 변환
		// sRGB = pow(Linear, 1/gamma) - 사람의 눈이 선형적으로 느끼도록
		// ImGui에서 g_Gamma 값을 조절할 수 있음 (기본값 2.2, 표준 sRGB 값)
		float gamma = max(g_Gamma, 0.1f);
		color = pow(saturate(color), 1.0f / gamma);

		return float4(color, alphaTex);
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

    // Shadowing (directional) with PCF
    if (g_ShadowEnabled != 0)
    {
        // VS에서 가져온 라이트 공간 좌표 사용.동차좌표계
        float3 sh = pIn.posShadowH.xyz / pIn.posShadowH.w;          // NDC
        float2 uv = sh.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);        // [0,1]
        float  depth = sh.z;                    // light clip depth

        // Early out if outside
        if (all(uv >= 0.0.xx) && all(uv <= 1.0.xx))
        {
            float texel = 1.0f / max(g_ShadowMapSize, 1.0f);
            float r = max(g_ShadowPCFRadius, 0.0f) * texel;
            // 3x3 PCF
            float sum = 0.0f; int taps = 0;
            [unroll]
            for (int dy = -1; dy <= 1; ++dy)
            {
                [unroll]
                for (int dx = -1; dx <= 1; ++dx)
                {
					float2 uvOff = uv + float2(dx,dy) * r;
					float d = g_ShadowMap.Sample(g_ShadowSamp, uvOff).r;
					if (depth - g_ShadowBias <= d)
					{
						sum += 1.0f;
					}
					else
					{
						sum += 0.0f;
					}
                    taps++;
                }
            }
            float vis = sum / max(taps, 1);
            litColor *= vis;
        }
    }

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