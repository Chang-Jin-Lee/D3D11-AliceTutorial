#include "pch.h"
#include "FbxModel.h"
#include "FbxMaterial.h"
#include "FbxGeometry.h"
#include "FbxSkeleton.h"
#include "FbxAnimation.h"
#include "../Helper.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace DirectX;

struct FbxModel::Impl
{
	// Core
	std::unique_ptr<Assimp::Importer> importer;
	const aiScene* scene = nullptr; // owned by Assimp
	XMFLOAT4X4 globalInverse{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

	// Subsystems
	FbxMaterialLoader materials;
	FbxGeometryBuilder geometry;
	FbxSkeleton skeleton;
	FbxAnimation anim;

	// Maps for convenience
	std::unordered_map<std::string,int> nodeIndexOfName; // same as skeleton.NodeIndexOfName

	FbxModel::AnimationType animType = FbxModel::AnimationType::None;
};

FbxModel::FbxModel() : m_(new Impl) {}
FbxModel::~FbxModel() { Release(); }

void FbxModel::Release()
{
	m_->materials.Clear();
	m_->geometry.Clear();
	m_->skeleton = FbxSkeleton{};
	m_->anim.Clear();
	m_->nodeIndexOfName.clear();
	m_->scene = nullptr;
	m_->importer.reset();
	m_->animType = AnimationType::None;
}

bool FbxModel::Load(ID3D11Device* device, const std::wstring& pathW)
{
	Release();
	m_->importer = std::make_unique<Assimp::Importer>();
    // FBX 피벗/프리/포스트 회전 보존을 끄면 Assimp가 생성하는 _$AssimpFbx$* 헬퍼 노드가 제거되어
    // 본/노드 수가 DCC(Blender)와 더 일치하게 됩니다.
    m_->importer->SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
	m_->importer->SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
	std::string pathA = Utf8FromWString(pathW);
	m_->scene = m_->importer->ReadFile(pathA,
		aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
		aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded |
		aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph | aiProcess_LimitBoneWeights);
	if (!m_->scene || !m_->scene->HasMeshes()) return false;

	// Global inverse
	{
		aiMatrix4x4 I = m_->scene->mRootNode->mTransformation; I.Inverse();
		m_->globalInverse._11 = (float)I.a1; m_->globalInverse._12 = (float)I.a2; m_->globalInverse._13 = (float)I.a3; m_->globalInverse._14 = (float)I.a4;
		m_->globalInverse._21 = (float)I.b1; m_->globalInverse._22 = (float)I.b2; m_->globalInverse._23 = (float)I.b3; m_->globalInverse._24 = (float)I.b4;
		m_->globalInverse._31 = (float)I.c1; m_->globalInverse._32 = (float)I.c2; m_->globalInverse._33 = (float)I.c3; m_->globalInverse._34 = (float)I.c4;
		m_->globalInverse._41 = (float)I.d1; m_->globalInverse._42 = (float)I.d2; m_->globalInverse._43 = (float)I.d3; m_->globalInverse._44 = (float)I.d4;
	}

	// Base dir
	std::wstring baseDir = pathW; size_t slash = baseDir.find_last_of(L"/\\"); baseDir = (slash == std::wstring::npos) ? L"" : baseDir.substr(0, slash + 1);

	// Build subsystems
	if (!m_->materials.Load(device, m_->scene, baseDir)) return false;
	if (!m_->geometry.Build(device, m_->scene)) return false;
	m_->skeleton.BuildFromScene(m_->scene);
	m_->skeleton.CollectBonesAndOffsets(m_->scene);
	m_->nodeIndexOfName = m_->skeleton.NodeIndexOfName();

	// Decide animation mode and prepare
	bool hasBones = m_->skeleton.HasBones();
	if (!hasBones && m_->scene->mNumAnimations > 0)
	{
		m_->skeleton.BuildRigidBones();
		// Build rigid weights from per-vertex owning nodes so GPU skinning path can be reused
		{
			auto& verts = m_->geometry.GetCPUVertices();
			const auto& owners = m_->geometry.GetVertexOwningNodeNames();
			const auto& boneNames = m_->skeleton.GetBoneNames();
			const auto& skelNodes = m_->skeleton.GetSkeleton();
			std::unordered_map<std::string,int> boneIndexOfName;
			boneIndexOfName.reserve(boneNames.size());
			for (int i = 0; i < (int)boneNames.size(); ++i) boneIndexOfName[boneNames[(size_t)i]] = i;
			if (!verts.empty())
			{
				for (size_t i = 0; i < verts.size(); ++i)
				{
					unsigned short bi = 0;
					if (i < owners.size())
					{
						const std::string& owner = owners[i];
						auto itB = boneIndexOfName.find(owner);
						if (itB != boneIndexOfName.end()) bi = (unsigned short)itB->second;
						else
						{
							auto itNode = m_->nodeIndexOfName.find(owner);
							int node = (itNode != m_->nodeIndexOfName.end()) ? itNode->second : -1;
							while (node >= 0)
							{
								const auto& sn = skelNodes[(size_t)node];
								auto itB2 = boneIndexOfName.find(sn.name);
								if (itB2 != boneIndexOfName.end()) { bi = (unsigned short)itB2->second; break; }
								node = sn.parent;
							}
						}
					}
					verts[i].boneIdx[0] = bi; verts[i].boneIdx[1] = verts[i].boneIdx[2] = verts[i].boneIdx[3] = 0;
					verts[i].boneWeight = { 1.0f, 0.0f, 0.0f, 0.0f };
				}
				m_->geometry.RebuildVBFromCPU(device);
			}
		}
		m_->animType = AnimationType::Rigid;
	}
	else if (hasBones)
	{
		m_->animType = AnimationType::Skinned;
		// Build skinning weights and re-upload VB so skinned VS works
		// IMPORTANT: BLENDINDICES must index into the palette ordered by boneNames (not node index)
		const auto& boneNames = m_->skeleton.GetBoneNames();
		std::unordered_map<std::string,int> boneIndexOfBoneName;
		boneIndexOfBoneName.reserve(boneNames.size());
		for (int i = 0; i < (int)boneNames.size(); ++i) boneIndexOfBoneName[boneNames[(size_t)i]] = i;
		auto& verts = m_->geometry.GetCPUVertices();
		if (!verts.empty())
		{
			struct Influence4 { unsigned short idx[4] = {0,0,0,0}; float w[4] = {0,0,0,0}; };
			std::vector<Influence4> inf; inf.assign(verts.size(), {});
			// Build base vertex table (mesh -> start in aggregated array) using same traversal order as geometry
			std::vector<size_t> baseVertex; baseVertex.resize(m_->scene->mNumMeshes, 0);
			size_t cursor = 0;
			std::function<void(const aiNode*)> fillBase = [&](const aiNode* node){
				for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
				{
					unsigned meshIdx = node->mMeshes[mi];
					baseVertex[meshIdx] = cursor;
					cursor += m_->scene->mMeshes[meshIdx]->mNumVertices;
				}
				for (unsigned ci = 0; ci < node->mNumChildren; ++ci) fillBase(node->mChildren[ci]);
			};
			fillBase(m_->scene->mRootNode);

			for (unsigned mi = 0; mi < m_->scene->mNumMeshes; ++mi)
			{
				const aiMesh* mesh = m_->scene->mMeshes[mi];
				size_t base = baseVertex[mi];
				for (unsigned bi = 0; bi < mesh->mNumBones; ++bi)
				{
					const aiBone* b = mesh->mBones[bi];
					int boneIdx = -1;
					auto itB = boneIndexOfBoneName.find(b->mName.C_Str());
					if (itB != boneIndexOfBoneName.end()) boneIdx = itB->second;
					if (boneIdx < 0) continue;
					for (unsigned wi = 0; wi < b->mNumWeights; ++wi)
					{
						const aiVertexWeight& vw = b->mWeights[wi];
						size_t v = base + (size_t)vw.mVertexId;
						if (v >= inf.size()) continue;
						int slot = 0; float minW = inf[v].w[0];
						for (int s = 1; s < 4; ++s) { if (inf[v].w[s] < minW) { minW = inf[v].w[s]; slot = s; } }
						inf[v].idx[slot] = (unsigned short)boneIdx;
						inf[v].w[slot] = (float)vw.mWeight;
					}
				}
			}
			// Normalize and apply to vertices
			for (size_t i = 0; i < inf.size(); ++i)
			{
				float s = inf[i].w[0] + inf[i].w[1] + inf[i].w[2] + inf[i].w[3];
				if (s > 1e-6f) { float inv = 1.0f / s; for (int k = 0; k < 4; ++k) inf[i].w[k] *= inv; }
				verts[i].boneIdx[0] = inf[i].idx[0];
				verts[i].boneIdx[1] = inf[i].idx[1];
				verts[i].boneIdx[2] = inf[i].idx[2];
				verts[i].boneIdx[3] = inf[i].idx[3];
				verts[i].boneWeight = { inf[i].w[0], inf[i].w[1], inf[i].w[2], inf[i].w[3] };
			}
			m_->geometry.RebuildVBFromCPU(device);
		}
	}
	else
	{
		m_->animType = AnimationType::None;
	}

	// Init animation metadata and ensure CB
	m_->anim.InitMetadata(m_->scene);
	m_->anim.SetType((m_->animType == AnimationType::Rigid) ? FbxAnimation::AnimType::Rigid : (m_->animType == AnimationType::Skinned ? FbxAnimation::AnimType::Skinned : FbxAnimation::AnimType::None));
	m_->anim.EnsureBoneCB(device, 1023);
	return true;
}

// Mesh getters
bool FbxModel::HasMesh() const { return m_->geometry.GetVB() && m_->geometry.GetIB() && m_->geometry.GetIndexCount() > 0; }
ID3D11Buffer* FbxModel::GetVertexBuffer() const { return m_->geometry.GetVB(); }
ID3D11Buffer* FbxModel::GetIndexBuffer() const { return m_->geometry.GetIB(); }
int FbxModel::GetIndexCount() const { return m_->geometry.GetIndexCount(); }
UINT FbxModel::GetVertexStride() const { return m_->geometry.GetVertexStride(); }
const std::vector<FbxSubset>& FbxModel::GetSubsets() const { return m_->geometry.GetSubsets(); }
const std::vector<ID3D11ShaderResourceView*>& FbxModel::GetMaterialSRVs() const { return m_->materials.GetMaterialSRVs(); }
const std::vector<ID3D11ShaderResourceView*>& FbxModel::GetMetallicSRVs() const { return m_->materials.GetMetallicSRVs(); }
const std::vector<ID3D11ShaderResourceView*>& FbxModel::GetRoughnessSRVs() const { return m_->materials.GetRoughnessSRVs(); }
const std::vector<ID3D11ShaderResourceView*>& FbxModel::GetNormalSRVs() const { return m_->materials.GetNormalSRVs(); }

// Skeleton/animation
bool FbxModel::HasSkeleton() const { return m_->skeleton.HasBones(); }
bool FbxModel::HasAnimations() const { return !m_->anim.GetNames().empty(); }
const std::vector<FbxSkeletonNode>& FbxModel::GetSkeleton() const { return m_->skeleton.GetSkeleton(); }
int FbxModel::GetSkeletonRoot() const { return m_->skeleton.GetRootIndex(); }
ID3D11Buffer* FbxModel::GetBoneConstantBuffer() const { return m_->anim.GetBoneCB(); }
UINT FbxModel::GetBoneCount() const { return (UINT)m_->skeleton.GetBoneNames().size(); }

const std::vector<std::string>& FbxModel::GetAnimationNames() const { return m_->anim.GetNames(); }
int FbxModel::GetCurrentAnimationIndex() const { return m_->anim.GetCurrentIndex(); }
void FbxModel::SetCurrentAnimation(int idx) { m_->anim.SetCurrentIndex(idx); }
void FbxModel::SetAnimationPlaying(bool playing) { m_->anim.SetPlaying(playing); }
bool FbxModel::IsAnimationPlaying() const { return m_->anim.IsPlaying(); }
double FbxModel::GetAnimationTimeSeconds() const { return m_->anim.GetTimeSec(); }
void FbxModel::SetAnimationTimeSeconds(double t) { m_->anim.SetTimeSec(t); }
void FbxModel::UpdateAnimation(ID3D11DeviceContext* ctx, double dtSec)
{
	m_->anim.UpdateAndUpload(
		ctx,
		dtSec,
		m_->scene,
		m_->nodeIndexOfName,
		m_->skeleton.GetBoneNames(),
		m_->skeleton.GetBoneOffsets(),
		m_->globalInverse);
}
double FbxModel::GetClipDurationSec(int idx) const { return m_->anim.GetClipDurationSec(idx); }

FbxModel::AnimationType FbxModel::GetCurrentAnimationType() const { return m_->animType; }

// Shared data accessors for per-instance animators
const aiScene* FbxModel::GetScenePtr() const { return m_->scene; }
const std::unordered_map<std::string,int>& FbxModel::GetNodeIndexOfName() const { return m_->nodeIndexOfName; }
const std::vector<std::string>& FbxModel::GetBoneNames() const { return m_->skeleton.GetBoneNames(); }
const std::vector<XMFLOAT4X4>& FbxModel::GetBoneOffsets() const { return m_->skeleton.GetBoneOffsets(); }
const XMFLOAT4X4& FbxModel::GetGlobalInverse() const { return m_->globalInverse; }


