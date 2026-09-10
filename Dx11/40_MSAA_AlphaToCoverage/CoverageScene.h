#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace Coverage40
{
enum class Mode { AlphaTest1x, AlphaTest4x, AlphaToCoverage4x };
enum class AlphaMode { Opaque, Mask, Blend };
enum class SurfaceRule { Opaque, Mask, Blend, TestCoverage, AlphaToCoverage };

inline constexpr std::string_view LaceMaterialName =
    "N00_002_01_Tops_01_CLOTH_02 (Instance)";

struct MaterialInput
{
    AlphaMode mode{ AlphaMode::Opaque };
    float cutoff{ 0.5f };
    std::size_t index{};
    std::string_view name;
    bool diagnosticPattern{};
};

struct Settings
{
    Mode mode{ Mode::AlphaTest1x };
    float alphaMultiplier{ 1.0f };
    float laceCutoff{ 0.5f };
    float diagnosticCutoff{ 0.5f };
};

struct EffectiveMaterial
{
    SurfaceRule rule{ SurfaceRule::Opaque };
    float cutoff{ 0.5f };
    float opacity{ 1.0f };
    bool comparisonTarget{};
};

struct TransparentDraw
{
    std::size_t id;
    float viewDepth;
};

bool IsLaceMaterial(const MaterialInput& input);
EffectiveMaterial ResolveMaterial(const MaterialInput& input, const Settings& settings);
void OrderTransparentDraws(std::vector<TransparentDraw>& draws);
std::uint32_t SampleCount(Mode mode);
bool Supports4x(bool colorQueryOk, std::uint32_t colorLevels,
                bool depthQueryOk, std::uint32_t depthLevels, bool requiredFormats);
std::uint64_t TargetBytes(std::uint32_t width, std::uint32_t height,
                          std::uint32_t samples);
}
