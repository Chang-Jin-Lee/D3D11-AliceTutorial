#include <cstdint>
#include <iostream>
#include <array>
#include <d3d11.h>
#include <wrl/client.h>
#include <assimp/scene.h>

#include "ModelTransparency.h"
#include "FbxMaterial.h"

namespace
{
    bool Expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    }

    bool TestMetallicFallback()
    {
        using Microsoft::WRL::ComPtr;
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        if (!Expect(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP,
            nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context)),
            "WARP device must be available for texture readback"))
            return false;

        aiMaterial material;
        aiMaterial* materials[] = { &material };
        aiScene scene;
        scene.mNumMaterials = 1;
        scene.mMaterials = materials;
        FbxMaterialLoader loader;
        const bool loaded = loader.Load(device.Get(), &scene, L"");
        // The synthetic scene borrows stack storage; Assimp must not delete it.
        scene.mNumMaterials = 0;
        scene.mMaterials = nullptr;
        if (!Expect(loaded,
            "material without textures must load with fallback textures"))
            return false;

        const auto& srvs = loader.GetMetallicSRVs();
        if (!Expect(srvs.size() == 1 && srvs[0], "metallic fallback SRV must exist"))
            return false;
        ComPtr<ID3D11Resource> resource;
        srvs[0]->GetResource(&resource);
        ComPtr<ID3D11Texture2D> texture;
        if (!Expect(SUCCEEDED(resource.As(&texture)), "fallback must be a 2D texture"))
            return false;
        D3D11_TEXTURE2D_DESC description{};
        texture->GetDesc(&description);
        if (!Expect(description.Format == DXGI_FORMAT_R8G8B8A8_UNORM,
            "fallback readback requires RGBA8 texels"))
            return false;
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        description.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> staging;
        if (!Expect(SUCCEEDED(device->CreateTexture2D(&description, nullptr, &staging)),
            "fallback staging texture must be created"))
            return false;
        context->CopyResource(staging.Get(), texture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (!Expect(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)),
            "fallback texture must be readable"))
            return false;
        const auto* pixel = static_cast<const std::uint8_t*>(mapped.pData);
        const std::array<std::uint8_t, 4> actual = { pixel[0], pixel[1], pixel[2], pixel[3] };
        context->Unmap(staging.Get(), 0);
        return Expect(actual == std::array<std::uint8_t, 4>{ 0, 0, 0, 255 },
            "missing metallic map must produce opaque black, not red with zero alpha");
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

    // NPOT images must include the final column and row in lower mip coverage.
    const std::uint8_t edge[12] = {
        0, 0, 0, 0, 0, 0, 0, 0, 240, 220, 200, 255,
    };
    for (const auto& size : { std::array<std::uint32_t, 2>{ 3, 1 }, { 1, 3 } })
    {
        passed &= Expect(ModelTextureProcessing::DownsampleAlphaWeightedRgba8(
            edge, size[0], size[1], destination, 1, 1), "odd mip dimension must downsample");
        passed &= Expect(destination[0] == 240 && destination[1] == 220
            && destination[2] == 200 && destination[3] == 85,
            "last texel of a 3-texel strip must retain its color and one-third coverage");
    }
    const std::uint8_t center[20] = {
        0, 0, 0, 0, 0, 0, 0, 0, 240, 220, 200, 255, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    std::uint8_t twoPixels[8] = {};
    passed &= Expect(ModelTextureProcessing::DownsampleAlphaWeightedRgba8(
        center, 5, 1, twoPixels, 2, 1), "5-to-2 mip must downsample");
    passed &= Expect(twoPixels[0] == 240 && twoPixels[3] == 51
        && twoPixels[4] == 240 && twoPixels[7] == 51,
        "center texel must contribute equally to both 2.5-texel footprints");

    passed &= TestMetallicFallback();

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
