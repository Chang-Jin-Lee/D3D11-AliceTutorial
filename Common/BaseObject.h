#pragma once
#include <string>
#include <memory>
#include <vector>
#include <directxtk/SimpleMath.h>
#include "Transform.h"
#include <d3d11.h>

enum class ECubeType { Basic, Texture };
enum class ObjectKind { Cube, Model };

class BaseObject
{
public:
    virtual ~BaseObject() = default;
    ObjectKind kind{ ObjectKind::Cube };
    std::wstring name;
};

class CubeObject final : public BaseObject
{
public:
    CubeObject() { kind = ObjectKind::Cube; }
    CubeObject(const std::wstring& n, const Transform& t, ECubeType tp)
        : cubeTransform(t), cubeType(tp) { kind = ObjectKind::Cube; name = n; }
    Transform cubeTransform{};
    ECubeType cubeType{ ECubeType::Basic };
    // 간단 머티리얼(모델과 동일 레이아웃: ambient/diffuse/specular/reflect)
    DirectX::XMFLOAT4 matAmbient {1,1,1,1};
    DirectX::XMFLOAT4 matDiffuse {1,1,1,1};
    DirectX::XMFLOAT4 matSpecular{1,1,1,32}; // w = shininess
    DirectX::XMFLOAT4 matReflect {0,0,0,0};

    // 텍스처 경로/리소스(있을 때만 사용)
    const wchar_t* facePaths[6]    = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    const wchar_t* normalPaths[6]  = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    const wchar_t* specularPaths[6]= { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    ID3D11ShaderResourceView* faceSRV[6]   = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    ID3D11ShaderResourceView* normalSRV[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    ID3D11ShaderResourceView* specSRV[6]   = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    void LoadTexture2DAt(ID3D11Device* m_pDevice, const int& n, const std::wstring& path);
    void LoadTextureNormalAt(ID3D11Device* m_pDevice, const int& n, const std::wstring& path);
    void LoadTextureSpecularAt(ID3D11Device* m_pDevice, const int& n, const std::wstring& path);
};

class ModelObject final : public BaseObject
{
public:
    ModelObject() { kind = ObjectKind::Model; }
    ModelObject(const std::wstring& n, int idx)
        : modelIndex(idx) { kind = ObjectKind::Model; name = n; }
    int modelIndex{ -1 };
};


