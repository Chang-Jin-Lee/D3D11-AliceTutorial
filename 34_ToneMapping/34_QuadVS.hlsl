#include "34_Shared.fxh"

PS_INPUT_QUAD main(VS_INPUT_QUAD input)
{
    PS_INPUT_QUAD output = (PS_INPUT_QUAD) 0;  
    output.position = float4(input.position.xy, 0.0f, 1.0f);
    output.uv = input.uv;    
    return output;
}