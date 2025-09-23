#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

class Skybox
{
public:
    bool Initialize(ID3D11Device* device,
                    const wchar_t* ddsPath,
                    ID3D11VertexShader* vs,
                    ID3D11PixelShader* ps,
                    ID3D11InputLayout* inputLayout,
                    ID3D11Buffer* sharedConstantBuffer);

    bool ChangeDDS(ID3D11Device* device, const wchar_t* ddsPath);

    void Render(ID3D11DeviceContext* ctx,
                ID3D11Buffer* vertexBuffer,
                ID3D11Buffer* indexBuffer,
                UINT indexCount,
                UINT vertexStride,
                UINT vertexOffset,
                const DirectX::XMMATRIX& viewT,
                const DirectX::XMMATRIX& projT);

    void Release();

    ID3D11ShaderResourceView* GetTexture() const { return m_srv; }

private:
    ID3D11ShaderResourceView* m_srv = nullptr;
    ID3D11VertexShader* m_vs = nullptr;
    ID3D11PixelShader*  m_ps = nullptr;
    ID3D11InputLayout*  m_inputLayout = nullptr;
    ID3D11Buffer*       m_constantBuffer = nullptr; // shared b0
    ID3D11SamplerState* m_sampler = nullptr;
    ID3D11DepthStencilState* m_ds = nullptr;
    ID3D11RasterizerState* m_rs = nullptr;
};