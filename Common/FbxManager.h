#pragma once

#include <string>
#include <vector>
#include <memory>

struct FbxSubset
{
    uint32_t startIndex = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

class ID3D11Device;
class ID3D11Buffer;
struct aiScene;
class ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

class FbxManager
{
public:
    FbxManager();
    ~FbxManager();

    bool Load(ID3D11Device* device, const std::wstring& pathW);
    void Release();

    // Queries
    bool HasMesh() const;
    ID3D11Buffer* GetVertexBuffer() const;
    ID3D11Buffer* GetIndexBuffer() const;
    int GetIndexCount() const;
    UINT GetVertexStride() const;
    UINT GetVertexOffset() const;

    const std::vector<FbxSubset>& GetSubsets() const;
    const std::vector<ID3D11ShaderResourceView*>& GetMaterialSRVs() const;

    // 본 구조를 위한 스켈레톤 노드. 애니메이션 실행을 위해서 만듬
    struct SkeletonNode
    {
        std::string name;
        int parent = -1;
        std::vector<int> children;
        bool isBone = false;
    };

    bool HasSkeleton() const;
    bool HasAnimations() const;
    const std::vector<SkeletonNode>& GetSkeleton() const;
    int GetSkeletonRoot() const;

    const std::vector<std::string>& GetAnimationNames() const;
    int GetCurrentAnimationIndex() const;
    void SetCurrentAnimation(int idx);
    void SetAnimationPlaying(bool playing);
    bool IsAnimationPlaying() const;
    double GetAnimationTimeSeconds() const;
    void SetAnimationTimeSeconds(double t);
    void UpdateAnimation(ID3D11DeviceContext* ctx, double dtSec);
    double GetClipDurationSec(int idx) const;

    ID3D11Buffer* GetBoneConstantBuffer() const;
    UINT GetBoneCount() const;

    // GPU 스키닝 본 팔레트 최대 크기
    static constexpr int kMaxBones = 1023;

private:
    struct Impl;
    std::unique_ptr<Impl> m_;

    bool LoadMaterials(ID3D11Device* device, const struct aiScene* scene, const std::wstring& baseDir);
    bool BuildMeshBuffers(ID3D11Device* device, const struct aiScene* scene);
    void EnsureBoneCB(ID3D11Device* device);
    void RebuildChannelMap();
};


