#include "pch.h"
#include "FbxSkeleton.h"
#include "../Helper.h"

#include <assimp/scene.h>

void FbxSkeleton::BuildFromScene(const aiScene* scene)
{
	m_Skeleton.clear(); m_NodeIndexOfName.clear(); m_RootIndex = -1;
	if (!scene || !scene->mRootNode) return;
	std::function<int(const aiNode*, int)> build = [&](const aiNode* node, int parent){
		int idx = (int)m_Skeleton.size();
		std::string nm = node->mName.C_Str();
		std::wstring nmW = WStringFromUtf8(nm);
		FbxSkeletonNode sn{}; sn.name = nm; sn.nameW = nmW; sn.parent = parent; sn.isBone = false;
		m_Skeleton.push_back(std::move(sn));
		m_NodeIndexOfName[m_Skeleton.back().name] = idx;
		for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
		{
			int ch = build(node->mChildren[ci], idx);
			m_Skeleton[idx].children.push_back(ch);
		}
		return idx;
	};
	m_RootIndex = build(scene->mRootNode, -1);
}

void FbxSkeleton::CollectBonesAndOffsets(const aiScene* scene)
{
	m_BoneNames.clear(); m_BoneOffset.clear();
	if (!scene) return;
	std::unordered_map<std::string,int> boneIndexOfName;
	for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi)
	{
		const aiMesh* mesh = scene->mMeshes[mi];
		for (unsigned bi = 0; bi < mesh->mNumBones; ++bi)
		{
			const aiBone* b = mesh->mBones[bi];
			std::string name = b->mName.C_Str();
			if (boneIndexOfName.find(name) == boneIndexOfName.end())
			{
				int newIndex = (int)m_BoneNames.size();
				boneIndexOfName[name] = newIndex;
				m_BoneNames.push_back(name);
				DirectX::XMFLOAT4X4 off;
				off._11 = (float)b->mOffsetMatrix.a1; off._12 = (float)b->mOffsetMatrix.a2; off._13 = (float)b->mOffsetMatrix.a3; off._14 = (float)b->mOffsetMatrix.a4;
				off._21 = (float)b->mOffsetMatrix.b1; off._22 = (float)b->mOffsetMatrix.b2; off._23 = (float)b->mOffsetMatrix.b3; off._24 = (float)b->mOffsetMatrix.b4;
				off._31 = (float)b->mOffsetMatrix.c1; off._32 = (float)b->mOffsetMatrix.c2; off._33 = (float)b->mOffsetMatrix.c3; off._34 = (float)b->mOffsetMatrix.c4;
				off._41 = (float)b->mOffsetMatrix.d1; off._42 = (float)b->mOffsetMatrix.d2; off._43 = (float)b->mOffsetMatrix.d3; off._44 = (float)b->mOffsetMatrix.d4;
				m_BoneOffset.push_back(off);
				auto itNode = m_NodeIndexOfName.find(name);
				if (itNode != m_NodeIndexOfName.end())
				{
					m_Skeleton[itNode->second].isBone = true;
					if (m_Skeleton[itNode->second].nameW.empty()) m_Skeleton[itNode->second].nameW = WStringFromUtf8(name);
				}
			}
		}
	}
}

void FbxSkeleton::BuildRigidBones()
{
	if (!m_BoneNames.empty()) return;
	m_BoneNames.clear(); m_BoneOffset.clear();
	m_BoneNames.reserve(m_Skeleton.size());
	m_BoneOffset.reserve(m_Skeleton.size());
	for (size_t i = 0; i < m_Skeleton.size(); ++i)
	{
		const auto& sn = m_Skeleton[i];
		m_BoneNames.push_back(sn.name);
		DirectX::XMFLOAT4X4 I{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
		m_BoneOffset.push_back(I);
	}
}


