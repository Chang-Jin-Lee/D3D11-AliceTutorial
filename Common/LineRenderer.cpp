#include "pch.h"
#include "LineRenderer.h"

using namespace DirectX;

struct LineV { XMFLOAT3 pos; XMFLOAT3 normal; XMFLOAT4 color; };

bool LineRenderer::Initialize(ID3D11Device* device)
{
    if (m_vb) return true;
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(LineV) * 2;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED(device->CreateBuffer(&bd, nullptr, &m_vb));
}

void LineRenderer::Release()
{
    if (m_vb) { m_vb->Release(); m_vb = nullptr; }
}

void LineRenderer::DrawLine(ID3D11DeviceContext* ctx,
                            const XMFLOAT3& p0,
                            const XMFLOAT3& p1,
                            const XMFLOAT4& color,
                            ID3D11InputLayout* inputLayout,
                            ID3D11VertexShader* vs,
                            ID3D11PixelShader* ps,
                            ID3D11Buffer* constantBuffer)
{
    if (!m_vb) return;
    LineV line[2] = {};
    line[0].pos = p0; line[1].pos = p1;
    line[0].normal = XMFLOAT3(0,1,0); line[1].normal = XMFLOAT3(0,1,0);
    line[0].color = color; line[1].color = color;

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(ctx->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, line, sizeof(line));
        ctx->Unmap(m_vb, 0);
    }

    UINT stride = sizeof(LineV);
    UINT offset = 0;
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ctx->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
    ctx->IASetInputLayout(inputLayout);

    // use existing constant buffer, set pad=3 for red lines or keep as is for axes
    // caller should set appropriate constants if necessary
    ctx->VSSetConstantBuffers(0, 1, &constantBuffer);
    ctx->PSSetConstantBuffers(0, 1, &constantBuffer);
    ctx->VSSetShader(vs, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);
    ctx->Draw(2, 0);
}

void LineRenderer::DrawAxes(ID3D11DeviceContext* ctx,
                            float length,
                            ID3D11InputLayout* inputLayout,
                            ID3D11VertexShader* vs,
                            ID3D11PixelShader* ps,
                            ID3D11Buffer* constantBuffer)
{
    // X (red)
    DrawLine(ctx, XMFLOAT3(0,0,0), XMFLOAT3(length,0,0), XMFLOAT4(1,0,0,1), inputLayout, vs, ps, constantBuffer);
    // Y (green)
    DrawLine(ctx, XMFLOAT3(0,0,0), XMFLOAT3(0,length,0), XMFLOAT4(0,1,0,1), inputLayout, vs, ps, constantBuffer);
    // Z (blue)
    DrawLine(ctx, XMFLOAT3(0,0,0), XMFLOAT3(0,0,length), XMFLOAT4(0,0,1,1), inputLayout, vs, ps, constantBuffer);
}

void LineRenderer::DrawAxesSymmetric(ID3D11DeviceContext* ctx,
                                     float length,
                                     ID3D11InputLayout* inputLayout,
                                     ID3D11VertexShader* vs,
                                     ID3D11PixelShader* ps,
                                     ID3D11Buffer* constantBuffer)
{
    // X axis
    DrawLine(ctx, XMFLOAT3(-length,0,0), XMFLOAT3(length,0,0), XMFLOAT4(1,0,0,1), inputLayout, vs, ps, constantBuffer);
    // Y axis
    DrawLine(ctx, XMFLOAT3(0,-length,0), XMFLOAT3(0,length,0), XMFLOAT4(0,1,0,1), inputLayout, vs, ps, constantBuffer);
    // Z axis
    DrawLine(ctx, XMFLOAT3(0,0,-length), XMFLOAT3(0,0,length), XMFLOAT4(0,0,1,1), inputLayout, vs, ps, constantBuffer);
}

void LineRenderer::DrawLightDirection(ID3D11DeviceContext* ctx,
                                      const XMFLOAT3& lightPos,
                                      const XMFLOAT3& lightDir,
                                      float length,
                                      ID3D11InputLayout* inputLayout,
                                      ID3D11VertexShader* vs,
                                      ID3D11PixelShader* ps,
                                      ID3D11Buffer* constantBuffer)
{
    XMVECTOR p0 = XMLoadFloat3(&lightPos);
    XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&lightDir));
    XMVECTOR p1 = XMVectorAdd(p0, XMVectorScale(dir, length));
    XMFLOAT3 a; XMStoreFloat3(&a, p0);
    XMFLOAT3 b; XMStoreFloat3(&b, p1);
    DrawLine(ctx, a, b, XMFLOAT4(1,0,0,1), inputLayout, vs, ps, constantBuffer);
}

void LineRenderer::DrawAxesOverlay(ID3D11DeviceContext* ctx,
                                   const XMMATRIX& viewMatrix,
                                   const XMFLOAT2& anchorNDC,
                                   float lengthNDC,
                                   ID3D11InputLayout* inputLayout,
                                   ID3D11VertexShader* vs,
                                   ID3D11PixelShader* ps,
                                   ID3D11Buffer* constantBuffer)
{
    // Caller sets CB to identity; we draw NDC lines derived from world axes transformed by view rotation
    XMFLOAT3 o = XMFLOAT3(anchorNDC.x, anchorNDC.y, 0.0f);

    // Transform canonical world axes by view rotation (ignore translation)
    XMVECTOR ex = XMVectorSet(1,0,0,0);
    XMVECTOR ey = XMVectorSet(0,1,0,0);
    XMVECTOR ez = XMVectorSet(0,0,1,0);
    XMVECTOR vx = XMVector3TransformNormal(ex, viewMatrix);
    XMVECTOR vy = XMVector3TransformNormal(ey, viewMatrix);
    XMVECTOR vz = XMVector3TransformNormal(ez, viewMatrix);
    // Use XY components to form 2D directions in NDC, normalize
    auto to2D = [](XMVECTOR v) {
        XMFLOAT3 f; XMStoreFloat3(&f, v);
        XMFLOAT2 d = XMFLOAT2(f.x, f.y);
        float len = sqrtf(d.x*d.x + d.y*d.y);
        if (len < 1e-5f) len = 1.0f;
        d.x /= len; d.y /= len;
        return d;
    };
    XMFLOAT2 dx = to2D(vx);
    XMFLOAT2 dy = to2D(vy);
    XMFLOAT2 dz = to2D(vz);

    DrawLine(ctx, o, XMFLOAT3(o.x + dx.x * lengthNDC, o.y + dx.y * lengthNDC, 0.0f), XMFLOAT4(1,0,0,1), inputLayout, vs, ps, constantBuffer); // X
    DrawLine(ctx, o, XMFLOAT3(o.x + dy.x * lengthNDC, o.y + dy.y * lengthNDC, 0.0f), XMFLOAT4(0,1,0,1), inputLayout, vs, ps, constantBuffer); // Y
    DrawLine(ctx, o, XMFLOAT3(o.x + dz.x * lengthNDC, o.y + dz.y * lengthNDC, 0.0f), XMFLOAT4(0,0,1,1), inputLayout, vs, ps, constantBuffer); // Z
}


