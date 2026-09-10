#pragma once

#include <cstdint>
#include <string>

#include <d3d11.h>
#include <wrl/client.h>

#include "CoverageScene.h"

namespace Coverage40
{
struct Capabilities
{
    bool supports4x{};
    std::wstring reason;
};

class MsaaPipeline
{
public:
    bool Initialize(ID3D11Device* device, const std::wstring& shaderDirectory);
    bool Configure(ID3D11Device* device, UINT width, UINT height, Mode mode);
    void BeginFrame(ID3D11DeviceContext* context, const float clearColor[4]);
    void BeginSurface(ID3D11DeviceContext* context, SurfaceRule rule);
    void EndScene(ID3D11DeviceContext* context);
    void Resolve(ID3D11DeviceContext* context);
    void Present(ID3D11DeviceContext* context, ID3D11RenderTargetView* output,
                 float exposure);

    const Capabilities& Support() const { return support_; }
    Mode CurrentMode() const { return mode_; }
    UINT Width() const { return width_; }
    UINT Height() const { return height_; }
    UINT Samples() const { return SampleCount(mode_); }
    std::uint64_t MemoryBytes() const { return TargetBytes(width_, height_, Samples()); }
    ID3D11Texture2D* ColorTexture() const { return colorTexture_.Get(); }
    ID3D11Texture2D* DepthTexture() const { return depthTexture_.Get(); }
    ID3D11Texture2D* LinearTexture() const;

private:
    Capabilities support_;
    Mode mode_{ Mode::AlphaTest1x };
    UINT width_{};
    UINT height_{};
    bool initialized_{};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> colorTexture_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> resolvedTexture_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> colorTarget_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthTarget_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> linearView_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> opaqueBlend_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> alphaToCoverageBlend_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> alphaBlend_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthWrite_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthReadOnly_;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthDisabled_;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> presentRasterizer_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> presentVertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> presentPixelShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> presentConstants_;
};
}
