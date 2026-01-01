#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <DirectXMath.h>
#include <assimp/scene.h> 

using namespace DirectX;

// =========================================================
// [기본 구조체] TransformSRT
// 스켈레톤 쪽은 Assimp 채널과 동일한 관례(Translate -> Rotate -> Scale)를 그대로 쓴다.
// (기존 `FbxAnimation.cpp`의 로컬 행렬 합성과 동일하게 맞춰야 GPU 스키닝 팔레트가 깨지지 않는다)
// =========================================================
struct TransformSRT {
    XMVECTOR S{ 1,1,1,0 };     // Scale (크기)
    XMVECTOR R{ 0,0,0,1 };     // Rotation (회전 - Quaternion)
    XMVECTOR T{ 0,0,0,1 };     // Translation (이동)

    // TRS -> 행렬 변환
    // (FbxAnimation과 일치: mLocal = mT * mR * mS)
    XMMATRIX ToMatrix() const {
        const XMMATRIX mS = XMMatrixScalingFromVector(S);
        const XMMATRIX mR = XMMatrixRotationQuaternion(R);
        const XMMATRIX mT = XMMatrixTranslationFromVector(T);
        return mT * mR * mS;
    }

    // 두 포즈 선형 보간 (Transition Blending)
    static TransformSRT Lerp(const TransformSRT& a, const TransformSRT& b, float t) {
        TransformSRT result;
        result.S = XMVectorLerp(a.S, b.S, t);
        result.R = XMQuaternionSlerp(a.R, b.R, t); // 회전은 구면 보간
        result.T = XMVectorLerp(a.T, b.T, t);
        return result;
    }

    // 애디티브 연산: (타겟 - 베이스) + 현재
    static TransformSRT Additive(const TransformSRT& current, const TransformSRT& add, const TransformSRT& base, float alpha) {
        if (alpha <= 0.001f) return current;
        
        // 1. 차이(Delta) 계산
        XMVECTOR deltaT = XMVectorSubtract(add.T, base.T);
        // 쿼터니언 차이: Add * Inverse(Base)
        XMVECTOR invBaseR = XMQuaternionInverse(base.R);
        XMVECTOR deltaR = XMQuaternionMultiply(add.R, invBaseR);

        // 2. 현재 상태에 차이 적용 (알파값으로 강도 조절)
        TransformSRT result;
        result.S = current.S; // 스케일은 보통 애디티브하지 않음 (필요시 추가)
        
        // 위치: 현재 + (델타 * 강도)
        result.T = XMVectorAdd(current.T, XMVectorScale(deltaT, alpha));
        
        // 회전: (델타 * 강도) * 현재
        XMVECTOR weightedDeltaR = XMQuaternionSlerp(XMQuaternionIdentity(), deltaR, alpha);
        result.R = XMQuaternionMultiply(weightedDeltaR, current.R);

        return result;
    }
};

// =========================================================
// [소켓 시스템] 언리얼 스타일
// =========================================================
struct Socket {
    std::string name;           // 소켓 이름 (예: "Muzzle")
    std::string parentBoneName; // 부착될 본 이름 (예: "Hand_R")
    int parentBoneIndex = -1;   // 본 인덱스 캐시
    XMMATRIX offsetMatrix;      // 소켓의 로컬 오프셋 (SRT)
    XMMATRIX finalWorldMatrix;  // 계산된 최종 월드 행렬

    // ImGui 편집용 (offsetMatrix를 언제든 재생성 가능하게 값도 들고 있는다)
    XMFLOAT3 offsetPos = { 0,0,0 };
    XMFLOAT3 offsetRotDeg = { 0,0,0 };
    XMFLOAT3 offsetScale = { 1,1,1 };
 // node 계층 기준 parent(노드 인덱스). boneNames에 없어도(메시에 가중치가 안 걸려도) 붙일 수 있게.
 int parentNodeIndex = -1;
};

// =========================================================
// [고급 애니메이터] 캐릭터 전용 컨트롤러
// =========================================================
class CharacterAnimator {
public:
    // 외부(앱)에서 애니메이션 포인터를 안전하게 가져오기 위한 도우미용
    const aiScene* scene = nullptr;
    const std::unordered_map<std::string, int>* nodeIndexOfName = nullptr;
    XMFLOAT4X4 globalInverseF = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

    std::vector<std::string> boneNames;
 std::vector<int> boneParents;      // (legacy) 본-리스트 기준 부모. 이제 사용하지 않음(중간 노드 누락 가능)
    std::vector<XMMATRIX> boneOffsets; // 바인드 포즈 역행렬
    std::unordered_map<std::string, int> boneIndexMap;
    std::vector<int> boneNodeIndices; // boneNames -> nodeIndexOfName
    
    // GPU 업로드용 최종 행렬
    std::vector<XMMATRIX> finalTransforms; 
    // 소켓 계산/IK 등에 쓰기 위한 글로벌(노드) 행렬 캐시 (boneNames 기준, Off 미적용)
    std::vector<XMMATRIX> boneGlobals; 

 // ===== 노드 전체 계층(=FbxAnimation::EvaluateGlobals와 동일한 의미) =====
 // nodeIndexOfName.size()에 정렬된 캐시들. "중간 노드 스케일/축"을 반드시 포함해야 함.
 std::vector<const aiNode*> nodePtrByIndex;
 std::vector<int> nodeParentByIndex;
 std::vector<std::string> nodeNameByIndex;
    
    // 소켓 목록
    std::vector<Socket> sockets;

    // 초기화
    void Initialize(
        const aiScene* inScene,
        const std::unordered_map<std::string, int>& inNodeIndexOfName,
        const XMFLOAT4X4& inGlobalInverse,
        const std::vector<std::string>& inBoneNames,
        const std::vector<DirectX::XMFLOAT4X4>& inBoneOffsets)
    {
        scene = inScene;
        nodeIndexOfName = &inNodeIndexOfName;
        globalInverseF = inGlobalInverse;

        boneNames = inBoneNames;
        const size_t count = boneNames.size();
        boneParents.assign(count, -1);
        boneOffsets.resize(count);
        finalTransforms.assign(count, XMMatrixIdentity());
        boneGlobals.assign(count, XMMatrixIdentity());
        boneNodeIndices.assign(count, -1);
        boneIndexMap.clear();
        boneIndexMap.reserve(count);

        // 1) 이름 -> boneIndex 맵을 먼저 완성
        for (size_t i = 0; i < count; ++i)
            boneIndexMap[boneNames[i]] = (int)i;

        // 2) 오프셋/부모/노드 인덱스 설정
        for (size_t i = 0; i < count; ++i) {
            boneOffsets[i] = XMLoadFloat4x4(&inBoneOffsets[i]);

            auto itNode = inNodeIndexOfName.find(boneNames[i]);
            boneNodeIndices[i] = (itNode != inNodeIndexOfName.end()) ? itNode->second : -1;

            // 부모 본 인덱스: Assimp 노드의 부모가 "본 리스트"에 포함된 경우만 연결
            if (inScene && inScene->mRootNode) {
                aiNode* node = inScene->mRootNode->FindNode(boneNames[i].c_str());
                if (node && node->mParent) {
                    const std::string pName = node->mParent->mName.C_Str();
                    auto itB = boneIndexMap.find(pName);
                    boneParents[i] = (itB != boneIndexMap.end()) ? itB->second : -1;
                }
            }
        }

        // 3) 노드 전체 계층 캐시 구축 (nodeIndexOfName 인덱스 정렬)
        nodePtrByIndex.clear();
        nodeParentByIndex.clear();
        nodeNameByIndex.clear();
        nodePtrByIndex.resize(inNodeIndexOfName.size(), nullptr);
        nodeParentByIndex.resize(inNodeIndexOfName.size(), -1);
        nodeNameByIndex.resize(inNodeIndexOfName.size());
        if (inScene && inScene->mRootNode) {
            std::function<void(const aiNode*, int)> build = [&](const aiNode* node, int parentIdx) {
                auto it = inNodeIndexOfName.find(node->mName.C_Str());
                const int idx = (it != inNodeIndexOfName.end()) ? it->second : -1;
                if (idx >= 0 && (size_t)idx < nodePtrByIndex.size()) {
                    nodePtrByIndex[(size_t)idx] = node;
                    nodeParentByIndex[(size_t)idx] = parentIdx;
                    nodeNameByIndex[(size_t)idx] = node->mName.C_Str();
                }
                for (unsigned ci = 0; ci < node->mNumChildren; ++ci) {
                    build(node->mChildren[ci], idx);
                }
            };
            build(inScene->mRootNode, -1);
        }
    }

    // 소켓 오프셋 행렬 재계산 (ImGui 수정 후 호출)
    static XMMATRIX BuildOffsetMatrix_SRT(const XMFLOAT3& pos, const XMFLOAT3& rotDeg, const XMFLOAT3& scale) {
        const XMMATRIX mS = XMMatrixScaling(scale.x, scale.y, scale.z);
        const XMMATRIX mR = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotDeg.x),
            XMConvertToRadians(rotDeg.y),
            XMConvertToRadians(rotDeg.z));
        const XMMATRIX mT = XMMatrixTranslation(pos.x, pos.y, pos.z);
        return mS * mR * mT;
    }

    // 소켓 추가
    void AddSocket(std::string name, std::string parentBone, XMFLOAT3 pos, XMFLOAT3 rotDeg, XMFLOAT3 scale) {
        Socket s;
        s.name = name;
        s.parentBoneName = parentBone;
        if(boneIndexMap.count(parentBone)) s.parentBoneIndex = boneIndexMap[parentBone];
        
        s.offsetPos = pos;
        s.offsetRotDeg = rotDeg;
        s.offsetScale = scale;
        // 소켓 오프셋 행렬 생성 (S * R * T) - UI에서 직관적
        s.offsetMatrix = BuildOffsetMatrix_SRT(pos, rotDeg, scale);
        
        sockets.push_back(s);
    }

    // 소켓 편집(없으면 생성)
    Socket* FindSocket(const std::string& socketName) {
        for (auto& s : sockets) if (s.name == socketName) return &s;
        return nullptr;
    }
    void SetSocketSRT(const std::string& socketName, const std::string& parentBone, const XMFLOAT3& pos, const XMFLOAT3& rotDeg, const XMFLOAT3& scale) {
        Socket* s = FindSocket(socketName);
        if (!s) {
            AddSocket(socketName, parentBone, pos, rotDeg, scale);
            return;
        }
        s->parentBoneName = parentBone;
        auto it = boneIndexMap.find(parentBone);
        s->parentBoneIndex = (it != boneIndexMap.end()) ? it->second : -1;
        // node 인덱스는 boneNames에 없어도 찾을 수 있어야 함
        if (nodeIndexOfName) {
            auto itN = nodeIndexOfName->find(parentBone);
            s->parentNodeIndex = (itN != nodeIndexOfName->end()) ? itN->second : -1;
        }
        s->offsetPos = pos;
        s->offsetRotDeg = rotDeg;
        s->offsetScale = scale;
        s->offsetMatrix = BuildOffsetMatrix_SRT(pos, rotDeg, scale);
    }

    // =========================================================
    // [메인 업데이트] 4가지 기술 통합 구현
    // =========================================================
    void UpdateAnimation(
        float dt, 
        const aiAnimation* animA, float timeA,
        const aiAnimation* animB, float timeB,
        float blendFactor,
        const aiAnimation* upperAnim = nullptr, float timeUpper = 0.f,
        const aiAnimation* addAnim = nullptr, float timeAdd = 0.f,
        const aiAnimation* refAnim = nullptr,
        // ---- Procedural Additive (랜덤 반동/흔들림 등) ----
        float proceduralAdditiveAlpha = 0.0f,
        uint32_t proceduralSeed = 0,
        // ---- IK (Reload 등에서 손을 목표로 붙이기) ----
        bool enableIK = false,
        const char* ikTipBoneName = nullptr,
        int ikChainLength = 0,
        XMVECTOR ikTargetModelSpace = XMVectorZero(),
        float ikWeight = 1.0f
    ) {
        if (!scene || !nodeIndexOfName) return;

        // =========================================================
        // 핵심 수정:
        // - FbxAnimation은 "노드 전체 계층"을 기준으로 글로벌을 만든다.
        // - 채널이 없으면 node->mTransformation(기본 로컬)을 사용한다.
        // - 이 기본 로컬에는 root 스케일/축 보정이 들어있을 수 있으므로, 여기서도 반드시 동일하게 해야 함.
        // =========================================================
        const size_t nodeCount = nodePtrByIndex.size();
        if (nodeCount == 0) return;

        auto BuildChannelMap = [&](const aiAnimation* anim, std::vector<const aiNodeAnim*>& out) {
            out.assign(nodeCount, nullptr);
            if (!anim) return;
            for (unsigned i = 0; i < anim->mNumChannels; ++i) {
                const aiNodeAnim* ch = anim->mChannels[i];
                auto it = nodeIndexOfName->find(ch->mNodeName.C_Str());
                if (it != nodeIndexOfName->end()) {
                    const int idx = it->second;
                    if (idx >= 0 && (size_t)idx < out.size()) out[(size_t)idx] = ch;
                }
            }
        };

        std::vector<const aiNodeAnim*> chA, chB, chUpper, chAdd, chRef;
        BuildChannelMap(animA, chA);
        BuildChannelMap(animB, chB);
        BuildChannelMap(upperAnim, chUpper);
        BuildChannelMap(addAnim, chAdd);
        BuildChannelMap(refAnim, chRef);

        auto MatrixToSRT = [&](const aiMatrix4x4& m) -> TransformSRT {
            XMFLOAT4X4 fm;
            fm._11 = (float)m.a1; fm._12 = (float)m.a2; fm._13 = (float)m.a3; fm._14 = (float)m.a4;
            fm._21 = (float)m.b1; fm._22 = (float)m.b2; fm._23 = (float)m.b3; fm._24 = (float)m.b4;
            fm._31 = (float)m.c1; fm._32 = (float)m.c2; fm._33 = (float)m.c3; fm._34 = (float)m.c4;
            fm._41 = (float)m.d1; fm._42 = (float)m.d2; fm._43 = (float)m.d3; fm._44 = (float)m.d4;
            const XMMATRIX M = XMLoadFloat4x4(&fm);
            XMVECTOR S, R, T;
            TransformSRT s{};
            if (XMMatrixDecompose(&S, &R, &T, M)) {
                s.S = S; s.R = R; s.T = T;
            }
            return s;
        };

        auto SampleNodeSRT = [&](const aiAnimation* anim, const std::vector<const aiNodeAnim*>& chMap, int nodeIdx, float timeSec) -> TransformSRT {
            // 채널이 없으면 node->mTransformation(기본 로컬)
            const aiNode* node = (nodeIdx >= 0 && (size_t)nodeIdx < nodePtrByIndex.size()) ? nodePtrByIndex[(size_t)nodeIdx] : nullptr;
            if (!anim || nodeIdx < 0 || (size_t)nodeIdx >= chMap.size() || !chMap[(size_t)nodeIdx] || !node) {
                return node ? MatrixToSRT(node->mTransformation) : TransformSRT{};
            }
            const aiNodeAnim* ch = chMap[(size_t)nodeIdx];
            const double tps = (anim->mTicksPerSecond != 0.0) ? anim->mTicksPerSecond : 25.0;
            const double dur = (anim->mDuration > 0.0) ? anim->mDuration : 0.0;
            double tTicks = (double)timeSec * tps;
            if (dur > 0.0) {
                tTicks = std::fmod(tTicks, dur);
                if (tTicks < 0.0) tTicks += dur;
            }
            const aiVector3D S = (ch->mNumScalingKeys > 0) ? InterpVec(ch->mScalingKeys, ch->mNumScalingKeys, tTicks) : aiVector3D(1, 1, 1);
            const aiVector3D T = (ch->mNumPositionKeys > 0) ? InterpVec(ch->mPositionKeys, ch->mNumPositionKeys, tTicks) : aiVector3D(0, 0, 0);
            const aiQuaternion R = (ch->mNumRotationKeys > 0) ? InterpQuat(ch->mRotationKeys, ch->mNumRotationKeys, tTicks) : aiQuaternion();
            TransformSRT s{};
            s.S = XMVectorSet(S.x, S.y, S.z, 0.0f);
            s.T = XMVectorSet(T.x, T.y, T.z, 1.0f);
            s.R = XMVectorSet(R.x, R.y, R.z, R.w);
            return s;
        };

        // 1) 로컬 SRT 계산 (노드 전체 기준)
        std::vector<TransformSRT> localSRTs(nodeCount);
        for (int idx = 0; idx < (int)nodeCount; ++idx) {
            const std::string& nName = nodeNameByIndex[(size_t)idx];
            const TransformSRT srtA = SampleNodeSRT(animA, chA, idx, timeA);
            const TransformSRT srtB = SampleNodeSRT(animB, chB, idx, timeB);

            TransformSRT finalSRT = TransformSRT::Lerp(srtA, srtB, blendFactor);

            // Layer mask: 문자열 기반 간이 마스크 (추후 UI에서 mask 목록 지정 가능)
            const bool isUpper =
                (nName.find("Spine") != std::string::npos) ||
                (nName.find("Chest") != std::string::npos) ||
                (nName.find("Neck") != std::string::npos) ||
                (nName.find("Head") != std::string::npos) ||
                (nName.find("Arm") != std::string::npos) ||
                (nName.find("Hand") != std::string::npos) ||
                (nName.find("Shoulder") != std::string::npos);

            if (isUpper && upperAnim) {
                const TransformSRT srtUpper = SampleNodeSRT(upperAnim, chUpper, idx, timeUpper);
                finalSRT = TransformSRT::Lerp(finalSRT, srtUpper, 0.9f);
            }

            // Additive
            if (addAnim && refAnim) {
                const TransformSRT srtAdd = SampleNodeSRT(addAnim, chAdd, idx, timeAdd);
                const TransformSRT srtRef = SampleNodeSRT(refAnim, chRef, idx, 0.0f);
                finalSRT = TransformSRT::Additive(finalSRT, srtAdd, srtRef, 1.0f);
            }

            localSRTs[(size_t)idx] = finalSRT;
        }

        // ---- Procedural Additive: 간단 랜덤 흔들림(상체/팔/머리 위주) ----
        // - 실제 게임에서는 Shoot 애니메이션을 Additive로 쓰는 게 더 정교하지만,
        //   여기서는 "즉시 결과 확인"을 위해 노이즈 기반의 회전 델타를 추가한다.
        if (proceduralAdditiveAlpha > 0.0f) {
            auto Hash01 = [](uint32_t x) -> float {
                // xorshift32
                x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                // 0..1
                return (float)(x & 0x00FFFFFF) / (float)0x01000000;
            };
            const float a = (proceduralAdditiveAlpha > 1.0f) ? 1.0f : proceduralAdditiveAlpha;
            for (size_t i = 0; i < nodeCount; ++i) {
                const std::string& bn = nodeNameByIndex[i];
                const bool upperLike =
                    (bn.find("Spine") != std::string::npos) ||
                    (bn.find("Head") != std::string::npos) ||
                    (bn.find("Neck") != std::string::npos) ||
                    (bn.find("Arm") != std::string::npos) ||
                    (bn.find("Hand") != std::string::npos);
                if (!upperLike) continue;

                // boneIndex + seed로 고정된 위상/주파수 부여
                uint32_t h = (uint32_t)i * 747796405u + proceduralSeed * 2891336453u + 1u;
                float p0 = Hash01(h) * XM_2PI;
                float p1 = Hash01(h ^ 0xA3C59AC3u) * XM_2PI;
                float p2 = Hash01(h ^ 0x1B56C4E9u) * XM_2PI;

                // 작은 각도(라디안). alpha로 강도 조절.
                const float rx = std::sinf((timeA * 11.0f) + p0) * (0.02f * a);
                const float ry = std::sinf((timeA * 17.0f) + p1) * (0.02f * a);
                const float rz = std::sinf((timeA * 23.0f) + p2) * (0.02f * a);

                XMVECTOR qDelta = XMQuaternionRotationRollPitchYaw(rx, ry, rz);
                localSRTs[i].R = XMQuaternionNormalize(XMQuaternionMultiply(qDelta, localSRTs[i].R));
            }
        }

        // ---- IK (CCD) ----
        // - 타겟은 "캐릭터 모델 로컬 공간"으로 받는다 (aliceWorld 역행렬을 곱해 만든 공간)
        if (enableIK && ikTipBoneName && ikChainLength > 0 && ikWeight > 0.0f) {
            // IK 대상은 boneNames에 없을 수 있으니 nodeIndexOfName에서 먼저 찾는다.
            auto itNode = nodeIndexOfName->find(std::string(ikTipBoneName));
            if (itNode != nodeIndexOfName->end()) {
                SolveIK_CCD(localSRTs, itNode->second, ikChainLength, ikTargetModelSpace, ikWeight);
            }
        }

        // 2) 글로벌 행렬 계층 구조 계산 (FbxAnimation::EvaluateGlobals와 동일: parent * local)
        std::vector<XMMATRIX> globals(nodeCount, XMMatrixIdentity());
        std::vector<uint8_t> done(nodeCount, 0);
        auto computeNode = [&](auto&& self, int idx) -> void {
            if (idx < 0 || (size_t)idx >= nodeCount) return;
            if (done[(size_t)idx]) return;
            const int pi = nodeParentByIndex[(size_t)idx];
            if (pi >= 0) self(self, pi);
            const XMMATRIX L = localSRTs[(size_t)idx].ToMatrix(); // TRS
            const XMMATRIX P = (pi >= 0) ? globals[(size_t)pi] : XMMatrixIdentity();
            globals[(size_t)idx] = P * L;
            done[(size_t)idx] = 1;
        };
        // bone 노드만 계산해도 되지만, IK/소켓 안정성을 위해 전체를 한 번에 계산
        for (int i = 0; i < (int)nodeCount; ++i) computeNode(computeNode, i);

        // 3) 최종 스키닝 행렬 (Gi * G * Off) - 기존 FbxAnimation과 동일
        const XMMATRIX Gi = XMLoadFloat4x4(&globalInverseF);
        for(size_t bi=0; bi<boneNames.size(); ++bi) {
            const int nodeIdx = boneNodeIndices[bi];
            const XMMATRIX G = (nodeIdx >= 0 && (size_t)nodeIdx < globals.size()) ? globals[(size_t)nodeIdx] : XMMatrixIdentity();
            boneGlobals[bi] = G;
            finalTransforms[bi] = (Gi * G) * boneOffsets[bi];
        }

        // 4) 소켓 업데이트 (캐릭터 기준 월드의 직전 단계: 본 글로벌 기준)
        for(auto& socket : sockets) {
            int nodeIdx = socket.parentNodeIndex;
            if (nodeIdx < 0 && socket.parentBoneIndex >= 0 && (size_t)socket.parentBoneIndex < boneNodeIndices.size())
                nodeIdx = boneNodeIndices[(size_t)socket.parentBoneIndex];
            if(nodeIdx >= 0 && (size_t)nodeIdx < globals.size()) {
                socket.finalWorldMatrix = socket.offsetMatrix * globals[(size_t)nodeIdx];
            }
        }
    }

    // 소켓의 최종 월드 행렬 가져오기
    XMMATRIX GetSocketWorldMatrix(const std::string& socketName, CXMMATRIX characterWorld) {
        for(const auto& s : sockets) {
            if(s.name == socketName) {
                return s.finalWorldMatrix * characterWorld;
            }
        }
        return characterWorld; 
    }

private:
    static aiVector3D InterpVec(const aiVectorKey* keys, unsigned count, double tTicks)
    {
        if (count == 0) return aiVector3D(0, 0, 0);
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
    static aiQuaternion InterpQuat(const aiQuatKey* keys, unsigned count, double tTicks)
    {
        if (count == 0) return aiQuaternion();
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

    // Assimp 채널에서 시간(timeSec)에 해당하는 키프레임 SRT 추출 (보간 포함)
    TransformSRT SampleBone(const aiAnimation* anim, const std::string& name, float timeSec) {
        TransformSRT result{};
        if (!anim) return result;

        const aiNodeAnim* ch = nullptr;
        for (unsigned i = 0; i < anim->mNumChannels; ++i) {
            const aiNodeAnim* c = anim->mChannels[i];
            if (c && name == std::string(c->mNodeName.C_Str())) { ch = c; break; }
        }
        if (!ch) return result; // 채널 없으면 identity

        const double tps = (anim->mTicksPerSecond != 0.0) ? anim->mTicksPerSecond : 25.0;
        const double dur = (anim->mDuration > 0.0) ? anim->mDuration : 0.0;
        double tTicks = (double)timeSec * tps;
        if (dur > 0.0) {
            tTicks = std::fmod(tTicks, dur);
            if (tTicks < 0.0) tTicks += dur;
        }

        const aiVector3D S = (ch->mNumScalingKeys > 0) ? InterpVec(ch->mScalingKeys, ch->mNumScalingKeys, tTicks) : aiVector3D(1, 1, 1);
        const aiVector3D T = (ch->mNumPositionKeys > 0) ? InterpVec(ch->mPositionKeys, ch->mNumPositionKeys, tTicks) : aiVector3D(0, 0, 0);
        const aiQuaternion R = (ch->mNumRotationKeys > 0) ? InterpQuat(ch->mRotationKeys, ch->mNumRotationKeys, tTicks) : aiQuaternion();

        result.S = XMVectorSet(S.x, S.y, S.z, 0.0f);
        result.T = XMVectorSet(T.x, T.y, T.z, 1.0f);
        result.R = XMVectorSet(R.x, R.y, R.z, R.w);
        return result;
    }

    // 간단한 CCD IK Solver (로컬 회전만 업데이트)
    // - tipIndex: 끝(예: Hand_L)
    // - chainLength: tip부터 몇 개 부모까지 포함할지
    // - targetMS: 캐릭터 모델 로컬 공간 타겟 위치 (w=1)
    void SolveIK_CCD(std::vector<TransformSRT>& srts, int tipIndex, int chainLength, XMVECTOR targetMS, float weight) {
        if (tipIndex < 0 || (size_t)tipIndex >= srts.size()) return;
        if (chainLength <= 0) return;
        if (weight <= 0.0f) return;
        if (weight > 1.0f) weight = 1.0f;

        // 체인 구성 (tip -> root 방향)
        std::vector<int> joints;
        joints.reserve(chainLength);
        int curr = tipIndex;
        for (int i = 0; i < chainLength && curr != -1; ++i) {
            joints.push_back(curr);
            // 노드 전체 계층 기준 부모
            curr = (curr >= 0 && (size_t)curr < nodeParentByIndex.size()) ? nodeParentByIndex[(size_t)curr] : -1;
        }
        if (joints.size() < 2) return;

        auto RecomputeGlobals = [&](std::vector<XMMATRIX>& globals) {
            globals.assign(srts.size(), XMMatrixIdentity());
            for (size_t i = 0; i < srts.size(); ++i) {
                const XMMATRIX localM = srts[i].ToMatrix();
                const int parent = (i < nodeParentByIndex.size()) ? nodeParentByIndex[i] : -1;
                globals[i] = (parent != -1) ? (globals[(size_t)parent] * localM) : localM;
            }
        };

        // 반복 횟수: 너무 크면 흔들림/불안정. 우선 8회 정도.
        const int iterations = 8;
        std::vector<XMMATRIX> globals;
        for (int it = 0; it < iterations; ++it) {
            RecomputeGlobals(globals);

            const XMVECTOR tipPos = XMVector3TransformCoord(XMVectorZero(), globals[(size_t)tipIndex]);
            const XMVECTOR toTarget = XMVectorSubtract(targetMS, tipPos);
            if (XMVectorGetX(XMVector3LengthSq(toTarget)) < 1.0e-5f) break;

            // 관절을 따라가며 tip을 target로 끌어당김
            for (size_t ji = 1; ji < joints.size(); ++ji) {
                const int j = joints[ji];
                const XMMATRIX Gj = globals[(size_t)j];

                const XMVECTOR jointPos = XMVector3TransformCoord(XMVectorZero(), Gj);
                const XMVECTOR curTipPos = XMVector3TransformCoord(XMVectorZero(), globals[(size_t)tipIndex]);

                XMVECTOR v1 = XMVectorSubtract(curTipPos, jointPos);
                XMVECTOR v2 = XMVectorSubtract(targetMS, jointPos);
                const float l1 = XMVectorGetX(XMVector3Length(v1));
                const float l2 = XMVectorGetX(XMVector3Length(v2));
                if (l1 < 1.0e-5f || l2 < 1.0e-5f) continue;
                v1 = XMVectorScale(v1, 1.0f / l1);
                v2 = XMVectorScale(v2, 1.0f / l2);

                float dot = XMVectorGetX(XMVector3Dot(v1, v2));
                if (dot > 1.0f) dot = 1.0f;
                if (dot < -1.0f) dot = -1.0f;
                const float angle = std::acos(dot) * weight;
                if (angle < 1.0e-4f) continue;

                XMVECTOR axisWS = XMVector3Cross(v1, v2);
                if (XMVectorGetX(XMVector3LengthSq(axisWS)) < 1.0e-8f) continue;
                axisWS = XMVector3Normalize(axisWS);

                // 월드 축을 조인트 로컬 축으로 변환 (번역 제거 + 역행렬)
                XMMATRIX GjNoT = Gj; GjNoT.r[3] = XMVectorSet(0, 0, 0, 1);
                XMMATRIX invGj = XMMatrixInverse(nullptr, GjNoT);
                XMVECTOR axisLS = XMVector3Normalize(XMVector3TransformNormal(axisWS, invGj));

                const XMVECTOR qDelta = XMQuaternionRotationAxis(axisLS, angle);
                srts[(size_t)j].R = XMQuaternionNormalize(XMQuaternionMultiply(qDelta, srts[(size_t)j].R));
            }
        }
    }
};