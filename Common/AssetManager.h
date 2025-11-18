// ======================================= 경로 기반 ============================================================
//#pragma once
//#include <memory>
//#include <string>
//#include <unordered_map>
//#include "Singleton.h"
//
//class FbxModel;
//class ObjManager;
//class PmxManager;
//struct ID3D11Device;
//
//// 에셋 종류. 3D 모델들만 일단은.
//enum class EAssetKind
//{
//	FbxModel,
//	ObjModel,
//	PmxModel,
//};
//
//// 경로 기반 에셋 키
//struct AssetKey
//{
//	std::wstring PathW;
//	EAssetKind Kind;
//	
//	bool operator==(const AssetKey& other) const
//	{
//		return Kind == other.Kind && PathW == other.PathW;
//	}
//	
//	// unordered_map에서 사용하기 위한 해시 함수입니다
//	struct Hash
//	{
//		std::size_t operator()(const AssetKey& key) const
//		{
//			std::hash<std::wstring> hashWString;
//			std::hash<int> hashInt;
//			return hashWString(key.PathW) ^ (hashInt(static_cast<int>(key.Kind)) << 1);
//		}
//	};
//};
//
//// 에셋 매니저: 다양한 모델 타입 공유/캐시
//class AssetManager : public Singleton<AssetManager>
//{
//public:
//	AssetManager() = default;
//	
//	// FBX 모델을 긁어봄. 이미 있으면 재사용, 없으면 로드
//	std::shared_ptr<FbxModel> GetFbxModel(ID3D11Device* device, const std::wstring& pathW);
//	
//	// OBJ 모델을 긁어봄. 이미 있으면 재사용, 없으면 로드
//	std::shared_ptr<ObjManager> GetObjModel(ID3D11Device* device, const std::wstring& pathW);
//	
//	// PMX 모델을 긁어봄. 이미 있으면 재사용, 없으면 로드
//	std::shared_ptr<PmxManager> GetPmxModel(ID3D11Device* device, const std::wstring& pathW);
//
//private:
//	// 내부 캐시는 각 에셋 종류별로 별도로 캐시 관리함.
//	// TODO : 추후에 인터페이스 넣던가 해서 하나로 합쳐야함.
//	std::unordered_map<AssetKey, std::weak_ptr<FbxModel>, AssetKey::Hash> m_FbxCache;
//	std::unordered_map<AssetKey, std::weak_ptr<ObjManager>, AssetKey::Hash> m_ObjCache;
//	std::unordered_map<AssetKey, std::weak_ptr<PmxManager>, AssetKey::Hash> m_PmxCache;
//};

// ======================================= 데이터 기반 ============================================================
#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include <fstream>
#include <filesystem>
#include "Singleton.h"

// 전방 선언으로 의존 최소화
class FbxModel;
struct ID3D11Device;

enum class EAssetKind
{
	FbxModel,
};

// 파일 내용 기반 해시 키 (경로가 달라도 같은 파일이면 같은 키)
struct AssetKey
{
	uint64_t FileHash;  // 파일 내용 해시
	EAssetKind Kind;

	bool operator==(const AssetKey& other) const
	{
		return Kind == other.Kind && FileHash == other.FileHash;
	}

	struct Hash
	{
		std::size_t operator()(const AssetKey& key) const
		{
			std::hash<uint64_t> hashU64;
			std::hash<int> hashInt;
			return hashU64(key.FileHash) ^ (hashInt(static_cast<int>(key.Kind)) << 1);
		}
	};
};

// 간단 에셋 매니저: FBX 모델 공유/캐시 (파일 내용 기반 해시 사용)
class AssetManager : public Singleton<AssetManager>
{
public:
	AssetManager() = default;
	// FBX 모델을 공유 포인터로 획득 (이미 있으면 재사용, 없으면 로드)
	// 경로가 달라도 같은 파일이면 캐시에서 재사용
	std::shared_ptr<FbxModel> GetFbxModel(ID3D11Device* device, const std::wstring& pathW);

private:
	// 파일 내용 해시 계산 (FNV-1a 해시 알고리즘 사용)
	static uint64_t ComputeFileHash(const std::wstring& pathW);

	// 내부 캐시: 파일 해시 기반
	std::unordered_map<AssetKey, std::weak_ptr<FbxModel>, AssetKey::Hash> m_FbxCache;
	// 경로 -> 해시 매핑 (경로로 해시를 빠르게 찾기 위해)
	std::unordered_map<std::wstring, uint64_t> m_PathToHash;
	std::mutex m_Mutex;
};

