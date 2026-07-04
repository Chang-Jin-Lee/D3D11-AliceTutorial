#include "pch.h"
#include "Skybox.h"
#include <directxtk/DDSTextureLoader.h>
#include <d3d11.h>

using namespace DirectX;

class Skybox::Impl
{
public:
    Impl() {}
    ~Impl() = default;

    ID3D11ShaderResourceView* m_srv = nullptr;
    ID3D11VertexShader* m_vs = nullptr;
    ID3D11PixelShader* m_ps = nullptr;
    ID3D11InputLayout* m_inputLayout = nullptr;
    ID3D11Buffer* m_constantBuffer = nullptr; // shared b0
    ID3D11SamplerState* m_sampler = nullptr;
    ID3D11DepthStencilState* m_ds = nullptr;
    ID3D11RasterizerState* m_rs = nullptr;
};

Skybox::Skybox() : pImpl(std::make_unique<Impl>()) {}

Skybox::Skybox(ID3D11Device* device, const wchar_t* ddsPath, ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11InputLayout* inputLayout, ID3D11Buffer* sharedConstantBuffer)
    : pImpl(std::make_unique<Impl>())
{
	Initialize(device, ddsPath, vs, ps, inputLayout, sharedConstantBuffer);
}

Skybox::~Skybox()
{
}

bool Skybox::Initialize(ID3D11Device* device,
                        const wchar_t* ddsPath,
                        ID3D11VertexShader* vs,
                        ID3D11PixelShader* ps,
                        ID3D11InputLayout* inputLayout,
                        ID3D11Buffer* sharedConstantBuffer)
{
    pImpl->m_vs = vs; pImpl->m_ps = ps; pImpl->m_inputLayout = inputLayout; pImpl->m_constantBuffer = sharedConstantBuffer;
    if (FAILED(CreateDDSTextureFromFile(device, ddsPath, nullptr, &pImpl->m_srv))) return false;

    if (!pImpl->m_sampler)
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&sd, &pImpl->m_sampler))) return false;
    }

    if (!pImpl->m_ds)
    {
        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        dsd.StencilEnable = FALSE;
        if (FAILED(device->CreateDepthStencilState(&dsd, &pImpl->m_ds))) return false;
    }

    if (!pImpl->m_rs)
    {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.FrontCounterClockwise = false;
        rd.DepthClipEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&rd, &pImpl->m_rs))) return false;
    }

    return true;
}

bool Skybox::ChangeDDS(ID3D11Device* device, const wchar_t* ddsPath)
{
    ID3D11ShaderResourceView* nextSRV = nullptr;
    if (FAILED(CreateDDSTextureFromFile(device, ddsPath, nullptr, &nextSRV)))
    {
        return false;
    }

    if (pImpl->m_srv) { pImpl->m_srv->Release(); pImpl->m_srv = nullptr; }
    pImpl->m_srv = nextSRV;
    return true;
}

void Skybox::Render(ID3D11DeviceContext* ctx,
                    ID3D11Buffer* vertexBuffer,
                    ID3D11Buffer* indexBuffer,
                    UINT indexCount,
                    UINT vertexStride,
                    UINT vertexOffset,
                    const XMMATRIX& viewT,
                    const XMMATRIX& projT)
{
    ID3D11RasterizerState* prevRS = nullptr;
    ID3D11DepthStencilState* prevDS = nullptr; UINT prevRef = 0;
    ID3D11ShaderResourceView* prevSRV0 = nullptr; // preserve PS t0 to avoid dimension mismatch in other passes
    ctx->RSGetState(&prevRS);
    ctx->OMGetDepthStencilState(&prevDS, &prevRef);
    ctx->PSGetShaderResources(0, 1, &prevSRV0);

    ctx->OMSetDepthStencilState(pImpl->m_ds, 0);
    ctx->RSSetState(pImpl->m_rs);

    // build wvp with view translation removed
    XMMATRIX view = XMMatrixTranspose(viewT);
    view.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, XMVectorGetW(view.r[3]));
    XMMATRIX viewNoTransT = XMMatrixTranspose(view);
    XMMATRIX wvpT = XMMatrixMultiply(projT, viewNoTransT);

    // write into shared constant buffer b0 (leading 64 bytes)
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(pImpl->m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &wvpT, sizeof(XMMATRIX));
        ctx->Unmap(pImpl->m_constantBuffer, 0);
    }

    UINT stride = vertexStride;
    UINT offset = vertexOffset;
    ctx->IASetInputLayout(pImpl->m_inputLayout);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    ctx->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    ctx->VSSetShader(pImpl->m_vs, nullptr, 0);
    ctx->PSSetShader(pImpl->m_ps, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &pImpl->m_sampler);
    ctx->PSSetShaderResources(0, 1, &pImpl->m_srv);
    ctx->VSSetConstantBuffers(0, 1, &pImpl->m_constantBuffer);
    ctx->PSSetConstantBuffers(0, 1, &pImpl->m_constantBuffer);
    ctx->DrawIndexed(indexCount, 0, 0);

    // restore
    ctx->OMSetDepthStencilState(prevDS, prevRef);
    ctx->RSSetState(prevRS);
    // restore previous PS t0 SRV (skybox binds a TextureCube to t0)
    ctx->PSSetShaderResources(0, 1, &prevSRV0);
    if (prevSRV0) prevSRV0->Release();
    if (prevDS) prevDS->Release();
    if (prevRS) prevRS->Release();
}

void Skybox::Release()
{
    if (pImpl->m_sampler) { pImpl->m_sampler->Release(); pImpl->m_sampler = nullptr; }
    if (pImpl->m_ds) { pImpl->m_ds->Release(); pImpl->m_ds = nullptr; }
    if (pImpl->m_rs) { pImpl->m_rs->Release(); pImpl->m_rs = nullptr; }
    if (pImpl->m_srv) { pImpl->m_srv->Release(); pImpl->m_srv = nullptr; }
}

ID3D11ShaderResourceView* Skybox::GetTexture() const
{
    return pImpl->m_srv;
}
