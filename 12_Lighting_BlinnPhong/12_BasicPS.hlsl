#include "12_Shared.fxh"

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

    float3 lightVec = -g_DirLight.direction;
    float3 eyeVec = normalize(g_EyePosW - pIn.posW);
    float4 ambient, diffuse, specular;
    ambient = diffuse = specular = float4(0.0f, 0.0f, 0.0f, 0.0f); // �ʱ�ȭ 

    // ambient
    ambient = g_Material.ambient * g_DirLight.ambient; // ȯ�汤

    // diffuse
    float3 H = normalize(lightVec + eyeVec); // �ݻ� ����, �ϴ��� �߰� ���ͷ� ��ü
    float specularScalar = diffuse = pow(saturate(dot(pIn.normalW, H)), g_Material.specular.w); // ���� ���
    diffuse = specularScalar * g_DirLight.diffuse;

    // specular
    specular = specularScalar * g_Material.specular * g_DirLight.specular;
    
    float4 litColor = pIn.color * (ambient + diffuse) + specular;
    litColor.a = g_Material.diffuse.a * pIn.color.a;
    
    return litColor;
}