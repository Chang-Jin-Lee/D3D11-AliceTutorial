#pragma once
#include <cstddef>
#include <string_view>
#include <vector>

namespace Transparency39
{
    enum class AlphaMode { Opaque, Mask, Blend };
    inline constexpr std::string_view LaceMaterialName = "N00_002_01_Tops_01_CLOTH_02 (Instance)";
    struct MaterialInput
    {
        AlphaMode mode{ AlphaMode::Opaque };
        float cutoff{ 0.5f };
        std::size_t index{};
        std::string_view name;
        bool diagnosticPlane{};
    };
    struct MaterialSettings
    {
        bool alphaTest{};
        bool laceExperiment{};
        float alphaMultiplier{ 1.0f };
        float experimentCutoff{ 0.5f };
    };
    struct EffectiveMaterial
    {
        AlphaMode mode{ AlphaMode::Opaque };
        float cutoff{};
        float opacity{ 1.0f };
        bool laceOverride{};
    };
    struct TransparentDraw { std::size_t id; float viewDepth; };
    bool IsLaceMaterial(std::size_t index, std::string_view name);
    EffectiveMaterial ResolveMaterial(const MaterialInput& input, const MaterialSettings& settings);
    void OrderTransparentDraws(std::vector<TransparentDraw>& draws, bool sortedBlend, bool reverse);
}
