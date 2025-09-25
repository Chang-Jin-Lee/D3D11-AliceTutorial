#pragma once

#include <d3d11.h>
#include <string>
#include <vector>
#include <wrl/client.h>
#include "StaticMesh.h" // VertexData

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
	UINT GetVertexStride() const { return (UINT)sizeof(VertexData); }
	UINT GetVertexOffset() const { return 0; }
	// GPU 스키닝용 인플루언스 버퍼/스트라이드
	ID3D11Buffer* GetInfluenceBuffer() const { return m_pInfluenceVB; }
	UINT GetInfluenceStride() const { return m_InfluenceStride; }

	const std::vector<PmxSubset>& GetSubsets() const { return m_Subsets; }
	const std::vector<ID3D11ShaderResourceView*>& GetMaterialSRVs() const { return m_MaterialSRVs; }

    // 로드 상태/정보
	bool HasError() const { return !m_LastError.empty(); }
	const std::wstring& GetLastError() const { return m_LastError; }
	const std::wstring& GetLoadedPath() const { return m_LoadedPath; }
	size_t GetVertexCount() const { return m_Vertices.size(); }
	size_t GetSubsetCount() const { return m_Subsets.size(); }
	size_t GetMaterialCount() const { return m_MaterialSRVs.size(); }

	// VMD 관련: 로드 및 재생 상태
	struct VmdInfo { uint32_t numBoneKeys = 0; uint32_t numMorphKeys = 0; float durationSec = 0.0f; };
	bool LoadVmd(const std::wstring& vmdPath);
    
	bool HasVmd() const { return m_HasVmd; }
	VmdInfo GetVmdInfo() const { return m_VmdInfo; }
	// 재생 제어
	void Play() { m_Playing = true; }
	void Pause() { m_Playing = false; }
	void Stop() { m_Playing = false; m_TimeSec = 0.0f; }
	void SetLoop(bool loop) { m_Loop = loop; }
	bool GetLoop() const { return m_Loop; }
	void SetPlaybackSpeed(float s) { m_Speed = s; }
	float GetPlaybackSpeed() const { return m_Speed; }
	void SetTime(float t) { m_TimeSec = t; }
	float GetTime() const { return m_TimeSec; }
	bool IsPlaying() const { return m_Playing; }
	// 애니메이션 실행을 위한 Tick. App::OnUpdate에서 호출합니다
	void Tick(float dtSec);
	// 스키닝 업데이트: 본/가중치가 있고 VMD가 있을 때 CPU 스키닝으로 VB 갱신
	void UpdateSkinning(ID3D11DeviceContext* context);
	bool HasSkinning() const { return !m_Bones.empty() && !m_VertexInfluences.empty(); }
	// Bone matrices for GPU: 최대 64개로 채워 넣음
	int FillBoneMatrices64(DirectX::XMFLOAT4X4 outBones[64]) const;
	int FillBoneMatrices256(DirectX::XMFLOAT4X4 outBones[256]) const;

	// 루트 모션(센터 본) 평가: 현재 시간을 기준으로 루트 이동/회전 행렬을 반환
	bool HasRootMotion() const { return !m_VmdCenterKeys.empty(); }
	DirectX::XMMATRIX EvaluateRootMotion() const;

	// 본/노드 스켈레톤 정보 (Assimp 노드 트리 기반)
	struct Node
	{
		std::string name;
		int parent = -1;
		DirectX::XMMATRIX local; // 바인드포즈 로컬
	};
	const std::vector<Node>& GetNodes() const { return m_Nodes; }

private:
	// 로드 헬퍼
	bool LoadMaterials(ID3D11Device* device, const struct aiScene* scene, const std::wstring& baseDir);
	bool BuildMeshBuffers(ID3D11Device* device, const struct aiScene* scene);

private:
	// 버퍼
	ID3D11Buffer* m_pVB = nullptr;
	ID3D11Buffer* m_pIB = nullptr;
	int m_IndexCount = 0;
	// GPU 스키닝용 인플루언스 버퍼 (indices/weights)
	ID3D11Buffer* m_pInfluenceVB = nullptr;
	UINT m_InfluenceStride = 0;

	// PMX 원본 데이터(필요 시 유지)
	std::vector<VertexData> m_Vertices;
	std::vector<uint32_t> m_Indices;
	std::vector<PmxSubset> m_Subsets;
	// 스키닝 원본 보관 (POSITION/NORMAL 원본)
	std::vector<DirectX::XMFLOAT3> m_OrigPositions;
	std::vector<DirectX::XMFLOAT3> m_OrigNormals;

	// 본/가중치
	struct Bone
	{
		std::string name;
		DirectX::XMMATRIX offset; // aiBone::mOffsetMatrix (모델공간->본공간)
	};
	struct VertexInfluence
	{
		uint16_t indices[4] = {0,0,0,0};
		float weights[4] = {0,0,0,0};
	};
	std::vector<Bone> m_Bones;
	std::vector<VertexInfluence> m_VertexInfluences; // size = vertices
	std::vector<DirectX::XMMATRIX> m_CurrentBoneMatrices; // 최종 본 매트릭스 캐시
	std::vector<DirectX::XMMATRIX> m_LocalPose;  // 프레임별 로컬 캐시 (노드 수와 동일)
	std::vector<DirectX::XMMATRIX> m_GlobalPose; // 프레임별 글로벌 캐시 (노드 수와 동일)
	std::vector<Node> m_Nodes;
	std::unordered_map<std::string, int> m_NameToNode; // 이름→노드 인덱스
	std::vector<DirectX::XMMATRIX> m_NodeBindGlobal;   // 바인드포즈에서의 노드 글로벌 행렬
	// 본 이름→본 인덱스, 본→노드 맵핑
	std::unordered_map<std::string, uint32_t> m_BoneNameToIndex;
	std::vector<int> m_BoneToNode; // size=m_Bones

	// 머티리얼 텍스처
	std::vector<ID3D11ShaderResourceView*> m_MaterialSRVs; // size=materials
	ID3D11ShaderResourceView* m_pWhiteSRV = nullptr;       // 폴백 1x1 white
	std::vector<std::pair<std::wstring, ID3D11ShaderResourceView*>> m_TexCache; // 경로→SRV

    
	std::wstring m_LastError;
	std::wstring m_LoadedPath;

	// VMD 상태를 나타내는 변수
	bool m_HasVmd = false;
	VmdInfo m_VmdInfo{};
	std::wstring m_VmdPath;
    
	bool m_Playing = false;
	bool m_Loop = true;
	float m_Speed = 1.0f;
	float m_TimeSec = 0.0f;

	struct VmdCenterKey { uint32_t frame = 0; DirectX::XMFLOAT3 pos{}; DirectX::XMFLOAT4 rot{}; };
	std::vector<VmdCenterKey> m_VmdCenterKeys;
	// VMD 본 트랙: 이름별 키 리스트
	struct VmdBoneKey { uint32_t frame; DirectX::XMFLOAT3 pos; DirectX::XMFLOAT4 rot; };
	std::unordered_map<std::string, std::vector<VmdBoneKey>> m_VmdBoneTracks;
	struct TrackBinding { int nodeIndex; const std::vector<VmdBoneKey>* keys; };
	std::vector<TrackBinding> m_BoundTracks;
};
