#pragma once

#include <memory>
#include <DirectXMath.h>

class ID3D11Device;
class ID3D11DeviceContext;
class ID3D11VertexShader;
class ID3D11PixelShader;
class ID3D11InputLayout;
class ID3D11Buffer;
class ID3D11ShaderResourceView;

class Skybox
{
public:
    Skybox();
    Skybox(ID3D11Device* device,
        const wchar_t* ddsPath,
        ID3D11VertexShader* vs,
        ID3D11PixelShader* ps,
        ID3D11InputLayout* inputLayout,
        ID3D11Buffer* sharedConstantBuffer);
    ~Skybox();
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

    ID3D11ShaderResourceView* GetTexture() const;

private:
    class Impl;
	std::unique_ptr<Impl> pImpl;
};