#include "pch.h"
#include "AssetManager.h"
#include "./Mesh/FbxModel.h"

std::shared_ptr<FbxModel> AssetManager::GetFbxModel(ID3D11Device* device, const std::wstring& pathW)
{
	if (!device || pathW.empty()) return nullptr;

	std::lock_guard<std::mutex> lock(m_Mutex);

	// 1) 캐시 조회
	if (auto it = m_FbxCache.find(pathW); it != m_FbxCache.end())
	{
		if (auto sp = it->second.lock())
		{
			return sp; // 재사용
		}
		else
		{
			m_FbxCache.erase(it); // 정리
		}
	}

	// 2) 새로 로드
	auto model = std::make_shared<FbxModel>();
	if (!model->Load(device, pathW))
	{
		return nullptr;
	}
	m_FbxCache[pathW] = model;
	return model;
}



