#include "pch.h"
#include "FbxManager.h"
#include "Helper.h"
#include "Vertex.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <directxtk/WICTextureLoader.h>

using Microsoft::WRL::ComPtr;

static std::wstring WStringFromUtf8(const std::string& s) { return std::wstring(s.begin(), s.end()); }
static std::string  StringFromAi(const aiString& s) { return std::string(s.C_Str()); }

struct FbxManager::AssimpImporterHolder
{
    Assimp::Importer importer;
};

void FbxManager::Release()
{
    if (m_pVB) { m_pVB->Release(); m_pVB = nullptr; }
    if (m_pIB) { m_pIB->Release(); m_pIB = nullptr; }
    for (auto& p : m_MaterialSRVs) { if (p) { p->Release(); p = nullptr; } }
    m_MaterialSRVs.clear();
    for (auto& kv : m_TexCache) { if (kv.second) { kv.second->Release(); kv.second = nullptr; } }
    m_TexCache.clear();
    if (m_pWhite) { m_pWhite->Release(); m_pWhite = nullptr; }
    m_Subsets.clear();
    m_IndexCount = 0;
    m_VertexStride = 0;
    m_BindVertices.clear();
    m_IndicesCPU.clear();
    m_Influences.clear();
    m_BoneNames.clear();
    m_BoneOffset.clear();
    m_BoneIndexOfName.clear();
    m_NodeIndexOfName.clear();
    m_Skeleton.clear();
    m_RootIndex = -1;
    m_HasSkinning = false;
    m_HasAnimations = false;
    m_AnimationNames.clear();
    m_ClipDurationSec.clear();
    m_ClipTicksPerSec.clear();
    m_CurrentClip = -1;
    m_ClipTimeSec = 0.0;
    m_Playing = false;
    m_SceneMutable = nullptr;
    if (m_ImporterOpaque) { delete reinterpret_cast<AssimpImporterHolder*>(m_ImporterOpaque); m_ImporterOpaque = nullptr; }
    if (m_pBoneCB) { m_pBoneCB->Release(); m_pBoneCB = nullptr; }
}

bool FbxManager::Load(ID3D11Device* device, const std::wstring& pathW)
{
    Release();
    AssimpImporterHolder* holder = new AssimpImporterHolder();
    m_ImporterOpaque = holder;
    // Importer properties for speed/robustness
    holder->importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
    std::string pathA(pathW.begin(), pathW.end());
    const aiScene* scene = holder->importer.ReadFile(pathA,
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
        aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded |
        aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph | aiProcess_LimitBoneWeights);
    if (!scene || !scene->HasMeshes()) return false;
    m_SceneMutable = const_cast<aiScene*>(scene);
    // Save global inverse of root (row-vector mapping)
    {
        aiMatrix4x4 I = scene->mRootNode->mTransformation;
        I.Inverse();
        DirectX::XMFLOAT4X4 gi;
        gi._11 = (float)I.a1; gi._12 = (float)I.a2; gi._13 = (float)I.a3; gi._14 = (float)I.a4;
        gi._21 = (float)I.b1; gi._22 = (float)I.b2; gi._23 = (float)I.b3; gi._24 = (float)I.b4;
        gi._31 = (float)I.c1; gi._32 = (float)I.c2; gi._33 = (float)I.c3; gi._34 = (float)I.c4;
        gi._41 = (float)I.d1; gi._42 = (float)I.d2; gi._43 = (float)I.d3; gi._44 = (float)I.d4;
        m_GlobalInverse = gi;
    }

    std::wstring baseDir = pathW;
    size_t slash = baseDir.find_last_of(L"/\\");
    baseDir = (slash == std::wstring::npos) ? L"" : baseDir.substr(0, slash + 1);

    if (!LoadMaterials(device, scene, baseDir)) return false;
    if (!BuildMeshBuffers(device, scene)) return false;

    // 본 구조를 위한 스켈레톤 노드와 애니메이션 생성
    // 스켈레톤 노드
    m_Skeleton.clear();
    m_NodeIndexOfName.clear();
    std::function<int(const aiNode*, int)> build = [&](const aiNode* node, int parent){
        int idx = (int)m_Skeleton.size();
        m_Skeleton.push_back({ node->mName.C_Str(), parent, {}, false });
        m_NodeIndexOfName[m_Skeleton.back().name] = idx;
        for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
        {
            int ch = build(node->mChildren[ci], idx);
            m_Skeleton[idx].children.push_back(ch);
        }
        return idx;
    };
    m_RootIndex = build(scene->mRootNode, -1);

    // 본/오프셋과 가중치
    m_HasSkinning = false;
    m_BoneNames.clear();
    m_BoneOffset.clear();
    m_BoneIndexOfName.clear();

    for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi)
    {
        const aiMesh* mesh = scene->mMeshes[mi];
        for (unsigned bi = 0; bi < mesh->mNumBones; ++bi)
        {
            const aiBone* b = mesh->mBones[bi];
            std::string name = b->mName.C_Str();
            if (m_BoneIndexOfName.find(name) == m_BoneIndexOfName.end())
            {
                int newIndex = (int)m_BoneNames.size();
                m_BoneIndexOfName[name] = newIndex;
                m_BoneNames.push_back(name);
                DirectX::XMFLOAT4X4 off;
                off._11 = (float)b->mOffsetMatrix.a1; off._12 = (float)b->mOffsetMatrix.a2; off._13 = (float)b->mOffsetMatrix.a3; off._14 = (float)b->mOffsetMatrix.a4;
                off._21 = (float)b->mOffsetMatrix.b1; off._22 = (float)b->mOffsetMatrix.b2; off._23 = (float)b->mOffsetMatrix.b3; off._24 = (float)b->mOffsetMatrix.b4;
                off._31 = (float)b->mOffsetMatrix.c1; off._32 = (float)b->mOffsetMatrix.c2; off._33 = (float)b->mOffsetMatrix.c3; off._34 = (float)b->mOffsetMatrix.c4;
                off._41 = (float)b->mOffsetMatrix.d1; off._42 = (float)b->mOffsetMatrix.d2; off._43 = (float)b->mOffsetMatrix.d3; off._44 = (float)b->mOffsetMatrix.d4;
                m_BoneOffset.push_back(off);
                auto itNode = m_NodeIndexOfName.find(name);
                if (itNode != m_NodeIndexOfName.end()) m_Skeleton[itNode->second].isBone = true;
            }
        }
    }

    // 정점당 최대 4개의 영향을 받음
    if (!m_Influences.empty()) m_Influences.clear();
    m_Influences.assign(m_BindVertices.size(), {});

    // 각 메시의 기준 정점 재구성
    std::vector<size_t> baseVertex; baseVertex.resize(scene->mNumMeshes, 0);
    size_t cursor = 0;
    std::function<void(const aiNode*)> fillBase = [&](const aiNode* node){
        for (unsigned mi2 = 0; mi2 < node->mNumMeshes; ++mi2)
        {
            unsigned meshIdx = node->mMeshes[mi2];
            baseVertex[meshIdx] = cursor;
            cursor += scene->mMeshes[meshIdx]->mNumVertices;
        }
        for (unsigned ci = 0; ci < node->mNumChildren; ++ci) fillBase(node->mChildren[ci]);
    };
    fillBase(scene->mRootNode);

    for (unsigned mi2 = 0; mi2 < scene->mNumMeshes; ++mi2)
    {
        const aiMesh* mesh = scene->mMeshes[mi2];
        size_t base = baseVertex[mi2];
        for (unsigned bi = 0; bi < mesh->mNumBones; ++bi)
        {
            const aiBone* b = mesh->mBones[bi];
            auto it = m_BoneIndexOfName.find(b->mName.C_Str());
            if (it == m_BoneIndexOfName.end()) continue;
            int boneIdx = it->second;
            for (unsigned wi = 0; wi < b->mNumWeights; ++wi)
            {
                const aiVertexWeight& vw = b->mWeights[wi];
                size_t v = base + (size_t)vw.mVertexId;
                if (v >= m_Influences.size()) continue;
                // 가중치가 더 큰 쪽을 보존: 비어있으면 빈 슬롯, 가득이면 가장 작은 슬롯 대체
                int slot = 0; float minW = m_Influences[v].w[0];
                for (int s = 1; s < 4; ++s) { if (m_Influences[v].w[s] < minW) { minW = m_Influences[v].w[s]; slot = s; } }
                m_Influences[v].idx[slot] = (unsigned short)boneIdx;
                m_Influences[v].w[slot] = (float)vw.mWeight;
            }
        }
    }
    for (auto& inf : m_Influences)
    {
        float s = inf.w[0] + inf.w[1] + inf.w[2] + inf.w[3];
        if (s > 1e-6f)
        {
            float inv = 1.0f / s;
            inf.w[0] *= inv; inf.w[1] *= inv; inf.w[2] *= inv; inf.w[3] *= inv;
        }
        else
        {
            // 본 영향이 전혀 없는 정점은 ID 0에 완전 가중치로 설정 (아이덴티티 본)
            inf.idx[0] = 0; inf.w[0] = 1.0f;
            inf.idx[1] = 0; inf.w[1] = 0.0f;
            inf.idx[2] = 0; inf.w[2] = 0.0f;
            inf.idx[3] = 0; inf.w[3] = 0.0f;
        }
    }
    m_HasSkinning = !m_BoneNames.empty();

    // 애니메이션 메타데이터
    if (scene->mNumAnimations > 0)
    {
        m_HasAnimations = true;
        m_AnimationNames.reserve(scene->mNumAnimations);
        m_ClipDurationSec.reserve(scene->mNumAnimations);
        m_ClipTicksPerSec.reserve(scene->mNumAnimations);
        for (unsigned i = 0; i < scene->mNumAnimations; ++i)
        {
            const aiAnimation* a = scene->mAnimations[i];
            std::string nm = a->mName.length > 0 ? StringFromAi(a->mName) : ("Anim" + std::to_string(i));
            double tps = (a->mTicksPerSecond != 0.0) ? a->mTicksPerSecond : 25.0;
            double durSec = (tps != 0.0) ? (a->mDuration / tps) : 0.0;
            m_AnimationNames.push_back(nm);
            m_ClipTicksPerSec.push_back(tps);
            m_ClipDurationSec.push_back(durSec);
        }
        m_CurrentClip = 0;
        m_ClipTimeSec = 0.0;
        m_Playing = false;
    }
    // 본/가중치를 정점에 반영하고 바인드된 버텍스를 재업로드
    if (m_HasSkinning && m_pVB)
    {
        for (size_t i = 0; i < m_BindVertices.size(); ++i)
        {
            const auto& inf = m_Influences[i];
            m_BindVertices[i].boneIdx[0] = inf.idx[0];
            m_BindVertices[i].boneIdx[1] = inf.idx[1];
            m_BindVertices[i].boneIdx[2] = inf.idx[2];
            m_BindVertices[i].boneIdx[3] = inf.idx[3];
            m_BindVertices[i].boneWeight = { inf.w[0], inf.w[1], inf.w[2], inf.w[3] };
        }
        // VB 업데이트(전체 업데이트)
        // DEFAULT 버퍼로 생성되어 있으므로 재생성으로 업로드
        SAFE_RELEASE(m_pVB);
        D3D11_BUFFER_DESC vb{}; vb.BindFlags = D3D11_BIND_VERTEX_BUFFER; vb.Usage = D3D11_USAGE_DEFAULT;
        vb.ByteWidth = (UINT)(m_BindVertices.size() * sizeof(VertexSkinnedTBN));
        D3D11_SUBRESOURCE_DATA vbd{}; vbd.pSysMem = m_BindVertices.data();
        HR_T(device->CreateBuffer(&vb, &vbd, &m_pVB));
    }
    return true;
}

bool FbxManager::LoadMaterials(ID3D11Device* device, const aiScene* scene, const std::wstring& baseDir)
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

            // fbx 파일에 임베디드된 텍스처 로딩을 시도함 이름 또는 *인덱스 표기를 모두 처리함 (Assimp 헬퍼를 사용해서)
            if (!t.empty())
            {
                const aiTexture* at = scene->GetEmbeddedTexture(t.c_str());
                if (at)
                {
                    ComPtr<ID3D11Resource> res; ID3D11ShaderResourceView* srv = nullptr;
                    if (at->mHeight == 0)
                    {
                        // 이미지를 읽어옴. 압축 버퍼를 사용함 (PNG/JPG 등)
                        if (SUCCEEDED(CreateWICTextureFromMemory(device, reinterpret_cast<const uint8_t*>(at->pcData), at->mWidth, res.GetAddressOf(), &srv)))
                            m_MaterialSRVs[m] = srv;
                    }
                    else
                    {
                        // RAW BGRA8 픽셀 데이터
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

            // 임베디드된 텍스쳐가 없거나 실패 시, 구형 *인덱스 방식 처리
            if (!m_MaterialSRVs[m] && !t.empty() && t[0] == '*')
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

            // 위 방법 둘다 안될경우에 외부 파일 경로 시도
            if (!m_MaterialSRVs[m])
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

bool FbxManager::BuildMeshBuffers(ID3D11Device* device, const aiScene* scene)
{
    std::vector<VertexSkinnedTBN> vertices;
    vertices.reserve(8192);
    std::vector<uint32_t> indices;
    indices.reserve(16384);
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
                aiVector3D p = mesh->mVertices[i];
                aiVector3D n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0,1,0);
                aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0,0,0);
                aiVector3D tg = mesh->HasTangentsAndBitangents() ? mesh->mTangents[i]   : aiVector3D(1,0,0);
                aiVector3D bt = mesh->HasTangentsAndBitangents() ? mesh->mBitangents[i] : aiVector3D(0,1,0);
                VertexSkinnedTBN v{};
                
                v.pos = {p.x,p.y,p.z};
                v.n = {n.x,n.y,n.z};
                v.t = {tg.x,tg.y,tg.z};
                v.b = {bt.x,bt.y,bt.z};
                v.color = {1,1,1,1};
                v.uv = {uv.x,uv.y};
                v.boneIdx[0] = v.boneIdx[1] = v.boneIdx[2] = v.boneIdx[3] = 0;
                v.boneWeight = {0,0,0,0};
                
                vertices.push_back(v);
            }
            uint32_t start = (uint32_t)indices.size();
            for (unsigned f = 0; f < mesh->mNumFaces; ++f)
            {
                const aiFace& face = mesh->mFaces[f];
                if (face.mNumIndices == 3)
                {
                    // Reverse winding to correct back/front after Y-flip
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

    // Keep CPU copy and create VB
    m_BindVertices = vertices;
    m_IndicesCPU = indices;
    m_Influences.assign(vertices.size(), {});

    m_VertexStride = sizeof(VertexSkinnedTBN);
    D3D11_BUFFER_DESC vb{};
    vb.ByteWidth = (UINT)(vertices.size() * sizeof(VertexSkinnedTBN));
    vb.BindFlags = D3D11_BIND_VERTEX_BUFFER; vb.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA vbd{}; vbd.pSysMem = vertices.data(); HR_T(device->CreateBuffer(&vb, &vbd, &m_pVB));

    m_IndexCount = (int)indices.size();
    D3D11_BUFFER_DESC ib{}; ib.ByteWidth = (UINT)(indices.size() * sizeof(uint32_t)); ib.BindFlags = D3D11_BIND_INDEX_BUFFER; ib.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA ibd{}; ibd.pSysMem = indices.data(); HR_T(device->CreateBuffer(&ib, &ibd, &m_pIB));
    return true;
}

void FbxManager::SetCurrentAnimation(int idx)
{
    if (!m_HasAnimations) return;
    if (idx < 0 || idx >= (int)m_AnimationNames.size()) return;
    m_CurrentClip = idx;
    m_ClipTimeSec = 0.0;
}

void FbxManager::SetAnimationTimeSeconds(double t)
{
    if (!m_HasAnimations || m_CurrentClip < 0) { m_ClipTimeSec = 0.0; return; }
    double dur = m_ClipDurationSec[m_CurrentClip];
    if (dur <= 0.0) { m_ClipTimeSec = 0.0; return; }
    while (t < 0.0) t += dur;
    while (t >= dur) t -= dur;
    m_ClipTimeSec = t;
}

static aiVector3D InterpVec(const aiVectorKey* keys, unsigned count, double t)
{
    if (count == 0) return aiVector3D(0,0,0);
    if (count == 1) return keys[0].mValue;
    unsigned i = 0;
    while (i + 1 < count && t >= keys[i+1].mTime) ++i;
    unsigned j = (i + 1 < count) ? i + 1 : i;
    double dt = keys[j].mTime - keys[i].mTime; double a = (dt > 0.0) ? (t - keys[i].mTime) / dt : 0.0;
    aiVector3D v0 = keys[i].mValue, v1 = keys[j].mValue; return v0 + (float)a * (v1 - v0);
}

static aiQuaternion InterpQuat(const aiQuatKey* keys, unsigned count, double t)
{
    if (count == 0) return aiQuaternion();
    if (count == 1) return keys[0].mValue;
    unsigned i = 0;
    while (i + 1 < count && t >= keys[i+1].mTime) ++i;
    unsigned j = (i + 1 < count) ? i + 1 : i;
    double dt = keys[j].mTime - keys[i].mTime; double a = (dt > 0.0) ? (t - keys[i].mTime) / dt : 0.0;
    aiQuaternion q; aiQuaternion::Interpolate(q, keys[i].mValue, keys[j].mValue, (float)a); q.Normalize(); return q;
}

void FbxManager::UpdateAnimation(ID3D11DeviceContext* ctx, double dtSec)
{
    if (!m_HasAnimations || m_CurrentClip < 0) return;
    if (m_Playing) SetAnimationTimeSeconds(m_ClipTimeSec + dtSec);

    const aiScene* scene = reinterpret_cast<const aiScene*>(m_SceneMutable);
    if (!scene) return;
    const aiAnimation* anim = (m_HasAnimations && m_CurrentClip >= 0) ? scene->mAnimations[m_CurrentClip] : nullptr;
    std::unordered_map<std::string, const aiNodeAnim*> channelOf;
    if (anim)
    {
        for (unsigned i = 0; i < anim->mNumChannels; ++i)
        {
            const aiNodeAnim* ch = anim->mChannels[i];
            channelOf[ch->mNodeName.C_Str()] = ch;
        }
    }

    std::vector<DirectX::XMFLOAT4X4> global; global.resize(m_Skeleton.size());
    std::function<void(const aiNode*, int, const DirectX::XMMATRIX&)> eval = [&](const aiNode* node, int idx, const DirectX::XMMATRIX& parent){
        aiVector3D S(1,1,1), T(0,0,0); aiQuaternion R;
        aiMatrix4x4 mLocal = node->mTransformation;
        auto itCh = channelOf.find(node->mName.C_Str());
        if (itCh != channelOf.end() && anim)
        {
            double tTicks = m_ClipTimeSec * ((m_CurrentClip >= 0) ? m_ClipTicksPerSec[m_CurrentClip] : 25.0);
            const aiNodeAnim* ch = itCh->second;
            S = (ch->mNumScalingKeys   > 0) ? InterpVec(ch->mScalingKeys,   ch->mNumScalingKeys,   tTicks) : aiVector3D(1,1,1);
            T = (ch->mNumPositionKeys  > 0) ? InterpVec(ch->mPositionKeys,  ch->mNumPositionKeys,  tTicks) : aiVector3D(0,0,0);
            R = (ch->mNumRotationKeys  > 0) ? InterpQuat(ch->mRotationKeys, ch->mNumRotationKeys,  tTicks) : aiQuaternion();
            aiMatrix4x4 mS; mS.Scaling(S, mS); aiMatrix4x4 mR = aiMatrix4x4(R.GetMatrix()); aiMatrix4x4 mT; mT.Translation(T, mT);
            // Assimp은 노드 변환을 row-vector 기준으로 T*R*S로 곱해도 일관되게 동작 (fbx exporter 옵션과 일치)
            aiMatrix4x4 m = mT * mR * mS;
            mLocal = m;
        }
        DirectX::XMFLOAT4X4 lm;
        lm._11 = (float)mLocal.a1; lm._12 = (float)mLocal.a2; lm._13 = (float)mLocal.a3; lm._14 = (float)mLocal.a4;
        lm._21 = (float)mLocal.b1; lm._22 = (float)mLocal.b2; lm._23 = (float)mLocal.b3; lm._24 = (float)mLocal.b4;
        lm._31 = (float)mLocal.c1; lm._32 = (float)mLocal.c2; lm._33 = (float)mLocal.c3; lm._34 = (float)mLocal.c4;
        lm._41 = (float)mLocal.d1; lm._42 = (float)mLocal.d2; lm._43 = (float)mLocal.d3; lm._44 = (float)mLocal.d4;
        DirectX::XMMATRIX L = DirectX::XMLoadFloat4x4(&lm);
        // DirectX row-vector: global = parent * local
        DirectX::XMMATRIX G = DirectX::XMMatrixMultiply(parent, L);
        DirectX::XMStoreFloat4x4(&global[idx], G);
        for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
        {
            auto it = m_NodeIndexOfName.find(node->mChildren[ci]->mName.C_Str());
            int childIdx = (it != m_NodeIndexOfName.end()) ? it->second : -1;
            if (childIdx >= 0) eval(node->mChildren[ci], childIdx, G);
        }
    };
    if (m_RootIndex >= 0) eval(scene->mRootNode, m_RootIndex, DirectX::XMMatrixIdentity());

    std::vector<DirectX::XMMATRIX> palette; palette.resize(m_BoneNames.size(), DirectX::XMMatrixIdentity());
    for (size_t bi = 0; bi < m_BoneNames.size(); ++bi)
    {
        auto itN = m_NodeIndexOfName.find(m_BoneNames[bi]);
        if (itN == m_NodeIndexOfName.end()) continue;
        int nodeIdx = itN->second;
        DirectX::XMMATRIX G = DirectX::XMLoadFloat4x4(&global[nodeIdx]);
        DirectX::XMMATRIX Off = DirectX::XMLoadFloat4x4(&m_BoneOffset[bi]);
        DirectX::XMMATRIX Gi = DirectX::XMLoadFloat4x4(&m_GlobalInverse);
        // 표준 스키닝: Final = GlobalInverse * Global * Offset
        palette[bi] = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(Gi, G), Off);
    }
    // 본 팔레트 상수 버퍼 업로드 (VS b1)
    if (!m_pBoneCB && ctx)
    {
        ComPtr<ID3D11Device> dev;
        ctx->GetDevice(dev.GetAddressOf());
        if (dev)
        {
            D3D11_BUFFER_DESC bd{};
            bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            bd.ByteWidth = sizeof(DirectX::XMFLOAT4X4) * kMaxBones + sizeof(unsigned int) + sizeof(float) * 3;
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            HR_T(dev->CreateBuffer(&bd, nullptr, &m_pBoneCB));
        }
    }
    if (m_pBoneCB)
    {
        struct BoneCB { DirectX::XMFLOAT4X4 m[kMaxBones]; unsigned int boneCount; float pad[3]; };
        BoneCB cb{};
        size_t n = std::min(palette.size(), (size_t)kMaxBones);
        // 먼저 전부 아이덴티티로 채운다
        DirectX::XMMATRIX I = DirectX::XMMatrixIdentity();
        for (size_t i = 0; i < (size_t)kMaxBones; ++i)
        {
            DirectX::XMStoreFloat4x4(&cb.m[i], I);
        }
        // 사용되는 본만 전치하여 덮어쓴다
        for (size_t i = 0; i < n; ++i)
        {
            DirectX::XMMATRIX Mt = DirectX::XMMatrixTranspose(palette[i]);
            DirectX::XMStoreFloat4x4(&cb.m[i], Mt);
        }
        cb.boneCount = (unsigned int)n;
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(ctx->Map(m_pBoneCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        {
            memcpy(mapped.pData, &cb, sizeof(BoneCB));
            ctx->Unmap(m_pBoneCB, 0);
        }
    }
}


