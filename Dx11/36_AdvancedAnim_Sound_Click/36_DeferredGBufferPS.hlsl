// G-Buffer 패스 Pixel Shader
// 월드좌표, 월드Normal, 금속성, 거칠기, BaseColor를 G-Buffer에 출력

#include "36_Shared.fxh"

GBufferOut main(VertexOut pIn)
{
    GBufferOut gOut;
    
    // 텍스처 샘플링
    float4 textureColor;
    if (g_UseDiffuseMap != 0)
    {
        textureColor = g_DiffuseMap.Sample(g_Sam, pIn.tex);
    }
    else
    {
        textureColor = float4(1,1,1,1);
    }

    // 알파 컷아웃
    float alphaBase;
    if (g_UseDiffuseMap != 0)
    {
        alphaBase = textureColor.a;
    }
    else
    {
        alphaBase = 1.0f;
    }
    float alphaTex = alphaBase * g_Material.diffuse.a;
    clip(alphaTex - 0.1f);
    
    // 월드 노말 계산 (노말맵 적용)
    float3 N = normalize(pIn.normalW);
    if (g_EnableNormalMap != 0)
    {
        float3 T = normalize(pIn.tangentW);
        float3 B = normalize(pIn.bitanW);
        float handed = dot(cross(T, B), N);
        if (handed < 0.0f) B = -B;
        float3x3 TBN = float3x3(T, B, N);
        float3 N_ts = g_NormalMap.Sample(g_Sam, pIn.tex).xyz * 2.0f - 1.0f;
        N_ts.y = -N_ts.y; // 그린 채널 반전 보정
        N_ts = normalize(N_ts);
        N = normalize(mul(N_ts, TBN));
    }
    
    // PBR 머티리얼 파라미터 (Forward와 동일한 로직)
    float roughnessTex = 1.0f;
    float metalnessTex = 0.0f;
    float3 baseColor;
    
    if (g_UseDiffuseMap != 0)
    {
        // 텍스처에서 Roughness/Metalness 추출
        roughnessTex = textureColor.g;
        metalnessTex = textureColor.b;
    }
    
    // BaseColor 계산
    if (g_UseTextureColor != 0 && g_UseDiffuseMap != 0)
    {
        // 텍스처 색상 사용 (sRGB 그대로 저장, Light PS에서 선형 변환)
        baseColor = textureColor.rgb * g_PBRBaseColor.rgb;
    }
    else
    {
        // 텍스처 색 사용 안 함: g_PBRBaseColor는 선형 공간이므로 sRGB로 인코딩해서 저장
        // Light PS에서 감마 디코딩하면 원래 선형 값이 됨 
        baseColor = pow(max(g_PBRBaseColor.rgb, 0.0f), 1.0f / 2.2f);
    }
    
    // Roughness/Metalness 계산
    float metalness = saturate(g_PBRMetalness);
    float roughness = saturate(g_PBRRoughness);
    
    if (g_UseTextureColor != 0 && g_UseDiffuseMap != 0)
    {
        metalness = saturate(metalness * metalnessTex);
        roughness = saturate(roughness * roughnessTex);
    }
    
    // G-Buffer 출력
    // 0: PositionWS (월드 좌표)
    // 1: NormalWS (월드 노말, -1~1을 0~1로 변환)
    // 2: Metalness (금속성)
    // 3: Roughness (거칠기)
    // 4: BaseColor (베이스 컬러, sRGB)
    gOut.PositionWS = float4(pIn.posW, 1.0f);
    gOut.NormalWS = float4(N, 1.0f);
    gOut.Metalness = float4(metalness, 0, 0, 1);
    gOut.Roughness = float4(roughness, 0, 0, 1);
    gOut.BaseColor = float4(baseColor, 1.0f);
    
    return gOut;
}