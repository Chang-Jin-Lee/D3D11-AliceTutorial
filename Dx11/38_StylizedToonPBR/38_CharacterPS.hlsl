#include "38_Shared.fxh"

Texture2D baseColorTexture : register(t0);
Texture2D metallicTexture : register(t1);
Texture2D roughnessTexture : register(t2);
Texture2D normalTexture : register(t3);
Texture2D shadowMap : register(t4);
SamplerState linearSampler : register(s0);
SamplerComparisonState shadowSampler : register(s1);

struct CharacterPixelOutput
{
    float4 hdrColor : SV_TARGET0;
    float4 encodedNormalProfile : SV_TARGET1;
};

float DistributionGGX(float nDotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denominator = nDotH * nDotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(3.14159265f * denominator * denominator, 0.0001f);
}

float GeometrySchlickGGX(float nDotV, float roughness)
{
    float k = ((roughness + 1.0f) * (roughness + 1.0f)) * 0.125f;
    return nDotV / max(nDotV * (1.0f - k) + k, 0.0001f);
}

float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float SampleCharacterShadow(float4 shadowPosition)
{
    float3 projected = shadowPosition.xyz / max(shadowPosition.w, 0.0001f);
    float2 uv = projected.xy * float2(0.5f, -0.5f) + 0.5f;
    if (projected.z <= 0.0f || projected.z >= 1.0f || any(uv < 0.0f) || any(uv > 1.0f))
        return 1.0f;

    float visibility = 0.0f;
    float2 texel = shadowParameters.xx;
    float radius = max(shadowParameters.y, 1.0f);
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
            visibility += shadowMap.SampleCmpLevelZero(shadowSampler, uv + float2(x, y) * texel * radius, projected.z - shadowParameters.z);
    }
    return visibility / 9.0f;
}

void PSShadow(ShadowVertexOutput input)
{
    float4 sampledBase = baseColorTexture.Sample(linearSampler, input.uv) * input.vertexColor;
    // A depth-only shadow map can only record binary coverage, so this pass keeps the low-alpha
    // cutoff that turns a transparent material into a silhouette.
    float alphaCutoff = materialParameters.w;
    clip(sampledBase.a - alphaCutoff);
}

CharacterPixelOutput PSMain(CharacterVertexOutput input)
{
    CharacterPixelOutput output;
    float4 sampledBase = baseColorTexture.Sample(linearSampler, input.uv) * input.vertexColor;
    // The colour pass must preserve partial coverage: only glTF MASK materials clip here, and they
    // clip at their authored cutoff. OPAQUE and BLEND materials arrive with a zero cutoff so the
    // alpha blend state composites their coverage instead of punching it out.
    float coverageCutoff = alphaParameters.x;
    clip(sampledBase.a - coverageCutoff);

    float3 geometricNormal = normalize(input.worldNormal);
    float3 sampledNormal = normalTexture.Sample(linearSampler, input.uv).xyz * 2.0f - 1.0f;
    float3x3 tbn = float3x3(normalize(input.worldTangent), normalize(input.worldBinormal), geometricNormal);
    float normalWeight = textureParameters.w;
    float3 normal = normalize(lerp(geometricNormal, mul(sampledNormal, tbn), normalWeight));
    float3 viewDirection = normalize(cameraPosition.xyz - input.worldPosition);
    float3 lightVector = normalize(lightDirection.xyz);
    float3 halfVector = normalize(viewDirection + lightVector);

    float materialProfile = materialParameters.x;
    float metallicSample = metallicTexture.Sample(linearSampler, input.uv).r;
    float roughnessSample = roughnessTexture.Sample(linearSampler, input.uv).r;
    float metallic = saturate(lerp(materialParameters.z, metallicSample, textureParameters.y));
    float roughness = clamp(lerp(materialParameters.y, roughnessSample, textureParameters.z), 0.08f, 1.0f);
    float nDotL = saturate(dot(normal, lightVector));
    float nDotV = saturate(dot(normal, viewDirection));
    float nDotH = saturate(dot(normal, halfVector));
    float vDotH = saturate(dot(viewDirection, halfVector));
    float visibility = SampleCharacterShadow(input.shadowPosition);

    float3 f0 = lerp(0.04f.xxx, sampledBase.rgb, metallic);
    float3 fresnel = FresnelSchlick(vDotH, f0);
    float distribution = DistributionGGX(nDotH, roughness);
    float geometry = GeometrySchlickGGX(nDotV, roughness) * GeometrySchlickGGX(nDotL, roughness);
    float3 specular = distribution * geometry * fresnel / max(4.0f * nDotV * nDotL, 0.001f);
    float3 diffuse = sampledBase.rgb * (1.0f - metallic) / 3.14159265f;

    float bandSoftness = diffuseBandThresholds.z;
    float lowerBand = smoothstep(diffuseBandThresholds.x - bandSoftness, diffuseBandThresholds.x + bandSoftness, nDotL * visibility);
    float upperBand = smoothstep(diffuseBandThresholds.y - bandSoftness, diffuseBandThresholds.y + bandSoftness, nDotL * visibility);
    // Floor 0.10 rather than 0.22: the old floor kept the darkest band at 64 percent of the
    // lit band, which ACES then compressed into a 1.3:1 display ratio and read as flat and washed
    // out. The three weights still sum to 1.0 so a fully lit surface is unchanged.
    float toonDiffuse = 0.14f + lowerBand * 0.375f + upperBand * 0.485f;
    float3 shadowTintColor = coolShadowTint.rgb;
    float3 keyTintColor = warmKeyTint.rgb;
    float3 bandTint = lerp(shadowTintColor, keyTintColor, toonDiffuse);

    float hairBand = smoothstep(0.82f, 0.96f, dot(normalize(input.worldTangent + normal * 0.35f), halfVector));
    hairBand *= toonParameters.y * (1.0f - saturate(abs(materialProfile - MaterialProfileHair)));
    float rimTerm = pow(saturate(1.0f - nDotV), 3.0f) * saturate(nDotL + 0.18f) * toonParameters.z;
    float profileSpecularScale = materialProfile < 0.5f ? 0.55f : (materialProfile < 1.5f ? 1.35f : 0.35f);

    float3 pbrColor = (diffuse + specular * profileSpecularScale) * keyTintColor * nDotL * visibility + sampledBase.rgb * 0.08f;
    float3 toonColor = sampledBase.rgb * bandTint * (0.22f + toonDiffuse * 0.98f)
                     + specular * profileSpecularScale * (0.3f + upperBand)
                     + keyTintColor * (hairBand * 0.65f + rimTerm * 0.34f);
    float useToon = step(0.5f, toonParameters.w);
    output.hdrColor = float4(lerp(pbrColor, toonColor, useToon), sampledBase.a);
    output.encodedNormalProfile = float4(geometricNormal * 0.5f + 0.5f, (materialProfile + 0.5f) / 3.0f);
    return output;
}
