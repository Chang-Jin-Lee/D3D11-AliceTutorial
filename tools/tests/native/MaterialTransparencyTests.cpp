#include <cstdint>
#include <iostream>

#include "ModelTransparency.h"

namespace
{
    bool Expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    }
}

int main()
{
    bool passed = true;

    // Regression: transparent black texels surrounding a bright eye detail must not
    // darken the RGB stored in a lower mip. Only coverage (alpha) should decrease.
    const std::uint8_t source[16] = {
        240, 220, 200, 255,
          0,   0,   0,   0,
          0,   0,   0,   0,
          0,   0,   0,   0,
    };
    std::uint8_t destination[4] = {};
    passed &= Expect(
        ModelTextureProcessing::DownsampleAlphaWeightedRgba8(source, 2, 2, destination, 1, 1),
        "2x2 RGBA mip downsample must succeed");
    passed &= Expect(destination[0] == 240, "transparent texels must not darken red");
    passed &= Expect(destination[1] == 220, "transparent texels must not darken green");
    passed &= Expect(destination[2] == 200, "transparent texels must not darken blue");
    passed &= Expect(destination[3] == 64, "lower mip alpha must preserve average coverage");

    // Regression: glTF BLEND was previously treated as an opaque 0.1 cutout.
    const std::uint32_t opaque = ModelMaterialProcessing::ParseAlphaMode("OPAQUE");
    const std::uint32_t mask = ModelMaterialProcessing::ParseAlphaMode("mask");
    const std::uint32_t blend = ModelMaterialProcessing::ParseAlphaMode("Blend");
    passed &= Expect(opaque == 0, "OPAQUE alpha mode must map to the opaque pass");
    passed &= Expect(mask == 1, "MASK alpha mode must map to the alpha-test pass");
    passed &= Expect(blend == 2, "BLEND alpha mode must map to the transparent pass");
    passed &= Expect(
        ModelMaterialProcessing::ResolveAlphaCutoff(mask, false, 0.0f) == 0.5f,
        "MASK without alphaCutoff must use the glTF 0.5 default");
    passed &= Expect(
        ModelMaterialProcessing::ResolveAlphaCutoff(mask, true, 0.37f) == 0.37f,
        "MASK must retain its authored alphaCutoff");
    passed &= Expect(
        ModelMaterialProcessing::ResolveAlphaCutoff(blend, true, 0.75f) == 0.0f,
        "BLEND must preserve partial coverage instead of clipping it");

    const std::uint32_t subsetMaterials[] = { 0, 1, 2, 3 };
    const std::uint32_t materialModes[] = { opaque, blend, mask, blend };
    std::uint32_t orderedSubsets[4] = {};
    std::uint32_t firstBlendSubset = 0;
    const std::uint32_t orderedCount = ModelMaterialProcessing::BuildMaterialPassOrder(
        subsetMaterials,
        4,
        materialModes,
        4,
        orderedSubsets,
        &firstBlendSubset);
    passed &= Expect(orderedCount == 4, "every subset must appear in the material pass order");
    passed &= Expect(firstBlendSubset == 2, "opaque and MASK subsets must complete before BLEND");
    passed &= Expect(
        orderedSubsets[0] == 0 && orderedSubsets[1] == 2,
        "opaque and MASK subset declaration order must stay stable");
    passed &= Expect(
        orderedSubsets[2] == 1 && orderedSubsets[3] == 3,
        "BLEND subset declaration order must stay stable");

    if (!passed)
        return 1;

    std::cout << "Material transparency regression tests passed.\n";
    return 0;
}
