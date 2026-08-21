#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "App.h"

#include "../Common/AssetManager.h"
#include "../Common/Mesh/FbxModel.h"
#include "../Common/ReadmeCapture.h"
#include "../Common/Vertex.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <assimp/GltfMaterial.h>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cwchar>
#include <string>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
    using MaterialProfile = App::MaterialProfile;

    // The only approved runtime model is "..\Resource\fbx\Public\MyAlice\Player\SampleModel.glb".
    constexpr wchar_t kCharacterPath[] = L".." L"\\Resource\\fbx\\Public\\MyAlice\\Player\\SampleModel.glb";
    constexpr double kCapturePoseTimeSeconds = 0.5;
    constexpr UINT kShadowMapSize = 2048;
    constexpr float kHeroScale = 80.0f;
    constexpr float kHeroYawRadians = -0.31415927f;
    constexpr float kBlendCoverageCutoff = 0.02f;

    struct MaterialOverride
    {
        uint32_t materialIndex;
        App::MaterialProfile profile;
    };

    constexpr std::array<MaterialOverride, 13> kSampleModelMaterialOverrides = {{
        { 0, MaterialProfile::Skin },
        { 1, MaterialProfile::Cloth },
        { 2, MaterialProfile::Hair },
        { 3, MaterialProfile::Cloth },
        { 4, MaterialProfile::Cloth },
        { 5, MaterialProfile::Skin },
        { 6, MaterialProfile::Skin },
        { 7, MaterialProfile::Skin },
        { 8, MaterialProfile::Skin },
        { 9, MaterialProfile::Skin },
        { 10, MaterialProfile::Skin },
        { 11, MaterialProfile::Hair },
        { 12, MaterialProfile::Hair },
    }};

    bool CompileShader(
        const wchar_t* fileName,
        const char* entryPoint,
        const char* profile,
        ComPtr<ID3DBlob>& shaderBlob)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ComPtr<ID3DBlob> errorBlob;
        const HRESULT result = D3DCompileFromFile(
            fileName,
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint,
            profile,
            flags,
            0,
            shaderBlob.ReleaseAndGetAddressOf(),
            errorBlob.GetAddressOf());
        if (FAILED(result))
        {
            if (errorBlob)
                OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
            return false;
        }
        return true;
    }

    template <typename T>
    bool UpdateDynamicBuffer(ID3D11DeviceContext* context, ID3D11Buffer* buffer, const T& value)
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            return false;
        *static_cast<T*>(mapped.pData) = value;
        context->Unmap(buffer, 0);
        return true;
    }
}

App::App()
{
    wcscpy_s(m_szTitle, L"Stylized Toon PBR");
}

bool App::OnInitialize()
{
    m_readmeCapture = ReadmeCapture::IsEnabled();
    m_cpuFrameStart = std::chrono::steady_clock::now();

    if (!CreateDeviceResources())
        return FailInitialization(L"Direct3D 11 device and swap-chain initialization failed.");

    AssetManager::Create();
    m_assetManagerCreated = true;

    if (!CreateRenderPipeline())
        return FailInitialization(L"The Hybrid Toon-PBR shader pipeline could not be created. Check the Project 38 shader files.");
    if (!CreateWindowSizeResources())
        return FailInitialization(L"The HDR, normal/profile, depth, or backbuffer resources could not be created.");
    if (!LoadCharacter())
        return FailInitialization(L"The public SampleModel.glb is missing or contains no renderable skinned mesh.");
    if (!InitializeImGui())
        return FailInitialization(L"ImGui initialization failed.");

    ApplyPreset(m_lightingPreset);
    m_profilerAvailable = m_gpuProfiler.Initialize(m_device.Get());
    return true;
}

void App::OnUninitialize()
{
    ShutdownImGui();

    if (m_context)
    {
        UnbindShaderResources();
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        m_context->ClearState();
        m_context->Flush();
    }

    m_poseAnimator.reset();
    m_character.reset();
    if (m_assetManagerCreated)
    {
        AssetManager::Destroy();
        m_assetManagerCreated = false;
    }

    ReleaseWindowSizeResources();
    m_shadowShaderResourceView.Reset();
    m_shadowDepthStencilView.Reset();
    m_shadowTexture.Reset();
    m_flatNormalTexture.Reset();
    m_blackTexture.Reset();
    m_whiteTexture.Reset();
    m_postConstantBuffer.Reset();
    m_characterConstantBuffer.Reset();
    m_shadowSampler.Reset();
    m_linearSampler.Reset();
    m_depthStencilState.Reset();
    m_shadowDoubleSidedRasterizerState.Reset();
    m_shadowRasterizerState.Reset();
    m_characterDoubleSidedRasterizerState.Reset();
    m_characterRasterizerState.Reset();
    m_skinnedInputLayout.Reset();
    m_toneMapPixelShader.Reset();
    m_outlinePixelShader.Reset();
    m_shadowPixelShader.Reset();
    m_characterPixelShader.Reset();
    m_fullscreenVertexShader.Reset();
    m_shadowVertexShader.Reset();
    m_characterVertexShader.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

void App::OnUpdate(const float& dt)
{
    if (!m_character || !m_context)
        return;

    if (m_readmeCapture)
    {
        m_character->SetAnimationTimeSeconds(kCapturePoseTimeSeconds);
        m_character->UpdateAnimation(m_context.Get(), 0.0);
    }
    else
    {
        m_character->UpdateAnimation(m_context.Get(), static_cast<double>(dt));
    }
    UpdateAnimationPalette();
}

void App::OnRender()
{
    if (!m_context || !m_swapChain || !m_renderTargetView || !m_hdrRenderTargetView || !m_depthStencilView)
        return;

    const auto frameStart = std::chrono::steady_clock::now();
    m_cpuFrameMs = std::chrono::duration<double, std::milli>(frameStart - m_cpuFrameStart).count();
    m_cpuFrameStart = frameStart;

    if (m_imguiInitialized)
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    m_gpuProfiler.BeginFrame(m_context.Get());

    m_gpuProfiler.BeginPass(m_context.Get(), GpuPass::Shadow);
    RenderShadowPass();
    m_gpuProfiler.EndPass(m_context.Get(), GpuPass::Shadow);

    m_gpuProfiler.BeginPass(m_context.Get(), GpuPass::Character);
    RenderCharacterPass();
    m_gpuProfiler.EndPass(m_context.Get(), GpuPass::Character);

    m_gpuProfiler.BeginPass(m_context.Get(), GpuPass::Outline);
    RenderOutlinePass();
    m_gpuProfiler.EndPass(m_context.Get(), GpuPass::Outline);

    m_gpuProfiler.BeginPass(m_context.Get(), GpuPass::ToneMap);
    RenderToneMapPass();
    m_gpuProfiler.EndPass(m_context.Get(), GpuPass::ToneMap);

    m_gpuProfiler.EndFrame(m_context.Get());
    m_gpuProfiler.Resolve(m_context.Get());

    if (m_imguiInitialized)
    {
        RenderHud();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    m_swapChain->Present(1, 0);
}

void App::OnInputProcess(
    const Keyboard::State& keyState,
    const Keyboard::KeyboardStateTracker& keyTracker,
    const Mouse::State& mouseState,
    const Mouse::ButtonStateTracker& mouseTracker)
{
    (void)keyState;
    (void)mouseState;
    (void)mouseTracker;

    if (keyTracker.IsKeyPressed(Keyboard::Keys::D1))
        m_renderMode = RenderMode::Pbr;
    if (keyTracker.IsKeyPressed(Keyboard::Keys::D2))
        m_renderMode = RenderMode::ToonPbr;
    if (keyTracker.IsKeyPressed(Keyboard::Keys::D3))
        m_renderMode = RenderMode::Split;
    if (keyTracker.IsKeyPressed(Keyboard::Keys::N))
        ApplyPreset(LightingPreset::NeonContrast);
    if (keyTracker.IsKeyPressed(Keyboard::Keys::I))
        ApplyPreset(LightingPreset::IndustrialSoft);
}

LRESULT CALLBACK App::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_SIZE && m_swapChain && wParam != SIZE_MINIMIZED)
    {
        const UINT width = LOWORD(lParam);
        const UINT height = HIWORD(lParam);
        if (width > 0 && height > 0 && !ResizeSwapChain(width, height))
        {
            MessageBoxW(hWnd,
                L"The Hybrid Toon-PBR render targets could not be resized.",
                L"Stylized Toon PBR",
                MB_OK | MB_ICONERROR);
        }
        return 0;
    }

    return GameApp::WndProc(hWnd, message, wParam, lParam);
}

bool App::CreateDeviceResources()
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc{};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = m_ClientWidth;
    swapChainDesc.BufferDesc.Height = m_ClientHeight;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = m_hWnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT creationFlags = 0;
#if defined(_DEBUG)
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL selectedFeatureLevel{};
    HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        featureLevels,
        static_cast<UINT>(std::size(featureLevels)),
        D3D11_SDK_VERSION,
        &swapChainDesc,
        m_swapChain.GetAddressOf(),
        m_device.GetAddressOf(),
        &selectedFeatureLevel,
        m_context.GetAddressOf());

#if defined(_DEBUG)
    if (result == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
        creationFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            creationFlags,
            featureLevels,
            static_cast<UINT>(std::size(featureLevels)),
            D3D11_SDK_VERSION,
            &swapChainDesc,
            m_swapChain.ReleaseAndGetAddressOf(),
            m_device.ReleaseAndGetAddressOf(),
            &selectedFeatureLevel,
            m_context.ReleaseAndGetAddressOf());
    }
#endif
    return SUCCEEDED(result);
}

bool App::CreateRenderPipeline()
{
    return CompileShaders()
        && CreatePipelineStates()
        && CreateConstantBuffers()
        && CreateShadowResources()
        && CreateFallbackTextures();
}

bool App::CompileShaders()
{
    ComPtr<ID3DBlob> characterVertexBlob;
    if (!CompileShader(L"38_CharacterVS.hlsl", "VSMain", "vs_5_0", characterVertexBlob))
        return false;
    if (FAILED(m_device->CreateVertexShader(
            characterVertexBlob->GetBufferPointer(),
            characterVertexBlob->GetBufferSize(),
            nullptr,
            m_characterVertexShader.GetAddressOf())))
        return false;

    const D3D11_INPUT_ELEMENT_DESC skinnedLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(m_device->CreateInputLayout(
            skinnedLayout,
            static_cast<UINT>(std::size(skinnedLayout)),
            characterVertexBlob->GetBufferPointer(),
            characterVertexBlob->GetBufferSize(),
            m_skinnedInputLayout.GetAddressOf())))
        return false;

    ComPtr<ID3DBlob> shaderBlob;
    if (!CompileShader(L"38_CharacterVS.hlsl", "VSShadow", "vs_5_0", shaderBlob)
        || FAILED(m_device->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, m_shadowVertexShader.GetAddressOf())))
        return false;

    shaderBlob.Reset();
    if (!CompileShader(L"38_CharacterPS.hlsl", "PSShadow", "ps_5_0", shaderBlob)
        || FAILED(m_device->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, m_shadowPixelShader.GetAddressOf())))
        return false;

    shaderBlob.Reset();
    if (!CompileShader(L"38_CharacterPS.hlsl", "PSMain", "ps_5_0", shaderBlob)
        || FAILED(m_device->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, m_characterPixelShader.GetAddressOf())))
        return false;

    shaderBlob.Reset();
    if (!CompileShader(L"38_FullscreenVS.hlsl", "VSMain", "vs_5_0", shaderBlob)
        || FAILED(m_device->CreateVertexShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, m_fullscreenVertexShader.GetAddressOf())))
        return false;

    shaderBlob.Reset();
    if (!CompileShader(L"38_ToneMapPS.hlsl", "PSMain", "ps_5_0", shaderBlob)
        || FAILED(m_device->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, m_toneMapPixelShader.GetAddressOf())))
        return false;

    shaderBlob.Reset();
    m_outlineShaderAvailable = CompileShader(L"38_OutlinePS.hlsl", "PSMain", "ps_5_0", shaderBlob)
        && SUCCEEDED(m_device->CreatePixelShader(
            shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, m_outlinePixelShader.GetAddressOf()));
    return true;
}

bool App::CreatePipelineStates()
{
    D3D11_RASTERIZER_DESC rasterizerDescription{};
    rasterizerDescription.FillMode = D3D11_FILL_SOLID;
    rasterizerDescription.CullMode = D3D11_CULL_BACK;
    rasterizerDescription.DepthClipEnable = TRUE;
    rasterizerDescription.ScissorEnable = TRUE;
    if (FAILED(m_device->CreateRasterizerState(&rasterizerDescription, m_characterRasterizerState.GetAddressOf())))
        return false;

    rasterizerDescription.CullMode = D3D11_CULL_NONE;
    if (FAILED(m_device->CreateRasterizerState(&rasterizerDescription, m_characterDoubleSidedRasterizerState.GetAddressOf())))
        return false;

    rasterizerDescription.CullMode = D3D11_CULL_BACK;
    rasterizerDescription.DepthBias = 1200;
    rasterizerDescription.SlopeScaledDepthBias = 2.0f;
    rasterizerDescription.DepthBiasClamp = 0.0f;
    if (FAILED(m_device->CreateRasterizerState(&rasterizerDescription, m_shadowRasterizerState.GetAddressOf())))
        return false;

    rasterizerDescription.CullMode = D3D11_CULL_NONE;
    if (FAILED(m_device->CreateRasterizerState(&rasterizerDescription, m_shadowDoubleSidedRasterizerState.GetAddressOf())))
        return false;

    D3D11_DEPTH_STENCIL_DESC depthStencilDescription{};
    depthStencilDescription.DepthEnable = TRUE;
    depthStencilDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    if (FAILED(m_device->CreateDepthStencilState(&depthStencilDescription, m_depthStencilState.GetAddressOf())))
        return false;

    D3D11_SAMPLER_DESC samplerDescription{};
    samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(m_device->CreateSamplerState(&samplerDescription, m_linearSampler.GetAddressOf())))
        return false;

    samplerDescription.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDescription.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDescription.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDescription.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    samplerDescription.BorderColor[0] = 1.0f;
    samplerDescription.BorderColor[1] = 1.0f;
    samplerDescription.BorderColor[2] = 1.0f;
    samplerDescription.BorderColor[3] = 1.0f;
    if (FAILED(m_device->CreateSamplerState(&samplerDescription, m_shadowSampler.GetAddressOf())))
        return false;

    return true;
}

bool App::CreateConstantBuffers()
{
    D3D11_BUFFER_DESC bufferDescription{};
    bufferDescription.Usage = D3D11_USAGE_DYNAMIC;
    bufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDescription.ByteWidth = static_cast<UINT>(sizeof(CharacterConstants));
    if (FAILED(m_device->CreateBuffer(&bufferDescription, nullptr, m_characterConstantBuffer.GetAddressOf())))
        return false;

    bufferDescription.ByteWidth = static_cast<UINT>(sizeof(PostConstants));
    return SUCCEEDED(m_device->CreateBuffer(&bufferDescription, nullptr, m_postConstantBuffer.GetAddressOf()));
}

bool App::CreateShadowResources()
{
    D3D11_TEXTURE2D_DESC textureDescription{};
    textureDescription.Width = kShadowMapSize;
    textureDescription.Height = kShadowMapSize;
    textureDescription.MipLevels = 1;
    textureDescription.ArraySize = 1;
    textureDescription.Format = DXGI_FORMAT_R32_TYPELESS;
    textureDescription.SampleDesc.Count = 1;
    textureDescription.Usage = D3D11_USAGE_DEFAULT;
    textureDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(m_device->CreateTexture2D(&textureDescription, nullptr, m_shadowTexture.GetAddressOf())))
        return false;

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDescription{};
    depthViewDescription.Format = DXGI_FORMAT_D32_FLOAT;
    depthViewDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (FAILED(m_device->CreateDepthStencilView(
            m_shadowTexture.Get(), &depthViewDescription, m_shadowDepthStencilView.GetAddressOf())))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC shaderViewDescription{};
    shaderViewDescription.Format = DXGI_FORMAT_R32_FLOAT;
    shaderViewDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    shaderViewDescription.Texture2D.MipLevels = 1;
    return SUCCEEDED(m_device->CreateShaderResourceView(
        m_shadowTexture.Get(), &shaderViewDescription, m_shadowShaderResourceView.GetAddressOf()));
}

bool App::CreateFallbackTextures()
{
    const auto createTexture = [this](uint32_t rgba, ComPtr<ID3D11ShaderResourceView>& output) -> bool
    {
        D3D11_TEXTURE2D_DESC textureDescription{};
        textureDescription.Width = 1;
        textureDescription.Height = 1;
        textureDescription.MipLevels = 1;
        textureDescription.ArraySize = 1;
        textureDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDescription.SampleDesc.Count = 1;
        textureDescription.Usage = D3D11_USAGE_IMMUTABLE;
        textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA textureData{};
        textureData.pSysMem = &rgba;
        textureData.SysMemPitch = sizeof(rgba);
        ComPtr<ID3D11Texture2D> texture;
        return SUCCEEDED(m_device->CreateTexture2D(&textureDescription, &textureData, texture.GetAddressOf()))
            && SUCCEEDED(m_device->CreateShaderResourceView(texture.Get(), nullptr, output.GetAddressOf()));
    };

    return createTexture(0xffffffffu, m_whiteTexture)
        && createTexture(0xff000000u, m_blackTexture)
        && createTexture(0xffff8080u, m_flatNormalTexture);
}

bool App::CreateWindowSizeResources()
{
    if (m_resourceWidth == m_ClientWidth && m_resourceHeight == m_ClientHeight
        && m_renderTargetView && m_hdrRenderTargetView && m_depthStencilView)
        return true;

    ReleaseWindowSizeResources();

    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())))
        || FAILED(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.GetAddressOf())))
        return false;

    const auto createColorTarget = [this](
        DXGI_FORMAT format,
        ComPtr<ID3D11Texture2D>& texture,
        ComPtr<ID3D11RenderTargetView>& renderTarget,
        ComPtr<ID3D11ShaderResourceView>& shaderResource) -> bool
    {
        D3D11_TEXTURE2D_DESC textureDescription{};
        textureDescription.Width = m_ClientWidth;
        textureDescription.Height = m_ClientHeight;
        textureDescription.MipLevels = 1;
        textureDescription.ArraySize = 1;
        textureDescription.Format = format;
        textureDescription.SampleDesc.Count = 1;
        textureDescription.Usage = D3D11_USAGE_DEFAULT;
        textureDescription.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        return SUCCEEDED(m_device->CreateTexture2D(&textureDescription, nullptr, texture.GetAddressOf()))
            && SUCCEEDED(m_device->CreateRenderTargetView(texture.Get(), nullptr, renderTarget.GetAddressOf()))
            && SUCCEEDED(m_device->CreateShaderResourceView(texture.Get(), nullptr, shaderResource.GetAddressOf()));
    };

    if (!createColorTarget(
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            m_hdrTexture,
            m_hdrRenderTargetView,
            m_hdrShaderResourceView))
        return false;

    const bool normalProfileAvailable = createColorTarget(
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        m_normalProfileTexture,
        m_normalProfileRenderTargetView,
        m_normalProfileShaderResourceView);
    if (!normalProfileAvailable)
    {
        m_normalProfileShaderResourceView.Reset();
        m_normalProfileRenderTargetView.Reset();
        m_normalProfileTexture.Reset();
    }

    D3D11_TEXTURE2D_DESC depthDescription{};
    depthDescription.Width = m_ClientWidth;
    depthDescription.Height = m_ClientHeight;
    depthDescription.MipLevels = 1;
    depthDescription.ArraySize = 1;
    depthDescription.Format = DXGI_FORMAT_R32_TYPELESS;
    depthDescription.SampleDesc.Count = 1;
    depthDescription.Usage = D3D11_USAGE_DEFAULT;
    depthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(m_device->CreateTexture2D(&depthDescription, nullptr, m_depthTexture.GetAddressOf())))
        return false;

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDescription{};
    depthViewDescription.Format = DXGI_FORMAT_D32_FLOAT;
    depthViewDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (FAILED(m_device->CreateDepthStencilView(m_depthTexture.Get(), &depthViewDescription, m_depthStencilView.GetAddressOf())))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC depthShaderViewDescription{};
    depthShaderViewDescription.Format = DXGI_FORMAT_R32_FLOAT;
    depthShaderViewDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    depthShaderViewDescription.Texture2D.MipLevels = 1;
    if (FAILED(m_device->CreateShaderResourceView(
            m_depthTexture.Get(), &depthShaderViewDescription, m_depthShaderResourceView.GetAddressOf())))
        return false;

    if (normalProfileAvailable)
    {
        if (!createColorTarget(
                DXGI_FORMAT_R8_UNORM,
                m_outlineTexture,
                m_outlineRenderTargetView,
                m_outlineShaderResourceView))
        {
            m_outlineShaderResourceView.Reset();
            m_outlineRenderTargetView.Reset();
            m_outlineTexture.Reset();
        }
    }
    else
    {
        m_outlineShaderResourceView.Reset();
        m_outlineRenderTargetView.Reset();
        m_outlineTexture.Reset();
    }

    m_resourceWidth = m_ClientWidth;
    m_resourceHeight = m_ClientHeight;
    m_outlineAvailable = m_outlineShaderAvailable
        && m_normalProfileShaderResourceView
        && m_outlineRenderTargetView
        && m_outlineShaderResourceView
        && m_depthShaderResourceView;
    return true;
}

bool App::ResizeSwapChain(UINT width, UINT height)
{
    if (width == m_resourceWidth && height == m_resourceHeight)
        return true;

    UnbindShaderResources();
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    ReleaseWindowSizeResources();
    if (FAILED(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)))
        return false;

    m_ClientWidth = width;
    m_ClientHeight = height;
    return CreateWindowSizeResources();
}

bool App::LoadCharacter()
{
    m_character = AssetManager::GetInstance().GetFbxModel(m_device.Get(), kCharacterPath);
    if (!m_character || !m_character->HasMesh()
        || m_character->GetVertexStride() != sizeof(VertexSkinnedTBN)
        || !m_character->GetBoneConstantBuffer())
        return false;

    BuildMaterialProfiles();
    SelectIdleAnimation();
    m_poseAnimator = std::make_unique<CharacterAnimator>();
    m_poseAnimator->Initialize(
        m_device.Get(),
        m_character->GetScenePtr(),
        m_character->GetNodeIndexOfName(),
        m_character->GetGlobalInverse(),
        m_character->GetBoneNames(),
        m_character->GetBoneOffsets());
    if (!m_poseAnimator->GetBoneCB())
        return false;
    m_character->UpdateAnimation(m_context.Get(), 0.0);
    UpdateAnimationPalette();
    return true;
}

void App::BuildMaterialProfiles()
{
    m_materialRenderInfo.clear();
    const aiScene* scene = m_character ? m_character->GetScenePtr() : nullptr;
    if (!scene)
        return;

    m_materialRenderInfo.resize(scene->mNumMaterials);
    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex)
    {
        aiMaterial* material = scene->mMaterials[materialIndex];
        MaterialRenderInfo& renderInfo = m_materialRenderInfo[materialIndex];
        aiString materialName;
        if (material->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS)
            renderInfo.profile = ClassifyMaterialName(materialName.C_Str());

        aiString alphaModeValue;
        std::string alphaMode = "OPAQUE";
        if (material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaModeValue) == AI_SUCCESS)
            alphaMode = alphaModeValue.C_Str();
        std::transform(alphaMode.begin(), alphaMode.end(), alphaMode.begin(), [](unsigned char character)
        {
            return static_cast<char>(std::toupper(character));
        });

        if (alphaMode == "MASK")
        {
            renderInfo.alphaCutoff = 0.5f;
            material->Get(AI_MATKEY_GLTF_ALPHACUTOFF, renderInfo.alphaCutoff);
        }
        else if (alphaMode == "BLEND")
        {
            // A depth-only shadow map needs binary coverage; use the same low-alpha coverage cutoff in both passes.
            renderInfo.alphaCutoff = kBlendCoverageCutoff;
        }

        int doubleSided = 0;
        if (material->Get(AI_MATKEY_TWOSIDED, doubleSided) == AI_SUCCESS)
            renderInfo.doubleSided = doubleSided != 0;
    }

    for (const auto& overrideEntry : kSampleModelMaterialOverrides)
    {
        if (overrideEntry.materialIndex < m_materialRenderInfo.size())
            m_materialRenderInfo[overrideEntry.materialIndex].profile = overrideEntry.profile;
    }
}

App::MaterialProfile App::ClassifyMaterialName(const char* materialName) const
{
    std::string lowerName = materialName ? materialName : "";
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });

    if (lowerName.find("hair") != std::string::npos)
        return MaterialProfile::Hair;
    if (lowerName.find("face") != std::string::npos || lowerName.find("skin") != std::string::npos)
        return MaterialProfile::Skin;
    if (lowerName.find("cloth") != std::string::npos || lowerName.find("body") != std::string::npos)
        return MaterialProfile::Cloth;
    return MaterialProfile::Cloth;
}

void App::SelectIdleAnimation()
{
    if (!m_character || !m_character->HasAnimations())
        return;

    const auto& animationNames = m_character->GetAnimationNames();
    int idleIndex = 0;
    int vrmIdleFallbackIndex = -1;
    for (size_t animationIndex = 0; animationIndex < animationNames.size(); ++animationIndex)
    {
        std::string lowerName = animationNames[animationIndex];
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
        if (lowerName.find("idle") != std::string::npos)
        {
            idleIndex = static_cast<int>(animationIndex);
            break;
        }
        if (lowerName == "vrm_1")
            vrmIdleFallbackIndex = static_cast<int>(animationIndex);
    }

    if (idleIndex == 0 && vrmIdleFallbackIndex >= 0)
        idleIndex = vrmIdleFallbackIndex;

    m_character->SetCurrentAnimation(idleIndex);
    m_character->SetAnimationPlaying(!m_readmeCapture);
    if (m_readmeCapture)
        m_character->SetAnimationTimeSeconds(kCapturePoseTimeSeconds);
}

void App::UpdateAnimationPalette()
{
    if (!m_character || !m_poseAnimator || !m_context)
        return;

    const aiScene* scene = m_character->GetScenePtr();
    const int clipIndex = m_character->GetCurrentAnimationIndex();
    if (!scene || clipIndex < 0 || static_cast<unsigned int>(clipIndex) >= scene->mNumAnimations)
        return;

    const aiAnimation* clip = scene->mAnimations[clipIndex];
    const float timeSeconds = static_cast<float>(m_character->GetAnimationTimeSeconds());
    m_poseAnimator->UpdateAnimation(0.0f, clip, timeSeconds, clip, timeSeconds, 0.0f);
    m_poseAnimator->UploadPalette(m_context.Get(), m_poseAnimator->finalTransforms);
    m_context->CopyResource(m_character->GetBoneConstantBuffer(), m_poseAnimator->GetBoneCB());
}

void App::ApplyPreset(LightingPreset preset)
{
    m_lightingPreset = preset;
    if (preset == LightingPreset::NeonContrast)
    {
        m_lowBandThreshold = 0.34f;
        m_highBandThreshold = 0.69f;
        m_bandSoftness = 0.055f;
        m_shadowSoftness = 1.4f;
        m_hairHighlightStrength = 0.82f;
        m_rimStrength = 0.38f;
        m_shadowTint = { 0.24f, 0.34f, 0.58f };
        m_keyTint = { 1.05f, 0.72f, 0.48f };
        m_exposure = 1.08f;
    }
    else
    {
        m_lowBandThreshold = 0.38f;
        m_highBandThreshold = 0.72f;
        m_bandSoftness = 0.095f;
        m_shadowSoftness = 1.15f;
        m_hairHighlightStrength = 0.58f;
        m_rimStrength = 0.26f;
        m_shadowTint = { 0.38f, 0.42f, 0.52f };
        m_keyTint = { 1.0f, 0.84f, 0.70f };
        m_exposure = 0.98f;
    }
}

void App::UnbindShaderResources()
{
    if (!m_context)
        return;
    std::array<ID3D11ShaderResourceView*, 8> nullShaderResources{};
    m_context->PSSetShaderResources(0, static_cast<UINT>(nullShaderResources.size()), nullShaderResources.data());
}

void App::SetFullScreenViewportAndScissor()
{
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(m_resourceWidth);
    viewport.Height = static_cast<float>(m_resourceHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    const D3D11_RECT scissorRectangle = {
        0,
        0,
        static_cast<LONG>(m_resourceWidth),
        static_cast<LONG>(m_resourceHeight),
    };
    m_context->RSSetScissorRects(1, &scissorRectangle);
}

void App::RenderShadowPass()
{
    UnbindShaderResources();
    m_context->OMSetRenderTargets(0, nullptr, m_shadowDepthStencilView.Get());
    m_context->ClearDepthStencilView(m_shadowDepthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(kShadowMapSize);
    viewport.Height = static_cast<float>(kShadowMapSize);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);
    const D3D11_RECT shadowScissor = { 0, 0, static_cast<LONG>(kShadowMapSize), static_cast<LONG>(kShadowMapSize) };
    m_context->RSSetScissorRects(1, &shadowScissor);
    m_context->RSSetState(m_shadowRasterizerState.Get());
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    DrawCharacter(RenderMode::Pbr, AspectRatio(), true);
}

void App::RenderCharacterPass()
{
    UnbindShaderResources();
    ID3D11RenderTargetView* renderTargets[] = {
        m_hdrRenderTargetView.Get(),
        nullptr,
    };
    UINT renderTargetCount = 1;
    if (m_normalProfileRenderTargetView)
        renderTargets[renderTargetCount++] = m_normalProfileRenderTargetView.Get();
    m_context->OMSetRenderTargets(renderTargetCount, renderTargets, m_depthStencilView.Get());

    constexpr float hdrClear[] = { 0.018f, 0.026f, 0.052f, 1.0f };
    constexpr float normalClear[] = { 0.5f, 0.5f, 1.0f, 0.0f };
    m_context->ClearRenderTargetView(m_hdrRenderTargetView.Get(), hdrClear);
    if (m_normalProfileRenderTargetView)
        m_context->ClearRenderTargetView(m_normalProfileRenderTargetView.Get(), normalClear);
    m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    m_context->RSSetState(m_characterRasterizerState.Get());
    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);

    if (m_renderMode == RenderMode::Split)
    {
        const UINT leftWidth = m_resourceWidth / 2;
        const UINT rightWidth = m_resourceWidth - leftWidth;
        D3D11_VIEWPORT leftViewport{};
        leftViewport.Width = static_cast<float>(leftWidth);
        leftViewport.Height = static_cast<float>(m_resourceHeight);
        leftViewport.MinDepth = 0.0f;
        leftViewport.MaxDepth = 1.0f;
        const D3D11_RECT leftScissor = { 0, 0, static_cast<LONG>(leftWidth), static_cast<LONG>(m_resourceHeight) };
        m_context->RSSetViewports(1, &leftViewport);
        m_context->RSSetScissorRects(1, &leftScissor);
        DrawCharacter(RenderMode::Pbr, static_cast<float>(leftWidth) / static_cast<float>(m_resourceHeight), false);

        D3D11_VIEWPORT rightViewport = leftViewport;
        rightViewport.TopLeftX = static_cast<float>(leftWidth);
        rightViewport.Width = static_cast<float>(rightWidth);
        const D3D11_RECT rightScissor = {
            static_cast<LONG>(leftWidth),
            0,
            static_cast<LONG>(m_resourceWidth),
            static_cast<LONG>(m_resourceHeight),
        };
        m_context->RSSetViewports(1, &rightViewport);
        m_context->RSSetScissorRects(1, &rightScissor);
        DrawCharacter(RenderMode::ToonPbr, static_cast<float>(rightWidth) / static_cast<float>(m_resourceHeight), false);
        SetFullScreenViewportAndScissor();
    }
    else
    {
        SetFullScreenViewportAndScissor();
        DrawCharacter(m_renderMode, AspectRatio(), false);
    }
}

void App::DrawCharacter(RenderMode mode, float projectionAspect, bool shadowOnly)
{
    if (!m_character)
        return;

    ID3D11Buffer* vertexBuffer = m_character->GetVertexBuffer();
    ID3D11Buffer* indexBuffer = m_character->GetIndexBuffer();
    UINT stride = sizeof(VertexSkinnedTBN);
    UINT offset = 0;
    m_context->IASetInputLayout(m_skinnedInputLayout.Get());
    m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    m_context->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(shadowOnly ? m_shadowVertexShader.Get() : m_characterVertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(shadowOnly ? m_shadowPixelShader.Get() : m_characterPixelShader.Get(), nullptr, 0);

    ID3D11Buffer* boneConstantBuffer = m_character->GetBoneConstantBuffer();
    ID3D11Buffer* characterConstantBuffer = m_characterConstantBuffer.Get();
    m_context->VSSetConstantBuffers(0, 1, &characterConstantBuffer);
    m_context->VSSetConstantBuffers(1, 1, &boneConstantBuffer);
    m_context->PSSetConstantBuffers(0, 1, &characterConstantBuffer);
    ID3D11SamplerState* linearSampler = m_linearSampler.Get();
    m_context->PSSetSamplers(0, 1, &linearSampler);
    if (!shadowOnly)
    {
        ID3D11SamplerState* shadowSampler = m_shadowSampler.Get();
        m_context->PSSetSamplers(1, 1, &shadowSampler);
    }

    const auto& baseColorTextures = m_character->GetMaterialSRVs();
    const auto& metallicTextures = m_character->GetMetallicSRVs();
    const auto& roughnessTextures = m_character->GetRoughnessSRVs();
    const auto& normalTextures = m_character->GetNormalSRVs();
    for (const auto& subset : m_character->GetSubsets())
    {
        const uint32_t materialIndex = subset.materialIndex;
        const MaterialRenderInfo renderInfo = materialIndex < m_materialRenderInfo.size()
            ? m_materialRenderInfo[materialIndex]
            : MaterialRenderInfo{};

        ID3D11ShaderResourceView* baseColor = materialIndex < baseColorTextures.size()
            ? baseColorTextures[materialIndex]
            : nullptr;
        ID3D11ShaderResourceView* metallic = materialIndex < metallicTextures.size()
            ? metallicTextures[materialIndex]
            : nullptr;
        ID3D11ShaderResourceView* roughness = materialIndex < roughnessTextures.size()
            ? roughnessTextures[materialIndex]
            : nullptr;
        ID3D11ShaderResourceView* normal = materialIndex < normalTextures.size()
            ? normalTextures[materialIndex]
            : nullptr;

        UpdateCharacterConstants(
            mode,
            renderInfo.profile,
            renderInfo.alphaCutoff,
            projectionAspect,
            baseColor != nullptr,
            metallic != nullptr,
            roughness != nullptr,
            normal != nullptr);

        ID3D11RasterizerState* materialRasterizerState = shadowOnly
            ? (renderInfo.doubleSided ? m_shadowDoubleSidedRasterizerState.Get() : m_shadowRasterizerState.Get())
            : (renderInfo.doubleSided ? m_characterDoubleSidedRasterizerState.Get() : m_characterRasterizerState.Get());
        m_context->RSSetState(materialRasterizerState);

        if (shadowOnly)
        {
            ID3D11ShaderResourceView* baseColorForShadow = baseColor ? baseColor : m_whiteTexture.Get();
            m_context->PSSetShaderResources(0, 1, &baseColorForShadow);
        }
        else
        {
            ID3D11ShaderResourceView* textures[] = {
                baseColor ? baseColor : m_whiteTexture.Get(),
                metallic ? metallic : m_blackTexture.Get(),
                roughness ? roughness : m_whiteTexture.Get(),
                normal ? normal : m_flatNormalTexture.Get(),
                m_shadowShaderResourceView.Get(),
            };
            m_context->PSSetShaderResources(0, static_cast<UINT>(std::size(textures)), textures);
        }
        m_context->DrawIndexed(subset.indexCount, subset.startIndex, 0);
    }
}

void App::UpdateCharacterConstants(
    RenderMode mode,
    MaterialProfile profile,
    float alphaCutoff,
    float projectionAspect,
    bool hasBaseColor,
    bool hasMetallic,
    bool hasRoughness,
    bool hasNormal)
{
    const XMMATRIX worldMatrix = XMMatrixScaling(kHeroScale, kHeroScale, kHeroScale)
        * XMMatrixRotationY(kHeroYawRadians)
        * XMMatrixTranslation(22.0f, 32.0f, 0.0f);
    const XMVECTOR cameraEye = XMVectorSet(12.0f, 67.0f, -470.0f, 1.0f);
    const XMVECTOR cameraTarget = XMVectorSet(18.0f, 63.0f, 0.0f, 1.0f);
    const XMMATRIX viewMatrix = XMMatrixLookAtLH(cameraEye, cameraTarget, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(35.0f),
        std::max(projectionAspect, 0.1f),
        0.5f,
        700.0f);

    const XMVECTOR lightVector = XMVector3Normalize(XMVectorSet(-0.42f, 0.78f, -0.46f, 0.0f));
    const XMVECTOR shadowTarget = XMVectorSet(18.0f, 62.0f, 0.0f, 1.0f);
    const XMVECTOR lightPosition = shadowTarget + lightVector * 260.0f;
    const XMMATRIX lightView = XMMatrixLookAtLH(lightPosition, shadowTarget, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX lightProjection = XMMatrixOrthographicLH(220.0f, 220.0f, 1.0f, 600.0f);

    float roughness = 0.72f;
    float metallic = 0.02f;
    switch (profile)
    {
    case MaterialProfile::Skin:
        roughness = 0.58f;
        metallic = 0.0f;
        break;
    case MaterialProfile::Hair:
        roughness = 0.28f;
        metallic = 0.03f;
        break;
    case MaterialProfile::Cloth:
        roughness = 0.78f;
        metallic = 0.01f;
        break;
    }

    CharacterConstants constants{};
    XMStoreFloat4x4(&constants.world, XMMatrixTranspose(worldMatrix));
    XMStoreFloat4x4(&constants.viewProjection, XMMatrixTranspose(viewMatrix * projectionMatrix));
    XMStoreFloat4x4(&constants.lightViewProjection, XMMatrixTranspose(lightView * lightProjection));
    XMStoreFloat4(&constants.cameraPosition, cameraEye);
    XMStoreFloat4(&constants.lightDirection, lightVector);
    constants.warmKeyTint = { m_keyTint.x, m_keyTint.y, m_keyTint.z, 1.0f };
    constants.coolShadowTint = { m_shadowTint.x, m_shadowTint.y, m_shadowTint.z, 1.0f };
    constants.diffuseBandThresholds = { m_lowBandThreshold, m_highBandThreshold, m_bandSoftness, 0.0f };
    constants.toonParameters = {
        m_bandSoftness,
        m_hairHighlightStrength,
        m_rimStrength,
        mode == RenderMode::ToonPbr ? 1.0f : 0.0f,
    };
    constants.materialParameters = { static_cast<float>(profile), roughness, metallic, alphaCutoff };
    constants.shadowParameters = { 1.0f / static_cast<float>(kShadowMapSize), m_shadowSoftness, 0.0012f, 0.0f };
    constants.textureParameters = {
        hasBaseColor ? 1.0f : 0.0f,
        hasMetallic ? 1.0f : 0.0f,
        hasRoughness ? 1.0f : 0.0f,
        hasNormal ? 1.0f : 0.0f,
    };
    UpdateDynamicBuffer(m_context.Get(), m_characterConstantBuffer.Get(), constants);
}

void App::UpdatePostConstants()
{
    PostConstants constants{};
    constants.inverseResolution = {
        1.0f / std::max(1.0f, static_cast<float>(m_resourceWidth)),
        1.0f / std::max(1.0f, static_cast<float>(m_resourceHeight)),
        static_cast<float>(m_resourceWidth),
        static_cast<float>(m_resourceHeight),
    };
    constants.outlineParameters = { m_outlineWidth, static_cast<float>(m_outlineQuality), 0.10f, 0.16f };
    constants.depthReconstructionParameters = { 0.5f, 700.0f, 0.008f, 900.0f };
    constants.toneMapParameters = {
        m_exposure,
        static_cast<float>(m_lightingPreset),
        m_outlineAvailable ? 0.9f : 0.0f,
        0.0f,
    };
    constants.backgroundColor = { 0.018f, 0.026f, 0.052f, 1.0f };
    UpdateDynamicBuffer(m_context.Get(), m_postConstantBuffer.Get(), constants);
}

void App::RenderOutlinePass()
{
    UnbindShaderResources();
    if (!m_outlineRenderTargetView)
        return;

    constexpr float clearMask[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetRenderTargets(1, m_outlineRenderTargetView.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_outlineRenderTargetView.Get(), clearMask);
    if (!m_outlineAvailable)
        return;

    SetFullScreenViewportAndScissor();
    UpdatePostConstants();
    m_context->RSSetState(m_characterRasterizerState.Get());
    m_context->OMSetDepthStencilState(nullptr, 0);
    m_context->IASetInputLayout(nullptr);
    m_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_fullscreenVertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_outlinePixelShader.Get(), nullptr, 0);
    ID3D11Buffer* postConstantBuffer = m_postConstantBuffer.Get();
    m_context->PSSetConstantBuffers(0, 1, &postConstantBuffer);
    ID3D11ShaderResourceView* inputs[] = { m_normalProfileShaderResourceView.Get(), m_depthShaderResourceView.Get() };
    m_context->PSSetShaderResources(0, static_cast<UINT>(std::size(inputs)), inputs);
    ID3D11SamplerState* sampler = m_linearSampler.Get();
    m_context->PSSetSamplers(0, 1, &sampler);
    m_context->Draw(3, 0);
}

void App::RenderToneMapPass()
{
    UnbindShaderResources();
    ID3D11RenderTargetView* backBufferTarget = m_renderTargetView.Get();
    m_context->OMSetRenderTargets(1, &backBufferTarget, nullptr);
    constexpr float clearColor[] = { 0.012f, 0.018f, 0.035f, 1.0f };
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
    SetFullScreenViewportAndScissor();
    UpdatePostConstants();
    m_context->RSSetState(m_characterRasterizerState.Get());
    m_context->OMSetDepthStencilState(nullptr, 0);
    m_context->IASetInputLayout(nullptr);
    m_context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    m_context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_fullscreenVertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_toneMapPixelShader.Get(), nullptr, 0);
    ID3D11Buffer* postConstantBuffer = m_postConstantBuffer.Get();
    m_context->PSSetConstantBuffers(0, 1, &postConstantBuffer);
    ID3D11ShaderResourceView* inputs[] = {
        m_hdrShaderResourceView.Get(),
        m_outlineShaderResourceView ? m_outlineShaderResourceView.Get() : m_blackTexture.Get(),
        m_normalProfileShaderResourceView ? m_normalProfileShaderResourceView.Get() : m_flatNormalTexture.Get(),
    };
    m_context->PSSetShaderResources(0, static_cast<UINT>(std::size(inputs)), inputs);
    ID3D11SamplerState* sampler = m_linearSampler.Get();
    m_context->PSSetSamplers(0, 1, &sampler);
    m_context->Draw(3, 0);
}

void App::RenderHud()
{
    ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.90f);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("Project 38", nullptr, flags))
    {
        ImGui::TextUnformatted("Hybrid Toon-PBR Character Showcase");
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 480.0f);
        ImGui::TextUnformatted("Material-aware toon shading, hair highlights, rim lighting, stable outlines, and GPU cost comparison.");
        ImGui::PopTextWrapPos();
        ImGui::Separator();

        int renderMode = static_cast<int>(m_renderMode);
        const char* renderModes[] = { "PBR", "Hybrid Toon-PBR", "Split" };
        if (ImGui::Combo("Mode", &renderMode, renderModes, static_cast<int>(std::size(renderModes))))
            m_renderMode = static_cast<RenderMode>(renderMode);

        int lightingPreset = static_cast<int>(m_lightingPreset);
        const char* lightingPresets[] = { "Neon Contrast", "Industrial Soft" };
        if (ImGui::Combo("Preset", &lightingPreset, lightingPresets, static_cast<int>(std::size(lightingPresets))))
            ApplyPreset(static_cast<LightingPreset>(lightingPreset));

        const char* modeName = renderModes[static_cast<int>(m_renderMode)];
        const char* presetName = lightingPresets[static_cast<int>(m_lightingPreset)];
        ImGui::Text("Active: %s / %s", modeName, presetName);
        ImGui::Text("CPU: %.2f ms", m_cpuFrameMs);
        const GpuTimings& timings = m_gpuProfiler.Latest();
        if (!m_profilerAvailable)
        {
            ImGui::TextUnformatted("GPU: unavailable");
        }
        else if (!timings.valid)
        {
            ImGui::TextUnformatted("GPU: warming up");
        }
        else
        {
            ImGui::Text("GPU: %.3f ms", timings.totalMs);
            ImGui::Text("Shadow: %.3f ms", timings.passMs[static_cast<size_t>(GpuPass::Shadow)]);
            ImGui::Text("Character: %.3f ms", timings.passMs[static_cast<size_t>(GpuPass::Character)]);
            ImGui::Text("Outline: %.3f ms", timings.passMs[static_cast<size_t>(GpuPass::Outline)]);
            ImGui::Text("ToneMap: %.3f ms", timings.passMs[static_cast<size_t>(GpuPass::ToneMap)]);
        }

        ImGui::Separator();
        float thresholds[] = { m_lowBandThreshold, m_highBandThreshold };
        if (ImGui::DragFloat2("Band thresholds", thresholds, 0.005f, 0.05f, 0.95f, "%.3f"))
        {
            m_lowBandThreshold = std::min(thresholds[0], thresholds[1] - 0.02f);
            m_highBandThreshold = std::max(thresholds[1], m_lowBandThreshold + 0.02f);
        }
        ImGui::DragFloat("Band softness", &m_bandSoftness, 0.002f, 0.005f, 0.20f, "%.3f");
        ImGui::ColorEdit3("Shadow tint", &m_shadowTint.x);
        ImGui::ColorEdit3("Key tint", &m_keyTint.x);
        ImGui::SliderFloat("Hair highlight", &m_hairHighlightStrength, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Rim strength", &m_rimStrength, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Shadow softness", &m_shadowSoftness, 0.5f, 3.0f, "%.2f texels");
        ImGui::SliderFloat("Outline width", &m_outlineWidth, 0.5f, 3.0f, "%.2f px");
        ImGui::SliderInt("Outline quality", &m_outlineQuality, 1, 2);
        ImGui::SliderFloat("Exposure", &m_exposure, 0.4f, 2.0f, "%.2f");
        if (!m_outlineAvailable)
            ImGui::TextDisabled("Outlines disabled: optional normal/depth edge pipeline unavailable");
        ImGui::TextDisabled("Keys: 1 PBR, 2 Toon-PBR, 3 Split, N/I presets");
    }
    ImGui::End();
}

bool App::InitializeImGui()
{
    IMGUI_CHECKVERSION();
    if (!ImGui::CreateContext())
        return false;

    ImGui::StyleColorsDark();
    if (!ImGui_ImplWin32_Init(m_hWnd))
    {
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplDX11_Init(m_device.Get(), m_context.Get()))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    m_imguiInitialized = true;
    return true;
}

bool App::FailInitialization(const wchar_t* message)
{
    MessageBoxW(m_hWnd, message, L"Stylized Toon PBR initialization error", MB_OK | MB_ICONERROR);
    OnUninitialize();
    return false;
}

void App::ReleaseWindowSizeResources()
{
    m_outlineShaderResourceView.Reset();
    m_outlineRenderTargetView.Reset();
    m_outlineTexture.Reset();
    m_depthShaderResourceView.Reset();
    m_depthStencilView.Reset();
    m_depthTexture.Reset();
    m_normalProfileShaderResourceView.Reset();
    m_normalProfileRenderTargetView.Reset();
    m_normalProfileTexture.Reset();
    m_hdrShaderResourceView.Reset();
    m_hdrRenderTargetView.Reset();
    m_hdrTexture.Reset();
    m_renderTargetView.Reset();
    m_outlineAvailable = false;
    m_resourceWidth = 0;
    m_resourceHeight = 0;
}

void App::ShutdownImGui()
{
    if (!m_imguiInitialized)
        return;

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    m_imguiInitialized = false;
}
