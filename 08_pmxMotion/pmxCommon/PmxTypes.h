#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>

namespace pmx
{
	struct Vertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 uv;
		uint16_t boneIndex[4] = {0,0,0,0};
		DirectX::XMFLOAT4 boneWeight = {0,0,0,0};
	};

	struct Subset { uint32_t startIndex; uint32_t indexCount; uint32_t materialIndex; };

	struct Bone
	{
		std::wstring name;
		int nodeIndex = -1;
		DirectX::XMMATRIX offset = DirectX::XMMatrixIdentity();
	};

	struct Skeleton
	{
		std::vector<std::wstring> nodeName;
		std::vector<int> parent;
		std::unordered_map<std::wstring,int> nameToIndex;
		std::vector<DirectX::XMMATRIX> localBind;
		std::vector<DirectX::XMMATRIX> localAnim;
		std::vector<DirectX::XMMATRIX> global;
		std::vector<Bone> bones;
	};

	struct VmdKey { uint32_t frame; DirectX::XMFLOAT3 pos; DirectX::XMFLOAT4 rot; };
	using VmdTracks = std::unordered_map<std::wstring, std::vector<VmdKey>>;
} 