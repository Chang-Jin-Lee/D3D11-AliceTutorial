#include "SoundManager.h"

#include <fmod.hpp>
#include "../Common/Helper.h" // Utf8FromWString

// 사용 중인 FMOD 라이브러리 이름은 프로젝트 설정에 따라 다를 수 있다.
// 필요 시 이 pragma 를 수정하거나 제거해도 된다.
#pragma comment(lib, "fmod_vc.lib")

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


