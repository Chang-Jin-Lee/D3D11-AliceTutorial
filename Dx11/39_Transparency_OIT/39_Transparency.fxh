struct OitOutput
{
    float4 accumulation : SV_Target0;
    float revealage : SV_Target1;
};

OitOutput MakeOitOutput(float3 linearColor, float alpha, float normalizedViewDepth)
{
    alpha = saturate(alpha);
    if (alpha == 0.0)
        discard;
    linearColor = clamp(linearColor, 0.0, 8.0);
    normalizedViewDepth = saturate(normalizedViewDepth);
    const float weight = clamp(
        0.01 + 8.0 * pow(1.0 - normalizedViewDepth, 3.0), 0.01, 8.0);
    OitOutput result;
    result.accumulation = float4(linearColor * alpha * weight, alpha * weight);
    result.revealage = alpha;
    return result;
}
