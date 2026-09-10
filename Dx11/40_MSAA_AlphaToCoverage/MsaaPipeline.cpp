#include "MsaaPipeline.h"

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <utility>

#include <d3dcompiler.h>

namespace Coverage40
{
namespace
{
using Microsoft::WRL::ComPtr;

constexpr DXGI_FORMAT kColorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;
constexpr UINT kFullSampleMask = 0xffffffff;

std::wstring QueryDescription(const wchar_t* operation, DXGI_FORMAT format,
    UINT count, HRESULT result, UINT value)
{
    std::wostringstream stream;
    stream << operation << L"(format=" << static_cast<UINT>(format)
        << L", count=" << count << L") HRESULT=0x" << std::hex
        << std::uppercase << static_cast<unsigned long>(result) << std::dec
        << L", value=" << value;
    return stream.str();
}

std::wstring OperationDescription(const wchar_t* operation, DXGI_FORMAT format,
    UINT count, HRESULT result)
{
    std::wostringstream stream;
    stream << operation << L"(format=" << static_cast<UINT>(format)
        << L", count=" << count << L") HRESULT=0x" << std::hex
        << std::uppercase << static_cast<unsigned long>(result);
    return stream.str();
}

bool HasAll(UINT value, UINT required)
{
    return (value & required) == required;
}

std::wstring WidenDiagnostics(const void* data, SIZE_T size)
{
    const auto* bytes = static_cast<const char*>(data);
    const int byteCount = static_cast<int>(size);
    const int characterCount = MultiByteToWideChar(CP_UTF8, 0, bytes, byteCount,
        nullptr, 0);
    if (characterCount <= 0)
        return L"compiler diagnostics unavailable";
    std::wstring result(static_cast<size_t>(characterCount), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, bytes, byteCount, result.data(), characterCount);
    return result;
}

bool CompileShader(const std::filesystem::path& path, const char* entryPoint,
    const char* target, ComPtr<ID3DBlob>& bytecode, std::wstring& reason)
{
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
    const HRESULT result = D3DCompileFromFile(path.c_str(), nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, target, flags, 0,
        &bytecode, &errors);
    if (SUCCEEDED(result))
        return true;

    std::wostringstream stream;
    stream << L"D3DCompileFromFile(" << path.wstring() << L", " << entryPoint
        << L", " << target << L") HRESULT=0x" << std::hex << std::uppercase
        << static_cast<unsigned long>(result);
    if (errors)
        stream << L"; " << WidenDiagnostics(errors->GetBufferPointer(),
            errors->GetBufferSize());
    reason = stream.str();
    return false;
}

void ReportRuntimeFailure(const std::wstring& reason)
{
    const std::wstring line = L"Coverage40: " + reason + L"\n";
    OutputDebugStringW(line.c_str());
}
}

bool MsaaPipeline::Initialize(ID3D11Device* device, const std::wstring& shaderDirectory)
{
    initialized_ = false;
    support_ = {};
    timings_ = {};
    timingSlots_ = {};
    activeTimingSlot_ = kNoTimingSlot;
    timingGeneration_ = 0;
    if (!device)
    {
        support_.reason = L"Initialize(device=null)";
        return false;
    }

    UINT colorSupport{};
    UINT depthSupport{};
    const HRESULT colorSupportResult = device->CheckFormatSupport(kColorFormat, &colorSupport);
    const HRESULT depthSupportResult = device->CheckFormatSupport(kDepthFormat, &depthSupport);
    const UINT requiredColor = D3D11_FORMAT_SUPPORT_TEXTURE2D |
        D3D11_FORMAT_SUPPORT_RENDER_TARGET |
        D3D11_FORMAT_SUPPORT_SHADER_SAMPLE |
        D3D11_FORMAT_SUPPORT_SHADER_LOAD |
        D3D11_FORMAT_SUPPORT_BLENDABLE;
    const UINT requiredDepth = D3D11_FORMAT_SUPPORT_TEXTURE2D |
        D3D11_FORMAT_SUPPORT_DEPTH_STENCIL;
    if (FAILED(colorSupportResult) || !HasAll(colorSupport, requiredColor))
    {
        support_.reason = QueryDescription(L"CheckFormatSupport", kColorFormat, 1,
            colorSupportResult, colorSupport) + L"; required=" +
            std::to_wstring(requiredColor);
        return false;
    }
    if (FAILED(depthSupportResult) || !HasAll(depthSupport, requiredDepth))
    {
        support_.reason = QueryDescription(L"CheckFormatSupport", kDepthFormat, 1,
            depthSupportResult, depthSupport) + L"; required=" +
            std::to_wstring(requiredDepth);
        return false;
    }

    UINT colorLevels{};
    UINT depthLevels{};
    const HRESULT colorQualityResult = device->CheckMultisampleQualityLevels(
        kColorFormat, 4, &colorLevels);
    const HRESULT depthQualityResult = device->CheckMultisampleQualityLevels(
        kDepthFormat, 4, &depthLevels);
    const UINT requiredMultisampleColor = D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET |
        D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE;
    const bool multisampleFormats = HasAll(colorSupport, requiredMultisampleColor);
    support_.supports4x = Supports4x(SUCCEEDED(colorQualityResult), colorLevels,
        SUCCEEDED(depthQualityResult), depthLevels, multisampleFormats);
    if (!support_.supports4x)
    {
        support_.reason = QueryDescription(L"CheckMultisampleQualityLevels",
            kColorFormat, 4, colorQualityResult, colorLevels) + L"; " +
            QueryDescription(L"CheckMultisampleQualityLevels", kDepthFormat, 4,
                depthQualityResult, depthLevels) + L"; colorFormatSupport=" +
            std::to_wstring(colorSupport) + L", requiredMultisample=" +
            std::to_wstring(requiredMultisampleColor);
    }

    ComPtr<ID3DBlob> vertexBytecode;
    ComPtr<ID3DBlob> pixelBytecode;
    const std::filesystem::path presentPath =
        std::filesystem::path(shaderDirectory) / L"40_Present.hlsl";
    if (!CompileShader(presentPath, "VSFullscreen", "vs_5_0", vertexBytecode,
            support_.reason) ||
        !CompileShader(presentPath, "PSPresent", "ps_5_0", pixelBytecode,
            support_.reason))
        return false;

    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    HRESULT result = device->CreateVertexShader(vertexBytecode->GetBufferPointer(),
        vertexBytecode->GetBufferSize(), nullptr, &vertexShader);
    if (FAILED(result))
    {
        support_.reason = OperationDescription(L"CreateVertexShader", kColorFormat, 1, result);
        return false;
    }
    result = device->CreatePixelShader(pixelBytecode->GetBufferPointer(),
        pixelBytecode->GetBufferSize(), nullptr, &pixelShader);
    if (FAILED(result))
    {
        support_.reason = OperationDescription(L"CreatePixelShader", kColorFormat, 1, result);
        return false;
    }

    D3D11_BLEND_DESC opaqueDescription{};
    opaqueDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    opaqueDescription.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    opaqueDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    opaqueDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    opaqueDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    opaqueDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    opaqueDescription.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ComPtr<ID3D11BlendState> opaqueBlend;
    result = device->CreateBlendState(&opaqueDescription, &opaqueBlend);
    if (FAILED(result))
    {
        support_.reason = OperationDescription(L"CreateBlendState(opaque)", kColorFormat, 1, result);
        return false;
    }

    D3D11_BLEND_DESC a2cDescription = opaqueDescription;
    a2cDescription.AlphaToCoverageEnable = TRUE;
    ComPtr<ID3D11BlendState> a2cBlend;
    result = device->CreateBlendState(&a2cDescription, &a2cBlend);
    if (FAILED(result))
    {
        support_.reason = OperationDescription(L"CreateBlendState(A2C)", kColorFormat, 4, result);
        return false;
    }

    D3D11_BLEND_DESC blendDescription = opaqueDescription;
    blendDescription.RenderTarget[0].BlendEnable = TRUE;
    blendDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDescription.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    ComPtr<ID3D11BlendState> alphaBlend;
    result = device->CreateBlendState(&blendDescription, &alphaBlend);
    if (FAILED(result))
    {
        support_.reason = OperationDescription(L"CreateBlendState(alpha)", kColorFormat, 1, result);
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depthWriteDescription{};
    depthWriteDescription.DepthEnable = TRUE;
    depthWriteDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthWriteDescription.DepthFunc = D3D11_COMPARISON_LESS;
    ComPtr<ID3D11DepthStencilState> depthWrite;
    result = device->CreateDepthStencilState(&depthWriteDescription, &depthWrite);
    if (FAILED(result))
    {
        support_.reason = OperationDescription(L"CreateDepthStencilState(write)", kDepthFormat, 1, result);
        return false;
    }
    D3D11_DEPTH_STENCIL_DESC depthReadDescription = depthWriteDescription;
    depthReadDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    ComPtr<ID3D11DepthStencilState> depthRead;
    result = device->CreateDepthStencilState(&depthReadDescription, &depthRead);
    if (FAILED(result))
    {
        support_.reason = OperationDescription(L"CreateDepthStencilState(read)", kDepthFormat, 1, result);
        return false;
    }
    D3D11_DEPTH_STENCIL_DESC depthDisabledDescription{};
    depthDisabledDescription.DepthEnable = FALSE;
    depthDisabledDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDisabledDescription.DepthFunc = D3D11_COMPARISON_ALWAYS;
    ComPtr<ID3D11DepthStencilState> depthDisabled;
    result = device->CreateDepthStencilState(&depthDisabledDescription, &depthDisabled);
    if (FAILED(result))
    {
        support_.reason = OperationDescription(L"CreateDepthStencilState(disabled)", kDepthFormat, 1, result);
        return false;
    }

    D3D11_RASTERIZER_DESC rasterDescription{};
    rasterDescription.FillMode = D3D11_FILL_SOLID;
    rasterDescription.CullMode = D3D11_CULL_NONE;
    rasterDescription.DepthClipEnable = TRUE;
    ComPtr<ID3D11RasterizerState> rasterizer;
    result = device->CreateRasterizerState(&rasterDescription, &rasterizer);
    if (FAILED(result))
    {
        support_.reason = OperationDescription(L"CreateRasterizerState(present)", kColorFormat, 1, result);
        return false;
    }

    D3D11_BUFFER_DESC constantDescription{};
    constantDescription.ByteWidth = 16;
    constantDescription.Usage = D3D11_USAGE_DEFAULT;
    constantDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> constants;
    result = device->CreateBuffer(&constantDescription, nullptr, &constants);
    if (FAILED(result))
    {
        support_.reason = OperationDescription(L"CreateBuffer(present constants)", kColorFormat, 1, result);
        return false;
    }

    presentVertexShader_ = std::move(vertexShader);
    presentPixelShader_ = std::move(pixelShader);
    opaqueBlend_ = std::move(opaqueBlend);
    alphaToCoverageBlend_ = std::move(a2cBlend);
    alphaBlend_ = std::move(alphaBlend);
    depthWrite_ = std::move(depthWrite);
    depthReadOnly_ = std::move(depthRead);
    depthDisabled_ = std::move(depthDisabled);
    presentRasterizer_ = std::move(rasterizer);
    presentConstants_ = std::move(constants);
    InitializeTimingQueries(device);
    initialized_ = true;
    return true;
}

bool MsaaPipeline::Configure(ID3D11Device* device, UINT width, UINT height, Mode mode)
{
    const UINT samples = SampleCount(mode);
    if (!initialized_ || !device || TargetBytes(width, height, samples) == 0 ||
        (samples == 4 && !support_.supports4x))
        return false;

    const bool complete = colorTexture_ && depthTexture_ && colorTarget_ &&
        depthTarget_ && linearView_ && (samples == 1 || resolvedTexture_);
    if (complete && width == width_ && height == height_ && samples == Samples())
    {
        mode_ = mode;
        UpdateTimingConfiguration(mode, width, height);
        return true;
    }

    D3D11_TEXTURE2D_DESC colorDescription{};
    colorDescription.Width = width;
    colorDescription.Height = height;
    colorDescription.MipLevels = 1;
    colorDescription.ArraySize = 1;
    colorDescription.Format = kColorFormat;
    colorDescription.SampleDesc = { samples, 0 };
    colorDescription.Usage = D3D11_USAGE_DEFAULT;
    colorDescription.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (samples == 1)
        colorDescription.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> color;
    ComPtr<ID3D11RenderTargetView> colorTarget;
    HRESULT result = device->CreateTexture2D(&colorDescription, nullptr, &color);
    if (FAILED(result))
    {
        ReportRuntimeFailure(OperationDescription(L"CreateTexture2D(color)",
            kColorFormat, samples, result));
        return false;
    }
    result = device->CreateRenderTargetView(color.Get(), nullptr, &colorTarget);
    if (FAILED(result))
    {
        ReportRuntimeFailure(OperationDescription(L"CreateRenderTargetView(color)",
            kColorFormat, samples, result));
        return false;
    }

    D3D11_TEXTURE2D_DESC depthDescription = colorDescription;
    depthDescription.Format = kDepthFormat;
    depthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth;
    ComPtr<ID3D11DepthStencilView> depthTarget;
    result = device->CreateTexture2D(&depthDescription, nullptr, &depth);
    if (FAILED(result))
    {
        ReportRuntimeFailure(OperationDescription(L"CreateTexture2D(depth)",
            kDepthFormat, samples, result));
        return false;
    }
    result = device->CreateDepthStencilView(depth.Get(), nullptr, &depthTarget);
    if (FAILED(result))
    {
        ReportRuntimeFailure(OperationDescription(L"CreateDepthStencilView(depth)",
            kDepthFormat, samples, result));
        return false;
    }

    ComPtr<ID3D11Texture2D> resolved;
    ComPtr<ID3D11ShaderResourceView> linearView;
    if (samples == 1)
    {
        result = device->CreateShaderResourceView(color.Get(), nullptr, &linearView);
        if (FAILED(result))
        {
            ReportRuntimeFailure(OperationDescription(L"CreateShaderResourceView(color)",
                kColorFormat, samples, result));
            return false;
        }
    }
    else
    {
        D3D11_TEXTURE2D_DESC resolvedDescription = colorDescription;
        resolvedDescription.SampleDesc = { 1, 0 };
        resolvedDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        result = device->CreateTexture2D(&resolvedDescription, nullptr, &resolved);
        if (FAILED(result))
        {
            ReportRuntimeFailure(OperationDescription(L"CreateTexture2D(resolve)",
                kColorFormat, 1, result));
            return false;
        }
        result = device->CreateShaderResourceView(resolved.Get(), nullptr, &linearView);
        if (FAILED(result))
        {
            ReportRuntimeFailure(OperationDescription(L"CreateShaderResourceView(resolve)",
                kColorFormat, 1, result));
            return false;
        }
    }

    colorTexture_ = std::move(color);
    depthTexture_ = std::move(depth);
    resolvedTexture_ = std::move(resolved);
    colorTarget_ = std::move(colorTarget);
    depthTarget_ = std::move(depthTarget);
    linearView_ = std::move(linearView);
    width_ = width;
    height_ = height;
    mode_ = mode;
    UpdateTimingConfiguration(mode, width, height);
    return true;
}

void MsaaPipeline::BeginFrame(ID3D11DeviceContext* context, const float clearColor[4])
{
    if (!context || !colorTarget_ || !depthTarget_)
        return;

    PollTimings(context);
    TimingSlot* timingSlot = nullptr;
    if (timings_.available && activeTimingSlot_ == kNoTimingSlot)
    {
        for (std::size_t index = 0; index < timingSlots_.size(); ++index)
        {
            TimingSlot& slot = timingSlots_[index];
            if (slot.inFlight)
                continue;

            slot.inFlight = true;
            slot.sceneEnded = false;
            slot.generation = timingGeneration_;
            slot.mode = mode_;
            slot.width = width_;
            slot.height = height_;
            slot.resolveApplicable = Samples() == 4;
            activeTimingSlot_ = index;
            timingSlot = &slot;
            break;
        }
    }

    ID3D11RenderTargetView* target = colorTarget_.Get();
    context->OMSetRenderTargets(1, &target, depthTarget_.Get());
    if (timingSlot)
    {
        context->Begin(timingSlot->disjoint.Get());
        context->End(timingSlot->sceneBegin.Get());
    }
    context->ClearRenderTargetView(target, clearColor);
    context->ClearDepthStencilView(depthTarget_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width_),
        static_cast<float>(height_), 0.0f, 1.0f };
    context->RSSetViewports(1, &viewport);
}

void MsaaPipeline::BeginSurface(ID3D11DeviceContext* context, SurfaceRule rule)
{
    if (!context)
        return;
    ID3D11BlendState* blend = opaqueBlend_.Get();
    ID3D11DepthStencilState* depth = depthWrite_.Get();
    if (rule == SurfaceRule::AlphaToCoverage)
        blend = alphaToCoverageBlend_.Get();
    else if (rule == SurfaceRule::Blend)
    {
        blend = alphaBlend_.Get();
        depth = depthReadOnly_.Get();
    }
    context->OMSetBlendState(blend, nullptr, kFullSampleMask);
    context->OMSetDepthStencilState(depth, 0);
}

void MsaaPipeline::EndScene(ID3D11DeviceContext* context)
{
    if (!context || activeTimingSlot_ == kNoTimingSlot)
        return;

    TimingSlot& slot = timingSlots_[activeTimingSlot_];
    if (!slot.sceneEnded)
    {
        context->End(slot.sceneEnd.Get());
        slot.sceneEnded = true;
    }
}

void MsaaPipeline::Resolve(ID3D11DeviceContext* context)
{
    if (!context)
        return;
    context->OMSetRenderTargets(0, nullptr, nullptr);
    if (Samples() == 4 && resolvedTexture_ && colorTexture_)
    {
        if (activeTimingSlot_ != kNoTimingSlot)
            context->End(timingSlots_[activeTimingSlot_].resolveBegin.Get());
        context->ResolveSubresource(resolvedTexture_.Get(), 0, colorTexture_.Get(), 0,
            kColorFormat);
        if (activeTimingSlot_ != kNoTimingSlot)
            context->End(timingSlots_[activeTimingSlot_].resolveEnd.Get());
    }

    if (activeTimingSlot_ != kNoTimingSlot)
    {
        TimingSlot& slot = timingSlots_[activeTimingSlot_];
        if (!slot.sceneEnded)
        {
            context->End(slot.sceneEnd.Get());
            slot.sceneEnded = true;
        }
        context->End(slot.disjoint.Get());
        activeTimingSlot_ = kNoTimingSlot;
    }
}

void MsaaPipeline::Present(ID3D11DeviceContext* context,
    ID3D11RenderTargetView* output, float exposure)
{
    if (!context || !output || !linearView_)
        return;
    ID3D11RenderTargetView* target = output;
    context->OMSetRenderTargets(1, &target, nullptr);
    context->OMSetBlendState(opaqueBlend_.Get(), nullptr, kFullSampleMask);
    context->OMSetDepthStencilState(depthDisabled_.Get(), 0);
    const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width_),
        static_cast<float>(height_), 0.0f, 1.0f };
    context->RSSetViewports(1, &viewport);
    context->RSSetState(presentRasterizer_.Get());
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(presentVertexShader_.Get(), nullptr, 0);
    context->PSSetShader(presentPixelShader_.Get(), nullptr, 0);
    const float constants[4] = { exposure, 0.0f, 0.0f, 0.0f };
    context->UpdateSubresource(presentConstants_.Get(), 0, nullptr, constants, 0, 0);
    ID3D11Buffer* constantBuffer = presentConstants_.Get();
    context->PSSetConstantBuffers(0, 1, &constantBuffer);
    ID3D11ShaderResourceView* resource = linearView_.Get();
    context->PSSetShaderResources(0, 1, &resource);
    context->Draw(3, 0);
    ID3D11ShaderResourceView* nullResource = nullptr;
    context->PSSetShaderResources(0, 1, &nullResource);
}

ID3D11Texture2D* MsaaPipeline::LinearTexture() const
{
    return Samples() == 1 ? colorTexture_.Get() : resolvedTexture_.Get();
}

void MsaaPipeline::InitializeTimingQueries(ID3D11Device* device)
{
    std::array<TimingSlot, kTimingSlotCount> slots{};
    D3D11_QUERY_DESC disjointDescription{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
    D3D11_QUERY_DESC timestampDescription{ D3D11_QUERY_TIMESTAMP, 0 };
    for (TimingSlot& slot : slots)
    {
        if (FAILED(device->CreateQuery(&disjointDescription, &slot.disjoint)) ||
            FAILED(device->CreateQuery(&timestampDescription, &slot.sceneBegin)) ||
            FAILED(device->CreateQuery(&timestampDescription, &slot.sceneEnd)) ||
            FAILED(device->CreateQuery(&timestampDescription, &slot.resolveBegin)) ||
            FAILED(device->CreateQuery(&timestampDescription, &slot.resolveEnd)))
        {
            timings_.available = false;
            return;
        }
    }

    timingSlots_ = std::move(slots);
    timings_.available = true;
}

void MsaaPipeline::DisableTimings()
{
    timings_.available = false;
    timings_.valid = false;
    timings_.sceneMs = 0.0;
    timings_.resolveMs = 0.0;
    timingSlots_ = {};
    activeTimingSlot_ = kNoTimingSlot;
}

void MsaaPipeline::PollTimings(ID3D11DeviceContext* context)
{
    if (!timings_.available)
        return;

    for (std::size_t index = 0; index < timingSlots_.size(); ++index)
    {
        TimingSlot& slot = timingSlots_[index];
        if (!slot.inFlight || index == activeTimingSlot_)
            continue;

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
        const HRESULT disjointStatus = context->GetData(slot.disjoint.Get(),
            &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (disjointStatus == S_FALSE)
            continue;
        if (FAILED(disjointStatus))
        {
            DisableTimings();
            return;
        }

        UINT64 sceneBegin{};
        const HRESULT sceneBeginStatus = context->GetData(slot.sceneBegin.Get(),
            &sceneBegin, sizeof(sceneBegin), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (sceneBeginStatus == S_FALSE)
            continue;
        if (FAILED(sceneBeginStatus))
        {
            DisableTimings();
            return;
        }

        UINT64 sceneEnd{};
        const HRESULT sceneEndStatus = context->GetData(slot.sceneEnd.Get(),
            &sceneEnd, sizeof(sceneEnd), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (sceneEndStatus == S_FALSE)
            continue;
        if (FAILED(sceneEndStatus))
        {
            DisableTimings();
            return;
        }

        UINT64 resolveBegin{};
        UINT64 resolveEnd{};
        if (slot.resolveApplicable)
        {
            const HRESULT resolveBeginStatus = context->GetData(slot.resolveBegin.Get(),
                &resolveBegin, sizeof(resolveBegin), D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (resolveBeginStatus == S_FALSE)
                continue;
            if (FAILED(resolveBeginStatus))
            {
                DisableTimings();
                return;
            }

            const HRESULT resolveEndStatus = context->GetData(slot.resolveEnd.Get(),
                &resolveEnd, sizeof(resolveEnd), D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (resolveEndStatus == S_FALSE)
                continue;
            if (FAILED(resolveEndStatus))
            {
                DisableTimings();
                return;
            }
        }

        slot.inFlight = false;
        if (slot.generation != timingGeneration_ || disjoint.Disjoint ||
            disjoint.Frequency == 0 || sceneEnd < sceneBegin ||
            (slot.resolveApplicable && resolveEnd < resolveBegin))
            continue;

        const double millisecondsPerTick = 1000.0 /
            static_cast<double>(disjoint.Frequency);
        timings_.available = true;
        timings_.valid = true;
        timings_.resolveApplicable = slot.resolveApplicable;
        timings_.mode = slot.mode;
        timings_.width = slot.width;
        timings_.height = slot.height;
        timings_.sceneMs = static_cast<double>(sceneEnd - sceneBegin) *
            millisecondsPerTick;
        timings_.resolveMs = slot.resolveApplicable ?
            static_cast<double>(resolveEnd - resolveBegin) * millisecondsPerTick : 0.0;
    }
}

void MsaaPipeline::UpdateTimingConfiguration(Mode mode, UINT width, UINT height)
{
    ++timingGeneration_;
    const bool available = timings_.available;
    timings_ = { available, false, SampleCount(mode) == 4,
        mode, width, height, 0.0, 0.0 };
}
}
