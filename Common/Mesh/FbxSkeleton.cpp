#include "pch.h"
#include "FbxSkeleton.h"
#include "../Helper.h"

#include <assimp/scene.h>
#include <queue>

void FbxSkeleton::BuildFromScene(const aiScene* scene)
{
	m_Skeleton.clear(); m_NodeIndexOfName.clear(); m_RootIndex = -1;
	if (!scene || !scene->mRootNode) return;
    // 1) 노드 수를 세어 미리 reserve (재할당 최소화)
    auto countNodes = [&](const aiNode* root){
        size_t cnt = 0; std::queue<const aiNode*> q; q.push(root);
        while (!q.empty()) { const aiNode* n = q.front(); q.pop(); ++cnt; for (unsigned i=0;i<n->mNumChildren;++i) q.push(n->mChildren[i]); }
        return cnt;
    };
    size_t nodeCount = countNodes(scene->mRootNode);
    m_Skeleton.reserve(nodeCount);
    m_NodeIndexOfName.reserve(nodeCount);

    // 2) 비재귀 BFS로 부모→자식 순서로 생성
    std::queue<std::pair<const aiNode*, int>> q; // (node, parentIndex)
    q.push({ scene->mRootNode, -1 });
    int rootIndex = -1;
    while (!q.empty())
    {
        const aiNode* node = q.front().first; int parent = q.front().second; q.pop();
        int idx = (int)m_Skeleton.size();
        if (rootIndex < 0) rootIndex = idx;
        std::string nm = node->mName.C_Str();
        std::wstring nmW = WStringFromUtf8(nm);
        FbxSkeletonNode sn{}; sn.name = nm; sn.nameW = nmW; sn.parent = parent; sn.isBone = false;
        m_Skeleton.push_back(std::move(sn));
        m_NodeIndexOfName[m_Skeleton.back().name] = idx;
        if (parent >= 0) m_Skeleton[parent].children.push_back(idx);
        for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
            q.push({ node->mChildren[ci], idx });
    }
    m_RootIndex = rootIndex;
}

void FbxSkeleton::CollectBonesAndOffsets(const aiScene* scene)
{
    m_BoneNames.clear(); m_BoneOffset.clear();
    if (!scene) return;
    // 상한치 기반 reserve로 리해시/재할당 최소화
    size_t totalBonesUpper = 0;
    for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi) totalBonesUpper += scene->mMeshes[mi]->mNumBones;
    std::unordered_map<std::string,int> boneIndexOfName; boneIndexOfName.reserve(totalBonesUpper);
    m_BoneNames.reserve(totalBonesUpper); m_BoneOffset.reserve(totalBonesUpper);
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
                boneIndexOfName.emplace(name, newIndex);
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


