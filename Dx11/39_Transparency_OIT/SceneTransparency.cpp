#include "SceneTransparency.h"
#include <algorithm>

namespace Transparency39
{
    bool IsLaceMaterial(std::size_t index, std::string_view name)
    {
        return index == 4 && name == LaceMaterialName;
    }
    EffectiveMaterial ResolveMaterial(const MaterialInput& input, const MaterialSettings& settings)
    {
        EffectiveMaterial result{ input.mode, input.cutoff, 1.0f, false };
        result.laceOverride = settings.laceExperiment && input.mode == AlphaMode::Mask &&
            IsLaceMaterial(input.index, input.name);
        if (result.laceOverride) result.mode = AlphaMode::Blend;
        if (result.laceOverride || input.diagnosticPlane) result.opacity = settings.alphaMultiplier;
        if (settings.alphaTest && result.mode == AlphaMode::Blend)
        {
            result.mode = AlphaMode::Mask;
            result.cutoff = settings.experimentCutoff;
        }
        return result;
    }
    void OrderTransparentDraws(std::vector<TransparentDraw>& draws, bool sortedBlend, bool reverse)
    {
        if (sortedBlend)
            std::stable_sort(draws.begin(), draws.end(), [](const auto& a, const auto& b) {
                return a.viewDepth > b.viewDepth;
            });
        if (reverse) std::reverse(draws.begin(), draws.end());
    }
}
