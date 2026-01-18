#include "pch.h"
#include "PmxManager.h"
#include "Helper.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <directxtk/WICTextureLoader.h>
#include <d3d11.h>
#include <algorithm>
#include <fstream>
#include <Windows.h>
// MMDFormats VMD parser (user-attached)
#include "../22_VMD/Vmd.h"

using Microsoft::WRL::ComPtr;

static std::string StringFromAi(const aiString& s) { return std::string(s.C_Str()); }

// ---------------- Minimal VMD reader (no external dependency) ----------------
namespace {
    static std::wstring ConvertSjisToWString(const char* buf, size_t len)
    {
        if (!buf || len == 0) return L"";
        int wlen = MultiByteToWideChar(932 /*CP_SJIS*/, 0, buf, (int)len, nullptr, 0);
        if (wlen <= 0) return L"";
        std::wstring out; out.resize((size_t)wlen);
        MultiByteToWideChar(932, 0, buf, (int)len, out.data(), wlen);
        // trim trailing nulls
        size_t pos = out.find_first_of(L"\0");
        if (pos != std::wstring::npos) out.resize(pos);
        return out;
    }

    struct VMDRawMotion
    {
        char boneName[15];
        uint32_t frame;
        float translate[3];
        float quaternion[4];
        uint8_t interpolation[64];
    };

    static bool ReadVMDMotions(const std::wstring& pathW, std::vector<VMDRawMotion>& out)
    {
        std::ifstream ifs(pathW, std::ios::binary);
        if (!ifs) return false;
        // header
        char header[30] = {}; char model[20] = {};
        ifs.read(header, 30); if (!ifs) return false;
        ifs.read(model, 20); if (!ifs) return false;
        uint32_t numMotions = 0;
        ifs.read(reinterpret_cast<char*>(&numMotions), 4); if (!ifs) return false;
        out.clear(); out.reserve(numMotions);
        // VMD spec: bone motion entry is 111 bytes (no padding)
        constexpr size_t kVmdMotionSize = 111;
        for (uint32_t i = 0; i < numMotions; ++i)
        {
            unsigned char buf[kVmdMotionSize];
            ifs.read(reinterpret_cast<char*>(buf), kVmdMotionSize);
            if (!ifs) return false;
            VMDRawMotion m{};
            // 0..14 : boneName[15]
            memcpy(m.boneName, buf + 0, 15);
            // 15..18 : frame (uint32 LE)
            memcpy(&m.frame, buf + 15, 4);
            // 19..30 : translate (float x3)
            memcpy(m.translate, buf + 19, 12);
            // 31..46 : quaternion (float x4)
            memcpy(m.quaternion, buf + 31, 16);
            // 47..110 : interpolation (64)
            memcpy(m.interpolation, buf + 47, 64);
            out.push_back(m);
        }
        // we ignore morphs/camera/light sections for simplicity
        return true;
    }
}

void PmxManager::Release()
{
    for (auto& srv : m_MaterialSRVs) { if (srv) { srv->Release(); srv = nullptr; } }
	m_MaterialSRVs.clear();
    if (m_pWhiteSRV) { m_pWhiteSRV->Release(); m_pWhiteSRV = nullptr; }
    for (auto& kv : m_TexCache) { if (kv.second) { kv.second->Release(); kv.second = nullptr; } }
	m_TexCache.clear();
    if (m_pVB) { m_pVB->Release(); m_pVB = nullptr; }
    if (m_pIB) { m_pIB->Release(); m_pIB = nullptr; }
	m_Vertices.clear();
	m_Indices.clear();
	m_Subsets.clear();
	m_IndexCount = 0;
    // skeleton/animation
    m_Influences.clear();
    m_BoneNames.clear();
    m_BoneOffset.clear();
    m_BoneIndexOfName.clear();
    m_NodeIndexOfName.clear();
    m_Skeleton.clear();
    m_LocalBind.clear();
    m_RootIndex = -1;
    m_HasSkinning = false;
    m_GlobalInverse = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    if (m_pBoneCB) { m_pBoneCB->Release(); m_pBoneCB = nullptr; }
    // VMD
    m_VMDChannels.clear();
    m_HasVMD = false;
    m_AnimTimeSec = 0.0;
    m_ClipDurationSec = 0.0;
    m_Playing = false;
}

bool PmxManager::Load(ID3D11Device* device, const std::wstring& pmxPath)
{
	Release();
	Assimp::Importer importer;
	std::string pathA = Utf8FromWString(pmxPath);
	const aiScene* scene = importer.ReadFile(pathA,
		aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
		aiProcess_ImproveCacheLocality | aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded |
		aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph | aiProcess_LimitBoneWeights);
	if (!scene || !scene->HasMeshes())
	{
		return false;
	}

	std::wstring modelPathW = pmxPath;
	size_t slash = modelPathW.find_last_of(L"/\\");
	std::wstring baseDir = (slash == std::wstring::npos) ? L"" : modelPathW.substr(0, slash + 1);

	if (!LoadMaterials(device, scene, baseDir)) return false;
	if (!BuildMeshBuffers(device, scene)) return false;

	// build skeleton/weights
	{
		aiMatrix4x4 I = scene->mRootNode->mTransformation; I.Inverse();
		DirectX::XMFLOAT4X4 gi;
		gi._11 = (float)I.a1; gi._12 = (float)I.a2; gi._13 = (float)I.a3; gi._14 = (float)I.a4;
		gi._21 = (float)I.b1; gi._22 = (float)I.b2; gi._23 = (float)I.b3; gi._24 = (float)I.b4;
		gi._31 = (float)I.c1; gi._32 = (float)I.c2; gi._33 = (float)I.c3; gi._34 = (float)I.c4;
		gi._41 = (float)I.d1; gi._42 = (float)I.d2; gi._43 = (float)I.d3; gi._44 = (float)I.d4;
		m_GlobalInverse = gi;
	}
	BuildSkeletonAndNodeIndex(scene);
	CollectBonesAndOffsets(scene);
    // 어떤 FBX/PMX는 본 이름 노드가 씬 트리에 없을 수 있으므로 보강
    EnsureBoneNodesExist();
	std::vector<size_t> baseVertex; BuildBaseVertexTable(scene, baseVertex);
	AccumulateVertexWeights(scene, baseVertex);
	NormalizeInfluencesAndFlag();
	if (m_pVB) { ApplyInfluencesToVB(device); }
	EnsureBoneCB(device);
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
							td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // aiTexel은 BGRA
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
                aiVector3D p = mesh->mVertices[i];
				aiVector3D n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0,1,0);
				aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0,0,0);
				aiColor4D c = mesh->HasVertexColors(0) ? mesh->mColors[0][i] : aiColor4D(1,1,1,1);
				aiVector3D tg = mesh->HasTangentsAndBitangents() ? mesh->mTangents[i]   : aiVector3D(1,0,0);
				aiVector3D bt = mesh->HasTangentsAndBitangents() ? mesh->mBitangents[i] : aiVector3D(0,1,0);
                VertexSkinnedTBN v{};
                v.pos = { p.x, p.y, p.z };
                v.n = { n.x, n.y, n.z };
                v.t = { tg.x, tg.y, tg.z };
                v.b = { bt.x, bt.y, bt.z };
                v.color = { c.r, c.g, c.b, c.a };
                v.uv = { uv.x, uv.y };
                v.boneIdx[0] = v.boneIdx[1] = v.boneIdx[2] = v.boneIdx[3] = 0;
                v.boneWeight = { 1,0,0,0 };
                m_Vertices.push_back(v);
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

    // stride
    m_VertexStride = (int)sizeof(VertexSkinnedTBN);

	// VB
	D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = (UINT)(m_Vertices.size() * sizeof(VertexSkinnedTBN));
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

void PmxManager::BuildBonePalette(const std::vector<DirectX::XMFLOAT4X4>& global, std::vector<DirectX::XMMATRIX>& outPalette) const
{
    outPalette.resize(m_BoneNames.size(), DirectX::XMMatrixIdentity());
    for (size_t bi = 0; bi < m_BoneNames.size(); ++bi)
    {
        int nodeIdx = -1;
        // 우선 wide 이름으로 매핑 시도
        if (bi < m_BoneNamesW.size())
        {
            auto itNW = m_NodeIndexOfNameW.find(m_BoneNamesW[bi]);
            if (itNW != m_NodeIndexOfNameW.end()) nodeIdx = itNW->second;
        }
        // 실패 시 UTF-8 키로 재시도
        if (nodeIdx < 0)
        {
            auto itN = m_NodeIndexOfName.find(m_BoneNames[bi]);
            if (itN != m_NodeIndexOfName.end()) nodeIdx = itN->second;
        }
        if (nodeIdx < 0 || nodeIdx >= (int)global.size()) continue;
        DirectX::XMMATRIX G = DirectX::XMLoadFloat4x4(&global[nodeIdx]);
        DirectX::XMMATRIX Off = DirectX::XMLoadFloat4x4(&m_BoneOffset[bi]);
        DirectX::XMMATRIX Gi = DirectX::XMLoadFloat4x4(&m_GlobalInverse);
        outPalette[bi] = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(Gi, G), Off);
    }
}

void PmxManager::UploadBonePalette(ID3D11DeviceContext* ctx, const std::vector<DirectX::XMMATRIX>& palette)
{
    if (!ctx) return;
    Microsoft::WRL::ComPtr<ID3D11Device> dev; ctx->GetDevice(dev.GetAddressOf());
    EnsureBoneCB(dev.Get());
    if (!m_pBoneCB) return;
    struct BoneCB { DirectX::XMFLOAT4X4 m[kMaxBones]; unsigned int boneCount; float pad[3]; };
    BoneCB cb{};
    size_t n = std::min(palette.size(), (size_t)kMaxBones);
    DirectX::XMMATRIX I = DirectX::XMMatrixIdentity();
    for (size_t i = 0; i < (size_t)kMaxBones; ++i) { DirectX::XMStoreFloat4x4(&cb.m[i], I); }
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

void PmxManager::UploadIdentityPalette(ID3D11DeviceContext* ctx)
{
    if (!ctx) return;
    std::vector<DirectX::XMMATRIX> pal;
    pal.resize(GetBoneCount(), DirectX::XMMatrixIdentity());
    UploadBonePalette(ctx, pal);
}

bool PmxManager::LoadVMD(ID3D11Device* device, const std::wstring& vmdPath)
{
    (void)device;
    // 초기화
    m_VMDChannels.clear();
    m_HasVMD = false;
    m_AnimTimeSec = 0.0;
    m_ClipDurationSec = 0.0;

    // 우선 MMDFormats 파서 사용 시도 (사용자가 첨부한 파서)
    {
        std::ifstream stream(vmdPath, std::ios::binary);
        if (stream)
        {
            auto motion = vmd::VmdMotion::LoadFromStream(&stream);
            if (motion)
            {
                uint32_t maxFrame = 0;
                for (const auto& bf : motion->bone_frames)
                {
                    // 이름은 Shift-JIS. CP932로 와이드 변환
                    std::wstring boneW = ConvertSjisToWString(bf.name.c_str(), bf.name.size());
                    VMDKey k{};
                    k.t = (double)std::max(0, bf.frame) / 30.0; // 30fps 고정 가정
                    k.T = { bf.position[0], bf.position[1], bf.position[2] };
                    k.Q = { bf.orientation[0], bf.orientation[1], bf.orientation[2], bf.orientation[3] };
                    m_VMDChannels[boneW].push_back(k);
                    if ((uint32_t)std::max(0, bf.frame) > maxFrame) maxFrame = (uint32_t)bf.frame;
                }
                for (auto& kv : m_VMDChannels)
                {
                    auto& vec = kv.second;
                    std::sort(vec.begin(), vec.end(), [](const VMDKey& a, const VMDKey& b){ return a.t < b.t; });
                }
                m_ClipDurationSec = (double)maxFrame / 30.0;
                m_AnimTimeSec = 0.0;
                m_HasVMD = (m_ClipDurationSec > 0.0);
                if (m_HasVMD) return true;
            }
        }
    }

    // 폴백: 최소 파서로 재시도
    {
        std::vector<VMDRawMotion> motions;
        if (!ReadVMDMotions(vmdPath, motions)) return false;
        uint32_t maxFrame = 0;
        for (const auto& m : motions)
        {
            std::wstring boneW = ConvertSjisToWString(m.boneName, 15);
            VMDKey k{}; k.t = (double)m.frame / 30.0; // 30fps
            k.T = { m.translate[0], m.translate[1], m.translate[2] };
            k.Q = { m.quaternion[0], m.quaternion[1], m.quaternion[2], m.quaternion[3] };
            m_VMDChannels[boneW].push_back(k);
            if (m.frame > maxFrame) maxFrame = m.frame;
        }
        for (auto& kv : m_VMDChannels)
        {
            auto& vec = kv.second;
            std::sort(vec.begin(), vec.end(), [](const VMDKey& a, const VMDKey& b){ return a.t < b.t; });
        }
        m_ClipDurationSec = (double)maxFrame / 30.0;
        m_AnimTimeSec = 0.0;
        m_HasVMD = (m_ClipDurationSec > 0.0);
        return m_HasVMD;
    }
}

void PmxManager::SetAnimationTimeSeconds(double t)
{
    if (m_ClipDurationSec <= 0.0) { m_AnimTimeSec = 0.0; return; }
    while (t < 0.0) t += m_ClipDurationSec;
    while (t >= m_ClipDurationSec) t -= m_ClipDurationSec;
    m_AnimTimeSec = t;
}

void PmxManager::UpdateAnimation(ID3D11DeviceContext* ctx, double dtSec)
{
    if (m_Playing) SetAnimationTimeSeconds(m_AnimTimeSec + dtSec);
    // VMD가 없어도 스켈레톤 정지 포즈 팔레트를 유지해야 버텍스가 0,0,0로 붕괴되지 않음
    if (!m_HasVMD)
    {
        UploadIdentityPalette(ctx);
        return;
    }
    std::vector<DirectX::XMFLOAT4X4> global;
    // Evaluate globals: apply VMD TR on top of bind-local but pre-multiply to avoid double-scale
    global.resize(m_Skeleton.size());
    std::function<void(int, const DirectX::XMMATRIX&)> eval = [&](int idx, const DirectX::XMMATRIX& parent){
        const auto& node = m_Skeleton[idx];
        DirectX::XMMATRIX Lbind = (idx >= 0 && idx < (int)m_LocalBind.size()) ? DirectX::XMLoadFloat4x4(&m_LocalBind[idx]) : DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX L = Lbind;
        auto it = m_VMDChannels.find(node.nameW);
        if (it != m_VMDChannels.end() && !it->second.empty())
        {
            const auto& keys = it->second;
            int i = 0; while (i + 1 < (int)keys.size() && m_AnimTimeSec >= keys[i + 1].t) ++i;
            int j = (i + 1 < (int)keys.size()) ? i + 1 : i;
            double dt = keys[j].t - keys[i].t; double a = (dt > 0.0) ? (m_AnimTimeSec - keys[i].t) / dt : 0.0;
            DirectX::XMFLOAT3 T0 = keys[i].T, T1 = keys[j].T; DirectX::XMVECTOR T = DirectX::XMVectorLerp(XMLoadFloat3(&T0), XMLoadFloat3(&T1), (float)a);
            DirectX::XMFLOAT4 Q0 = keys[i].Q, Q1 = keys[j].Q; DirectX::XMVECTOR Q = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionSlerp(XMLoadFloat4(&Q0), XMLoadFloat4(&Q1), (float)a));
            DirectX::XMMATRIX mR = DirectX::XMMatrixRotationQuaternion(Q);
            DirectX::XMMATRIX mT = DirectX::XMMatrixTranslationFromVector(T);
            // FBX 파이프라인과 일관: 로컬= (T*R) * Lbind  (pivot=bind 위치, 이중 스케일 방지)
            L = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(mT, mR), Lbind);
        }
        DirectX::XMMATRIX G = DirectX::XMMatrixMultiply(parent, L);
        DirectX::XMStoreFloat4x4(&global[idx], G);
        for (int ch : node.children) eval(ch, G);
    };
    if (m_RootIndex >= 0) eval(m_RootIndex, DirectX::XMMatrixIdentity());
    std::vector<DirectX::XMMATRIX> palette; BuildBonePalette(global, palette);
    UploadBonePalette(ctx, palette);
}

// -------- Skeleton & Weights helpers --------
void PmxManager::BuildSkeletonAndNodeIndex(const aiScene* scene)
{
    m_Skeleton.clear();
    m_NodeIndexOfName.clear();
    m_LocalBind.clear();
    std::function<int(const aiNode*, int)> build = [&](const aiNode* node, int parent){
        int idx = (int)m_Skeleton.size();
        std::string nmUtf8 = node->mName.C_Str();
        std::wstring nmW = WStringFromUtf8(nmUtf8);
        PmxManager::SkeletonNode sn{}; sn.name = nmUtf8; sn.nameW = nmW; sn.parent = parent; sn.isBone = false;
        m_Skeleton.push_back(std::move(sn));
        m_NodeIndexOfName[m_Skeleton.back().name] = idx;
        m_NodeIndexOfNameW[m_Skeleton.back().nameW] = idx;
        DirectX::XMFLOAT4X4 bind{};
        bind._11 = (float)node->mTransformation.a1; bind._12 = (float)node->mTransformation.a2; bind._13 = (float)node->mTransformation.a3; bind._14 = (float)node->mTransformation.a4;
        bind._21 = (float)node->mTransformation.b1; bind._22 = (float)node->mTransformation.b2; bind._23 = (float)node->mTransformation.b3; bind._24 = (float)node->mTransformation.b4;
        bind._31 = (float)node->mTransformation.c1; bind._32 = (float)node->mTransformation.c2; bind._33 = (float)node->mTransformation.c3; bind._34 = (float)node->mTransformation.c4;
        bind._41 = (float)node->mTransformation.d1; bind._42 = (float)node->mTransformation.d2; bind._43 = (float)node->mTransformation.d3; bind._44 = (float)node->mTransformation.d4;
        m_LocalBind.push_back(bind);
        for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
        {
            int ch = build(node->mChildren[ci], idx);
            m_Skeleton[idx].children.push_back(ch);
        }
        return idx;
    };
    m_RootIndex = build(scene->mRootNode, -1);
}

void PmxManager::CollectBonesAndOffsets(const aiScene* scene)
{
    m_HasSkinning = false;
    m_BoneNames.clear();
    m_BoneNamesW.clear();
    m_BoneOffset.clear();
    m_BoneIndexOfName.clear();
    for (unsigned mi = 0; mi < scene->mNumMeshes; ++mi)
    {
        const aiMesh* mesh = scene->mMeshes[mi];
        for (unsigned bi = 0; bi < mesh->mNumBones; ++bi)
        {
            const aiBone* b = mesh->mBones[bi];
            std::string name = b->mName.C_Str();
            std::wstring nameW = WStringFromUtf8(name);
            if (m_BoneIndexOfName.find(name) == m_BoneIndexOfName.end())
            {
                int newIndex = (int)m_BoneNames.size();
                m_BoneIndexOfName[name] = newIndex;
                m_BoneNames.push_back(name);
                m_BoneNamesW.push_back(nameW);
                DirectX::XMFLOAT4X4 off;
                off._11 = (float)b->mOffsetMatrix.a1; off._12 = (float)b->mOffsetMatrix.a2; off._13 = (float)b->mOffsetMatrix.a3; off._14 = (float)b->mOffsetMatrix.a4;
                off._21 = (float)b->mOffsetMatrix.b1; off._22 = (float)b->mOffsetMatrix.b2; off._23 = (float)b->mOffsetMatrix.b3; off._24 = (float)b->mOffsetMatrix.b4;
                off._31 = (float)b->mOffsetMatrix.c1; off._32 = (float)b->mOffsetMatrix.c2; off._33 = (float)b->mOffsetMatrix.c3; off._34 = (float)b->mOffsetMatrix.c4;
                off._41 = (float)b->mOffsetMatrix.d1; off._42 = (float)b->mOffsetMatrix.d2; off._43 = (float)b->mOffsetMatrix.d3; off._44 = (float)b->mOffsetMatrix.d4;
                m_BoneOffset.push_back(off);
                auto itNode = m_NodeIndexOfName.find(name);
                if (itNode != m_NodeIndexOfName.end()) {
                    m_Skeleton[itNode->second].isBone = true;
                    if (m_Skeleton[itNode->second].nameW.empty()) m_Skeleton[itNode->second].nameW = nameW;
                }
                auto itNodeW = m_NodeIndexOfNameW.find(nameW);
                if (itNodeW != m_NodeIndexOfNameW.end()) {
                    m_Skeleton[itNodeW->second].isBone = true;
                }
            }
        }
    }
}

void PmxManager::BuildBaseVertexTable(const aiScene* scene, std::vector<size_t>& baseVertex)
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

void PmxManager::AccumulateVertexWeights(const aiScene* scene, const std::vector<size_t>& baseVertex)
{
    if (!m_Influences.empty()) m_Influences.clear();
    m_Influences.assign(m_Vertices.size(), {});
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
                int slot = 0; float minW = m_Influences[v].w[0];
                for (int s = 1; s < 4; ++s) { if (m_Influences[v].w[s] < minW) { minW = m_Influences[v].w[s]; slot = s; } }
                m_Influences[v].idx[slot] = (unsigned short)boneIdx;
                m_Influences[v].w[slot] = (float)vw.mWeight;
            }
        }
    }
}

void PmxManager::NormalizeInfluencesAndFlag()
{
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
            inf.idx[0] = 0; inf.w[0] = 1.0f;
            inf.idx[1] = 0; inf.w[1] = 0.0f;
            inf.idx[2] = 0; inf.w[2] = 0.0f;
            inf.idx[3] = 0; inf.w[3] = 0.0f;
        }
    }
    m_HasSkinning = !m_BoneNames.empty();
}

void PmxManager::ApplyInfluencesToVB(ID3D11Device* device)
{
    if (m_Vertices.size() != m_Influences.size()) return;
    for (size_t i = 0; i < m_Vertices.size(); ++i)
    {
        const auto& inf = m_Influences[i];
        m_Vertices[i].boneIdx[0] = inf.idx[0];
        m_Vertices[i].boneIdx[1] = inf.idx[1];
        m_Vertices[i].boneIdx[2] = inf.idx[2];
        m_Vertices[i].boneIdx[3] = inf.idx[3];
        m_Vertices[i].boneWeight = { inf.w[0], inf.w[1], inf.w[2], inf.w[3] };
    }
    if (m_pVB) { m_pVB->Release(); m_pVB = nullptr; }
    D3D11_BUFFER_DESC vb{}; vb.BindFlags = D3D11_BIND_VERTEX_BUFFER; vb.Usage = D3D11_USAGE_DEFAULT;
    vb.ByteWidth = (UINT)(m_Vertices.size() * sizeof(VertexSkinnedTBN));
    D3D11_SUBRESOURCE_DATA vbd{}; vbd.pSysMem = m_Vertices.data();
    HR_T(device->CreateBuffer(&vb, &vbd, &m_pVB));
}

void PmxManager::EnsureBoneCB(ID3D11Device* device)
{
    if (m_pBoneCB || !device) return;
    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(DirectX::XMFLOAT4X4) * kMaxBones + sizeof(unsigned int) + sizeof(float) * 3;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HR_T(device->CreateBuffer(&bd, nullptr, &m_pBoneCB));
}

void PmxManager::EnsureBoneNodesExist()
{
    if (m_RootIndex < 0) return;
    // 본 이름인데 트리에 없는 경우, 루트의 자식으로 노드를 추가하여 채널 매핑/팔레트 가능하게 만듦
    for (size_t bi = 0; bi < m_BoneNames.size(); ++bi)
    {
        const std::string& name = m_BoneNames[bi];
        const std::wstring& nameW = (bi < m_BoneNamesW.size()) ? m_BoneNamesW[bi] : WStringFromUtf8(name);
        if (m_NodeIndexOfName.find(name) != m_NodeIndexOfName.end()) continue;
        if (m_NodeIndexOfNameW.find(nameW) != m_NodeIndexOfNameW.end()) continue;
        int idx = (int)m_Skeleton.size();
        PmxManager::SkeletonNode sn{};
        sn.name = name; sn.nameW = nameW; sn.parent = m_RootIndex; sn.isBone = true;
        m_Skeleton.push_back(sn);
        m_NodeIndexOfName[name] = idx;
        m_NodeIndexOfNameW[nameW] = idx;
        m_Skeleton[m_RootIndex].children.push_back(idx);
        DirectX::XMFLOAT4X4 I{ 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        m_LocalBind.push_back(I);
    }
}
