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
#include "10_LightingHelper.hlsli"

cbuffer ConstantBuffer : register(b0)
{
    matrix g_World;                     //   ��Ʈ������ float4x4�� ��ü �� �� �ֽ��ϴ�. ���� ������ ��Ʈ������ �⺻������ �� ��Ʈ������ �⺻���� ����ϴ�.
    matrix g_View;                      //   �� ��Ʈ������ ��ǥ�ϱ� ���� ���� �߰� �� �� �ֽ��ϴ�.
    matrix g_Proj;                      //   �� Ʃ�丮���� ���� �⺻ �� ������ ��Ʈ������ ��������� ��Ʈ������ C ++ �ڵ� ���鿡�� �̸� ��ȯ�ؾ��մϴ�.
    matrix g_WorldInvTranspose;         //   ���� ����� ����ġ ����Դϴ�. ���� ��꿡 �ʿ��մϴ�. �̹� ������Ʈ�� �߰��Ǿ����ϴ�.

    DirectionalLight g_DirLight;
    float3 g_EyePosW;
    float  g_Pad;
}

struct VertexIn
{
    float3 posL : POSITION;
    float3 normalL : NORMAL;    // �̹� ������Ʈ���� Normal�� �߰��˴ϴ�.
    float4 color : COLOR;
};

struct VertexOut
{
    float4 posH : SV_POSITION;
    float3 posW : TEXCOORD0; // ���迡���� ��ġ
    float3 normalW : TEXCOORD1; // ������ �븻 ������ ����
    float4 color : COLOR;
};
