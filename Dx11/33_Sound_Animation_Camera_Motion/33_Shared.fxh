// 정점 셰이더.

/*
    @brief :
        이 셰이더는 조명 계산을 위해 정점 위치와 정점 노말 벡터를 월드 공간으로 변환합니다.
        또한 정점 색상도 전달합니다.
    @details :
        ConstantBuffer : matrix g_WorldInvTranspose; 추가 -> 월드 행렬의 역전치 행렬
        struct VertexIn : float3 normalL : NORMAL; 추가 -> 정점 노말 벡터
        struct VertexOut : float3 posW : TEXCOORD0; 추가 -> 월드 공간에서의 위치
*/
#include "33_LightingHelper.hlsli"

Texture2D  g_DiffuseMap : register(t0);
TextureCube g_TexCube   : register(t1);
// Normal, Specular maps for normal mapping pipeline
Texture2D  g_NormalMap  : register(t2);
Texture2D  g_SpecularMap: register(t3);
// Shadow map (depth)
Texture2D  g_ShadowMap  : register(t4);
// IBL (Image-Based Lighting) 텍스처들
//  - g_IBL_Diffuse  : Diffuse IBL (Irradiance map, N 방향 샘플)
//  - g_IBL_Specular : Specular IBL (Prefiltered env map, R 방향 + roughness)
//  - g_IBL_BRDF_LUT : BRDF LUT (RG = A,B, NdotV/Roughness → 평균 F,G 계수)
TextureCube g_IBL_Diffuse  : register(t5);
TextureCube g_IBL_Specular : register(t6);
Texture2D   g_IBL_BRDF_LUT : register(t7);
SamplerState g_Sam : register(s0);
SamplerState g_ShadowSamp : register(s1);

cbuffer ConstantBuffer : register(b0)
{
    matrix g_World;                     //   매트릭스는 float4x4로 대체 될 수 있습니다. 행이 없으면 매트릭스는 기본적으로 열 매트릭스로 기본값을 얻습니다.
    matrix g_View;                      //   행 매트릭스를 대표하기 전에 행을 추가 할 수 있습니다.
    matrix g_Proj;                      //   이 튜토리얼은 향후 기본 열 마스터 매트릭스를 사용하지만 매트릭스는 C ++ 코드 측면에서 미리 변환해야합니다.
    matrix g_WorldInvTranspose;         //   월드 행렬의 역전치 행렬입니다. 조명 계산에 필요합니다. 이번 프로젝트에 추가되었습니다.

    Material g_Material;                // 머티리얼 구조체
    DirectionalLight g_DirLight;
    float3 g_EyePosW;
    int    g_ShadingMode;               // 0:Phong,1:Blinn,2:Lambert,3:Unlit,4:TextureOnly,5:Toon,6:PBR
    // Compact flags + pad into one 4-slot vector
    int    g_EnableNormalMap;           // 0/1
    int    g_UseSpecularMap;            // 0/1
    int    g_UseDiffuseMap;             // 0/1 (텍스처 없으면 머티리얼만)
    float  g_Pad;                       // 디버그/단축 경로용
    // PBR / 화면 감마 보정 값 (기본 2.2)
    float  g_Gamma;                     // 감마 값
    int    g_UseTextureColor;           // 0: 텍스처 색 무시(고정 회색), 1: 텍스처 색 + BaseColor
    float2 g_PBRPad;

    // PBR 전용 머티리얼 (baseColor, metalness, roughness, AO)
    float4 g_PBRBaseColor;
    float  g_PBRMetalness;
    float  g_PBRRoughness;
    float  g_PBRAmbientOcclusion;
    float  g_PBRPad2;
// 이번 프로젝트 코드
//////////////////////////////////////////////////////////////////////////
    // Toon/Outline params (packed tightly)
    float  g_OutlineWidth;              // 림 밴드 폭 (PS Rim 용)
    float  g_OutlinePow;                // 림 감마/파워 (PS Rim 용)
    float  g_OutlineThickness;          // 외곽선 두께 (VS 팽창 용)
    float  g_OutlineStrength;           // 아웃라인 강도(0~4 권장)
    float4 g_OutlineColor;              // 아웃라인 색상 (a 안씀)

    // Shadow params
    matrix g_LightViewProj;             // 라이트 뷰-프로젝션
    float  g_ShadowBias;                // 깊이 바이어스(미터 단위)
    float  g_ShadowMapSize;             // 섀도우맵 한 변의 크기(px)
    float  g_ShadowPCFRadius;           // PCF 반경(텍셀 단위)
    int    g_ShadowEnabled;             // 0/1

    // Debug/Lines: AABB에 루트 본(또는 지정 본) 변환을 적용할 때 사용
    int    g_BoundsBoneIndex;           // <0: 사용 안함, >=0: g_BonePalette[idx] 적용
    float3 g_BoundsPad;
}

// GPU 스키닝을 위한 레지스터 (b1)
#define MAX_BONES 1023
cbuffer BonesBuffer : register(b1)
{
	row_major matrix g_BonePalette[MAX_BONES];
	uint g_BoneCount;
	float3 g_BonePad;
}

struct VertexIn
{
    float3 posL     : POSITION;
    float3 normalL  : NORMAL;    // TBN에서의 Normal. 텍스쳐의 노말 방향입니다
    float3 tangentL : TANGENT;   // TBN에서의 tangent.  (object/local space)
    float3 bitanL   : BINORMAL;  // TBN에서의 bitangent.  (object/local space)
    float2 tex      : TEXCOORD;  // 2D UV 좌표
    float4 color    : COLOR;
};

struct VertexOut
{
    float4 posH      : SV_POSITION;
    float3 posW      : TEXCOORD0;    // 월드에서의 위치
    float3 normalW   : TEXCOORD1;    // 월드의 노말 벡터의 방향
    float2 tex       : TEXCOORD2;
    float4 color     : COLOR;
    // TBN 노말 매핑을 위한 월드 공간
    float3 tangentW  : TEXCOORD3;
    float3 bitanW    : TEXCOORD4;
    // 라이트 공간 투영 좌표 (W 포함) - PS 섀도우 샘플에 사용
    float4 posShadowH: TEXCOORD5;
};

// 스키닝 입력 정점 구조체
struct VertexInSkinned
{
	float3 posL     : POSITION;
	float3 normalL  : NORMAL;
	float3 tangentL : TANGENT;
	float3 bitanL   : BINORMAL;
	float2 tex      : TEXCOORD;
	float4 color    : COLOR;
	uint4  boneIdx  : BLENDINDICES;
	float4 boneW    : BLENDWEIGHT;
};
