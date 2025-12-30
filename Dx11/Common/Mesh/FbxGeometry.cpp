#include "pch.h"
#include "FbxGeometry.h"
#include "../Helper.h"

#include <assimp/scene.h>
#include <d3d11.h>

#include <queue>
#include <algorithm>
#include <numeric>
#if __has_include(<execution>)
#include <execution>
#define FBX_HAS_EXECUTION 1
#else
#define FBX_HAS_EXECUTION 0
#endif

struct FbxGeometryBuilder::Impl
{
	ID3D11Buffer* vb = nullptr;
	ID3D11Buffer* ib = nullptr;
	int indexCount = 0;
	UINT vertexStride = sizeof(VertexSkinnedTBN);
	std::vector<FbxSubset> subsets;
	std::vector<VertexSkinnedTBN> bindVertices;
	std::vector<uint32_t> indices;
	std::vector<std::string> owningNode; // per-vertex owner node name
};

FbxGeometryBuilder::FbxGeometryBuilder() : m_(new Impl) {}
FbxGeometryBuilder::~FbxGeometryBuilder() { Clear(); delete m_; }

void FbxGeometryBuilder::Clear()
{
	SAFE_RELEASE(m_->vb);
    m_->vb = nullptr;
	SAFE_RELEASE(m_->ib);
    m_->ib = nullptr;
	m_->indexCount = 0;
	m_->subsets.clear();
	m_->bindVertices.clear();
	m_->indices.clear();
	m_->owningNode.clear();
}

bool FbxGeometryBuilder::Build(ID3D11Device* device, const aiScene* scene)
{
	if (!device || !scene || !scene->HasMeshes()) return false;
	Clear();

	// 1. 트리 순회를 위한 스택과 메쉬 정보 저장을 위한 구조체
	struct MeshInfo { const aiNode* node; const aiMesh* mesh; };
	std::vector<MeshInfo> infos;
	std::vector<const aiNode*> nodeStack = { scene->mRootNode };

	size_t totalV = 0, totalI = 0;

	// 2. DFS로 모든 노드를 순회하며 메쉬 정보 수집 및 개수 파악
	while (!nodeStack.empty())
	{
		const aiNode* node = nodeStack.back();
		nodeStack.pop_back();

		// 현재 노드의 메쉬들 수집
		for (unsigned i = 0; i < node->mNumMeshes; ++i)
		{
			const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			infos.push_back({ node, mesh });

			totalV += mesh->mNumVertices;
			// 삼각형(Face)인 경우만 인덱스 3개 추가
			for (unsigned f = 0; f < mesh->mNumFaces; ++f)
				if (mesh->mFaces[f].mNumIndices == 3) totalI += 3;
		}

		// 자식 노드 스택에 추가
		for (unsigned i = 0; i < node->mNumChildren; ++i)
			nodeStack.push_back(node->mChildren[i]);
	}

	if (totalV == 0) return false;

	// 3. 버퍼 한 번에 할당 (Reallocation 방지)
	m_->bindVertices.resize(totalV);
	m_->owningNode.resize(totalV);
	m_->indices.resize(totalI);
	m_->subsets.resize(infos.size());

	// 4. 데이터 채우기
	size_t vOffset = 0;
	size_t iOffset = 0;

	for (size_t jobIdx = 0; jobIdx < infos.size(); ++jobIdx)
	{
		const auto& [node, mesh] = infos[jobIdx];

		// 정점 데이터 복사
		for (unsigned i = 0; i < mesh->mNumVertices; ++i)
		{
			// 데이터 추출 (없으면 기본값 0,0,0 등 사용)
			const auto& p = mesh->mVertices[i];
			const auto& n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0, 1, 0);
			const auto& uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0, 0, 0);
			const auto& t = mesh->HasTangentsAndBitangents() ? mesh->mTangents[i] : aiVector3D(1, 0, 0);

			// 구조체 채우기
			VertexSkinnedTBN& v = m_->bindVertices[vOffset + i];
			v.pos = { p.x, p.y, p.z };
			v.n = { n.x, n.y, n.z };
			v.uv = { uv.x, uv.y };
			v.t = { t.x, t.y, t.z };
			v.color = { 1, 1, 1, 1 };
			// 본 가중치는 초기화 (필요 시 별도 처리)
			v.boneIdx[0] = v.boneIdx[1] = v.boneIdx[2] = v.boneIdx[3] = 0;
			v.boneWeight = { 0, 0, 0, 0 };

			m_->owningNode[vOffset + i] = node->mName.C_Str();
		}

		// 인덱스 데이터 복사
		uint32_t currentFaceIndices = 0;
		for (unsigned f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			if (face.mNumIndices != 3) continue; // 삼각형만 처리

			// 현재 버퍼의 오프셋을 더해서 저장
			m_->indices[iOffset + currentFaceIndices + 0] = static_cast<uint32_t>(vOffset + face.mIndices[0]);
			m_->indices[iOffset + currentFaceIndices + 1] = static_cast<uint32_t>(vOffset + face.mIndices[1]);
			m_->indices[iOffset + currentFaceIndices + 2] = static_cast<uint32_t>(vOffset + face.mIndices[2]);
			currentFaceIndices += 3;
		}

		// 서브셋(DrawCall 단위) 정보 저장
		m_->subsets[jobIdx] = {
			static_cast<uint32_t>(iOffset),
			static_cast<uint32_t>(currentFaceIndices),
			mesh->mMaterialIndex
		};

		// 다음 메쉬를 위해 오프셋 갱신
		vOffset += mesh->mNumVertices;
		iOffset += currentFaceIndices;
	}

	// 5. DX11 버퍼 생성
	// Vertex Buffer
	D3D11_BUFFER_DESC vbDesc = {}; // 0으로 초기화
	vbDesc.ByteWidth = (UINT)(m_->bindVertices.size() * sizeof(VertexSkinnedTBN));
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = m_->bindVertices.data();

	if (FAILED(device->CreateBuffer(&vbDesc, &vbData, &m_->vb))) return false;

	// Index Buffer
	m_->indexCount = (int)m_->indices.size();
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = (UINT)(m_->indices.size() * sizeof(uint32_t));
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = m_->indices.data();

	if (FAILED(device->CreateBuffer(&ibDesc, &ibData, &m_->ib))) return false;

	return true;
}

ID3D11Buffer* FbxGeometryBuilder::GetVB() const { return m_->vb; }
ID3D11Buffer* FbxGeometryBuilder::GetIB() const { return m_->ib; }
int FbxGeometryBuilder::GetIndexCount() const { return m_->indexCount; }
UINT FbxGeometryBuilder::GetVertexStride() const { return m_->vertexStride; }
const std::vector<FbxSubset>& FbxGeometryBuilder::GetSubsets() const { return m_->subsets; }
const std::vector<std::string>& FbxGeometryBuilder::GetVertexOwningNodeNames() const { return m_->owningNode; }
std::vector<VertexSkinnedTBN>& FbxGeometryBuilder::GetCPUVertices() { return m_->bindVertices; }
const std::vector<VertexSkinnedTBN>& FbxGeometryBuilder::GetCPUVertices() const { return m_->bindVertices; }

bool FbxGeometryBuilder::RebuildVBFromCPU(ID3D11Device* device)
{
	if (!device) return false;
	SAFE_RELEASE(m_->vb);
	m_->vb = nullptr;
	D3D11_BUFFER_DESC vb{}; vb.BindFlags = D3D11_BIND_VERTEX_BUFFER; vb.Usage = D3D11_USAGE_DEFAULT;
	vb.ByteWidth = (UINT)(m_->bindVertices.size() * sizeof(VertexSkinnedTBN));
	D3D11_SUBRESOURCE_DATA vbd{}; vbd.pSysMem = m_->bindVertices.data();
	HR_T(device->CreateBuffer(&vb, &vbd, &m_->vb));
	return m_->vb != nullptr;
}


