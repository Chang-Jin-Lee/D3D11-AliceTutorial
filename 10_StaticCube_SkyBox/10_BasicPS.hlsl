#include "10_shared.fxh"

// �ȼ� ���̴�(���̴�/���̴�)
float4 main(VertexOut pIn) : SV_Target
{
    // �����: g_Pad == 1 -> �����, g_Pad == 2 -> ��� ��Ŀ, g_Pad == 3 -> ������ ����
    if (abs(g_Pad - 1.0f) < 1e-3)
    {
        return float4(1.0f, 0.0f, 1.0f, 1.0f);
    }
    if (abs(g_Pad - 2.0f) < 1e-3)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    if (abs(g_Pad - 3.0f) < 1e-3)
    {
        return float4(1.0f, 0.0f, 0.0f, 1.0f);
    }

    float3 baseColor = float3(1.0, 0.7, 1.0);
    pIn.normalW = normalize(pIn.normalW);
    float saturatedDir = saturate(dot(g_DirLight.direction.xyz, pIn.normalW));
    
    float3 lit = g_DirLight.color.rgb + g_DirLight.color.rgb * saturatedDir;
    float3 litColor = baseColor * lit;
    
    return float4(litColor, 1.0f);
}