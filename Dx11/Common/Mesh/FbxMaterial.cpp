#include "pch.h"
#include "FbxMaterial.h"
#include "../Helper.h"

#include <directxtk/WICTextureLoader.h>
#include <directxtk/DDSTextureLoader.h>
// TGA 뿐 아니라 임베디드 PNG/JPG도 CPU에서 디코드해야 밉 체인을 만들 수 있으므로
// STBI_ONLY_TGA 로 디코더를 좁히지 않는다.
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <wrl/client.h>
#include <assimp/scene.h>
#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <limits>

using Microsoft::WRL::ComPtr;

// 내부 구현: 베이스 컬러 / 메탈릭 / 러프니스 맵을 각각 관리
struct FbxMaterialLoader::Impl
{
	std::vector<ID3D11ShaderResourceView*> baseColorSRVs;
	std::vector<ID3D11ShaderResourceView*> metallicSRVs;
	std::vector<ID3D11ShaderResourceView*> roughnessSRVs;
	std::vector<ID3D11ShaderResourceView*> normalSRVs;
	std::vector<ModelMaterialProcessing::MaterialAlphaInfo> alphaInfos;
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
	m_->alphaInfos.clear();

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

const std::vector<ModelMaterialProcessing::MaterialAlphaInfo>&
FbxMaterialLoader::GetMaterialAlphaInfos() const
{
	return m_->alphaInfos;
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

// ---------------------------------------------------------------------------
// 모델 텍스처 밉맵 체인
//
// 샘플러는 어디서나 MIN_MAG_MIP_LINEAR + MaxLOD = FLOAT32_MAX 로 밉을 요구하는데
// 정작 모델 텍스처에는 밉이 하나뿐이었다. 축소되어 그려지는 캐릭터는 픽셀마다
// 거의 임의의 텍셀을 집게 되고, 포즈가 서브픽셀만 움직여도 다시 뽑히므로 옷이
// 끓어 보인다(shimmer).
//
// 밉은 CPU에서 박스 필터로 만든다. 모델 로딩은 백그라운드 스레드에서 일어나는데
// (App::LoadDataAsync) ID3D11DeviceContext는 스레드 안전하지 않아 GenerateMips/
// UpdateSubresource를 쓸 수 없다. ID3D11Device 메서드는 free-threaded 라서
// 어느 스레드에서 불러도 안전하다.
// ---------------------------------------------------------------------------

// pixels: 4바이트/픽셀, 행 간격 width*4 로 꽉 찬 버퍼.
// 밉 체인 생성이 실패하면 기존과 동일한 단일 밉 텍스처로 폴백한다.
static HRESULT CreateSRVWithCpuMips(
	ID3D11Device* device,
	const void* pixels,
	UINT width,
	UINT height,
	DXGI_FORMAT format,
	ID3D11ShaderResourceView** outSRV)
{
	if (!device || !pixels || !outSRV || width == 0 || height == 0) return E_INVALIDARG;
	*outSRV = nullptr;

	UINT mipCount = 1;
	for (UINT w = width, h = height; w > 1 || h > 1; ++mipCount)
	{
		w = (w > 1) ? (w >> 1) : 1;
		h = (h > 1) ? (h >> 1) : 1;
	}

	std::vector<std::vector<uint8_t>> levels(mipCount);
	std::vector<D3D11_SUBRESOURCE_DATA> sd(mipCount);

	try
	{
		const uint8_t* prev = static_cast<const uint8_t*>(pixels);
		UINT pw = width, ph = height;
		for (UINT level = 1; level < mipCount; ++level)
		{
			const UINT cw = (pw > 1) ? (pw >> 1) : 1;
			const UINT ch = (ph > 1) ? (ph >> 1) : 1;
			levels[level].resize(static_cast<size_t>(cw) * ch * 4);
			uint8_t* dst = levels[level].data();
			if (!ModelTextureProcessing::DownsampleAlphaWeightedRgba8(
				prev, pw, ph, dst, cw, ch))
			{
				mipCount = 1;
				break;
			}
			prev = levels[level].data();
			pw = cw; ph = ch;
		}
	}
	catch (const std::bad_alloc&)
	{
		mipCount = 1; // 메모리가 부족하면 밉 없이라도 올린다.
	}

	{
		UINT pw = width, ph = height;
		for (UINT level = 0; level < mipCount; ++level)
		{
			sd[level].pSysMem = (level == 0) ? pixels : static_cast<const void*>(levels[level].data());
			sd[level].SysMemPitch = pw * 4;
			sd[level].SysMemSlicePitch = 0;
			pw = (pw > 1) ? (pw >> 1) : 1;
			ph = (ph > 1) ? (ph >> 1) : 1;
		}
	}

	D3D11_TEXTURE2D_DESC td{};
	td.Width = width;
	td.Height = height;
	td.MipLevels = mipCount;
	td.ArraySize = 1;
	td.Format = format;
	td.SampleDesc.Count = 1;
	td.Usage = D3D11_USAGE_IMMUTABLE;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	ComPtr<ID3D11Texture2D> tex;
	HRESULT hr = device->CreateTexture2D(&td, sd.data(), tex.GetAddressOf());
	if (FAILED(hr) && mipCount > 1)
	{
		// 밉 체인 생성 실패 → 기존과 동일한 단일 밉 텍스처로 폴백
		mipCount = 1;
		td.MipLevels = 1;
		hr = device->CreateTexture2D(&td, sd.data(), tex.GetAddressOf());
	}
	if (FAILED(hr)) return hr;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
	srvd.Format = format;
	srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvd.Texture2D.MipLevels = mipCount;
	srvd.Texture2D.MostDetailedMip = 0;
	return device->CreateShaderResourceView(tex.Get(), &srvd, outSRV);
}

// 압축 이미지(PNG/JPG 등)를 CPU에서 디코드해 밉 체인과 함께 올린다.
// 디코드할 수 없으면(지원하지 않는 포맷, 16비트 PNG 등) nullptr을 돌려주고
// 호출부가 기존 DirectXTK 경로로 폴백한다.
static ID3D11ShaderResourceView* CreateSRVFromCompressedWithMips(
	ID3D11Device* device,
	const uint8_t* data,
	size_t size)
{
	if (!device || !data || size == 0) return nullptr;
	if (size > static_cast<size_t>(std::numeric_limits<int>::max())) return nullptr;

	// 16비트 PNG는 stb가 8비트로 떨어뜨리므로 기존 WIC 경로에 맡긴다.
	if (stbi_is_16_bit_from_memory(data, static_cast<int>(size))) return nullptr;

	int w = 0, h = 0, comp = 0;
	stbi_uc* px = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &comp, STBI_rgb_alpha);
	if (!px) return nullptr;
	if (w <= 0 || h <= 0) { stbi_image_free(px); return nullptr; }

	ID3D11ShaderResourceView* srv = nullptr;
	HRESULT hr = CreateSRVWithCpuMips(device, px, static_cast<UINT>(w), static_cast<UINT>(h),
		DXGI_FORMAT_R8G8B8A8_UNORM, &srv);
	stbi_image_free(px);
	if (FAILED(hr)) { if (srv) srv->Release(); return nullptr; }
	return srv;
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
		// 밉 체인까지 만드는 경로를 먼저 쓰고, 디코드가 안 되면 기존 경로로 폴백한다.
		if (auto* mipped = CreateSRVFromCompressedWithMips(
			device,
			reinterpret_cast<const uint8_t*>(at->pcData),
			at->mWidth))
		{
			return mipped;
		}

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
		// RAW BGRA8. 밉 체인을 만들어 축소 시 텍셀이 튀는 현상(shimmer)을 막는다.
		if (SUCCEEDED(CreateSRVWithCpuMips(
			device,
			at->pcData,
			at->mWidth,
			at->mHeight,
			DXGI_FORMAT_B8G8R8A8_UNORM,
			&srv)))
		{
			return srv;
		}
	}
	return nullptr;
}


static HRESULT CreateTextureFromTgaFile(ID3D11Device* device, const wchar_t* path, ID3D11ShaderResourceView** outSRV)
{
	if (!device || !path || !outSRV) return E_INVALIDARG;
	*outSRV = nullptr;

	std::ifstream file(std::filesystem::path(path), std::ios::binary);
	if (!file) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

	std::vector<unsigned char> bytes(
		(std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	if (bytes.empty()) return E_FAIL;
	if (bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);

	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, STBI_rgb_alpha);
	if (!pixels || width <= 0 || height <= 0)
	{
		if (pixels) stbi_image_free(pixels);
		return E_FAIL;
	}

	HRESULT hr = CreateSRVWithCpuMips(
		device,
		pixels,
		static_cast<UINT>(width),
		static_cast<UINT>(height),
		DXGI_FORMAT_R8G8B8A8_UNORM,
		outSRV);
	stbi_image_free(pixels);
	return hr;
}

static std::wstring LowerExtension(const std::wstring& path)
{
	std::wstring ext = std::filesystem::path(path).extension().wstring();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
	return ext;
}

// 외부 이미지 파일을 통째로 읽어 밉 체인과 함께 올린다. 실패하면 nullptr.
static ID3D11ShaderResourceView* CreateSRVFromFileWithMips(ID3D11Device* device, const std::wstring& path)
{
	if (!device || path.empty()) return nullptr;
	try
	{
		std::ifstream f(std::filesystem::path(path), std::ios::binary);
		if (!f) return nullptr;
		std::vector<uint8_t> bytes(
			(std::istreambuf_iterator<char>(f)),
			std::istreambuf_iterator<char>());
		if (bytes.empty()) return nullptr;
		return CreateSRVFromCompressedWithMips(device, bytes.data(), bytes.size());
	}
	catch (...)
	{
		return nullptr;
	}
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
	std::wstring ext = LowerExtension(path);

	// 밉 체인까지 만드는 CPU 경로를 먼저 시도한다. DDS는 보통 이미 밉을 갖고 있어 제외.
	if (ext != L".dds")
	{
		if (auto* mipped = CreateSRVFromFileWithMips(device, path))
		{
			*outSRV = mipped;
			return S_OK;
		}
	}

	HRESULT hr = (ext == L".dds")
		? CreateDDSTextureFromFile(device, path.c_str(), resPtr, &srv)
		: CreateWICTextureFromFile(device, path.c_str(), resPtr, &srv);
	if (FAILED(hr))
	{
		// 확장자가 .tga 이면 직접 파싱 시도
		if (ext == L".tga")
		{
			hr = CreateTextureFromTgaFile(device, path.c_str(), &srv);
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
	m_->alphaInfos.assign(matCount, {});

	for (unsigned m = 0; m < scene->mNumMaterials; ++m)
	{
		aiMaterial* mat = scene->mMaterials[m];

		aiString alphaModeValue;
		std::string alphaMode = "OPAQUE";
		if (mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaModeValue) == AI_SUCCESS)
			alphaMode = alphaModeValue.C_Str();
		const std::uint32_t parsedAlphaMode =
			ModelMaterialProcessing::ParseAlphaMode(alphaMode);
		float authoredCutoff = 0.0f;
		const bool hasAuthoredCutoff =
			mat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, authoredCutoff) == AI_SUCCESS;
		m_->alphaInfos[m].mode =
			static_cast<ModelMaterialProcessing::MaterialAlphaMode>(parsedAlphaMode);
		m_->alphaInfos[m].cutoff = ModelMaterialProcessing::ResolveAlphaCutoff(
			parsedAlphaMode, hasAuthoredCutoff, authoredCutoff);

		// glTF PBR base color first, then legacy diffuse for FBX/OBJ/PMX.
		m_->baseColorSRVs[m] = LoadTextureFromMaterial(
			device, scene, mat,
			aiTextureType_BASE_COLOR,
			baseDir,
			m_->cache,
			nullptr);
		if (!m_->baseColorSRVs[m])
		{
			m_->baseColorSRVs[m] = LoadTextureFromMaterial(
				device, scene, mat,
				aiTextureType_DIFFUSE,
				baseDir,
				m_->cache,
				m_->white);
		}

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
