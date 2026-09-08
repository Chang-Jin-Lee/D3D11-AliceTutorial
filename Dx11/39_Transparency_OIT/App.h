#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <memory>
#include <string>
#include <vector>
#include "../Common/GameApp.h"
#include "OitPipeline.h"
#include "SceneTransparency.h"

class FbxModel;

class App final : public GameApp
{
public:
    App();
    ~App() override;
    bool OnInitialize() override;
    void OnUninitialize() override;
    void OnUpdate(const float& dt) override;
    void OnRender() override;
    void OnInputProcess(const DirectX::Keyboard::State&, const DirectX::Keyboard::KeyboardStateTracker&,
                        const DirectX::Mouse::State&, const DirectX::Mouse::ButtonStateTracker&) override;
    LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM) override;

private:
    struct Material
    {
        std::string name;
        Transparency39::AlphaMode mode{};
        float cutoff{};
        DirectX::XMFLOAT4 factor{ 1,1,1,1 };
        bool doubleSided{};
        bool decodeSrgb{};
    };
    struct Plane
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4 color;
        bool transparent;
    };
    struct Draw
    {
        bool character;
        std::size_t index;
        Transparency39::EffectiveMaterial material;
    };
    struct alignas(16) SurfaceConstants
    {
        DirectX::XMFLOAT4X4 world, view, projection;
        DirectX::XMFLOAT4 baseColor;
        // x: effective alpha mode, y: cutoff, z: opacity, w: use skinning.
        DirectX::XMFLOAT4 material;
        // x: near, y: far, z: has base texture, w: decode texture sRGB.
        DirectX::XMFLOAT4 surface;
    };
    bool CreateDevice();
    bool CreateSurfaceResources();
    bool LoadCharacter();
    bool BuildPosedCentroids();
    bool Resize(UINT width, UINT height);
    bool Fail(const wchar_t* operation, HRESULT hr = E_FAIL);
    void DrawSurface(const Draw& draw, bool oit);
    void BuildDraws();
    void RenderHud();
    void HandleKey(WPARAM key);
    float SceneWidth() const;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backbuffer_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> surfaceVs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> surfacePs_, oitPs_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> layout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants_, planeVertices_, planeIndices_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> cullBack_, cullNone_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    Transparency39::OitPipeline pipeline_;
    std::unique_ptr<FbxModel> character_;
    std::vector<Material> materials_;
    std::vector<DirectX::XMFLOAT3> centroids_;
    std::vector<Plane> planes_;
    std::vector<Draw> draws_;
    std::vector<std::size_t> opaque_;
    std::vector<Transparency39::TransparentDraw> transparent_;
    DirectX::XMFLOAT4X4 characterWorld_, view_, projection_;
    Transparency39::Mode mode_{ Transparency39::Mode::WeightedOit };
    Transparency39::DebugView debugView_{ Transparency39::DebugView::Composite };
    Transparency39::MaterialSettings settings_;
    int scene_{}; // Both / Character / Planes
    bool laceValid_{}, reverse_{}, orbit_{}, closeUp_{}, minimized_{}, failed_{}, imgui_{};
    bool capture_{};
    float orbitAngle_{}, exposure_{ 1 };
    float background_[4]{ 0.075f, 0.095f, 0.13f, 1 };
    double sortMs_{};
    std::string poseName_;
};
