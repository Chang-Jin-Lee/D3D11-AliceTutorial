#pragma once

#include "../Common/GameApp.h"

#include <d3d11.h>
#include <wrl/client.h>

class App final : public GameApp
{
public:
    App();

    bool OnInitialize() override;
    void OnUninitialize() override;
    void OnUpdate(const float& dt) override;
    void OnRender() override;
    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;

private:
    bool CreateDeviceResources();
    bool CreateWindowSizeResources();
    bool ResizeSwapChain(UINT width, UINT height);
    bool InitializeImGui();
    bool FailInitialization(const wchar_t* message);
    void ReleaseWindowSizeResources();
    void ShutdownImGui();

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;
    bool m_imguiInitialized = false;
};
