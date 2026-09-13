#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <memory>
#include <string>
#include <vector>

#include "../Common/GameApp.h"
#include "CoverageScene.h"
#include "MsaaPipeline.h"

class FbxModel;

namespace Coverage40
{
class App final : public GameApp
{
public:
    App();
    ~App() override;

    bool OnInitialize() override;
    void OnUninitialize() override;
    void OnUpdate(const float& dt) override;
    void OnRender() override;
    void OnInputProcess(const DirectX::Keyboard::State&,
        const DirectX::Keyboard::KeyboardStateTracker&,
        const DirectX::Mouse::State&,
        const DirectX::Mouse::ButtonStateTracker&) override;
    LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM) override;

private:
    enum class Scene { Character, Diagnostic };

    struct Material
    {
        std::string name;
        AlphaMode mode{};
        float cutoff{};
        DirectX::XMFLOAT4 factor{ 1, 1, 1, 1 };
        bool doubleSided{};
        bool decodeSrgb{};
    };

    struct DiagnosticSurface
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4 color;
        bool pattern{};
        bool doubleSided{};
    };

    struct Draw
    {
        bool character{};
        std::size_t index{};
        EffectiveMaterial material;
    };

    struct alignas(16) SurfaceConstants
    {
        DirectX::XMFLOAT4X4 world, view, projection;
        DirectX::XMFLOAT4 baseColor;
        DirectX::XMFLOAT4 material;
        DirectX::XMFLOAT4 surface;
    };
    static_assert(sizeof(SurfaceConstants) % 16 == 0);

    bool CreateDevice();
    bool CreateSurfaceResources();
    bool LoadCharacter();
    bool BuildPosedCentroids();
    bool Resize(UINT width, UINT height);
    void BuildDraws();
    void DrawSurface(const Draw& draw);
    void RenderHud();
    void HandleKey(WPARAM key);
    bool Fail(const wchar_t* operation, HRESULT hr = E_FAIL);

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backbuffer_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> surfaceVs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> surfacePs_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> diagnosticVertices_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> diagnosticIndices_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> diagnosticPattern_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> cullBack_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> cullNone_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    MsaaPipeline pipeline_;
    std::unique_ptr<FbxModel> character_;
    std::vector<Material> materials_;
    std::vector<DirectX::XMFLOAT3> centroids_;
    std::vector<DiagnosticSurface> diagnosticSurfaces_;
    std::vector<Draw> opaqueDraws_;
    std::vector<Draw> draws_;
    std::vector<TransparentDraw> transparentDraws_;
    DirectX::XMFLOAT4X4 characterWorld_, view_, projection_;
    Settings settings_;
    Scene scene_{ Scene::Character };
    bool laceValid_{};
    bool orbit_{};
    bool closeUp_{ true };
    bool minimized_{};
    bool targetsMatch_{};
    bool failed_{};
    bool imgui_{};
    bool capture_{};
    float orbitAngle_{};
    float exposure_{ 1.0f };
    float background_[4]{ 0.075f, 0.095f, 0.13f, 1.0f };
    std::string poseName_;
    std::string comparisonReason_;
    std::string configurationError_;
    HRESULT configurationHr_{ S_OK };
};
}
