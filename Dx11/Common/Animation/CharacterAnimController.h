#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <cstdint>

#include <DirectXMath.h>
#include <assimp/scene.h>
#include <d3d11.h>

#include "Animator.h"

using namespace DirectX;

// ------------------------------------------------------------
// App.cpp가 넘겨주는 입력
// ------------------------------------------------------------
struct CharacterInputState
{
    bool move = false;
    bool run  = false;

    bool crouchTogglePressed = false; // Ctrl (one-frame)
    bool firePressed         = false; // LMB (one-frame) in Shoot_Stance
    bool reloadPressed       = false; // R   (one-frame) in Shoot_Stance
};

struct AimInputState
{
    bool  enabled = false;
    float yawRad  = 0.0f;
    float weight  = 1.0f;
};

// ------------------------------------------------------------
// 크로스페이드 파라미터(ExitTime + EntryOffset + SmoothStep)
// ------------------------------------------------------------
struct CrossFadeParams
{
    float durationSec = 0.20f;

    bool  useExitTime = true;
    float exitNorm = 0.85f;   // from state의 normalized time이 이 값 넘어야 전환 시작
    float entryNorm = 0.00f;  // to state 시작 offset(0..1)

    bool  smoothStep = true;
};

// ------------------------------------------------------------
// 리코일/사격/IK/소켓
// ------------------------------------------------------------
struct RecoilParams
{
    bool enabled = true;
    float kick = 1.0f;
    float decay = 10.0f;
    uint32_t seed = 1337u;
};

struct ShootParams
{
    float oneShotDurationSec = 0.25f; // Shoot(원샷) 재생 길이(clip 길이를 쓰고 싶으면 0으로 두고 clip length 사용)
};

struct IKParams
{
    bool enabled = true;
    std::string tipBone = "Hand_L";
    int chainLength = 3;
    float weight = 1.0f;
};

struct SocketSRTParams
{
    std::string socketName = "WeaponPoint";
    std::string parentBone = "Hand_R";
    XMFLOAT3 pos = { 0.1f, 0.05f, 0.0f };
    XMFLOAT3 rotDeg = { 0.0f, 90.0f, 0.0f };
    XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
};

struct WeaponMagSocketParams
{
    XMFLOAT3 pos = { 0,0,0 };
    XMFLOAT3 rotDeg = { 0,0,0 };
    XMFLOAT3 scale = { 1,1,1 };
};

struct WeaponTransformOut
{
    XMFLOAT3 pos{ 0,0,0 };
    XMFLOAT3 rotDeg{ 0,0,0 };
    XMFLOAT3 scale{ 1,1,1 };
    bool valid = false;
};

// ============================================================
// 파라미터 저장소(UE/Unity Animator Parameters 비슷)
// ============================================================
class AnimParams
{
public:
    void SetBool(const std::string& k, bool v) { m_Bools[k] = v; }
    void SetFloat(const std::string& k, float v) { m_Floats[k] = v; }
    void SetInt(const std::string& k, int v) { m_Ints[k] = v; }
    void FireTrigger(const std::string& k) { m_Triggers.insert(k); }

    bool GetBool(const std::string& k, bool def = false) const
    {
        auto it = m_Bools.find(k);
        return (it != m_Bools.end()) ? it->second : def;
    }

    float GetFloat(const std::string& k, float def = 0.0f) const
    {
        auto it = m_Floats.find(k);
        return (it != m_Floats.end()) ? it->second : def;
    }

    int GetInt(const std::string& k, int def = 0) const
    {
        auto it = m_Ints.find(k);
        return (it != m_Ints.end()) ? it->second : def;
    }

    bool HasTrigger(const std::string& k) const
    {
        return m_Triggers.find(k) != m_Triggers.end();
    }

    void ClearTriggers() { m_Triggers.clear(); }

private:
    std::unordered_map<std::string, bool>  m_Bools;
    std::unordered_map<std::string, float> m_Floats;
    std::unordered_map<std::string, int>   m_Ints;
    std::unordered_set<std::string>        m_Triggers;
};

// ============================================================
// 전환 조건(간단하지만 충분히 강력)
// ============================================================
struct AnimCondition
{
    enum class Type
    {
        BoolTrue,
        BoolFalse,
        Trigger,       // 이번 프레임 트리거 발생
        FloatGreater,
        FloatLess,
        IntEquals
    };

    Type type = Type::BoolTrue;
    std::string param;
    float f = 0.0f;
    int i = 0;

    bool Eval(const AnimParams& p) const
    {
        switch (type)
        {
        case Type::BoolTrue:     return p.GetBool(param, false) == true;
        case Type::BoolFalse:    return p.GetBool(param, false) == false;
        case Type::Trigger:      return p.HasTrigger(param);
        case Type::FloatGreater: return p.GetFloat(param, 0.0f) > f;
        case Type::FloatLess:    return p.GetFloat(param, 0.0f) < f;
        case Type::IntEquals:    return p.GetInt(param, 0) == i;
        default: return false;
        }
    }
};

// ============================================================
// 상태 정의
// ============================================================
struct AnimStateDef
{
    std::string name;

    // 이 상태가 재생할 클립 키(= animIndex key)
    // 빈 문자열이면 "None(비활성)" 상태로 취급
    std::string clipKey;

    bool loop = true;
    float speed = 1.0f;

    // 0이면 clip length 사용, >0이면 고정 길이 사용(예: Shoot 0.25s)
    float forcedDurationSec = 0.0f;

    // 애니가 끝나면 자동으로 특정 상태로 빠져나가기(원샷용)
    bool autoExitOnEnd = false;
    std::string autoExitTo;
    CrossFadeParams autoExitFade{};

    // (Upper Layer 용) 이 상태가 base 위에 덮이는 강도
    float layerAlpha = 1.0f;

    // NEW: 역재생(서기)용 - time이 0에 도달하면 자동 전환
    bool autoExitOnStart = false;
};

// ============================================================
// 전환 정의(AnyState + Priority + ExitTime/EntryOffset + CrossFade)
// ============================================================
struct AnimTransitionDef
{
    int from = -1; // -1이면 AnyState
    int to = 0;

    int priority = 0; // 큰 값이 먼저 선택됨
    CrossFadeParams fade{};

    std::vector<AnimCondition> conditions;

    bool Eval(const AnimParams& p) const
    {
        for (const auto& c : conditions)
            if (!c.Eval(p)) return false;
        return true;
    }
};

// ============================================================
// 애니 라이브러리(키 -> aiAnimation*)
// ============================================================
struct AnimLibrary
{
    const aiScene* scene = nullptr;
    const std::unordered_map<std::string, int>* indexMap = nullptr;

    const aiAnimation* Get(const std::string& key) const
    {
        if (!scene || !indexMap) return nullptr;
        auto it = indexMap->find(key);
        if (it == indexMap->end()) return nullptr;
        const int idx = it->second;
        if (idx < 0 || (unsigned)idx >= scene->mNumAnimations) return nullptr;
        return scene->mAnimations[idx];
    }

    static float LengthSec(const aiAnimation* a)
    {
        if (!a) return 0.0f;
        const double tps = (a->mTicksPerSecond != 0.0) ? a->mTicksPerSecond : 25.0;
        const double durTicks = (a->mDuration > 0.0) ? a->mDuration : 0.0;
        return (tps > 0.0) ? (float)(durTicks / tps) : 0.0f;
    }

    static float WrapOrClamp(float t, float len, bool loop)
    {
        if (len <= 0.0f) return 0.0f;
        if (loop)
        {
            t = std::fmod(t, len);
            if (t < 0.0f) t += len;
            return t;
        }
        return std::clamp(t, 0.0f, len);
    }

    static float Norm01(float t, float len, bool loop)
    {
        if (len <= 0.0f) return 0.0f;
        t = WrapOrClamp(t, len, loop);
        return std::clamp(t / len, 0.0f, 1.0f);
    }
};

// ============================================================
// 상태머신 런타임
// ============================================================
class AnimStateMachine
{
public:
    void Clear()
    {
        m_States.clear();
        m_Transitions.clear();
        m_StateIndex.clear();
        Reset();
    }

    int AddState(const AnimStateDef& s)
    {
        int idx = (int)m_States.size();
        m_States.push_back(s);
        m_StateIndex[s.name] = idx;
        if (m_Current < 0) m_Current = idx;
        return idx;
    }

    void AddTransition(const AnimTransitionDef& t)
    {
        m_Transitions.push_back(t);
    }

    int FindState(const std::string& name) const
    {
        auto it = m_StateIndex.find(name);
        return (it != m_StateIndex.end()) ? it->second : -1;
    }

    void SetInitialState(const std::string& name)
    {
        int idx = FindState(name);
        if (idx >= 0)
        {
            m_Current = idx;
            m_TimeCurrent = 0.0f;
            m_InTransition = false;
            m_Pending = -1;
        }
    }

    void Reset()
    {
        m_Current = -1;
        m_TimeCurrent = 0.0f;

        m_InTransition = false;
        m_From = -1;
        m_To = -1;
        m_TimeFrom = 0.0f;
        m_TimeTo = 0.0f;
        m_BlendTime = 0.0f;
        m_Blend01 = 0.0f;

        m_Pending = -1;
    }

    // tick: 조건 평가 + exit-time 대기 + crossfade + auto-exit 처리
    void Tick(float dt, const AnimParams& params, const AnimLibrary& lib)
    {
        if (m_Current < 0 || m_Current >= (int)m_States.size())
            return;

        // 1) 시간 진행
        if (m_InTransition)
        {
            const auto& sFrom = m_States[(size_t)m_From];
            const auto& sTo   = m_States[(size_t)m_To];

            m_BlendTime += dt;

            m_TimeFrom += dt * sFrom.speed;
            m_TimeTo   += dt * sTo.speed;

            // 역재생 처리
            float lenFrom = StateLengthSec(sFrom, lib);
            float lenTo = StateLengthSec(sTo, lib);
            if (sFrom.speed < 0.0f)
                m_TimeFrom = std::clamp(m_TimeFrom, 0.0f, lenFrom);
            else
                m_TimeFrom = AnimLibrary::WrapOrClamp(m_TimeFrom, lenFrom, sFrom.loop);
            if (sTo.speed < 0.0f)
                m_TimeTo = std::clamp(m_TimeTo, 0.0f, lenTo);
            else
                m_TimeTo = AnimLibrary::WrapOrClamp(m_TimeTo, lenTo, sTo.loop);

            float dur = (std::max)(0.0001f, m_ActiveFade.durationSec);
            float a = std::clamp(m_BlendTime / dur, 0.0f, 1.0f);
            if (m_ActiveFade.smoothStep)
                a = a * a * (3.0f - 2.0f * a);
            m_Blend01 = a;

            if (a >= 1.0f - 1e-5f)
            {
                // 전환 완료
                m_InTransition = false;
                m_Current = m_To;
                m_TimeCurrent = m_TimeTo;
                m_Blend01 = 0.0f;
                m_Pending = -1;
            }
        }
        else
        {
            const auto& s = m_States[(size_t)m_Current];
            float len = StateLengthSec(s, lib);
            m_TimeCurrent += dt * s.speed;
            // 역재생(speed < 0) 처리: clamp to [0, len]
            if (s.speed < 0.0f)
                m_TimeCurrent = std::clamp(m_TimeCurrent, 0.0f, len);
            else
                m_TimeCurrent = AnimLibrary::WrapOrClamp(m_TimeCurrent, len, s.loop);
        }

        // 2) AutoExitOnEnd / AutoExitOnStart
        if (!m_InTransition)
        {
            const auto& s = m_States[(size_t)m_Current];
            if (!s.autoExitTo.empty())
            {
                float len = StateLengthSec(s, lib);
                if (len > 0.0f)
                {
                    // forward one-shot
                    if (s.autoExitOnEnd)
                    {
                        float norm = AnimLibrary::Norm01(m_TimeCurrent, len, false);
                        if (norm >= 1.0f - 1e-4f)
                        {
                            int toIdx = FindState(s.autoExitTo);
                            if (toIdx >= 0 && toIdx != m_Current)
                            {
                                StartTransition(m_Current, toIdx, s.autoExitFade, lib);
                                return;
                            }
                        }
                    }

                    // NEW: reverse one-shot (reach start)
                    if (s.autoExitOnStart)
                    {
                        if (m_TimeCurrent <= 1e-4f)
                        {
                            int toIdx = FindState(s.autoExitTo);
                            if (toIdx >= 0 && toIdx != m_Current)
                            {
                                StartTransition(m_Current, toIdx, s.autoExitFade, lib);
                                return;
                            }
                        }
                    }
                }
            }
        }

        // 3) 조건 기반 전환 평가 (매 프레임 재평가 + interrupt 허용)
        if (!m_InTransition)
        {
            int bestIdx = -1;
            int bestPri = -999999;

            for (int ti = 0; ti < (int)m_Transitions.size(); ++ti)
            {
                const auto& tr = m_Transitions[(size_t)ti];

                if (!(tr.from == -1 || tr.from == m_Current)) continue;
                if (tr.to == m_Current) continue;
                if (!tr.Eval(params)) continue;

                if (tr.priority > bestPri)
                {
                    bestPri = tr.priority;
                    bestIdx = ti;
                }
            }

            if (bestIdx >= 0)
            {
                const auto& tr = m_Transitions[(size_t)bestIdx];
                const auto& sFrom = m_States[(size_t)m_Current];

                float lenFrom = StateLengthSec(sFrom, lib);
                float normFrom = AnimLibrary::Norm01(m_TimeCurrent, lenFrom, sFrom.loop);

                if (!tr.fade.useExitTime || normFrom >= tr.fade.exitNorm)
                {
                    StartTransition(m_Current, tr.to, tr.fade, lib);
                    m_Pending = -1;
                    return;
                }
                else
                {
                    // 디버그용 표시만
                    m_Pending = bestIdx;
                }
            }
            else
            {
                m_Pending = -1;
            }
        }
    }

    // 레이어 평가 출력(현재/전환 상태를 A/B로 뽑아줌)
    void EvaluateLayerBlend(const AnimLibrary& lib,
                            const std::unordered_map<std::string, int>& clipIndexMap,
                            CharacterAnimator::LayerBlendDesc& out) const
    {
        (void)clipIndexMap;

        out = {};
        if (m_Current < 0 || m_Current >= (int)m_States.size())
            return;

        if (m_InTransition)
        {
            const auto& sFrom = m_States[(size_t)m_From];
            const auto& sTo   = m_States[(size_t)m_To];

            const aiAnimation* aA = sFrom.clipKey.empty() ? nullptr : lib.Get(sFrom.clipKey);
            const aiAnimation* aB = sTo.clipKey.empty()   ? nullptr : lib.Get(sTo.clipKey);

            out.enabled = (aA != nullptr) || (aB != nullptr);
            out.animA = aA;
            out.timeA = WrapForState(sFrom, m_TimeFrom, lib);

            out.animB = aB;
            out.timeB = WrapForState(sTo, m_TimeTo, lib);

            out.blend01 = m_Blend01;

            // layerAlpha도 전환 중이면 함께 lerp(Upper가 자연스럽게 fade in/out)
            out.layerAlpha = sFrom.layerAlpha + (sTo.layerAlpha - sFrom.layerAlpha) * m_Blend01;
        }
        else
        {
            const auto& s = m_States[(size_t)m_Current];
            const aiAnimation* a = s.clipKey.empty() ? nullptr : lib.Get(s.clipKey);

            out.enabled = (a != nullptr) && (s.layerAlpha > 0.0001f);
            out.animA = a;
            out.timeA = WrapForState(s, m_TimeCurrent, lib);

            out.animB = a;
            out.timeB = out.timeA;

            out.blend01 = 0.0f;
            out.layerAlpha = s.layerAlpha;
        }
    }

    // 현재 상태 이름
    std::string CurrentStateName() const
    {
        if (m_Current < 0 || m_Current >= (int)m_States.size()) return {};
        return m_States[(size_t)m_Current].name;
    }

    // getters (추가)
    float GetCurrentTimeSec() const { return m_TimeCurrent; }
    bool  IsInTransition() const { return m_InTransition; }
    float GetFromTimeSec() const { return m_TimeFrom; }
    float GetToTimeSec() const { return m_TimeTo; }
    float GetBlend01() const { return m_Blend01; }

    // --- UI/Debug Accessors ---
    const std::vector<AnimStateDef>& GetStates() const { return m_States; }
    std::vector<AnimStateDef>& GetStates() { return m_States; }

    const std::vector<AnimTransitionDef>& GetTransitions() const { return m_Transitions; }
    std::vector<AnimTransitionDef>& GetTransitions() { return m_Transitions; }

    int GetCurrentStateIndex() const { return m_Current; }
    int GetFromStateIndex() const { return m_From; }
    int GetToStateIndex() const { return m_To; }
    int GetPendingTransitionIndex() const { return m_Pending; }

    const CrossFadeParams& GetActiveFade() const { return m_ActiveFade; }

    // 클릭해서 강제 상태 전환(디버그용)
    void ForceState(int idx)
    {
        if (idx < 0 || idx >= (int)m_States.size()) return;
        m_Current = idx;
        m_TimeCurrent = 0.0f;
        m_InTransition = false;
        m_Pending = -1;
        m_From = m_To = -1;
        m_TimeFrom = m_TimeTo = 0.0f;
        m_BlendTime = 0.0f;
        m_Blend01 = 0.0f;
    }

private:
    float StateLengthSec(const AnimStateDef& s, const AnimLibrary& lib) const
    {
        if (s.forcedDurationSec > 0.0f) return s.forcedDurationSec;
        return AnimLibrary::LengthSec(lib.Get(s.clipKey));
    }

    float WrapForState(const AnimStateDef& s, float t, const AnimLibrary& lib) const
    {
        float len = StateLengthSec(s, lib);
        // forcedDurationSec 사용 시 원샷도 loop 여부로 wrap/clamp 처리
        return AnimLibrary::WrapOrClamp(t, len, s.loop);
    }

    void StartTransition(int fromIdx, int toIdx, const CrossFadeParams& fade, const AnimLibrary& lib)
    {
        m_InTransition = true;
        m_From = fromIdx;
        m_To = toIdx;
        m_ActiveFade = fade;

        // from은 현재 시간에서 시작
        m_TimeFrom = m_TimeCurrent;

        // to는 entryNorm 위치에서 시작 (역재생의 경우 entryNorm=1.0이면 lenTo에서 시작)
        const auto& sTo = m_States[(size_t)toIdx];
        float lenTo = StateLengthSec(sTo, lib);
        m_TimeTo = (lenTo > 0.0f) ? (fade.entryNorm * lenTo) : 0.0f;

        m_BlendTime = 0.0f;
        m_Blend01 = 0.0f;
    }

private:
    std::vector<AnimStateDef> m_States;
    std::vector<AnimTransitionDef> m_Transitions;
    std::unordered_map<std::string, int> m_StateIndex;

    int   m_Current = -1;
    float m_TimeCurrent = 0.0f;

    // transition runtime
    bool  m_InTransition = false;
    int   m_From = -1;
    int   m_To = -1;
    float m_TimeFrom = 0.0f;
    float m_TimeTo = 0.0f;
    float m_BlendTime = 0.0f;
    float m_Blend01 = 0.0f;
    CrossFadeParams m_ActiveFade{};

    // exit-time pending transition index
    int m_Pending = -1;
};

// ============================================================
// 컨트롤러 설정(애니 키 + 전환 테이블 + 레이어 파라미터)
// ============================================================
struct CharacterAnimControllerConfig
{
    // 애니 키(= animIndex에서 찾는 key)
    std::string keyIdle = "Idle";
    std::string keyWalk = "Walk";
    std::string keyRun  = "Run";

    // NEW
    std::string keyIdleToShoot   = "IdleToShoot";
    std::string keyShootStance   = "Shoot_Stance"; // 요청 이름 기준
    std::string keyShoot         = "Shoot";
    std::string keyReload        = "Reload";

    // 전환 테이블: layer[from][to] = CrossFade
    // AnyState는 from="*"
    std::unordered_map<std::string, std::unordered_map<std::string, CrossFadeParams>> baseTransitions;
    std::unordered_map<std::string, std::unordered_map<std::string, CrossFadeParams>> upperTransitions;
    std::unordered_map<std::string, std::unordered_map<std::string, CrossFadeParams>> addTransitions;

    // Locomotion은 즉시 반응해야 하므로 ExitTime을 기본 OFF
    CrossFadeParams defaultBaseFade{ 0.12f, false, 0.0f, 0.0f, true };
    CrossFadeParams defaultUpperFade{ 0.12f, false, 0.0f, 0.0f, true };
    CrossFadeParams defaultAddFade{ 0.05f, false, 0.0f, 0.0f, true };

    // upper layer 기본 알파(stance 같은 상체 덮기 강도)
    float upperStanceAlpha = 0.95f;
    float upperReloadAlpha = 1.00f;
    float upperShootAlpha  = 0.95f;

    // additive 기본 강도
    float additiveShootAlpha = 1.0f;

    // 서브 시스템
    RecoilParams recoil{};
    ShootParams  shoot{};
    IKParams     ik{};
    SocketSRTParams weaponSocket{};
    WeaponMagSocketParams weaponMagSocket{};
};

// ============================================================
// CharacterAnimController
// - Base/Upper/Additive 3개 상태머신 + 룰 + 업로드 + 무기 적용
// ============================================================
class CharacterAnimController
{
public:
    CharacterAnimControllerConfig config;
    std::unordered_map<std::string, int> animIndex; // "Idle"=0 같은 매핑

public:
    bool InitializeRig(
        ID3D11Device* device,
        const aiScene* scene,
        const std::unordered_map<std::string, int>& nodeMap,
        const XMFLOAT4X4& globalInv,
        const std::vector<std::string>& boneNames,
        const std::vector<XMFLOAT4X4>& boneOffsets,
        const std::vector<std::string>* optionalAnimNames = nullptr)
    {
        m_Lib.scene = scene;
        m_Lib.indexMap = &animIndex;

        // 이름 캐시(contains 자동 바인딩용)
        m_AnimNames.clear();
        if (optionalAnimNames && optionalAnimNames->size() == (scene ? scene->mNumAnimations : 0))
            m_AnimNames = *optionalAnimNames;
        else if (scene)
        {
            m_AnimNames.reserve(scene->mNumAnimations);
            for (unsigned i = 0; i < scene->mNumAnimations; ++i)
            {
                const aiAnimation* a = scene->mAnimations[i];
                std::string nm = (a && a->mName.length > 0) ? a->mName.C_Str() : ("Anim" + std::to_string(i));
                m_AnimNames.push_back(std::move(nm));
            }
        }

        // 저수준 리그 초기화
        m_Rig.Initialize(device, scene, nodeMap, globalInv, boneNames, boneOffsets);

        // 소켓 설정
        m_Rig.SetSocketSRT(
            config.weaponSocket.socketName,
            config.weaponSocket.parentBone,
            config.weaponSocket.pos,
            config.weaponSocket.rotDeg,
            config.weaponSocket.scale);

        // 자동 바인딩(비어있을 때만 채움)
        AutoBindCommonSlotsByContains();

        // 기본 그래프 생성(원하면 Initialize 후 BuildCustomGraph로 바꿔도 됨)
        BuildDefaultGraph();

        ResetRuntime();

        m_Inited = true;
        return true;
    }

    void AdjustSocket()
    {
		m_Rig.SetSocketSRT(
			config.weaponSocket.socketName,
			config.weaponSocket.parentBone,
			config.weaponSocket.pos,
			config.weaponSocket.rotDeg,
			config.weaponSocket.scale);
    }

    void ResetRuntime()
    {
        m_Params = AnimParams{};

        m_BaseSM.Reset();
        m_UpperSM.Reset();
        m_AddSM.Reset();

        // 초기 상태 지정
        m_BaseSM.SetInitialState("Idle");
        m_UpperSM.SetInitialState("None");
        m_AddSM.SetInitialState("None");

        m_TimeSec = 0.0f;
        m_RecoilAlpha = 0.0f;

        m_LastWeaponWorld = XMMatrixIdentity();
    }

    // ------------------------------------------------------------
    // "완벽하게": base/upper/add 모두 상태머신 + 룰로 구성된 기본 그래프
    // ------------------------------------------------------------
    void BuildDefaultGraph()
    {
        // ---------------- Base SM ----------------
        m_BaseSM.Clear();

        auto AddStateEx = [&](const AnimStateDef& def)
        {
            m_BaseSM.AddState(def);
        };

        // Idle
        {
            AnimStateDef s{};
            s.name = "Idle";
            s.clipKey = config.keyIdle;
            s.loop = true;
            s.speed = 1.0f;
            AddStateEx(s);
        }

        // Walk
        {
            AnimStateDef s{};
            s.name = "Walk";
            s.clipKey = config.keyWalk;
            s.loop = true;
            s.speed = 1.0f;
            AddStateEx(s);
        }

        // Run
        {
            AnimStateDef s{};
            s.name = "Run";
            s.clipKey = config.keyRun;
            s.loop = true;
            s.speed = 1.0f;
            AddStateEx(s);
        }

        // IdleToShoot (Enter crouch)
        {
            AnimStateDef s{};
            s.name = "IdleToShoot";
            s.clipKey = config.keyIdleToShoot;
            s.loop = false;
            s.speed = 1.0f;
            s.autoExitOnEnd = true;
            s.autoExitTo = "Shoot_Stance";
            s.autoExitFade = { 0.08f, false, 0.0f, 0.0f, true };
            AddStateEx(s);
        }

        // Shoot_Stance (Crouched stance loop)
        {
            AnimStateDef s{};
            s.name = "Shoot_Stance";
            s.clipKey = config.keyShootStance;
            s.loop = true;
            s.speed = 1.0f;
            AddStateEx(s);
        }

        // Shoot (one-shot -> back to stance)
        {
            AnimStateDef s{};
            s.name = "Shoot";
            s.clipKey = config.keyShoot;
            s.loop = false;
            s.speed = 1.0f;
            s.forcedDurationSec = (config.shoot.oneShotDurationSec > 0.0f) ? config.shoot.oneShotDurationSec : 0.0f;
            s.autoExitOnEnd = true;
            s.autoExitTo = "Shoot_Stance";
            s.autoExitFade = { 0.05f, false, 0.0f, 0.0f, true };
            AddStateEx(s);
        }

        // Reload (one-shot -> back to stance)
        {
            AnimStateDef s{};
            s.name = "Reload";
            s.clipKey = config.keyReload;
            s.loop = false;
            s.speed = 1.0f;
            s.autoExitOnEnd = true;
            s.autoExitTo = "Shoot_Stance";
            s.autoExitFade = { 0.06f, false, 0.0f, 0.0f, true };
            AddStateEx(s);
        }

        // IdleToShoot_Reverse (Exit crouch = reverse play)
        {
            AnimStateDef s{};
            s.name = "IdleToShoot_Reverse";
            s.clipKey = config.keyIdleToShoot;
            s.loop = false;
            s.speed = -1.0f;                 // reverse
            s.autoExitOnStart = true;        // NEW
            s.autoExitTo = "Idle";
            s.autoExitFade = { 0.08f, false, 0.0f, 0.0f, true };
            AddStateEx(s);
        }

        m_BaseSM.SetInitialState("Idle");

        // ---------------- Upper/Add SM (최소 유지: UI용) ----------------
        m_UpperSM.Clear();
        m_UpperSM.AddState(AnimStateDef{ "None", "", true, 1.0f, 0.0f, false, "", {}, 0.0f });
        m_UpperSM.SetInitialState("None");

        m_AddSM.Clear();
        m_AddSM.AddState(AnimStateDef{ "None", "", true, 1.0f, 0.0f, false, "", {}, 0.0f });
        m_AddSM.SetInitialState("None");

        // ---------------- transitions: Locomotion ----------------
        AddBaseTrans("Idle", "Walk", 0, {
            {AnimCondition::Type::BoolTrue,  "Move"},
            {AnimCondition::Type::BoolFalse, "Run"}
        });

        AddBaseTrans("Idle", "Run", 0, {
            {AnimCondition::Type::BoolTrue, "Move"},
            {AnimCondition::Type::BoolTrue, "Run"}
        });

        AddBaseTrans("Walk", "Idle", 0, {
            {AnimCondition::Type::BoolFalse, "Move"}
        });

        AddBaseTrans("Walk", "Run", 0, {
            {AnimCondition::Type::BoolTrue, "Run"}
        });

        AddBaseTrans("Run", "Idle", 0, {
            {AnimCondition::Type::BoolFalse, "Move"}
        });

        AddBaseTrans("Run", "Walk", 0, {
            {AnimCondition::Type::BoolTrue,  "Move"},
            {AnimCondition::Type::BoolFalse, "Run"}
        });

        // ---------------- transitions: Crouch toggle (Ctrl) ----------------
        // Idle/Walk/Run -> IdleToShoot
        AddBaseTrans("Idle", "IdleToShoot", 200, {
            {AnimCondition::Type::Trigger, "CrouchToggle"}
        });
        AddBaseTrans("Walk", "IdleToShoot", 200, {
            {AnimCondition::Type::Trigger, "CrouchToggle"}
        });
        AddBaseTrans("Run", "IdleToShoot", 200, {
            {AnimCondition::Type::Trigger, "CrouchToggle"}
        });

        // Shoot_Stance -> IdleToShoot_Reverse (entryNorm=1.0에서 시작해야 역재생이 자연스러움)
        {
            AnimTransitionDef t{};
            t.from = m_BaseSM.FindState("Shoot_Stance");
            t.to   = m_BaseSM.FindState("IdleToShoot_Reverse");
            t.priority = 200;
            t.fade = { 0.08f, false, 0.0f, 1.0f, true }; // entryNorm=1.0 (끝에서 시작)
            t.conditions = { {AnimCondition::Type::Trigger, "CrouchToggle"} };
            if (t.from >= 0 && t.to >= 0) m_BaseSM.AddTransition(t);
        }

        // ---------------- transitions: Shoot / Reload only in stance ----------------
        // Reload 우선
        AddBaseTrans("Shoot_Stance", "Reload", 100, {
            {AnimCondition::Type::Trigger, "ReloadPressed"}
        });

        AddBaseTrans("Shoot_Stance", "Shoot", 80, {
            {AnimCondition::Type::Trigger, "FirePressed"}
        });
    }

    // ------------------------------------------------------------
    // 매 프레임 업데이트(포즈+업로드+무기적용까지)
    // ※ 아래 TickAndApply는 템플릿으로 작성: 네 프로젝트의 ModelEntry/FbxAnimator 타입에 맞게 그대로 컴파일된다.
    // ------------------------------------------------------------
    template<typename CharacterModelT, typename WeaponModelT>
    void TickAndApply(float dt,
                      const CharacterInputState& input,
                      CharacterModelT& character,
                      WeaponModelT* weaponOrNull,
                      ID3D11Device* device,
                      ID3D11DeviceContext* ctx,
                      const AimInputState* aimOrNull = nullptr) // NEW (기본 nullptr)
    {
        // 1) 파라미터 업데이트
        m_TimeSec += dt;

        m_Params.SetBool("Move", input.move);
        m_Params.SetBool("Run",  input.run);

        if (input.crouchTogglePressed) m_Params.FireTrigger("CrouchToggle");
        if (input.firePressed)         m_Params.FireTrigger("FirePressed");
        if (input.reloadPressed)       m_Params.FireTrigger("ReloadPressed");

        // Base만 tick (Upper/Add는 최소 유지 상태라 tick해도 무해)
        m_BaseSM.Tick(dt, m_Params, m_Lib);
        m_UpperSM.Tick(dt, m_Params, m_Lib);
        m_AddSM.Tick(dt, m_Params, m_Lib);

        // 3) 리코일(절차적)
        UpdateRecoil(dt, input.firePressed);

        // 4) 레이어 결과를 CharacterAnimator::UpdateDesc로 변환
        CharacterAnimator::UpdateDesc d{};
        d.dt = dt;

        // base
        d.base.enabled = true;
        m_BaseSM.EvaluateLayerBlend(m_Lib, animIndex, d.base);
        d.base.layerAlpha = 1.0f;

        // upper/add 끔
        d.upper.enabled = false;
        d.additive.enabled = false;

        // recoil/procedural은 유지해도 되고(원하면 UI에서 끄기)
        d.procedural.strength = m_RecoilAlpha;
        d.procedural.seed = config.recoil.seed;
        d.procedural.timeSec = m_TimeSec;

        // IK: Reload 상태(또는 Reload로 전환 중)일 때만 활성화
        XMVECTOR ikTargetMS = XMVectorZero();
        bool enableIK = false;
        if (config.ik.enabled)
        {
            if (m_BaseSM.CurrentStateName() == "Reload")
                enableIK = true;
            else if (m_BaseSM.IsInTransition())
            {
                int toIdx = m_BaseSM.GetToStateIndex();
                if (toIdx >= 0)
                {
                    const auto& st = m_BaseSM.GetStates()[(size_t)toIdx];
                    if (st.name == "Reload") enableIK = true;
                }
            }
        }

        if (enableIK)
        {
            const XMMATRIX charWorld = character.GetWorldMatrix();
            const XMMATRIX weaponWorldNow = m_Rig.GetSocketWorldMatrix(config.weaponSocket.socketName, charWorld);

            const XMMATRIX magLocal = BuildTRS(config.weaponMagSocket.pos,
                                               config.weaponMagSocket.rotDeg,
                                               config.weaponMagSocket.scale);

            const XMMATRIX magWorld = magLocal * weaponWorldNow;
            const XMVECTOR magPosWS = XMVector3TransformCoord(XMVectorZero(), magWorld);

            const XMMATRIX invCharWorld = XMMatrixInverse(nullptr, charWorld);
            ikTargetMS = XMVector3TransformCoord(magPosWS, invCharWorld);

            d.ik.enabled = true;
            d.ik.tipBone = config.ik.tipBone.c_str();
            d.ik.chainLen = config.ik.chainLength;
            d.ik.targetMS = ikTargetMS;
            d.ik.weight = config.ik.weight;
        }
        else
        {
            d.ik.enabled = false;
        }

        // Aim 설정
        d.aim.enabled = false;
        if (aimOrNull && aimOrNull->enabled)
        {
            const std::string s = m_BaseSM.CurrentStateName();
            const bool crouchLike =
                (s == "IdleToShoot" || s == "Shoot_Stance" || s == "Shoot" || s == "Reload" || s == "IdleToShoot_Reverse");

            if (crouchLike)
            {
                d.aim.enabled = true;
                d.aim.yawRad  = aimOrNull->yawRad;
                d.aim.weight  = aimOrNull->weight;
            }
        }

        // 5) 리그 업데이트
        m_Rig.Update(d);

        // 6) 팔레트 업로드(컨트롤러가 수행)
        // character.fbxBaseAnimator가 EnsureBoneCB/UploadPalette를 갖는 타입이면 OK
        character.fbxBaseAnimator.EnsureBoneCB(device, 1023);
        character.fbxBaseAnimator.UploadPalette(ctx, m_Rig.finalTransforms);

        // 7) 무기 소켓 TRS 적용(컨트롤러가 수행)
        const XMMATRIX charWorld = character.GetWorldMatrix();
        m_LastWeaponWorld = m_Rig.GetSocketWorldMatrix(config.weaponSocket.socketName, charWorld);

        if (weaponOrNull)
        {
            WeaponTransformOut wt = DecomposeToTRS(m_LastWeaponWorld);
            if (wt.valid)
            {
                weaponOrNull->pos = wt.pos;
                weaponOrNull->rotDeg = wt.rotDeg;
                weaponOrNull->scale = wt.scale;
            }
        }

        // 8) 트리거는 프레임 끝에 소거(Trigger = one-frame)
        m_Params.ClearTriggers();
    }

    // 외부에서 상태/디버그용으로 보고 싶을 때
    std::string DebugBaseState() const { return m_BaseSM.CurrentStateName(); }
    std::string DebugUpperState() const { return m_UpperSM.CurrentStateName(); }
    std::string DebugAddState() const { return m_AddSM.CurrentStateName(); }

    XMMATRIX GetWeaponWorldMatrix() const { return m_LastWeaponWorld; }

    // 현재 자세 상태 쿼리
    bool IsInShootStance() const
    {
        return m_BaseSM.CurrentStateName() == "Shoot_Stance";
    }

    bool IsMovementLocked() const
    {
        const std::string s = m_BaseSM.CurrentStateName();
        return (s == "IdleToShoot" || s == "Shoot_Stance" || s == "Shoot" || s == "Reload" || s == "IdleToShoot_Reverse");
    }

    // SM 접근자
    AnimStateMachine& BaseSM() { return m_BaseSM; }
    AnimStateMachine& UpperSM() { return m_UpperSM; }
    AnimStateMachine& AddSM() { return m_AddSM; }

    const AnimStateMachine& BaseSM() const { return m_BaseSM; }
    const AnimStateMachine& UpperSM() const { return m_UpperSM; }
    const AnimStateMachine& AddSM() const { return m_AddSM; }

    const std::vector<std::string>& GetAnimNames() const { return m_AnimNames; }
    std::unordered_map<std::string, int>& GetAnimIndexMap() { return animIndex; }
    const std::unordered_map<std::string, int>& GetAnimIndexMap() const { return animIndex; }

    // ------------------------------------------------------------
    // 자동 슬롯 바인딩(애니 이름 contains로 key->index를 채움)
    // ------------------------------------------------------------
    void AutoBindCommonSlotsByContains()
    {
        // locomotion
        TryBindSlotSmart(config.keyIdle, { "idle" }, { "idle_to_shoot", "idletoshoot" });
        TryBindSlotSmart(config.keyWalk, { "walk" });
        TryBindSlotSmart(config.keyRun,  { "run" });

        // NEW: crouch enter/exit clip
        TryBindSlotSmart(config.keyIdleToShoot, { "idle_to_shoot", "idletoshoot", "idle2shoot" });

        // stance / shoot / reload
        TryBindSlotSmart(config.keyShootStance, { "shoot_stance", "shoot stance", "stance" });
        TryBindSlotSmart(config.keyReload,      { "reload" });

        // IMPORTANT: Shoot은 stance에 붙으면 안 됨
        TryBindSlotSmart(config.keyShoot, { "shoot" },
                         { "stance", "reload", "idle_to_shoot", "idletoshoot" });
    }

private:
    // ---------- helpers ----------
    static std::string ToLower(std::string s)
    {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    }

    static bool IContains(const std::string& hay, const std::string& needle)
    {
        return ToLower(hay).find(ToLower(needle)) != std::string::npos;
    }

    void TryBindSlotContains(const std::string& slotKey, std::initializer_list<std::string> needles)
    {
        auto it = animIndex.find(slotKey);
        if (it != animIndex.end() && it->second >= 0) return;

        int idx = FindAnimIndexContains(needles);
        if (idx >= 0) animIndex[slotKey] = idx;
    }

    int FindAnimIndexContains(std::initializer_list<std::string> needles) const
    {
        for (int i = 0; i < (int)m_AnimNames.size(); ++i)
        {
            for (auto& n : needles)
            {
                if (IContains(m_AnimNames[(size_t)i], n))
                    return i;
            }
        }
        return -1;
    }

    // NEW: exact match (ignore case)
    int FindAnimIndexExactICase(const std::string& exact) const
    {
        const std::string ex = ToLower(exact);
        for (int i = 0; i < (int)m_AnimNames.size(); ++i)
        {
            if (ToLower(m_AnimNames[(size_t)i]) == ex)
                return i;
        }
        return -1;
    }

    // NEW: include/exclude 스코어링(최소 1개 include는 매치되어야 함)
    int FindAnimIndexBestMatch(std::initializer_list<std::string> includes,
                               std::initializer_list<std::string> excludes) const
    {
        int best = -1;
        int bestScore = -999999;

        for (int i = 0; i < (int)m_AnimNames.size(); ++i)
        {
            const std::string n = ToLower(m_AnimNames[(size_t)i]);

            bool anyInclude = false;
            int score = 0;

            for (auto& inc : includes)
            {
                if (n.find(ToLower(inc)) != std::string::npos)
                {
                    anyInclude = true;
                    score += 100;
                }
            }
            if (!anyInclude) continue;

            for (auto& exc : excludes)
            {
                if (n.find(ToLower(exc)) != std::string::npos)
                {
                    score -= 1000; // 강한 패널티
                }
            }

            // 짧은 이름 약간 선호(동점 정리)
            score -= (int)n.size();

            if (score > bestScore)
            {
                bestScore = score;
                best = i;
            }
        }
        return best;
    }

    // NEW: smart bind
    void TryBindSlotSmart(const std::string& slotKey,
                          std::initializer_list<std::string> includes,
                          std::initializer_list<std::string> excludes = {})
    {
        auto it = animIndex.find(slotKey);
        if (it != animIndex.end() && it->second >= 0) return;

        int idx = FindAnimIndexExactICase(slotKey);
        if (idx < 0) idx = FindAnimIndexBestMatch(includes, excludes);

        if (idx >= 0)
            animIndex[slotKey] = idx;
    }

    static CrossFadeParams GetFade(
        const std::unordered_map<std::string, std::unordered_map<std::string, CrossFadeParams>>& table,
        const std::string& from, const std::string& to,
        const CrossFadeParams& fallback)
    {
        auto itF = table.find(from);
        if (itF != table.end())
        {
            auto itT = itF->second.find(to);
            if (itT != itF->second.end()) return itT->second;
        }
        // AnyState("*")도 지원
        auto itAny = table.find("*");
        if (itAny != table.end())
        {
            auto itT = itAny->second.find(to);
            if (itT != itAny->second.end()) return itT->second;
        }
        return fallback;
    }

    void AddBaseTrans(const std::string& from, const std::string& to, int pri, std::initializer_list<AnimCondition> conds)
    {
        AnimTransitionDef t;
        t.from = m_BaseSM.FindState(from);
        t.to   = m_BaseSM.FindState(to);
        t.priority = pri;
        t.fade = GetFade(config.baseTransitions, from, to, config.defaultBaseFade);
        t.conditions.assign(conds.begin(), conds.end());
        if (t.from >= 0 && t.to >= 0) m_BaseSM.AddTransition(t);
    }

    void AddUpperTrans(const std::string& from, const std::string& to, int pri, std::initializer_list<AnimCondition> conds)
    {
        AnimTransitionDef t;
        t.from = m_UpperSM.FindState(from);
        t.to   = m_UpperSM.FindState(to);
        t.priority = pri;
        t.fade = GetFade(config.upperTransitions, from, to, config.defaultUpperFade);
        t.conditions.assign(conds.begin(), conds.end());
        if (t.from >= 0 && t.to >= 0) m_UpperSM.AddTransition(t);
    }

    void AddUpperAnyTrans(const std::string& to, int pri, std::initializer_list<AnimCondition> conds)
    {
        AnimTransitionDef t;
        t.from = -1; // AnyState
        t.to   = m_UpperSM.FindState(to);
        t.priority = pri;
        t.fade = GetFade(config.upperTransitions, "*", to, config.defaultUpperFade);
        t.conditions.assign(conds.begin(), conds.end());
        if (t.to >= 0) m_UpperSM.AddTransition(t);
    }

    void AddAddTrans(const std::string& from, const std::string& to, int pri, std::initializer_list<AnimCondition> conds)
    {
        AnimTransitionDef t;
        t.from = m_AddSM.FindState(from);
        t.to   = m_AddSM.FindState(to);
        t.priority = pri;
        t.fade = GetFade(config.addTransitions, from, to, config.defaultAddFade);
        t.conditions.assign(conds.begin(), conds.end());
        if (t.from >= 0 && t.to >= 0) m_AddSM.AddTransition(t);
    }

    void UpdateRecoil(float dt, bool firePressed)
    {
        if (!config.recoil.enabled)
        {
            m_RecoilAlpha = 0.0f;
            return;
        }

        if (firePressed)
        {
            m_RecoilAlpha += config.recoil.kick;
            if (m_RecoilAlpha > 1.0f) m_RecoilAlpha = 1.0f;
        }

        const float decay = (std::max)(0.0f, config.recoil.decay);
        m_RecoilAlpha *= std::exp(-decay * dt);
        if (m_RecoilAlpha < 0.0001f) m_RecoilAlpha = 0.0f;
    }

    static XMMATRIX BuildTRS(const XMFLOAT3& pos, const XMFLOAT3& rotDeg, const XMFLOAT3& scale)
    {
        const XMMATRIX mS = XMMatrixScaling(scale.x, scale.y, scale.z);
        const XMMATRIX mR = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(rotDeg.x),
            XMConvertToRadians(rotDeg.y),
            XMConvertToRadians(rotDeg.z));
        const XMMATRIX mT = XMMatrixTranslation(pos.x, pos.y, pos.z);
        return mT * mR * mS;
    }

    static WeaponTransformOut DecomposeToTRS(CXMMATRIX m)
    {
        WeaponTransformOut out{};
        XMVECTOR S, R, T;
        if (!XMMatrixDecompose(&S, &R, &T, m))
            return out;

        XMStoreFloat3(&out.scale, S);
        XMStoreFloat3(&out.pos, T);

        XMFLOAT4 rq{}; XMStoreFloat4(&rq, R);

        const float sinp = 2.0f * (rq.w * rq.x + rq.y * rq.z);
        const float cosp = 1.0f - 2.0f * (rq.x * rq.x + rq.y * rq.y);
        const float pitch = std::atan2(sinp, cosp);

        const float siny = 2.0f * (rq.w * rq.y - rq.z * rq.x);
        const float yaw = (std::fabs(siny) >= 1.0f) ? std::copysign(XM_PIDIV2, siny) : std::asin(siny);

        const float sinr = 2.0f * (rq.w * rq.z + rq.x * rq.y);
        const float cosr = 1.0f - 2.0f * (rq.y * rq.y + rq.z * rq.z);
        const float roll = std::atan2(sinr, cosr);

        out.rotDeg = XMFLOAT3(XMConvertToDegrees(pitch), XMConvertToDegrees(yaw), XMConvertToDegrees(roll));
        out.valid = true;
        return out;
    }

private:
    bool m_Inited = false;

    AnimParams m_Params{};
    AnimLibrary m_Lib{};

    CharacterAnimator m_Rig;

    AnimStateMachine m_BaseSM;
    AnimStateMachine m_UpperSM;
    AnimStateMachine m_AddSM;

    std::vector<std::string> m_AnimNames;

    float m_TimeSec = 0.0f;
    float m_RecoilAlpha = 0.0f;

    XMMATRIX m_LastWeaponWorld = XMMatrixIdentity();
};

