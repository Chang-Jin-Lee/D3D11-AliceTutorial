#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>
#include "PmxTypes.h"

namespace pmx
{
	bool LoadVMD(const std::wstring& path, VmdTracks& outTracks, uint32_t& outMaxFrame);
	void SampleTrack(const std::vector<VmdKey>& keys, float tFrame, DirectX::XMFLOAT3& outP, DirectX::XMFLOAT4& outQ);
	std::wstring NormalizeBoneName(const std::wstring& in);
} 