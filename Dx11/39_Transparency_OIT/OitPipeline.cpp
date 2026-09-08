#include "OitPipeline.h"

#include <cstdio>
#include <string>
#include <utility>

#include <d3dcompiler.h>
#include <windows.h>

namespace
{
    using Microsoft::WRL::ComPtr;

    void ReportFailure(const wchar_t* operation, HRESULT result)
    {
        wchar_t message[256]{};
        static_cast<void>(swprintf_s(message, L"OitPipeline: %ls failed (HRESULT 0x%08X).\n",
            operation, static_cast<unsigned int>(result)));
        OutputDebugStringW(message);
    }

    bool SucceededOrReport(HRESULT result, const wchar_t* operation)
    {
        if (SUCCEEDED(result))
            return true;
        ReportFailure(operation, result);
        return false;
    }

    std::wstring ShaderPath(const std::wstring& directory)
    {
        if (directory.empty())
            return L"39_Resolve.hlsl";
        const wchar_t last = directory.back();
        return directory + ((last == L'\\' || last == L'/') ? L"" : L"\\") + L"39_Resolve.hlsl";
    }

    ComPtr<ID3DBlob> CompileShader(const std::wstring& path, const char* entryPoint, const char* target)
    {
        ComPtr<ID3DBlob> bytecode;
        ComPtr<ID3DBlob> errors;
        const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
        const HRESULT result = D3DCompileFromFile(path.c_str(), nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, target, flags, 0, &bytecode, &errors);
        if (FAILED(result))
        {
            ReportFailure(L"D3DCompileFromFile", result);
            if (errors)
            {
                std::string diagnostic(static_cast<const char*>(errors->GetBufferPointer()),
                    errors->GetBufferSize());
                diagnostic += '\n';
                OutputDebugStringA(diagnostic.c_str());
            }
            return {};
        }
        return bytecode;
    }

    bool CreateColorTarget(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT format,
        ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& target,
        ComPtr<ID3D11ShaderResourceView>& resource)
    {
        D3D11_TEXTURE2D_DESC description{};
        description.Width = width;
        description.Height = height;
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = format;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_DEFAULT;
        description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        return SucceededOrReport(device->CreateTexture2D(&description, nullptr, &texture),
                   L"CreateTexture2D") &&
            SucceededOrReport(device->CreateRenderTargetView(texture.Get(), nullptr, &target),
                L"CreateRenderTargetView") &&
            SucceededOrReport(device->CreateShaderResourceView(texture.Get(), nullptr, &resource),
                L"CreateShaderResourceView");
    }

    void UnbindPixelResources(ID3D11DeviceContext* context)
    {
        ID3D11ShaderResourceView* nullResources[3] = {};
        context->PSSetShaderResources(0, 3, nullResources);
    }
}

namespace Transparency39
{
    bool OitPipeline::Initialize(ID3D11Device* device, const std::wstring& shaderDirectory)
    {
        if (!device)
        {
            ReportFailure(L"Initialize(null device)", E_INVALIDARG);
            return false;
        }
        const std::wstring path = ShaderPath(shaderDirectory);
        const ComPtr<ID3DBlob> vertexBytecode = CompileShader(path, "VSFullscreen", "vs_5_0");
        const ComPtr<ID3DBlob> copyBytecode = CompileShader(path, "PSCopy", "ps_5_0");
        const ComPtr<ID3DBlob> resolveBytecode = CompileShader(path, "PSResolve", "ps_5_0");
        const ComPtr<ID3DBlob> compositeBytecode = CompileShader(path, "PSPresentComposite", "ps_5_0");
        const ComPtr<ID3DBlob> accumulationBytecode = CompileShader(path, "PSPresentAccumulation", "ps_5_0");
        const ComPtr<ID3DBlob> revealageBytecode = CompileShader(path, "PSPresentRevealage", "ps_5_0");
        if (!vertexBytecode || !copyBytecode || !resolveBytecode || !compositeBytecode ||
            !accumulationBytecode || !revealageBytecode)
            return false;

        ComPtr<ID3D11VertexShader> vertexShader;
        ComPtr<ID3D11PixelShader> copyShader;
        ComPtr<ID3D11PixelShader> resolveShader;
        ComPtr<ID3D11PixelShader> compositeShader;
        ComPtr<ID3D11PixelShader> accumulationShader;
        ComPtr<ID3D11PixelShader> revealageShader;
        if (!SucceededOrReport(device->CreateVertexShader(vertexBytecode->GetBufferPointer(),
                vertexBytecode->GetBufferSize(), nullptr, &vertexShader), L"CreateVertexShader") ||
            !SucceededOrReport(device->CreatePixelShader(copyBytecode->GetBufferPointer(),
                copyBytecode->GetBufferSize(), nullptr, &copyShader), L"CreatePixelShader(PSCopy)") ||
            !SucceededOrReport(device->CreatePixelShader(resolveBytecode->GetBufferPointer(),
                resolveBytecode->GetBufferSize(), nullptr, &resolveShader), L"CreatePixelShader(PSResolve)") ||
            !SucceededOrReport(device->CreatePixelShader(compositeBytecode->GetBufferPointer(),
                compositeBytecode->GetBufferSize(), nullptr, &compositeShader),
                L"CreatePixelShader(PSPresentComposite)"))
            return false;
        if (!SucceededOrReport(device->CreatePixelShader(accumulationBytecode->GetBufferPointer(),
                accumulationBytecode->GetBufferSize(), nullptr, &accumulationShader),
                L"CreatePixelShader(PSPresentAccumulation)") ||
            !SucceededOrReport(device->CreatePixelShader(revealageBytecode->GetBufferPointer(),
                revealageBytecode->GetBufferSize(), nullptr, &revealageShader),
                L"CreatePixelShader(PSPresentRevealage)"))
            return false;

        D3D11_BLEND_DESC opaqueDescription{};
        opaqueDescription.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        ComPtr<ID3D11BlendState> opaqueBlend;
        if (!SucceededOrReport(device->CreateBlendState(&opaqueDescription, &opaqueBlend),
            L"CreateBlendState(opaque)"))
            return false;

        D3D11_BLEND_DESC sortedDescription{};
        auto& sortedTarget = sortedDescription.RenderTarget[0];
        sortedTarget.BlendEnable = TRUE;
        sortedTarget.SrcBlend = D3D11_BLEND_SRC_ALPHA;
        sortedTarget.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        sortedTarget.BlendOp = D3D11_BLEND_OP_ADD;
        sortedTarget.SrcBlendAlpha = D3D11_BLEND_ONE;
        sortedTarget.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        sortedTarget.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        sortedTarget.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        ComPtr<ID3D11BlendState> sortedBlend;
        if (!SucceededOrReport(device->CreateBlendState(&sortedDescription, &sortedBlend),
            L"CreateBlendState(sorted)"))
            return false;

        D3D11_BLEND_DESC oitDescription{};
        oitDescription.IndependentBlendEnable = TRUE;
        auto& accumulationTarget = oitDescription.RenderTarget[0];
        accumulationTarget.BlendEnable = TRUE;
        accumulationTarget.SrcBlend = D3D11_BLEND_ONE;
        accumulationTarget.DestBlend = D3D11_BLEND_ONE;
        accumulationTarget.BlendOp = D3D11_BLEND_OP_ADD;
        accumulationTarget.SrcBlendAlpha = D3D11_BLEND_ONE;
        accumulationTarget.DestBlendAlpha = D3D11_BLEND_ONE;
        accumulationTarget.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        accumulationTarget.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        auto& revealageTarget = oitDescription.RenderTarget[1];
        revealageTarget.BlendEnable = TRUE;
        revealageTarget.SrcBlend = D3D11_BLEND_ZERO;
        revealageTarget.DestBlend = D3D11_BLEND_INV_SRC_COLOR;
        revealageTarget.BlendOp = D3D11_BLEND_OP_ADD;
        revealageTarget.SrcBlendAlpha = D3D11_BLEND_ZERO;
        revealageTarget.DestBlendAlpha = D3D11_BLEND_ONE;
        revealageTarget.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        revealageTarget.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED;
        ComPtr<ID3D11BlendState> oitBlend;
        if (!SucceededOrReport(device->CreateBlendState(&oitDescription, &oitBlend),
            L"CreateBlendState(OIT)"))
            return false;

        D3D11_DEPTH_STENCIL_DESC depthWriteDescription{};
        depthWriteDescription.DepthEnable = TRUE;
        depthWriteDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthWriteDescription.DepthFunc = D3D11_COMPARISON_LESS;
        ComPtr<ID3D11DepthStencilState> depthWrite;
        if (!SucceededOrReport(device->CreateDepthStencilState(&depthWriteDescription, &depthWrite),
            L"CreateDepthStencilState(write)"))
            return false;
        D3D11_DEPTH_STENCIL_DESC depthReadDescription = depthWriteDescription;
        depthReadDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        ComPtr<ID3D11DepthStencilState> depthRead;
        if (!SucceededOrReport(device->CreateDepthStencilState(&depthReadDescription, &depthRead),
            L"CreateDepthStencilState(read)"))
            return false;
        D3D11_DEPTH_STENCIL_DESC depthDisabledDescription{};
        depthDisabledDescription.DepthEnable = FALSE;
        depthDisabledDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depthDisabledDescription.DepthFunc = D3D11_COMPARISON_ALWAYS;
        ComPtr<ID3D11DepthStencilState> depthDisabled;
        if (!SucceededOrReport(device->CreateDepthStencilState(&depthDisabledDescription, &depthDisabled),
            L"CreateDepthStencilState(disabled)"))
            return false;

        D3D11_RASTERIZER_DESC rasterizerDescription{};
        rasterizerDescription.FillMode = D3D11_FILL_SOLID;
        rasterizerDescription.CullMode = D3D11_CULL_NONE;
        rasterizerDescription.DepthClipEnable = TRUE;
        ComPtr<ID3D11RasterizerState> rasterizer;
        if (!SucceededOrReport(device->CreateRasterizerState(&rasterizerDescription, &rasterizer),
            L"CreateRasterizerState"))
            return false;

        D3D11_BUFFER_DESC constantDescription{};
        constantDescription.ByteWidth = 16;
        constantDescription.Usage = D3D11_USAGE_DEFAULT;
        constantDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        ComPtr<ID3D11Buffer> presentConstants;
        if (!SucceededOrReport(device->CreateBuffer(&constantDescription, nullptr, &presentConstants),
            L"CreateBuffer(PresentConstants)"))
            return false;

        fullscreenVertexShader_ = std::move(vertexShader);
        copyPixelShader_ = std::move(copyShader);
        resolvePixelShader_ = std::move(resolveShader);
        presentCompositePixelShader_ = std::move(compositeShader);
        presentAccumulationPixelShader_ = std::move(accumulationShader);
        presentRevealagePixelShader_ = std::move(revealageShader);
        opaqueBlend_ = std::move(opaqueBlend);
        sortedBlend_ = std::move(sortedBlend);
        oitBlend_ = std::move(oitBlend);
        depthWrite_ = std::move(depthWrite);
        depthRead_ = std::move(depthRead);
        depthDisabled_ = std::move(depthDisabled);
        fullscreenRasterizer_ = std::move(rasterizer);
        presentConstants_ = std::move(presentConstants);

        timings_ = {};
        std::array<TimingSlot, kTimingSlotCount> querySlots{};
        bool queriesAvailable = true;
        for (TimingSlot& slot : querySlots)
        {
            D3D11_QUERY_DESC queryDescription{};
            queryDescription.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            if (FAILED(device->CreateQuery(&queryDescription, &slot.disjoint)))
            {
                queriesAvailable = false;
                break;
            }
            queryDescription.Query = D3D11_QUERY_TIMESTAMP;
            if (FAILED(device->CreateQuery(&queryDescription, &slot.transparentBegin)) ||
                FAILED(device->CreateQuery(&queryDescription, &slot.transparentEnd)) ||
                FAILED(device->CreateQuery(&queryDescription, &slot.compositeBegin)) ||
                FAILED(device->CreateQuery(&queryDescription, &slot.compositeEnd)))
            {
                queriesAvailable = false;
                break;
            }
        }
        if (queriesAvailable)
        {
            timingSlots_ = std::move(querySlots);
            timings_.available = true;
        }
        else
        {
            timingSlots_ = {};
            OutputDebugStringW(L"OitPipeline: GPU timestamp queries unavailable; rendering will continue.\n");
        }
        nextTimingSlot_ = 0;
        activeTimingSlot_ = -1;
        transparentTimingEnded_ = false;
        return true;
    }

    bool OitPipeline::Resize(ID3D11Device* device, UINT width, UINT height)
    {
        if (!device || width == 0 || height == 0)
        {
            ReportFailure(L"Resize(invalid dimensions or device)", E_INVALIDARG);
            return false;
        }
        ComPtr<ID3D11Texture2D> hdrTexture, accumulationTexture, revealageTexture, linearTexture;
        ComPtr<ID3D11RenderTargetView> hdrRtv, accumulationRtv, revealageRtv, linearRtv;
        ComPtr<ID3D11ShaderResourceView> hdrSrv, accumulationSrv, revealageSrv, linearSrv;
        if (!CreateColorTarget(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT,
                hdrTexture, hdrRtv, hdrSrv) ||
            !CreateColorTarget(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT,
                accumulationTexture, accumulationRtv, accumulationSrv) ||
            !CreateColorTarget(device, width, height, DXGI_FORMAT_R16_FLOAT,
                revealageTexture, revealageRtv, revealageSrv) ||
            !CreateColorTarget(device, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT,
                linearTexture, linearRtv, linearSrv))
            return false;

        D3D11_TEXTURE2D_DESC depthDescription{};
        depthDescription.Width = width;
        depthDescription.Height = height;
        depthDescription.MipLevels = 1;
        depthDescription.ArraySize = 1;
        depthDescription.Format = DXGI_FORMAT_R32_TYPELESS;
        depthDescription.SampleDesc.Count = 1;
        depthDescription.Usage = D3D11_USAGE_DEFAULT;
        depthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ComPtr<ID3D11Texture2D> depthTexture;
        if (!SucceededOrReport(device->CreateTexture2D(&depthDescription, nullptr, &depthTexture),
            L"CreateTexture2D(depth)"))
            return false;
        D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDescription{};
        depthViewDescription.Format = DXGI_FORMAT_D32_FLOAT;
        depthViewDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        ComPtr<ID3D11DepthStencilView> depthDsv;
        if (!SucceededOrReport(device->CreateDepthStencilView(depthTexture.Get(),
            &depthViewDescription, &depthDsv), L"CreateDepthStencilView"))
            return false;

        hdrTexture_ = std::move(hdrTexture); hdrRtv_ = std::move(hdrRtv); hdrSrv_ = std::move(hdrSrv);
        accumulationTexture_ = std::move(accumulationTexture);
        accumulationRtv_ = std::move(accumulationRtv); accumulationSrv_ = std::move(accumulationSrv);
        revealageTexture_ = std::move(revealageTexture);
        revealageRtv_ = std::move(revealageRtv); revealageSrv_ = std::move(revealageSrv);
        linearTexture_ = std::move(linearTexture); linearRtv_ = std::move(linearRtv);
        linearSrv_ = std::move(linearSrv); depthTexture_ = std::move(depthTexture);
        depthDsv_ = std::move(depthDsv);
        width_ = width;
        height_ = height;
        return true;
    }

    void OitPipeline::PollTimings(ID3D11DeviceContext* context)
    {
        if (!context || !timings_.available)
            return;
        constexpr UINT flags = D3D11_ASYNC_GETDATA_DONOTFLUSH;
        for (TimingSlot& slot : timingSlots_)
        {
            if (!slot.inFlight)
                continue;
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
            if (context->GetData(slot.disjoint.Get(), &disjoint, sizeof(disjoint), flags) != S_OK)
                continue;
            UINT64 transparentBegin{}, transparentEnd{}, compositeBegin{}, compositeEnd{};
            if (context->GetData(slot.transparentBegin.Get(), &transparentBegin,
                    sizeof(transparentBegin), flags) != S_OK ||
                context->GetData(slot.transparentEnd.Get(), &transparentEnd,
                    sizeof(transparentEnd), flags) != S_OK ||
                context->GetData(slot.compositeBegin.Get(), &compositeBegin,
                    sizeof(compositeBegin), flags) != S_OK ||
                context->GetData(slot.compositeEnd.Get(), &compositeEnd,
                    sizeof(compositeEnd), flags) != S_OK)
                continue;
            slot.inFlight = false;
            if (disjoint.Disjoint || disjoint.Frequency == 0 || transparentEnd < transparentBegin ||
                compositeEnd < compositeBegin)
            {
                timings_.valid = false;
                continue;
            }
            const double millisecondsPerTick = 1000.0 / static_cast<double>(disjoint.Frequency);
            timings_.transparentMs = static_cast<double>(transparentEnd - transparentBegin) * millisecondsPerTick;
            timings_.compositeMs = static_cast<double>(compositeEnd - compositeBegin) * millisecondsPerTick;
            timings_.valid = true;
        }
    }

    void OitPipeline::BeginTiming(ID3D11DeviceContext* context)
    {
        if (activeTimingSlot_ >= 0 || !context || !timings_.available)
            return;
        transparentTimingEnded_ = false;
        for (std::size_t offset = 0; offset < kTimingSlotCount; ++offset)
        {
            const std::size_t index = (nextTimingSlot_ + offset) % kTimingSlotCount;
            TimingSlot& slot = timingSlots_[index];
            if (slot.inFlight)
                continue;
            context->Begin(slot.disjoint.Get());
            context->End(slot.transparentBegin.Get());
            activeTimingSlot_ = static_cast<int>(index);
            return;
        }
    }

    void OitPipeline::BeginOpaque(ID3D11DeviceContext* context, const float clearColor[4])
    {
        if (!context || !clearColor || !hdrRtv_ || !depthDsv_)
            return;
        PollTimings(context);
        UnbindPixelResources(context);
        ID3D11RenderTargetView* target = hdrRtv_.Get();
        context->OMSetRenderTargets(1, &target, depthDsv_.Get());
        context->OMSetBlendState(opaqueBlend_.Get(), nullptr, 0xffffffffU);
        context->OMSetDepthStencilState(depthWrite_.Get(), 0);
        const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width_),
            static_cast<float>(height_), 0.0f, 1.0f };
        context->RSSetViewports(1, &viewport);
        context->ClearRenderTargetView(hdrRtv_.Get(), clearColor);
        context->ClearDepthStencilView(depthDsv_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    }

    void OitPipeline::BeginTransparency(ID3D11DeviceContext* context, Mode mode)
    {
        if (!context || !hdrRtv_ || !accumulationRtv_ || !revealageRtv_ || !depthDsv_)
            return;
        mode_ = mode;
        const float clearAccumulation[4] = {};
        const float clearRevealage[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        context->ClearRenderTargetView(accumulationRtv_.Get(), clearAccumulation);
        context->ClearRenderTargetView(revealageRtv_.Get(), clearRevealage);
        const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width_),
            static_cast<float>(height_), 0.0f, 1.0f };
        context->RSSetViewports(1, &viewport);
        if (mode == Mode::WeightedOit)
        {
            ID3D11RenderTargetView* targets[2] = { accumulationRtv_.Get(), revealageRtv_.Get() };
            context->OMSetRenderTargets(2, targets, depthDsv_.Get());
            context->OMSetBlendState(oitBlend_.Get(), nullptr, 0xffffffffU);
            context->OMSetDepthStencilState(depthRead_.Get(), 0);
        }
        else
        {
            ID3D11RenderTargetView* target = hdrRtv_.Get();
            context->OMSetRenderTargets(1, &target, depthDsv_.Get());
            context->OMSetBlendState(
                mode == Mode::SortedBlend ? sortedBlend_.Get() : opaqueBlend_.Get(), nullptr, 0xffffffffU);
            context->OMSetDepthStencilState(
                mode == Mode::SortedBlend ? depthRead_.Get() : depthWrite_.Get(), 0);
        }
        BeginTiming(context);
    }

    void OitPipeline::EndTransparency(ID3D11DeviceContext* context)
    {
        if (!context || activeTimingSlot_ < 0 || transparentTimingEnded_)
            return;
        context->End(timingSlots_[static_cast<std::size_t>(activeTimingSlot_)].transparentEnd.Get());
        transparentTimingEnded_ = true;
    }

    void OitPipeline::DrawFullscreen(ID3D11DeviceContext* context,
        ID3D11RenderTargetView* target, ID3D11PixelShader* pixelShader)
    {
        context->OMSetRenderTargets(1, &target, nullptr);
        context->OMSetBlendState(opaqueBlend_.Get(), nullptr, 0xffffffffU);
        context->OMSetDepthStencilState(depthDisabled_.Get(), 0);
        const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, static_cast<float>(width_),
            static_cast<float>(height_), 0.0f, 1.0f };
        context->RSSetViewports(1, &viewport);
        context->RSSetState(fullscreenRasterizer_.Get());
        ID3D11Buffer* nullBuffer = nullptr;
        const UINT zero = 0;
        context->IASetInputLayout(nullptr);
        context->IASetVertexBuffers(0, 1, &nullBuffer, &zero, &zero);
        context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(fullscreenVertexShader_.Get(), nullptr, 0);
        context->PSSetShader(pixelShader, nullptr, 0);
        context->Draw(3, 0);
    }

    void OitPipeline::Resolve(ID3D11DeviceContext* context)
    {
        if (!context || !linearRtv_)
            return;
        EndTransparency(context);
        TimingSlot* timingSlot = activeTimingSlot_ >= 0 ?
            &timingSlots_[static_cast<std::size_t>(activeTimingSlot_)] : nullptr;
        if (timingSlot)
            context->End(timingSlot->compositeBegin.Get());
        ID3D11RenderTargetView* resolveTarget = linearRtv_.Get();
        context->OMSetRenderTargets(1, &resolveTarget, nullptr);
        ID3D11ShaderResourceView* resources[3] = {
            hdrSrv_.Get(), accumulationSrv_.Get(), revealageSrv_.Get()
        };
        context->PSSetShaderResources(0, 3, resources);
        DrawFullscreen(context, linearRtv_.Get(),
            mode_ == Mode::WeightedOit ? resolvePixelShader_.Get() : copyPixelShader_.Get());
        UnbindPixelResources(context);
        if (timingSlot)
        {
            context->End(timingSlot->compositeEnd.Get());
            context->End(timingSlot->disjoint.Get());
            timingSlot->inFlight = true;
            nextTimingSlot_ = (static_cast<std::size_t>(activeTimingSlot_) + 1) % kTimingSlotCount;
            activeTimingSlot_ = -1;
            transparentTimingEnded_ = false;
        }
    }

    void OitPipeline::Present(ID3D11DeviceContext* context, ID3D11RenderTargetView* output,
        float exposure, DebugView debugView)
    {
        if (!context || !output || !linearSrv_)
            return;
        const float constants[4] = { exposure, 0.0f, 0.0f, 0.0f };
        context->UpdateSubresource(presentConstants_.Get(), 0, nullptr, constants, 0, 0);
        ID3D11Buffer* constantBuffer = presentConstants_.Get();
        context->PSSetConstantBuffers(0, 1, &constantBuffer);
        context->OMSetRenderTargets(1, &output, nullptr);
        ID3D11ShaderResourceView* resources[3] = {
            linearSrv_.Get(), accumulationSrv_.Get(), revealageSrv_.Get()
        };
        context->PSSetShaderResources(0, 3, resources);
        ID3D11PixelShader* shader = presentCompositePixelShader_.Get();
        if (debugView == DebugView::Accumulation)
            shader = presentAccumulationPixelShader_.Get();
        else if (debugView == DebugView::Revealage)
            shader = presentRevealagePixelShader_.Get();
        DrawFullscreen(context, output, shader);
        UnbindPixelResources(context);
    }

    ID3D11Texture2D* OitPipeline::LinearTexture() const { return linearTexture_.Get(); }
    ID3D11Texture2D* OitPipeline::DepthTexture() const { return depthTexture_.Get(); }
    ID3D11ShaderResourceView* OitPipeline::AccumulationSRV() const { return accumulationSrv_.Get(); }
    ID3D11ShaderResourceView* OitPipeline::RevealageSRV() const { return revealageSrv_.Get(); }
    const GpuTimings& OitPipeline::Timings() const { return timings_; }
}
