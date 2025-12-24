// G-Buffer 패스 Pixel Shader
// 월드좌표, 월드Normal, 금속성, 거칠기, BaseColor를 G-Buffer에 출력

#include "35_Shared.fxh"

GBufferOut main(VertexOut pIn)
{
    GBufferOut gOut;
    
    // 텍스처 샘플링
    float4 diffuseTex = g_DiffuseMap.Sample(g_Sam, pIn.tex);

    // 알파 클리핑 (Alpha Test)
    // 텍스처를 사용하는 경우, 알파값이 0.1보다 작으면 픽셀을 버림(Discard)
    if (g_UseDiffuseMap != 0)
    {
        clip(diffuseTex.a - 0.1f);
    }
    
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
    
    // PBR 머티리얼 파라미터
    float3 baseColor = (g_UseDiffuseMap != 0) ? diffuseTex.rgb : float3(1, 1, 1);
    baseColor *= g_PBRBaseColor.rgb;
    
    // Roughness/Metalness는 텍스처에서 추출하거나 상수 버퍼에서 가져옴
    float roughness = saturate(g_PBRRoughness);
    float metalness = saturate(g_PBRMetalness);
    
    // G-Buffer 출력
    // 0: PositionWS (월드 좌표)
    // 1: NormalWS (월드 노말, -1~1을 0~1로 변환)
    // 2: Metalness (금속성)
    // 3: Roughness (거칠기)
    // 4: BaseColor (베이스 컬러, sRGB)
    gOut.PositionWS = float4(pIn.posW, 1.0f);
    gOut.NormalWS = float4(N * 0.5f + 0.5f, 1.0f);
    gOut.Metalness = float4(metalness, 0, 0, 1);
    gOut.Roughness = float4(roughness, 0, 0, 1);
    gOut.BaseColor = float4(baseColor, 1.0f);
    
    return gOut;
}