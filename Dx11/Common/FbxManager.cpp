#include "pch.h"
#include "FbxManager.h"
#include "Helper.h"
#include "Vertex.h"

#include <d3d11.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <directxtk/WICTextureLoader.h>
#include "../Common/Helper.h"

using Microsoft::WRL::ComPtr;


static std::string  StringFromAi(const aiString& s) { return std::string(s.C_Str()); }


struct FbxManager::Impl
{
    ID3D11Buffer* pVB = nullptr;
    ID3D11Buffer* pIB = nullptr;
    int IndexCount = 0;
    size_t VertexStride = 0;
    std::vector<FbxSubset> Subsets;
    std::vector<ID3D11ShaderResourceView*> MaterialSRVs;
    std::unordered_map<std::wstring, ID3D11ShaderResourceView*> TexCache;
    ID3D11ShaderResourceView* pWhite = nullptr;
    struct AssimpImporterHolder { Assimp::Importer importer; };
    AssimpImporterHolder* Importer = nullptr;
    struct aiScene* SceneMutable = nullptr;
    struct Influence4 { unsigned short idx[4] = {0,0,0,0}; float w[4] = {0,0,0,0}; };
    std::vector<VertexSkinnedTBN> BindVertices;
    std::vector<uint32_t> IndicesCPU;
    std::vector<Influence4> Influences;
    bool HasSkinning = false;
    std::vector<std::string> BoneNames;
    std::vector<DirectX::XMFLOAT4X4> BoneOffset;
    std::unordered_map<std::string, int> BoneIndexOfName;
    std::unordered_map<std::string, int> NodeIndexOfName;
    // For rigid: remember which aiNode produced each vertex (by name)
    std::vector<std::string> VertexOwningNodeName;
    std::vector<FbxManager::SkeletonNode> Skeleton;
    int RootIndex = -1;
    DirectX::XMFLOAT4X4 GlobalInverse = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    bool HasAnimations = false;
    std::vector<std::string> AnimationNames;
    std::vector<double> ClipDurationSec;
    std::vector<double> ClipTicksPerSec;
    int CurrentClip = -1;
    double ClipTimeSec = 0.0;
    bool Playing = false;
    std::vector<const aiNodeAnim*> ChannelOfNode;
    ID3D11Buffer* pBoneCB = nullptr;

    // User overrides for per-node local TRS (applied after animation local)
    struct OverrideData { DirectX::XMFLOAT3 T{0,0,0}; DirectX::XMFLOAT3 Rdeg{0,0,0}; DirectX::XMFLOAT3 S{1,1,1}; bool enabled=false; };
    std::vector<OverrideData> Overrides; // size = Skeleton.size()
};

FbxManager::FbxManager() : m_(std::make_unique<Impl>()) {}
// 실제 리소스 정리는 각 프로젝트(App)에서 명시적으로 Release()를 호출해 수행합니다.
// 소멸자에서는 중복 Release를 피하기 위해 아무 것도 하지 않습니다.
FbxManager::~FbxManager() = default;

// --- Helpers implementation (declared in header) ---
void FbxManager::BuildSkeletonAndNodeIndex(const aiScene* scene)
{
    m_->Skeleton.clear();
    m_->NodeIndexOfName.clear();
    std::function<int(const aiNode*, int)> build = [&](const aiNode* node, int parent){
        int idx = (int)m_->Skeleton.size();
        std::string nmUtf8 = node->mName.C_Str();
        std::wstring nmW = WStringFromUtf8(nmUtf8);
        FbxManager::SkeletonNode sn{}; sn.name = nmUtf8; sn.nameW = nmW; sn.parent = parent; sn.isBone = false;
        m_->Skeleton.push_back(std::move(sn));
        m_->NodeIndexOfName[m_->Skeleton.back().name] = idx;
        for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
        {
            int ch = build(node->mChildren[ci], idx);
            m_->Skeleton[idx].children.push_back(ch);
        }
        return idx;
    };
    m_->RootIndex = build(scene->mRootNode, -1);
    // Initialize overrides to skeleton size
    m_->Overrides.clear();
    m_->Overrides.resize(m_->Skeleton.size());
}

void FbxManager::CollectBonesAndOffsets(const aiScene* scene)
{
    m_->HasSkinning = false;
    m_->BoneNames.clear();
    m_->BoneOffset.clear();
    m_->BoneIndexOfName.clear();
    for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi)
    {
        const aiMesh* mesh = scene->mMeshes[mi];
        for (unsigned bi = 0; bi < mesh->mNumBones; ++bi)
        {
            const aiBone* b = mesh->mBones[bi];
            std::string name = b->mName.C_Str();
            std::wstring nameW = WStringFromUtf8(name);
            if (m_->BoneIndexOfName.find(name) == m_->BoneIndexOfName.end())
            {
                int newIndex = (int)m_->BoneNames.size();
                m_->BoneIndexOfName[name] = newIndex;
                m_->BoneNames.push_back(name);
                DirectX::XMFLOAT4X4 off;
                off._11 = (float)b->mOffsetMatrix.a1; off._12 = (float)b->mOffsetMatrix.a2; off._13 = (float)b->mOffsetMatrix.a3; off._14 = (float)b->mOffsetMatrix.a4;
                off._21 = (float)b->mOffsetMatrix.b1; off._22 = (float)b->mOffsetMatrix.b2; off._23 = (float)b->mOffsetMatrix.b3; off._24 = (float)b->mOffsetMatrix.b4;
                off._31 = (float)b->mOffsetMatrix.c1; off._32 = (float)b->mOffsetMatrix.c2; off._33 = (float)b->mOffsetMatrix.c3; off._34 = (float)b->mOffsetMatrix.c4;
                off._41 = (float)b->mOffsetMatrix.d1; off._42 = (float)b->mOffsetMatrix.d2; off._43 = (float)b->mOffsetMatrix.d3; off._44 = (float)b->mOffsetMatrix.d4;
                m_->BoneOffset.push_back(off);
                auto itNode = m_->NodeIndexOfName.find(name);
                if (itNode != m_->NodeIndexOfName.end()) {
                    m_->Skeleton[itNode->second].isBone = true;
                    if (m_->Skeleton[itNode->second].nameW.empty()) m_->Skeleton[itNode->second].nameW = nameW;
                }
            }
        }
    }
}

void FbxManager::BuildBaseVertexTable(const aiScene* scene, std::vector<size_t>& baseVertex)
{
    baseVertex.clear();
    baseVertex.resize(scene->mNumMeshes, 0);
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
}

void FbxManager::AccumulateVertexWeights(const aiScene* scene, const std::vector<size_t>& baseVertex)
{
    if (!m_->Influences.empty()) m_->Influences.clear();
    m_->Influences.assign(m_->BindVertices.size(), {});
    for (unsigned mi2 = 0; mi2 < scene->mNumMeshes; ++mi2)
    {
        const aiMesh* mesh = scene->mMeshes[mi2];
        size_t base = baseVertex[mi2];
        for (unsigned bi = 0; bi < mesh->mNumBones; ++bi)
        {
            const aiBone* b = mesh->mBones[bi];
            auto it = m_->BoneIndexOfName.find(b->mName.C_Str());
            if (it == m_->BoneIndexOfName.end()) continue;
            int boneIdx = it->second;
            for (unsigned wi = 0; wi < b->mNumWeights; ++wi)
            {
                const aiVertexWeight& vw = b->mWeights[wi];
                size_t v = base + (size_t)vw.mVertexId;
                if (v >= m_->Influences.size()) continue;
                int slot = 0; float minW = m_->Influences[v].w[0];
                for (int s = 1; s < 4; ++s) { if (m_->Influences[v].w[s] < minW) { minW = m_->Influences[v].w[s]; slot = s; } }
                m_->Influences[v].idx[slot] = (unsigned short)boneIdx;
                m_->Influences[v].w[slot] = (float)vw.mWeight;
            }
        }
    }
}

void FbxManager::NormalizeInfluencesAndFlag()
{
    for (size_t i = 0; i < m_->Influences.size(); ++i)
    {
        auto& inf = m_->Influences[i];
        float s = inf.w[0] + inf.w[1] + inf.w[2] + inf.w[3];
        if (s > 1e-6f)
        {
            float inv = 1.0f / s;
            inf.w[0] *= inv; inf.w[1] *= inv; inf.w[2] *= inv; inf.w[3] *= inv;
        }
        else
        {
            // No bone weights: treat as rigid attached to the mesh owner node
            inf.idx[0] = 0; inf.w[0] = 1.0f;
            inf.idx[1] = 0; inf.w[1] = 0.0f;
            inf.idx[2] = 0; inf.w[2] = 0.0f;
            inf.idx[3] = 0; inf.w[3] = 0.0f;
            if (i < m_->VertexOwningNodeName.size())
            {
                const std::string& owner = m_->VertexOwningNodeName[i];
                auto itB = m_->BoneIndexOfName.find(owner);
                if (itB != m_->BoneIndexOfName.end())
                {
                    inf.idx[0] = (unsigned short)itB->second; // bind to that bone
                    inf.w[0] = 1.0f;
                }
                else
                {
                    // If owner is not a bone, try to find nearest ancestor that is marked as bone
                    auto itNode = m_->NodeIndexOfName.find(owner);
                    int node = (itNode != m_->NodeIndexOfName.end()) ? itNode->second : -1;
                    while (node >= 0)
                    {
                        const auto& sn = m_->Skeleton[node];
                        auto itBone = m_->BoneIndexOfName.find(sn.name);
                        if (itBone != m_->BoneIndexOfName.end()) { inf.idx[0] = (unsigned short)itBone->second; break; }
                        node = sn.parent;
                    }
                }
            }
        }
    }
    m_->HasSkinning = !m_->BoneNames.empty();
}

void FbxManager::ApplyInfluencesToVB(ID3D11Device* device)
{
    if (!m_->HasSkinning) return;
    for (size_t i = 0; i < m_->BindVertices.size(); ++i)
    {
        const auto& inf = m_->Influences[i];
        m_->BindVertices[i].boneIdx[0] = inf.idx[0];
        m_->BindVertices[i].boneIdx[1] = inf.idx[1];
        m_->BindVertices[i].boneIdx[2] = inf.idx[2];
        m_->BindVertices[i].boneIdx[3] = inf.idx[3];
        m_->BindVertices[i].boneWeight = { inf.w[0], inf.w[1], inf.w[2], inf.w[3] };
    }
    if (m_->pVB) { m_->pVB->Release(); m_->pVB = nullptr; }
    D3D11_BUFFER_DESC vb{}; vb.BindFlags = D3D11_BIND_VERTEX_BUFFER; vb.Usage = D3D11_USAGE_DEFAULT;
    vb.ByteWidth = (UINT)(m_->BindVertices.size() * sizeof(VertexSkinnedTBN));
    D3D11_SUBRESOURCE_DATA vbd{}; vbd.pSysMem = m_->BindVertices.data();
    HR_T(device->CreateBuffer(&vb, &vbd, &m_->pVB));
}

void FbxManager::InitAnimationMetadata(const aiScene* scene)
{
    if (scene->mNumAnimations == 0) return;
    m_->HasAnimations = true;
    m_->AnimationNames.reserve(scene->mNumAnimations);
    m_->ClipDurationSec.reserve(scene->mNumAnimations);
    m_->ClipTicksPerSec.reserve(scene->mNumAnimations);
    for (unsigned i = 0; i < scene->mNumAnimations; ++i)
    {
        const aiAnimation* a = scene->mAnimations[i];
        std::string nm = a->mName.length > 0 ? StringFromAi(a->mName) : ("Anim" + std::to_string(i));
        double tps = (a->mTicksPerSecond != 0.0) ? a->mTicksPerSecond : 25.0;
        double durSec = (tps != 0.0) ? (a->mDuration / tps) : 0.0;
        m_->AnimationNames.push_back(nm);
        m_->ClipTicksPerSec.push_back(tps);
        m_->ClipDurationSec.push_back(durSec);
    }
    m_->CurrentClip = 0;
    m_->ClipTimeSec = 0.0;
    m_->Playing = false;
}

static aiVector3D InterpVec(const aiVectorKey* keys, unsigned count, double t)
{
	if (count == 0) return aiVector3D(0, 0, 0);
	if (count == 1) return keys[0].mValue;
	unsigned i = 0;
	while (i + 1 < count && t >= keys[i + 1].mTime) ++i;
	unsigned j = (i + 1 < count) ? i + 1 : i;
	double dt = keys[j].mTime - keys[i].mTime; double a = (dt > 0.0) ? (t - keys[i].mTime) / dt : 0.0;
	aiVector3D v0 = keys[i].mValue, v1 = keys[j].mValue; return v0 + (float)a * (v1 - v0);
}

static aiQuaternion InterpQuat(const aiQuatKey* keys, unsigned count, double t)
{
	if (count == 0) return aiQuaternion();
	if (count == 1) return keys[0].mValue;
	unsigned i = 0;
	while (i + 1 < count && t >= keys[i + 1].mTime) ++i;
	unsigned j = (i + 1 < count) ? i + 1 : i;
	double dt = keys[j].mTime - keys[i].mTime; double a = (dt > 0.0) ? (t - keys[i].mTime) / dt : 0.0;
	aiQuaternion q; aiQuaternion::Interpolate(q, keys[i].mValue, keys[j].mValue, (float)a); q.Normalize(); return q;
}

void FbxManager::EvaluateGlobalMatrices(const aiScene* scene, const std::unordered_map<std::wstring, const aiNodeAnim*>& channelOf, std::vector<DirectX::XMFLOAT4X4>& outGlobal) const
{
    outGlobal.resize(m_->Skeleton.size());
    std::function<void(const aiNode*, int, const DirectX::XMMATRIX&)> eval = [&](const aiNode* node, int idx, const DirectX::XMMATRIX& parent){
        aiVector3D S(1,1,1), T(0,0,0); aiQuaternion R;
        aiMatrix4x4 mLocal = node->mTransformation;
        auto itCh = channelOf.find(WStringFromUtf8(node->mName.C_Str()));
        if (itCh != channelOf.end())
        {
            double tTicks = m_->ClipTimeSec * ((m_->CurrentClip >= 0 && (size_t)m_->CurrentClip < m_->ClipTicksPerSec.size()) ? m_->ClipTicksPerSec[m_->CurrentClip] : 25.0);
            const aiNodeAnim* ch = itCh->second;
            S = (ch->mNumScalingKeys   > 0) ? InterpVec(ch->mScalingKeys,   ch->mNumScalingKeys,   tTicks) : aiVector3D(1,1,1);
            T = (ch->mNumPositionKeys  > 0) ? InterpVec(ch->mPositionKeys,  ch->mNumPositionKeys,  tTicks) : aiVector3D(0,0,0);
            R = (ch->mNumRotationKeys  > 0) ? InterpQuat(ch->mRotationKeys, ch->mNumRotationKeys,  tTicks) : aiQuaternion();
            aiMatrix4x4 mS; mS.Scaling(S, mS); aiMatrix4x4 mR = aiMatrix4x4(R.GetMatrix()); aiMatrix4x4 mT; mT.Translation(T, mT);
            aiMatrix4x4 mA = mT * mR * mS;
            mLocal = mA;
        }
        // Apply user overrides: local = local * (T*R*S)
        if (idx >= 0 && idx < (int)m_->Overrides.size())
        {
            const auto& ov = m_->Overrides[idx];
            if (ov.enabled)
            {
                DirectX::XMMATRIX oT = DirectX::XMMatrixTranslation(ov.T.x, ov.T.y, ov.T.z);
                DirectX::XMMATRIX oR = DirectX::XMMatrixRotationRollPitchYaw(
                    DirectX::XMConvertToRadians(ov.Rdeg.x),
                    DirectX::XMConvertToRadians(ov.Rdeg.y),
                    DirectX::XMConvertToRadians(ov.Rdeg.z));
                DirectX::XMMATRIX oS = DirectX::XMMatrixScaling(ov.S.x, ov.S.y, ov.S.z);
                DirectX::XMMATRIX o = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(oT, oR), oS);
                DirectX::XMFLOAT4X4 lm;
                lm._11 = (float)mLocal.a1; lm._12 = (float)mLocal.a2; lm._13 = (float)mLocal.a3; lm._14 = (float)mLocal.a4;
                lm._21 = (float)mLocal.b1; lm._22 = (float)mLocal.b2; lm._23 = (float)mLocal.b3; lm._24 = (float)mLocal.b4;
                lm._31 = (float)mLocal.c1; lm._32 = (float)mLocal.c2; lm._33 = (float)mLocal.c3; lm._34 = (float)mLocal.c4;
                lm._41 = (float)mLocal.d1; lm._42 = (float)mLocal.d2; lm._43 = (float)mLocal.d3; lm._44 = (float)mLocal.d4;
                DirectX::XMMATRIX L0 = DirectX::XMLoadFloat4x4(&lm);
                DirectX::XMMATRIX L = DirectX::XMMatrixMultiply(L0, o);
                DirectX::XMFLOAT4X4 lo; DirectX::XMStoreFloat4x4(&lo, L);
                mLocal.a1 = lo._11; mLocal.a2 = lo._12; mLocal.a3 = lo._13; mLocal.a4 = lo._14;
                mLocal.b1 = lo._21; mLocal.b2 = lo._22; mLocal.b3 = lo._23; mLocal.b4 = lo._24;
                mLocal.c1 = lo._31; mLocal.c2 = lo._32; mLocal.c3 = lo._33; mLocal.c4 = lo._34;
                mLocal.d1 = lo._41; mLocal.d2 = lo._42; mLocal.d3 = lo._43; mLocal.d4 = lo._44;
            }
        }
        DirectX::XMFLOAT4X4 lm;
        lm._11 = (float)mLocal.a1; lm._12 = (float)mLocal.a2; lm._13 = (float)mLocal.a3; lm._14 = (float)mLocal.a4;
        lm._21 = (float)mLocal.b1; lm._22 = (float)mLocal.b2; lm._23 = (float)mLocal.b3; lm._24 = (float)mLocal.b4;
        lm._31 = (float)mLocal.c1; lm._32 = (float)mLocal.c2; lm._33 = (float)mLocal.c3; lm._34 = (float)mLocal.c4;
        lm._41 = (float)mLocal.d1; lm._42 = (float)mLocal.d2; lm._43 = (float)mLocal.d3; lm._44 = (float)mLocal.d4;
        DirectX::XMMATRIX L = DirectX::XMLoadFloat4x4(&lm);
        DirectX::XMMATRIX G = DirectX::XMMatrixMultiply(parent, L);
        DirectX::XMStoreFloat4x4(&outGlobal[idx], G);
        for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
        {
            auto it = m_->NodeIndexOfName.find(node->mChildren[ci]->mName.C_Str());
            int childIdx = (it != m_->NodeIndexOfName.end()) ? it->second : -1;
            if (childIdx >= 0) eval(node->mChildren[ci], childIdx, G);
        }
    };
    if (m_->RootIndex >= 0) eval(scene->mRootNode, m_->RootIndex, DirectX::XMMatrixIdentity());
}

void FbxManager::BuildBonePalette(const std::vector<DirectX::XMFLOAT4X4>& global, std::vector<DirectX::XMMATRIX>& outPalette) const
{
    outPalette.resize(m_->BoneNames.size(), DirectX::XMMatrixIdentity());
    for (size_t bi = 0; bi < m_->BoneNames.size(); ++bi)
    {
        auto itN = m_->NodeIndexOfName.find(m_->BoneNames[bi]);
        if (itN == m_->NodeIndexOfName.end()) continue;
        int nodeIdx = itN->second;
        DirectX::XMMATRIX G = DirectX::XMLoadFloat4x4(&global[nodeIdx]);
        DirectX::XMMATRIX Off = DirectX::XMLoadFloat4x4(&m_->BoneOffset[bi]);
        DirectX::XMMATRIX Gi = DirectX::XMLoadFloat4x4(&m_->GlobalInverse);
        outPalette[bi] = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(Gi, G), Off);
    }
}

void FbxManager::EnsureBoneCB(ID3D11Device* device)
{
    if (m_->pBoneCB || !device) return;
    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(DirectX::XMFLOAT4X4) * kMaxBones + sizeof(unsigned int) + sizeof(float) * 3;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HR_T(device->CreateBuffer(&bd, nullptr, &m_->pBoneCB));
}

void FbxManager::UploadBonePalette(ID3D11DeviceContext* ctx, const std::vector<DirectX::XMMATRIX>& palette)
{
    if (!ctx) return;
    Microsoft::WRL::ComPtr<ID3D11Device> dev;
    ctx->GetDevice(dev.GetAddressOf());
    EnsureBoneCB(dev.Get());
    if (!m_->pBoneCB) return;

    struct BoneCB { DirectX::XMFLOAT4X4 m[kMaxBones]; unsigned int boneCount; float pad[3]; };
    BoneCB cb{};
    size_t n = std::min(palette.size(), (size_t)kMaxBones);
    DirectX::XMMATRIX I = DirectX::XMMatrixIdentity();
    for (size_t i = 0; i < (size_t)kMaxBones; ++i)
    {
        DirectX::XMStoreFloat4x4(&cb.m[i], I);
    }
    for (size_t i = 0; i < n; ++i)
    {
        DirectX::XMMATRIX Mt = DirectX::XMMatrixTranspose(palette[i]);
        DirectX::XMStoreFloat4x4(&cb.m[i], Mt);
    }
    cb.boneCount = (unsigned int)n;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(m_->pBoneCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, &cb, sizeof(BoneCB));
        ctx->Unmap(m_->pBoneCB, 0);
    }
}

bool FbxManager::GetCurrentGlobalMatrices(std::vector<DirectX::XMFLOAT4X4>& out) const
{
    const aiScene* scene = reinterpret_cast<const aiScene*>(m_->SceneMutable);
    if (!scene) return false;
    std::unordered_map<std::wstring, const aiNodeAnim*> channelOf; // use current time/channel map (rebuilt in Update)
    EvaluateGlobalMatrices(scene, channelOf, out);
    return !out.empty();
}

void FbxManager::UploadRigidNodePalette(ID3D11DeviceContext* ctx)
{
    // Build palette matching bone list order (required by BLENDINDICES)
    // Evaluate globals first
    std::vector<DirectX::XMFLOAT4X4> global;
    const aiScene* scene = reinterpret_cast<const aiScene*>(m_->SceneMutable);
    std::unordered_map<std::wstring, const aiNodeAnim*> channelOf;
    if (scene && m_->HasAnimations && m_->CurrentClip >= 0)
    {
        const aiAnimation* anim = scene->mAnimations[m_->CurrentClip];
        for (unsigned i = 0; i < anim->mNumChannels; ++i)
        {
            const aiNodeAnim* ch = anim->mChannels[i];
            channelOf[WStringFromUtf8(ch->mNodeName.C_Str())] = ch;
        }
    }
    if (scene) EvaluateGlobalMatrices(scene, channelOf, global);
    if (global.empty()) return;
    std::vector<DirectX::XMMATRIX> pal;
    pal.resize(m_->BoneNames.size(), DirectX::XMMatrixIdentity());
    DirectX::XMMATRIX Gi = DirectX::XMLoadFloat4x4(&m_->GlobalInverse);
    for (size_t bi = 0; bi < m_->BoneNames.size(); ++bi)
    {
        auto itN = m_->NodeIndexOfName.find(m_->BoneNames[bi]);
        if (itN == m_->NodeIndexOfName.end()) continue;
        int nodeIdx = itN->second;
        if (nodeIdx < 0 || nodeIdx >= (int)global.size()) continue;
        DirectX::XMMATRIX G = DirectX::XMLoadFloat4x4(&global[(size_t)nodeIdx]);
        // Rigid: remove scene root correction but no bone offset: M = Gi * G
        pal[bi] = DirectX::XMMatrixMultiply(Gi, G);
    }
    UploadBonePalette(ctx, pal);
}

// Rigid 애니메이션 관련 helper 함수. 본이 아예 없다면 스켈레탈 노드를 통해서 가짜 본을 만들어 준다
void FbxManager::BuildRigidBonesFromSkeleton()
{
    if (!m_->BoneNames.empty()) return;
    m_->BoneNames.clear();
    m_->BoneOffset.clear();
    m_->BoneIndexOfName.clear();
    m_->BoneNames.reserve(m_->Skeleton.size());
    m_->BoneOffset.reserve(m_->Skeleton.size());
    for (size_t i = 0; i < m_->Skeleton.size(); ++i)
    {
        const auto& sn = m_->Skeleton[i];
        m_->BoneIndexOfName[sn.name] = (int)i;
        m_->BoneNames.push_back(sn.name);
        DirectX::XMFLOAT4X4 I{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        m_->BoneOffset.push_back(I);
    }
}

void FbxManager::BuildRigidWeightsFromOwners()
{
    if (m_->Influences.size() != m_->BindVertices.size())
        m_->Influences.assign(m_->BindVertices.size(), {});
    for (size_t i = 0; i < m_->BindVertices.size(); ++i)
    {
        unsigned short bi = 0;
        if (i < m_->VertexOwningNodeName.size())
        {
            const std::string& owner = m_->VertexOwningNodeName[i];
            auto it = m_->BoneIndexOfName.find(owner);
            if (it != m_->BoneIndexOfName.end()) bi = (unsigned short)it->second;
            else {
                auto itNode = m_->NodeIndexOfName.find(owner);
                int node = (itNode != m_->NodeIndexOfName.end()) ? itNode->second : -1;
                while (node >= 0)
                {
                    const auto& sn = m_->Skeleton[node];
                    auto itB = m_->BoneIndexOfName.find(sn.name);
                    if (itB != m_->BoneIndexOfName.end()) { bi = (unsigned short)itB->second; break; }
                    node = sn.parent;
                }
            }
        }
        m_->Influences[i].idx[0] = bi; m_->Influences[i].w[0] = 1.0f;
        m_->Influences[i].idx[1] = m_->Influences[i].idx[2] = m_->Influences[i].idx[3] = 0;
        m_->Influences[i].w[1] = m_->Influences[i].w[2] = m_->Influences[i].w[3] = 0.0f;
    }
    m_->HasSkinning = !m_->BoneNames.empty();
}

void FbxManager::Release()
{

	SAFE_RELEASE(m_->pVB);
	SAFE_RELEASE(m_->pIB);
    for (auto& p : m_->MaterialSRVs) { SAFE_RELEASE(p); }
    m_->MaterialSRVs.clear();
    for (auto& kv : m_->TexCache) { if (kv.second) { kv.second->Release(); kv.second = nullptr; } }
    m_->TexCache.clear();
    if (m_->pWhite) { m_->pWhite->Release(); m_->pWhite = nullptr; }
    m_->Subsets.clear();
    m_->IndexCount = 0;
    m_->VertexStride = 0;
    m_->BindVertices.clear();
    m_->IndicesCPU.clear();
    m_->Influences.clear();
    m_->BoneNames.clear();
    m_->BoneOffset.clear();
    m_->BoneIndexOfName.clear();
    m_->NodeIndexOfName.clear();
    m_->Skeleton.clear();
    m_->RootIndex = -1;
    m_->HasSkinning = false;
    m_->HasAnimations = false;
    m_->AnimationNames.clear();
    m_->ClipDurationSec.clear();
    m_->ClipTicksPerSec.clear();
    m_->CurrentClip = -1;
    m_->ClipTimeSec = 0.0;
    m_->Playing = false;
    m_->SceneMutable = nullptr;
    if (m_->Importer) { delete m_->Importer; m_->Importer = nullptr; }
    if (m_->pBoneCB) { m_->pBoneCB->Release(); m_->pBoneCB = nullptr; }
    m_->Overrides.clear();
    m_CurrentType = AnimationType::None;
}

bool FbxManager::Load(ID3D11Device* device, const std::wstring& pathW)
{
    Release();
    m_->Importer = new Impl::AssimpImporterHolder();
    // Importer properties for speed/robustness
    m_->Importer->importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, 4);
    std::string pathA = Utf8FromWString(pathW);
    const aiScene* scene = m_->Importer->importer.ReadFile(pathA,
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
        aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded |
        aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph | aiProcess_LimitBoneWeights);
    if (!scene || !scene->HasMeshes()) return false;
    m_->SceneMutable = const_cast<aiScene*>(scene);
    // Save global inverse of root (row-vector mapping)
    {
        aiMatrix4x4 I = scene->mRootNode->mTransformation;
        I.Inverse();
        DirectX::XMFLOAT4X4 gi;
        gi._11 = (float)I.a1; gi._12 = (float)I.a2; gi._13 = (float)I.a3; gi._14 = (float)I.a4;
        gi._21 = (float)I.b1; gi._22 = (float)I.b2; gi._23 = (float)I.b3; gi._24 = (float)I.b4;
        gi._31 = (float)I.c1; gi._32 = (float)I.c2; gi._33 = (float)I.c3; gi._34 = (float)I.c4;
        gi._41 = (float)I.d1; gi._42 = (float)I.d2; gi._43 = (float)I.d3; gi._44 = (float)I.d4;
        m_->GlobalInverse = gi;
    }

    std::wstring baseDir = pathW;
    size_t slash = baseDir.find_last_of(L"/\\");
    baseDir = (slash == std::wstring::npos) ? L"" : baseDir.substr(0, slash + 1);

    if (!LoadMaterials(device, scene, baseDir)) return false;
    if (!BuildMeshBuffers(device, scene)) return false;

    BuildSkeletonAndNodeIndex(scene);
    CollectBonesAndOffsets(scene);

    // Rigid 애니메이션. Bone이 없음 → 가중치 누적/정규화 스킵
    if (m_->BoneNames.empty() && scene->mNumAnimations > 0)
    {
        // Rigid: 스켈레톤 노드로 가짜 본 구성 후, 각 정점에 소유 노드 1웨이트만 부여
        m_CurrentType = AnimationType::Rigid;
        BuildRigidBonesFromSkeleton();
        BuildRigidWeightsFromOwners();
        if (m_->pVB) { ApplyInfluencesToVB(device); }
        InitAnimationMetadata(scene);
        return true;
    }

    // Skinned 애니메이션. 본이 있음. → 가중치 누적/정규화 수행
    if (!m_->BoneNames.empty())
    {
        std::vector<size_t> baseVertex; BuildBaseVertexTable(scene, baseVertex);
        AccumulateVertexWeights(scene, baseVertex);
        NormalizeInfluencesAndFlag();
        if (m_->pVB) { ApplyInfluencesToVB(device); }
        InitAnimationMetadata(scene);
        m_CurrentType = AnimationType::Skinned;
        return true;
    }

    // Static Mesh 본도, 애니메이션도 없음.
    InitAnimationMetadata(scene);
    m_CurrentType = AnimationType::None;
    return true;
}

bool FbxManager::LoadMaterials(ID3D11Device* device, const aiScene* scene, const std::wstring& baseDir)
{
    if (!m_->pWhite)
    {
        UINT white = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC td{}; td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = &white; sd.SysMemPitch = sizeof(UINT);
        ComPtr<ID3D11Texture2D> tex; HR_T(device->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MipLevels = 1; srvd.Texture2D.MostDetailedMip = 0;
        HR_T(device->CreateShaderResourceView(tex.Get(), &srvd, &m_->pWhite));
    }

    m_->MaterialSRVs.assign(scene->mNumMaterials, nullptr);

    auto findCached = [&](const std::wstring& key){ auto it = m_->TexCache.find(key); return it==m_->TexCache.end()? (ID3D11ShaderResourceView*)nullptr : it->second; };
    auto addCache = [&](const std::wstring& key, ID3D11ShaderResourceView* v){ if (v) { m_->TexCache[key] = v; v->AddRef(); } };

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
                            m_->MaterialSRVs[m] = srv;
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
                            if (SUCCEEDED(device->CreateShaderResourceView(tex.Get(), &srvd, &srv))) m_->MaterialSRVs[m] = srv;
                        }
                    }
                }
            }

            // 임베디드된 텍스쳐가 없거나 실패 시, 구형 *인덱스 방식 처리
            if (!m_->MaterialSRVs[m] && !t.empty() && t[0] == '*')
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
                                m_->MaterialSRVs[m] = srv;
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
                                if (SUCCEEDED(device->CreateShaderResourceView(tex.Get(), &srvd, &srv))) m_->MaterialSRVs[m] = srv;
                            }
                        }
                    }
                }
            }

            // 위 방법 둘다 안될경우에 외부 파일 경로 시도
            if (!m_->MaterialSRVs[m])
            {
                std::wstring wtex = WStringFromUtf8(t);
                bool isAbs = (!wtex.empty() && (wtex.find(L":") != std::wstring::npos || wtex[0] == L'/' || wtex[0] == L'\\'));
                std::wstring full = isAbs ? wtex : (baseDir + wtex);
                if (auto* cached = findCached(full)) { m_->MaterialSRVs[m] = cached; cached->AddRef(); }
                else
                {
                    ComPtr<ID3D11Resource> res; ID3D11ShaderResourceView* srv = nullptr;
                    if (SUCCEEDED(CreateWICTextureFromFile(device, full.c_str(), res.GetAddressOf(), &srv))) { m_->MaterialSRVs[m] = srv; addCache(full, srv); }
                }
            }
        }
        if (!m_->MaterialSRVs[m]) { m_->MaterialSRVs[m] = m_->pWhite; if (m_->pWhite) m_->pWhite->AddRef(); }
    }
    return true;
}

bool FbxManager::BuildMeshBuffers(ID3D11Device* device, const aiScene* scene)
{
    std::vector<VertexSkinnedTBN> vertices;
    vertices.reserve(8192);
    std::vector<uint32_t> indices;
    indices.reserve(16384);
    m_->Subsets.clear();

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
                // remember owner node (mesh attached node)
                m_->VertexOwningNodeName.push_back(node->mName.C_Str());
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
            m_->Subsets.push_back({ start, count, mesh->mMaterialIndex });
        }
        for (unsigned ci = 0; ci < node->mNumChildren; ++ci) traverse(node->mChildren[ci], global);
    };
    traverse(scene->mRootNode, aiMatrix4x4());

    if (vertices.empty() || indices.empty()) return false;

    // Keep CPU copy and create VB
    m_->BindVertices = vertices;
    m_->IndicesCPU = indices;
    m_->Influences.assign(vertices.size(), {});

    m_->VertexStride = sizeof(VertexSkinnedTBN);
    D3D11_BUFFER_DESC vb{};
    vb.ByteWidth = (UINT)(vertices.size() * sizeof(VertexSkinnedTBN));
    vb.BindFlags = D3D11_BIND_VERTEX_BUFFER; vb.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA vbd{}; vbd.pSysMem = vertices.data(); HR_T(device->CreateBuffer(&vb, &vbd, &m_->pVB));

    m_->IndexCount = (int)indices.size();
    D3D11_BUFFER_DESC ib{}; ib.ByteWidth = (UINT)(indices.size() * sizeof(uint32_t)); ib.BindFlags = D3D11_BIND_INDEX_BUFFER; ib.Usage = D3D11_USAGE_DEFAULT;
    D3D11_SUBRESOURCE_DATA ibd{}; ibd.pSysMem = indices.data(); HR_T(device->CreateBuffer(&ib, &ibd, &m_->pIB));
    return true;
}

void FbxManager::SetCurrentAnimation(int idx)
{
    if (!m_->HasAnimations) return;
    if (idx < 0 || idx >= (int)m_->AnimationNames.size()) return;
    m_->CurrentClip = idx;
    m_->ClipTimeSec = 0.0;
}

void FbxManager::SetAnimationTimeSeconds(double t)
{
    if (!m_->HasAnimations || m_->CurrentClip < 0) { m_->ClipTimeSec = 0.0; return; }
    double dur = m_->ClipDurationSec[m_->CurrentClip];
    if (dur <= 0.0) { m_->ClipTimeSec = 0.0; return; }
    while (t < 0.0) t += dur;
    while (t >= dur) t -= dur;
    m_->ClipTimeSec = t;
}

void FbxManager::UpdateAnimation(ID3D11DeviceContext* ctx, double dtSec)
{
    // 시간 진행
    if (m_->Playing) SetAnimationTimeSeconds(m_->ClipTimeSec + dtSec);

    // Rigid 모드: 노드 글로벌 행렬을 팔레트로 업로드 (Offset 없음)
    if (m_CurrentType == AnimationType::Rigid)
    {
        UploadRigidNodePalette(ctx);
        return;
    }

    // Skinned 모드: 기존 스키닝 팔레트 업로드
    if (!m_->HasAnimations || m_->CurrentClip < 0) return;
    const aiScene* scene = reinterpret_cast<const aiScene*>(m_->SceneMutable);
    if (!scene) return;
    const aiAnimation* anim = (m_->HasAnimations && m_->CurrentClip >= 0) ? scene->mAnimations[m_->CurrentClip] : nullptr;
    std::unordered_map<std::wstring, const aiNodeAnim*> channelOf;
    if (anim)
    {
        for (unsigned i = 0; i < anim->mNumChannels; ++i)
        {
            const aiNodeAnim* ch = anim->mChannels[i];
            channelOf[WStringFromUtf8(ch->mNodeName.C_Str())] = ch;
        }
    }

    // 전 노드의 글로벌 행렬 평가(키 보간 포함)
    std::vector<DirectX::XMFLOAT4X4> global;
    EvaluateGlobalMatrices(scene, channelOf, global);

    // 스키닝 팔레트 구성: GlobalInverse * Global(node) * Offset
    std::vector<DirectX::XMMATRIX> palette;
    BuildBonePalette(global, palette);
    // 본 팔레트 상수 버퍼 업로드
    UploadBonePalette(ctx, palette);
}

void FbxManager::UpdateAnimationGrid(ID3D11DeviceContext* ctx, double dtSec)
{
    if (m_CurrentType == AnimationType::Rigid)
    {
        UploadRigidNodePalette(ctx);
        return;
    }
    UpdateAnimation(ctx, 0.0); // 시간은 유지
    std::vector<DirectX::XMFLOAT4X4> global;
    GetCurrentGlobalMatrices(global);
    std::vector<DirectX::XMMATRIX> pal;
    BuildBonePalette(global, pal);
    UploadBonePalette(ctx, pal);
}

// ---------------- pImpl implementations ----------------

bool FbxManager::HasMesh() const { return m_ && m_->pVB && m_->pIB && m_->IndexCount > 0; }
ID3D11Buffer* FbxManager::GetVertexBuffer() const { return m_ ? m_->pVB : nullptr; }
ID3D11Buffer* FbxManager::GetIndexBuffer() const { return m_ ? m_->pIB : nullptr; }
int FbxManager::GetIndexCount() const { return m_ ? m_->IndexCount : 0; }
UINT FbxManager::GetVertexStride() const { return m_ ? (UINT)m_->VertexStride : 0; }
UINT FbxManager::GetVertexOffset() const { return 0; }

const std::vector<FbxSubset>& FbxManager::GetSubsets() const { return m_->Subsets; }
const std::vector<ID3D11ShaderResourceView*>& FbxManager::GetMaterialSRVs() const { return m_->MaterialSRVs; }

bool FbxManager::HasSkeleton() const { return m_->HasSkinning; }
bool FbxManager::HasAnimations() const { return m_->HasAnimations; }
const std::vector<FbxManager::SkeletonNode>& FbxManager::GetSkeleton() const { return m_->Skeleton; }
int FbxManager::GetSkeletonRoot() const { return m_->RootIndex; }

const std::vector<std::string>& FbxManager::GetAnimationNames() const { return m_->AnimationNames; }
int FbxManager::GetCurrentAnimationIndex() const { return m_->CurrentClip; }
void FbxManager::SetAnimationPlaying(bool playing) { m_->Playing = playing; }
bool FbxManager::IsAnimationPlaying() const { return m_->Playing; }
double FbxManager::GetAnimationTimeSeconds() const { return m_->ClipTimeSec; }
double FbxManager::GetClipDurationSec(int idx) const { return (idx>=0 && idx<(int)m_->ClipDurationSec.size()) ? m_->ClipDurationSec[idx] : 0.0; }

ID3D11Buffer* FbxManager::GetBoneConstantBuffer() const { return m_->pBoneCB; }
UINT FbxManager::GetBoneCount() const { return (UINT)m_->BoneNames.size(); }

void FbxManager::RebuildChannelMap()
{
    m_->ChannelOfNode.assign(m_->Skeleton.size(), nullptr);
    const aiScene* scene = reinterpret_cast<const aiScene*>(m_->SceneMutable);
    if (!scene || !m_->HasAnimations || m_->CurrentClip < 0) return;
    const aiAnimation* anim = scene->mAnimations[m_->CurrentClip];
    for (unsigned i = 0; i < anim->mNumChannels; ++i)
    {
        const aiNodeAnim* ch = anim->mChannels[i];
        auto it = m_->NodeIndexOfName.find(ch->mNodeName.C_Str());
        if (it != m_->NodeIndexOfName.end())
        {
            int nodeIdx = it->second;
            if (nodeIdx >= 0 && nodeIdx < (int)m_->ChannelOfNode.size()) m_->ChannelOfNode[nodeIdx] = ch;
        }
    }
}

// --- Overrides public API ---
void FbxManager::SetBoneOverride(int nodeIndex, const BoneOverride& ov)
{
    if (nodeIndex < 0) return;
    if ((size_t)nodeIndex >= m_->Overrides.size()) m_->Overrides.resize((size_t)nodeIndex + 1);
    m_->Overrides[nodeIndex].T = ov.T;
    m_->Overrides[nodeIndex].Rdeg = ov.Rdeg;
    m_->Overrides[nodeIndex].S = ov.S;
    m_->Overrides[nodeIndex].enabled = ov.enabled;
}

bool FbxManager::GetBoneOverride(int nodeIndex, BoneOverride& out) const
{
    if (nodeIndex < 0 || (size_t)nodeIndex >= m_->Overrides.size()) return false;
    const auto& o = m_->Overrides[nodeIndex];
    out.T = o.T; out.Rdeg = o.Rdeg; out.S = o.S; out.enabled = o.enabled;
    return o.enabled;
}

void FbxManager::ResetBoneOverrides()
{
    for (auto& o : m_->Overrides) { o.T = {0,0,0}; o.Rdeg = {0,0,0}; o.S = {1,1,1}; o.enabled = false; }
}

bool FbxManager::HasBoneOverrides() const
{
    for (const auto& o : m_->Overrides) if (o.enabled) return true; return false;
}


