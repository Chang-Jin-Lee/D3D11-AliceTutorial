#include "16_Shared.fxh"
// 정점 셰이더 (GPU 스키닝 지원)
VertexOut main(VertexIn vIn)
{
    VertexOut vOut;

    float3 pos = vIn.posL;
    float3 nrm = vIn.normalL;
    if (g_NumBones > 0)
    {
        float4 p = float4(pos,1);
        float3 n = nrm;
        // 가중치/인덱스 정리
        float4 w = vIn.boneW;
        // 인덱스 범위 밖은 무효화
        if (vIn.boneIdx.x >= (uint)g_NumBones) w.x = 0;
        if (vIn.boneIdx.y >= (uint)g_NumBones) w.y = 0;
        if (vIn.boneIdx.z >= (uint)g_NumBones) w.z = 0;
        if (vIn.boneIdx.w >= (uint)g_NumBones) w.w = 0;
        float wsum = w.x + w.y + w.z + w.w;
        if (wsum > 1e-5)
        {
            w /= wsum;
            float4x4 M0 = g_Bones[vIn.boneIdx.x];
            float4x4 M1 = g_Bones[vIn.boneIdx.y];
            float4x4 M2 = g_Bones[vIn.boneIdx.z];
            float4x4 M3 = g_Bones[vIn.boneIdx.w];
            float4 sp = mul(p, M0) * w.x + mul(p, M1) * w.y + mul(p, M2) * w.z + mul(p, M3) * w.w;
            float3 sn = mul(n, (float3x3)M0) * w.x + mul(n, (float3x3)M1) * w.y + mul(n, (float3x3)M2) * w.z + mul(n, (float3x3)M3) * w.w;
            pos = sp.xyz;
            nrm = normalize(sn);
        }
    }

    float4 posW = mul(float4(pos, 1.0f), g_World);
    vOut.posH = mul(posW, g_View);
    vOut.posH = mul(vOut.posH, g_Proj);
    vOut.posW = posW.xyz;

    vOut.normalW = normalize(mul(nrm, (float3x3) g_WorldInvTranspose));
    vOut.tex = vIn.tex;
    vOut.color = vIn.color;

    return vOut;
}