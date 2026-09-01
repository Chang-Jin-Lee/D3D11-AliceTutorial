#pragma once

#include <cstdint>
#include <string_view>

namespace ModelMaterialProcessing
{
    enum class MaterialAlphaMode : std::uint32_t
    {
        Opaque = 0,
        Mask = 1,
        Blend = 2,
    };

    struct MaterialAlphaInfo
    {
        MaterialAlphaMode mode = MaterialAlphaMode::Opaque;
        float cutoff = 0.0f;
    };

    std::uint32_t ParseAlphaMode(std::string_view value);
    float ResolveAlphaCutoff(
        std::uint32_t alphaMode,
        bool hasAuthoredCutoff,
        float authoredCutoff);

    std::uint32_t BuildMaterialPassOrder(
        const std::uint32_t* subsetMaterialIndices,
        std::uint32_t subsetCount,
        const std::uint32_t* materialAlphaModes,
        std::uint32_t materialCount,
        std::uint32_t* orderedSubsetIndices,
        std::uint32_t* firstBlendSubset);
}

namespace ModelTextureProcessing
{
    bool DownsampleAlphaWeightedRgba8(
        const std::uint8_t* source,
        std::uint32_t sourceWidth,
        std::uint32_t sourceHeight,
        std::uint8_t* destination,
        std::uint32_t destinationWidth,
        std::uint32_t destinationHeight);
}
