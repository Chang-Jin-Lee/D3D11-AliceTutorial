#include "34_Shared.fxh"

float3 Rec709ToRec2020(float3 color)
{
    static const float3x3 conversion =
    {
        0.627402, 0.329292, 0.043306,
        0.069095, 0.919544, 0.011360,
        0.016394, 0.088028, 0.895578
    };
    return mul(conversion, color);
}

// PQ는 10,000nit 기준이므로, 모니터 최대 밝기(g_MaxHDRNits)를 반영하여 스케일링
// color_for_PQ = linear01 * (displayMaxNits / 10000.0)
float3 LinearToST2084(float3 color)
{
    // g_MaxHDRNits를 반영하여 HDR 스케일링 (10000 nits 기준으로 정규화)
    const float st2084max = 10000.0;
    float hdrScalar = g_MaxHDRNits / st2084max;
    float3 scaledColor = color * hdrScalar;
    
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;
    float3 cp = pow(abs(scaledColor), m1);
    return pow((c1 + c2 * cp) / (1 + c3 * cp), m2);
}

float4 main(PS_INPUT_QUAD input) : SV_Target
{
     // 1. 선형 HDR 값 로드 (Nits 값으로 간주)
    float3 C_linear709 = g_SceneHDR.Sample(g_SamplerLinear, input.uv).rgb;  
    float3 C_exposure = C_linear709 * pow(2.0f, g_Exposure);    
    float3 C_tonemapped = ACESFilm(C_exposure);
  
    // Rec709 → Rec2020 색공간 변환 (LinearToST2084 내부에서 g_MaxHDRNits 처리)
    float3 C_Rec2020 = Rec709ToRec2020(C_tonemapped); 
    float3 C_ST2084 = LinearToST2084(C_Rec2020);
    
    // 최종 PQ 인코딩된 값 [0.0, 1.0]을 R10G10B10A2_UNORM 백버퍼에 출력
    return float4(C_ST2084, 1.0);
}