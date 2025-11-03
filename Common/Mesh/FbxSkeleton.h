#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "FbxTypes.h"

struct aiScene;

// Builds skeleton tree, node ¡æ index map, bone lists and offsets
class FbxSkeleton
{
public:
	FbxSkeleton() = default;
	~FbxSkeleton() = default;

	void BuildFromScene(const aiScene* scene);
	void CollectBonesAndOffsets(const aiScene* scene);

	const std::vector<FbxSkeletonNode>& GetSkeleton() const { return m_Skeleton; }
	int GetRootIndex() const { return m_RootIndex; }

	// Bone info
	bool HasBones() const { return !m_BoneNames.empty(); }
	const std::vector<std::string>& GetBoneNames() const { return m_BoneNames; }
	const std::vector<DirectX::XMFLOAT4X4>& GetBoneOffsets() const { return m_BoneOffset; }

	// Maps
	const std::unordered_map<std::string,int>& NodeIndexOfName() const { return m_NodeIndexOfName; }

	// Rigid helper (build bones from skeleton)
	void BuildRigidBones();

private:
	std::vector<FbxSkeletonNode> m_Skeleton;
	int m_RootIndex = -1;
	std::unordered_map<std::string,int> m_NodeIndexOfName;
	std::vector<std::string> m_BoneNames;
	std::vector<DirectX::XMFLOAT4X4> m_BoneOffset;
};


