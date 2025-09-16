#include "02_shared.fxh"
// 정점 셰이더.
VertexOut main(VertexIn vIn)
{
    VertexOut vOut;
    vOut.posH = mul(float4(vIn.posL, 1.0f), g_World);   // MUL은 행렬 곱하기입니다. 연산자*는 객체를 요구합니다.
    vOut.posH = mul(vOut.posH, g_View);                 // 같은 수의 행과 열이있는 두 개의 행렬, 결과는 다음과 같습니다.
    vOut.posH = mul(vOut.posH, g_Proj);                 // cij = aij *bij
    vOut.color = vIn.color;                             // 여기서 Alpha 채널의 값은 기본적으로 1.0입니다.

    return vOut;
}