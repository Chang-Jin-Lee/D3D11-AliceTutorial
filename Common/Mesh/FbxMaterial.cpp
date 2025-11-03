#include "pch.h"
#include "FbxMaterial.h"
#include "../Helper.h"

#include <directxtk/WICTextureLoader.h>
#include <wrl/client.h>
#include <assimp/scene.h>

using Microsoft::WRL::ComPtr;

struct FbxMaterialLoader::Impl
{
	std::vector<ID3D11ShaderResourceView*> materialSRVs;
	std::unordered_map<std::wstring, ID3D11ShaderResourceView*> cache;
	ID3D11ShaderResourceView* white = nullptr;
};

FbxMaterialLoader::FbxMaterialLoader() : m_(new Impl) {}
FbxMaterialLoader::~FbxMaterialLoader() { Clear(); delete m_; }

void FbxMaterialLoader::Clear()
{
	for (auto* p : m_->materialSRVs) SAFE_RELEASE(p);
	m_->materialSRVs.clear();
	for (auto& kv : m_->cache) { SAFE_RELEASE(kv.second); }
	m_->cache.clear();
    SAFE_RELEASE(m_->white);
    m_->white = nullptr;
}

const std::vector<ID3D11ShaderResourceView*>& FbxMaterialLoader::GetMaterialSRVs() const
{
	return m_->materialSRVs;
}

static ID3D11ShaderResourceView* FindCached(std::unordered_map<std::wstring, ID3D11ShaderResourceView*>& cache, const std::wstring& key)
{
	auto it = cache.find(key); return (it == cache.end()) ? nullptr : it->second;
}

static void AddCache(std::unordered_map<std::wstring, ID3D11ShaderResourceView*>& cache, const std::wstring& key, ID3D11ShaderResourceView* v)
{
	if (v) { cache[key] = v; v->AddRef(); }
}

bool FbxMaterialLoader::Load(ID3D11Device* device, const aiScene* scene, const std::wstring& baseDir)
{
	if (!device || !scene) return false;
	Clear();

	if (!m_->white)
	{
		UINT white = 0xFFFFFFFF;
		D3D11_TEXTURE2D_DESC td{}; td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = &white; sd.SysMemPitch = sizeof(UINT);
		ComPtr<ID3D11Texture2D> tex; HR_T(device->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
		D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MipLevels = 1; srvd.Texture2D.MostDetailedMip = 0;
		HR_T(device->CreateShaderResourceView(tex.Get(), &srvd, &m_->white));
	}

	m_->materialSRVs.assign(scene->mNumMaterials, nullptr);

	for (unsigned m = 0; m < scene->mNumMaterials; ++m)
	{
		aiMaterial* mat = scene->mMaterials[m];
		aiString texPath;
		if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
		{
			std::string t = texPath.C_Str();

			// Embedded
			if (!t.empty())
			{
				const aiTexture* at = scene->GetEmbeddedTexture(t.c_str());
				if (at)
				{
					ComPtr<ID3D11Resource> res; ID3D11ShaderResourceView* srv = nullptr;
					if (at->mHeight == 0)
					{
						if (SUCCEEDED(CreateWICTextureFromMemory(device, reinterpret_cast<const uint8_t*>(at->pcData), at->mWidth, res.GetAddressOf(), &srv)))
							m_->materialSRVs[m] = srv;
					}
					else
					{
						D3D11_TEXTURE2D_DESC td{}; td.Width = at->mWidth; td.Height = at->mHeight; td.MipLevels = 1; td.ArraySize = 1;
						td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
						D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = at->pcData; sd.SysMemPitch = at->mWidth * sizeof(aiTexel);
						ComPtr<ID3D11Texture2D> tex;
						if (SUCCEEDED(device->CreateTexture2D(&td, &sd, tex.GetAddressOf())))
						{
							D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MipLevels = 1; srvd.Texture2D.MostDetailedMip = 0;
							if (SUCCEEDED(device->CreateShaderResourceView(tex.Get(), &srvd, &srv))) m_->materialSRVs[m] = srv;
						}
					}
				}
			}

			// Legacy *index
			if (!m_->materialSRVs[m] && !t.empty() && t[0] == '*')
			{
				int idx = atoi(t.c_str() + 1);
				if (idx >= 0 && (unsigned)idx < scene->mNumTextures)
				{
					const aiTexture* at = scene->mTextures[idx];
					if (at)
					{
						ComPtr<ID3D11Resource> res; ID3D11ShaderResourceView* srv = nullptr;
						if (at->mHeight == 0)
						{
							if (SUCCEEDED(CreateWICTextureFromMemory(device, reinterpret_cast<const uint8_t*>(at->pcData), at->mWidth, res.GetAddressOf(), &srv)))
								m_->materialSRVs[m] = srv;
						}
						else
						{
							D3D11_TEXTURE2D_DESC td{}; td.Width = at->mWidth; td.Height = at->mHeight; td.MipLevels = 1; td.ArraySize = 1;
							td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
							D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = at->pcData; sd.SysMemPitch = at->mWidth * sizeof(aiTexel);
							ComPtr<ID3D11Texture2D> tex;
							if (SUCCEEDED(device->CreateTexture2D(&td, &sd, tex.GetAddressOf())))
							{
								D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MipLevels = 1; srvd.Texture2D.MostDetailedMip = 0;
								if (SUCCEEDED(device->CreateShaderResourceView(tex.Get(), &srvd, &srv))) m_->materialSRVs[m] = srv;
							}
						}
					}
				}
			}

			// External file
			if (!m_->materialSRVs[m])
			{
				std::wstring wtex = WStringFromUtf8(t);
				bool isAbs = (!wtex.empty() && (wtex.find(L":") != std::wstring::npos || wtex[0] == L'/' || wtex[0] == L'\\'));
				std::wstring full = isAbs ? wtex : (baseDir + wtex);
				if (auto* cached = FindCached(m_->cache, full)) { m_->materialSRVs[m] = cached; cached->AddRef(); }
				else
				{
					ComPtr<ID3D11Resource> res; ID3D11ShaderResourceView* srv = nullptr;
					if (SUCCEEDED(CreateWICTextureFromFile(device, full.c_str(), res.GetAddressOf(), &srv))) { m_->materialSRVs[m] = srv; AddCache(m_->cache, full, srv); }
				}
			}
		}
		if (!m_->materialSRVs[m]) { m_->materialSRVs[m] = m_->white; if (m_->white) m_->white->AddRef(); }
	}
	return true;
}


