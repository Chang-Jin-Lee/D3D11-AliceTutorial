#include "38_Shared.fxh"

// cbBones is the FbxModel palette shared by the character and shadow entries.

float4x4 BuildSkinMatrix(CharacterVertexInput input)
{
    return input.boneWeights.x * bonePalette[input.boneIndices.x]
         + input.boneWeights.y * bonePalette[input.boneIndices.y]
         + input.boneWeights.z * bonePalette[input.boneIndices.z]
         + input.boneWeights.w * bonePalette[input.boneIndices.w];
}

CharacterVertexOutput VSMain(CharacterVertexInput input)
{
    CharacterVertexOutput output;
    float4x4 skin = BuildSkinMatrix(input);
    float4 skinnedPosition = mul(float4(input.position, 1.0f), skin);
    float3x3 skin3 = (float3x3)skin;
    float4 positionWorld = mul(skinnedPosition, world);

    output.position = mul(positionWorld, viewProjection);
    output.worldPosition = positionWorld.xyz;
    output.worldNormal = normalize(mul(mul(input.normal, skin3), (float3x3)world));
    output.worldTangent = normalize(mul(mul(input.tangent, skin3), (float3x3)world));
    output.worldBinormal = normalize(mul(mul(input.binormal, skin3), (float3x3)world));
    output.uv = input.uv;
    output.vertexColor = input.color;
    output.shadowPosition = mul(positionWorld, lightViewProjection);
    return output;
}

ShadowVertexOutput VSShadow(CharacterVertexInput input)
{
    ShadowVertexOutput output;
    float4x4 skin = BuildSkinMatrix(input);
    float4 positionWorld = mul(mul(float4(input.position, 1.0f), skin), world);
    output.position = mul(positionWorld, lightViewProjection);
    output.uv = input.uv;
    output.vertexColor = input.color;
    return output;
}
