#include <shared.fxh>
// ¡§¡° ºŒ¿Ã¥ı.
PS_INPUT main(float4 pos : POSITION, float4 color : COLOR)
{
    PS_INPUT output;
    output.pos = pos;
    output.color = color;
    return output;
}