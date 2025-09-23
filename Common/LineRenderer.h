#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

class LineRenderer
{
public:
    LineRenderer() = default;
    ~LineRenderer() { Release(); }

    bool Initialize(ID3D11Device* device);
    void Release();

    // Draw single line from p0 to p1 with color
    void DrawLine(ID3D11DeviceContext* ctx,
                  const DirectX::XMFLOAT3& p0,
                  const DirectX::XMFLOAT3& p1,
                  const DirectX::XMFLOAT4& color,
                  ID3D11InputLayout* inputLayout,
                  ID3D11VertexShader* vs,
                  ID3D11PixelShader* ps,
                  ID3D11Buffer* constantBuffer);

    // Draw axis gizmo at origin with given length
    void DrawAxes(ID3D11DeviceContext* ctx,
                  float length,
                  ID3D11InputLayout* inputLayout,
                  ID3D11VertexShader* vs,
                  ID3D11PixelShader* ps,
                  ID3D11Buffer* constantBuffer);

    // Draw symmetric axes centered at origin: [-length, +length] on each axis
    void DrawAxesSymmetric(ID3D11DeviceContext* ctx,
                           float length,
                           ID3D11InputLayout* inputLayout,
                           ID3D11VertexShader* vs,
                           ID3D11PixelShader* ps,
                           ID3D11Buffer* constantBuffer);

    // Draw a line representing light direction from position along normalized dir with given length
    void DrawLightDirection(ID3D11DeviceContext* ctx,
                            const DirectX::XMFLOAT3& lightPos,
                            const DirectX::XMFLOAT3& lightDir,
                            float length,
                            ID3D11InputLayout* inputLayout,
                            ID3D11VertexShader* vs,
                            ID3D11PixelShader* ps,
                            ID3D11Buffer* constantBuffer);

    // Draw small overlay axes in NDC at given anchor (e.g., {-0.9, 0.9}) with length in NDC units
    void DrawAxesOverlay(ID3D11DeviceContext* ctx,
                         const DirectX::XMMATRIX& viewMatrix,
                         const DirectX::XMFLOAT2& anchorNDC,
                         float lengthNDC,
                         ID3D11InputLayout* inputLayout,
                         ID3D11VertexShader* vs,
                         ID3D11PixelShader* ps,
                         ID3D11Buffer* constantBuffer);

private:
    ID3D11Buffer* m_vb = nullptr;
};


