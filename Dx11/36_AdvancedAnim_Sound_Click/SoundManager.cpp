#include "SoundManager.h"

#include <fmod.hpp>
#include "../Common/Helper.h" // Utf8FromWString

// 사용 중인 FMOD 라이브러리 이름은 프로젝트 설정에 따라 다를 수 있다.
// 필요 시 이 pragma 를 수정하거나 제거해도 된다.
#pragma comment(lib, "fmod_vc.lib")

#include <unordered_map>
#include <algorithm>

namespace
{
	FMOD::System*  g_System = nullptr;
	FMOD::Sound*   g_Music = nullptr;
	FMOD::Channel* g_Channel = nullptr;
	float          g_LengthSec = 0.0f;

	inline bool Check(FMOD_RESULT r)
	{
		return (r == FMOD_OK);
	}

	struct SfxEntry
	{
		FMOD::Sound*   sound   = nullptr;
		FMOD::Channel* channel = nullptr;
		bool loop = false;
	};

	std::unordered_map<std::string, SfxEntry> g_Sfx;
}

bool Sound::Initialize()
{
	if (g_System) return true;

	FMOD_RESULT r = FMOD::System_Create(&g_System);
	if (!Check(r) || !g_System) return false;

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
	UnloadAllSfx();   // 추가
	UnloadMusic();

	if (g_System)
	{
		g_System->close();
		g_System->release();
		g_System = nullptr;
	}
}

bool Sound::LoadMusic(const std::wstring& pathW)
{
	if (!g_System && !Initialize()) return false;

	UnloadMusic();

	std::string pathU8 = Utf8FromWString(pathW);
	FMOD_RESULT r = g_System->createSound(
		pathU8.c_str(),
		FMOD_DEFAULT | FMOD_CREATESTREAM,
		nullptr,
		&g_Music);
	if (!Check(r) || !g_Music)
	{
		g_Music = nullptr;
		return false;
	}

	// 길이(ms)를 초 단위로 캐시
	unsigned int lenMs = 0;
	r = g_Music->getLength(&lenMs, FMOD_TIMEUNIT_MS);
	if (Check(r))
	{
		g_LengthSec = static_cast<float>(lenMs) / 1000.0f;
	}
	else
	{
		g_LengthSec = 0.0f;
	}

	return true;
}

void Sound::UnloadMusic()
{
	if (g_Channel)
	{
		g_Channel->stop();
		g_Channel = nullptr;
	}
	if (g_Music)
	{
		g_Music->release();
		g_Music = nullptr;
	}
	g_LengthSec = 0.0f;
}

void Sound::Play()
{
	if (!g_System || !g_Music) return;

	// 이미 채널이 있으면 일시정지만 해제
	if (g_Channel)
	{
		g_Channel->setPaused(false);
		return;
	}

	FMOD_RESULT r = g_System->playSound(g_Music, nullptr, false, &g_Channel);
	if (!Check(r))
	{
		g_Channel = nullptr;
	}
}

void Sound::Pause(bool pause)
{
	if (!g_Channel) return;
	g_Channel->setPaused(pause);
}

void Sound::Stop()
{
	if (!g_Channel) return;
	g_Channel->stop();
	g_Channel = nullptr;
}

bool Sound::IsPlaying()
{
	if (!g_Channel) return false;
	bool playing = false;
	if (!Check(g_Channel->isPlaying(&playing))) return false;
	return playing;
}

bool Sound::IsPaused()
{
	if (!g_Channel) return false;
	bool paused = false;
	if (!Check(g_Channel->getPaused(&paused))) return false;
	return paused;
}

bool Sound::SetTimeSeconds(float sec)
{
	if (!g_System || !g_Music) return false;

	// 범위 클램프
	if (sec < 0.0f) sec = 0.0f;
	if (g_LengthSec > 0.0f && sec > g_LengthSec) sec = g_LengthSec;

	// 채널이 없으면 일시정지 상태로 하나 만들어 둔다 (타임라인 스크럽용)
	if (!g_Channel)
	{
		FMOD_RESULT rPlay = g_System->playSound(g_Music, nullptr, true, &g_Channel);
		if (!Check(rPlay) || !g_Channel)
		{
			g_Channel = nullptr;
			return false;
		}
	}

	unsigned int posMs = static_cast<unsigned int>(sec * 1000.0f);
	if (!Check(g_Channel->setPosition(posMs, FMOD_TIMEUNIT_MS))) return false;
	return true;
}

float Sound::GetTimeSeconds()
{
	if (!g_Channel) return 0.0f;
	unsigned int posMs = 0;
	if (!Check(g_Channel->getPosition(&posMs, FMOD_TIMEUNIT_MS))) return 0.0f;
	return static_cast<float>(posMs) / 1000.0f;
}

float Sound::GetLengthSeconds()
{
	return g_LengthSec;
}

void Sound::Update()
{
	if (!g_System) return;
	g_System->update();
}

// ===================== SFX =====================
bool Sound::LoadSfx(const std::string& key, const std::wstring& pathW, bool loop)
{
	if (!g_System && !Initialize()) return false;

	// 기존 있으면 해제
	UnloadSfx(key);

	std::string pathU8 = Utf8FromWString(pathW);

	FMOD::Sound* s = nullptr;
	FMOD_RESULT r = g_System->createSound(
		pathU8.c_str(),
		FMOD_DEFAULT,   // SFX는 보통 stream 필요 없음
		nullptr,
		&s);

	if (!Check(r) || !s) return false;

	// loop 설정
	s->setMode(loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);

	SfxEntry e;
	e.sound = s;
	e.channel = nullptr;
	e.loop = loop;
	g_Sfx[key] = e;
	return true;
}

void Sound::UnloadSfx(const std::string& key)
{
	auto it = g_Sfx.find(key);
	if (it == g_Sfx.end()) return;

	if (it->second.channel)
	{
		it->second.channel->stop();
		it->second.channel = nullptr;
	}
	if (it->second.sound)
	{
		it->second.sound->release();
		it->second.sound = nullptr;
	}
	g_Sfx.erase(it);
}

void Sound::UnloadAllSfx()
{
	for (auto& kv : g_Sfx)
	{
		if (kv.second.channel) kv.second.channel->stop();
		if (kv.second.sound)   kv.second.sound->release();
		kv.second.channel = nullptr;
		kv.second.sound = nullptr;
	}
	g_Sfx.clear();
}

bool Sound::IsSfxPlaying(const std::string& key)
{
	auto it = g_Sfx.find(key);
	if (it == g_Sfx.end() || !it->second.channel) return false;

	bool playing = false;
	if (!Check(it->second.channel->isPlaying(&playing))) return false;
	return playing;
}

bool Sound::PlaySfx(const std::string& key, float volume, bool restart, float pitch)
{
	auto it = g_Sfx.find(key);
	if (it == g_Sfx.end() || !g_System || !it->second.sound) return false;

	// pitch clamp(너무 극단 방지)
	pitch = std::clamp(pitch, 0.5f, 2.0f);
	volume = std::clamp(volume, 0.0f, 1.0f);

	// 이미 재생 중이고 restart=false면 재시작하지 않음
	if (!restart && IsSfxPlaying(key))
	{
		it->second.channel->setVolume(volume);
		it->second.channel->setPitch(pitch);
		return true;
	}

	// restart면 기존 채널 정지
	if (it->second.channel)
	{
		it->second.channel->stop();
		it->second.channel = nullptr;
	}

	FMOD::Channel* ch = nullptr;
	FMOD_RESULT r = g_System->playSound(it->second.sound, nullptr, false, &ch);
	if (!Check(r) || !ch) return false;

	ch->setVolume(volume);
	ch->setPitch(pitch);

	it->second.channel = ch;
	return true;
}

void Sound::StopSfx(const std::string& key)
{
	auto it = g_Sfx.find(key);
	if (it == g_Sfx.end()) return;

	if (it->second.channel)
	{
		it->second.channel->stop();
		it->second.channel = nullptr;
	}
}

void Sound::SetSfxPitch(const std::string& key, float pitch)
{
	auto it = g_Sfx.find(key);
	if (it == g_Sfx.end() || !it->second.channel) return;
	pitch = std::clamp(pitch, 0.5f, 2.0f);
	it->second.channel->setPitch(pitch);
}

void Sound::SetSfxVolume(const std::string& key, float volume)
{
	auto it = g_Sfx.find(key);
	if (it == g_Sfx.end() || !it->second.channel) return;
	volume = std::clamp(volume, 0.0f, 1.0f);
	it->second.channel->setVolume(volume);
}


