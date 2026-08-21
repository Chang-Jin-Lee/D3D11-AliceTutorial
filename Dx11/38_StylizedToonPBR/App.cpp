#include "App.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <cwchar>

App::App()
{
    wcscpy_s(m_szTitle, L"Stylized Toon PBR");
}

bool App::OnInitialize()
{
    if (!CreateDeviceResources())
        return FailInitialization(L"Direct3D 11 device and swap-chain initialization failed.");
    if (!CreateWindowSizeResources())
        return FailInitialization(L"Direct3D 11 render-target or depth-buffer initialization failed.");
    if (!InitializeImGui())
        return FailInitialization(L"ImGui initialization failed.");

    return true;
}

void App::OnUninitialize()
{
    ShutdownImGui();

    if (m_context)
    {
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
        m_context->ClearState();
        m_context->Flush();
    }

    ReleaseWindowSizeResources();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

void App::OnUpdate(const float& dt)
{
    (void)dt;
}

void App::OnRender()
{
    if (!m_context || !m_renderTargetView || !m_depthStencilView || !m_swapChain)
        return;

    ID3D11RenderTargetView* renderTarget = m_renderTargetView.Get();
    m_context->OMSetRenderTargets(1, &renderTarget, m_depthStencilView.Get());

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(m_ClientWidth);
    viewport.Height = static_cast<float>(m_ClientHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    constexpr float clearColor[] = { 0.035f, 0.045f, 0.075f, 1.0f };
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
    m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    if (m_imguiInitialized)
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.88f);
        if (ImGui::Begin("Stylized Toon-PBR"))
        {
            ImGui::TextUnformatted("Project 38 application shell");
            ImGui::TextDisabled("Renderer implementation follows in Task 4.");
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    m_swapChain->Present(1, 0);
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
                L"The Direct3D render targets could not be resized.",
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
    swapChainDesc.BufferCount = 1;
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

    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
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
            m_swapChain.GetAddressOf(),
            m_device.GetAddressOf(),
            &selectedFeatureLevel,
            m_context.GetAddressOf());
    }
#endif

    return SUCCEEDED(result);
}

bool App::CreateWindowSizeResources()
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()))))
        return false;
    if (FAILED(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.GetAddressOf())))
        return false;

    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = m_ClientWidth;
    depthDesc.Height = m_ClientHeight;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(m_device->CreateTexture2D(&depthDesc, nullptr, m_depthTexture.GetAddressOf())))
        return false;
    if (FAILED(m_device->CreateDepthStencilView(m_depthTexture.Get(), nullptr, m_depthStencilView.GetAddressOf())))
        return false;

    return true;
}

bool App::ResizeSwapChain(UINT width, UINT height)
{
    if (m_context)
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
    ReleaseWindowSizeResources();

    if (FAILED(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)))
        return false;

    m_ClientWidth = width;
    m_ClientHeight = height;
    return CreateWindowSizeResources();
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
    m_depthStencilView.Reset();
    m_depthTexture.Reset();
    m_renderTargetView.Reset();
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
