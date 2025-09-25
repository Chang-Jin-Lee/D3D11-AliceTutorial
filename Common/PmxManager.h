#pragma once

#include <d3d11.h>
#include <string>
#include <vector>
#include <wrl/client.h>
#include "StaticMesh.h" // VertexData

// PMX ����� ���� (��Ƽ���� �ε����� ��ο� ����)
struct PmxSubset
{
	uint32_t startIndex = 0;
	uint32_t indexCount = 0;
	uint32_t materialIndex = 0;
};

class PmxManager
{
public:
	// ����
	PmxManager() = default;
	~PmxManager() = default;

	// PMX �ε�: ����̽��� ���� ��θ� �޾� VB/IB/�����/�ؽ�ó SRV�� �غ�
	bool Load(ID3D11Device* device, const std::wstring& pmxPath);
    
	void Release();

	// ����
	bool HasMesh() const { return m_pVB != nullptr && m_pIB != nullptr && m_IndexCount > 0; }
	ID3D11Buffer* GetVertexBuffer() const { return m_pVB; }
	ID3D11Buffer* GetIndexBuffer() const { return m_pIB; }
	int GetIndexCount() const { return m_IndexCount; }
	UINT GetVertexStride() const { return (UINT)sizeof(VertexData); }
	UINT GetVertexOffset() const { return 0; }
	// GPU ��Ű�׿� ���÷�� ����/��Ʈ���̵�
	ID3D11Buffer* GetInfluenceBuffer() const { return m_pInfluenceVB; }
	UINT GetInfluenceStride() const { return m_InfluenceStride; }

	const std::vector<PmxSubset>& GetSubsets() const { return m_Subsets; }
	const std::vector<ID3D11ShaderResourceView*>& GetMaterialSRVs() const { return m_MaterialSRVs; }

    // �ε� ����/����
	bool HasError() const { return !m_LastError.empty(); }
	const std::wstring& GetLastError() const { return m_LastError; }
	const std::wstring& GetLoadedPath() const { return m_LoadedPath; }
	size_t GetVertexCount() const { return m_Vertices.size(); }
	size_t GetSubsetCount() const { return m_Subsets.size(); }
	size_t GetMaterialCount() const { return m_MaterialSRVs.size(); }

	// VMD ����: �ε� �� ��� ����
	struct VmdInfo { uint32_t numBoneKeys = 0; uint32_t numMorphKeys = 0; float durationSec = 0.0f; };
	bool LoadVmd(const std::wstring& vmdPath);
    
	bool HasVmd() const { return m_HasVmd; }
	VmdInfo GetVmdInfo() const { return m_VmdInfo; }
	// ��� ����
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
	// �ִϸ��̼� ������ ���� Tick. App::OnUpdate���� ȣ���մϴ�
	void Tick(float dtSec);
	// ��Ű�� ������Ʈ: ��/����ġ�� �ְ� VMD�� ���� �� CPU ��Ű������ VB ����
	void UpdateSkinning(ID3D11DeviceContext* context);
	bool HasSkinning() const { return !m_Bones.empty() && !m_VertexInfluences.empty(); }
	// Bone matrices for GPU: �ִ� 64���� ä�� ����
	int FillBoneMatrices64(DirectX::XMFLOAT4X4 outBones[64]) const;
	int FillBoneMatrices256(DirectX::XMFLOAT4X4 outBones[256]) const;

	// ��Ʈ ���(���� ��) ��: ���� �ð��� �������� ��Ʈ �̵�/ȸ�� ����� ��ȯ
	bool HasRootMotion() const { return !m_VmdCenterKeys.empty(); }
	DirectX::XMMATRIX EvaluateRootMotion() const;

	// ��/��� ���̷��� ���� (Assimp ��� Ʈ�� ���)
	struct Node
	{
		std::string name;
		int parent = -1;
		DirectX::XMMATRIX local; // ���ε����� ����
	};
	const std::vector<Node>& GetNodes() const { return m_Nodes; }

private:
	// �ε� ����
	bool LoadMaterials(ID3D11Device* device, const struct aiScene* scene, const std::wstring& baseDir);
	bool BuildMeshBuffers(ID3D11Device* device, const struct aiScene* scene);

private:
	// ����
	ID3D11Buffer* m_pVB = nullptr;
	ID3D11Buffer* m_pIB = nullptr;
	int m_IndexCount = 0;
	// GPU ��Ű�׿� ���÷�� ���� (indices/weights)
	ID3D11Buffer* m_pInfluenceVB = nullptr;
	UINT m_InfluenceStride = 0;

	// PMX ���� ������(�ʿ� �� ����)
	std::vector<VertexData> m_Vertices;
	std::vector<uint32_t> m_Indices;
	std::vector<PmxSubset> m_Subsets;
	// ��Ű�� ���� ���� (POSITION/NORMAL ����)
	std::vector<DirectX::XMFLOAT3> m_OrigPositions;
	std::vector<DirectX::XMFLOAT3> m_OrigNormals;

	// ��/����ġ
	struct Bone
	{
		std::string name;
		DirectX::XMMATRIX offset; // aiBone::mOffsetMatrix (�𵨰���->������)
	};
	struct VertexInfluence
	{
		uint16_t indices[4] = {0,0,0,0};
		float weights[4] = {0,0,0,0};
	};
	std::vector<Bone> m_Bones;
	std::vector<VertexInfluence> m_VertexInfluences; // size = vertices
	std::vector<DirectX::XMMATRIX> m_CurrentBoneMatrices; // ���� �� ��Ʈ���� ĳ��
	std::vector<DirectX::XMMATRIX> m_LocalPose;  // �����Ӻ� ���� ĳ�� (��� ���� ����)
	std::vector<DirectX::XMMATRIX> m_GlobalPose; // �����Ӻ� �۷ι� ĳ�� (��� ���� ����)
	std::vector<Node> m_Nodes;
	std::unordered_map<std::string, int> m_NameToNode; // �̸����� �ε���
	std::vector<DirectX::XMMATRIX> m_NodeBindGlobal;   // ���ε�������� ��� �۷ι� ���
	// �� �̸��溻 �ε���, ������ ����
	std::unordered_map<std::string, uint32_t> m_BoneNameToIndex;
	std::vector<int> m_BoneToNode; // size=m_Bones

	// ��Ƽ���� �ؽ�ó
	std::vector<ID3D11ShaderResourceView*> m_MaterialSRVs; // size=materials
	ID3D11ShaderResourceView* m_pWhiteSRV = nullptr;       // ���� 1x1 white
	std::vector<std::pair<std::wstring, ID3D11ShaderResourceView*>> m_TexCache; // ��Ρ�SRV

    
	std::wstring m_LastError;
	std::wstring m_LoadedPath;

	// VMD ���¸� ��Ÿ���� ����
	bool m_HasVmd = false;
	VmdInfo m_VmdInfo{};
	std::wstring m_VmdPath;
    
	bool m_Playing = false;
	bool m_Loop = true;
	float m_Speed = 1.0f;
	float m_TimeSec = 0.0f;

	struct VmdCenterKey { uint32_t frame = 0; DirectX::XMFLOAT3 pos{}; DirectX::XMFLOAT4 rot{}; };
	std::vector<VmdCenterKey> m_VmdCenterKeys;
	// VMD �� Ʈ��: �̸��� Ű ����Ʈ
	struct VmdBoneKey { uint32_t frame; DirectX::XMFLOAT3 pos; DirectX::XMFLOAT4 rot; };
	std::unordered_map<std::string, std::vector<VmdBoneKey>> m_VmdBoneTracks;
	struct TrackBinding { int nodeIndex; const std::vector<VmdBoneKey>* keys; };
	std::vector<TrackBinding> m_BoundTracks;
};
