#pragma once

#include <d3d11.h>
#include <string>
#include <vector>
#include <wrl/client.h>
#include <unordered_map>

struct FbxSubset
{
    uint32_t startIndex = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

class FbxManager
{
public:
    FbxManager() = default;
    ~FbxManager() = default;

    bool Load(ID3D11Device* device, const std::wstring& pathW);
    void Release();

    // Queries
    bool HasMesh() const { return m_pVB != nullptr && m_pIB != nullptr && m_IndexCount > 0; }
    ID3D11Buffer* GetVertexBuffer() const { return m_pVB; }
    ID3D11Buffer* GetIndexBuffer() const { return m_pIB; }
    int GetIndexCount() const { return m_IndexCount; }
    UINT GetVertexStride() const { return (UINT)m_VertexStride; }
    UINT GetVertexOffset() const { return 0; }

    const std::vector<FbxSubset>& GetSubsets() const { return m_Subsets; }
    const std::vector<ID3D11ShaderResourceView*>& GetMaterialSRVs() const { return m_MaterialSRVs; }

private:
    bool LoadMaterials(ID3D11Device* device, const struct aiScene* scene, const std::wstring& baseDir);
    bool BuildMeshBuffers(ID3D11Device* device, const struct aiScene* scene);

private:
    ID3D11Buffer* m_pVB = nullptr;
    ID3D11Buffer* m_pIB = nullptr;
    int m_IndexCount = 0;
    size_t m_VertexStride = 0;

    std::vector<FbxSubset> m_Subsets;
    std::vector<ID3D11ShaderResourceView*> m_MaterialSRVs;

    // Texture cache and fallbacks
    std::unordered_map<std::wstring, ID3D11ShaderResourceView*> m_TexCache;
    ID3D11ShaderResourceView* m_pWhite = nullptr;
};


