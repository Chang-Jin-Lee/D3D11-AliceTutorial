#include "pch.h"
#include "FbxAnimation.h"
#include "../Helper.h"

#include <assimp/scene.h>
#include <d3d11.h>
#include <wrl/client.h>

using namespace DirectX;

// 안전한 행렬 보간 헬퍼 (XMMatrixLerp가 없는 환경을 위해 직접 구현)
static XMMATRIX LerpMatrix(const XMMATRIX& A, const XMMATRIX& B, float t)
{
	return A + (B - A) * t;
}

FbxAnimation::FbxAnimation() {}
FbxAnimation::~FbxAnimation() { Clear(); }

void FbxAnimation::Clear()
{
	SAFE_RELEASE(m_pBoneCB);
	m_pBoneCB = nullptr;
	m_Clips.clear();
	m_Names.clear(); m_DurationSec.clear(); m_TicksPerSec.clear();
	m_Current = -1; m_TimeSec = 0.0; m_Playing = false; m_Type = AnimType::None;
    m_Scene = nullptr; m_NodeIndexOfName.clear(); m_BoneNames = nullptr; m_BoneOffsets = nullptr; m_GlobalInverse = nullptr;
    m_ChannelOfNode.clear(); m_ChannelDirty = true; m_GlobalScratch.clear(); m_PaletteScratch.clear();
    // Optimized traversal caches
    m_NodePtrByIndex.clear();
    m_ParentIndexByIndex.clear();
    m_BoneNodeIndices.clear();
    m_Precomputed.clear();
}

void FbxAnimation::InitMetadata(const aiScene* scene)
{
	m_Clips.clear();
	m_Names.clear(); m_DurationSec.clear(); m_TicksPerSec.clear();
	if (!scene) return;
	if (scene->mNumAnimations == 0) return;
	m_Clips.reserve(scene->mNumAnimations);
	for (unsigned i = 0; i < scene->mNumAnimations; ++i)
	{
		if (scene->mAnimations[i]) m_Clips.push_back(scene->mAnimations[i]);
	}
	m_Names.reserve(m_Clips.size());
	m_DurationSec.reserve(m_Clips.size());
	m_TicksPerSec.reserve(m_Clips.size());
	for (size_t i = 0; i < m_Clips.size(); ++i)
	{
		const aiAnimation* a = m_Clips[i];
		std::string nm = a->mName.length > 0 ? std::string(a->mName.C_Str()) : (std::string("Anim") + std::to_string(i));
		double tps = (a->mTicksPerSecond != 0.0) ? a->mTicksPerSecond : 25.0;
		double durSec = (tps != 0.0) ? (a->mDuration / tps) : 0.0;
		m_Names.push_back(nm);
		m_TicksPerSec.push_back(tps);
		m_DurationSec.push_back(durSec);
	}
	m_Current = 0; m_TimeSec = 0.0; m_Playing = false;
}

void FbxAnimation::SetExternalClip(const aiAnimation* clip, const std::string& name)
{
    m_Clips.clear();
    m_Names.clear();
    m_DurationSec.clear();
    m_TicksPerSec.clear();
    m_Precomputed.clear();
    m_Current = -1;
    m_TimeSec = 0.0;
    m_ChannelDirty = true;

    if (!clip) return;

    const double ticksPerSec = clip->mTicksPerSecond != 0.0 ? clip->mTicksPerSecond : 25.0;
    m_Clips.push_back(clip);
    m_Names.push_back(name.empty() ? "External" : name);
    m_TicksPerSec.push_back(ticksPerSec);
    m_DurationSec.push_back(ticksPerSec != 0.0 ? clip->mDuration / ticksPerSec : 0.0);
    m_Current = 0;

    if (m_Scene && m_BoneNames && m_BoneOffsets && m_GlobalInverse)
    {
        PrecomputeAll(m_Scene, m_NodeIndexOfName, *m_BoneNames, *m_BoneOffsets, *m_GlobalInverse, 30);
    }
}

void FbxAnimation::SetCurrentIndex(int idx)
{
	if (idx < 0 || idx >= (int)m_Names.size()) return;
	m_Current = idx; m_TimeSec = 0.0;
    m_ChannelDirty = true;
}
void FbxAnimation::SetSharedContext(
    const aiScene* scene,
    const std::unordered_map<std::string,int>& nodeIndexOfName,
    const std::vector<std::string>* boneNames,
    const std::vector<DirectX::XMFLOAT4X4>* boneOffsets,
    const DirectX::XMFLOAT4X4* globalInverse)
{
    m_Scene = scene;
    m_NodeIndexOfName = nodeIndexOfName;
    m_BoneNames = boneNames;
    m_BoneOffsets = boneOffsets;
    m_GlobalInverse = globalInverse;
    m_ChannelOfNode.assign(m_NodeIndexOfName.size(), nullptr);
    m_ChannelDirty = true;

    // Build fast traversal caches aligned with node indices
    m_NodePtrByIndex.clear();
    m_ParentIndexByIndex.clear();
    m_BoneNodeIndices.clear();
    if (m_Scene && !m_NodeIndexOfName.empty())
    {
        m_NodePtrByIndex.resize(m_NodeIndexOfName.size(), nullptr);
        m_ParentIndexByIndex.resize(m_NodeIndexOfName.size(), -1);

        std::function<void(const aiNode*, int)> build = [&](const aiNode* node, int parentIdx)
        {
            auto it = m_NodeIndexOfName.find(node->mName.C_Str());
            int idx = (it != m_NodeIndexOfName.end()) ? it->second : -1;
            if (idx >= 0 && (size_t)idx < m_NodePtrByIndex.size())
            {
                m_NodePtrByIndex[(size_t)idx] = node;
                m_ParentIndexByIndex[(size_t)idx] = parentIdx;
            }
            for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
            {
                build(node->mChildren[ci], idx);
            }
        };
        if (m_Scene->mRootNode)
        {
            auto itRoot = m_NodeIndexOfName.find(m_Scene->mRootNode->mName.C_Str());
            int rootIdx = (itRoot != m_NodeIndexOfName.end()) ? itRoot->second : -1;
            build(m_Scene->mRootNode, -1);
            // Ensure root parent index is -1 if valid
            if (rootIdx >= 0 && (size_t)rootIdx < m_ParentIndexByIndex.size()) m_ParentIndexByIndex[(size_t)rootIdx] = -1;
        }

        // Map bone names to node indices (for optimized evaluation path)
        if (m_BoneNames && !m_BoneNames->empty())
        {
            m_BoneNodeIndices.reserve(m_BoneNames->size());
            for (const auto& bn : *m_BoneNames)
            {
                auto itB = m_NodeIndexOfName.find(bn);
                m_BoneNodeIndices.push_back((itB != m_NodeIndexOfName.end()) ? itB->second : -1);
            }
        }
    }

    // Precompute all clips to avoid per-frame evaluation
    // Default to 30 samples per second to balance memory/quality
    if (m_Scene && m_BoneNames && m_BoneOffsets && m_GlobalInverse)
    {
        PrecomputeAll(m_Scene, m_NodeIndexOfName, *m_BoneNames, *m_BoneOffsets, *m_GlobalInverse, 30);
    }
}

static void RebuildChannelMapIfNeeded(
    const aiAnimation* animation,
    const std::unordered_map<std::string, int>& nodeIndexOfName,
    std::vector<const aiNodeAnim*>& out)
{
    std::fill(out.begin(), out.end(), nullptr);
    if (!animation) return;
    for (unsigned i = 0; i < animation->mNumChannels; ++i)
    {
        const aiNodeAnim* ch = animation->mChannels[i];
        auto it = nodeIndexOfName.find(ch->mNodeName.C_Str());
        if (it != nodeIndexOfName.end())
        {
            int idx = it->second;
            if (idx >= 0 && (size_t)idx < out.size()) out[(size_t)idx] = ch;
        }
    }
}


void FbxAnimation::SetTimeSec(double t)
{
	if (m_Current < 0 || m_Current >= (int)m_Names.size()) { m_TimeSec = 0.0; return; }
	double dur = m_DurationSec[m_Current];
	if (dur <= 0.0) { m_TimeSec = 0.0; return; }
	while (t < 0.0) t += dur; while (t >= dur) t -= dur; m_TimeSec = t;
}

double FbxAnimation::GetClipDurationSec(int idx) const
{
	if (idx < 0 || idx >= (int)m_DurationSec.size()) return 0.0; return m_DurationSec[idx];
}

void FbxAnimation::EnsureBoneCB(ID3D11Device* device, int maxBones)
{
	if (m_pBoneCB || !device) return;
	D3D11_BUFFER_DESC bd{}; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; bd.Usage = D3D11_USAGE_DYNAMIC; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bd.ByteWidth = sizeof(DirectX::XMFLOAT4X4) * (UINT)maxBones + sizeof(unsigned int) + sizeof(float) * 3;
	HR_T(device->CreateBuffer(&bd, nullptr, &m_pBoneCB));
}

// 바이너리 서치로 키 찾기 (O(logN))
static aiVector3D InterpVec(const aiVectorKey* keys, unsigned count, double t)
{
	if (count == 0) return aiVector3D(0, 0, 0);
	if (count == 1) return keys[0].mValue;
	
	// upper_bound로 바이너리 서치
	const aiVectorKey* first = keys;
	const aiVectorKey* last = keys + count;
	const aiVectorKey* it = std::upper_bound(first, last, t, [](double val, const aiVectorKey& key) { return val < key.mTime; });
	
	unsigned i = 0, j = 0;
	if (it == first) { i = 0; j = 0; }
	else if (it == last) { i = count - 1; j = count - 1; }
	else { i = (unsigned)(it - keys - 1); j = (unsigned)(it - keys); }
	
	double dt = keys[j].mTime - keys[i].mTime;
	double a = (dt > 0.0) ? (t - keys[i].mTime) / dt : 0.0;
	aiVector3D v0 = keys[i].mValue, v1 = keys[j].mValue;
	return v0 + (float)a * (v1 - v0);
}

static aiQuaternion InterpQuat(const aiQuatKey* keys, unsigned count, double t)
{
	if (count == 0) return aiQuaternion();
	if (count == 1) return keys[0].mValue;
	
	// upper_bound로 바이너리 서치
	const aiQuatKey* first = keys;
	const aiQuatKey* last = keys + count;
	const aiQuatKey* it = std::upper_bound(first, last, t, [](double val, const aiQuatKey& key) { return val < key.mTime; });
	
	unsigned i = 0, j = 0;
	if (it == first) { i = 0; j = 0; }
	else if (it == last) { i = count - 1; j = count - 1; }
	else { i = (unsigned)(it - keys - 1); j = (unsigned)(it - keys); }
	
	double dt = keys[j].mTime - keys[i].mTime;
	double a = (dt > 0.0) ? (t - keys[i].mTime) / dt : 0.0;
	aiQuaternion q;
	aiQuaternion::Interpolate(q, keys[i].mValue, keys[j].mValue, (float)a);
	q.Normalize();
	return q;
}

void FbxAnimation::EvaluateGlobals(
    const aiScene* scene,
    const std::unordered_map<std::string,int>& nodeIndexOfName,
    std::vector<XMFLOAT4X4>& outGlobal) const
{
	if (!scene) return;
	
	const size_t N = nodeIndexOfName.size();
	if (outGlobal.size() != N) outGlobal.resize(N); // clear 하지 말고 resize만

	// done 스탬프 (스레드별 재사용)
	static thread_local std::vector<uint32_t> doneStamp;
	static thread_local uint32_t stamp = 1;
	if (doneStamp.size() != N) doneStamp.assign(N, 0);
	
	++stamp;
	if (stamp == 0) { // overflow 보호
		std::fill(doneStamp.begin(), doneStamp.end(), 0);
		stamp = 1;
	}

	// tTicks도 노드마다 계산하지 말고 1회만
	const double tTicks = m_TimeSec * ((m_Current >= 0 && (size_t)m_Current < m_TicksPerSec.size()) ? m_TicksPerSec[m_Current] : 25.0);

	// Optimized path: compute only nodes needed by bones using parent indices
	if (!m_ParentIndexByIndex.empty() && !m_BoneNodeIndices.empty() && m_NodePtrByIndex.size() == nodeIndexOfName.size())
	{
		auto computeNode = [&](auto&& self, int idx) -> void {
			if (idx < 0 || (size_t)idx >= m_NodePtrByIndex.size()) return;
			if (doneStamp[(size_t)idx] == stamp) return; // ? stamp로 방문 체크
			int pi = (idx < (int)m_ParentIndexByIndex.size()) ? m_ParentIndexByIndex[(size_t)idx] : -1;
			if (pi >= 0) self(self, pi);
			const aiNode* node = m_NodePtrByIndex[(size_t)idx];
			aiMatrix4x4 mLocal = node ? node->mTransformation : aiMatrix4x4();
			if ((size_t)idx < m_ChannelOfNode.size())
			{
				const aiNodeAnim* ch = m_ChannelOfNode[(size_t)idx];
				if (ch)
				{
					aiVector3D S = (ch->mNumScalingKeys   > 0) ? InterpVec(ch->mScalingKeys,   ch->mNumScalingKeys,   tTicks) : aiVector3D(1,1,1);
					aiVector3D T = (ch->mNumPositionKeys  > 0) ? InterpVec(ch->mPositionKeys,  ch->mNumPositionKeys,  tTicks) : aiVector3D(0,0,0);
					aiQuaternion R = (ch->mNumRotationKeys  > 0) ? InterpQuat(ch->mRotationKeys, ch->mNumRotationKeys,  tTicks) : aiQuaternion();
					aiMatrix4x4 mS; mS.Scaling(S, mS); aiMatrix4x4 mR = aiMatrix4x4(R.GetMatrix()); aiMatrix4x4 mT; mT.Translation(T, mT);
					mLocal = mT * mR * mS;
				}
			}
			XMFLOAT4X4 lm; lm._11 = (float)mLocal.a1; lm._12 = (float)mLocal.a2; lm._13 = (float)mLocal.a3; lm._14 = (float)mLocal.a4;
			lm._21 = (float)mLocal.b1; lm._22 = (float)mLocal.b2; lm._23 = (float)mLocal.b3; lm._24 = (float)mLocal.b4;
			lm._31 = (float)mLocal.c1; lm._32 = (float)mLocal.c2; lm._33 = (float)mLocal.c3; lm._34 = (float)mLocal.c4;
			lm._41 = (float)mLocal.d1; lm._42 = (float)mLocal.d2; lm._43 = (float)mLocal.d3; lm._44 = (float)mLocal.d4;
			XMMATRIX L = XMLoadFloat4x4(&lm);
			XMMATRIX parent = XMMatrixIdentity();
			if (pi >= 0) parent = XMLoadFloat4x4(&outGlobal[(size_t)pi]);
			XMMATRIX G = XMMatrixMultiply(parent, L);
			XMStoreFloat4x4(&outGlobal[(size_t)idx], G);
			doneStamp[(size_t)idx] = stamp; //stamp로 마킹
		};
		for (int bn : m_BoneNodeIndices) if (bn >= 0) computeNode(computeNode, bn);
		return;
	}
	std::function<void(const aiNode*, int, const XMMATRIX&)> eval = [&](const aiNode* node, int idx, const XMMATRIX& parent){
		aiVector3D S(1,1,1), T(0,0,0); aiQuaternion R; aiMatrix4x4 mLocal = node->mTransformation;
        auto itIndex = nodeIndexOfName.find(node->mName.C_Str());
        if (itIndex != nodeIndexOfName.end())
		{
            int nodeIdx = itIndex->second;
            if (nodeIdx >= 0 && (size_t)nodeIdx < m_ChannelOfNode.size())
            {
                const aiNodeAnim* ch = m_ChannelOfNode[(size_t)nodeIdx];
                if (ch)
                {
                    S = (ch->mNumScalingKeys   > 0) ? InterpVec(ch->mScalingKeys,   ch->mNumScalingKeys,   tTicks) : aiVector3D(1,1,1);
                    T = (ch->mNumPositionKeys  > 0) ? InterpVec(ch->mPositionKeys,  ch->mNumPositionKeys,  tTicks) : aiVector3D(0,0,0);
                    R = (ch->mNumRotationKeys  > 0) ? InterpQuat(ch->mRotationKeys, ch->mNumRotationKeys,  tTicks) : aiQuaternion();
                    aiMatrix4x4 mS; mS.Scaling(S, mS); aiMatrix4x4 mR = aiMatrix4x4(R.GetMatrix()); aiMatrix4x4 mT; mT.Translation(T, mT);
                    mLocal = mT * mR * mS;
                }
            }
		}
		XMFLOAT4X4 lm; lm._11 = (float)mLocal.a1; lm._12 = (float)mLocal.a2; lm._13 = (float)mLocal.a3; lm._14 = (float)mLocal.a4;
		lm._21 = (float)mLocal.b1; lm._22 = (float)mLocal.b2; lm._23 = (float)mLocal.b3; lm._24 = (float)mLocal.b4;
		lm._31 = (float)mLocal.c1; lm._32 = (float)mLocal.c2; lm._33 = (float)mLocal.c3; lm._34 = (float)mLocal.c4;
		lm._41 = (float)mLocal.d1; lm._42 = (float)mLocal.d2; lm._43 = (float)mLocal.d3; lm._44 = (float)mLocal.d4;
		XMMATRIX L = XMLoadFloat4x4(&lm);
		XMMATRIX G = XMMatrixMultiply(parent, L);
		if ((size_t)idx < outGlobal.size()) XMStoreFloat4x4(&outGlobal[(size_t)idx], G);
		for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
		{
			auto it = nodeIndexOfName.find(node->mChildren[ci]->mName.C_Str());
			int childIdx = (it != nodeIndexOfName.end()) ? it->second : -1;
			if (childIdx >= 0) eval(node->mChildren[ci], childIdx, G);
		}
	};
	int rootIdx = -1; // find root by nodeIndexOfName of root node
	if (scene->mRootNode) { auto it = nodeIndexOfName.find(scene->mRootNode->mName.C_Str()); if (it != nodeIndexOfName.end()) rootIdx = it->second; }
	if (rootIdx >= 0) eval(scene->mRootNode, rootIdx, XMMatrixIdentity());
}

void FbxAnimation::PrecomputeAll(
	const aiScene* scene,
	const std::unordered_map<std::string,int>& nodeIndexOfName,
	const std::vector<std::string>& boneNames,
	const std::vector<XMFLOAT4X4>& boneOffsets,
	const XMFLOAT4X4& globalInverse,
	int samplesPerSecond)
{
	if (!scene || boneNames.empty() || samplesPerSecond <= 0) { m_Precomputed.clear(); return; }
	m_Precomputed.clear();
	m_Precomputed.resize(m_Clips.size());

	// Preserve current playback state while precomputing
	int oldClip = m_Current; double oldTime = m_TimeSec; bool oldPlaying = m_Playing;
	m_Playing = false;

	for (size_t clipIdx = 0; clipIdx < m_Clips.size(); ++clipIdx)
	{
		PrecomputedClip pc{};
		pc.ticksPerSec = (clipIdx < m_TicksPerSec.size()) ? m_TicksPerSec[clipIdx] : 25.0;
		pc.durationSec = (clipIdx < m_DurationSec.size()) ? m_DurationSec[clipIdx] : 0.0;
		pc.sampleDt = (samplesPerSecond > 0) ? (1.0 / (double)samplesPerSecond) : 0.0;
		pc.rigid = (m_Type == AnimType::Rigid);
		if (pc.durationSec <= 0.0 || pc.sampleDt <= 0.0) { m_Precomputed[clipIdx] = pc; continue; }

		// Build channel map for this clip
		m_Current = (int)clipIdx;
		m_ChannelOfNode.assign(nodeIndexOfName.size(), nullptr);
		RebuildChannelMapIfNeeded(m_Clips[clipIdx], nodeIndexOfName, m_ChannelOfNode);

		int numSamples = (int)std::ceil(pc.durationSec * samplesPerSecond);
		if (numSamples < 1) numSamples = 1;
		pc.times.resize((size_t)numSamples);
		pc.palettes.resize((size_t)numSamples);
		XMMATRIX Gi = XMLoadFloat4x4(&globalInverse);

		//  1) global 벡터를 샘플 루프 밖으로 빼서 재사용
		std::vector<XMFLOAT4X4> global;
		global.resize(nodeIndexOfName.size()); // 한번만 resize

		//  2) boneName -> nodeIdx 해시탐색을 샘플 루프 밖에서 1회만
		const size_t boneCount = boneNames.size();
		std::vector<int> boneNodeIdx(boneCount, -1);
		for (size_t bi = 0; bi < boneCount; ++bi)
		{
			auto it = nodeIndexOfName.find(boneNames[bi]);
			boneNodeIdx[bi] = (it != nodeIndexOfName.end()) ? it->second : -1;
		}

		//  offsets도 XMMATRIX로 1회 로드(샘플마다 XMLoad 하지 않게)
		std::vector<XMMATRIX> offMats;
		offMats.reserve(boneCount);
		for (size_t bi = 0; bi < boneCount; ++bi)
			offMats.push_back(XMLoadFloat4x4(&boneOffsets[bi]));

		for (int si = 0; si < numSamples; ++si)
		{
			double tSec = si * pc.sampleDt; if (tSec > pc.durationSec) tSec = pc.durationSec;
			pc.times[(size_t)si] = tSec;
			// Evaluate globals at tSec (using internal EvaluateGlobals which reads m_TimeSec/m_Current/m_ChannelOfNode)
			m_TimeSec = tSec;
			EvaluateGlobals(scene, nodeIndexOfName, global); // ? 재사용된 global 벡터 사용
			
			// ? identity로 채우지 말고 그냥 resize
			auto& pal = pc.palettes[(size_t)si];
			pal.resize(boneCount);
			
			for (size_t bi = 0; bi < boneCount; ++bi)
			{
				int nodeIdx = boneNodeIdx[bi];
				if (nodeIdx < 0 || nodeIdx >= (int)global.size())
				{
					pal[bi] = XMMatrixIdentity();
					continue;
				}
				
				XMMATRIX G = XMLoadFloat4x4(&global[(size_t)nodeIdx]);
				if (pc.rigid)
				{
					pal[bi] = XMMatrixMultiply(Gi, G);
				}
				else
				{
					pal[bi] = XMMatrixMultiply(XMMatrixMultiply(Gi, G), offMats[bi]); // ? 미리 로드된 offMats 사용
				}
			}
		}
		pc.valid = true;
		m_Precomputed[clipIdx] = std::move(pc);
	}

	// Restore playback state
	m_Current = oldClip; m_TimeSec = oldTime; m_Playing = oldPlaying;
}


// 원래 비 효율적으로 모든 노드를 선형 탐색하는 로직인데, 위 방법이 오류가 생긴다면 이걸로 돌릴것
// static aiVector3D InterpVec(const aiVectorKey* keys, unsigned count, double t)
// {
// 	if (count == 0) return aiVector3D(0, 0, 0);
// 	if (count == 1) return keys[0].mValue;
// 	unsigned i = 0; while (i + 1 < count && t >= keys[i + 1].mTime) ++i; unsigned j = (i + 1 < count) ? i + 1 : i;
// 	double dt = keys[j].mTime - keys[i].mTime; double a = (dt > 0.0) ? (t - keys[i].mTime) / dt : 0.0;
// 	aiVector3D v0 = keys[i].mValue, v1 = keys[j].mValue; return v0 + (float)a * (v1 - v0);
// }

// static aiQuaternion InterpQuat(const aiQuatKey* keys, unsigned count, double t)
// {
// 	if (count == 0) return aiQuaternion();
// 	if (count == 1) return keys[0].mValue;
// 	unsigned i = 0; while (i + 1 < count && t >= keys[i + 1].mTime) ++i; unsigned j = (i + 1 < count) ? i + 1 : i;
// 	double dt = keys[j].mTime - keys[i].mTime; double a = (dt > 0.0) ? (t - keys[i].mTime) / dt : 0.0;
// 	aiQuaternion q; aiQuaternion::Interpolate(q, keys[i].mValue, keys[j].mValue, (float)a); q.Normalize(); return q;
// }

// void FbxAnimation::EvaluateGlobals(
//     const aiScene* scene,
//     const std::unordered_map<std::string,int>& nodeIndexOfName,
//     std::vector<XMFLOAT4X4>& outGlobal) const
// {
// 	outGlobal.clear(); if (!scene) return;
// 	outGlobal.resize(nodeIndexOfName.size());

// 	// Optimized path: compute only nodes needed by bones using parent indices
// 	if (!m_ParentIndexByIndex.empty() && !m_BoneNodeIndices.empty() && m_NodePtrByIndex.size() == nodeIndexOfName.size())
// 	{
// 		std::vector<uint8_t> done; done.assign(nodeIndexOfName.size(), 0);
// 		auto computeNode = [&](auto&& self, int idx) -> void {
// 			if (idx < 0 || (size_t)idx >= m_NodePtrByIndex.size()) return;
// 			if (done[(size_t)idx]) return;
// 			int pi = (idx < (int)m_ParentIndexByIndex.size()) ? m_ParentIndexByIndex[(size_t)idx] : -1;
// 			if (pi >= 0) self(self, pi);
// 			const aiNode* node = m_NodePtrByIndex[(size_t)idx];
// 			aiMatrix4x4 mLocal = node ? node->mTransformation : aiMatrix4x4();
// 			if ((size_t)idx < m_ChannelOfNode.size())
// 			{
// 				const aiNodeAnim* ch = m_ChannelOfNode[(size_t)idx];
// 				if (ch)
// 				{
// 					double tTicks = m_TimeSec * ((m_Current >= 0 && (size_t)m_Current < m_TicksPerSec.size()) ? m_TicksPerSec[m_Current] : 25.0);
// 					aiVector3D S = (ch->mNumScalingKeys   > 0) ? InterpVec(ch->mScalingKeys,   ch->mNumScalingKeys,   tTicks) : aiVector3D(1,1,1);
// 					aiVector3D T = (ch->mNumPositionKeys  > 0) ? InterpVec(ch->mPositionKeys,  ch->mNumPositionKeys,  tTicks) : aiVector3D(0,0,0);
// 					aiQuaternion R = (ch->mNumRotationKeys  > 0) ? InterpQuat(ch->mRotationKeys, ch->mNumRotationKeys,  tTicks) : aiQuaternion();
// 					aiMatrix4x4 mS; mS.Scaling(S, mS); aiMatrix4x4 mR = aiMatrix4x4(R.GetMatrix()); aiMatrix4x4 mT; mT.Translation(T, mT);
// 					mLocal = mT * mR * mS;
// 				}
// 			}
// 			XMFLOAT4X4 lm; lm._11 = (float)mLocal.a1; lm._12 = (float)mLocal.a2; lm._13 = (float)mLocal.a3; lm._14 = (float)mLocal.a4;
// 			lm._21 = (float)mLocal.b1; lm._22 = (float)mLocal.b2; lm._23 = (float)mLocal.b3; lm._24 = (float)mLocal.b4;
// 			lm._31 = (float)mLocal.c1; lm._32 = (float)mLocal.c2; lm._33 = (float)mLocal.c3; lm._34 = (float)mLocal.c4;
// 			lm._41 = (float)mLocal.d1; lm._42 = (float)mLocal.d2; lm._43 = (float)mLocal.d3; lm._44 = (float)mLocal.d4;
// 			XMMATRIX L = XMLoadFloat4x4(&lm);
// 			XMMATRIX parent = XMMatrixIdentity();
// 			if (pi >= 0) parent = XMLoadFloat4x4(&outGlobal[(size_t)pi]);
// 			XMMATRIX G = XMMatrixMultiply(parent, L);
// 			XMStoreFloat4x4(&outGlobal[(size_t)idx], G);
// 			done[(size_t)idx] = 1;
// 		};
// 		for (int bn : m_BoneNodeIndices) if (bn >= 0) computeNode(computeNode, bn);
// 		return;
// 	}
// 	std::function<void(const aiNode*, int, const XMMATRIX&)> eval = [&](const aiNode* node, int idx, const XMMATRIX& parent){
// 		aiVector3D S(1,1,1), T(0,0,0); aiQuaternion R; aiMatrix4x4 mLocal = node->mTransformation;
//         auto itIndex = nodeIndexOfName.find(node->mName.C_Str());
//         if (itIndex != nodeIndexOfName.end())
// 		{
//             int nodeIdx = itIndex->second;
//             if (nodeIdx >= 0 && (size_t)nodeIdx < m_ChannelOfNode.size())
//             {
//                 const aiNodeAnim* ch = m_ChannelOfNode[(size_t)nodeIdx];
//                 if (ch)
//                 {
//                     double tTicks = m_TimeSec * ((m_Current >= 0 && (size_t)m_Current < m_TicksPerSec.size()) ? m_TicksPerSec[m_Current] : 25.0);
//                     S = (ch->mNumScalingKeys   > 0) ? InterpVec(ch->mScalingKeys,   ch->mNumScalingKeys,   tTicks) : aiVector3D(1,1,1);
//                     T = (ch->mNumPositionKeys  > 0) ? InterpVec(ch->mPositionKeys,  ch->mNumPositionKeys,  tTicks) : aiVector3D(0,0,0);
//                     R = (ch->mNumRotationKeys  > 0) ? InterpQuat(ch->mRotationKeys, ch->mNumRotationKeys,  tTicks) : aiQuaternion();
//                     aiMatrix4x4 mS; mS.Scaling(S, mS); aiMatrix4x4 mR = aiMatrix4x4(R.GetMatrix()); aiMatrix4x4 mT; mT.Translation(T, mT);
//                     mLocal = mT * mR * mS;
//                 }
//             }
// 		}
// 		XMFLOAT4X4 lm; lm._11 = (float)mLocal.a1; lm._12 = (float)mLocal.a2; lm._13 = (float)mLocal.a3; lm._14 = (float)mLocal.a4;
// 		lm._21 = (float)mLocal.b1; lm._22 = (float)mLocal.b2; lm._23 = (float)mLocal.b3; lm._24 = (float)mLocal.b4;
// 		lm._31 = (float)mLocal.c1; lm._32 = (float)mLocal.c2; lm._33 = (float)mLocal.c3; lm._34 = (float)mLocal.c4;
// 		lm._41 = (float)mLocal.d1; lm._42 = (float)mLocal.d2; lm._43 = (float)mLocal.d3; lm._44 = (float)mLocal.d4;
// 		XMMATRIX L = XMLoadFloat4x4(&lm);
// 		XMMATRIX G = XMMatrixMultiply(parent, L);
// 		if ((size_t)idx < outGlobal.size()) XMStoreFloat4x4(&outGlobal[(size_t)idx], G);
// 		for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
// 		{
// 			auto it = nodeIndexOfName.find(node->mChildren[ci]->mName.C_Str());
// 			int childIdx = (it != nodeIndexOfName.end()) ? it->second : -1;
// 			if (childIdx >= 0) eval(node->mChildren[ci], childIdx, G);
// 		}
// 	};
// 	int rootIdx = -1; // find root by nodeIndexOfName of root node
// 	if (scene->mRootNode) { auto it = nodeIndexOfName.find(scene->mRootNode->mName.C_Str()); if (it != nodeIndexOfName.end()) rootIdx = it->second; }
// 	if (rootIdx >= 0) eval(scene->mRootNode, rootIdx, XMMatrixIdentity());
// }

// void FbxAnimation::PrecomputeAll(
// 	const aiScene* scene,
// 	const std::unordered_map<std::string,int>& nodeIndexOfName,
// 	const std::vector<std::string>& boneNames,
// 	const std::vector<XMFLOAT4X4>& boneOffsets,
// 	const XMFLOAT4X4& globalInverse,
// 	int samplesPerSecond)
// {
// 	if (!scene || boneNames.empty() || samplesPerSecond <= 0) { m_Precomputed.clear(); return; }
// 	m_Precomputed.clear();
// 	m_Precomputed.resize(m_Names.size());

// 	// Preserve current playback state while precomputing
// 	int oldClip = m_Current; double oldTime = m_TimeSec; bool oldPlaying = m_Playing;
// 	m_Playing = false;

// 	for (size_t clipIdx = 0; clipIdx < m_Names.size(); ++clipIdx)
// 	{
// 		PrecomputedClip pc{};
// 		pc.ticksPerSec = (clipIdx < m_TicksPerSec.size()) ? m_TicksPerSec[clipIdx] : 25.0;
// 		pc.durationSec = (clipIdx < m_DurationSec.size()) ? m_DurationSec[clipIdx] : 0.0;
// 		pc.sampleDt = (samplesPerSecond > 0) ? (1.0 / (double)samplesPerSecond) : 0.0;
// 		pc.rigid = (m_Type == AnimType::Rigid);
// 		if (pc.durationSec <= 0.0 || pc.sampleDt <= 0.0) { m_Precomputed[clipIdx] = pc; continue; }

// 		// Build channel map for this clip
// 		m_Current = (int)clipIdx;
// 		m_ChannelOfNode.assign(nodeIndexOfName.size(), nullptr);
// 		RebuildChannelMapIfNeeded(scene, m_Current, nodeIndexOfName, m_ChannelOfNode);

// 		int numSamples = (int)std::ceil(pc.durationSec * samplesPerSecond);
// 		if (numSamples < 1) numSamples = 1;
// 		pc.times.resize((size_t)numSamples);
// 		pc.palettes.resize((size_t)numSamples);
// 		XMMATRIX Gi = XMLoadFloat4x4(&globalInverse);

// 		for (int si = 0; si < numSamples; ++si)
// 		{
// 			double tSec = si * pc.sampleDt; if (tSec > pc.durationSec) tSec = pc.durationSec;
// 			pc.times[(size_t)si] = tSec;
// 			// Evaluate globals at tSec (using internal EvaluateGlobals which reads m_TimeSec/m_Current/m_ChannelOfNode)
// 			m_TimeSec = tSec;
// 			std::vector<XMFLOAT4X4> global;
// 			EvaluateGlobals(scene, nodeIndexOfName, global);
// 			pc.palettes[(size_t)si].resize(boneNames.size(), XMMatrixIdentity());
// 			for (size_t bi = 0; bi < boneNames.size(); ++bi)
// 			{
// 				auto itN = nodeIndexOfName.find(boneNames[bi]); if (itN == nodeIndexOfName.end()) continue;
// 				int nodeIdx = itN->second; if (nodeIdx < 0 || nodeIdx >= (int)global.size()) continue;
// 				XMMATRIX G = XMLoadFloat4x4(&global[(size_t)nodeIdx]);
// 				if (pc.rigid)
// 				{
// 					pc.palettes[(size_t)si][bi] = XMMatrixMultiply(Gi, G);
// 				}
// 				else
// 				{
// 					XMMATRIX Off = XMLoadFloat4x4(&boneOffsets[bi]);
// 					pc.palettes[(size_t)si][bi] = XMMatrixMultiply(XMMatrixMultiply(Gi, G), Off);
// 				}
// 			}
// 		}
// 		pc.valid = true;
// 		m_Precomputed[clipIdx] = std::move(pc);
// 	}

// 	// Restore playback state
// 	m_Current = oldClip; m_TimeSec = oldTime; m_Playing = oldPlaying;
// }

void FbxAnimation::UploadPalette(ID3D11DeviceContext* ctx, const std::vector<XMMATRIX>& pal)
{
	if (!ctx) return;
	Microsoft::WRL::ComPtr<ID3D11Device> dev; ctx->GetDevice(dev.GetAddressOf());
	EnsureBoneCB(dev.Get(), (int)pal.size() + 1);
	if (!m_pBoneCB) return;
	struct BoneCB { XMFLOAT4X4 m[1023]; unsigned int boneCount; float pad[3]; };
	BoneCB cb{}; size_t n = std::min(pal.size(), (size_t)1023);
	XMMATRIX I = XMMatrixIdentity();
	for (size_t i = 0; i < 1023; ++i) XMStoreFloat4x4(&cb.m[i], XMMatrixTranspose(I));
	for (size_t i = 0; i < n; ++i) XMStoreFloat4x4(&cb.m[i], XMMatrixTranspose(pal[i]));
	cb.boneCount = (unsigned int)n;
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(ctx->Map(m_pBoneCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) { memcpy(mapped.pData, &cb, sizeof(BoneCB)); ctx->Unmap(m_pBoneCB, 0); }
}

void FbxAnimation::UpdateAndUpload(
	ID3D11DeviceContext* ctx,
	double dtSec,
	const aiScene* scene,
	const std::unordered_map<std::string,int>& nodeIndexOfName,
	const std::vector<std::string>& boneNames,
	const std::vector<XMFLOAT4X4>& boneOffsets,
	const XMFLOAT4X4& globalInverse)
{
	if (m_Playing) SetTimeSec(m_TimeSec + dtSec);
	const aiScene* sc = m_Scene ? m_Scene : scene;
	const auto& nodeMap = !m_NodeIndexOfName.empty() ? m_NodeIndexOfName : nodeIndexOfName;
	const auto* bones = m_BoneNames ? m_BoneNames : &boneNames;
	const auto* offsets = m_BoneOffsets ? m_BoneOffsets : &boneOffsets;
	const XMFLOAT4X4* giPtr = m_GlobalInverse ? m_GlobalInverse : &globalInverse;

	// Fast path: if precomputed exists for current clip, just upload (with time interpolation for smooth motion)
	if (m_Current >= 0 && (size_t)m_Current < m_Precomputed.size())
	{
		const auto& pc = m_Precomputed[(size_t)m_Current];
		if (pc.valid && !pc.times.empty())
		{
			double dur = pc.durationSec;
			double t = m_TimeSec;
			if (dur > 0.0)
			{
				while (t < 0.0) t += dur;
				while (t >= dur) t -= dur;
			}

			// 샘플 간 선형 보간으로 매끄러운 애니메이션 구현
			if (pc.sampleDt > 0.0 && pc.palettes.size() >= 2)
			{
				double f = t / pc.sampleDt;
				double fFloor = std::floor(f);
				int idx0 = (int)fFloor;
				int idx1 = idx0 + 1;
				if (idx0 < 0) idx0 = 0;
				if (idx1 >= (int)pc.palettes.size()) idx1 = (int)pc.palettes.size() - 1;

				float a = (float)(f - fFloor);
				if (idx0 == idx1 || a <= 0.0f)
				{
					UploadPalette(ctx, pc.palettes[(size_t)idx0]);
				}
				else
				{
					const auto& pal0 = pc.palettes[(size_t)idx0];
					const auto& pal1 = pc.palettes[(size_t)idx1];
					size_t nb = pal0.size();
					if (pal1.size() < nb) nb = pal1.size();
					m_PaletteScratch.resize(nb, XMMatrixIdentity());
					for (size_t i = 0; i < nb; ++i)
					{
						m_PaletteScratch[i] = LerpMatrix(pal0[i], pal1[i], a);
					}
					UploadPalette(ctx, m_PaletteScratch);
				}
				return;
			}
			else
			{
				// 샘플 간격 정보가 없으면 가장 가까운 팔레트만 사용
				int idx = 0;
				if (!pc.palettes.empty())
				{
					idx = (int)(pc.palettes.size() * (dur > 0.0 ? (t / dur) : 0.0));
					if (idx >= (int)pc.palettes.size()) idx = (int)pc.palettes.size() - 1;
					if (idx < 0) idx = 0;
				}
				UploadPalette(ctx, pc.palettes[(size_t)idx]);
				return;
			}
		}
	}

	// Fallback: compute on the fly (if not precomputed)
	if (m_Type == AnimType::Rigid) { UploadRigid(ctx, sc, nodeMap, *bones, *giPtr); return; }
	if (!sc || (m_Current < 0 && m_Type != AnimType::Skinned)) return;
	if (m_Current >= 0 && (size_t)m_Current < m_Clips.size() && m_ChannelDirty && !m_ChannelOfNode.empty()) { RebuildChannelMapIfNeeded(m_Clips[(size_t)m_Current], nodeMap, m_ChannelOfNode); m_ChannelDirty = false; }
	EvaluateGlobals(sc, nodeMap, m_GlobalScratch);
	m_PaletteScratch.resize(bones->size(), XMMatrixIdentity());
	XMMATRIX Gi = XMLoadFloat4x4(giPtr);
	for (size_t bi = 0; bi < bones->size(); ++bi)
	{
		auto itN = nodeMap.find((*bones)[bi]); if (itN == nodeMap.end()) continue;
		int nodeIdx = itN->second; if (nodeIdx < 0 || nodeIdx >= (int)m_GlobalScratch.size()) continue;
		XMMATRIX G = XMLoadFloat4x4(&m_GlobalScratch[(size_t)nodeIdx]);
		XMMATRIX Off = XMLoadFloat4x4(&(*offsets)[bi]);
		m_PaletteScratch[bi] = XMMatrixMultiply(XMMatrixMultiply(Gi, G), Off);
	}
	UploadPalette(ctx, m_PaletteScratch);
}

void FbxAnimation::UploadRigid(
	ID3D11DeviceContext* ctx,
	const aiScene* scene,
	const std::unordered_map<std::string,int>& nodeIndexOfName,
	const std::vector<std::string>& boneNames,
	const XMFLOAT4X4& globalInverse)
{
    std::vector<XMFLOAT4X4> global; EvaluateGlobals(scene, nodeIndexOfName, global);
	if (global.empty()) return;
	std::vector<XMMATRIX> pal; pal.resize(boneNames.size(), XMMatrixIdentity());
	XMMATRIX Gi = XMLoadFloat4x4(&globalInverse);
	for (size_t bi = 0; bi < boneNames.size(); ++bi)
	{
		auto itN = nodeIndexOfName.find(boneNames[bi]); if (itN == nodeIndexOfName.end()) continue;
		int nodeIdx = itN->second; if (nodeIdx < 0 || nodeIdx >= (int)global.size()) continue;
		XMMATRIX G = XMLoadFloat4x4(&global[(size_t)nodeIdx]);
		pal[bi] = XMMatrixMultiply(Gi, G);
	}
	UploadPalette(ctx, pal);
}



