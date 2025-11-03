#include "pch.h"
#include "FbxGeometry.h"
#include "../Helper.h"

#include <assimp/scene.h>
#include <d3d11.h>

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

	std::function<void(const aiNode*)> traverse;
	traverse = [&](const aiNode* node){
		for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
		{
			const aiMesh* mesh = scene->mMeshes[node->mMeshes[mi]];
			size_t base = m_->bindVertices.size();
			m_->bindVertices.reserve(base + mesh->mNumVertices);
			m_->owningNode.reserve(base + mesh->mNumVertices);
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
				m_->bindVertices.push_back(v);
				m_->owningNode.push_back(node->mName.C_Str());
			}
			uint32_t start = (uint32_t)m_->indices.size();
			for (unsigned f = 0; f < mesh->mNumFaces; ++f)
			{
				const aiFace& face = mesh->mFaces[f];
				if (face.mNumIndices == 3)
				{
					m_->indices.push_back((uint32_t)(base + face.mIndices[0]));
					m_->indices.push_back((uint32_t)(base + face.mIndices[1]));
					m_->indices.push_back((uint32_t)(base + face.mIndices[2]));
				}
			}
			uint32_t count = (uint32_t)m_->indices.size() - start;
			m_->subsets.push_back({ start, count, mesh->mMaterialIndex });
		}
		for (unsigned ci = 0; ci < node->mNumChildren; ++ci) traverse(node->mChildren[ci]);
	};
	traverse(scene->mRootNode);

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


