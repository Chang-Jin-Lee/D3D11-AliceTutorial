#include <shared.fxh>
// ¡§¡° ºŒ¿Ã¥ı.
VertexOut main(VertexIn vIn)
{
    VertexOut vOut;
    
    // world-space values for lighting
    float4 posW = mul(float4(vIn.posL, 1.0f), g_World);
    vOut.posW = posW.xyz;
    vOut.normalW = mul(float4(vIn.normalL, 0.0f), g_WorldInvTranspose).xyz;

    // clip-space position
    float4 posH = mul(posW, g_View);
    vOut.posH = mul(posH, g_Proj);

    vOut.color = vIn.color;
    
    return vOut;
}