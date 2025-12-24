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
#include "23_LightingHelper.hlsli"

Texture2D g_DiffuseMap : register(t0);
TextureCube g_TexCube : register(t1);
// Normal, Specular maps for normal mapping pipeline
Texture2D g_NormalMap : register(t2);
Texture2D g_SpecularMap : register(t3);
SamplerState g_Sam : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    matrix g_World;                     //   매트릭스는 float4x4로 대체 될 수 있습니다. 행이 없으면 매트릭스는 기본적으로 열 매트릭스로 기본값을 얻습니다.
    matrix g_View;                      //   행 매트릭스를 대표하기 전에 행을 추가 할 수 있습니다.
    matrix g_Proj;                      //   이 튜토리얼은 향후 기본 열 마스터 매트릭스를 사용하지만 매트릭스는 C ++ 코드 측면에서 미리 변환해야합니다.
    matrix g_WorldInvTranspose;         //   월드 행렬의 역전치 행렬입니다. 조명 계산에 필요합니다. 이번 프로젝트에 추가되었습니다.

    Material g_Material;                // 머티리얼 구조체
    DirectionalLight g_DirLight;
    float3 g_EyePosW;
    float  g_Pad;
    int    g_ShadingMode;               // 0:Phong,1:Blinn,2:Lambert,3:Unlit,4:TextureOnly
    float3 g_Pad2;
    int    g_EnableNormalMap;           // 0: off, 1: on
    float3 g_Pad3;
    int    g_UseSpecularMap;            // 0: off, 1: on
    float3 g_Pad4;
    int g_UseRigid; // 0: Skinned, 1: Rigid
    float3 g_Pad5;
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
