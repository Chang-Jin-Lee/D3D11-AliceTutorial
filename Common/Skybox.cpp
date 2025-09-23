#include "pch.h"
#include "Skybox.h"
#include <directxtk/DDSTextureLoader.h>

using namespace DirectX;

bool Skybox::Initialize(ID3D11Device* device,
                        const wchar_t* ddsPath,
                        ID3D11VertexShader* vs,
                        ID3D11PixelShader* ps,
                        ID3D11InputLayout* inputLayout,
                        ID3D11Buffer* sharedConstantBuffer)
{
    m_vs = vs; m_ps = ps; m_inputLayout = inputLayout; m_constantBuffer = sharedConstantBuffer;
    if (FAILED(CreateDDSTextureFromFile(device, ddsPath, nullptr, &m_srv))) return false;

    if (!m_sampler)
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        if (FAILED(device->CreateSamplerState(&sd, &m_sampler))) return false;
    }

    if (!m_ds)
    {
        D3D11_DEPTH_STENCIL_DESC dsd{};
        dsd.DepthEnable = TRUE;
        dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        dsd.StencilEnable = FALSE;
        if (FAILED(device->CreateDepthStencilState(&dsd, &m_ds))) return false;
    }

    if (!m_rs)
    {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.FrontCounterClockwise = false;
        rd.DepthClipEnable = TRUE;
        if (FAILED(device->CreateRasterizerState(&rd, &m_rs))) return false;
    }

    return true;
}

bool Skybox::ChangeDDS(ID3D11Device* device, const wchar_t* ddsPath)
{
    if (m_srv) { m_srv->Release(); m_srv = nullptr; }
    return SUCCEEDED(CreateDDSTextureFromFile(device, ddsPath, nullptr, &m_srv));
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
    ctx->RSGetState(&prevRS);
    ctx->OMGetDepthStencilState(&prevDS, &prevRef);

    ctx->OMSetDepthStencilState(m_ds, 0);
    ctx->RSSetState(m_rs);

    // build wvp with view translation removed
    XMMATRIX view = XMMatrixTranspose(viewT);
    view.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, XMVectorGetW(view.r[3]));
    XMMATRIX viewNoTransT = XMMatrixTranspose(view);
    XMMATRIX wvpT = XMMatrixMultiply(projT, viewNoTransT);

    // write into shared constant buffer b0 (leading 64 bytes)
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &wvpT, sizeof(XMMATRIX));
        ctx->Unmap(m_constantBuffer, 0);
    }

    UINT stride = vertexStride;
    UINT offset = vertexOffset;
    ctx->IASetInputLayout(m_inputLayout);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
    ctx->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
    ctx->VSSetShader(m_vs, nullptr, 0);
    ctx->PSSetShader(m_ps, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &m_sampler);
    ctx->PSSetShaderResources(0, 1, &m_srv);
    ctx->VSSetConstantBuffers(0, 1, &m_constantBuffer);
    ctx->PSSetConstantBuffers(0, 1, &m_constantBuffer);
    ctx->DrawIndexed(indexCount, 0, 0);

    // restore
    ctx->OMSetDepthStencilState(prevDS, prevRef);
    ctx->RSSetState(prevRS);
    if (prevDS) prevDS->Release();
    if (prevRS) prevRS->Release();
}

void Skybox::Release()
{
    if (m_sampler) { m_sampler->Release(); m_sampler = nullptr; }
    if (m_ds) { m_ds->Release(); m_ds = nullptr; }
    if (m_rs) { m_rs->Release(); m_rs = nullptr; }
    if (m_srv) { m_srv->Release(); m_srv = nullptr; }
}

#include "pch.h"
#include "Skybox.h"
