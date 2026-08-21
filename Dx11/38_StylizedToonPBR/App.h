#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "../Common/Animation/Animator.h"
#include "../Common/GameApp.h"
#include "GpuProfiler.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

class FbxModel;

class App final : public GameApp
{
public:
    enum class RenderMode : uint32_t { Pbr, ToonPbr, Split };
    enum class LightingPreset : uint32_t { NeonContrast, IndustrialSoft };
    enum class MaterialProfile : uint32_t { Skin, Hair, Cloth };

    App();

    bool OnInitialize() override;
    void OnUninitialize() override;
    void OnUpdate(const float& dt) override;
    void OnRender() override;
    void OnInputProcess(
        const DirectX::Keyboard::State& keyState,
        const DirectX::Keyboard::KeyboardStateTracker& keyTracker,
        const DirectX::Mouse::State& mouseState,
        const DirectX::Mouse::ButtonStateTracker& mouseTracker) override;
    LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;

private:
    struct alignas(16) CharacterConstants
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4X4 lightViewProjection;
        DirectX::XMFLOAT4 cameraPosition;
        DirectX::XMFLOAT4 lightDirection;
        DirectX::XMFLOAT4 warmKeyTint;
        DirectX::XMFLOAT4 coolShadowTint;
        DirectX::XMFLOAT4 diffuseBandThresholds;
        DirectX::XMFLOAT4 toonParameters;
        DirectX::XMFLOAT4 materialParameters;
        DirectX::XMFLOAT4 shadowParameters;
        DirectX::XMFLOAT4 textureParameters;
    };

    struct alignas(16) PostConstants
    {
        DirectX::XMFLOAT4 inverseResolution;
        DirectX::XMFLOAT4 outlineParameters;
        DirectX::XMFLOAT4 toneMapParameters;
        DirectX::XMFLOAT4 backgroundColor;
    };

    bool CreateDeviceResources();
    bool CreateRenderPipeline();
    bool CreateShadowResources();
    bool CreateFallbackTextures();
    bool CreateWindowSizeResources();
    bool ResizeSwapChain(UINT width, UINT height);
    bool LoadCharacter();
    bool InitializeImGui();
    bool FailInitialization(const wchar_t* message);
    bool CompileShaders();
    bool CreatePipelineStates();
    bool CreateConstantBuffers();

    void ReleaseWindowSizeResources();
    void ShutdownImGui();
    void UnbindShaderResources();
    void SetFullScreenViewportAndScissor();
    void RenderShadowPass();
    void RenderCharacterPass();
    void RenderOutlinePass();
    void RenderToneMapPass();
    void RenderHud();
    void DrawCharacter(RenderMode mode, float projectionAspect, bool shadowOnly);
    void UpdateCharacterConstants(
        RenderMode mode,
        MaterialProfile profile,
        float projectionAspect,
        bool hasBaseColor,
        bool hasMetallic,
        bool hasRoughness,
        bool hasNormal);
    void UpdatePostConstants();
    void BuildMaterialProfiles();
    MaterialProfile ClassifyMaterialName(const char* materialName) const;
    void SelectIdleAnimation();
    void UpdateAnimationPalette();
    void ApplyPreset(LightingPreset preset);

    RenderMode m_renderMode = RenderMode::ToonPbr;
    LightingPreset m_lightingPreset = LightingPreset::NeonContrast;
    std::shared_ptr<FbxModel> m_character;
    std::unique_ptr<CharacterAnimator> m_poseAnimator;
    std::vector<MaterialProfile> m_materialProfiles;

    float m_lowBandThreshold = 0.34f;
    float m_highBandThreshold = 0.69f;
    float m_bandSoftness = 0.055f;
    float m_shadowSoftness = 1.4f;
    float m_hairHighlightStrength = 0.82f;
    float m_rimStrength = 0.38f;
    float m_outlineWidth = 1.35f;
    int m_outlineQuality = 2;
    float m_exposure = 1.08f;
    DirectX::XMFLOAT3 m_shadowTint{ 0.24f, 0.34f, 0.58f };
    DirectX::XMFLOAT3 m_keyTint{ 1.05f, 0.72f, 0.48f };
    bool m_readmeCapture = false;
    bool m_assetManagerCreated = false;
    bool m_imguiInitialized = false;
    bool m_profilerAvailable = false;
    bool m_outlineShaderAvailable = false;
    bool m_outlineAvailable = false;
    double m_cpuFrameMs = 0.0;
    std::chrono::steady_clock::time_point m_cpuFrameStart{};
    UINT m_resourceWidth = 0;
    UINT m_resourceHeight = 0;

    GpuProfiler m_gpuProfiler;

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_hdrTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_hdrRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_hdrShaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_normalProfileTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_normalProfileRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_normalProfileShaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_depthShaderResourceView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_outlineTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_outlineRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_outlineShaderResourceView;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_shadowTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_shadowDepthStencilView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shadowShaderResourceView;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_characterVertexShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_shadowVertexShader;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_fullscreenVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_characterPixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_outlinePixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_toneMapPixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_skinnedInputLayout;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_characterConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_postConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_characterRasterizerState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_shadowRasterizerState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_linearSampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_shadowSampler;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_whiteTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_blackTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_flatNormalTexture;
};
