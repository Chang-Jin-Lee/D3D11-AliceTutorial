#include "SoundManager.h"

#include <fmod.hpp>
#include "../Common/Helper.h" // Utf8FromWString
#include <map>
#include <vector>
#include <algorithm>
#include <string>

// 사용 중인 FMOD 라이브러리 이름은 프로젝트 설정에 따라 다를 수 있다.
// 필요 시 이 pragma 를 수정하거나 제거해도 된다.
#pragma comment(lib, "fmod_vc.lib")

namespace
{
	FMOD::System* g_System = nullptr;

	// [리소스 저장소] 파일경로(Key) -> FMOD 사운드 객체(Value)
	// 리소스 매니저에 연결해야하는 로직임 엔진에서 쓸때는 이 부분을 연결하셈
	std::map<std::wstring, FMOD::Sound*> g_SoundBank;

	// BGM 채널 (BGM은 한 번에 하나만 재생한다고 가정)
	FMOD::Channel* g_ChannelBGM = nullptr;
	float g_VolBGM = 1.0f;
	std::wstring g_CurrentBGMPath; // 현재 재생 중인 BGM 경로

	// SFX 채널들 (여러 개 동시 재생 관리용)
	std::vector<FMOD::Channel*> g_ChannelsSFX;
	float g_VolSFX = 1.0f;

	inline bool Check(FMOD_RESULT r)
	{
		return (r == FMOD_OK);
	}

	// 재생 끝난 SFX 채널 청소
	void CleanupSFX()
	{
		if (g_ChannelsSFX.empty()) return;

		auto it = std::remove_if(g_ChannelsSFX.begin(), g_ChannelsSFX.end(),
			[](FMOD::Channel* c) {
				if (!c) return true;
				bool playing = false;
				c->isPlaying(&playing);
				return !playing;
			});

		g_ChannelsSFX.erase(it, g_ChannelsSFX.end());
	}
}

bool Sound::Initialize()
{
	if (g_System) return true;

	FMOD_RESULT r = FMOD::System_Create(&g_System);
	if (!Check(r) || !g_System) return false;

	// 채널 수를 512개로 넉넉하게 잡음
	r = g_System->init(512, FMOD_INIT_NORMAL, nullptr);
	if (!Check(r))
	{
		g_System->release();
		g_System = nullptr;
		return false;
	}
	return true;
}

void Sound::Shutdown()
{
	StopBGM();
	StopAllSFX();

	// 로드된 모든 사운드 해제 (Sound Bank 비우기)
	for (auto& pair : g_SoundBank)
	{
		if (pair.second)
		{
			pair.second->release();
		}
	}
	g_SoundBank.clear();

	if (g_System)
	{
		g_System->close();
		g_System->release();
		g_System = nullptr;
	}
}

void Sound::Update()
{
	if (!g_System) return;
	g_System->update();
	CleanupSFX();
}

// 파일을 로드해서 맵에 저장
bool Sound::Load(const std::wstring& path, Type type)
{
	if (!g_System && !Initialize()) return false;

	// 이미 로드되어 있는지 확인 (캐싱)
	auto it = g_SoundBank.find(path);
	if (it != g_SoundBank.end())
	{
		return true; // 이미 있음
	}

	// FMOD 사운드 생성
	FMOD::Sound* newSound = nullptr;
	std::string pathU8 = Utf8FromWString(path);
	FMOD_MODE mode = FMOD_DEFAULT;

	// 타입에 따라 플래그 결정
	if (type == Type::BGM)
	{
		mode |= FMOD_CREATESTREAM; // 스트리밍 (디스크 읽기)
		mode |= FMOD_LOOP_NORMAL;  // 무한 반복
	}
	else // SFX
	{
		mode |= FMOD_CREATESAMPLE; // 메모리 로드 (중첩 가능)
		mode |= FMOD_LOOP_OFF;     // 한 번 재생
	}

	FMOD_RESULT r = g_System->createSound(pathU8.c_str(), mode, nullptr, &newSound);
	if (!Check(r) || !newSound)
	{
		// 로드 실패
		return false;
	}

	//맵에 등록
	g_SoundBank[path] = newSound;
	return true;
}

// ================= BGM =================

void Sound::PlayBGM(const std::wstring& path)
{
	if (!g_System) return;

	// 로드 안 되어 있으면 로드 시도
	if (g_SoundBank.find(path) == g_SoundBank.end())
	{
		if (!Load(path, Type::BGM)) return; // 파일 없음
	}

	FMOD::Sound* sound = g_SoundBank[path];
	if (!sound) return;

	// 만약 이미 다른 BGM이 재생 중이면 정지
	StopBGM();

	// 재생
	FMOD_RESULT r = g_System->playSound(sound, nullptr, false, &g_ChannelBGM);
	if (Check(r) && g_ChannelBGM)
	{
		g_ChannelBGM->setVolume(g_VolBGM);
		g_CurrentBGMPath = path;
	}
}

void Sound::PauseBGM(bool pause)
{
	if (g_ChannelBGM)
	{
		g_ChannelBGM->setPaused(pause);
	}
}

void Sound::StopBGM()
{
	if (g_ChannelBGM)
	{
		g_ChannelBGM->stop();
		g_ChannelBGM = nullptr;
	}
	g_CurrentBGMPath.clear();
}

void Sound::SetBGMVolume(float volume)
{
	g_VolBGM = volume;
	if (g_VolBGM < 0.0f) g_VolBGM = 0.0f;
	if (g_VolBGM > 1.0f) g_VolBGM = 1.0f;

	if (g_ChannelBGM)
	{
		g_ChannelBGM->setVolume(g_VolBGM);
	}
}

bool Sound::IsBGMPlaying()
{
	if (!g_ChannelBGM) return false;
	bool playing = false;
	if (!Check(g_ChannelBGM->isPlaying(&playing))) return false;
	return playing;
}

bool Sound::IsBGMPaused()
{
	if (!g_ChannelBGM) return false;
	bool paused = false;
	if (!Check(g_ChannelBGM->getPaused(&paused))) return false;
	return paused;
}

bool Sound::SetBGMTimeSeconds(float sec)
{
	if (!g_ChannelBGM || g_CurrentBGMPath.empty()) return false;

	auto it = g_SoundBank.find(g_CurrentBGMPath);
	if (it == g_SoundBank.end() || !it->second) return false;

	// 사운드 길이 확인
	unsigned int lenMs = 0;
	if (!Check(it->second->getLength(&lenMs, FMOD_TIMEUNIT_MS))) return false;
	float lenSec = static_cast<float>(lenMs) / 1000.0f;

	// 범위 클램프
	if (sec < 0.0f) sec = 0.0f;
	if (lenSec > 0.0f && sec > lenSec) sec = lenSec;

	unsigned int posMs = static_cast<unsigned int>(sec * 1000.0f);
	if (!Check(g_ChannelBGM->setPosition(posMs, FMOD_TIMEUNIT_MS))) return false;
	return true;
}

float Sound::GetBGMTimeSeconds()
{
	if (!g_ChannelBGM) return 0.0f;
	unsigned int posMs = 0;
	if (!Check(g_ChannelBGM->getPosition(&posMs, FMOD_TIMEUNIT_MS))) return 0.0f;
	return static_cast<float>(posMs) / 1000.0f;
}

float Sound::GetBGMLengthSeconds()
{
	if (g_CurrentBGMPath.empty()) return 0.0f;

	auto it = g_SoundBank.find(g_CurrentBGMPath);
	if (it == g_SoundBank.end() || !it->second) return 0.0f;

	unsigned int lenMs = 0;
	if (!Check(it->second->getLength(&lenMs, FMOD_TIMEUNIT_MS))) return 0.0f;
	return static_cast<float>(lenMs) / 1000.0f;
}

// ================= SFX =================

void Sound::PlaySFX(const std::wstring& path)
{
	if (!g_System) return;

	// 로드 안 되어 있으면 로드 시도
	if (g_SoundBank.find(path) == g_SoundBank.end())
	{
		if (!Load(path, Type::SFX)) return;
	}

	FMOD::Sound* sound = g_SoundBank[path];
	if (!sound) return;

	// Fire and Forget 방식
	FMOD::Channel* channel = nullptr;
	FMOD_RESULT r = g_System->playSound(sound, nullptr, false, &channel);

	if (Check(r) && channel)
	{
		channel->setVolume(g_VolSFX);
		g_ChannelsSFX.push_back(channel);
	}
}

void Sound::StopAllSFX()
{
	for (auto ch : g_ChannelsSFX)
	{
		if (ch)
		{
			ch->stop();
		}
	}
	g_ChannelsSFX.clear();
}

// 가장 최근 효과음 정지
void Sound::StopLastSFX()
{
    // 재생 중인 효과음이 없으면 리턴
    if (g_ChannelsSFX.empty()) return;

    // 벡터의 맨 뒤(back)가 가장 최근에 추가된 채널임
    FMOD::Channel* lastCh = g_ChannelsSFX.back();

    if (lastCh)
    {
        // 채널 정지
        lastCh->stop();
    }

    // 관리 리스트에서 즉시 제거
    g_ChannelsSFX.pop_back();
}

void Sound::SetSFXVolume(float volume)
{
	g_VolSFX = volume;
	if (g_VolSFX < 0.0f) g_VolSFX = 0.0f;
	if (g_VolSFX > 1.0f) g_VolSFX = 1.0f;

	// 현재 재생 중인 모든 효과음 볼륨 조절
	for (auto ch : g_ChannelsSFX)
	{
		if (ch)
		{
			ch->setVolume(g_VolSFX);
		}
	}
}
