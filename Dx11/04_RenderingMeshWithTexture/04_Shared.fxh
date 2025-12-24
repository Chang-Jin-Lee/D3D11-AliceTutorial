// 정점 셰이더.

cbuffer ConstantBuffer : register(b0)
{
    matrix g_World; //   매트릭스는 float4x4로 대체 될 수 있습니다. 행이 없으면 매트릭스는 기본적으로 열 매트릭스로 기본값을 얻습니다.
    matrix g_View;  //   행 매트릭스를 대표하기 전에 행을 추가 할 수 있습니다.
    matrix g_Proj;  //   이 튜토리얼은 향후 기본 열 마스터 매트릭스를 사용하지만 매트릭스는 C ++ 코드 측면에서 미리 변환해야합니다.
}


struct VertexIn
{
    float3 posL : POSITION;
    float4 color : COLOR;
};

struct VertexOut
{
    float4 posH : SV_POSITION;
    float4 color : COLOR;
};
