// ���� ���̴�.

/*
    @brief :
        �� ���̴��� ���� ����� ���� ���� ��ġ�� ���� �븻 ���͸� ���� �������� ��ȯ�մϴ�.
        ���� ���� ���� �����մϴ�.
    @details :
        ConstantBuffer : matrix g_WorldInvTranspose; �߰� -> ���� ����� ����ġ ���
        struct VertexIn : float3 normalL : NORMAL; �߰� -> ���� �븻 ����
        struct VertexOut : float3 posW : TEXCOORD0; �߰� -> ���� ���������� ��ġ
*/
#include "15_LightingHelper.hlsli"

Texture2D g_DiffuseMap : register(t0);
TextureCube g_TexCube : register(t1);
SamplerState g_Sam : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    matrix g_World;                     //   ��Ʈ������ float4x4�� ��ü �� �� �ֽ��ϴ�. ���� ������ ��Ʈ������ �⺻������ �� ��Ʈ������ �⺻���� ����ϴ�.
    matrix g_View;                      //   �� ��Ʈ������ ��ǥ�ϱ� ���� ���� �߰� �� �� �ֽ��ϴ�.
    matrix g_Proj;                      //   �� Ʃ�丮���� ���� �⺻ �� ������ ��Ʈ������ ��������� ��Ʈ������ C ++ �ڵ� ���鿡�� �̸� ��ȯ�ؾ��մϴ�.
    matrix g_WorldInvTranspose;         //   ���� ����� ����ġ ����Դϴ�. ���� ��꿡 �ʿ��մϴ�. �̹� ������Ʈ�� �߰��Ǿ����ϴ�.

    Material g_Material;                // ��Ƽ���� ����ü
    DirectionalLight g_DirLight;
    float3 g_EyePosW;
    float  g_Pad;                       // misc
    int    g_ShadingMode;               // 0:Phong,1:Blinn,2:Lambert,3:Unlit,4:TextureOnly
    float3 g_Pad2;                      // align
}

struct VertexIn
{
    float3 posL   : POSITION;
    float3 normalL: NORMAL;    // �̹� ������Ʈ���� Normal�� �߰��˴ϴ�.
    float2 tex    : TEXCOORD;  // 2D UV
    float4 color  : COLOR;
};

struct VertexOut
{
    float4 posH   : SV_POSITION;
    float3 posW   : TEXCOORD0;    // ���忡���� ��ġ
    float3 normalW: TEXCOORD1;    // ������ �븻 ������ ����
    float2 tex    : TEXCOORD2;
    float4 color  : COLOR;
};
