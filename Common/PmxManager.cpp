#include "pch.h"
#include "PmxManager.h"
#include "Helper.h"

#include <unordered_map>
#include <fstream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <directxtk/WICTextureLoader.h>

using Microsoft::WRL::ComPtr;

static std::wstring WStringFromUtf8(const std::string& s) { return std::wstring(s.begin(), s.end()); }

// Shift-JIS(CP932) 15바이트 본 이름을 UTF-8로 변환
static std::string Utf8FromShiftJis15(const char* name15)
{
    // 유효 길이 탐색 (널 종료 또는 15바이트)
    int lenSJIS = 0; while (lenSJIS < 15 && name15[lenSJIS] != '\0') ++lenSJIS;
    if (lenSJIS == 0) return std::string();
    // Shift-JIS(CP 932) -> UTF-16
    int wlen = MultiByteToWideChar(932 /*CP932*/, 0, name15, lenSJIS, nullptr, 0);
    std::wstring w; w.resize((size_t)wlen);
    if (wlen > 0) MultiByteToWideChar(932, 0, name15, lenSJIS, &w[0], wlen);
    // UTF-16 -> UTF-8
    int u8len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string u8; u8.resize((size_t)u8len);
    if (u8len > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &u8[0], u8len, nullptr, nullptr);
    return u8;
}

void PmxManager::Release()
{
	m_LastError.clear();
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
	m_LoadedPath.clear();
	m_HasVmd = false;
	m_VmdPath.clear();
	m_VmdInfo = {};
	m_Playing = false; m_TimeSec = 0.0f; m_Speed = 1.0f; m_Loop = true;
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
		m_LastError = L"Assimp 로드 실패 혹은 Mesh 없음";
		return false;
	}

	std::wstring modelPathW = pmxPath;
	size_t slash = modelPathW.find_last_of(L"/\\");
	std::wstring baseDir = (slash == std::wstring::npos) ? L"" : modelPathW.substr(0, slash + 1);

	if (!LoadMaterials(device, scene, baseDir)) { m_LastError = L"머티리얼 로드 실패"; return false; }
	if (!BuildMeshBuffers(device, scene)) { m_LastError = L"메시 버퍼 생성 실패"; return false; }
	m_LoadedPath = pmxPath;
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
	m_OrigPositions.clear();
	m_OrigNormals.clear();
	m_Bones.clear();
	m_VertexInfluences.clear();
	m_CurrentBoneMatrices.clear();
    SAFE_RELEASE(m_pInfluenceVB);

	// 미리 할당: 노드 트래버스로 총합 산출
	uint64_t totalVertices = 0;
	uint64_t totalIndices = 0;
	uint64_t totalSubsets = 0;
	std::function<void(const aiNode*)> countTraverse = [&](const aiNode* node){
		for (unsigned mi = 0; mi < node->mNumMeshes; ++mi) {
			const aiMesh* mesh = scene->mMeshes[node->mMeshes[mi]];
			totalVertices += mesh->mNumVertices;
			totalIndices += mesh->mNumFaces * 3ull;
			++totalSubsets;
		}
		for (unsigned ci = 0; ci < node->mNumChildren; ++ci) countTraverse(node->mChildren[ci]);
	};
	countTraverse(scene->mRootNode);
	m_Vertices.reserve((size_t)totalVertices);
	m_Indices.reserve((size_t)totalIndices);
	m_Subsets.reserve((size_t)totalSubsets);

    auto transformPoint = [](const aiVector3D& v, const aiMatrix4x4& m) -> aiVector3D {
        aiVector3D r; r.x = v.x * m.a1 + v.y * m.b1 + v.z * m.c1 + m.d1;
        r.y = v.x * m.a2 + v.y * m.b2 + v.z * m.c2 + m.d2;
        r.z = v.x * m.a3 + v.y * m.b3 + v.z * m.c3 + m.d3; return r; };

    // 노드 트리 수집 (스켈레톤)
    m_Nodes.clear(); m_NameToNode.clear();
    std::function<void(const aiNode*, int, const aiMatrix4x4&)> buildNodes;
    buildNodes = [&](const aiNode* node, int parent, const aiMatrix4x4& parentM){
        int idx = (int)m_Nodes.size();
        m_Nodes.push_back({ node->mName.C_Str(), parent,
            XMMatrixSet(
                node->mTransformation.a1, node->mTransformation.a2, node->mTransformation.a3, node->mTransformation.a4,
                node->mTransformation.b1, node->mTransformation.b2, node->mTransformation.b3, node->mTransformation.b4,
                node->mTransformation.c1, node->mTransformation.c2, node->mTransformation.c3, node->mTransformation.c4,
                node->mTransformation.d1, node->mTransformation.d2, node->mTransformation.d3, node->mTransformation.d4)
        });
        m_NameToNode[m_Nodes.back().name] = idx;
        for (unsigned ci=0; ci<node->mNumChildren; ++ci) buildNodes(node->mChildren[ci], idx, node->mTransformation);
    };
    buildNodes(scene->mRootNode, -1, aiMatrix4x4());
    // 바인드포즈 글로벌 계산 (노드 계층)
    m_NodeBindGlobal.assign(m_Nodes.size(), DirectX::XMMatrixIdentity());
    // 루트부터 순서대로 채움 (m_Nodes는 DFS 순서라 부모 인덱스 < 자식 인덱스 보장)
    for (size_t i=0;i<m_Nodes.size();++i)
    {
        int parent = m_Nodes[i].parent;
        DirectX::XMMATRIX L = m_Nodes[i].local;
        m_NodeBindGlobal[i] = (parent>=0) ? (L * m_NodeBindGlobal[parent]) : L;
    }

    std::function<void(const aiNode*, const aiMatrix4x4&)> traverse;
    traverse = [&](const aiNode* node, const aiMatrix4x4& parent) {
		aiMatrix4x4 global = parent * node->mTransformation;
		for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
		{
			const aiMesh* mesh = scene->mMeshes[node->mMeshes[mi]];
			size_t baseIndex = m_Vertices.size();
            // Ensure a synthetic bone for this node to support rigid vertices
            uint32_t nodeBoneIndex;
            {
                auto it = m_BoneNameToIndex.find(node->mName.C_Str());
                if (it == m_BoneNameToIndex.end())
                {
                    Bone nb{}; nb.name = node->mName.C_Str(); nb.offset = DirectX::XMMatrixIdentity();
                    nodeBoneIndex = (uint32_t)m_Bones.size();
                    m_Bones.push_back(nb);
                    m_BoneNameToIndex[nb.name] = nodeBoneIndex;
                }
                else nodeBoneIndex = it->second;
            }
			for (unsigned i = 0; i < mesh->mNumVertices; ++i)
			{
                // Store in mesh local space (no node global baked). Skinning/bones will move them.
                aiVector3D p = mesh->mVertices[i];
                aiVector3D n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0,1,0);
				aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0,0,0);
				aiColor4D c = mesh->HasVertexColors(0) ? mesh->mColors[0][i] : aiColor4D(1,1,1,1);
				m_Vertices.push_back({ XMFLOAT3(p.x, p.y, p.z), XMFLOAT3(n.x, n.y, n.z), XMFLOAT4(c.r, c.g, c.b, c.a), XMFLOAT2(uv.x, uv.y) });
				m_OrigPositions.push_back(XMFLOAT3(p.x, p.y, p.z));
				m_OrigNormals.push_back(XMFLOAT3(n.x, n.y, n.z));
				m_VertexInfluences.push_back({});
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
			// 본 가중치 수집 (Assimp)
            if (mesh->HasBones())
			{
				for (unsigned bi = 0; bi < mesh->mNumBones; ++bi)
				{
					const aiBone* b = mesh->mBones[bi];
					Bone bone{}; bone.name = b->mName.C_Str();
					bone.offset = XMMatrixSet(
						b->mOffsetMatrix.a1, b->mOffsetMatrix.a2, b->mOffsetMatrix.a3, b->mOffsetMatrix.a4,
						b->mOffsetMatrix.b1, b->mOffsetMatrix.b2, b->mOffsetMatrix.b3, b->mOffsetMatrix.b4,
						b->mOffsetMatrix.c1, b->mOffsetMatrix.c2, b->mOffsetMatrix.c3, b->mOffsetMatrix.c4,
						b->mOffsetMatrix.d1, b->mOffsetMatrix.d2, b->mOffsetMatrix.d3, b->mOffsetMatrix.d4);
					uint32_t boneIndex = (uint32_t)m_Bones.size();
					m_Bones.push_back(bone);
                    m_BoneNameToIndex[bone.name] = boneIndex;
					for (unsigned w = 0; w < b->mNumWeights; ++w)
					{
						uint32_t v = (uint32_t)baseIndex + b->mWeights[w].mVertexId;
						float    wv = b->mWeights[w].mWeight;
						// influence 4개 제한
						VertexInfluence& inf = m_VertexInfluences[v];
						int slot = -1; for (int k=0;k<4;++k) if (inf.weights[k] == 0.0f) { slot = k; break; }
						if (slot < 0)
						{
							// 가장 작은 가중치 교체
							int minIdx = 0; for (int k=1;k<4;++k) if (inf.weights[k] < inf.weights[minIdx]) minIdx = k;
							slot = minIdx;
						}
						inf.indices[slot] = (uint16_t)boneIndex;
						inf.weights[slot] = wv;
					}
				}
			}
            // Fallback: vertices without any weight follow the owning node (rigid)
            for (size_t vi = 0; vi < mesh->mNumVertices; ++vi)
            {
                VertexInfluence& inf = m_VertexInfluences[baseIndex + vi];
                float wsum = inf.weights[0] + inf.weights[1] + inf.weights[2] + inf.weights[3];
                if (wsum <= 0.0f)
                {
                    inf.indices[0] = (uint16_t)nodeBoneIndex;
                    inf.weights[0] = 1.0f;
                }
            }
		}
		for (unsigned ci = 0; ci < node->mNumChildren; ++ci) traverse(node->mChildren[ci], global);
	};
	traverse(scene->mRootNode, aiMatrix4x4());

    // 본→노드 매핑
    m_BoneToNode.assign(m_Bones.size(), -1);
    for (size_t i=0;i<m_Bones.size();++i) {
        auto it = m_NameToNode.find(m_Bones[i].name);
        if (it != m_NameToNode.end()) m_BoneToNode[i] = it->second;
    }
    // synthetic bone의 offset 보정: 바인드 글로벌의 역행렬로 설정
    auto isIdentity = [](const DirectX::XMMATRIX& M)->bool{
        using namespace DirectX;
        static const XMMATRIX I = XMMatrixIdentity();
        const float* a = (const float*)&M; const float* b = (const float*)&I;
        for (int i=0;i<16;++i){ if (fabsf(a[i] - b[i]) > 1e-6f) return false; }
        return true;
    };
    for (size_t bi=0; bi<m_Bones.size(); ++bi)
    {
        int ni = m_BoneToNode[bi];
        if (ni >= 0 && (size_t)ni < m_NodeBindGlobal.size())
        {
            if (isIdentity(m_Bones[bi].offset))
            {
                DirectX::XMMATRIX invBind = DirectX::XMMatrixInverse(nullptr, m_NodeBindGlobal[ni]);
                m_Bones[bi].offset = invBind;
            }
        }
    }
    m_LocalPose.assign(m_Nodes.size(), DirectX::XMMatrixIdentity());
    m_GlobalPose.assign(m_Nodes.size(), DirectX::XMMatrixIdentity());

	if (m_Vertices.empty() || m_Indices.empty()) return false;

    // VB (정점 데이터는 STATIC, 스키닝은 GPU에서 수행 예정)
	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = (UINT)(m_Vertices.size() * sizeof(VertexData));
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.CPUAccessFlags = 0;
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

    // Influence VB (indices: uint4, weights: float4) interleaved
    struct GPUInfluence { uint32_t i0,i1,i2,i3; float w0,w1,w2,w3; };
    std::vector<GPUInfluence> infl; infl.resize(m_VertexInfluences.size());
    for (size_t i=0;i<m_VertexInfluences.size();++i){
        const auto& v = m_VertexInfluences[i];
        infl[i] = { v.indices[0], v.indices[1], v.indices[2], v.indices[3], v.weights[0], v.weights[1], v.weights[2], v.weights[3] };
    }
    if (!infl.empty()){
        D3D11_BUFFER_DESC idesc{}; idesc.ByteWidth = (UINT)(infl.size()*sizeof(GPUInfluence));
        idesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; idesc.Usage = D3D11_USAGE_DEFAULT;
        D3D11_SUBRESOURCE_DATA idata{}; idata.pSysMem = infl.data();
        HR_T(device->CreateBuffer(&idesc, &idata, &m_pInfluenceVB));
        m_InfluenceStride = sizeof(GPUInfluence);
    }

	return true;
}

bool PmxManager::LoadVmd(const std::wstring& vmdPath)
{
    // 고속/안전 파싱: 파일 전체를 메모리로 읽어 경계 검사를 하며 진행
    m_HasVmd = false;
    m_VmdInfo = {};
    m_VmdCenterKeys.clear();
    m_VmdPath.clear();
    try {
        std::ifstream fs(vmdPath, std::ios::binary | std::ios::ate);
        if (!fs) { m_LastError = L"VMD 파일 열기 실패"; return false; }
        std::streamsize fsize = fs.tellg();
        if (fsize <= 0) { m_LastError = L"VMD 파일 크기 오류"; return false; }
        fs.seekg(0, std::ios::beg);
        std::vector<uint8_t> buf((size_t)fsize);
        if (!fs.read(reinterpret_cast<char*>(buf.data()), fsize)) { m_LastError = L"VMD 파일 읽기 실패"; return false; }

        auto rd32 = [&](const uint8_t* p) -> uint32_t { uint32_t v; memcpy(&v, p, 4); return v; };
        size_t off = 0, end = buf.size();
        auto need = [&](size_t n){ return off + n <= end; };
        // 헤더 30바이트
        if (!need(30)) { m_LastError = L"VMD 헤더 손상"; return false; }
        off += 30;
        // 모델명 20바이트(Shift-JIS)
        if (!need(20)) { m_LastError = L"VMD 모델명 손상"; return false; }
        off += 20;
        // 본 키 개수
        if (!need(4)) { m_LastError = L"VMD 본카운트 손상"; return false; }
        uint32_t boneCount = rd32(&buf[off]); off += 4;

        // 본 키 블록 크기(한 항목 111바이트) 검증 및 클램프
        const size_t BONE_KEY_SIZE = 15 + 4 + 12 + 16 + 64; // 111
        size_t remain = end - off;
        size_t maxKeysBySize = remain / BONE_KEY_SIZE;
        if (boneCount > maxKeysBySize) {
            // 파일 내 실제 가능한 개수로 클램프 (손상/잘못된 파일 방지)
            boneCount = (uint32_t)maxKeysBySize;
        }
        m_VmdCenterKeys.reserve(std::min<uint32_t>(boneCount, 200000));
        m_VmdBoneTracks.clear(); m_VmdBoneTracks.reserve(std::min<uint32_t>(boneCount, 5000));

        uint32_t maxFrame = 0;
        for (uint32_t i = 0; i < boneCount; ++i)
        {
            if (!need(BONE_KEY_SIZE)) break;
            const uint8_t* p = &buf[off];
            const char* name = reinterpret_cast<const char*>(p);           // 15
            uint32_t frameNo = rd32(p + 15);                               // 4
            const float* pos = reinterpret_cast<const float*>(p + 19);     // 12
            const float* rot = reinterpret_cast<const float*>(p + 31);     // 16
            // interp[64] at p+47 (skip)
            if (frameNo > maxFrame) maxFrame = frameNo;
            // 본 트랙 적재 (Shift-JIS→가벼운 ASCII 대체)
            std::string uname = Utf8FromShiftJis15(name);
            m_VmdBoneTracks[uname].push_back({ frameNo, {pos[0],pos[1],pos[2]}, {rot[0],rot[1],rot[2],rot[3]} });
            // Center 루트 모션도 유지
            if (uname.find("Center") != std::string::npos) m_VmdCenterKeys.push_back({ frameNo, {pos[0],pos[1],pos[2]}, {rot[0],rot[1],rot[2],rot[3]} });
            off += BONE_KEY_SIZE;
        }

        // 모프 카운트 (있다면)
        uint32_t morphCount = 0;
        if (need(4)) { morphCount = rd32(&buf[off]); off += 4; }
        // 모프 프레임(name[15], frame[4], weight[4]) 23 bytes
        const size_t MORPH_KEY_SIZE = 15 + 4 + 4;
        size_t maxMorphBySize = (end - off) / MORPH_KEY_SIZE;
        if (morphCount > maxMorphBySize) morphCount = (uint32_t)maxMorphBySize;
        for (uint32_t i = 0; i < morphCount; ++i)
        {
            if (!need(MORPH_KEY_SIZE)) break;
            const uint8_t* p = &buf[off];
            uint32_t fr = rd32(p + 15);
            if (fr > maxFrame) maxFrame = fr;
            off += MORPH_KEY_SIZE;
        }

        m_VmdInfo.numBoneKeys = boneCount;
        m_VmdInfo.numMorphKeys = morphCount;
        m_VmdInfo.durationSec = (float)maxFrame / 30.0f;
        m_HasVmd = true;
        m_VmdPath = vmdPath;
        // 자동 재생: 처음부터 곧바로 확인할 수 있도록 시작
        m_TimeSec = 0.0f; 
        m_Playing = (m_VmdInfo.durationSec > 0.0f);

        // 각 본 트랙 프레임 정렬
        for (auto& kv : m_VmdBoneTracks) {
            auto& vec = kv.second;
            std::sort(vec.begin(), vec.end(), [](const VmdBoneKey& a, const VmdBoneKey& b){ return a.frame < b.frame; });
        }
        // 트랙 바인딩 테이블 구성 (이름 매칭 O(1)로 줄여 Tick 코스트\ 절감)
        m_BoundTracks.clear(); m_BoundTracks.reserve(m_VmdBoneTracks.size());
        for (auto& kv : m_VmdBoneTracks) {
            auto it = m_NameToNode.find(kv.first);
            if (it == m_NameToNode.end()) continue;
            m_BoundTracks.push_back({ it->second, &kv.second });
        }
        return true;
    } catch (...) {
        m_LastError = L"VMD 파싱 중 예외";
        return false;
    }
}

 

void PmxManager::Tick(float dtSec)
{
	if (!m_Playing || !m_HasVmd) return;
	float adv = dtSec * m_Speed;
	m_TimeSec += adv;
	if (m_TimeSec > m_VmdInfo.durationSec) {
		if (m_Loop && m_VmdInfo.durationSec > 0.0f) {
			m_TimeSec = std::fmod(m_TimeSec, m_VmdInfo.durationSec);
		} else {
			m_TimeSec = m_VmdInfo.durationSec;
			m_Playing = false;
		}
	}

    // 본 포즈 평가: 바운드된 트랙만 적용 → 글로벌 계산 → 본 행렬 생성 (할당 최소화)
    using namespace DirectX;
    if (m_CurrentBoneMatrices.size() < m_Bones.size()) m_CurrentBoneMatrices.resize(m_Bones.size(), XMMatrixIdentity());
    if (m_Nodes.empty()) return;

    // 1) 로컬 배열 준비 (기본 로컬 = 바인드포즈 노드 로컬)
    if (m_LocalPose.size() != m_Nodes.size()) m_LocalPose.assign(m_Nodes.size(), XMMatrixIdentity());
    for (size_t i=0;i<m_Nodes.size();++i) m_LocalPose[i] = m_Nodes[i].local;

    // 2) 시간 → 프레임 (클램프)
    float frameF = m_TimeSec * 30.0f;
    if (frameF < 0.0f) frameF = 0.0f;

    // 3) VMD 트랙 적용 (사전 바인딩된 트랙만)
    for (const auto& bind : m_BoundTracks)
    {
        int ni = bind.nodeIndex; if (ni < 0 || (size_t)ni >= m_LocalPose.size()) continue;
        const auto& keys = *bind.keys; if (keys.empty()) continue;
        // 이분 탐색
        size_t lo=0, hi=keys.size();
        while (lo + 1 < hi) { size_t mid=(lo+hi)/2; if (keys[mid].frame <= (uint32_t)frameF) lo=mid; else hi=mid; }
        const auto& a = keys[lo];
            if (lo + 1 >= keys.size())
        {
            XMVECTOR q = XMQuaternionNormalize(XMLoadFloat4(&a.rot));
            XMVECTOR t = XMLoadFloat3(&a.pos);
            // 바인드포즈 로컬 위에 덮어쓰기: T * R * BindLocalScale
            m_LocalPose[ni] = XMMatrixTranslationFromVector(t) * XMMatrixRotationQuaternion(q);
        }
        else
        {
            const auto& b = keys[lo+1];
            float span = float(int(b.frame) - int(a.frame));
            float tnorm = span>0.0f ? (frameF - (float)a.frame)/span : 0.0f;
            XMVECTOR pa = XMLoadFloat3(&a.pos), pb = XMLoadFloat3(&b.pos);
            XMVECTOR qa = XMQuaternionNormalize(XMLoadFloat4(&a.rot));
            XMVECTOR qb = XMQuaternionNormalize(XMLoadFloat4(&b.rot));
            XMVECTOR p = XMVectorLerp(pa, pb, tnorm);
            XMVECTOR q = XMQuaternionSlerp(qa, qb, tnorm);
            m_LocalPose[ni] = XMMatrixTranslationFromVector(p) * XMMatrixRotationQuaternion(q);
        }
    }

    // 4) 글로벌 변환 계산 (캐시 사용)
    if (m_GlobalPose.size() != m_Nodes.size()) m_GlobalPose.assign(m_Nodes.size(), XMMatrixIdentity());
    for (size_t i=0;i<m_Nodes.size();++i)
    {
        int parent = m_Nodes[i].parent;
        m_GlobalPose[i] = (parent>=0) ? (m_LocalPose[i] * m_GlobalPose[parent]) : m_LocalPose[i];
    }

    // 5) 본 행렬 = 글로벌(노드) * 오프셋 (노드에 해당 본이 없으면 바인드 글로벌 사용)
    for (size_t bi=0; bi<m_Bones.size(); ++bi)
    {
        int ni = (bi < m_BoneToNode.size()) ? m_BoneToNode[bi] : -1;
        XMMATRIX g = XMMatrixIdentity();
        if (ni>=0 && (size_t)ni < m_GlobalPose.size()) g = m_GlobalPose[ni];
        else if (ni>=0 && (size_t)ni < m_NodeBindGlobal.size()) g = m_NodeBindGlobal[ni];
        // Assimp offsetMatrix는 보통 바인드포즈 역행렬(inverse bind)로, 최종 = g * offset
        m_CurrentBoneMatrices[bi] = g * m_Bones[bi].offset;
    }
}

DirectX::XMMATRIX PmxManager::EvaluateRootMotion() const
{
	using namespace DirectX;
	if (m_VmdCenterKeys.empty() || m_VmdInfo.durationSec <= 0.0f) return XMMatrixIdentity();
	float frameF = m_TimeSec * 30.0f;
	// 이분 탐색
	size_t lo = 0, hi = m_VmdCenterKeys.size();
	while (lo + 1 < hi) {
		size_t mid = (lo + hi) / 2;
		if (m_VmdCenterKeys[mid].frame <= (uint32_t)frameF) lo = mid; else hi = mid;
	}
	const VmdCenterKey& a = m_VmdCenterKeys[lo];
	if (lo + 1 >= m_VmdCenterKeys.size())
	{
		XMVECTOR q = XMLoadFloat4(&a.rot);
		XMVECTOR p = XMLoadFloat3(&a.pos);
		return XMMatrixRotationQuaternion(q) * XMMatrixTranslationFromVector(p);
	}
	const VmdCenterKey& b = m_VmdCenterKeys[lo + 1];
	float t = 0.0f;
	float span = (float)((int)b.frame - (int)a.frame);
	if (span > 0.0f) t = (frameF - (float)a.frame) / span;
	XMVECTOR pa = XMLoadFloat3(&a.pos), pb = XMLoadFloat3(&b.pos);
	XMVECTOR qa = XMLoadFloat4(&a.rot), qb = XMLoadFloat4(&b.rot);
	XMVECTOR p = XMVectorLerp(pa, pb, t);
	XMVECTOR q = XMQuaternionSlerp(qa, qb, t);
	return XMMatrixRotationQuaternion(q) * XMMatrixTranslationFromVector(p);
}

int PmxManager::FillBoneMatrices64(DirectX::XMFLOAT4X4 outBones[64]) const
{
    using namespace DirectX;
    int count = (int)std::min<size_t>(m_CurrentBoneMatrices.size(), 64);
    for (int i=0;i<count;++i){ XMStoreFloat4x4(&outBones[i], XMMatrixTranspose(m_CurrentBoneMatrices[i])); }
    for (int i=count;i<64;++i){ outBones[i] = XMFLOAT4X4(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1); }
    return count;
}

int PmxManager::FillBoneMatrices256(DirectX::XMFLOAT4X4 outBones[256]) const
{
	using namespace DirectX;
	int count = (int)std::min<size_t>(m_CurrentBoneMatrices.size(), 256);
	for (int i = 0; i < count; ++i) { XMStoreFloat4x4(&outBones[i], XMMatrixTranspose(m_CurrentBoneMatrices[i])); }
	for (int i = count; i < 256; ++i) { outBones[i] = XMFLOAT4X4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1); }
	return count;
}

void PmxManager::UpdateSkinning(ID3D11DeviceContext* context)
{
    // GPU 스키닝 전환: CPU 스키닝 비활성 (유지용 함수)
    (void)context;
}
