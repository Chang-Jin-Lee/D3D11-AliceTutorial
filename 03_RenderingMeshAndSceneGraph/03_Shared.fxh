// ���� ���̴�.

cbuffer ConstantBuffer : register(b0)
{
    matrix g_World; //   ��Ʈ������ float4x4�� ��ü �� �� �ֽ��ϴ�. ���� ������ ��Ʈ������ �⺻������ �� ��Ʈ������ �⺻���� ����ϴ�.
    matrix g_View;  //   �� ��Ʈ������ ��ǥ�ϱ� ���� ���� �߰� �� �� �ֽ��ϴ�.
    matrix g_Proj;  //   �� Ʃ�丮���� ���� �⺻ �� ������ ��Ʈ������ ��������� ��Ʈ������ C ++ �ڵ� ���鿡�� �̸� ��ȯ�ؾ��մϴ�.
}


struct VertexIn
{
    float3 posL : POSITION;
    float4 color : COLOR;
};

struct VertexOut
{
    float4 posH : SV_POSITION;
    float4 color : COLOR;
};
