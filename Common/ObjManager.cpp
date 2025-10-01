#include "pch.h"
#include "ObjManager.h"
#include "Helper.h"
#include "Vertex.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <directxtk/WICTextureLoader.h>

using Microsoft::WRL::ComPtr;

static std::wstring WStringFromUtf8(const std::string& s) { return std::wstring(s.begin(), s.end()); }

void ObjManager::Release()
{
    SAFE_RELEASE(m_pVB);
    SAFE_RELEASE(m_pIB);
    for (auto& p : m_MaterialSRVs) SAFE_RELEASE(p);
    m_MaterialSRVs.clear();
    for (auto& kv : m_TexCache) SAFE_RELEASE(kv.second);
    m_TexCache.clear();
    SAFE_RELEASE(m_pWhite);
    m_Subsets.clear();
    m_IndexCount = 0;
    m_VertexStride = 0;
}

bool ObjManager::Load(ID3D11Device* device, const std::wstring& pathW)
{
    Release();
    Assimp::Importer importer;
    std::string pathA(pathW.begin(), pathW.end());
    const aiScene* scene = importer.ReadFile(pathA,
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
        aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded);
    if (!scene || !scene->HasMeshes()) return false;

    std::wstring baseDir = pathW;
    size_t slash = baseDir.find_last_of(L"/\\");
    baseDir = (slash == std::wstring::npos) ? L"" : baseDir.substr(0, slash + 1);

    if (!LoadMaterials(device, scene, baseDir)) return false;
    if (!BuildMeshBuffers(device, scene)) return false;
    return true;
}

bool ObjManager::LoadMaterials(ID3D11Device* device, const aiScene* scene, const std::wstring& baseDir)
{
    if (!m_pWhite)
    {
        UINT white = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC td{}; td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = &white; sd.SysMemPitch = sizeof(UINT);
        ComPtr<ID3D11Texture2D> tex; HR_T(device->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MipLevels = 1; srvd.Texture2D.MostDetailedMip = 0;
        HR_T(device->CreateShaderResourceView(tex.Get(), &srvd, &m_pWhite));
    }

    m_MaterialSRVs.assign(scene->mNumMaterials, nullptr);
    auto findCached = [&](const std::wstring& key){ auto it = m_TexCache.find(key); return it==m_TexCache.end()? (ID3D11ShaderResourceView*)nullptr : it->second; };
    auto addCache = [&](const std::wstring& key, ID3D11ShaderResourceView* v){ if (v) { m_TexCache[key] = v; v->AddRef(); } };

    for (unsigned m = 0; m < scene->mNumMaterials; ++m)
    {
        aiMaterial* mat = scene->mMaterials[m];
        aiString texPath;
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
        {
            std::string t = texPath.C_Str();
            if (!t.empty() && t[0] == '*')
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
                                m_MaterialSRVs[m] = srv;
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
                if (auto* cached = findCached(full)) { m_MaterialSRVs[m] = cached; cached->AddRef(); }
                else
                {
                    ComPtr<ID3D11Resource> res; ID3D11ShaderResourceView* srv = nullptr;
                    if (SUCCEEDED(CreateWICTextureFromFile(device, full.c_str(), res.GetAddressOf(), &srv))) { m_MaterialSRVs[m] = srv; addCache(full, srv); }
                }
            }
        }
        if (!m_MaterialSRVs[m]) { m_MaterialSRVs[m] = m_pWhite; if (m_pWhite) m_pWhite->AddRef(); }
    }
    return true;
}

bool ObjManager::BuildMeshBuffers(ID3D11Device* device, const aiScene* scene)
{
    std::vector<VertexTBN> vertices; vertices.reserve(8192);
    std::vector<uint32_t> indices; indices.reserve(16384);
    m_Subsets.clear();

    auto transformPoint = [](const aiVector3D& v, const aiMatrix4x4& m) -> aiVector3D {
        aiVector3D r;
        r.x = v.x * m.a1 + v.y * m.b1 + v.z * m.c1 + m.d1;
        r.y = v.x * m.a2 + v.y * m.b2 + v.z * m.c2 + m.d2;
        r.z = v.x * m.a3 + v.y * m.b3 + v.z * m.c3 + m.d3;
        return r; 
    };

    std::function<void(const aiNode*, const aiMatrix4x4&)> traverse;
    traverse = [&](const aiNode* node, const aiMatrix4x4& parent){
        aiMatrix4x4 global = parent * node->mTransformation;
        for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
        {
            const aiMesh* mesh = scene->mMeshes[node->mMeshes[mi]];
            size_t base = vertices.size();
            for (unsigned i = 0; i < mesh->mNumVertices; ++i)
            {
                aiVector3D p = transformPoint(mesh->mVertices[i], global);
                aiVector3D n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0,1,0);
                aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0,0,0);
                aiVector3D tg = mesh->HasTangentsAndBitangents() ? mesh->mTangents[i]   : aiVector3D(1,0,0);
                aiVector3D bt = mesh->HasTangentsAndBitangents() ? mesh->mBitangents[i] : aiVector3D(0,1,0);

                // OBJ: flip Y to match engine orientation; reverse winding as well
                //p.y  = -p.y; n.y  = -n.y; tg.y = -tg.y; bt.y = -bt.y;
                vertices.push_back({ {p.x,p.y,p.z}, {n.x,n.y,n.z}, {tg.x,tg.y,tg.z}, {bt.x,bt.y,bt.z}, {1,1,1,1}, {uv.x,1.0f-uv.y} });
            }
            uint32_t start = (uint32_t)indices.size();
            for (unsigned f = 0; f < mesh->mNumFaces; ++f)
            {
                const aiFace& face = mesh->mFaces[f];
                if (face.mNumIndices == 3)
                {
                    indices.push_back((uint32_t)(base + face.mIndices[0]));
                    indices.push_back((uint32_t)(base + face.mIndices[1]));
                    indices.push_back((uint32_t)(base + face.mIndices[2]));
                }
            }
            uint32_t count = (uint32_t)indices.size() - start;
            m_Subsets.push_back({ start, count, mesh->mMaterialIndex });
        }
        for (unsigned ci = 0; ci < node->mNumChildren; ++ci) traverse(node->mChildren[ci], global);
    };
    traverse(scene->mRootNode, aiMatrix4x4());

    if (vertices.empty() || indices.empty()) return false;

    m_VertexStride = sizeof(VertexTBN);
    D3D11_BUFFER_DESC vb{}; vb.ByteWidth = (UINT)(vertices.size() * sizeof(VertexTBN)); vb.BindFlags = D3D11_BIND_VERTEX_BUFFER; vb.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA vbd{}; vbd.pSysMem = vertices.data(); HR_T(device->CreateBuffer(&vb, &vbd, &m_pVB));

    m_IndexCount = (int)indices.size();
    D3D11_BUFFER_DESC ib{}; ib.ByteWidth = (UINT)(indices.size() * sizeof(uint32_t)); ib.BindFlags = D3D11_BIND_INDEX_BUFFER; ib.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA ibd{}; ibd.pSysMem = indices.data(); HR_T(device->CreateBuffer(&ib, &ibd, &m_pIB));
    return true;
}


