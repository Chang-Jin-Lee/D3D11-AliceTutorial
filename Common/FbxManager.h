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

    // 본 구조를 위한 스켈레톤 노드. 애니메이션 실행을 위해서 만듬
    struct SkeletonNode
    {
        std::string name;
        int parent = -1;
        std::vector<int> children;
        bool isBone = false;
    };

    bool HasSkeleton() const { return m_HasSkinning; }
    bool HasAnimations() const { return m_HasAnimations; }
    const std::vector<SkeletonNode>& GetSkeleton() const { return m_Skeleton; }
    int GetSkeletonRoot() const { return m_RootIndex; }

    const std::vector<std::string>& GetAnimationNames() const { return m_AnimationNames; }
    int GetCurrentAnimationIndex() const { return m_CurrentClip; }
    void SetCurrentAnimation(int idx);
    void SetAnimationPlaying(bool playing) { m_Playing = playing; }
    bool IsAnimationPlaying() const { return m_Playing; }
    double GetAnimationTimeSeconds() const { return m_ClipTimeSec; }
    void SetAnimationTimeSeconds(double t);
    void UpdateAnimation(ID3D11DeviceContext* ctx, double dtSec);
    double GetClipDurationSec(int idx) const { return (idx>=0 && idx<(int)m_ClipDurationSec.size()) ? m_ClipDurationSec[idx] : 0.0; }

private:
    bool LoadMaterials(ID3D11Device* device, const struct aiScene* scene, const std::wstring& baseDir);
    bool BuildMeshBuffers(ID3D11Device* device, const struct aiScene* scene);
    void BuildSkeletonAndWeights(const struct aiScene* scene);
    void BuildSkeletonNodes(const struct aiScene* scene);
    void NormalizeAllWeights();
    void EnsureDynamicVBForSkinning(ID3D11Device* device);
    void SkinAndUpdateVB(ID3D11DeviceContext* ctx);

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

    // Assimp 라이브러리에서 씬을 유지하기 위해 만듬
    class AssimpImporterHolder; // 전방선언
    struct aiScene* m_SceneMutable = nullptr; // m_Importer에 의한 생명주기 관리
    void* m_ImporterOpaque = nullptr; // assimp에서 importer 홀더를 쓰기 위함

    // CPU 측에서 스키닝 데이터를 관리하기 위해 만듬
    struct Influence4 { unsigned short idx[4] = {0,0,0,0}; float w[4] = {0,0,0,0}; };
    std::vector<struct VertexSkinnedTBN> m_BindVertices; // 초기 위치, 본 정보 저장
    std::vector<uint32_t> m_IndicesCPU; // 인덱스 복사
    std::vector<Influence4> m_Influences; //정점당 최대 4개의 영향을 받음

    // 본
    bool m_HasSkinning = false;
    std::vector<std::string> m_BoneNames;
    std::vector<DirectX::XMFLOAT4X4> m_BoneOffset; // aiBone::mOffsetMatrix 본 오프셋
    std::unordered_map<std::string, int> m_BoneIndexOfName;
    std::unordered_map<std::string, int> m_NodeIndexOfName;

    // 본 구조를 위한 스켈레톤 트리
    std::vector<SkeletonNode> m_Skeleton;
    int m_RootIndex = -1;
    DirectX::XMFLOAT4X4 m_GlobalInverse = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

    // 애니메이션 상태
    bool m_HasAnimations = false;
    std::vector<std::string> m_AnimationNames;
    std::vector<double> m_ClipDurationSec;
    std::vector<double> m_ClipTicksPerSec;
    int m_CurrentClip = -1;
    double m_ClipTimeSec = 0.0;
    bool m_Playing = false;

    // GPU 스키닝 본 팔레트를 위한 상수 버퍼
    static constexpr int kMaxBones = 1023; // 64*1023 + 16 = 65488 bytes (<= 64KB)
    ID3D11Buffer* m_pBoneCB = nullptr;
public:
    ID3D11Buffer* GetBoneConstantBuffer() const { return m_pBoneCB; }
    UINT GetBoneCount() const { return (UINT)m_BoneNames.size(); }
private:
    void EnsureBoneCB(ID3D11Device* device);
};


