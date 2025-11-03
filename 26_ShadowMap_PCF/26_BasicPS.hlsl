#include "26_Shared.fxh"

// 픽셀 셰이더(쉐이더/셰이더)
float4 main(VertexOut pIn) : SV_Target
{
	// 디버그 단축 경로들 (g_Pad)
	if (abs(g_Pad - 1.0f) < 1e-3) { return float4(1,0,1,1); }
	if (abs(g_Pad - 2.0f) < 1e-3) { return float4(1,1,1,1); }
	if (abs(g_Pad - 3.0f) < 1e-3) { return pIn.color; }
	// 이번 프로젝트 코드
	//////////////////////////////////////////////////////////////////////////
	// 아웃라인(멀티패스) 단색 출력: Strength로 강도 스케일
	if (abs(g_Pad - 6.0f) < 1e-3)
	{
		float s = saturate(g_OutlineStrength);
		return float4(g_OutlineColor.rgb * s, 1.0f);
	}


	// 알파 컷아웃 조명 모드에서만 적용
	float4 textureColor = g_DiffuseMap.Sample(g_Sam, pIn.tex);
    float alphaTex = textureColor.a * g_Material.diffuse.a;
    clip(alphaTex - 0.1f);

	// 월드 노말 계산(Nw) - 노말맵 토글에 따라 분기
	float3 N = normalize(pIn.normalW);
	if (g_EnableNormalMap != 0)
	{
		float3 T = normalize(pIn.tangentW);
		float3 N = normalize(pIn.normalW);
		float3 B = normalize(pIn.bitanW);
		float handed = dot(cross(T, B), N);
		if (handed < 0.0f) B = -B;
		float3x3 TBN = float3x3(T, B, N);
		float3 N_ts = g_NormalMap.Sample(g_Sam, pIn.tex).xyz * 2.0f - 1.0f;
		N_ts.y = -N_ts.y; // 그린 채널 반전 보정
		N_ts = normalize(N_ts);
		N = normalize(mul(N_ts, TBN));
	}

	// 공통: 라이팅 벡터들
	float3 L = normalize(-g_DirLight.direction);
	float3 V = normalize(g_EyePosW - pIn.posW);
	float NdotL = dot(N, L);
	float theta = saturate(NdotL);

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
	// TextureOnly (4): 텍스처만 출력
	else if (g_ShadingMode == 4)
	{
		float4 only = textureColor * g_Material.diffuse;
        only.a = alphaTex;
        return only;
	}
	
	// kd = texture * material.diffuse
    float4 kd = textureColor * g_Material.diffuse;
    float4 litColor = kd * (ambientTerm + diffuseTerm) + specularTerm;

    // Shadowing (directional) with PCF
    if (g_ShadowEnabled != 0)
    {
        // Transform world pos to shadow map UV
        float4 posL = mul(float4(pIn.posW,1.0f), g_LightViewProj);
        float3 sh = posL.xyz / posL.w;          // NDC
        float2 uv = sh.xy * 0.5f + 0.5f;        // [0,1]
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
                    sum += (depth - g_ShadowBias <= d) ? 1.0f : 0.0f;
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
        float q = (kSteps > 1) ? floor(theta * (kSteps - 1) + 0.5f) / (kSteps - 1) : theta;
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