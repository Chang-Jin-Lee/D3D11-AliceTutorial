#include "pch.h"
#include "FbxMaterial.h"
#include "../Helper.h"

#include <directxtk/WICTextureLoader.h>
#include <directxtk/DDSTextureLoader.h>
#include <DirectXTex.h>
#include <wrl/client.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <filesystem>

using Microsoft::WRL::ComPtr;

// 내부 구현: 베이스 컬러 / 메탈릭 / 러프니스 맵을 각각 관리
struct FbxMaterialLoader::Impl
{
	std::vector<ID3D11ShaderResourceView*> baseColorSRVs;
	std::vector<ID3D11ShaderResourceView*> metallicSRVs;
	std::vector<ID3D11ShaderResourceView*> roughnessSRVs;
	std::vector<ID3D11ShaderResourceView*> normalSRVs;
	std::unordered_map<std::wstring, ID3D11ShaderResourceView*> cache;
	ID3D11ShaderResourceView* white = nullptr; // 기본 색상 / roughness 기본값(1)
	ID3D11ShaderResourceView* black = nullptr; // metallic 기본값(0)
};

FbxMaterialLoader::FbxMaterialLoader() : m_(new Impl) {}
FbxMaterialLoader::~FbxMaterialLoader() { Clear(); delete m_; }

void FbxMaterialLoader::Clear()
{
	for (auto* p : m_->baseColorSRVs) SAFE_RELEASE(p);
	for (auto* p : m_->metallicSRVs) SAFE_RELEASE(p);
	for (auto* p : m_->roughnessSRVs) SAFE_RELEASE(p);
	for (auto* p : m_->normalSRVs) SAFE_RELEASE(p);
	m_->baseColorSRVs.clear();
	m_->metallicSRVs.clear();
	m_->roughnessSRVs.clear();
	m_->normalSRVs.clear();

	for (auto& kv : m_->cache) { SAFE_RELEASE(kv.second); }
	m_->cache.clear();

	SAFE_RELEASE(m_->white);
	SAFE_RELEASE(m_->black);
	m_->white = nullptr;
	m_->black = nullptr;
}

const std::vector<ID3D11ShaderResourceView*>& FbxMaterialLoader::GetMaterialSRVs() const
{
	// 기존 코드 호환: diffuse/baseColor 맵
	return m_->baseColorSRVs;
}

const std::vector<ID3D11ShaderResourceView*>& FbxMaterialLoader::GetMetallicSRVs() const
{
	return m_->metallicSRVs;
}

const std::vector<ID3D11ShaderResourceView*>& FbxMaterialLoader::GetRoughnessSRVs() const
{
	return m_->roughnessSRVs;
}

const std::vector<ID3D11ShaderResourceView*>& FbxMaterialLoader::GetNormalSRVs() const
{
	return m_->normalSRVs;
}

static ID3D11ShaderResourceView* FindCached(std::unordered_map<std::wstring, ID3D11ShaderResourceView*>& cache, const std::wstring& key)
{
	auto it = cache.find(key); return (it == cache.end()) ? nullptr : it->second;
}

static void AddCache(std::unordered_map<std::wstring, ID3D11ShaderResourceView*>& cache, const std::wstring& key, ID3D11ShaderResourceView* v)
{
	if (v) { cache[key] = v; v->AddRef(); }
}

// 단색(1x1) 텍스처 SRV 생성 헬퍼
static void CreateSolidColorSRV(ID3D11Device* device, UINT rgba, ID3D11ShaderResourceView** outSRV)
{
	if (!device || !outSRV || *outSRV) return;

	D3D11_TEXTURE2D_DESC td{};
	td.Width = 1;
	td.Height = 1;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_IMMUTABLE;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = &rgba;
	sd.SysMemPitch = sizeof(UINT);

	ComPtr<ID3D11Texture2D> tex;
	HR_T(device->CreateTexture2D(&td, &sd, tex.GetAddressOf()));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
	srvd.Format = td.Format;
	srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvd.Texture2D.MipLevels = 1;
	srvd.Texture2D.MostDetailedMip = 0;
	HR_T(device->CreateShaderResourceView(tex.Get(), &srvd, outSRV));
}

// aiTexture(임베디드 텍스처)로부터 SRV 생성
static ID3D11ShaderResourceView* CreateSRVFromEmbedded(
	ID3D11Device* device,
	const aiTexture* at)
{
	if (!device || !at) return nullptr;

	ComPtr<ID3D11Resource> res;
	ID3D11ShaderResourceView* srv = nullptr;

	if (at->mHeight == 0)
	{
		if (SUCCEEDED(CreateWICTextureFromMemory(
			device,
			reinterpret_cast<const uint8_t*>(at->pcData),
			at->mWidth,
			res.GetAddressOf(),
			&srv)))
		{
			return srv;
		}
	}
	else
	{
		D3D11_TEXTURE2D_DESC td{};
		td.Width = at->mWidth;
		td.Height = at->mHeight;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_IMMUTABLE;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = at->pcData;
		sd.SysMemPitch = at->mWidth * sizeof(aiTexel);

		ComPtr<ID3D11Texture2D> tex;
		if (SUCCEEDED(device->CreateTexture2D(&td, &sd, tex.GetAddressOf())))
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
			srvd.Format = td.Format;
			srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvd.Texture2D.MipLevels = 1;
			srvd.Texture2D.MostDetailedMip = 0;
			if (SUCCEEDED(device->CreateShaderResourceView(tex.Get(), &srvd, &srv)))
			{
				return srv;
			}
		}
	}
	return nullptr;
}


static HRESULT CreateTextureFromTgaFile(ID3D11Device* device, const wchar_t* path, ID3D11ShaderResourceView** outSRV)
{
	if (!device || !path || !outSRV) return E_INVALIDARG;
	*outSRV = nullptr;

	using namespace DirectX;

	TexMetadata metadata{};
	ScratchImage image;
	HRESULT hr = LoadFromTGAFile(path, &metadata, image);
	if (FAILED(hr)) return hr;

	hr = CreateShaderResourceView(
		device,
		image.GetImages(),
		image.GetImageCount(),
		metadata,
		outSRV);
	return hr;
}

// WIC + TGA 지원을 한꺼번에 처리하는 래퍼
static HRESULT CreateTextureFromFileWithTga(
	ID3D11Device* device,
	const std::wstring& path,
	ID3D11Resource** outRes,
	ID3D11ShaderResourceView** outSRV)
{
	if (!device || path.empty() || !outSRV) return E_INVALIDARG;

	ComPtr<ID3D11Resource> dummyRes;
	ID3D11Resource** resPtr = outRes ? outRes : dummyRes.GetAddressOf();

	ID3D11ShaderResourceView* srv = nullptr;
	HRESULT hr = CreateWICTextureFromFile(device, path.c_str(), resPtr, &srv);
	if (FAILED(hr))
	{
		// 확장자가 .tga 이면 직접 파싱 시도
		if (path.size() >= 4)
		{
			std::wstring ext = path.substr(path.size() - 4);
			if (ext == L".tga" || ext == L".TGA")
			{
				hr = CreateTextureFromTgaFile(device, path.c_str(), &srv);
			}
		}
	}

	if (FAILED(hr))
	{
		if (srv) srv->Release();
		return hr;
	}

	*outSRV = srv;
	return S_OK;
}

// 공통 텍스처 로더: aiMaterial + aiTextureType 기반으로 한 장 로드
static ID3D11ShaderResourceView* LoadTextureFromMaterial(
	ID3D11Device* device,
	const aiScene* scene,
	aiMaterial* mat,
	aiTextureType texType,
	const std::wstring& baseDir,
	std::unordered_map<std::wstring, ID3D11ShaderResourceView*>& cache,
	ID3D11ShaderResourceView* fallback)
{
	if (!device || !scene || !mat) return nullptr;

	aiString texPath;
	if (mat->GetTexture(texType, 0, &texPath) != AI_SUCCESS)
	{
		if (fallback) { fallback->AddRef(); }
		return fallback;
	}

	std::string t = texPath.C_Str();

	ID3D11ShaderResourceView* result = nullptr;

	// png, jpg등 으로 해봄
	if (!t.empty())
	{
		const aiTexture* at = scene->GetEmbeddedTexture(t.c_str());
		if (at)
		{
			result = CreateSRVFromEmbedded(device, at);
		}
	}

	// 안되면 tga 해보자 
	if (!result)
	{
		std::wstring wtex = WStringFromUtf8(t);
		bool isAbs = (!wtex.empty() && (wtex.find(L":") != std::wstring::npos || wtex[0] == L'/' || wtex[0] == L'\\'));
		std::wstring full = isAbs ? wtex : (baseDir + wtex);

		if (auto* cached = FindCached(cache, full))
		{
			result = cached;
			result->AddRef();
		}
		else
		{
			ComPtr<ID3D11Resource> res;
			ID3D11ShaderResourceView* srv = nullptr;
			HRESULT hr = CreateTextureFromFileWithTga(device, full, res.GetAddressOf(), &srv);
			if (SUCCEEDED(hr))
			{
				result = srv;
				AddCache(cache, full, srv);
			}
			else
			{
				// FBX가 외부 텍스처를 <fbxname>.fbm 폴더에 풀어놓는 경우 재시도
				std::wstring fileOnly = wtex;
				size_t p = wtex.find_last_of(L"/\\");
				if (p != std::wstring::npos) fileOnly = wtex.substr(p + 1);

				try
				{
					for (const auto& de : std::filesystem::directory_iterator(baseDir))
					{
						if (!de.is_directory()) continue;
						std::wstring dname = de.path().filename().wstring();
						if (dname.size() >= 4)
						{
							std::wstring ext = dname.substr(dname.size() - 4);
							if (ext == L".fbm" || ext == L".FBM")
							{
								std::filesystem::path alt = de.path() / fileOnly;
								ComPtr<ID3D11Resource> res2;
								ID3D11ShaderResourceView* srv2 = nullptr;
								if (SUCCEEDED(CreateTextureFromFileWithTga(device, alt.wstring(), res2.GetAddressOf(), &srv2)))
								{
									result = srv2;
									AddCache(cache, alt.wstring(), srv2);
									break;
								}
							}
						}
					}
				}
				catch (...)
				{
					// directory_iterator 실패 시 무시하고 폴백으로 넘어감
				}
			}
		}
	}

	if (!result && fallback)
	{
		fallback->AddRef();
		result = fallback;
	}

	return result;
}

bool FbxMaterialLoader::Load(ID3D11Device* device, const aiScene* scene, const std::wstring& baseDir)
{
	if (!device || !scene) return false;
	Clear();

	// 1x1 화이트/블랙 텍스처 생성 (폴백 및 기본값)
	CreateSolidColorSRV(device, 0xFFFFFFFF, &m_->white);   // RGBA(1,1,1,1)
	CreateSolidColorSRV(device, 0x000000FF, &m_->black);   // RGBA(0,0,0,1)

	const size_t matCount = scene->mNumMaterials;
	m_->baseColorSRVs.assign(matCount, nullptr);
	m_->metallicSRVs.assign(matCount, nullptr);
	m_->roughnessSRVs.assign(matCount, nullptr);
	m_->normalSRVs.assign(matCount, nullptr);

	for (unsigned m = 0; m < scene->mNumMaterials; ++m)
	{
		aiMaterial* mat = scene->mMaterials[m];

		// BaseColor / Diffuse
		m_->baseColorSRVs[m] = LoadTextureFromMaterial(
			device, scene, mat,
			aiTextureType_DIFFUSE,          // BaseColor
			baseDir,
			m_->cache,
			m_->white);

		// Metallic / Roughness (Assimp PBR 텍스처 타입 사용)
		m_->metallicSRVs[m] = LoadTextureFromMaterial(
			device, scene, mat,
			aiTextureType_METALNESS,
			baseDir,
			m_->cache,
			m_->black);

		m_->roughnessSRVs[m] = LoadTextureFromMaterial(
			device, scene, mat,
			aiTextureType_DIFFUSE_ROUGHNESS,
			baseDir,
			m_->cache,
			m_->white);

		// Normal map (tangent-space). 우선 NORMALS, 없으면 HEIGHT도 한 번 더 시도.
		ID3D11ShaderResourceView* nrm = LoadTextureFromMaterial(
			device, scene, mat,
			aiTextureType_NORMALS,
			baseDir,
			m_->cache,
			nullptr);
		if (!nrm)
		{
			nrm = LoadTextureFromMaterial(
				device, scene, mat,
				aiTextureType_HEIGHT,
				baseDir,
				m_->cache,
				nullptr);
		}
		m_->normalSRVs[m] = nrm;

	}

	return true;
}