#include "ModelTransparency.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace ModelMaterialProcessing
{
    std::uint32_t ParseAlphaMode(std::string_view value)
    {
        std::string normalized(value);
        std::transform(
            normalized.begin(),
            normalized.end(),
            normalized.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::toupper(character));
            });

        if (normalized == "MASK")
            return static_cast<std::uint32_t>(MaterialAlphaMode::Mask);
        if (normalized == "BLEND")
            return static_cast<std::uint32_t>(MaterialAlphaMode::Blend);
        return static_cast<std::uint32_t>(MaterialAlphaMode::Opaque);
    }

    float ResolveAlphaCutoff(
        std::uint32_t alphaMode,
        bool hasAuthoredCutoff,
        float authoredCutoff)
    {
        if (alphaMode != static_cast<std::uint32_t>(MaterialAlphaMode::Mask))
            return 0.0f;

        return hasAuthoredCutoff ? std::clamp(authoredCutoff, 0.0f, 1.0f) : 0.5f;
    }

    std::uint32_t BuildMaterialPassOrder(
        const std::uint32_t* subsetMaterialIndices,
        std::uint32_t subsetCount,
        const std::uint32_t* materialAlphaModes,
        std::uint32_t materialCount,
        std::uint32_t* orderedSubsetIndices,
        std::uint32_t* firstBlendSubset)
    {
        if (firstBlendSubset)
            *firstBlendSubset = 0;
        if ((!subsetMaterialIndices && subsetCount != 0)
            || (!orderedSubsetIndices && subsetCount != 0)
            || !firstBlendSubset)
        {
            return 0;
        }

        const auto isBlendSubset = [&](std::uint32_t subsetIndex)
        {
            const std::uint32_t materialIndex = subsetMaterialIndices[subsetIndex];
            const std::uint32_t alphaMode = materialIndex < materialCount && materialAlphaModes
                ? materialAlphaModes[materialIndex]
                : static_cast<std::uint32_t>(MaterialAlphaMode::Opaque);
            return alphaMode == static_cast<std::uint32_t>(MaterialAlphaMode::Blend);
        };

        std::uint32_t outputCount = 0;
        for (std::uint32_t subsetIndex = 0; subsetIndex < subsetCount; ++subsetIndex)
        {
            if (!isBlendSubset(subsetIndex))
                orderedSubsetIndices[outputCount++] = subsetIndex;
        }

        *firstBlendSubset = outputCount;
        for (std::uint32_t subsetIndex = 0; subsetIndex < subsetCount; ++subsetIndex)
        {
            if (isBlendSubset(subsetIndex))
                orderedSubsetIndices[outputCount++] = subsetIndex;
        }
        return outputCount;
    }
}

namespace ModelTextureProcessing
{
    bool DownsampleAlphaWeightedRgba8(
        const std::uint8_t* source,
        std::uint32_t sourceWidth,
        std::uint32_t sourceHeight,
        std::uint8_t* destination,
        std::uint32_t destinationWidth,
        std::uint32_t destinationHeight)
    {
        if (!source || !destination || sourceWidth == 0 || sourceHeight == 0)
            return false;

        const std::uint32_t expectedWidth = sourceWidth > 1 ? sourceWidth >> 1 : 1;
        const std::uint32_t expectedHeight = sourceHeight > 1 ? sourceHeight >> 1 : 1;
        if (destinationWidth != expectedWidth || destinationHeight != expectedHeight)
            return false;

        // Odd dimensions do not have exact 2x2 footprints. Integrate the area
        // covered by each output texel so the last row/column is not discarded.
        // Coordinates are scaled by the destination size to keep fractional
        // overlaps exact (e.g. the middle texel in a 5-to-2 reduction is shared).
        if ((sourceWidth > 1 && sourceWidth % 2 != 0)
            || (sourceHeight > 1 && sourceHeight % 2 != 0))
        {
            const std::uint64_t area = static_cast<std::uint64_t>(sourceWidth) * sourceHeight;
            for (std::uint32_t y = 0; y < destinationHeight; ++y)
            {
                const std::uint64_t top = static_cast<std::uint64_t>(y) * sourceHeight;
                const std::uint64_t bottom = top + sourceHeight;
                const auto firstY = static_cast<std::uint32_t>(top / destinationHeight);
                const auto endY = static_cast<std::uint32_t>(
                    (bottom + destinationHeight - 1) / destinationHeight);
                for (std::uint32_t x = 0; x < destinationWidth; ++x)
                {
                    const std::uint64_t left = static_cast<std::uint64_t>(x) * sourceWidth;
                    const std::uint64_t right = left + sourceWidth;
                    const auto firstX = static_cast<std::uint32_t>(left / destinationWidth);
                    const auto endX = static_cast<std::uint32_t>(
                        (right + destinationWidth - 1) / destinationWidth);
                    std::uint64_t alphaSum = 0;
                    std::uint64_t colorSums[3] = {};
                    for (std::uint32_t sy = firstY; sy < endY; ++sy)
                    {
                        const std::uint64_t texelTop = static_cast<std::uint64_t>(sy) * destinationHeight;
                        const std::uint64_t height = std::min(bottom, texelTop + destinationHeight)
                            - std::max(top, texelTop);
                        for (std::uint32_t sx = firstX; sx < endX; ++sx)
                        {
                            const std::uint64_t texelLeft = static_cast<std::uint64_t>(sx) * destinationWidth;
                            const std::uint64_t width = std::min(right, texelLeft + destinationWidth)
                                - std::max(left, texelLeft);
                            const auto* sample = source + (static_cast<size_t>(sy) * sourceWidth + sx) * 4;
                            const std::uint64_t weightedAlpha = width * height * sample[3];
                            alphaSum += weightedAlpha;
                            for (int channel = 0; channel < 3; ++channel)
                                colorSums[channel] += weightedAlpha * sample[channel];
                        }
                    }
                    auto* output = destination + (static_cast<size_t>(y) * destinationWidth + x) * 4;
                    for (int channel = 0; channel < 3; ++channel)
                    {
                        output[channel] = alphaSum == 0 ? 0 : static_cast<std::uint8_t>(
                            (colorSums[channel] + alphaSum / 2) / alphaSum);
                    }
                    output[3] = static_cast<std::uint8_t>((alphaSum + area / 2) / area);
                }
            }
            return true;
        }

        for (std::uint32_t y = 0; y < destinationHeight; ++y)
        {
            const std::uint32_t y0 = sourceHeight > 1 ? y * 2 : 0;
            const std::uint32_t y1 = sourceHeight > 1 ? std::min(y0 + 1, sourceHeight - 1) : 0;
            for (std::uint32_t x = 0; x < destinationWidth; ++x)
            {
                const std::uint32_t x0 = sourceWidth > 1 ? x * 2 : 0;
                const std::uint32_t x1 = sourceWidth > 1 ? std::min(x0 + 1, sourceWidth - 1) : 0;
                const std::uint8_t* samples[4] = {
                    source + (static_cast<size_t>(y0) * sourceWidth + x0) * 4,
                    source + (static_cast<size_t>(y0) * sourceWidth + x1) * 4,
                    source + (static_cast<size_t>(y1) * sourceWidth + x0) * 4,
                    source + (static_cast<size_t>(y1) * sourceWidth + x1) * 4,
                };
                std::uint8_t* output =
                    destination + (static_cast<size_t>(y) * destinationWidth + x) * 4;

                std::uint32_t alphaSum = 0;
                for (const std::uint8_t* sample : samples)
                    alphaSum += sample[3];

                for (int channel = 0; channel < 3; ++channel)
                {
                    if (alphaSum == 0)
                    {
                        output[channel] = 0;
                        continue;
                    }

                    std::uint32_t weightedColor = 0;
                    for (const std::uint8_t* sample : samples)
                        weightedColor += static_cast<std::uint32_t>(sample[channel]) * sample[3];
                    output[channel] = static_cast<std::uint8_t>(
                        (weightedColor + alphaSum / 2) / alphaSum);
                }
                output[3] = static_cast<std::uint8_t>((alphaSum + 2) / 4);
            }
        }
        return true;
    }
}
