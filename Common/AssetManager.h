#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include "Singleton.h"

// 전방 선언으로 의존 최소화
class FbxModel;
struct ID3D11Device;

// 간단 에셋 매니저: FBX 모델 공유/캐시
class AssetManager : public Singleton<AssetManager>
{
public:
	AssetManager() = default;
	// FBX 모델을 공유 포인터로 획득 (이미 있으면 재사용, 없으면 로드)
	std::shared_ptr<FbxModel> GetFbxModel(ID3D11Device* device, const std::wstring& pathW);

private:
	// 내부 캐시
	std::unordered_map<std::wstring, std::weak_ptr<FbxModel>> m_FbxCache;
	std::mutex m_Mutex;
};

