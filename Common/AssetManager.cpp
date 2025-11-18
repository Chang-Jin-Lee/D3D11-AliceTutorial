// ======================================= 경로 기반 ============================================================
//#include "pch.h"
//#include "AssetManager.h"
//#include "./Mesh/FbxModel.h"
//#include "./ObjManager.h"
//#include "./PmxManager.h"
//
//std::shared_ptr<FbxModel> AssetManager::GetFbxModel(ID3D11Device* device, const std::wstring& pathW)
//{
//	if (!device || pathW.empty()) return nullptr;
//
//	// AssetKey로 캐시 조회
//	AssetKey key{ pathW, EAssetKind::FbxModel };
//	if (auto it = m_FbxCache.find(key); it != m_FbxCache.end())
//	{
//		if (auto sp = it->second.lock())
//		{
//			return sp; // 재사용
//		}
//		else
//		{
//			m_FbxCache.erase(it); // 만료된 weak_ptr 정리
//		}
//	}
//
//	// 새로 로드
//	auto model = std::make_shared<FbxModel>();
//	if (!model->Load(device, pathW))
//	{
//		return nullptr;
//	}
//	m_FbxCache[key] = model;
//	return model;
//}
//
//std::shared_ptr<ObjManager> AssetManager::GetObjModel(ID3D11Device* device, const std::wstring& pathW)
//{
//	if (!device || pathW.empty()) return nullptr;
//
//	// AssetKey로 캐시 조회
//	AssetKey key{ pathW, EAssetKind::ObjModel };
//	if (auto it = m_ObjCache.find(key); it != m_ObjCache.end())
//	{
//		if (auto sp = it->second.lock())
//		{
//			return sp; // 재사용
//		}
//		else
//		{
//			m_ObjCache.erase(it); // 만료된 weak_ptr 정리
//		}
//	}
//
//	// 새로 로드
//	auto model = std::make_shared<ObjManager>();
//	if (!model->Load(device, pathW))
//	{
//		return nullptr;
//	}
//	m_ObjCache[key] = model;
//	return model;
//}
//
//std::shared_ptr<PmxManager> AssetManager::GetPmxModel(ID3D11Device* device, const std::wstring& pathW)
//{
//	if (!device || pathW.empty()) return nullptr;
//
//	// AssetKey로 캐시 조회
//	AssetKey key{ pathW, EAssetKind::PmxModel };
//	if (auto it = m_PmxCache.find(key); it != m_PmxCache.end())
//	{
//		if (auto sp = it->second.lock())
//		{
//			return sp; // 재사용
//		}
//		else
//		{
//			m_PmxCache.erase(it); // 만료된 weak_ptr 정리
//		}
//	}
//
//	// 새로 로드
//	auto model = std::make_shared<PmxManager>();
//	if (!model->Load(device, pathW))
//	{
//		return nullptr;
//	}
//	m_PmxCache[key] = model;
//	return model;
//}

// ======================================= 데이터 기반 ============================================================
#include "pch.h"
#include "AssetManager.h"
#include "./Mesh/FbxModel.h"

// FNV-1a 64비트 해시 함수 (파일 내용 해시 계산용)
static uint64_t FNV1a64(const uint8_t* data, size_t size)
{
	const uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
	const uint64_t FNV_PRIME = 1099511628211ULL;

	uint64_t hash = FNV_OFFSET_BASIS;
	for (size_t i = 0; i < size; ++i)
	{
		hash ^= static_cast<uint64_t>(data[i]);
		hash *= FNV_PRIME;
	}
	return hash;
}

// 파일 내용 기반 해시 계산
uint64_t AssetManager::ComputeFileHash(const std::wstring& pathW)
{
	if (!std::filesystem::exists(pathW)) return 0;

	try
	{
		// 파일 크기와 수정 시간으로 빠른 해시 계산
		auto fileSize = std::filesystem::file_size(pathW);
		auto writeTime = std::filesystem::last_write_time(pathW);
		
		// 파일 크기와 수정 시간을 해시에 포함
		std::uint64_t timeHash = static_cast<std::uint64_t>(writeTime.time_since_epoch().count());
		uint64_t sizeHash = fileSize;

		// 파일의 처음과 끝 부분 일부 바이트를 읽어서 해시에 포함 (더 정확한 검증)
		const size_t SAMPLE_SIZE = 4096; // 처음과 끝에서 각각 4KB씩 샘플링
		std::ifstream file(pathW, std::ios::binary);
		if (!file.is_open()) return 0;

		uint8_t buffer[SAMPLE_SIZE * 2] = { 0 };
		size_t readSize = 0;

		// 파일 시작 부분 읽기
		if (fileSize > 0)
		{
			size_t toRead = (std::min)(SAMPLE_SIZE, static_cast<size_t>(fileSize));
			file.read(reinterpret_cast<char*>(buffer), toRead);
			readSize += static_cast<size_t>(file.gcount());
		}

		// 파일 끝 부분 읽기 (파일이 충분히 큰 경우)
		if (fileSize > SAMPLE_SIZE)
		{
			file.seekg(-static_cast<std::streamoff>((std::min)(SAMPLE_SIZE, static_cast<size_t>(fileSize))), std::ios::end);
			file.read(reinterpret_cast<char*>(buffer + readSize), SAMPLE_SIZE);
			readSize += static_cast<size_t>(file.gcount());
		}

		file.close();

		// 파일 크기, 수정 시간, 샘플 데이터를 조합하여 해시 계산
		uint64_t sampleHash = FNV1a64(buffer, readSize);

		// 최종 해시: 크기, 시간, 샘플 데이터를 조합
		uint64_t finalHash = sizeHash;
		finalHash ^= (timeHash << 1);
		finalHash ^= (sampleHash << 2);

		return finalHash;
	}
	catch (...)
	{
		return 0;
	}
}

std::shared_ptr<FbxModel> AssetManager::GetFbxModel(ID3D11Device* device, const std::wstring& pathW)
{
	if (!device || pathW.empty()) return nullptr;

	std::lock_guard<std::mutex> lock(m_Mutex);

	// 1) 경로로 해시 조회 (이미 계산된 해시가 있는지 확인)
	uint64_t fileHash = 0;
	if (auto pathIt = m_PathToHash.find(pathW); pathIt != m_PathToHash.end())
	{
		fileHash = pathIt->second;
	}
	else
	{
		// 2) 파일 해시 계산
		fileHash = ComputeFileHash(pathW);
		if (fileHash == 0) return nullptr; // 파일이 없거나 읽을 수 없음

		// 경로 -> 해시 매핑 저장
		m_PathToHash[pathW] = fileHash;
	}

	// 3) 해시 기반 캐시 조회
	AssetKey key{ fileHash, EAssetKind::FbxModel };
	if (auto it = m_FbxCache.find(key); it != m_FbxCache.end())
	{
		if (auto sp = it->second.lock())
		{
			return sp; // 재사용 (같은 파일 내용이므로)
		}
		else
		{
			m_FbxCache.erase(it); // 만료된 weak_ptr 정리
		}
	}

	// 4) 새로 로드
	auto model = std::make_shared<FbxModel>();
	if (!model->Load(device, pathW))
	{
		return nullptr;
	}

	// 5) 해시 기반 캐시에 저장
	m_FbxCache[key] = model;
	return model;
}