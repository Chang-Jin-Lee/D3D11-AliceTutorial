#ifndef COVERAGE40_ALPHA_COVERAGE_FXH
#define COVERAGE40_ALPHA_COVERAGE_FXH

// SurfaceRule values shared with CoverageScene.h:
// 0 Opaque, 1 Mask, 2 Blend, 3 TestCoverage, 4 AlphaToCoverage.
static const uint SurfaceRuleOpaque = 0;
static const uint SurfaceRuleMask = 1;
static const uint SurfaceRuleBlend = 2;
static const uint SurfaceRuleTestCoverage = 3;
static const uint SurfaceRuleAlphaToCoverage = 4;

float ApplyCoverageAlpha(float alpha, uint rule, float cutoff)
{
    alpha = saturate(alpha);
    if (rule == SurfaceRuleOpaque)
        return 1.0f;
    if (rule == SurfaceRuleMask)
    {
        clip(alpha - cutoff);
        return 1.0f;
    }
    if (alpha <= 0.0f)
        discard;
    if (rule == SurfaceRuleTestCoverage)
    {
        clip(alpha - cutoff);
        return 1.0f;
    }
    return alpha;
}

#endif
