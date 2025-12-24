// 정점 셰이더.
#include "35_LightingHelper.hlsli"
#include "35_PBRHelper.hlsli"

Texture2D g_PositionWS : register(t0);  // 월드 좌표
Texture2D g_NormalWS : register(t1);  // 월드 노말
Texture2D g_Metalness : register(t2);  // 금속성
Texture2D g_Roughness : register(t3);  // 거칠기
Texture2D g_BaseColor : register(t4);  // 베이스 컬러
TextureCube g_IBL_Diffuse : register(t5);
TextureCube g_IBL_Specular : register(t6);
Texture2D   g_IBL_BRDF_LUT : register(t7);
Texture2D g_ShadowMap : register(t8);

SamplerState g_Sam                  : register(s0);
SamplerState g_ShadowSamp           : register(s1);
// Tone Mapping용
SamplerState g_SamplerLinear        : register(s2);

cbuffer ConstantBuffer              : register(b0)
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
    int    g_UseTextureColor;           // 0: 텍스처 색 무시(고정 회색), 1: 텍스처 색 + BaseColor
    float3 g_PBRPad;

    // PBR 전용 머티리얼 (baseColor, metalness, roughness, AO)
    float4 g_PBRBaseColor;
    float  g_PBRMetalness;
    float  g_PBRRoughness;
    float  g_PBRAmbientOcclusion;
    float  g_PBRPad2;

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

cbuffer PostProcessConstantBuffer : register(b2)
{
    float g_Exposure;
    float g_MaxHDRNits;
    float2 g_Padding; // 16바이트 정렬 맞춤
}

// 디렉션 라이트 상수 버퍼
cbuffer DirectionalLightBuffer : register(b3)
{
    float4 g_LightDirection;  // xyz: 방향, w: 강도
    float4 g_LightColor;      // rgb: 색상, w: unused
	float g_intensity;        // 라이트 강도
	float g_pad[3];
};


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

// G-Buffer 패스용 출력 정점 구조체
struct VertexOutDeffered
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD0;    // 월드 공간 위치
    float3 normalW : TEXCOORD1;    // 월드 공간 노말
    float2 tex : TEXCOORD2;    // 텍스처 좌표
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

struct GBufferOut
{
    float4 PositionWS : SV_Target0;  // 월드 좌표 (R16G16B16A16_FLOAT)
    float4 NormalWS : SV_Target1;  // 월드 노말 (R8G8B8A8_UNORM, -1~1을 0~1로 변환)
    float4 Metalness : SV_Target2;  // 금속성 (R8_UNORM)
    float4 Roughness : SV_Target3;  // 거칠기 (R8_UNORM)
    float4 BaseColor : SV_Target4;  // 베이스 컬러 (R8G8B8A8_UNORM_SRGB)
};

// HDR Tone Mapping을 위한 Quad
struct VS_INPUT_BASIC
{
    float4 position : POSITION;
    float3 normal : NORMAL;
};

struct PS_INPUT_BASIC
{
    float4 position : SV_POSITION;
    float3 normal : TEXCOORD0;
};

struct VS_INPUT_QUAD
{
    float3 position : POSITION; // 이미 NDC
    float2 uv : TEXCOORD0;
};

struct PS_INPUT_QUAD
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// 입력: Linear 공간의 HDR RGB 색상값
// 출력: 0.0 ~ 1.0 범위의 압축된 선형 RGB 값 (float3)
float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate(x * (a * x + b) / (x * (c * x + d) + e));
}

// 월드 좌표를 입력받아 라이트 공간으로 변환 후 그림자 판별
float CalcShadowFactorDeferred(float3 posW)
{
    if (g_ShadowEnabled == 0) return 1.0f;

    // 1. 월드 좌표 -> 라이트 클립 공간 좌표 변환
    float4 posShadowH = mul(float4(posW, 1.0f), g_LightViewProj);

    // 2. 동차 나눗셈 (Perspective Divide) -> NDC
    float3 projCoords = posShadowH.xyz / posShadowH.w;

    // 3. NDC [-1, 1] -> Texture UV [0, 1] (Y축 반전 주의: DX는 Top-Down이므로 -Y)
    float2 shadowUV;
    shadowUV.x = projCoords.x * 0.5f + 0.5f;
    shadowUV.y = projCoords.y * -0.5f + 0.5f;

    // 깊이값
    float currentDepth = projCoords.z;

    // 4. 범위 체크
    if (shadowUV.x < 0.0f || shadowUV.x > 1.0f || shadowUV.y < 0.0f || shadowUV.y > 1.0f)
        return 1.0f;

    // 5. PCF (Percentage Closer Filtering)
    float shadow = 0.0f;
    float texelSize = 1.0f / g_ShadowMapSize;

    // 간단한 3x3 PCF
    [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            [unroll]
            for (int x = -1; x <= 1; ++x)
            {
                float2 uvOffset = float2(x, y) * texelSize;
                float pcfDepth = g_ShadowMap.Sample(g_ShadowSamp, shadowUV + uvOffset).r;

                // 섀도우 맵의 깊이가 현재 픽셀 깊이보다 작으면(더 가까우면) 그림자
                if (currentDepth - g_ShadowBias > pcfDepth)
                {
                    shadow += 0.0f; // Shadowed
                }
                else
                {
                    shadow += 1.0f; // Lit
                }
            }
        }

    return shadow / 9.0f;
}