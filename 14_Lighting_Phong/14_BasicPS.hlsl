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
    // ���� ���� �ݿ��մϴ�.
    // �ؽ�ó ���İ� ������ �ȼ��� �����ؼ� �����ϰ� ����ϴ�
    // ���̸� �̱���ؼ� �� ���/��ī�̹ڽ��� ���̵��� �մϴ�
    float alphaTex = textureColor.a * g_Material.diffuse.a;
    clip(alphaTex - 0.1f);

    float3 normal = normalize(pIn.normalW);
    float3 light = normalize(-g_DirLight.direction);
    float3 eye = normalize(g_EyePosW - pIn.posW);
    // Phong: �ݻ纤�� ��� ����ŧ��
    float3 reflectDir = reflect(-light, normal);

    /*
        @brief :
            ���� shininess(?1)���� specular lobe�� �о��� ��/���� ���̶���Ʈ ������ ���� �� ����
        @details :
            - ����: N��L > 0, N��V > 0 ���������� ���������� ����
            - ���߿� ��� BSDF������ GGX(Tr/Trowbridge-Reitz) NDF + Smith G + Schlick F�� ���� ���� ������ ������ ������ �ϱ� ������ ���������� �ذ� ����
    */
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

    /*
        @details :
            ȯ�� �ݻ�(�����Ͻ�): reflect.a�� �����Ͻ��� ���� mip ���̾�� ��ȯ�ؼ� ���� ���÷� ����ϴ�
            - prefiltered env + LOD (���⼱ SampleBias�� ����ȭ)�� �����մϴ�
    */
    float roughness = saturate(g_Material.reflect.a);
    float3 rdir = reflect(-eye, normal);
    const float kMaxMip = 8.0f;                 // �ʿ� �� ť��� mip ���� ���� �����ؾ��մϴ�
    float mipBias = roughness * roughness * kMaxMip; // perceptual mapping
    float4 reflectionColor = g_TexCube.SampleBias(g_Sam, rdir, mipBias);
    /*
        @details :
            �ݻ� ������: ���� ���� ��(N��L==0)������ �ݻ絵 0 �� �ſ�ó�� �ڿ��� ���̴� ���� ����
    */
    float reflectGate = theta;                  // �ʿ�� pow(theta,2) ������ �ε巯�� �ѿ���
    litColor += (g_Material.reflect * reflectGate) * reflectionColor;
    // ������ ���󿡼��� ���� ���� �ؽ�ó ���� ������ ���� ���ϴ�
    litColor.a = alphaTex;

    return litColor;
}