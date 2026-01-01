#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <DirectXMath.h>
#include <assimp/scene.h>
#include <d3d11.h>
#include <wrl/client.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// =========================================================
// [상수 정의]
// =========================================================
static constexpr int MAX_BONES = 1023;

// =========================================================
// [TransformSRT] 
// 행렬을 Scale, Rotation, Translation 성분으로 분리하여 관리
// =========================================================
struct TransformSRT {
    XMVECTOR S{ 1.0f, 1.0f, 1.0f, 0.0f };
    XMVECTOR R{ 0.0f, 0.0f, 0.0f, 1.0f }; // Quaternion
    XMVECTOR T{ 0.0f, 0.0f, 0.0f, 1.0f };

    // TRS -> Matrix 변환
    // IMPORTANT:
    // - 이 프로젝트의 기존 스키닝 경로(`FbxAnimation.cpp`)는
    //   local = (T * R * S), global = (parent * local) 규약을 사용한다.
    // - CharacterAnimator도 반드시 동일한 규약을 따라야 팔레트가 깨지지 않는다.
    XMMATRIX ToMatrix() const {
        return XMMatrixTranslationFromVector(T) *
            XMMatrixRotationQuaternion(R) *
            XMMatrixScalingFromVector(S);
    }

    // 1. 키프레임 보간 & 전환 블렌딩 (A-B 사이 보간)
    static TransformSRT Lerp(const TransformSRT& a, const TransformSRT& b, float t) {
        TransformSRT res;
        res.S = XMVectorLerp(a.S, b.S, t);           // Scale: 선형 보간
        res.R = XMQuaternionSlerp(a.R, b.R, t);      // Rotation: 구면 보간 (Slerp)
        res.T = XMVectorLerp(a.T, b.T, t);           // Position: 선형 보간
        return res;
    }

    // 2. 애디티브 연산: Current + (Add - Ref) * Alpha
    static TransformSRT Additive(const TransformSRT& current, const TransformSRT& add, const TransformSRT& ref, float alpha) {
        if (alpha <= 0.001f) return current;

        TransformSRT res = current;

        // 위치: (Add - Ref) * Alpha 더하기
        XMVECTOR deltaT = XMVectorSubtract(add.T, ref.T);
        res.T = XMVectorAdd(current.T, XMVectorScale(deltaT, alpha));

        // 회전: (Add * InvRef) 만큼 회전 추가
        XMVECTOR invRefR = XMQuaternionInverse(ref.R);
        XMVECTOR deltaR = XMQuaternionMultiply(add.R, invRefR); // 차이 회전값

        // Alpha만큼 적용 (Identity에서 DeltaR로 Slerp)
        XMVECTOR weightedDeltaR = XMQuaternionSlerp(XMQuaternionIdentity(), deltaR, alpha);
        res.R = XMQuaternionMultiply(weightedDeltaR, current.R); // 현재 회전에 누적

        return res;
    }
};

// =========================================================
// [Socket] 무기 등을 부착할 위치
// =========================================================
struct Socket {
    std::string name;
    std::string parentBoneName;
    int parentNodeIndex = -1; // 전체 노드 배열에서의 인덱스

    XMFLOAT3 offsetPos = { 0,0,0 };
    XMFLOAT3 offsetRot = { 0,0,0 }; // Degree
    XMFLOAT3 offsetScale = { 1,1,1 };

    // NOTE: 미초기화 경고/버그 방지를 위해 기본값을 Identity로 둔다.
    XMMATRIX offsetMatrix = XMMatrixIdentity();     // 로컬 오프셋
    XMMATRIX finalWorldMatrix = XMMatrixIdentity(); // 최종 월드 행렬

    void UpdateOffset() {
        XMMATRIX mS = XMMatrixScaling(offsetScale.x, offsetScale.y, offsetScale.z);
        XMMATRIX mR = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(offsetRot.x),
            XMConvertToRadians(offsetRot.y),
            XMConvertToRadians(offsetRot.z));
        XMMATRIX mT = XMMatrixTranslation(offsetPos.x, offsetPos.y, offsetPos.z);
        // 동일 규약: local = T * R * S
        offsetMatrix = mT * mR * mS;
    }
};

// =========================================================
// [CharacterAnimator]
// 핵심 기능: 블렌딩, 애디티브, 레이어드, IK, 소켓
// =========================================================
class CharacterAnimator {
private:
    // 참조 데이터 (소유권 없음)
    const aiScene* m_Scene = nullptr;
    const std::unordered_map<std::string, int>* m_NodeIndexMap = nullptr;
    XMFLOAT4X4 m_GlobalInverse = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

    // 구조 데이터
    std::vector<std::string> m_BoneNames;
    std::vector<XMMATRIX> m_BoneOffsets;     // Inverse Bind Pose
    std::vector<int> m_BoneNodeIndices;      // Bone Name -> Node Index

    // 노드 캐시 (빠른 접근용)
    std::vector<const aiNode*> m_NodePtrs;
    std::vector<int> m_NodeParents;
    std::vector<std::string> m_NodeNames;

    // 매 프레임 계산되는 데이터
    std::vector<TransformSRT> m_LocalSRTs;   // 로컬 변환 (애니메이션 적용)
    std::vector<XMMATRIX> m_GlobalMatrices;  // 글로벌 변환 (계층 구조 반영)

    // 소켓 및 GPU 버퍼
    std::vector<Socket> m_Sockets;
    ComPtr<ID3D11Buffer> m_pBoneBuffer;

public:
    // 외부에서 접근 가능한 최종 행렬 (렌더링용)
    std::vector<XMMATRIX> finalTransforms;

    // -----------------------------------------------------------------
    // [초기화]
    // -----------------------------------------------------------------
    void Initialize(ID3D11Device* device, const aiScene* scene,
        const std::unordered_map<std::string, int>& nodeMap,
        const XMFLOAT4X4& globalInv,
        const std::vector<std::string>& boneNames,
        const std::vector<DirectX::XMFLOAT4X4>& boneOffsets)
    {
        m_Scene = scene;
        m_NodeIndexMap = &nodeMap;
        m_GlobalInverse = globalInv;
        m_BoneNames = boneNames;

        size_t boneCount = boneNames.size();
        m_BoneOffsets.resize(boneCount);
        m_BoneNodeIndices.assign(boneCount, -1);
        finalTransforms.assign(boneCount, XMMatrixIdentity());

        // 1. 본 오프셋 및 인덱스 매핑
        for (size_t i = 0; i < boneCount; ++i) {
            m_BoneOffsets[i] = XMLoadFloat4x4(&boneOffsets[i]);
            if (nodeMap.count(boneNames[i])) {
                m_BoneNodeIndices[i] = nodeMap.at(boneNames[i]);
            }
        }

        // 2. 전체 노드 계층 구조 캐싱 (재귀 없이 순회하기 위해)
        size_t nodeCount = nodeMap.size();
        m_NodePtrs.resize(nodeCount, nullptr);
        m_NodeParents.resize(nodeCount, -1);
        m_NodeNames.resize(nodeCount);
        m_LocalSRTs.resize(nodeCount);
        m_GlobalMatrices.resize(nodeCount, XMMatrixIdentity());

        if (m_Scene && m_Scene->mRootNode) {
            BuildNodeHierarchy(m_Scene->mRootNode, -1);
        }

        // 3. GPU 버퍼 생성 (최대 크기 1023으로 고정)
        if (device) {
            D3D11_BUFFER_DESC desc = {};
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth = sizeof(XMFLOAT4X4) * MAX_BONES + sizeof(unsigned int) * 4; // 넉넉하게
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            device->CreateBuffer(&desc, nullptr, m_pBoneBuffer.GetAddressOf());
        }
    }

    // -----------------------------------------------------------------
    // [메인 업데이트]
    // -----------------------------------------------------------------
    void UpdateAnimation(
        float dt,
        const aiAnimation* animA, float timeA,
        const aiAnimation* animB, float timeB,
        float blendFactor,
        const aiAnimation* animUpper = nullptr, float timeUpper = 0.f, // 상체 레이어
        const aiAnimation* animAdd = nullptr, float timeAdd = 0.f,     // 애디티브
        const aiAnimation* animRef = nullptr,                          // 애디티브 기준
        float proceduralRecoil = 0.0f, uint32_t seed = 0,              // 절차적 흔들림
        bool enableIK = false, const char* ikTipBone = nullptr, int chainLen = 0, XMVECTOR ikTarget = XMVectorZero(), float ikWeight = 0.0f
    ) {
        if (m_NodePtrs.empty()) return;

        // =========================================================
        // [Idle 검증용 패스트패스]
        // - 지금 단계의 목표: FbxAnimation과 "완전히 같은" 결과로 Idle이 정상 스키닝되는지 확인.
        // - 아래 조건(단일 애니메이션, 블렌드/레이어/애디티브/IK/노이즈 없음)에서는
        //   FbxAnimation.cpp와 동일한 평가식을 사용한다:
        //     local = (채널 있으면 T*R*S, 없으면 node->mTransformation 원본)
        //     global = parent * local
        //     final = Gi * global(node) * Off
        // =========================================================
        const bool isSimpleSingleAnim =
            (animA == animB) &&
            (blendFactor == 0.0f) &&
            (animUpper == nullptr) &&
            (animAdd == nullptr) &&
            (animRef == nullptr) &&
            (proceduralRecoil == 0.0f) &&
            (!enableIK);
        if (isSimpleSingleAnim) {
            EvaluateLikeFbxAnimation(animA, timeA);

            // 팔레트 계산
            XMMATRIX mGlobalInv = XMLoadFloat4x4(&m_GlobalInverse);
            for (size_t bi = 0; bi < m_BoneNodeIndices.size(); ++bi) {
                const int nodeIdx = m_BoneNodeIndices[bi];
                if (nodeIdx >= 0 && nodeIdx < (int)m_GlobalMatrices.size()) {
                    finalTransforms[bi] = mGlobalInv * m_GlobalMatrices[(size_t)nodeIdx] * m_BoneOffsets[bi];
                }
                else {
                    finalTransforms[bi] = XMMatrixIdentity();
                }
            }

            // 소켓 갱신
            for (auto& s : m_Sockets) {
                if (s.parentNodeIndex >= 0 && s.parentNodeIndex < (int)m_GlobalMatrices.size()) {
                    s.finalWorldMatrix = s.offsetMatrix * m_GlobalMatrices[(size_t)s.parentNodeIndex];
                }
                else {
                    s.finalWorldMatrix = s.offsetMatrix;
                }
            }
            return;
        }

        // =========================================================
        // [고급 경로 (블렌드/레이어/애디티브/IK/노이즈)]
        // 핵심 원칙:
        // - "채널이 없는 노드"는 bind pose의 원본 로컬 행렬(aiNode::mTransformation)을 그대로 유지한다.
        //   (SRT로 분해/재조합하면 FBX의 pivot/프리로테이션 등의 정보가 깨져서 원점 뭉개짐이 재발함)
        // - 수정이 필요한 노드(블렌드/레이어/애디티브/노이즈/IK)는 XMMatrixDecompose 기반으로 SRT를 다룬다.
        // =========================================================

        const size_t nodeCount = m_NodePtrs.size();
        auto Clamp01 = [](float x) { return std::clamp(x, 0.0f, 1.0f); };
        const float baseBlend = Clamp01(blendFactor);

        // --------- local 평가 (FbxAnimation과 동일 규칙) ---------
        auto BuildChannelMap = [&](const aiAnimation* anim, std::vector<const aiNodeAnim*>& outChOfNode) {
            outChOfNode.assign(nodeCount, nullptr);
            if (!anim || !m_NodeIndexMap) return;
            for (unsigned ci = 0; ci < anim->mNumChannels; ++ci) {
                const aiNodeAnim* ch = anim->mChannels[ci];
                if (!ch) continue;
                auto it = m_NodeIndexMap->find(ch->mNodeName.C_Str());
                if (it == m_NodeIndexMap->end()) continue;
                const int idx = it->second;
                if (idx >= 0 && (size_t)idx < outChOfNode.size()) outChOfNode[(size_t)idx] = ch;
            }
        };

        auto WrapToTicks = [&](const aiAnimation* anim, float timeSec) -> double {
            const double tps = (anim && anim->mTicksPerSecond != 0.0) ? anim->mTicksPerSecond : 25.0;
            const double dur = (anim && anim->mDuration > 0.0) ? anim->mDuration : 0.0;
            double tTicks = (double)timeSec * tps;
            if (dur > 0.0) {
                tTicks = std::fmod(tTicks, dur);
                if (tTicks < 0.0) tTicks += dur;
            }
            return tTicks;
        };

        auto EvalLocals = [&](const aiAnimation* anim, float timeSec,
            std::vector<XMMATRIX>& outLocals, std::vector<uint8_t>& outHasChannel)
        {
            outLocals.assign(nodeCount, XMMatrixIdentity());
            outHasChannel.assign(nodeCount, 0);

            std::vector<const aiNodeAnim*> chOfNode;
            BuildChannelMap(anim, chOfNode);
            const double tTicks = WrapToTicks(anim, timeSec);

            for (size_t i = 0; i < nodeCount; ++i) {
                const aiNode* node = m_NodePtrs[i];
                if (!node) {
                    outLocals[i] = XMMatrixIdentity();
                    outHasChannel[i] = 0;
                    continue;
                }

                const aiNodeAnim* ch = (i < chOfNode.size()) ? chOfNode[i] : nullptr;
                if (anim && ch) {
                    const aiVector3D S = InterpVecFbx(ch->mScalingKeys, ch->mNumScalingKeys, tTicks, aiVector3D(1, 1, 1));
                    const aiVector3D T = InterpVecFbx(ch->mPositionKeys, ch->mNumPositionKeys, tTicks, aiVector3D(0, 0, 0));
                    const aiQuaternion R = InterpQuatFbx(ch->mRotationKeys, ch->mNumRotationKeys, tTicks, aiQuaternion());

                    aiMatrix4x4 mS; mS.Scaling(S, mS);
                    aiMatrix4x4 mR = aiMatrix4x4(R.GetMatrix());
                    aiMatrix4x4 mT; mT.Translation(T, mT);
                    const aiMatrix4x4 mLocal = mT * mR * mS; // TRS (FbxAnimation과 동일)
                    outLocals[i] = AiToXM(mLocal);
                    outHasChannel[i] = 1;
                }
                else {
                    outLocals[i] = AiToXM(node->mTransformation); // bind 원본 유지
                    outHasChannel[i] = 0;
                }
            }
        };

        auto DecomposeSRT = [&](const XMMATRIX& m, XMVECTOR& outS, XMVECTOR& outR, XMVECTOR& outT) -> bool {
            // XMMatrixDecompose: outS, outR(quat), outT
            return XMMatrixDecompose(&outS, &outR, &outT, m) != 0;
        };

        auto ComposeTRS = [&](XMVECTOR S, XMVECTOR R, XMVECTOR T) -> XMMATRIX {
            // 기존 시스템과 동일: T * R * S
            return XMMatrixTranslationFromVector(T) * XMMatrixRotationQuaternion(R) * XMMatrixScalingFromVector(S);
        };

        auto BlendMatricesSRT = [&](const XMMATRIX& a, const XMMATRIX& b, float alpha) -> XMMATRIX {
            alpha = Clamp01(alpha);
            XMVECTOR Sa, Ra, Ta;
            XMVECTOR Sb, Rb, Tb;
            if (!DecomposeSRT(a, Sa, Ra, Ta)) return (alpha < 0.5f) ? a : b;
            if (!DecomposeSRT(b, Sb, Rb, Tb)) return (alpha < 0.5f) ? a : b;
            XMVECTOR S = XMVectorLerp(Sa, Sb, alpha);
            XMVECTOR T = XMVectorLerp(Ta, Tb, alpha);
            XMVECTOR R = XMQuaternionSlerp(Ra, Rb, alpha);
            R = XMQuaternionNormalize(R);
            return ComposeTRS(S, R, T);
        };

        auto ApplyAdditiveSRT = [&](const XMMATRIX& baseM, const XMMATRIX& addM, const XMMATRIX& refM, float alpha) -> XMMATRIX {
            alpha = Clamp01(alpha);
            XMVECTOR Sb, Rb, Tb;
            XMVECTOR Sa, Ra, Ta;
            XMVECTOR Sr, Rr, Tr;
            if (!DecomposeSRT(baseM, Sb, Rb, Tb)) return baseM;
            if (!DecomposeSRT(addM, Sa, Ra, Ta)) return baseM;
            if (!DecomposeSRT(refM, Sr, Rr, Tr)) return baseM;

            // deltaT = (add - ref), deltaS = (add - ref)
            XMVECTOR deltaT = XMVectorSubtract(Ta, Tr);
            XMVECTOR deltaS = XMVectorSubtract(Sa, Sr);

            // deltaR = add * inverse(ref)
            XMVECTOR invRefR = XMQuaternionInverse(Rr);
            XMVECTOR deltaR = XMQuaternionMultiply(Ra, invRefR);
            deltaR = XMQuaternionNormalize(deltaR);

            // 적용: base + alpha * delta
            XMVECTOR outT = XMVectorAdd(Tb, XMVectorScale(deltaT, alpha));
            XMVECTOR outS = XMVectorAdd(Sb, XMVectorScale(deltaS, alpha));

            // rotation: baseR * slerp(I, deltaR, alpha)
            XMVECTOR deltaApplied = XMQuaternionSlerp(XMQuaternionIdentity(), deltaR, alpha);
            XMVECTOR outR = XMQuaternionMultiply(deltaApplied, Rb);
            outR = XMQuaternionNormalize(outR);

            return ComposeTRS(outS, outR, outT);
        };

        auto ApplyProceduralNoiseToMatrix = [&](const XMMATRIX& inM, int idx, float time, float strength, uint32_t seedV) -> XMMATRIX {
            if (strength <= 0.0f) return inM;
            XMVECTOR S, R, T;
            if (!DecomposeSRT(inM, S, R, T)) return inM;

            auto Hash = [](uint32_t x) { x ^= x << 13; x ^= x >> 17; return (float)(x & 0xFFFF) / 65536.0f; };
            uint32_t h = (uint32_t)idx * 12345 + seedV;
            float rx = sinf(time * 10.0f + Hash(h) * 6.28f) * strength * 0.05f;
            float ry = sinf(time * 7.0f + Hash(h + 1) * 6.28f) * strength * 0.05f;
            float rz = sinf(time * 13.0f + Hash(h + 2) * 6.28f) * strength * 0.05f;
            XMVECTOR deltaR = XMQuaternionRotationRollPitchYaw(rx, ry, rz);
            R = XMQuaternionMultiply(deltaR, R);
            R = XMQuaternionNormalize(R);
            return ComposeTRS(S, R, T);
        };

        auto ComputeGlobalsFromLocals = [&](const std::vector<XMMATRIX>& locals, std::vector<XMMATRIX>& outGlobals) {
            outGlobals.assign(nodeCount, XMMatrixIdentity());
            std::vector<uint8_t> done(nodeCount, 0);
            auto computeNode = [&](auto&& self, int idx) -> void {
                if (idx < 0 || (size_t)idx >= nodeCount) return;
                if (done[(size_t)idx]) return;
                const int pi = m_NodeParents[(size_t)idx];
                if (pi >= 0) self(self, pi);
                const XMMATRIX parent = (pi >= 0 && (size_t)pi < nodeCount) ? outGlobals[(size_t)pi] : XMMatrixIdentity();
                outGlobals[(size_t)idx] = parent * locals[(size_t)idx];
                done[(size_t)idx] = 1;
            };
            for (int i = 0; i < (int)nodeCount; ++i) computeNode(computeNode, i);
        };

        // --------- 1) 베이스(Idle/Walk/Run) 평가 + 전환 블렌딩 ---------
        std::vector<XMMATRIX> localsA, localsB;
        std::vector<uint8_t> hasA, hasB;
        EvalLocals(animA, timeA, localsA, hasA);
        EvalLocals(animB, timeB, localsB, hasB);

        std::vector<XMMATRIX> localsFinal(nodeCount, XMMatrixIdentity());
        for (size_t i = 0; i < nodeCount; ++i) {
            // 둘 다 채널이 없으면 bind 원본(=localsA/localsB가 동일한 node->mTransformation)을 그대로 유지
            if (!hasA[i] && !hasB[i]) {
                localsFinal[i] = localsA[i];
            }
            else if (baseBlend <= 0.0f) {
                localsFinal[i] = localsA[i];
            }
            else if (baseBlend >= 1.0f) {
                localsFinal[i] = localsB[i];
            }
            else {
                localsFinal[i] = BlendMatricesSRT(localsA[i], localsB[i], baseBlend);
            }
        }

        // --------- 2) 상체 레이어(마스크) ---------
        if (animUpper) {
            std::vector<XMMATRIX> localsU;
            std::vector<uint8_t> hasU;
            EvalLocals(animUpper, timeUpper, localsU, hasU);

            for (size_t i = 0; i < nodeCount; ++i) {
                if (!IsUpperBody(m_NodeNames[i])) continue;
                if (!hasU[i]) continue; // 상체 애니메이션 채널이 없는 노드는 건드리지 않는다(=bind 유지)
                localsFinal[i] = BlendMatricesSRT(localsFinal[i], localsU[i], 0.95f);
            }
        }

        // --------- 3) 애디티브(예: Shoot 반동/상체 보정) ---------
        if (animAdd && animRef) {
            std::vector<XMMATRIX> localsAdd, localsRef;
            std::vector<uint8_t> hasAdd, hasRef;
            EvalLocals(animAdd, timeAdd, localsAdd, hasAdd);
            EvalLocals(animRef, 0.0f, localsRef, hasRef);

            for (size_t i = 0; i < nodeCount; ++i) {
                // 보통 상체에만 적용 (필요하면 App.cpp에서 마스크/강도를 조절)
                if (!IsUpperBody(m_NodeNames[i])) continue;
                if (!hasAdd[i]) continue; // 애디티브 채널이 없는 노드는 스킵 (bind 보존)
                localsFinal[i] = ApplyAdditiveSRT(localsFinal[i], localsAdd[i], localsRef[i], 1.0f);
            }
        }

        // --------- 4) 절차적 흔들림(노이즈) ---------
        if (proceduralRecoil > 0.0f) {
            for (size_t i = 0; i < nodeCount; ++i) {
                if (!IsUpperBody(m_NodeNames[i])) continue;
                localsFinal[i] = ApplyProceduralNoiseToMatrix(localsFinal[i], (int)i, timeA, proceduralRecoil, seed);
            }
        }

        // --------- 5) IK (간단 CCD: localsFinal을 직접 수정) ---------
        if (enableIK && ikTipBone && m_NodeIndexMap && m_NodeIndexMap->count(ikTipBone)) {
            const int tipIdx = m_NodeIndexMap->at(ikTipBone);
            if (tipIdx >= 0 && (size_t)tipIdx < nodeCount && chainLen > 0 && ikWeight > 0.0f) {
                // 체인 구성: tip -> parent ...
                std::vector<int> chain;
                int curr = tipIdx;
                for (int i = 0; i <= chainLen && curr != -1; ++i) {
                    chain.push_back(curr);
                    curr = m_NodeParents[(size_t)curr];
                }

                auto SolveIKOnce = [&]() {
                    std::vector<XMMATRIX> globals;
                    ComputeGlobalsFromLocals(localsFinal, globals);

                    XMVECTOR effectorPos = globals[(size_t)tipIdx].r[3];
                    for (size_t ci = 1; ci < chain.size(); ++ci) {
                        const int jointIdx = chain[ci];
                        const int pIdx = (jointIdx >= 0) ? m_NodeParents[(size_t)jointIdx] : -1;

                        XMVECTOR jointPos = globals[(size_t)jointIdx].r[3];
                        XMVECTOR toEff = XMVector3Normalize(XMVectorSubtract(effectorPos, jointPos));
                        XMVECTOR toTar = XMVector3Normalize(XMVectorSubtract(ikTarget, jointPos));

                        float dot = XMVectorGetX(XMVector3Dot(toEff, toTar));
                        if (dot > 0.999f) continue;
                        dot = std::clamp(dot, -1.0f, 1.0f);

                        XMVECTOR axisWS = XMVector3Cross(toEff, toTar);
                        axisWS = XMVector3Normalize(axisWS);
                        float angle = acosf(dot) * ikWeight;

                        // World axis -> parent space axis (로컬 회전 갱신용)
                        XMVECTOR axisLS = axisWS;
                        if (pIdx >= 0 && (size_t)pIdx < nodeCount) {
                            XMMATRIX invP = XMMatrixInverse(nullptr, globals[(size_t)pIdx]);
                            axisLS = XMVector3TransformNormal(axisWS, invP);
                            axisLS = XMVector3Normalize(axisLS);
                        }

                        // joint local을 SRT로 분해 -> R만 갱신
                        XMVECTOR S, R, T;
                        if (!DecomposeSRT(localsFinal[(size_t)jointIdx], S, R, T)) continue;
                        XMVECTOR qDelta = XMQuaternionRotationAxis(axisLS, angle);
                        R = XMQuaternionMultiply(qDelta, R);
                        R = XMQuaternionNormalize(R);
                        localsFinal[(size_t)jointIdx] = ComposeTRS(S, R, T);

                        // 업데이트된 상태에서 effector 재계산
                        ComputeGlobalsFromLocals(localsFinal, globals);
                        effectorPos = globals[(size_t)tipIdx].r[3];
                    }
                };

                // 5회 정도 반복
                for (int iter = 0; iter < 5; ++iter) SolveIKOnce();
            }
        }

        // --------- 6) globals + 팔레트 + 소켓 ---------
        ComputeGlobalsFromLocals(localsFinal, m_GlobalMatrices);

        // 최종 스키닝 행렬 (GlobalInv * NodeGlobal * BoneOffset)
        XMMATRIX mGlobalInv = XMLoadFloat4x4(&m_GlobalInverse);
        for (size_t bi = 0; bi < m_BoneNames.size(); ++bi) {
            const int nodeIdx = m_BoneNodeIndices[bi];
            if (nodeIdx >= 0 && nodeIdx < (int)m_GlobalMatrices.size()) {
                finalTransforms[bi] = mGlobalInv * m_GlobalMatrices[(size_t)nodeIdx] * m_BoneOffsets[bi];
            }
            else {
                finalTransforms[bi] = XMMatrixIdentity();
            }
        }

        // 소켓 갱신
        for (auto& s : m_Sockets) {
            if (s.parentNodeIndex >= 0 && s.parentNodeIndex < (int)m_GlobalMatrices.size()) {
                s.finalWorldMatrix = s.offsetMatrix * m_GlobalMatrices[(size_t)s.parentNodeIndex];
            }
            else {
                s.finalWorldMatrix = s.offsetMatrix;
            }
        }
    }

    // -----------------------------------------------------------------
    // [GPU 업로드]
    // -----------------------------------------------------------------
    void UploadPalette(ID3D11DeviceContext* ctx, const std::vector<XMMATRIX>& pal) {
        if (!ctx || !m_pBoneBuffer) return;

        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(ctx->Map(m_pBoneBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            // 구조체 정의 없이 포인터로 직접 접근 (충돌 방지)
            XMFLOAT4X4* pDest = reinterpret_cast<XMFLOAT4X4*>(mapped.pData);

            size_t count = std::min(pal.size(), (size_t)MAX_BONES);
            for (size_t i = 0; i < count; ++i) {
                XMStoreFloat4x4(&pDest[i], XMMatrixTranspose(pal[i]));
            }

            // 본 개수 정보 기록 (버퍼 끝부분)
            uint8_t* pByteDest = reinterpret_cast<uint8_t*>(mapped.pData);
            unsigned int* pCount = reinterpret_cast<unsigned int*>(pByteDest + sizeof(XMFLOAT4X4) * MAX_BONES);
            *pCount = (unsigned int)count;

            ctx->Unmap(m_pBoneBuffer.Get(), 0);
        }
    }

    // -----------------------------------------------------------------
    // [소켓 관련]
    // -----------------------------------------------------------------
    void SetSocketSRT(const std::string& name, const std::string& parentBone, XMFLOAT3 pos, XMFLOAT3 rot, XMFLOAT3 scale) {
        // 기존 소켓 찾기
        auto it = std::find_if(m_Sockets.begin(), m_Sockets.end(), [&](const Socket& s) { return s.name == name; });
        Socket* s = (it != m_Sockets.end()) ? &(*it) : &m_Sockets.emplace_back();

        s->name = name;
        s->parentBoneName = parentBone;
        s->offsetPos = pos; s->offsetRot = rot; s->offsetScale = scale;
        s->UpdateOffset();

        if (m_NodeIndexMap && m_NodeIndexMap->count(parentBone)) {
            s->parentNodeIndex = m_NodeIndexMap->at(parentBone);
        }
    }

    XMMATRIX GetSocketWorldMatrix(const std::string& name, CXMMATRIX charWorld) {
        for (const auto& s : m_Sockets) {
            if (s.name == name) return s.finalWorldMatrix * charWorld;
        }
        return charWorld;
    }

    ID3D11Buffer* GetBoneCB() const { return m_pBoneBuffer.Get(); }

private:
    // -----------------------------------------------------------------
    // [내부 로직 함수들] - 람다 제거됨
    // -----------------------------------------------------------------

    // aiMatrix4x4 -> XMMATRIX (분해/재조합 없이 원본 행렬을 그대로 사용)
    static XMMATRIX AiToXM(const aiMatrix4x4& m) {
        XMFLOAT4X4 fm;
        fm._11 = (float)m.a1; fm._12 = (float)m.a2; fm._13 = (float)m.a3; fm._14 = (float)m.a4;
        fm._21 = (float)m.b1; fm._22 = (float)m.b2; fm._23 = (float)m.b3; fm._24 = (float)m.b4;
        fm._31 = (float)m.c1; fm._32 = (float)m.c2; fm._33 = (float)m.c3; fm._34 = (float)m.c4;
        fm._41 = (float)m.d1; fm._42 = (float)m.d2; fm._43 = (float)m.d3; fm._44 = (float)m.d4;
        return XMLoadFloat4x4(&fm);
    }

    // FbxAnimation.cpp의 보간 로직과 동일한 형태(클램프 기반)
    static aiVector3D InterpVecFbx(const aiVectorKey* keys, unsigned count, double tTicks, const aiVector3D& fallback) {
        if (!keys || count == 0) return fallback;
        if (count == 1) return keys[0].mValue;
        unsigned i = 0;
        while (i + 1 < count && tTicks >= keys[i + 1].mTime) ++i;
        unsigned j = (i + 1 < count) ? (i + 1) : i;
        const double dt = keys[j].mTime - keys[i].mTime;
        const double a = (dt > 0.0) ? (tTicks - keys[i].mTime) / dt : 0.0;
        const aiVector3D v0 = keys[i].mValue;
        const aiVector3D v1 = keys[j].mValue;
        return v0 + (float)a * (v1 - v0);
    }

    static aiQuaternion InterpQuatFbx(const aiQuatKey* keys, unsigned count, double tTicks, const aiQuaternion& fallback) {
        if (!keys || count == 0) return fallback;
        if (count == 1) return keys[0].mValue;
        unsigned i = 0;
        while (i + 1 < count && tTicks >= keys[i + 1].mTime) ++i;
        unsigned j = (i + 1 < count) ? (i + 1) : i;
        const double dt = keys[j].mTime - keys[i].mTime;
        const double a = (dt > 0.0) ? (tTicks - keys[i].mTime) / dt : 0.0;
        aiQuaternion q;
        aiQuaternion::Interpolate(q, keys[i].mValue, keys[j].mValue, (float)a);
        q.Normalize();
        return q;
    }

    void EvaluateLikeFbxAnimation(const aiAnimation* anim, float timeSec) {
        const size_t nodeCount = m_NodePtrs.size();
        if (nodeCount == 0 || !m_NodeIndexMap) return;

        // channel map (nodeIdx -> channel)
        std::vector<const aiNodeAnim*> chOfNode(nodeCount, nullptr);
        if (anim) {
            for (unsigned ci = 0; ci < anim->mNumChannels; ++ci) {
                const aiNodeAnim* ch = anim->mChannels[ci];
                if (!ch) continue;
                auto it = m_NodeIndexMap->find(ch->mNodeName.C_Str());
                if (it == m_NodeIndexMap->end()) continue;
                const int idx = it->second;
                if (idx >= 0 && (size_t)idx < chOfNode.size()) chOfNode[(size_t)idx] = ch;
            }
        }

        // local matrices
        std::vector<XMMATRIX> locals(nodeCount, XMMatrixIdentity());
        const double tps = (anim && anim->mTicksPerSecond != 0.0) ? anim->mTicksPerSecond : 25.0;
        const double dur = (anim && anim->mDuration > 0.0) ? anim->mDuration : 0.0;
        double tTicks = (double)timeSec * tps;
        if (dur > 0.0) {
            tTicks = std::fmod(tTicks, dur);
            if (tTicks < 0.0) tTicks += dur;
        }

        for (size_t i = 0; i < nodeCount; ++i) {
            const aiNode* node = m_NodePtrs[i];
            if (!node) {
                locals[i] = XMMatrixIdentity();
                continue;
            }
            const aiNodeAnim* ch = (i < chOfNode.size()) ? chOfNode[i] : nullptr;
            if (anim && ch) {
                const aiVector3D S = InterpVecFbx(ch->mScalingKeys, ch->mNumScalingKeys, tTicks, aiVector3D(1, 1, 1));
                const aiVector3D T = InterpVecFbx(ch->mPositionKeys, ch->mNumPositionKeys, tTicks, aiVector3D(0, 0, 0));
                const aiQuaternion R = InterpQuatFbx(ch->mRotationKeys, ch->mNumRotationKeys, tTicks, aiQuaternion());

                aiMatrix4x4 mS; mS.Scaling(S, mS);
                aiMatrix4x4 mR = aiMatrix4x4(R.GetMatrix());
                aiMatrix4x4 mT; mT.Translation(T, mT);
                const aiMatrix4x4 mLocal = mT * mR * mS; // TRS (FbxAnimation과 동일)
                locals[i] = AiToXM(mLocal);
            }
            else {
                // 채널이 없으면 bind pose 원본 로컬 행렬을 그대로 사용
                locals[i] = AiToXM(node->mTransformation);
            }
        }

        // globals = parent * local (memoization)
        m_GlobalMatrices.assign(nodeCount, XMMatrixIdentity());
        std::vector<uint8_t> done(nodeCount, 0);
        auto computeNode = [&](auto&& self, int idx) -> void {
            if (idx < 0 || (size_t)idx >= nodeCount) return;
            if (done[(size_t)idx]) return;
            const int pi = m_NodeParents[(size_t)idx];
            if (pi >= 0) self(self, pi);
            const XMMATRIX parent = (pi >= 0 && (size_t)pi < nodeCount) ? m_GlobalMatrices[(size_t)pi] : XMMatrixIdentity();
            m_GlobalMatrices[(size_t)idx] = parent * locals[(size_t)idx];
            done[(size_t)idx] = 1;
        };
        for (int i = 0; i < (int)nodeCount; ++i) computeNode(computeNode, i);
    }

    // 계층 구조 빌드
    void BuildNodeHierarchy(const aiNode* node, int parentIdx) {
        if (!node || !m_NodeIndexMap) return;

        // NOTE:
        // - nodeIndexMap에 일부 중간 노드가 누락된 경우라도 서브트리를 끊지 않는다.
        // - "가장 가까운 유효 parent"를 자식에게 전달해 계층이 유지되게 한다.
        int idx = -1;
        auto it = m_NodeIndexMap->find(node->mName.C_Str());
        if (it != m_NodeIndexMap->end()) {
            idx = it->second;
            if (idx >= 0 && idx < (int)m_NodePtrs.size()) {
                m_NodePtrs[(size_t)idx] = node;
                m_NodeParents[(size_t)idx] = parentIdx;
                m_NodeNames[(size_t)idx] = node->mName.C_Str();
            }
        }

        const int parentForChildren = (idx >= 0) ? idx : parentIdx;
        for (unsigned i = 0; i < node->mNumChildren; ++i) {
            BuildNodeHierarchy(node->mChildren[i], parentForChildren);
        }
    }

    // 글로벌 행렬 계산 (재귀 + 메모이제이션)
    void ComputeGlobalMatrix(int idx, std::vector<bool>& calculated) {
        if (idx < 0 || idx >= (int)m_GlobalMatrices.size() || calculated[idx]) return;

        int pIdx = m_NodeParents[idx];
        if (pIdx != -1) {
            ComputeGlobalMatrix(pIdx, calculated); // 부모 먼저 계산
        }

        XMMATRIX local = m_LocalSRTs[idx].ToMatrix(); // local = T * R * S
        XMMATRIX parent = (pIdx != -1) ? m_GlobalMatrices[pIdx] : XMMatrixIdentity();

        // FbxAnimation과 동일: ChildGlobal = ParentGlobal * ChildLocal
        m_GlobalMatrices[idx] = parent * local;
        calculated[idx] = true;
    }

    // 애니메이션에서 노드의 SRT 추출 (키프레임 보간 포함)
    TransformSRT GetNodeSRT(const aiAnimation* anim, int nodeIndex, float time) {
        // 1. 애니메이션이 없거나 채널이 없으면? -> Bind Pose(기본 자세) 반환 [중요!]
        // (이게 없으면 캐릭터가 원점으로 뭉개짐)
        if (!anim) return GetBindPoseSRT(nodeIndex);

        const aiNodeAnim* ch = FindChannel(anim, m_NodeNames[nodeIndex]);
        if (!ch) return GetBindPoseSRT(nodeIndex);

        // 2. 시간 계산
        double tps = (anim->mTicksPerSecond != 0) ? anim->mTicksPerSecond : 25.0;
        double ticks = time * tps;
        if (anim->mDuration > 0) ticks = fmod(ticks, anim->mDuration);

        // 3. 키프레임 보간
        TransformSRT srt;
        aiVector3D S = InterpScale(ch, ticks);
        aiQuaternion R = InterpRot(ch, ticks);
        aiVector3D T = InterpPos(ch, ticks);

        srt.S = XMVectorSet(S.x, S.y, S.z, 0.f);
        srt.R = XMVectorSet(R.x, R.y, R.z, R.w);
        srt.T = XMVectorSet(T.x, T.y, T.z, 1.f);
        return srt;
    }

    // Bind Pose (기본 트랜스폼) 가져오기
    TransformSRT GetBindPoseSRT(int nodeIndex) {
        TransformSRT srt;
        if (nodeIndex >= 0 && m_NodePtrs[nodeIndex]) {
            aiVector3D s, t; aiQuaternion r;
            m_NodePtrs[nodeIndex]->mTransformation.Decompose(s, r, t);
            srt.S = XMVectorSet(s.x, s.y, s.z, 0.f);
            srt.R = XMVectorSet(r.x, r.y, r.z, r.w);
            srt.T = XMVectorSet(t.x, t.y, t.z, 1.f);
        }
        return srt;
    }

    const aiNodeAnim* FindChannel(const aiAnimation* anim, const std::string& name) {
        for (unsigned i = 0; i < anim->mNumChannels; ++i) {
            if (std::string(anim->mChannels[i]->mNodeName.C_Str()) == name)
                return anim->mChannels[i];
        }
        return nullptr;
    }

    // 보간 헬퍼 함수들
    aiVector3D InterpPos(const aiNodeAnim* ch, double t) {
        if (ch->mNumPositionKeys == 0) return aiVector3D(0, 0, 0);
        if (ch->mNumPositionKeys == 1) return ch->mPositionKeys[0].mValue;

        unsigned int i = 0;
        for (; i < ch->mNumPositionKeys - 1; i++)
            if (t < ch->mPositionKeys[i + 1].mTime) break;

        const auto& k1 = ch->mPositionKeys[i];
        const auto& k2 = ch->mPositionKeys[(i + 1) % ch->mNumPositionKeys];
        float dt = (float)(k2.mTime - k1.mTime);
        float alpha = (dt > 0) ? (float)((t - k1.mTime) / dt) : 0.0f;
        return k1.mValue + (k2.mValue - k1.mValue) * alpha;
    }

    aiQuaternion InterpRot(const aiNodeAnim* ch, double t) {
        if (ch->mNumRotationKeys == 0) return aiQuaternion(1, 0, 0, 0);
        if (ch->mNumRotationKeys == 1) return ch->mRotationKeys[0].mValue;

        unsigned int i = 0;
        for (; i < ch->mNumRotationKeys - 1; i++)
            if (t < ch->mRotationKeys[i + 1].mTime) break;

        const auto& k1 = ch->mRotationKeys[i];
        const auto& k2 = ch->mRotationKeys[(i + 1) % ch->mNumRotationKeys];
        float dt = (float)(k2.mTime - k1.mTime);
        float alpha = (dt > 0) ? (float)((t - k1.mTime) / dt) : 0.0f;
        aiQuaternion out;
        aiQuaternion::Interpolate(out, k1.mValue, k2.mValue, alpha);
        return out.Normalize();
    }

    aiVector3D InterpScale(const aiNodeAnim* ch, double t) {
        if (ch->mNumScalingKeys == 0) return aiVector3D(1, 1, 1);
        if (ch->mNumScalingKeys == 1) return ch->mScalingKeys[0].mValue;

        unsigned int i = 0;
        for (; i < ch->mNumScalingKeys - 1; i++)
            if (t < ch->mScalingKeys[i + 1].mTime) break;

        const auto& k1 = ch->mScalingKeys[i];
        const auto& k2 = ch->mScalingKeys[(i + 1) % ch->mNumScalingKeys];
        float dt = (float)(k2.mTime - k1.mTime);
        float alpha = (dt > 0) ? (float)((t - k1.mTime) / dt) : 0.0f;
        return k1.mValue + (k2.mValue - k1.mValue) * alpha;
    }

    // 마스크 (상체 판별)
    bool IsUpperBody(const std::string& name) {
        const char* keys[] = { "Spine", "Neck", "Head", "Arm", "Hand", "Weapon" };
        for (auto k : keys) if (name.find(k) != std::string::npos) return true;
        return false;
    }

    // 절차적 노이즈 (흔들림)
    void ApplyProceduralNoise(TransformSRT& srt, int idx, float time, float strength, uint32_t seed) {
        auto Hash = [](uint32_t x) { x ^= x << 13; x ^= x >> 17; return (float)(x & 0xFFFF) / 65536.0f; };
        uint32_t h = (uint32_t)idx * 12345 + seed;

        float rx = sinf(time * 10.0f + Hash(h) * 6.28f) * strength * 0.05f;
        float ry = sinf(time * 7.0f + Hash(h + 1) * 6.28f) * strength * 0.05f;
        float rz = sinf(time * 13.0f + Hash(h + 2) * 6.28f) * strength * 0.05f;

        XMVECTOR deltaR = XMQuaternionRotationRollPitchYaw(rx, ry, rz);
        srt.R = XMQuaternionMultiply(deltaR, srt.R);
    }

    // IK Solver (CCD)
    void SolveIK_CCD(const std::string& tipName, XMVECTOR target, int chainLen, float weight) {
        if (!m_NodeIndexMap->count(tipName)) return;
        int tipIdx = m_NodeIndexMap->at(tipName);

        // 체인 구성
        std::vector<int> chain;
        int curr = tipIdx;
        for (int i = 0; i <= chainLen && curr != -1; ++i) {
            chain.push_back(curr);
            curr = m_NodeParents[curr];
        }
        if (chain.empty()) return;

        // IK 반복 (5회)
        for (int iter = 0; iter < 5; ++iter) {
            // 현재 상태의 글로벌 행렬 갱신 (IK 계산을 위해)
            // 성능 최적화: 전체 노드가 아닌 IK 체인 관련 노드만 갱신하는 것이 좋음
            // 여기선 간단하게 전체 부모 관계가 갱신되었다고 가정하거나, 필요 시 여기서 재계산
            std::vector<bool> calc(m_NodePtrs.size(), false);
            for (int node : chain) ComputeGlobalMatrix(node, calc); // 최소한 체인은 갱신

            XMVECTOR effectorPos = m_GlobalMatrices[tipIdx].r[3];

            // 루트부터가 아닌, Tip의 부모부터 역순으로 처리 (Tip -> Parent -> ...)
            for (size_t i = 1; i < chain.size(); ++i) {
                int jointIdx = chain[i];
                ComputeGlobalMatrix(jointIdx, calc); // 안전장치

                XMVECTOR jointPos = m_GlobalMatrices[jointIdx].r[3];
                XMVECTOR toEffector = XMVector3Normalize(XMVectorSubtract(effectorPos, jointPos));
                XMVECTOR toTarget = XMVector3Normalize(XMVectorSubtract(target, jointPos));

                float dot = XMVectorGetX(XMVector3Dot(toEffector, toTarget));
                if (dot > 0.999f) continue;

                // 회전 축과 각도
                XMVECTOR axis = XMVector3Cross(toEffector, toTarget);
                axis = XMVector3Normalize(axis);
                float angle = acosf(std::clamp(dot, -1.0f, 1.0f)) * weight;

                // 글로벌 회전을 로컬 회전으로 변환
                // (부모의 역회전을 곱해야 함 - 간략화: 현재 로컬에 델타 곱하기)
                // 정확하게 하려면: JointWorldRot * Delta * Inverse(JointWorldRot) ... 복잡함
                // 여기서는 로컬 축 기준으로 가정하고 단순 적용
                XMVECTOR qDelta = XMQuaternionRotationAxis(axis, angle);

                // 부모 공간으로 축 변환 (간이 IK)
                // 실제론 World Axis를 Local Space로 가져와야 함:
                // LocalAxis = XMVector3TransformNormal(axis, XMMatrixInverse(nullptr, ParentGlobal))
                int pIdx = m_NodeParents[jointIdx];
                if (pIdx != -1) {
                    XMMATRIX mInvP = XMMatrixInverse(nullptr, m_GlobalMatrices[pIdx]);
                    XMVECTOR localAxis = XMVector3TransformNormal(axis, mInvP);
                    qDelta = XMQuaternionRotationAxis(localAxis, angle);
                }

                m_LocalSRTs[jointIdx].R = XMQuaternionMultiply(m_LocalSRTs[jointIdx].R, qDelta);
                m_LocalSRTs[jointIdx].R = XMQuaternionNormalize(m_LocalSRTs[jointIdx].R);

                // 이펙터 위치 갱신을 위해 다음 반복 전 글로벌 행렬 재계산 필요
                // 여기선 생략 (다음 이터레이션 혹은 프레임에 반영)
            }
        }
    }
};