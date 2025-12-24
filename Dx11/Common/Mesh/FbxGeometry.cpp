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

	// 비재귀 BFS(부모→자식 위상 순서)로 모든 메쉬를 나열하고, 레벨 단위 병렬 처리
	struct MeshEntry
	{
		const aiNode* node;
		const aiMesh* mesh;
		uint32_t materialIndex;
		uint32_t vertexCount;
		uint32_t indexCount; // 삼각형만 집계(3의 배수)
		size_t vertexOffset;
		size_t indexOffset;
		size_t entryIndex;
	};

	std::vector<MeshEntry> entries;
	std::vector<std::pair<size_t,size_t>> levelRanges; // {start, count}
	std::queue<const aiNode*> q;
	q.push(scene->mRootNode);
	size_t totalVertices = 0;
	size_t totalIndices = 0;
	while (!q.empty())
	{
		size_t levelSize = q.size();
		size_t start = entries.size();
		for (size_t li = 0; li < levelSize; ++li)
		{
			const aiNode* node = q.front(); q.pop();
			for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
			{
				const aiMesh* mesh = scene->mMeshes[node->mMeshes[mi]];
				uint32_t vtx = mesh->mNumVertices;
				uint32_t idx = 0;
				for (unsigned f = 0; f < mesh->mNumFaces; ++f)
				{
					const aiFace& face = mesh->mFaces[f];
					if (face.mNumIndices == 3) idx += 3;
				}
				size_t entryIndex = entries.size();
				entries.push_back({ node, mesh, mesh->mMaterialIndex, vtx, idx, 0, 0, entryIndex });
				totalVertices += vtx;
				totalIndices += idx;
			}
			for (unsigned ci = 0; ci < node->mNumChildren; ++ci) q.push(node->mChildren[ci]);
		}
		size_t count = entries.size() - start;
		levelRanges.push_back({ start, count });
	}

	// 오프셋 확정(프리픽스 합)
	size_t vOff = 0, iOff = 0;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		entries[i].vertexOffset = vOff;
		entries[i].indexOffset = iOff;
		entries[i].entryIndex = i;
		vOff += entries[i].vertexCount;
		iOff += entries[i].indexCount;
	}

	// 공유 버퍼 사전 할당 후, 각 엔트리가 자기 구간을 병렬로 채움
	m_->bindVertices.clear();
	m_->indices.clear();
	m_->owningNode.clear();
	m_->subsets.clear();
	m_->bindVertices.resize(totalVertices);
	m_->owningNode.resize(totalVertices);
	m_->indices.resize(totalIndices);
	m_->subsets.resize(entries.size());

	auto processEntry = [&](const MeshEntry& e)
	{
		const aiMesh* mesh = e.mesh;
		size_t vBase = e.vertexOffset;
		for (unsigned i = 0; i < mesh->mNumVertices; ++i)
		{
			aiVector3D p = mesh->mVertices[i];
			aiVector3D n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0,1,0);
			aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0,0,0);
			aiVector3D tg = mesh->HasTangentsAndBitangents() ? mesh->mTangents[i]   : aiVector3D(1,0,0);
			aiVector3D bt = mesh->HasTangentsAndBitangents() ? mesh->mBitangents[i] : aiVector3D(0,1,0);
			VertexSkinnedTBN v{};
			v.pos = {p.x,p.y,p.z}; v.n = {n.x,n.y,n.z}; v.t = {tg.x,tg.y,tg.z}; v.b = {bt.x,bt.y,bt.z};
			v.color = {1,1,1,1}; v.uv = {uv.x,uv.y};
			v.boneIdx[0]=v.boneIdx[1]=v.boneIdx[2]=v.boneIdx[3]=0; v.boneWeight = {0,0,0,0};
			m_->bindVertices[vBase + i] = v;
			m_->owningNode[vBase + i] = e.node->mName.C_Str();
		}
		size_t iBase = e.indexOffset;
		uint32_t triW = 0;
		for (unsigned f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			if (face.mNumIndices == 3)
			{
				m_->indices[iBase + triW + 0] = (uint32_t)(vBase + face.mIndices[0]);
				m_->indices[iBase + triW + 1] = (uint32_t)(vBase + face.mIndices[1]);
				m_->indices[iBase + triW + 2] = (uint32_t)(vBase + face.mIndices[2]);
				triW += 3;
			}
		}
		m_->subsets[e.entryIndex] = { (uint32_t)e.indexOffset, (uint32_t)e.indexCount, e.materialIndex };
	};

	for (const auto& range : levelRanges)
	{
		auto begin = entries.begin() + range.first;
		auto end = begin + range.second;
#if FBX_HAS_EXECUTION
		std::for_each(std::execution::par, begin, end, processEntry);
#else
		std::for_each(begin, end, processEntry);
#endif
	}

	if (m_->bindVertices.empty() || m_->indices.empty()) return false;

	D3D11_BUFFER_DESC vb{}; vb.BindFlags = D3D11_BIND_VERTEX_BUFFER; vb.Usage = D3D11_USAGE_DEFAULT;
	vb.ByteWidth = (UINT)(m_->bindVertices.size() * sizeof(VertexSkinnedTBN));
	D3D11_SUBRESOURCE_DATA vbd{}; vbd.pSysMem = m_->bindVertices.data();
	HR_T(device->CreateBuffer(&vb, &vbd, &m_->vb));

	m_->indexCount = (int)m_->indices.size();
	D3D11_BUFFER_DESC ib{}; ib.BindFlags = D3D11_BIND_INDEX_BUFFER; ib.Usage = D3D11_USAGE_DEFAULT; ib.ByteWidth = (UINT)(m_->indices.size() * sizeof(uint32_t));
	D3D11_SUBRESOURCE_DATA ibd{}; ibd.pSysMem = m_->indices.data();
	HR_T(device->CreateBuffer(&ib, &ibd, &m_->ib));
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


