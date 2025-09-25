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

	const std::vector<PmxSubset>& GetSubsets() const { return m_Subsets; }
	const std::vector<ID3D11ShaderResourceView*>& GetMaterialSRVs() const { return m_MaterialSRVs; }

private:
	// 로드 헬퍼
	bool LoadMaterials(ID3D11Device* device, const struct aiScene* scene, const std::wstring& baseDir);
	bool BuildMeshBuffers(ID3D11Device* device, const struct aiScene* scene);

private:
	// 버퍼
	ID3D11Buffer* m_pVB = nullptr;
	ID3D11Buffer* m_pIB = nullptr;
	int m_IndexCount = 0;

	// PMX 원본 데이터(필요 시 유지)
	std::vector<VertexData> m_Vertices;
	std::vector<uint32_t> m_Indices;
	std::vector<PmxSubset> m_Subsets;

	// 머티리얼 텍스처
	std::vector<ID3D11ShaderResourceView*> m_MaterialSRVs; // size=materials
	ID3D11ShaderResourceView* m_pWhiteSRV = nullptr;       // 폴백 1x1 white
	std::vector<std::pair<std::wstring, ID3D11ShaderResourceView*>> m_TexCache; // 경로→SRV
};
