#pragma once

#include <d3d11.h>
#include <string>
#include <vector>
#include <wrl/client.h>
#include "StaticMesh.h" // VertexData
#include "Vertex.h"
#include <unordered_map>

struct ID3D11DeviceContext;
namespace DirectX { struct XMFLOAT4X4; struct XMMATRIX; }

// PMX 서브셋 정보 (머티리얼 인덱스별 드로우 범위)
struct PmxSubset
{
	uint32_t startIndex = 0;
	uint32_t indexCount = 0;
	uint32_t materialIndex = 0;
};

class PmxManager
{
public:
	// 상태
	PmxManager() = default;
	~PmxManager() = default;

	// PMX 로드: 디바이스와 파일 경로를 받아 VB/IB/서브셋/텍스처 SRV를 준비
	bool Load(ID3D11Device* device, const std::wstring& pmxPath);
	void Release();

	// 질의
	bool HasMesh() const { return m_pVB != nullptr && m_pIB != nullptr && m_IndexCount > 0; }
	ID3D11Buffer* GetVertexBuffer() const { return m_pVB; }
	ID3D11Buffer* GetIndexBuffer() const { return m_pIB; }
	int GetIndexCount() const { return m_IndexCount; }
	UINT GetVertexStride() const { return (UINT)m_VertexStride; }
	UINT GetVertexOffset() const { return 0; }

	const std::vector<PmxSubset>& GetSubsets() const { return m_Subsets; }
	const std::vector<ID3D11ShaderResourceView*>& GetMaterialSRVs() const { return m_MaterialSRVs; }

	// --- 스켈레톤/애니메이션 (FBX 매니저와 유사 API) ---
	struct SkeletonNode
	{
		std::string name;           // UTF-8 name
		std::wstring nameW;         // 디버그 표시용
		int parent = -1;
		std::vector<int> children;
		bool isBone = false;
	};

	bool HasSkeleton() const { return m_HasSkinning; }
	bool HasAnimations() const { return m_HasVMD; }
	const std::vector<SkeletonNode>& GetSkeleton() const { return m_Skeleton; }
	int GetSkeletonRoot() const { return m_RootIndex; }

	// VMD 로딩/재생 제어
	bool LoadVMD(ID3D11Device* device, const std::wstring& vmdPath);
	void SetAnimationPlaying(bool playing) { m_Playing = playing; }
	bool IsAnimationPlaying() const { return m_Playing; }
	void SetAnimationTimeSeconds(double t);
	double GetAnimationTimeSeconds() const { return m_AnimTimeSec; }
	void UpdateAnimation(ID3D11DeviceContext* ctx, double dtSec);
	double GetClipDurationSec() const { return m_ClipDurationSec; }

	// GPU 본 팔레트 상수 버퍼
	ID3D11Buffer* GetBoneConstantBuffer() const { return m_pBoneCB; }
	UINT GetBoneCount() const { return (UINT)m_BoneNames.size(); }

	// GPU 스키닝 본 팔레트 최대 크기
	static constexpr int kMaxBones = 1023;

	// 팔레트가 없을 때(예: VMD 미로드 상태) 아이덴티티 팔레트 업로드
	void UploadIdentityPalette(ID3D11DeviceContext* ctx);

private:
	// 로드 헬퍼
	bool LoadMaterials(ID3D11Device* device, const struct aiScene* scene, const std::wstring& baseDir);
	bool BuildMeshBuffers(ID3D11Device* device, const struct aiScene* scene);

	// 스켈레톤/본/가중치 빌드 (Assimp)
	void BuildSkeletonAndNodeIndex(const struct aiScene* scene);
	void CollectBonesAndOffsets(const struct aiScene* scene);
	void BuildBaseVertexTable(const struct aiScene* scene, std::vector<size_t>& baseVertex);
	void AccumulateVertexWeights(const struct aiScene* scene, const std::vector<size_t>& baseVertex);
	void NormalizeInfluencesAndFlag();
	void ApplyInfluencesToVB(ID3D11Device* device);
	void EnsureBoneCB(ID3D11Device* device);

	// 애니메이션 평가(VMD)
	void BuildBonePalette(const std::vector<DirectX::XMFLOAT4X4>& global, std::vector<DirectX::XMMATRIX>& outPalette) const;
	void UploadBonePalette(ID3D11DeviceContext* ctx, const std::vector<DirectX::XMMATRIX>& palette);
	void EnsureBoneNodesExist();

private:
	// 버퍼
	ID3D11Buffer* m_pVB = nullptr;
	ID3D11Buffer* m_pIB = nullptr;
	int m_IndexCount = 0;
	int m_VertexStride = 0; // sizeof(PMX vertex)

	// PMX 원본 데이터(필요 시 유지)
	std::vector<VertexSkinnedTBN> m_Vertices;
	std::vector<uint32_t> m_Indices;
	std::vector<PmxSubset> m_Subsets;

	// 머티리얼 텍스처
	std::vector<ID3D11ShaderResourceView*> m_MaterialSRVs; // size=materials
	ID3D11ShaderResourceView* m_pWhiteSRV = nullptr;       // 폴백 1x1 white
	std::vector<std::pair<std::wstring, ID3D11ShaderResourceView*>> m_TexCache; // 경로→SRV

	// 스켈레톤/스키닝
	struct Influence4 { unsigned short idx[4] = {0,0,0,0}; float w[4] = {0,0,0,0}; };
	std::vector<Influence4> m_Influences;
	bool m_HasSkinning = false;
	std::vector<std::string> m_BoneNames;
	std::vector<std::wstring> m_BoneNamesW;
	std::vector<DirectX::XMFLOAT4X4> m_BoneOffset; // offset matrix per bone
	std::unordered_map<std::string, int> m_BoneIndexOfName;
	std::unordered_map<std::string, int> m_NodeIndexOfName;
	std::unordered_map<std::wstring, int> m_NodeIndexOfNameW;
	std::vector<SkeletonNode> m_Skeleton;
	int m_RootIndex = -1;
	DirectX::XMFLOAT4X4 m_GlobalInverse = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
	std::vector<DirectX::XMFLOAT4X4> m_LocalBind; // per-node bind local from scene
	ID3D11Buffer* m_pBoneCB = nullptr;

	// VMD 애니메이션(간단 키프레임 인터폴레이션)
	struct VMDKey { double t = 0.0; DirectX::XMFLOAT3 T = {0,0,0}; DirectX::XMFLOAT4 Q = {0,0,0,1}; };
	std::unordered_map<std::wstring, std::vector<VMDKey>> m_VMDChannels; // nodeNameW -> keys
	bool m_HasVMD = false;
	double m_AnimTimeSec = 0.0;
	double m_ClipDurationSec = 0.0;
	bool m_Playing = false;
};
