#include "CoverageScene.h"

#include <cstdint>
#include <iostream>
#include <vector>

using namespace Coverage40;

namespace
{
class TestContext
{
public:
    void Check(bool condition, const char* name)
    {
        ++checks_;
        if (condition)
        {
            std::cout << "PASS " << name << '\n';
            return;
        }

        std::cerr << "FAIL " << name << '\n';
        ++failures_;
    }

    int Finish() const
    {
        if (failures_ != 0)
        {
            std::cerr << failures_ << " of " << checks_ << " checks failed.\n";
            return 1;
        }

        std::cout << checks_ << " checks passed.\n";
        return 0;
    }

private:
    int checks_{};
    int failures_{};
};
}

int main()
{
    TestContext test;

    Settings settings;
    settings.mode = Mode::AlphaToCoverage4x;
    settings.alphaMultiplier = 0.25f;
    MaterialInput lace{ AlphaMode::Mask, 0.5f, 4, LaceMaterialName, false };
    test.Check(IsLaceMaterial(lace), "lace requires matching MASK material, index, and name");

    auto result = ResolveMaterial(lace, settings);
    test.Check(result.rule == SurfaceRule::AlphaToCoverage && result.cutoff == 0.5f &&
                   result.opacity == 0.25f && result.comparisonTarget,
               "A2C mode applies A2C and multiplier to validated lace");

    const auto eye = ResolveMaterial({ AlphaMode::Blend, 0.5f, 5, "eye", false }, settings);
    test.Check(eye.rule == SurfaceRule::Blend && eye.cutoff == 0.5f &&
                   eye.opacity == 1.0f && !eye.comparisonTarget,
               "authored BLEND remains unchanged");

    test.Check(!IsLaceMaterial({ AlphaMode::Mask, 0.5f, 3, LaceMaterialName, false }),
               "wrong material index cannot identify lace");
    test.Check(!IsLaceMaterial({ AlphaMode::Mask, 0.5f, 4,
                                 "N00_002_01_Tops_01_CLOTH_02", false }),
               "lace name suffix is required");
    test.Check(!IsLaceMaterial({ AlphaMode::Blend, 0.5f, 4, LaceMaterialName, false }),
               "authored mode must be MASK to identify lace");

    const auto opaque = ResolveMaterial({ AlphaMode::Opaque, 0.7f, 0, "body", false }, settings);
    test.Check(opaque.rule == SurfaceRule::Opaque && opaque.cutoff == 0.7f &&
                   opaque.opacity == 1.0f && !opaque.comparisonTarget,
               "ordinary OPAQUE material preserves authored values");

    const auto mask = ResolveMaterial({ AlphaMode::Mask, 0.3f, 0, "hair", false }, settings);
    test.Check(mask.rule == SurfaceRule::Mask && mask.cutoff == 0.3f &&
                   mask.opacity == 1.0f && !mask.comparisonTarget,
               "ordinary MASK material preserves authored cutoff");

    settings.diagnosticCutoff = 0.2f;
    settings.laceCutoff = 0.8f;
    const auto diagnostic = ResolveMaterial(
        { AlphaMode::Opaque, 0.9f, 0, "diagnostic", true }, settings);
    test.Check(diagnostic.rule == SurfaceRule::AlphaToCoverage && diagnostic.cutoff == 0.2f &&
                   diagnostic.opacity == 0.25f && diagnostic.comparisonTarget,
               "diagnostic target uses its independent cutoff");
    result = ResolveMaterial(lace, settings);
    test.Check(result.cutoff == 0.8f, "validated lace uses lace cutoff");

    settings.alphaMultiplier = -0.25f;
    test.Check(ResolveMaterial(lace, settings).opacity == 0.0f,
               "comparison opacity clamps below zero");
    settings.alphaMultiplier = 1.25f;
    test.Check(ResolveMaterial(lace, settings).opacity == 1.0f,
               "comparison opacity clamps above one");

    settings.alphaMultiplier = 0.5f;
    settings.mode = Mode::AlphaTest1x;
    test.Check(ResolveMaterial(lace, settings).rule == SurfaceRule::TestCoverage,
               "Alpha Test 1x uses test coverage rule");
    settings.mode = Mode::AlphaTest4x;
    test.Check(ResolveMaterial(lace, settings).rule == SurfaceRule::TestCoverage,
               "Alpha Test 4x uses test coverage rule");
    settings.mode = Mode::AlphaToCoverage4x;
    test.Check(ResolveMaterial(lace, settings).rule == SurfaceRule::AlphaToCoverage,
               "Alpha-to-Coverage 4x uses A2C rule");

    std::vector<TransparentDraw> draws{ { 0, 2.0f }, { 1, 4.0f }, { 2, 4.0f } };
    OrderTransparentDraws(draws);
    test.Check(draws[0].id == 1 && draws[1].id == 2 && draws[2].id == 0,
               "transparent draws sort back-to-front and preserve equal-depth order");

    test.Check(SampleCount(Mode::AlphaTest1x) == 1,
               "Alpha Test 1x selects one sample");
    test.Check(SampleCount(Mode::AlphaTest4x) == 4,
               "Alpha Test 4x selects four samples");
    test.Check(SampleCount(Mode::AlphaToCoverage4x) == 4,
               "Alpha-to-Coverage 4x selects four samples");

    test.Check(Supports4x(true, 1, true, 1, true),
               "4x support requires all successful positive checks");
    test.Check(!Supports4x(false, 1, true, 1, true),
               "failed color quality query rejects 4x");
    test.Check(!Supports4x(true, 0, true, 1, true),
               "zero color quality levels reject 4x");
    test.Check(!Supports4x(true, 1, false, 1, true),
               "failed depth quality query rejects 4x");
    test.Check(!Supports4x(true, 1, true, 0, true),
               "zero depth quality levels reject 4x");
    test.Check(!Supports4x(true, 1, true, 1, false),
               "missing required format support rejects 4x");

    test.Check(TargetBytes(1600, 900, 1) == 17280000ULL,
               "one-sample target bytes include color, depth, and resolved output");
    test.Check(TargetBytes(1600, 900, 4) == 80640000ULL,
               "four-sample target bytes include multisampled and resolved targets");
    test.Check(TargetBytes(0, 900, 1) == 0 && TargetBytes(1600, 0, 4) == 0,
               "zero-sized targets are rejected");
    test.Check(TargetBytes(1600, 900, 2) == 0 && TargetBytes(1600, 900, 8) == 0,
               "unsupported sample counts are rejected");
    test.Check(TargetBytes(16385, 1, 1) == 0 && TargetBytes(1, 16385, 4) == 0,
               "dimensions above the D3D11 2D limit are rejected");
    test.Check(TargetBytes(16384, 16384, 4) == 15032385536ULL,
               "maximum legal D3D11 2D target dimensions are accepted");

    return test.Finish();
}
