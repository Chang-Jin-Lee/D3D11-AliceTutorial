static const float PI = 3.14159265f;
static const float TWO_PI = 6.28318530718f;
static const float INV_PI = 0.31830988618f; // 1 / PI
static const float INV_TWO_PI = 0.15915494309f; // 1 / (2 * PI)

// -----------------------------------------------------------------------------
// PBR용 간단한 헬퍼 함수들. GGX, Schlick를 위한 것
// 각 함수가 의미하는 수식은 아래와 같습니다.
// -----------------------------------------------------------------------------

// NDF: GGX / Trowbridge-Reitz
//  D_ggx(n·h, α) = α² / ( π * ((n·h)²(α² - 1) + 1)² ),  α = roughness²
float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
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