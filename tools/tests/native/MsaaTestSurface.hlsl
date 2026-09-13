#include "../../../Dx11/40_MSAA_AlphaToCoverage/40_AlphaCoverage.fxh"

cbuffer SurfaceConstants : register(b0)
{
    float4 SurfaceColor;
    float ClipDepth;
    uint Rule;
    float AlphaCutoff;
    float SurfacePadding;
};

struct SurfaceOutput
{
    float4 position : SV_POSITION;
};

SurfaceOutput VSMain(uint vertexId : SV_VertexID)
{
    const float2 positions[3] = {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };
    SurfaceOutput output;
    output.position = float4(positions[vertexId], ClipDepth, 1.0f);
    return output;
}

float4 PSMain() : SV_TARGET
{
    return float4(SurfaceColor.rgb,
        ApplyCoverageAlpha(SurfaceColor.a, Rule, AlphaCutoff));
}
