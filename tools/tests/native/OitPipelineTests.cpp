#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXPackedVector.h>
#include <wrl/client.h>

#include "OitPipeline.h"

namespace
{
    using Microsoft::WRL::ComPtr;
    using Transparency39::DebugView;
    using Transparency39::Mode;
    using Transparency39::OitPipeline;

    constexpr UINT kInitialWidth = 8;
    constexpr UINT kInitialHeight = 8;

    struct Color
    {
        float r;
        float g;
        float b;
        float a;
    };

    struct SurfaceConstants
    {
        Color color;
        float clipDepth;
        float normalizedViewDepth;
        float alphaCutoff;
        float padding;
    };

    bool Expect(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "FAIL: " << message << '\n';
        return condition;
    }

    bool Near(float actual, float expected, float tolerance)
    {
        return std::isfinite(actual) && std::abs(actual - expected) <= tolerance;
    }

    bool ColorNear(const Color& actual, const Color& expected, float tolerance)
    {
        return Near(actual.r, expected.r, tolerance) &&
            Near(actual.g, expected.g, tolerance) &&
            Near(actual.b, expected.b, tolerance);
    }

    float MaxRgbDifference(const Color& left, const Color& right)
    {
        return std::max({ std::abs(left.r - right.r), std::abs(left.g - right.g),
            std::abs(left.b - right.b) });
    }

    float OitWeight(float normalizedDepth)
    {
        const float inverseDepth = 1.0f - std::clamp(normalizedDepth, 0.0f, 1.0f);
        return std::clamp(0.01f + 8.0f * inverseDepth * inverseDepth * inverseDepth, 0.01f, 8.0f);
    }

    Color ExpectedOit(const Color& background, const Color& first, float firstDepth,
        const Color& second, float secondDepth)
    {
        const float firstAlpha = std::clamp(first.a, 0.0f, 1.0f);
        const float secondAlpha = std::clamp(second.a, 0.0f, 1.0f);
        const float firstFactor = firstAlpha * OitWeight(firstDepth);
        const float secondFactor = secondAlpha * OitWeight(secondDepth);
        const float denominator = firstFactor + secondFactor;
        const float revealage = (1.0f - firstAlpha) * (1.0f - secondAlpha);
        const float coverage = 1.0f - revealage;
        return {
            ((first.r * firstFactor + second.r * secondFactor) / denominator) * coverage + background.r * revealage,
            ((first.g * firstFactor + second.g * secondFactor) / denominator) * coverage + background.g * revealage,
            ((first.b * firstFactor + second.b * secondFactor) / denominator) * coverage + background.b * revealage,
            1.0f
        };
    }

    float DisplayChannel(float linear, float exposure)
    {
        const float exposed = std::max(linear * std::max(exposure, 0.0f), 0.0f);
        const float mapped = exposed / (1.0f + exposed);
        return mapped <= 0.0031308f ? 12.92f * mapped :
            1.055f * std::pow(mapped, 1.0f / 2.4f) - 0.055f;
    }

    Color DisplayColor(const Color& linear, float exposure)
    {
        return { DisplayChannel(linear.r, exposure), DisplayChannel(linear.g, exposure),
            DisplayChannel(linear.b, exposure), 1.0f };
    }

    ComPtr<ID3DBlob> CompileShader(const std::filesystem::path& path,
        const char* entryPoint, const char* target)
    {
        ComPtr<ID3DBlob> bytecode;
        ComPtr<ID3DBlob> errors;
        const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
        const HRESULT result = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint, target, flags, 0, &bytecode, &errors);
        if (FAILED(result))
        {
            std::cerr << "FAIL: shader compilation " << entryPoint << " returned HRESULT 0x"
                << std::hex << static_cast<unsigned long>(result) << std::dec << '\n';
            if (errors)
                std::cerr.write(static_cast<const char*>(errors->GetBufferPointer()),
                    static_cast<std::streamsize>(errors->GetBufferSize()));
            return {};
        }
        return bytecode;
    }

    class Fixture
    {
    public:
        bool Initialize(const std::filesystem::path& repositoryRoot)
        {
            UINT flags = 0;
#if defined(_DEBUG)
            flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
            D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0;
            D3D_FEATURE_LEVEL actual{};
            HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                &requested, 1, D3D11_SDK_VERSION, &device_, &actual, &context_);
            if (FAILED(result) && (flags & D3D11_CREATE_DEVICE_DEBUG) != 0)
            {
                flags &= ~D3D11_CREATE_DEVICE_DEBUG;
                result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                    &requested, 1, D3D11_SDK_VERSION, &device_, &actual, &context_);
            }
            if (!Expect(SUCCEEDED(result) && actual == D3D_FEATURE_LEVEL_11_0,
                "WARP feature level 11 device must initialize"))
                return false;

            shaderDirectory_ = repositoryRoot / L"Dx11" / L"39_Transparency_OIT";
            if (!Expect(pipeline_.Initialize(device_.Get(), shaderDirectory_.wstring()),
                "production OIT pipeline must initialize"))
                return false;
            if (!Expect(pipeline_.Resize(device_.Get(), kInitialWidth, kInitialHeight),
                "production OIT pipeline must allocate initial targets"))
                return false;
            width_ = kInitialWidth;
            height_ = kInitialHeight;

            const auto surfacePath = repositoryRoot / L"tools" / L"tests" / L"native" / L"OitTestSurface.hlsl";
            const auto vertexBytecode = CompileShader(surfacePath, "VSMain", "vs_5_0");
            const auto opaqueBytecode = CompileShader(surfacePath, "PSOpaque", "ps_5_0");
            const auto alphaTestBytecode = CompileShader(surfacePath, "PSAlphaTest", "ps_5_0");
            const auto sortedBytecode = CompileShader(surfacePath, "PSSorted", "ps_5_0");
            const auto oitBytecode = CompileShader(surfacePath, "PSOit", "ps_5_0");
            if (!vertexBytecode || !opaqueBytecode || !alphaTestBytecode || !sortedBytecode || !oitBytecode)
                return false;

            if (!Expect(SUCCEEDED(device_->CreateVertexShader(vertexBytecode->GetBufferPointer(),
                    vertexBytecode->GetBufferSize(), nullptr, &vertexShader_)),
                "surface vertex shader must be created") ||
                !Expect(SUCCEEDED(device_->CreatePixelShader(opaqueBytecode->GetBufferPointer(),
                    opaqueBytecode->GetBufferSize(), nullptr, &opaqueShader_)),
                "opaque surface shader must be created") ||
                !Expect(SUCCEEDED(device_->CreatePixelShader(alphaTestBytecode->GetBufferPointer(),
                    alphaTestBytecode->GetBufferSize(), nullptr, &alphaTestShader_)),
                "alpha-test surface shader must be created") ||
                !Expect(SUCCEEDED(device_->CreatePixelShader(sortedBytecode->GetBufferPointer(),
                    sortedBytecode->GetBufferSize(), nullptr, &sortedShader_)),
                "sorted surface shader must be created") ||
                !Expect(SUCCEEDED(device_->CreatePixelShader(oitBytecode->GetBufferPointer(),
                    oitBytecode->GetBufferSize(), nullptr, &oitShader_)),
                "OIT surface shader must be created"))
                return false;

            D3D11_BUFFER_DESC constantDescription{};
            constantDescription.ByteWidth = sizeof(SurfaceConstants);
            constantDescription.Usage = D3D11_USAGE_DEFAULT;
            constantDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            return Expect(SUCCEEDED(device_->CreateBuffer(&constantDescription, nullptr, &surfaceConstants_)),
                "surface constant buffer must be created");
        }

        bool Resize(UINT width, UINT height)
        {
            ID3D11RenderTargetView* nullTarget = nullptr;
            context_->OMSetRenderTargets(1, &nullTarget, nullptr);
            ID3D11ShaderResourceView* nullResources[3] = {};
            context_->PSSetShaderResources(0, 3, nullResources);
            if (!pipeline_.Resize(device_.Get(), width, height))
                return false;
            width_ = width;
            height_ = height;
            return true;
        }

        void BeginOpaque(const Color& color)
        {
            const float clear[4] = { color.r, color.g, color.b, color.a };
            pipeline_.BeginOpaque(context_.Get(), clear);
        }

        void BeginTransparency(Mode mode) { pipeline_.BeginTransparency(context_.Get(), mode); }
        void EndTransparency() { pipeline_.EndTransparency(context_.Get()); }
        void Resolve() { pipeline_.Resolve(context_.Get()); }

        void DrawOpaque(const Color& color, float clipDepth)
        {
            Draw(color, clipDepth, clipDepth, 0.0f, opaqueShader_.Get());
        }

        void DrawAlphaTest(const Color& color, float clipDepth, float cutoff)
        {
            Draw(color, clipDepth, clipDepth, cutoff, alphaTestShader_.Get());
        }

        void DrawSorted(const Color& color, float clipDepth)
        {
            Draw(color, clipDepth, clipDepth, 0.0f, sortedShader_.Get());
        }

        void DrawOit(const Color& color, float clipDepth, float normalizedDepth)
        {
            Draw(color, clipDepth, normalizedDepth, 0.0f, oitShader_.Get());
        }

        Color ReadLinear() const
        {
            return ReadHalfTexture(pipeline_.LinearTexture(), 4);
        }

        Color ReadAccumulation() const
        {
            return ReadHalfTexture(TextureFromSrv(pipeline_.AccumulationSRV()).Get(), 4);
        }

        float ReadRevealage() const
        {
            return ReadHalfTexture(TextureFromSrv(pipeline_.RevealageSRV()).Get(), 1).r;
        }

        Color Present(float exposure, DebugView view)
        {
            D3D11_TEXTURE2D_DESC description{};
            description.Width = width_;
            description.Height = height_;
            description.MipLevels = 1;
            description.ArraySize = 1;
            description.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            description.SampleDesc.Count = 1;
            description.Usage = D3D11_USAGE_DEFAULT;
            description.BindFlags = D3D11_BIND_RENDER_TARGET;
            ComPtr<ID3D11Texture2D> texture;
            ComPtr<ID3D11RenderTargetView> target;
            if (!Expect(SUCCEEDED(device_->CreateTexture2D(&description, nullptr, &texture)),
                    "presentation texture must be created") ||
                !Expect(SUCCEEDED(device_->CreateRenderTargetView(texture.Get(), nullptr, &target)),
                    "presentation RTV must be created"))
                return { std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f };
            pipeline_.Present(context_.Get(), target.Get(), exposure, view);
            return ReadFloatTexture(texture.Get());
        }

        OitPipeline& Pipeline() { return pipeline_; }
        ID3D11DeviceContext* Context() const { return context_.Get(); }

    private:
        void Draw(const Color& color, float clipDepth, float normalizedDepth,
            float cutoff, ID3D11PixelShader* pixelShader)
        {
            const SurfaceConstants constants{ color, clipDepth, normalizedDepth, cutoff, 0.0f };
            context_->UpdateSubresource(surfaceConstants_.Get(), 0, nullptr, &constants, 0, 0);
            ID3D11Buffer* constantBuffer = surfaceConstants_.Get();
            context_->IASetInputLayout(nullptr);
            context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
            context_->VSSetConstantBuffers(0, 1, &constantBuffer);
            context_->PSSetShader(pixelShader, nullptr, 0);
            context_->PSSetConstantBuffers(0, 1, &constantBuffer);
            context_->Draw(3, 0);
        }

        static ComPtr<ID3D11Texture2D> TextureFromSrv(ID3D11ShaderResourceView* view)
        {
            ComPtr<ID3D11Resource> resource;
            ComPtr<ID3D11Texture2D> texture;
            if (view)
            {
                view->GetResource(&resource);
                static_cast<void>(resource.As(&texture));
            }
            return texture;
        }

        Color ReadHalfTexture(ID3D11Texture2D* source, UINT channelCount) const
        {
            if (!Expect(source != nullptr, "half-float source texture must exist"))
                return { std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f };
            D3D11_TEXTURE2D_DESC description{};
            source->GetDesc(&description);
            description.Usage = D3D11_USAGE_STAGING;
            description.BindFlags = 0;
            description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            description.MiscFlags = 0;
            ComPtr<ID3D11Texture2D> staging;
            if (!Expect(SUCCEEDED(device_->CreateTexture2D(&description, nullptr, &staging)),
                "half-float staging texture must be created"))
                return { std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f };
            context_->CopyResource(staging.Get(), source);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (!Expect(SUCCEEDED(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)),
                "half-float staging texture must map"))
                return { std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f };
            const auto* row = static_cast<const std::uint8_t*>(mapped.pData) +
                static_cast<size_t>(height_ / 2) * mapped.RowPitch;
            const auto* pixel = reinterpret_cast<const DirectX::PackedVector::HALF*>(row) +
                static_cast<size_t>(width_ / 2) * channelCount;
            Color result{};
            float* values[4] = { &result.r, &result.g, &result.b, &result.a };
            for (UINT channel = 0; channel < channelCount; ++channel)
                *values[channel] = DirectX::PackedVector::XMConvertHalfToFloat(pixel[channel]);
            context_->Unmap(staging.Get(), 0);
            return result;
        }

        Color ReadFloatTexture(ID3D11Texture2D* source) const
        {
            D3D11_TEXTURE2D_DESC description{};
            source->GetDesc(&description);
            description.Usage = D3D11_USAGE_STAGING;
            description.BindFlags = 0;
            description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            description.MiscFlags = 0;
            ComPtr<ID3D11Texture2D> staging;
            if (!Expect(SUCCEEDED(device_->CreateTexture2D(&description, nullptr, &staging)),
                "float staging texture must be created"))
                return { std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f };
            context_->CopyResource(staging.Get(), source);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (!Expect(SUCCEEDED(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)),
                "float staging texture must map"))
                return { std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f };
            const auto* row = static_cast<const std::uint8_t*>(mapped.pData) +
                static_cast<size_t>(height_ / 2) * mapped.RowPitch;
            const auto* pixel = reinterpret_cast<const float*>(row) + static_cast<size_t>(width_ / 2) * 4;
            const Color result{ pixel[0], pixel[1], pixel[2], pixel[3] };
            context_->Unmap(staging.Get(), 0);
            return result;
        }

        ComPtr<ID3D11Device> device_;
        ComPtr<ID3D11DeviceContext> context_;
        OitPipeline pipeline_;
        std::filesystem::path shaderDirectory_;
        UINT width_{};
        UINT height_{};
        ComPtr<ID3D11VertexShader> vertexShader_;
        ComPtr<ID3D11PixelShader> opaqueShader_;
        ComPtr<ID3D11PixelShader> alphaTestShader_;
        ComPtr<ID3D11PixelShader> sortedShader_;
        ComPtr<ID3D11PixelShader> oitShader_;
        ComPtr<ID3D11Buffer> surfaceConstants_;
    };

    bool TestEmptyTransparency(Fixture& fixture)
    {
        const Color background{ 0.12f, 0.24f, 0.36f, 1.0f };
        fixture.BeginOpaque(background);
        fixture.BeginTransparency(Mode::WeightedOit);
        fixture.EndTransparency();
        fixture.Resolve();
        return Expect(ColorNear(fixture.ReadLinear(), background, 0.003f),
            "empty OIT pass must preserve the HDR background");
    }

    bool TestSingleLayer(Fixture& fixture)
    {
        const Color background{ 0.1f, 0.2f, 0.3f, 1.0f };
        const Color layer{ 0.8f, 0.2f, 0.1f, 0.4f };
        const Color expected{
            layer.r * layer.a + background.r * (1.0f - layer.a),
            layer.g * layer.a + background.g * (1.0f - layer.a),
            layer.b * layer.a + background.b * (1.0f - layer.a), 1.0f };
        fixture.BeginOpaque(background);
        fixture.BeginTransparency(Mode::WeightedOit);
        fixture.DrawOit(layer, 0.4f, 0.4f);
        fixture.EndTransparency();
        fixture.Resolve();
        return Expect(ColorNear(fixture.ReadLinear(), expected, 0.003f),
            "one OIT layer must equal analytical straight-alpha OVER");
    }

    bool TestHdrInputClamp(Fixture& fixture)
    {
        fixture.BeginOpaque({ 0.0f, 0.0f, 0.0f, 1.0f });
        fixture.BeginTransparency(Mode::WeightedOit);
        fixture.DrawOit({ 100.0f, -2.0f, 4.0f, 0.5f }, 0.4f, 0.0f);
        fixture.EndTransparency();
        fixture.Resolve();
        // A single layer avoids overflow; expected RGB is clamp(input, 0, 8) * 0.5.
        bool passed = Expect(ColorNear(fixture.ReadLinear(), { 4.0f, 0.0f, 2.0f, 1.0f }, 0.003f),
            "one HDR layer must clamp both color bounds and preserve the in-range channel");
        passed &= Expect(Near(fixture.ReadRevealage(), 0.5f, 0.002f),
            "HDR color clamping must not change source alpha revealage");
        return passed;
    }

    bool TestOrderIndependenceAndSortedControl(Fixture& fixture)
    {
        const Color background{ 0.0f, 0.0f, 0.0f, 1.0f };
        const Color nearLayer{ 1.0f, 0.1f, 0.2f, 0.35f };
        const Color farLayer{ 0.1f, 0.9f, 0.4f, 0.65f };
        const Color expected = ExpectedOit(background, nearLayer, 0.15f, farLayer, 0.8f);
        const float expectedRevealage = (1.0f - nearLayer.a) * (1.0f - farLayer.a);

        auto renderOit = [&](bool reversed, Color& color, float& revealage)
        {
            fixture.BeginOpaque(background);
            fixture.BeginTransparency(Mode::WeightedOit);
            if (reversed)
            {
                fixture.DrawOit(farLayer, 0.8f, 0.8f);
                fixture.DrawOit(nearLayer, 0.15f, 0.15f);
            }
            else
            {
                fixture.DrawOit(nearLayer, 0.15f, 0.15f);
                fixture.DrawOit(farLayer, 0.8f, 0.8f);
            }
            fixture.EndTransparency();
            fixture.Resolve();
            color = fixture.ReadLinear();
            revealage = fixture.ReadRevealage();
        };

        Color forward{};
        Color reverse{};
        float forwardRevealage{};
        float reverseRevealage{};
        renderOit(false, forward, forwardRevealage);
        renderOit(true, reverse, reverseRevealage);
        bool passed = true;
        passed &= Expect(ColorNear(forward, expected, 0.005f) && ColorNear(reverse, expected, 0.005f),
            "two-layer OIT must match the weighted analytical result in both orders");
        passed &= Expect(MaxRgbDifference(forward, reverse) <= 0.005f,
            "reversing two OIT draws must preserve color");
        passed &= Expect(Near(forwardRevealage, expectedRevealage, 0.002f) &&
            Near(reverseRevealage, expectedRevealage, 0.002f),
            "OIT revealage must equal the product of inverse source alphas");

        auto renderSorted = [&](bool reversed)
        {
            fixture.BeginOpaque(background);
            fixture.BeginTransparency(Mode::SortedBlend);
            if (reversed)
            {
                fixture.DrawSorted(farLayer, 0.8f);
                fixture.DrawSorted(nearLayer, 0.15f);
            }
            else
            {
                fixture.DrawSorted(nearLayer, 0.15f);
                fixture.DrawSorted(farLayer, 0.8f);
            }
            fixture.EndTransparency();
            fixture.Resolve();
            return fixture.ReadLinear();
        };
        const Color sortedForward = renderSorted(false);
        const Color sortedReverse = renderSorted(true);
        passed &= Expect(MaxRgbDifference(sortedForward, sortedReverse) > 0.05f,
            "reversing ordinary OVER draws must visibly change color");
        return passed;
    }

    bool TestDepthAndAlphaRules(Fixture& fixture)
    {
        bool passed = true;
        const Color black{ 0.0f, 0.0f, 0.0f, 1.0f };
        const Color opaqueBlue{ 0.05f, 0.1f, 0.8f, 1.0f };
        fixture.BeginOpaque(black);
        fixture.DrawOpaque(opaqueBlue, 0.2f);
        fixture.BeginTransparency(Mode::WeightedOit);
        fixture.DrawOit({ 1.0f, 0.0f, 0.0f, 0.9f }, 0.8f, 0.8f);
        fixture.EndTransparency();
        fixture.Resolve();
        passed &= Expect(ColorNear(fixture.ReadLinear(), opaqueBlue, 0.003f) &&
            Near(fixture.ReadRevealage(), 1.0f, 0.002f),
            "opaque depth must reject rear OIT fragments");

        fixture.BeginOpaque(black);
        fixture.BeginTransparency(Mode::WeightedOit);
        fixture.DrawOit({ 8.0f, 7.0f, 6.0f, 0.0f }, 0.1f, 0.1f);
        fixture.EndTransparency();
        fixture.Resolve();
        passed &= Expect(ColorNear(fixture.ReadLinear(), black, 0.003f) &&
            Near(fixture.ReadRevealage(), 1.0f, 0.002f),
            "alpha-zero OIT fragments must write neither accumulation nor revealage");

        fixture.BeginOpaque(black);
        fixture.BeginTransparency(Mode::SortedBlend);
        fixture.DrawSorted({ 1.0f, 1.0f, 1.0f, 0.0f }, 0.1f);
        fixture.DrawSorted({ 1.0f, 0.0f, 0.0f, 1.0f }, 0.7f);
        fixture.EndTransparency();
        fixture.Resolve();
        passed &= Expect(ColorNear(fixture.ReadLinear(), { 1.0f, 0.0f, 0.0f, 1.0f }, 0.003f),
            "alpha-zero sorted fragments must not write depth");

        fixture.BeginOpaque(black);
        fixture.BeginTransparency(Mode::AlphaTest);
        fixture.DrawAlphaTest({ 0.0f, 1.0f, 0.0f, 0.49f }, 0.2f, 0.5f);
        fixture.DrawAlphaTest({ 1.0f, 0.0f, 0.0f, 1.0f }, 0.7f, 0.5f);
        fixture.EndTransparency();
        fixture.Resolve();
        passed &= Expect(ColorNear(fixture.ReadLinear(), { 1.0f, 0.0f, 0.0f, 1.0f }, 0.003f),
            "discarded alpha-test fragments must not write color or depth");

        fixture.BeginOpaque(black);
        fixture.BeginTransparency(Mode::AlphaTest);
        fixture.DrawAlphaTest({ 0.0f, 1.0f, 0.0f, 0.51f }, 0.2f, 0.5f);
        fixture.DrawAlphaTest({ 1.0f, 0.0f, 0.0f, 1.0f }, 0.7f, 0.5f);
        fixture.EndTransparency();
        fixture.Resolve();
        passed &= Expect(ColorNear(fixture.ReadLinear(), { 0.0f, 1.0f, 0.0f, 1.0f }, 0.003f),
            "retained alpha-test fragments must write color and depth");
        return passed;
    }

    bool TestResizeAndDebugPresentation(Fixture& fixture)
    {
        bool passed = true;
        passed &= Expect(!fixture.Resize(0, 7), "zero-width resize must be rejected");
        passed &= Expect(fixture.Resize(13, 7), "nonzero resize must recreate all pipeline targets");
        D3D11_TEXTURE2D_DESC description{};
        fixture.Pipeline().LinearTexture()->GetDesc(&description);
        passed &= Expect(description.Width == 13 && description.Height == 7,
            "resized linear texture must expose the requested dimensions");

        const Color background{ 0.0f, 0.0f, 0.0f, 1.0f };
        const Color nearLayer{ 1.0f, 0.1f, 0.2f, 0.35f };
        const Color farLayer{ 0.1f, 0.9f, 0.4f, 0.65f };
        const Color expectedComposite = ExpectedOit(background, nearLayer, 0.15f, farLayer, 0.8f);
        const float nearFactor = nearLayer.a * OitWeight(0.15f);
        const float farFactor = farLayer.a * OitWeight(0.8f);
        const float denominator = nearFactor + farFactor;
        const Color expectedAccumulation{
            (nearLayer.r * nearFactor + farLayer.r * farFactor) / denominator,
            (nearLayer.g * nearFactor + farLayer.g * farFactor) / denominator,
            (nearLayer.b * nearFactor + farLayer.b * farFactor) / denominator, 1.0f };
        const float revealage = (1.0f - nearLayer.a) * (1.0f - farLayer.a);

        fixture.BeginOpaque(background);
        fixture.BeginTransparency(Mode::WeightedOit);
        fixture.DrawOit(nearLayer, 0.15f, 0.15f);
        fixture.DrawOit(farLayer, 0.8f, 0.8f);
        fixture.EndTransparency();
        fixture.Resolve();
        const float exposure = 1.25f;
        const Color composite = fixture.Present(exposure, DebugView::Composite);
        const Color accumulation = fixture.Present(exposure, DebugView::Accumulation);
        const Color reveal = fixture.Present(exposure, DebugView::Revealage);
        passed &= Expect(ColorNear(composite, DisplayColor(expectedComposite, exposure), 0.005f),
            "Composite presentation must tone-map and convert the resolved linear HDR once");
        passed &= Expect(ColorNear(accumulation, DisplayColor(expectedAccumulation, exposure), 0.005f),
            "Accumulation presentation must display normalized weighted color");
        passed &= Expect(ColorNear(reveal,
            DisplayColor({ revealage, revealage, revealage, 1.0f }, exposure), 0.005f),
            "Revealage presentation must display finite grayscale revealage");
        return passed;
    }

    bool TestDenseLayersRemainFinite(Fixture& fixture)
    {
        fixture.BeginOpaque({ 0.03f, 0.04f, 0.05f, 1.0f });
        fixture.BeginTransparency(Mode::WeightedOit);
        for (int layer = 0; layer < 1200; ++layer)
            fixture.DrawOit({ 100.0f, 80.0f, 60.0f, 1.0f }, 0.01f, 0.0f);
        fixture.EndTransparency();
        fixture.Resolve();
        const Color resolved = fixture.ReadLinear();
        const Color presented = fixture.Present(1.0f, DebugView::Composite);
        return Expect(std::isfinite(resolved.r) && std::isfinite(resolved.g) &&
                std::isfinite(resolved.b) && std::isfinite(presented.r) &&
                std::isfinite(presented.g) && std::isfinite(presented.b),
            "dense high-color accumulation must resolve and present finite output");
    }

    bool TestGpuTimings(Fixture& fixture)
    {
        bool passed = Expect(fixture.Pipeline().Timings().available,
            "WARP must expose the optional timestamp query ring");
        for (int frame = 0; frame < 16 && !fixture.Pipeline().Timings().valid; ++frame)
        {
            fixture.BeginOpaque({ 0.0f, 0.0f, 0.0f, 1.0f });
            fixture.BeginTransparency(Mode::WeightedOit);
            fixture.EndTransparency();
            fixture.Resolve();
            // A staging Map is an intentional test-only completion point. Production
            // timing polling remains DONOTFLUSH and never waits for a query.
            static_cast<void>(fixture.ReadLinear());
        }
        const auto& timings = fixture.Pipeline().Timings();
        passed &= Expect(timings.valid && std::isfinite(timings.transparentMs) &&
                std::isfinite(timings.compositeMs) && timings.transparentMs >= 0.0 &&
                timings.compositeMs >= 0.0,
            "nonblocking query polling must eventually publish finite WARP timings");
        return passed;
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: OitPipelineTests <repository-root>\n";
        return 2;
    }

    Fixture fixture;
    if (!fixture.Initialize(std::filesystem::path(argv[1])))
        return 1;

    bool passed = true;
    passed &= TestGpuTimings(fixture);
    passed &= TestEmptyTransparency(fixture);
    passed &= TestSingleLayer(fixture);
    passed &= TestHdrInputClamp(fixture);
    passed &= TestOrderIndependenceAndSortedControl(fixture);
    passed &= TestDepthAndAlphaRules(fixture);
    passed &= TestResizeAndDebugPresentation(fixture);
    passed &= TestDenseLayersRemainFinite(fixture);
    if (!passed)
        return 1;

    std::cout << "OIT pipeline WARP pixel tests passed: 8 scenarios.\n";
    return 0;
}
