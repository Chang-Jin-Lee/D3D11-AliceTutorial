#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

struct FbxSubset
{
    uint32_t startIndex = 0;
    uint32_t indexCount = 0;
    uint32_t materialIndex = 0;
};

struct ID3D11Device;
struct ID3D11Buffer;
struct aiScene;
struct aiNodeAnim;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
namespace DirectX { struct XMFLOAT4X4; struct XMMATRIX; }

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
        // name: UTF-8 (검색/매핑 용), nameW: 디버그용
        std::string name;
        std::wstring nameW;
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

    // 스켈레톤 노드의 유니코드 이름 조회
    std::wstring GetSkeletonNodeNameW(int idx) const;

    // GPU 스키닝 본 팔레트 최대 크기
    static constexpr int kMaxBones = 1023;

private:
    struct Impl;
    std::unique_ptr<Impl> m_;

    bool LoadMaterials(ID3D11Device* device, const struct aiScene* scene, const std::wstring& baseDir);
    bool BuildMeshBuffers(ID3D11Device* device, const struct aiScene* scene);
    void EnsureBoneCB(ID3D11Device* device);
    void RebuildChannelMap();

    // --- Animation/Skeleton helpers (for readability & documentation) ---
    // 씬 트리를 복제해 스켈레톤과 이름→인덱스 맵을 구축합니다.
    void BuildSkeletonAndNodeIndex(const struct aiScene* scene);
    // 메시의 본 이름/오프셋 행렬을 수집하고 해당 노드에 isBone을 표시합니다.
    void CollectBonesAndOffsets(const struct aiScene* scene);
    // 메시별 베이스 정점 인덱스 테이블을 만듭니다.
    void BuildBaseVertexTable(const struct aiScene* scene, std::vector<size_t>& baseVertex);
    // 정점당 최대 4개의 본 가중치를 누적합니다.
    void AccumulateVertexWeights(const struct aiScene* scene, const std::vector<size_t>& baseVertex);
    // 가중치를 정규화하고 스키닝 사용 여부를 갱신합니다.
    void NormalizeInfluencesAndFlag();
    // 정점 버퍼에 boneIdx/weight를 반영하고 재업로드합니다.
    void ApplyInfluencesToVB(ID3D11Device* device);
    // 애니메이션 클립 메타데이터(이름/길이/TPS)를 초기화합니다.
    void InitAnimationMetadata(const struct aiScene* scene);
    // 현재 시간에서 노드 글로벌 행렬을 평가(키 보간 포함)합니다.
    void EvaluateGlobalMatrices(const struct aiScene* scene, const std::unordered_map<std::wstring, const struct aiNodeAnim*>& channelOf, std::vector<DirectX::XMFLOAT4X4>& outGlobal) const;
    // 표준 스키닝 팔레트(GlobalInverse * Global * Offset)를 생성합니다.
    void BuildBonePalette(const std::vector<DirectX::XMFLOAT4X4>& global, std::vector<DirectX::XMMATRIX>& outPalette) const;
    // 본 팔레트를 VS 상수 버퍼로 업로드합니다.
    void UploadBonePalette(ID3D11DeviceContext* ctx, const std::vector<DirectX::XMMATRIX>& palette);
};


