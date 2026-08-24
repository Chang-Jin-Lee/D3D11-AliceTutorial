#ifndef PROJECT38_SHARED_FXH
#define PROJECT38_SHARED_FXH

static const uint MaterialProfileSkin = 0;
static const uint MaterialProfileHair = 1;
static const uint MaterialProfileCloth = 2;

cbuffer cbCharacter : register(b0)
{
    float4x4 world;
    float4x4 viewProjection;
    float4x4 lightViewProjection;
    float4 cameraPosition;
    float4 lightDirection;
    float4 warmKeyTint;
    float4 coolShadowTint;
    float4 diffuseBandThresholds;
    float4 toonParameters;
    float4 materialParameters;
    float4 shadowParameters;
    float4 textureParameters;
    // x: colour-pass coverage cutoff. It is deliberately separate from materialParameters.w, which
    // carries the binary coverage cutoff the depth-only shadow pass needs.
    float4 alphaParameters;
    // x: true-skin warmth weight. Kept separate from material profile because facial overlays use
    // the Skin BRDF profile without being skin-coloured surfaces.
    float4 styleParameters;
};

cbuffer cbBones : register(b1)
{
    row_major float4x4 bonePalette[1023];
};

cbuffer cbPost : register(b0)
{
    float4 inverseResolution;
    float4 outlineParameters;
    float4 depthReconstructionParameters;
    float4 toneMapParameters;
    float4 backgroundColor;
};

struct CharacterVertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float3 binormal : BINORMAL;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    uint4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHT;
};

struct CharacterVertexOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float3 worldTangent : TEXCOORD2;
    float3 worldBinormal : TEXCOORD3;
    float2 uv : TEXCOORD4;
    float4 vertexColor : COLOR0;
    float4 shadowPosition : TEXCOORD5;
};

struct ShadowVertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 vertexColor : COLOR0;
};

#endif
