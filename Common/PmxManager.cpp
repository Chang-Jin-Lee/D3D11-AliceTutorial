#include "pch.h"
#include "PmxManager.h"
#include "Helper.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <directxtk/WICTextureLoader.h>

using Microsoft::WRL::ComPtr;

static std::wstring WStringFromUtf8(const std::string& s) { return std::wstring(s.begin(), s.end()); }

void PmxManager::Release()
{
	for (auto& srv : m_MaterialSRVs) SAFE_RELEASE(srv);
	m_MaterialSRVs.clear();
	SAFE_RELEASE(m_pWhiteSRV);
	for (auto& kv : m_TexCache) SAFE_RELEASE(kv.second);
	m_TexCache.clear();
	SAFE_RELEASE(m_pVB);
	SAFE_RELEASE(m_pIB);
	m_Vertices.clear();
	m_Indices.clear();
	m_Subsets.clear();
	m_IndexCount = 0;
}

bool PmxManager::Load(ID3D11Device* device, const std::wstring& pmxPath)
{
	Release();
	Assimp::Importer importer;
	std::string pathA(pmxPath.begin(), pmxPath.end());
	const aiScene* scene = importer.ReadFile(pathA,
		aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
		aiProcess_ImproveCacheLocality | aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded);
	if (!scene || !scene->HasMeshes())
	{
		return false;
	}

	std::wstring modelPathW = pmxPath;
	size_t slash = modelPathW.find_last_of(L"/\\");
	std::wstring baseDir = (slash == std::wstring::npos) ? L"" : modelPathW.substr(0, slash + 1);

	if (!LoadMaterials(device, scene, baseDir)) return false;
	if (!BuildMeshBuffers(device, scene)) return false;
	return true;
}

bool PmxManager::LoadMaterials(ID3D11Device* device, const aiScene* scene, const std::wstring& baseDir)
{
	m_MaterialSRVs.clear();
	m_MaterialSRVs.resize(scene->mNumMaterials, nullptr);

	auto findCached = [&](const std::wstring& key)->ID3D11ShaderResourceView*{
		for (auto& kv : m_TexCache) if (kv.first == key) return kv.second; return nullptr;
	};
	auto addCache = [&](const std::wstring& key, ID3D11ShaderResourceView* v){ if (v){ m_TexCache.push_back({key, v}); v->AddRef(); } };

	for (unsigned m = 0; m < scene->mNumMaterials; ++m)
	{
		aiMaterial* mat = scene->mMaterials[m];
		aiString texPath;
		if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
		{
			ID3D11ShaderResourceView* srv = nullptr;
			std::string t = texPath.C_Str();
			if (!t.empty() && t[0] == '*')
			{
				int idx = atoi(t.c_str() + 1);
				if (idx >= 0 && (unsigned)idx < scene->mNumTextures)
				{
					const aiTexture* at = scene->mTextures[idx];
					if (at)
					{
						if (at->mHeight == 0)
						{
							ComPtr<ID3D11Resource> res;
							if (SUCCEEDED(CreateWICTextureFromMemory(device, reinterpret_cast<const uint8_t*>(at->pcData), at->mWidth, res.GetAddressOf(), &srv)))
								m_MaterialSRVs[m] = srv;
						}
						else
						{
							D3D11_TEXTURE2D_DESC td{};
							td.Width = at->mWidth; td.Height = at->mHeight; td.MipLevels = 1; td.ArraySize = 1;
							td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // aiTexelÀº BGRA
							td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
							D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = at->pcData; sd.SysMemPitch = at->mWidth * sizeof(aiTexel);
							ComPtr<ID3D11Texture2D> tex;
							if (SUCCEEDED(device->CreateTexture2D(&td, &sd, tex.GetAddressOf())))
							{
								D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
								srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
								srvd.Texture2D.MostDetailedMip = 0; srvd.Texture2D.MipLevels = 1;
								if (SUCCEEDED(device->CreateShaderResourceView(tex.Get(), &srvd, &srv))) m_MaterialSRVs[m] = srv;
							}
						}
					}
				}
			}
			else
			{
				std::wstring wtex = WStringFromUtf8(t);
				bool isAbs = (!wtex.empty() && (wtex.find(L":") != std::wstring::npos || wtex[0] == L'/' || wtex[0] == L'\\'));
				std::wstring full = isAbs ? wtex : (baseDir + wtex);
				if (ID3D11ShaderResourceView* cached = findCached(full)) { m_MaterialSRVs[m] = cached; cached->AddRef(); }
				else
				{
					ComPtr<ID3D11Resource> res;
					HRESULT hrLoad = CreateWICTextureFromFile(device, full.c_str(), res.GetAddressOf(), &srv);
					if (FAILED(hrLoad))
					{
						std::wstring fbm = L"Alice.fbm/" + wtex;
						std::wstring full2 = baseDir + fbm;
						res.Reset(); srv = nullptr;
						hrLoad = CreateWICTextureFromFile(device, full2.c_str(), res.GetAddressOf(), &srv);
						if (SUCCEEDED(hrLoad)) addCache(full2, srv);
					}
					else { addCache(full, srv); }
					if (SUCCEEDED(hrLoad)) m_MaterialSRVs[m] = srv;
				}
			}
		}
	}

	// white fallback
	if (!m_pWhiteSRV)
	{
		UINT white = 0xFFFFFFFF;
		D3D11_TEXTURE2D_DESC td{}; td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = &white; sd.SysMemPitch = sizeof(UINT);
		ComPtr<ID3D11Texture2D> tex;
		HR_T(device->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
		D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MostDetailedMip = 0; srvd.Texture2D.MipLevels = 1;
		HR_T(device->CreateShaderResourceView(tex.Get(), &srvd, &m_pWhiteSRV));
	}
	for (auto& p : m_MaterialSRVs) { if (p == nullptr) { p = m_pWhiteSRV; if (p) p->AddRef(); } }
	return true;
}

bool PmxManager::BuildMeshBuffers(ID3D11Device* device, const aiScene* scene)
{
	m_Vertices.clear();
	m_Indices.clear();
	m_Subsets.clear();

	auto transformPoint = [](const aiVector3D& v, const aiMatrix4x4& m) -> aiVector3D {
		aiVector3D r; r.x = v.x * m.a1 + v.y * m.b1 + v.z * m.c1 + m.d1;
		r.y = v.x * m.a2 + v.y * m.b2 + v.z * m.c2 + m.d2;
		r.z = v.x * m.a3 + v.y * m.b3 + v.z * m.c3 + m.d3; return r; };

	std::function<void(const aiNode*, const aiMatrix4x4&)> traverse;
	traverse = [&](const aiNode* node, const aiMatrix4x4& parent) {
		aiMatrix4x4 global = parent * node->mTransformation;
		for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
		{
			const aiMesh* mesh = scene->mMeshes[node->mMeshes[mi]];
			size_t baseIndex = m_Vertices.size();
			for (unsigned i = 0; i < mesh->mNumVertices; ++i)
			{
				aiVector3D p = transformPoint(mesh->mVertices[i], global);
				aiVector3D n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0,1,0);
				aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0,0,0);
				aiColor4D c = mesh->HasVertexColors(0) ? mesh->mColors[0][i] : aiColor4D(1,1,1,1);
				m_Vertices.push_back({ XMFLOAT3(p.x, p.y, p.z), XMFLOAT3(n.x, n.y, n.z), XMFLOAT4(c.r, c.g, c.b, c.a), XMFLOAT2(uv.x, uv.y) });
			}
			uint32_t start = (uint32_t)m_Indices.size();
			for (unsigned f = 0; f < mesh->mNumFaces; ++f)
			{
				const aiFace& face = mesh->mFaces[f];
				if (face.mNumIndices == 3)
				{
					m_Indices.push_back(static_cast<uint32_t>(baseIndex + face.mIndices[0]));
					m_Indices.push_back(static_cast<uint32_t>(baseIndex + face.mIndices[1]));
					m_Indices.push_back(static_cast<uint32_t>(baseIndex + face.mIndices[2]));
				}
			}
			uint32_t count = (uint32_t)m_Indices.size() - start;
			m_Subsets.push_back({ start, count, mesh->mMaterialIndex });
		}
		for (unsigned ci = 0; ci < node->mNumChildren; ++ci) traverse(node->mChildren[ci], global);
	};
	traverse(scene->mRootNode, aiMatrix4x4());

	if (m_Vertices.empty() || m_Indices.empty()) return false;

	// VB
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = (UINT)(m_Vertices.size() * sizeof(VertexData));
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA vbData = {}; vbData.pSysMem = m_Vertices.data();
	HR_T(device->CreateBuffer(&vbDesc, &vbData, &m_pVB));

	// IB
	D3D11_BUFFER_DESC ibDesc = {};
	m_IndexCount = (int)m_Indices.size();
	ibDesc.ByteWidth = (UINT)(m_Indices.size() * sizeof(uint32_t));
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA ibData = {}; ibData.pSysMem = m_Indices.data();
	HR_T(device->CreateBuffer(&ibDesc, &ibData, &m_pIB));

	return true;
}
