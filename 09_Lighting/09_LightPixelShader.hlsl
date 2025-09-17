#include "09_shared.fxh"

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
    // ���� ���� ����Ʈ: L�� ������ǥ�� ������ �ݴ��̹Ƿ� -direction ���
    // diffuse specular ���� �ʿ�
    //float3 eyeVec = normalize(g_EyePosW - pIn.posW);

    float3 lightVec = -g_DirLight.direction;
    float diffuseFactor = saturate(dot(lightVec, pIn.normalW));
    
    float3 lit = 0.1 * g_DirLight.color.rgb + diffuseFactor * g_DirLight.color.rgb;
    float3 litColor = baseColor * lit;
    
    return float4(litColor, 1.0f);
}