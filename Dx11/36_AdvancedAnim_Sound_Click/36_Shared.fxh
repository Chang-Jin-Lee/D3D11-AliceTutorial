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
#include "36_LightingHelper.hlsli"
#include "36_PBRHelper.hlsli"

Texture2D  g_DiffuseMap             : register(t0);
TextureCube g_TexCube               : register(t1);
// Normal, Specular maps for normal mapping pipeline
Texture2D  g_NormalMap              : register(t2);
Texture2D  g_SpecularMap            : register(t3);
// Shadow map (depth)
Texture2D  g_ShadowMap              : register(t4);
// IBL (Image-Based Lighting) 텍스처들
//  - g_IBL_Diffuse  : Diffuse IBL (Irradiance map, N 방향 샘플)
//  - g_IBL_Specular : Specular IBL (Prefiltered env map, R 방향 + roughness)
//  - g_IBL_BRDF_LUT : BRDF LUT (RG = A,B, NdotV/Roughness → 평균 F,G 계수)
TextureCube g_IBL_Diffuse           : register(t5);
TextureCube g_IBL_Specular          : register(t6);
Texture2D   g_IBL_BRDF_LUT          : register(t7);
// Tone Mapping용
Texture2D   g_SceneHDR              : register(t8);

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
    // 드로우콜 단위 툰 셰이딩 스위치. 쇼케이스의 네 캐릭터만 1이고 바닥과
    // 스카이박스는 0이라, 씬 쪽 셰이딩은 예전 그대로 남는다.
    // (C++ 쪽은 ModelEntry::useToonShading - useInstancePbrMaterial과 같은 방식)
    // 기존 float3 패딩을 쪼갠 것이라 상수 버퍼 크기/정렬은 변하지 않는다.
    int    g_ToonEnabled;               // 0/1
    int    g_MaterialAlphaMode;          // 0: OPAQUE, 1: MASK, 2: BLEND
    float  g_MaterialAlphaCutoff;        // authored cutoff for MASK
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

// -----------------------------------------------------------------------------
// 그림자 계산 함수
//
// 이 함수가 돌려주는 값은 "가시성"이 아니라 "그림자를 적용한 뒤의 밝기 배율"이다.
// 예전에는 순수 가시성 0~1을 그대로 돌려줬고, 호출부가 그걸 litColor 전체에
// 곱했다. 앰비언트까지 같이 0이 되어, 완전히 가려진 픽셀은 진짜 (0,0,0)이 됐다 -
// 바닥의 캐스트 그림자가 새까만 실루엣으로 보이던 원인이 이것이다.
// kShadowIntensity로 상한을 두면 가려진 곳도 (1 - kShadowIntensity)만큼은 남아
// 형태가 읽힌다.
//
// PCF도 3x3에서 5x5로 넓혔다. 3x3은 단계가 9개뿐이라 경계가 계단처럼 끊겼다.
// -----------------------------------------------------------------------------
static const float kShadowIntensity = 0.78f; // 0: 그림자 없음, 1: 예전처럼 완전 검정

float CalcShadowFactor(float4 posShadowH)
{
    // 그림자가 꺼져 있으면 그림자 없음(1.0) 반환
    if (g_ShadowEnabled == 0)
        return 1.0f;

    // 동차 좌표계 나눗셈 (NDC 변환)
    float3 sh = posShadowH.xyz / posShadowH.w;

    // NDC [-1, 1] -> UV [0, 1] 변환 (Y축 반전)
    float2 uv = sh.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    float currentDepth = sh.z;

    // 쉐도우 맵 범위를 벗어났다면 그림자 없음 처리
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
    {
        return 1.0f;
    }

    // PCF (Percentage Closer Filtering)
    float texelSize = 1.0f / max(g_ShadowMapSize, 1.0f);
    float r = max(g_ShadowPCFRadius, 0.0f) * texelSize;
    float sum = 0.0f;
    int taps = 0;

    [unroll]
        for (int dy = -2; dy <= 2; ++dy)
        {
            [unroll]
            for (int dx = -2; dx <= 2; ++dx)
            {
                float2 uvOff = uv + float2(dx, dy) * r;
                float mapDepth = g_ShadowMap.Sample(g_ShadowSamp, uvOff).r;

                // 현재 깊이(bias 적용)가 맵의 깊이보다 작거나 같으면 빛을 받음(1.0)
                if (currentDepth - g_ShadowBias <= mapDepth)
                {
                    sum += 1.0f;
                }
                // 아니면 그림자(0.0) -> sum에 더하지 않음

                taps++;
            }
        }

    float visibility = sum / max(taps, 1);
    return saturate(lerp(1.0f, visibility, kShadowIntensity));
}