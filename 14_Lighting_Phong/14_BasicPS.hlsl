#include "14_Shared.fxh"

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
        // Use vertex color to allow RGB axes via LineRenderer
        return pIn.color;
    }

    float4 textureColor = g_DiffuseMap.Sample(g_Sam, pIn.tex);

    float3 normal = normalize(pIn.normalW);
    float3 light = normalize(-g_DirLight.direction);
    float3 eye = normalize(g_EyePosW - pIn.posW);
    // Phong: �ݻ纤�� ��� ����ŧ��
    float3 reflectDir = reflect(-light, normal);

    float NdotL = dot(normal, light);
    float NdotV = dot(normal, eye);
    float theta = saturate(NdotL);
    float specGate = saturate(sign(theta)) * saturate(sign(NdotV));
    float specularScalar = pow(max(dot(reflectDir, eye), 0.0f), g_Material.specular.w) * specGate;

    // ambient/diffuse/specular
    float4 ambient  = g_Material.ambient * g_DirLight.ambient;
    float4 diffuse  = theta * g_DirLight.diffuse;
    float4 specular = specularScalar * g_Material.specular * g_DirLight.specular;

    // kd = texture * material.diffuse
    float4 kd = textureColor * g_Material.diffuse;

    float4 litColor = kd * (ambient + diffuse) + specular;

    // ȯ�� �ݻ�: ť��� ���� �� ��Ƽ���� reflect�� ���� �ռ�
    {
        float3 incident = -eye;
        float3 reflectionVector = reflect(incident, normal);
        float4 reflectionColor = g_TexCube.Sample(g_Sam, reflectionVector);
        litColor += g_Material.reflect * reflectionColor;
    }
    litColor.a = kd.a * g_Material.diffuse.a;

    return litColor;
}