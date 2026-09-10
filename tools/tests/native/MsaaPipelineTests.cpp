#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXPackedVector.h>
#include <wrl/client.h>

#include "MsaaPipeline.h"

namespace
{
using Coverage40::Mode;
using Coverage40::MsaaPipeline;
using Coverage40::SurfaceRule;
using Microsoft::WRL::ComPtr;

constexpr float kLinearTolerance = 0.003f;
constexpr float kUnormTolerance = 2.0f / 255.0f;

struct Color { float r; float g; float b; float a; };

struct SurfaceConstants
{
    Color color;
    float clipDepth;
    std::uint32_t rule;
    float cutoff;
    float padding;
};

std::string HResultMessage(const char* operation, HRESULT result)
{
    std::ostringstream stream;
    stream << operation << " failed with HRESULT 0x" << std::hex << std::uppercase
        << static_cast<unsigned long>(result);
    return stream.str();
}

void ThrowIfFailed(HRESULT result, const char* operation)
{
    if (FAILED(result))
        throw std::runtime_error(HResultMessage(operation, result));
}

bool Near(float actual, float expected, float tolerance = kLinearTolerance)
{
    return std::isfinite(actual) && std::abs(actual - expected) <= tolerance;
}

bool ColorNear(const Color& actual, const Color& expected,
    float tolerance = kLinearTolerance)
{
    return Near(actual.r, expected.r, tolerance) &&
        Near(actual.g, expected.g, tolerance) &&
        Near(actual.b, expected.b, tolerance);
}

bool Expect(bool condition, const char* message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

ComPtr<ID3DBlob> CompileShader(const std::filesystem::path& path,
    const char* entryPoint, const char* target)
{
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    const HRESULT result = D3DCompileFromFile(path.c_str(), nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, target, flags, 0,
        &bytecode, &errors);
    if (FAILED(result))
    {
        std::ostringstream message;
        message << HResultMessage(entryPoint, result);
        if (errors)
            message << ": " << static_cast<const char*>(errors->GetBufferPointer());
        throw std::runtime_error(message.str());
    }
    return bytecode;
}

float DisplayChannel(float linear, float exposure)
{
    const float x = std::max(linear * exposure, 0.0f);
    const float y = x / (1.0f + x);
    return y <= 0.0031308f ? 12.92f * y :
        1.055f * std::pow(y, 1.0f / 2.4f) - 0.055f;
}

class Fixture
{
public:
    void Initialize(const std::filesystem::path& repo)
    {
        D3D_FEATURE_LEVEL requested = D3D_FEATURE_LEVEL_11_0;
        D3D_FEATURE_LEVEL actual{};
        HRESULT result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            D3D11_CREATE_DEVICE_DEBUG, &requested, 1, D3D11_SDK_VERSION,
            &device_, &actual, &context_);
        if (result == DXGI_ERROR_SDK_COMPONENT_MISSING)
        {
            debugLayerMissing_ = true;
            result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                &requested, 1, D3D11_SDK_VERSION, &device_, &actual, &context_);
        }
        ThrowIfFailed(result, "D3D11CreateDevice(WARP)");
        if (actual != D3D_FEATURE_LEVEL_11_0)
            throw std::runtime_error("WARP did not provide feature level 11_0");

        if (!debugLayerMissing_)
            ThrowIfFailed(device_.As(&infoQueue_), "QueryInterface(ID3D11InfoQueue)");

        shaderDirectory_ = repo / L"Dx11" / L"40_MSAA_AlphaToCoverage";
        if (!pipeline_.Initialize(device_.Get(), shaderDirectory_.wstring()))
        {
            std::wcerr << L"FAIL: MsaaPipeline::Initialize: "
                << pipeline_.Support().reason << L'\n';
            throw std::runtime_error("MsaaPipeline::Initialize failed");
        }

        const auto surfacePath = repo / L"tools" / L"tests" / L"native" /
            L"MsaaTestSurface.hlsl";
        const auto vertexBytecode = CompileShader(surfacePath, "VSMain", "vs_5_0");
        const auto pixelBytecode = CompileShader(surfacePath, "PSMain", "ps_5_0");
        ThrowIfFailed(device_->CreateVertexShader(vertexBytecode->GetBufferPointer(),
            vertexBytecode->GetBufferSize(), nullptr, &vertexShader_),
            "CreateVertexShader(test surface)");
        ThrowIfFailed(device_->CreatePixelShader(pixelBytecode->GetBufferPointer(),
            pixelBytecode->GetBufferSize(), nullptr, &pixelShader_),
            "CreatePixelShader(test surface)");

        D3D11_BUFFER_DESC constantDescription{};
        constantDescription.ByteWidth = sizeof(SurfaceConstants);
        constantDescription.Usage = D3D11_USAGE_DEFAULT;
        constantDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        ThrowIfFailed(device_->CreateBuffer(&constantDescription, nullptr, &surfaceConstants_),
            "CreateBuffer(test surface constants)");

        D3D11_RASTERIZER_DESC rasterDescription{};
        rasterDescription.FillMode = D3D11_FILL_SOLID;
        rasterDescription.CullMode = D3D11_CULL_NONE;
        rasterDescription.DepthClipEnable = TRUE;
        rasterDescription.MultisampleEnable = TRUE;
        ThrowIfFailed(device_->CreateRasterizerState(&rasterDescription, &surfaceRasterizer_),
            "CreateRasterizerState(test surface)");
    }

    MsaaPipeline& Pipeline() { return pipeline_; }
    ID3D11Device* Device() { return device_.Get(); }
    ID3D11DeviceContext* Context() { return context_.Get(); }

    void Configure(Mode mode) { ConfigureSize(16, 16, mode); }

    void ConfigureSize(UINT width, UINT height, Mode mode)
    {
        ID3D11RenderTargetView* nullTarget = nullptr;
        context_->OMSetRenderTargets(1, &nullTarget, nullptr);
        ID3D11ShaderResourceView* nullResource = nullptr;
        context_->PSSetShaderResources(0, 1, &nullResource);
        if (!pipeline_.Configure(device_.Get(), width, height, mode))
            throw std::runtime_error("MsaaPipeline::Configure failed");
    }

    void Begin(Color background)
    {
        const float clear[4] = { background.r, background.g, background.b, background.a };
        pipeline_.BeginFrame(context_.Get(), clear);
    }

    void Draw(Color color, float z, SurfaceRule rule, float cutoff = 0.5f,
        UINT sampleMask = 0xffffffff)
    {
        pipeline_.BeginSurface(context_.Get(), rule);
        ComPtr<ID3D11BlendState> blendState;
        float blendFactor[4]{};
        UINT currentMask{};
        context_->OMGetBlendState(&blendState, blendFactor, &currentMask);
        context_->OMSetBlendState(blendState.Get(), blendFactor, sampleMask);

        const SurfaceConstants constants{
            color, z, static_cast<std::uint32_t>(rule), cutoff, 0.0f };
        context_->UpdateSubresource(surfaceConstants_.Get(), 0, nullptr, &constants, 0, 0);
        ID3D11Buffer* constantBuffer = surfaceConstants_.Get();
        context_->IASetInputLayout(nullptr);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context_->RSSetState(surfaceRasterizer_.Get());
        context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
        context_->VSSetConstantBuffers(0, 1, &constantBuffer);
        context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
        context_->PSSetConstantBuffers(0, 1, &constantBuffer);
        context_->Draw(3, 0);
        context_->OMSetBlendState(blendState.Get(), blendFactor, 0xffffffff);
    }

    void Finish()
    {
        pipeline_.EndScene(context_.Get());
        pipeline_.Resolve(context_.Get());
    }

    Color ReadLinear()
    {
        ID3D11Texture2D* source = pipeline_.LinearTexture();
        if (!source)
            throw std::runtime_error("linear texture is missing");
        D3D11_TEXTURE2D_DESC description{};
        source->GetDesc(&description);
        if (description.SampleDesc.Count != 1 || description.Format != DXGI_FORMAT_R16G16B16A16_FLOAT)
            throw std::runtime_error("linear texture is not single-sample RGBA16F");
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        description.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> staging;
        ThrowIfFailed(device_->CreateTexture2D(&description, nullptr, &staging),
            "CreateTexture2D(linear staging)");
        context_->CopyResource(staging.Get(), source);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        ThrowIfFailed(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped),
            "Map(linear staging)");
        const auto* row = static_cast<const std::uint8_t*>(mapped.pData) + 8 * mapped.RowPitch;
        const auto* pixel = reinterpret_cast<const DirectX::PackedVector::HALF*>(row) + 8 * 4;
        const Color value{
            DirectX::PackedVector::XMConvertHalfToFloat(pixel[0]),
            DirectX::PackedVector::XMConvertHalfToFloat(pixel[1]),
            DirectX::PackedVector::XMConvertHalfToFloat(pixel[2]),
            DirectX::PackedVector::XMConvertHalfToFloat(pixel[3]) };
        context_->Unmap(staging.Get(), 0);
        return value;
    }

    Color Present(float exposure)
    {
        D3D11_TEXTURE2D_DESC description{};
        description.Width = pipeline_.Width();
        description.Height = pipeline_.Height();
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> output;
        ComPtr<ID3D11RenderTargetView> target;
        ThrowIfFailed(device_->CreateTexture2D(&description, nullptr, &output),
            "CreateTexture2D(present output)");
        ThrowIfFailed(device_->CreateRenderTargetView(output.Get(), nullptr, &target),
            "CreateRenderTargetView(present output)");
        pipeline_.Present(context_.Get(), target.Get(), exposure);

        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> staging;
        ThrowIfFailed(device_->CreateTexture2D(&description, nullptr, &staging),
            "CreateTexture2D(present staging)");
        context_->CopyResource(staging.Get(), output.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        ThrowIfFailed(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped),
            "Map(present staging)");
        const auto* pixel = static_cast<const std::uint8_t*>(mapped.pData) +
            8 * mapped.RowPitch + 8 * 4;
        const Color value{ pixel[0] / 255.0f, pixel[1] / 255.0f,
            pixel[2] / 255.0f, pixel[3] / 255.0f };
        context_->Unmap(staging.Get(), 0);
        return value;
    }

    bool RuntimeSupports4x() const
    {
        UINT colorLevels{};
        UINT depthLevels{};
        const HRESULT colorResult = device_->CheckMultisampleQualityLevels(
            DXGI_FORMAT_R16G16B16A16_FLOAT, 4, &colorLevels);
        const HRESULT depthResult = device_->CheckMultisampleQualityLevels(
            DXGI_FORMAT_D32_FLOAT, 4, &depthLevels);
        return SUCCEEDED(colorResult) && colorLevels > 0 &&
            SUCCEEDED(depthResult) && depthLevels > 0;
    }

    bool CheckDebugMessages()
    {
        if (!infoQueue_)
        {
            std::cout << "INFO: D3D11 debug layer unavailable "
                "(DXGI_ERROR_SDK_COMPONENT_MISSING); messages not collected.\n";
            return true;
        }

        bool passed = true;
        UINT warnings = 0;
        UINT errors = 0;
        const UINT64 messageCount = infoQueue_->GetNumStoredMessagesAllowedByRetrievalFilter();
        for (UINT64 index = 0; index < messageCount; ++index)
        {
            SIZE_T size{};
            ThrowIfFailed(infoQueue_->GetMessage(index, nullptr, &size),
                "ID3D11InfoQueue::GetMessage(size)");
            std::vector<std::uint8_t> storage(size);
            auto* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
            ThrowIfFailed(infoQueue_->GetMessage(index, message, &size),
                "ID3D11InfoQueue::GetMessage(data)");
            if (message->Severity == D3D11_MESSAGE_SEVERITY_CORRUPTION ||
                message->Severity == D3D11_MESSAGE_SEVERITY_ERROR)
            {
                std::cerr << "FAIL: D3D11 debug message: " << message->pDescription << '\n';
                ++errors;
                passed = false;
            }
            else if (message->Severity == D3D11_MESSAGE_SEVERITY_WARNING)
            {
                ++warnings;
                std::cerr << "WARNING: D3D11 debug message: " << message->pDescription << '\n';
            }
        }
        std::cout << "INFO: D3D11 debug audit: " << errors
            << " error/corruption, " << warnings << " warning.\n";
        passed &= Expect(warnings == 0, "D3D11 debug layer must report no warnings");
        return passed;
    }

private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11InfoQueue> infoQueue_;
    MsaaPipeline pipeline_;
    std::filesystem::path shaderDirectory_;
    bool debugLayerMissing_{};
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11Buffer> surfaceConstants_;
    ComPtr<ID3D11RasterizerState> surfaceRasterizer_;
};

bool TestEmptyAlphaTest1x(Fixture& fixture)
{
    fixture.Configure(Mode::AlphaTest1x);
    const Color background{ 0.1f, 0.2f, 0.3f, 1.0f };
    fixture.Begin(background);
    fixture.Finish();
    return Expect(ColorNear(fixture.ReadLinear(), background),
        "Alpha Test 1x empty frame must preserve the clear color");
}

bool TestAlphaTestModes(Fixture& fixture)
{
    bool passed = true;
    for (const Mode mode : { Mode::AlphaTest1x, Mode::AlphaTest4x })
    {
        fixture.Configure(mode);
        const Color background{ 0.1f, 0.2f, 0.3f, 1.0f };
        fixture.Begin(background);
        fixture.Draw({ 0.0f, 1.0f, 0.0f, 0.49f }, 0.2f,
            SurfaceRule::TestCoverage, 0.5f);
        fixture.Finish();
        passed &= Expect(ColorNear(fixture.ReadLinear(), background),
            "Alpha Test alpha 0.49 must preserve the background");

        fixture.Begin(background);
        fixture.Draw({ 0.0f, 1.0f, 0.0f, 0.51f }, 0.2f,
            SurfaceRule::TestCoverage, 0.5f);
        fixture.Finish();
        passed &= Expect(ColorNear(fixture.ReadLinear(), { 0.0f, 1.0f, 0.0f, 1.0f }),
            "Alpha Test alpha 0.51 must retain the foreground");

        fixture.Begin({ 0.0f, 0.0f, 0.0f, 1.0f });
        fixture.Draw({ 0.0f, 1.0f, 0.0f, 0.0f }, 0.2f,
            SurfaceRule::TestCoverage, 0.0f);
        fixture.Draw({ 1.0f, 0.0f, 0.0f, 1.0f }, 0.7f, SurfaceRule::Opaque);
        fixture.Finish();
        passed &= Expect(ColorNear(fixture.ReadLinear(), { 1.0f, 0.0f, 0.0f, 1.0f }),
            "TestCoverage alpha zero at cutoff zero must not write color or depth");
    }
    return passed;
}

bool TestAlphaToCoverageEndpointsAndDepth(Fixture& fixture)
{
    fixture.Configure(Mode::AlphaToCoverage4x);
    bool passed = true;
    fixture.Begin({ 0.1f, 0.2f, 0.3f, 1.0f });
    fixture.Draw({ 1.0f, 0.0f, 0.0f, 0.0f }, 0.2f, SurfaceRule::AlphaToCoverage);
    fixture.Finish();
    passed &= Expect(ColorNear(fixture.ReadLinear(), { 0.1f, 0.2f, 0.3f, 1.0f }),
        "A2C alpha zero must preserve background coverage");

    fixture.Begin({ 0.0f, 0.0f, 0.0f, 1.0f });
    fixture.Draw({ 0.0f, 1.0f, 0.0f, 0.0f }, 0.2f, SurfaceRule::AlphaToCoverage);
    fixture.Draw({ 1.0f, 0.0f, 0.0f, 1.0f }, 0.7f, SurfaceRule::Opaque);
    fixture.Finish();
    passed &= Expect(ColorNear(fixture.ReadLinear(), { 1.0f, 0.0f, 0.0f, 1.0f }),
        "A2C alpha zero must preserve depth");

    fixture.Begin({ 0.0f, 0.0f, 0.0f, 1.0f });
    fixture.Draw({ 0.0f, 1.0f, 0.0f, 1.0f }, 0.3f, SurfaceRule::AlphaToCoverage);
    fixture.Finish();
    passed &= Expect(ColorNear(fixture.ReadLinear(), { 0.0f, 1.0f, 0.0f, 1.0f }),
        "A2C alpha one must cover every sample");

    fixture.Begin({ 0.0f, 0.0f, 0.0f, 1.0f });
    fixture.Draw({ 0.0f, 0.0f, 1.0f, 1.0f }, 0.2f, SurfaceRule::Opaque);
    fixture.Draw({ 1.0f, 0.0f, 0.0f, 1.0f }, 0.6f, SurfaceRule::AlphaToCoverage);
    fixture.Finish();
    passed &= Expect(ColorNear(fixture.ReadLinear(), { 0.0f, 0.0f, 1.0f, 1.0f }),
        "opaque depth must reject rear A2C fragments");
    return passed;
}

bool TestPartialCoverage(Fixture& fixture)
{
    fixture.Configure(Mode::AlphaToCoverage4x);
    float selectedAlpha{};
    float selectedRed{};
    for (int step = 1; step < 32; ++step)
    {
        const float alpha = static_cast<float>(step) / 32.0f;
        fixture.Begin({ 0.0f, 0.0f, 0.0f, 1.0f });
        fixture.Draw({ 1.0f, 0.0f, 0.0f, alpha }, 0.3f,
            SurfaceRule::AlphaToCoverage);
        fixture.Finish();
        const float red = fixture.ReadLinear().r;
        if (red > kLinearTolerance && red < 1.0f - kLinearTolerance)
        {
            selectedAlpha = alpha;
            selectedRed = red;
            break;
        }
    }
    if (!Expect(selectedAlpha > 0.0f, "A2C scan must find non-vacuous partial coverage"))
        return false;
    std::cout << "INFO: partial A2C alpha=" << selectedAlpha
        << ", resolved red=" << selectedRed << '\n';

    fixture.Begin({ 0.0f, 0.0f, 0.0f, 1.0f });
    fixture.Draw({ 1.0f, 0.0f, 0.0f, selectedAlpha }, 0.3f,
        SurfaceRule::AlphaToCoverage);
    fixture.Draw({ 0.0f, 0.0f, 1.0f, 1.0f }, 0.6f, SurfaceRule::Opaque);
    fixture.Finish();
    const Color value = fixture.ReadLinear();
    return Expect(Near(value.r, selectedRed) && Near(value.g, 0.0f) &&
            Near(value.b, 1.0f - selectedRed),
        "partial A2C depth must allow rear blue only through uncovered samples");
}

bool TestStateRestoration(Fixture& fixture)
{
    fixture.Configure(Mode::AlphaToCoverage4x);
    bool passed = true;
    fixture.Begin({ 0.0f, 0.0f, 0.0f, 1.0f });
    fixture.Draw({ 1.0f, 0.0f, 0.0f, 0.0f }, 0.3f, SurfaceRule::AlphaToCoverage);
    fixture.Draw({ 0.0f, 1.0f, 0.0f, 0.25f }, 0.2f, SurfaceRule::Blend);
    fixture.Finish();
    passed &= Expect(ColorNear(fixture.ReadLinear(), { 0.0f, 0.25f, 0.0f, 1.0f }),
        "Blend after A2C must restore ordinary straight-alpha blending");

    fixture.Begin({ 0.0f, 0.0f, 0.0f, 1.0f });
    fixture.Draw({ 0.0f, 1.0f, 0.0f, 0.5f }, 0.2f, SurfaceRule::Blend);
    fixture.Draw({ 1.0f, 0.0f, 0.0f, 1.0f }, 0.7f, SurfaceRule::Opaque);
    fixture.Finish();
    passed &= Expect(ColorNear(fixture.ReadLinear(), { 1.0f, 0.0f, 0.0f, 1.0f }),
        "Blend surfaces must not write depth");
    return passed;
}

bool TestAuthoredMaskDifference(Fixture& fixture)
{
    fixture.Configure(Mode::AlphaTest4x);
    bool passed = true;
    fixture.Begin({ 0.0f, 0.0f, 0.0f, 1.0f });
    fixture.Draw({ 0.0f, 1.0f, 0.0f, 0.0f }, 0.2f, SurfaceRule::Mask, 0.0f);
    fixture.Draw({ 1.0f, 0.0f, 0.0f, 1.0f }, 0.7f, SurfaceRule::Opaque);
    fixture.Finish();
    passed &= Expect(ColorNear(fixture.ReadLinear(), { 0.0f, 1.0f, 0.0f, 1.0f }),
        "authored MASK alpha zero at cutoff zero must retain original clip semantics");

    fixture.Begin({ 0.0f, 0.0f, 0.0f, 1.0f });
    fixture.Draw({ 0.0f, 1.0f, 0.0f, 0.0f }, 0.2f,
        SurfaceRule::TestCoverage, 0.0f);
    fixture.Draw({ 1.0f, 0.0f, 0.0f, 1.0f }, 0.7f, SurfaceRule::Opaque);
    fixture.Finish();
    passed &= Expect(ColorNear(fixture.ReadLinear(), { 1.0f, 0.0f, 0.0f, 1.0f }),
        "comparison TestCoverage alpha zero must discard before cutoff testing");
    return passed;
}

bool TestResolveSampleAverage(Fixture& fixture)
{
    fixture.Configure(Mode::AlphaToCoverage4x);
    const std::array<Color, 4> colors = {{
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 1.0f, 1.0f },
        { 1.0f, 1.0f, 1.0f, 1.0f }
    }};
    fixture.Begin({ 0.0f, 0.0f, 0.0f, 1.0f });
    for (UINT sample = 0; sample < colors.size(); ++sample)
        fixture.Draw(colors[sample], 0.3f, SurfaceRule::AlphaToCoverage,
            0.5f, 1u << sample);
    fixture.Finish();
    return Expect(ColorNear(fixture.ReadLinear(), { 0.5f, 0.5f, 0.5f, 1.0f }),
        "production ResolveSubresource must average all four addressed samples");
}

bool TestPresentConversion(Fixture& fixture)
{
    fixture.Configure(Mode::AlphaTest1x);
    const Color linear{ 0.25f, 1.0f, 4.0f, 1.0f };
    const float exposure = 1.25f;
    fixture.Begin(linear);
    fixture.Finish();
    const Color value = fixture.Present(exposure);
    const Color expected{ DisplayChannel(linear.r, exposure),
        DisplayChannel(linear.g, exposure), DisplayChannel(linear.b, exposure), 1.0f };
    return Expect(ColorNear(value, expected, kUnormTolerance) &&
            Near(value.a, 1.0f, kUnormTolerance),
        "Present must tone-map and convert the resolved HDR texture exactly once");
}

bool TestConfigurationTransitions(Fixture& fixture)
{
    bool passed = true;
    fixture.Configure(Mode::AlphaTest4x);
    ID3D11Texture2D* fourSampleColor = fixture.Pipeline().ColorTexture();
    ID3D11Texture2D* fourSampleLinear = fixture.Pipeline().LinearTexture();
    passed &= Expect(fourSampleColor && fourSampleLinear &&
            fourSampleColor != fourSampleLinear,
        "4x modes must expose separate multisample color and linear resolve textures");
    passed &= Expect(fixture.Pipeline().Samples() == 4 &&
            fixture.Pipeline().MemoryBytes() == 16ULL * 16ULL * 56ULL,
        "4x getters must report sample count and target memory");

    fixture.Configure(Mode::AlphaToCoverage4x);
    passed &= Expect(fixture.Pipeline().ColorTexture() == fourSampleColor &&
            fixture.Pipeline().LinearTexture() == fourSampleLinear,
        "same-size transitions between 4x modes must retain resource addresses");

    fixture.Configure(Mode::AlphaTest1x);
    passed &= Expect(fixture.Pipeline().ColorTexture() == fixture.Pipeline().LinearTexture(),
        "1x color texture must also be the linear texture");

    fixture.ConfigureSize(32, 32, Mode::AlphaToCoverage4x);
    fixture.Begin({ 0.2f, 0.3f, 0.4f, 1.0f });
    fixture.Finish();
    passed &= Expect(ColorNear(fixture.ReadLinear(), { 0.2f, 0.3f, 0.4f, 1.0f }),
        "16-to-32 resize must produce a valid resolved pixel");

    const UINT oldWidth = fixture.Pipeline().Width();
    const UINT oldHeight = fixture.Pipeline().Height();
    const Mode oldMode = fixture.Pipeline().CurrentMode();
    ID3D11Texture2D* oldColor = fixture.Pipeline().ColorTexture();
    ID3D11Texture2D* oldLinear = fixture.Pipeline().LinearTexture();
    passed &= Expect(!fixture.Pipeline().Configure(fixture.Device(), 0, 16,
            Mode::AlphaTest1x),
        "zero-width Configure must be rejected");
    passed &= Expect(!fixture.Pipeline().Configure(fixture.Device(), 16385, 16,
            Mode::AlphaTest1x),
        "over-limit Configure must be rejected");
    passed &= Expect(fixture.Pipeline().Width() == oldWidth &&
            fixture.Pipeline().Height() == oldHeight &&
            fixture.Pipeline().CurrentMode() == oldMode &&
            fixture.Pipeline().ColorTexture() == oldColor &&
            fixture.Pipeline().LinearTexture() == oldLinear,
        "rejected Configure calls must preserve the previous complete configuration");

    const std::array<Mode, 3> modes = { Mode::AlphaTest1x,
        Mode::AlphaTest4x, Mode::AlphaToCoverage4x };
    for (int frame = 0; frame < 20; ++frame)
    {
        fixture.Configure(modes[static_cast<size_t>(frame) % modes.size()]);
        const float channel = static_cast<float>(frame + 1) / 32.0f;
        fixture.Begin({ channel, 0.0f, 0.0f, 1.0f });
        fixture.Finish();
        passed &= Expect(Near(fixture.ReadLinear().r, channel),
            "20-mode transition frames must clear prior frame results");
    }
    return passed;
}

bool ReportScenario(const char* name, bool passed)
{
    if (passed)
        std::cout << "PASS: " << name << '\n';
    return passed;
}

int InitialTestOutcome(bool oneSamplePassed, bool runtimeSupports4x)
{
    if (!oneSamplePassed)
        return 1;
    if (!runtimeSupports4x)
        return 2;
    return 0;
}

bool TestOneSampleFailurePrecedesUnsupported4xSkip()
{
    return Expect(InitialTestOutcome(false, false) == 1,
        "a failed 1x pixel test must take precedence over an unavailable-4x skip");
}
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: MsaaPipelineTests <repository-root>\n";
        return 2;
    }

    try
    {
        if (!ReportScenario("failed 1x result precedes unsupported-4x skip",
                TestOneSampleFailurePrecedesUnsupported4xSkip()))
            return 1;

        Fixture fixture;
        fixture.Initialize(std::filesystem::path(argv[1]));
        bool passed = ReportScenario("empty Alpha Test 1x clear preservation",
            TestEmptyAlphaTest1x(fixture));

        const int initialOutcome = InitialTestOutcome(passed, fixture.RuntimeSupports4x());
        if (initialOutcome != 0)
        {
            if (initialOutcome == 2)
                std::cout << "SKIP: WARP lacks 4x RGBA16F/D32 quality levels.\n";
            return initialOutcome;
        }
        passed &= ReportScenario("runtime 4x capability",
            Expect(fixture.Pipeline().Support().supports4x,
                "pipeline capability must expose runtime 4x support"));
        passed &= ReportScenario("Alpha Test 1x/4x cutoff and depth", TestAlphaTestModes(fixture));
        passed &= ReportScenario("A2C 4x endpoints and opaque depth",
            TestAlphaToCoverageEndpointsAndDepth(fixture));
        passed &= ReportScenario("A2C 4x partial coverage composition", TestPartialCoverage(fixture));
        passed &= ReportScenario("A2C/blend state restoration", TestStateRestoration(fixture));
        passed &= ReportScenario("authored MASK versus TestCoverage", TestAuthoredMaskDifference(fixture));
        passed &= ReportScenario("four-sample resolve average", TestResolveSampleAverage(fixture));
        passed &= ReportScenario("UNORM presentation conversion", TestPresentConversion(fixture));
        passed &= ReportScenario("transactional configuration and 20 transitions",
            TestConfigurationTransitions(fixture));
        passed &= ReportScenario("D3D11 debug messages", fixture.CheckDebugMessages());
        if (!passed)
            return 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }

    std::cout << "MSAA pipeline tests passed: 11 WARP pixel scenarios and "
        "1 control-flow probe.\n";
    return 0;
}
