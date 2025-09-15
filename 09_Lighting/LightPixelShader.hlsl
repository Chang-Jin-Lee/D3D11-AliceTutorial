#include <shared.fxh>

// �ȼ� ���̴�(���̴�/���̴�)
float4 main(VertexOut pIn) : SV_TARGET
{
    // �����: g_Pad == 1 -> �����, g_Pad == 2 -> ��� ��Ŀ
    if (abs(g_Pad - 1.0f) < 1e-3)
    {
        return float4(1.0f, 0.0f, 1.0f, 1.0f);
    }
    if (abs(g_Pad - 2.0f) < 1e-3)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    float3 baseColor = float3(0.9, 0.6, 0.2);
    float3 n = normalize(pIn.normalW);
    float ndotl = saturate(dot(g_DirLight.direction.xyz, n));
    float3 ambient = 0.2.xxx * g_DirLight.color.rgb;
    float3 lit = ambient + g_DirLight.color.rgb * ndotl;
    float3 rgb = baseColor * lit;
    return float4(rgb, 1.0f);
}