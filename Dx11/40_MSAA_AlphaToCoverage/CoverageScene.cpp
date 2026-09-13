#include "CoverageScene.h"

#include <algorithm>

namespace Coverage40
{
bool IsLaceMaterial(const MaterialInput& input)
{
    return input.mode == AlphaMode::Mask && input.index == 4 &&
           input.name == LaceMaterialName;
}

EffectiveMaterial ResolveMaterial(const MaterialInput& input, const Settings& settings)
{
    const bool target = input.diagnosticPattern || IsLaceMaterial(input);
    if (target)
    {
        return {
            settings.mode == Mode::AlphaToCoverage4x
                ? SurfaceRule::AlphaToCoverage
                : SurfaceRule::TestCoverage,
            input.diagnosticPattern ? settings.diagnosticCutoff : settings.laceCutoff,
            std::clamp(settings.alphaMultiplier, 0.0f, 1.0f),
            true
        };
    }

    SurfaceRule rule = SurfaceRule::Opaque;
    if (input.mode == AlphaMode::Mask)
    {
        rule = SurfaceRule::Mask;
    }
    else if (input.mode == AlphaMode::Blend)
    {
        rule = SurfaceRule::Blend;
    }

    return { rule, input.cutoff, 1.0f, false };
}

void OrderTransparentDraws(std::vector<TransparentDraw>& draws)
{
    std::stable_sort(draws.begin(), draws.end(),
        [](const TransparentDraw& left, const TransparentDraw& right) {
            return left.viewDepth > right.viewDepth;
        });
}

std::uint32_t SampleCount(Mode mode)
{
    return mode == Mode::AlphaTest1x ? 1U : 4U;
}

bool Supports4x(bool colorQueryOk, std::uint32_t colorLevels,
                bool depthQueryOk, std::uint32_t depthLevels, bool requiredFormats)
{
    return colorQueryOk && colorLevels > 0 && depthQueryOk && depthLevels > 0 &&
           requiredFormats;
}

std::uint64_t TargetBytes(std::uint32_t width, std::uint32_t height,
                          std::uint32_t samples)
{
    if (width == 0 || height == 0 || width > 16384 || height > 16384 ||
        (samples != 1 && samples != 4))
    {
        return 0;
    }

    return std::uint64_t(width) * height * (samples == 1 ? 12ULL : 56ULL);
}
}
