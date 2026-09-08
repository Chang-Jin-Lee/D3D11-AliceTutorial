#include "SceneTransparency.h"
#include <cmath>
#include <iostream>
#include <vector>
using namespace Transparency39;
int main()
{
    int failures = 0;
    auto check = [&](bool value, const char* name) {
        std::cout << (value ? "PASS " : "FAIL ") << name << '\n';
        failures += !value;
    };
    MaterialSettings settings;
    check(ResolveMaterial({ AlphaMode::Opaque }, settings).mode == AlphaMode::Opaque,
          "authored OPAQUE stays opaque");
    auto mask = ResolveMaterial({ AlphaMode::Mask, 0.23f }, settings);
    check(mask.mode == AlphaMode::Mask && mask.cutoff == 0.23f, "authored MASK keeps cutoff");
    check(ResolveMaterial({ AlphaMode::Blend }, settings).mode == AlphaMode::Blend,
          "authored BLEND stays transparent");
    MaterialInput lace{ AlphaMode::Mask, 0.31f, 4, LaceMaterialName };
    check(!ResolveMaterial(lace, settings).laceOverride, "lace override defaults off");
    settings.laceExperiment = true;
    settings.alphaMultiplier = 0.4f;
    auto result = ResolveMaterial(lace, settings);
    check(result.mode == AlphaMode::Blend && result.laceOverride && result.opacity == 0.4f,
          "validated lace becomes blend and receives multiplier");
    check(IsLaceMaterial(4, LaceMaterialName) && !IsLaceMaterial(3, LaceMaterialName) &&
          !IsLaceMaterial(4, "N00_002_01_Tops_01_CLOTH_02"), "lace guard validates both index and full name");
    lace.index = 3;
    result = ResolveMaterial(lace, settings);
    check(result.mode == AlphaMode::Mask && !result.laceOverride && result.opacity == 1,
          "wrong index cannot override lace");
    lace.index = 4;
    lace.name = "other";
    result = ResolveMaterial(lace, settings);
    check(result.mode == AlphaMode::Mask && !result.laceOverride && result.opacity == 1,
          "wrong name cannot override lace");
    check(ResolveMaterial({ AlphaMode::Blend, 0.5f, 8, "eye" }, settings).opacity == 1,
          "eye alpha is unaffected by experiment multiplier");
    check(ResolveMaterial({ AlphaMode::Blend, 0.5f, 0, "plane", true }, settings).opacity == 0.4f,
          "diagnostic planes receive multiplier");
    settings.alphaTest = true;
    settings.experimentCutoff = 0.67f;
    result = ResolveMaterial({ AlphaMode::Blend, 0.1f }, settings);
    check(result.mode == AlphaMode::Mask && result.cutoff == 0.67f,
          "AlphaTest converts original BLEND with experiment cutoff");
    result = ResolveMaterial({ AlphaMode::Mask, 0.23f }, settings);
    check(result.mode == AlphaMode::Mask && result.cutoff == 0.23f,
          "AlphaTest preserves original MASK cutoff");
    result = ResolveMaterial({ AlphaMode::Mask, 0.31f, 4, LaceMaterialName }, settings);
    check(result.mode == AlphaMode::Mask && result.cutoff == 0.67f && result.opacity == 0.4f,
          "AlphaTest cuts experimental lace using experiment settings");
    std::vector<TransparentDraw> draws{ {0,2}, {1,4}, {2,4}, {3,1} };
    OrderTransparentDraws(draws, true, false);
    check(draws[0].id == 1 && draws[1].id == 2 && draws[2].id == 0 && draws[3].id == 3,
          "global back-to-front ordering preserves ties");
    OrderTransparentDraws(draws, true, true);
    check(draws[0].id == 3 && draws[1].id == 0 && draws[2].id == 2 && draws[3].id == 1,
          "reverse deliberately reverses the sorted result including ties");
    draws = {{0,2}, {1,4}, {2,4}, {3,1}};
    OrderTransparentDraws(draws, false, false);
    check(draws[0].id == 0 && draws[1].id == 1 && draws[2].id == 2 && draws[3].id == 3,
          "OIT retains insertion order without sorting");
    OrderTransparentDraws(draws, false, true);
    check(draws[0].id == 3 && draws[1].id == 2 && draws[2].id == 1 && draws[3].id == 0,
          "OIT reverse changes actual submission order");
    return failures ? 1 : 0;
}
