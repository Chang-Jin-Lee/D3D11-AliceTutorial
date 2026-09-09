#include "../../../Dx11/39_Transparency_OIT/39_Transparency.fxh"

cbuffer SurfaceConstants : register(b0)
{
    float4 surfaceColor;
    float clipDepth;
    float normalizedViewDepth;
    float alphaCutoff;
    float padding;
};

float4 VSMain(uint vertexId : SV_VertexID) : SV_Position
{
    float2 positions[3] = { float2(-1.0, -1.0), float2(-1.0, 3.0), float2(3.0, -1.0) };
    return float4(positions[vertexId], clipDepth, 1.0);
}

float4 PSOpaque() : SV_Target
{
    return float4(surfaceColor.rgb, 1.0);
}

float4 PSAlphaTest() : SV_Target
{
    clip(surfaceColor.a - alphaCutoff);
    return float4(surfaceColor.rgb, 1.0);
}

float4 PSSorted() : SV_Target
{
    return surfaceColor;
}

OitOutput PSOit()
{
    return MakeOitOutput(surfaceColor.rgb, surfaceColor.a, normalizedViewDepth);
}
