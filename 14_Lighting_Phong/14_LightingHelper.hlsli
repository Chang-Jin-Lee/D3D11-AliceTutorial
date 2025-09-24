struct DirectionalLight
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float3 direction;
    float  pad;
};

struct Material
{
    float4 ambient;
    float4 diffuse;
    float4 specular; // w = »ç¾ç

    float4 reflect;
};