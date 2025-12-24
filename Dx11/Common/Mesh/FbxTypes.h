#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

// Direct3D forward declarations
struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct aiScene;
struct aiNodeAnim;

namespace DirectX { struct XMFLOAT4X4; struct XMMATRIX; }

// Subset information for indexed draw calls (material-bound ranges)
struct FbxSubset
{
	uint32_t startIndex = 0;
	uint32_t indexCount = 0;
	uint32_t materialIndex = 0;
};

// Simple skeleton node used for UI and traversal
struct FbxSkeletonNode
{
	std::string name;      // UTF-8
	std::wstring nameW;    // Debug/UI
	int parent = -1;
	std::vector<int> children;
	bool isBone = false;
};


