#include "pch.h"
#include "FbxAnimation.h"
#include "../Helper.h"

#include <assimp/scene.h>
#include <d3d11.h>

using namespace DirectX;

FbxAnimation::FbxAnimation() {}
FbxAnimation::~FbxAnimation() { Clear(); }

void FbxAnimation::Clear()
{
	SAFE_RELEASE(m_pBoneCB);
	m_pBoneCB = nullptr;
	m_Names.clear(); m_DurationSec.clear(); m_TicksPerSec.clear();
	m_Current = -1; m_TimeSec = 0.0; m_Playing = false; m_Type = AnimType::None;
}

void FbxAnimation::InitMetadata(const aiScene* scene)
{
	m_Names.clear(); m_DurationSec.clear(); m_TicksPerSec.clear();
	if (!scene) return;
	if (scene->mNumAnimations == 0) return;
	m_Names.reserve(scene->mNumAnimations);
	m_DurationSec.reserve(scene->mNumAnimations);
	m_TicksPerSec.reserve(scene->mNumAnimations);
	for (unsigned i = 0; i < scene->mNumAnimations; ++i)
	{
		const aiAnimation* a = scene->mAnimations[i];
		std::string nm = a->mName.length > 0 ? std::string(a->mName.C_Str()) : (std::string("Anim") + std::to_string(i));
		double tps = (a->mTicksPerSecond != 0.0) ? a->mTicksPerSecond : 25.0;
		double durSec = (tps != 0.0) ? (a->mDuration / tps) : 0.0;
		m_Names.push_back(nm);
		m_TicksPerSec.push_back(tps);
		m_DurationSec.push_back(durSec);
	}
	m_Current = 0; m_TimeSec = 0.0; m_Playing = false;
}

void FbxAnimation::SetCurrentIndex(int idx)
{
	if (idx < 0 || idx >= (int)m_Names.size()) return;
	m_Current = idx; m_TimeSec = 0.0;
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

static aiVector3D InterpVec(const aiVectorKey* keys, unsigned count, double t)
{
	if (count == 0) return aiVector3D(0, 0, 0);
	if (count == 1) return keys[0].mValue;
	unsigned i = 0; while (i + 1 < count && t >= keys[i + 1].mTime) ++i; unsigned j = (i + 1 < count) ? i + 1 : i;
	double dt = keys[j].mTime - keys[i].mTime; double a = (dt > 0.0) ? (t - keys[i].mTime) / dt : 0.0;
	aiVector3D v0 = keys[i].mValue, v1 = keys[j].mValue; return v0 + (float)a * (v1 - v0);
}

static aiQuaternion InterpQuat(const aiQuatKey* keys, unsigned count, double t)
{
	if (count == 0) return aiQuaternion();
	if (count == 1) return keys[0].mValue;
	unsigned i = 0; while (i + 1 < count && t >= keys[i + 1].mTime) ++i; unsigned j = (i + 1 < count) ? i + 1 : i;
	double dt = keys[j].mTime - keys[i].mTime; double a = (dt > 0.0) ? (t - keys[i].mTime) / dt : 0.0;
	aiQuaternion q; aiQuaternion::Interpolate(q, keys[i].mValue, keys[j].mValue, (float)a); q.Normalize(); return q;
}

void FbxAnimation::EvaluateGlobals(
	const aiScene* scene,
	const std::unordered_map<std::wstring, const aiNodeAnim*>& channelOf,
	const std::unordered_map<std::string,int>& nodeIndexOfName,
	std::vector<XMFLOAT4X4>& outGlobal) const
{
	outGlobal.clear(); if (!scene) return;
	outGlobal.resize(nodeIndexOfName.size());
	std::function<void(const aiNode*, int, const XMMATRIX&)> eval = [&](const aiNode* node, int idx, const XMMATRIX& parent){
		aiVector3D S(1,1,1), T(0,0,0); aiQuaternion R; aiMatrix4x4 mLocal = node->mTransformation;
		auto itCh = channelOf.find(WStringFromUtf8(node->mName.C_Str()));
		if (itCh != channelOf.end())
		{
			double tTicks = m_TimeSec * ((m_Current >= 0 && (size_t)m_Current < m_TicksPerSec.size()) ? m_TicksPerSec[m_Current] : 25.0);
			const aiNodeAnim* ch = itCh->second;
			S = (ch->mNumScalingKeys   > 0) ? InterpVec(ch->mScalingKeys,   ch->mNumScalingKeys,   tTicks) : aiVector3D(1,1,1);
			T = (ch->mNumPositionKeys  > 0) ? InterpVec(ch->mPositionKeys,  ch->mNumPositionKeys,  tTicks) : aiVector3D(0,0,0);
			R = (ch->mNumRotationKeys  > 0) ? InterpQuat(ch->mRotationKeys, ch->mNumRotationKeys,  tTicks) : aiQuaternion();
			aiMatrix4x4 mS; mS.Scaling(S, mS); aiMatrix4x4 mR = aiMatrix4x4(R.GetMatrix()); aiMatrix4x4 mT; mT.Translation(T, mT);
			mLocal = mT * mR * mS;
		}
		XMFLOAT4X4 lm; lm._11 = (float)mLocal.a1; lm._12 = (float)mLocal.a2; lm._13 = (float)mLocal.a3; lm._14 = (float)mLocal.a4;
		lm._21 = (float)mLocal.b1; lm._22 = (float)mLocal.b2; lm._23 = (float)mLocal.b3; lm._24 = (float)mLocal.b4;
		lm._31 = (float)mLocal.c1; lm._32 = (float)mLocal.c2; lm._33 = (float)mLocal.c3; lm._34 = (float)mLocal.c4;
		lm._41 = (float)mLocal.d1; lm._42 = (float)mLocal.d2; lm._43 = (float)mLocal.d3; lm._44 = (float)mLocal.d4;
		XMMATRIX L = XMLoadFloat4x4(&lm);
		XMMATRIX G = XMMatrixMultiply(parent, L);
		XMStoreFloat4x4(&outGlobal[idx], G);
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
	if (m_Type == AnimType::Rigid) { UploadRigid(ctx, scene, nodeIndexOfName, boneNames, globalInverse); return; }
	if (!scene || m_Current < 0) return;
	const aiAnimation* anim = (scene->mNumAnimations > 0) ? scene->mAnimations[m_Current] : nullptr;
	std::unordered_map<std::wstring, const aiNodeAnim*> channelOf;
	if (anim)
	{
		for (unsigned i = 0; i < anim->mNumChannels; ++i)
		{
			const aiNodeAnim* ch = anim->mChannels[i];
			channelOf[WStringFromUtf8(ch->mNodeName.C_Str())] = ch;
		}
	}
	std::vector<XMFLOAT4X4> global; EvaluateGlobals(scene, channelOf, nodeIndexOfName, global);
	std::vector<XMMATRIX> pal; pal.resize(boneNames.size(), XMMatrixIdentity());
	XMMATRIX Gi = XMLoadFloat4x4(&globalInverse);
	for (size_t bi = 0; bi < boneNames.size(); ++bi)
	{
		auto itN = nodeIndexOfName.find(boneNames[bi]); if (itN == nodeIndexOfName.end()) continue;
		int nodeIdx = itN->second; if (nodeIdx < 0 || nodeIdx >= (int)global.size()) continue;
		XMMATRIX G = XMLoadFloat4x4(&global[(size_t)nodeIdx]);
		XMMATRIX Off = XMLoadFloat4x4(&boneOffsets[bi]);
		pal[bi] = XMMatrixMultiply(XMMatrixMultiply(Gi, G), Off);
	}
	UploadPalette(ctx, pal);
}

void FbxAnimation::UploadRigid(
	ID3D11DeviceContext* ctx,
	const aiScene* scene,
	const std::unordered_map<std::string,int>& nodeIndexOfName,
	const std::vector<std::string>& boneNames,
	const XMFLOAT4X4& globalInverse)
{
	std::unordered_map<std::wstring, const aiNodeAnim*> channelOf; // time used internally
	std::vector<XMFLOAT4X4> global; EvaluateGlobals(scene, channelOf, nodeIndexOfName, global);
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


