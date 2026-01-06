//#include "SoundManager.h"
//
//#include <fmod.hpp>
////#include "../Common/Helper.h" // Utf8FromWString
//#include <map>
//#include <vector>
//#include <algorithm>
//#include <string>
//
//// 사용 중인 FMOD 라이브러리 이름은 프로젝트 설정에 따라 다를 수 있다.
//// 필요 시 이 pragma 를 수정하거나 제거해도 된다.
//#pragma comment(lib, "fmod_vc.lib")
//
//namespace
//{
//	FMOD::System* g_System = nullptr;
//
//	// [리소스 저장소] 사운드 이름(Key) -> FMOD 사운드 객체(Value)
//	// 리소스 매니저에 연결해야하는 로직임 엔진에서 쓸때는 이 부분을 연결하셈
//	std::map<std::wstring, FMOD::Sound*> g_SoundBank;
//
//	// BGM 채널 (BGM은 한 번에 하나만 재생한다고 가정)
//	FMOD::Channel* g_ChannelBGM = nullptr;
//	float g_VolBGM = 1.0f;
//	std::wstring g_CurrentBGMKey; // 현재 재생 중인 BGM key
//
//	// SFX 엔트리 구조체 (루프 SFX용)
//	struct SfxEntry
//	{
//		FMOD::Channel* channel = nullptr;
//		bool loop = false;
//	};
//
//	// SFX 채널 관리
//	std::map<std::wstring, SfxEntry> g_SfxChannels; // key별 채널 (루프 SFX용)
//	std::vector<FMOD::Channel*> g_ChannelsSFX; // 원샷 SFX용 (Fire and Forget)
//	float g_VolSFX = 1.0f;
//	float g_PitchSFX = 1.0f;
//
//	inline bool Check(FMOD_RESULT r)
//	{
//		return (r == FMOD_OK);
//	}
//
//	// 재생 끝난 SFX 채널 청소
//	void CleanupSFX()
//	{
//		// 원샷 SFX 채널 청소
//		if (!g_ChannelsSFX.empty())
//		{
//		auto it = std::remove_if(g_ChannelsSFX.begin(), g_ChannelsSFX.end(),
//			[](FMOD::Channel* c) {
//			if (!c) return true;
//			bool playing = false;
//				FMOD_RESULT r = c->isPlaying(&playing);
//				return (r != FMOD_OK) || !playing;
//		});
//
//		g_ChannelsSFX.erase(it, g_ChannelsSFX.end());
//		}
//
//		// 루프 SFX 채널 청소 (재생 끝난 것 제거)
//		for (auto it = g_SfxChannels.begin(); it != g_SfxChannels.end();)
//		{
//			bool remove = false;
//			if (it->second.channel)
//			{
//				bool playing = false;
//				FMOD_RESULT r = it->second.channel->isPlaying(&playing);
//				if (r != FMOD_OK || !playing)
//				{
//					it->second.channel = nullptr;
//					if (!it->second.loop) remove = true;
//				}
//			}
//			else
//			{
//				if (!it->second.loop) remove = true;
//			}
//
//			if (remove)
//				it = g_SfxChannels.erase(it);
//			else
//				++it;
//		}
//	}
//}
//
//bool Sound::Initialize()
//{
//	if (g_System) return true;
//
//	FMOD_RESULT r = FMOD::System_Create(&g_System);
//	if (!Check(r) || !g_System) return false;
//
//	// 채널 수를 512개로 넉넉하게 잡음
//	r = g_System->init(512, FMOD_INIT_NORMAL, nullptr);
//	if (!Check(r))
//	{
//		g_System->release();
//		g_System = nullptr;
//		return false;
//	}
//	return true;
//}
//
//void Sound::Shutdown()
//{
//	StopBGM();
//	StopAllSFX();
//
//	// SFX 채널 정리
//	for (auto& pair : g_SfxChannels)
//	{
//		if (pair.second.channel)
//		{
//			pair.second.channel->stop();
//			pair.second.channel = nullptr;
//		}
//	}
//	g_SfxChannels.clear();
//
//	// 로드된 모든 사운드 해제 (Sound Bank 비우기)
//	for (auto& pair : g_SoundBank)
//	{
//		if (pair.second)
//		{
//			pair.second->release();
//		}
//	}
//	g_SoundBank.clear();
//
//	if (g_System)
//	{
//		g_System->close();
//		g_System->release();
//		g_System = nullptr;
//	}
//}
//
//void Sound::Update()
//{
//	if (!g_System) return;
//	g_System->update();
//	CleanupSFX();
//}
//
//// 파일을 로드해서 맵에 저장
//bool Sound::Load(const std::wstring& key, const std::wstring& path, Type type)
//{
//	if (!g_System && !Initialize()) return false;
//
//	// 이미 로드되어 있는지 확인 (캐싱)
//	auto it = g_SoundBank.find(key);
//	if (it != g_SoundBank.end())
//	{
//		return true; // 이미 있음
//	}
//
//	// FMOD 사운드 생성
//	FMOD::Sound* newSound = nullptr;
//	std::string pathU8 = Utf8FromWString(path);
//	FMOD_MODE mode = FMOD_DEFAULT;
//
//	// 타입에 따라 플래그 결정
//	if (type == Type::BGM)
//	{
//		mode |= FMOD_CREATESTREAM; // 스트리밍 (디스크 읽기)
//		mode |= FMOD_LOOP_NORMAL;  // 무한 반복
//	}
//	else // SFX
//	{
//		mode |= FMOD_CREATESAMPLE; // 메모리 로드 (중첩 가능)
//		mode |= FMOD_LOOP_OFF;     // 기본값은 한 번 재생 (재생 시점에 루프 설정 가능)
//	}
//
//	FMOD_RESULT r = g_System->createSound(pathU8.c_str(), mode, nullptr, &newSound);
//	if (!Check(r) || !newSound)
//	{
//		// 로드 실패
//		return false;
//	}
//
//	//맵에 등록 (key로 저장)
//	g_SoundBank[key] = newSound;
//
//	return true;
//}
//
//// ================= BGM =================
//
//void Sound::PlayBGM(const std::wstring& key)
//{
//	if (!g_System) return;
//
//	// 로드 안 되어 있으면 재생 불가 (자동 로드 제거)
//	auto it = g_SoundBank.find(key);
//	if (it == g_SoundBank.end())
//	{
//		return; // 로드되지 않음
//	}
//
//	FMOD::Sound* sound = it->second;
//	if (!sound) return;
//
//	// 만약 이미 다른 BGM이 재생 중이면 정지
//	StopBGM();
//
//	// 재생
//	FMOD_RESULT r = g_System->playSound(sound, nullptr, false, &g_ChannelBGM);
//	if (Check(r) && g_ChannelBGM)
//	{
//		g_ChannelBGM->setVolume(g_VolBGM);
//		g_CurrentBGMKey = key;
//	}
//}
//
//void Sound::PauseBGM(bool pause)
//{
//	if (g_ChannelBGM)
//	{
//		g_ChannelBGM->setPaused(pause);
//	}
//}
//
//void Sound::StopBGM()
//{
//	if (g_ChannelBGM)
//	{
//		g_ChannelBGM->stop();
//		g_ChannelBGM = nullptr;
//	}
//	g_CurrentBGMKey.clear();
//}
//
//void Sound::SetBGMVolume(float volume)
//{
//	g_VolBGM = volume;
//	if (g_VolBGM < 0.0f) g_VolBGM = 0.0f;
//	if (g_VolBGM > 1.0f) g_VolBGM = 1.0f;
//
//	if (g_ChannelBGM)
//	{
//		g_ChannelBGM->setVolume(g_VolBGM);
//	}
//}
//
//bool Sound::IsBGMPlaying()
//{
//	if (!g_ChannelBGM) return false;
//	bool playing = false;
//	if (!Check(g_ChannelBGM->isPlaying(&playing))) return false;
//	return playing;
//}
//
//bool Sound::IsBGMPaused()
//{
//	if (!g_ChannelBGM) return false;
//	bool paused = false;
//	if (!Check(g_ChannelBGM->getPaused(&paused))) return false;
//	return paused;
//}
//
//bool Sound::SetBGMTimeSeconds(float sec)
//{
//	if (!g_ChannelBGM || g_CurrentBGMKey.empty()) return false;
//
//	auto it = g_SoundBank.find(g_CurrentBGMKey);
//	if (it == g_SoundBank.end() || !it->second) return false;
//
//	// 사운드 길이 확인
//	unsigned int lenMs = 0;
//	if (!Check(it->second->getLength(&lenMs, FMOD_TIMEUNIT_MS))) return false;
//	float lenSec = static_cast<float>(lenMs) / 1000.0f;
//
//	// 범위 클램프
//	if (sec < 0.0f) sec = 0.0f;
//	if (lenSec > 0.0f && sec > lenSec) sec = lenSec;
//
//	unsigned int posMs = static_cast<unsigned int>(sec * 1000.0f);
//	if (!Check(g_ChannelBGM->setPosition(posMs, FMOD_TIMEUNIT_MS))) return false;
//	return true;
//}
//
//float Sound::GetBGMTimeSeconds()
//{
//	if (!g_ChannelBGM) return 0.0f;
//	unsigned int posMs = 0;
//	if (!Check(g_ChannelBGM->getPosition(&posMs, FMOD_TIMEUNIT_MS))) return 0.0f;
//	return static_cast<float>(posMs) / 1000.0f;
//}
//
//float Sound::GetBGMLengthSeconds()
//{
//	if (g_CurrentBGMKey.empty()) return 0.0f;
//
//	auto it = g_SoundBank.find(g_CurrentBGMKey);
//	if (it == g_SoundBank.end() || !it->second) return 0.0f;
//
//	unsigned int lenMs = 0;
//	if (!Check(it->second->getLength(&lenMs, FMOD_TIMEUNIT_MS))) return 0.0f;
//	return static_cast<float>(lenMs) / 1000.0f;
//}
//
//std::wstring Sound::GetCurrentBGMKey()
//{
//	return g_CurrentBGMKey;
//}
//
//// ================= SFX =================
//
//bool Sound::PlaySFX(const std::wstring& key, float volume, float pitch, bool loop)
//{
//	if (!g_System) return false;
//
//	// 로드 안 되어 있으면 재생 불가 (자동 로드 제거)
//	auto it = g_SoundBank.find(key);
//	if (it == g_SoundBank.end())
//	{
//		return false; // 로드되지 않음
//	}
//
//	FMOD::Sound* sound = it->second;
//	if (!sound) return false;
//
//	// 볼륨, 피치 클램프
//	volume = std::clamp(volume, 0.0f, 1.0f);
//	pitch = std::clamp(pitch, 0.5f, 2.0f);
//
//	// 루프 모드 설정 (재생 시점에 결정)
//	FMOD_MODE mode;
//	if (Check(sound->getMode(&mode)))
//	{
//		if (loop)
//		{
//			mode |= FMOD_LOOP_NORMAL;
//		}
//		else
//		{
//			mode &= ~FMOD_LOOP_NORMAL;
//			mode |= FMOD_LOOP_OFF;
//		}
//		sound->setMode(mode);
//	}
//
//	if (loop)
//	{
//		// 루프 SFX: key별 하나의 채널만 관리
//		auto sfxIt = g_SfxChannels.find(key);
//		
//		// 이미 재생 중이면 재시작하지 않음 (볼륨/피치만 업데이트)
//		if (sfxIt != g_SfxChannels.end() && sfxIt->second.channel)
//		{
//			bool playing = false;
//			if (Check(sfxIt->second.channel->isPlaying(&playing)) && playing)
//			{
//				float finalVolume = volume * g_VolSFX;
//				sfxIt->second.channel->setVolume(finalVolume);
//				sfxIt->second.channel->setPitch(pitch);
//				return true;
//			}
//		}
//
//		// 재생 중이 아니면 재생 시작
//		if (sfxIt != g_SfxChannels.end() && sfxIt->second.channel)
//		{
//			// 기존 채널 정지
//			sfxIt->second.channel->stop();
//			sfxIt->second.channel = nullptr;
//	}
//
//		FMOD::Channel* channel = nullptr;
//		FMOD_RESULT r = g_System->playSound(sound, nullptr, false, &channel);
//
//		if (Check(r) && channel)
//		{
//			float finalVolume = volume * g_VolSFX;
//			channel->setVolume(finalVolume);
//			channel->setPitch(pitch);
//			
//			// 엔트리 업데이트
//			if (sfxIt == g_SfxChannels.end())
//			{
//				SfxEntry entry;
//				entry.channel = channel;
//				entry.loop = true;
//				g_SfxChannels[key] = entry;
//			}
//			else
//			{
//				sfxIt->second.channel = channel;
//				sfxIt->second.loop = true;
//			}
//			return true;
//		}
//		return false;
//	}
//	else
//	{
//		// 원샷 SFX: Fire and Forget 방식 (중첩 재생 가능)
//	FMOD::Channel* channel = nullptr;
//	FMOD_RESULT r = g_System->playSound(sound, nullptr, false, &channel);
//
//	if (Check(r) && channel)
//	{
//			float finalVolume = volume * g_VolSFX;
//			channel->setVolume(finalVolume);
//			channel->setPitch(pitch);
//		g_ChannelsSFX.push_back(channel);
//			return true;
//		}
//		return false;
//	}
//}
//
//bool Sound::IsSfxPlaying(const std::wstring& key)
//{
//	auto it = g_SfxChannels.find(key);
//	if (it == g_SfxChannels.end() || !it->second.channel) return false;
//
//	bool playing = false;
//	if (!Check(it->second.channel->isPlaying(&playing))) return false;
//	return playing;
//}
//
//void Sound::StopSfx(const std::wstring& key)
//{
//	auto it = g_SfxChannels.find(key);
//	if (it == g_SfxChannels.end()) return;
//
//	if (it->second.channel)
//	{
//		it->second.channel->stop();
//		it->second.channel = nullptr;
//	}
//
//	// 루프가 아닌 경우 엔트리 제거
//	if (!it->second.loop)
//	{
//		g_SfxChannels.erase(it);
//	}
//}
//
//void Sound::StopAllSFX()
//{
//	// 루프 SFX 정지
//	for (auto& pair : g_SfxChannels)
//	{
//		if (pair.second.channel)
//		{
//			pair.second.channel->stop();
//			pair.second.channel = nullptr;
//		}
//	}
//
//	// 원샷 SFX 정지
//	for (auto ch : g_ChannelsSFX)
//	{
//		if (ch)
//		{
//			ch->stop();
//		}
//	}
//	g_ChannelsSFX.clear();
//}
//
//// 가장 최근 효과음 정지
//void Sound::StopLastSFX()
//{
//	// 재생 중인 효과음이 없으면 리턴
//	if (g_ChannelsSFX.empty()) return;
//
//	// 벡터의 맨 뒤(back)가 가장 최근에 추가된 채널임
//	FMOD::Channel* lastCh = g_ChannelsSFX.back();
//
//	if (lastCh)
//	{
//		// 채널 정지
//		lastCh->stop();
//	}
//
//	// 관리 리스트에서 즉시 제거
//	g_ChannelsSFX.pop_back();
//}
//
//void Sound::SetSFXVolume(float volume)
//{
//	g_VolSFX = volume;
//	if (g_VolSFX < 0.0f) g_VolSFX = 0.0f;
//	if (g_VolSFX > 1.0f) g_VolSFX = 1.0f;
//
//	// 현재 재생 중인 모든 효과음 볼륨 조절
//	for (auto ch : g_ChannelsSFX)
//	{
//		if (ch)
//		{
//			ch->setVolume(g_VolSFX);
//		}
//	}
//}
//
//void Sound::SetSFXPitch(float pitch)
//{
//	g_PitchSFX = pitch;
//	if (g_PitchSFX < 0.5f) g_PitchSFX = 0.5f;
//	if (g_PitchSFX > 2.0f) g_PitchSFX = 2.0f;
//
//	// 현재 재생 중인 모든 효과음 피치 조절
//	for (auto ch : g_ChannelsSFX)
//	{
//		if (ch)
//		{
//			ch->setPitch(g_PitchSFX);
//		}
//	}
//}
//
//void Sound::SetSfxVolume(const std::wstring& key, float volume)
//{
//	volume = std::clamp(volume, 0.0f, 1.0f);
//	
//	auto it = g_SfxChannels.find(key);
//	if (it == g_SfxChannels.end() || !it->second.channel) return;
//
//	float finalVolume = volume * g_VolSFX;
//	it->second.channel->setVolume(finalVolume);
//}
//
//void Sound::SetSfxPitch(const std::wstring& key, float pitch)
//{
//	pitch = std::clamp(pitch, 0.5f, 2.0f);
//	
//	auto it = g_SfxChannels.find(key);
//	if (it == g_SfxChannels.end() || !it->second.channel) return;
//
//	it->second.channel->setPitch(pitch);
//}
//
//
//
////
////#include "SoundManager.h"
////
////#include <fmod.hpp>
////#include "../Common/Helper.h" // Utf8FromWString
////
////// 사용 중인 FMOD 라이브러리 이름은 프로젝트 설정에 따라 다를 수 있다.
////// 필요 시 이 pragma 를 수정하거나 제거해도 된다.
////#pragma comment(lib, "fmod_vc.lib")
////
////#include <unordered_map>
////#include <algorithm>
////
////namespace
////{
////	FMOD::System* g_System = nullptr;
////	FMOD::Sound* g_Music = nullptr;
////	FMOD::Channel* g_Channel = nullptr;
////	float          g_LengthSec = 0.0f;
////
////	inline bool Check(FMOD_RESULT r)
////	{
////		return (r == FMOD_OK);
////	}
////
////	struct SfxEntry
////	{
////		FMOD::Sound* sound = nullptr;
////		FMOD::Channel* channel = nullptr;
////		bool loop = false;
////	};
////
////	std::unordered_map<std::string, SfxEntry> g_Sfx;
////}
////
////bool Sound::Initialize()
////{
////	if (g_System) return true;
////
////	FMOD_RESULT r = FMOD::System_Create(&g_System);
////	if (!Check(r) || !g_System) return false;
////
////	r = g_System->init(512, FMOD_INIT_NORMAL, nullptr);
////	if (!Check(r))
////	{
////		g_System->release();
////		g_System = nullptr;
////		return false;
////	}
////	return true;
////}
////
////void Sound::Shutdown()
////{
////	UnloadAllSfx();   // 추가
////	UnloadMusic();
////
////	if (g_System)
////	{
////		g_System->close();
////		g_System->release();
////		g_System = nullptr;
////	}
////}
////
////bool Sound::LoadMusic(const std::wstring& pathW)
////{
////	if (!g_System && !Initialize()) return false;
////
////	UnloadMusic();
////
////	std::string pathU8 = Utf8FromWString(pathW);
////	FMOD_RESULT r = g_System->createSound(
////		pathU8.c_str(),
////		FMOD_DEFAULT | FMOD_CREATESTREAM,
////		nullptr,
////		&g_Music);
////	if (!Check(r) || !g_Music)
////	{
////		g_Music = nullptr;
////		return false;
////	}
////
////	// 길이(ms)를 초 단위로 캐시
////	unsigned int lenMs = 0;
////	r = g_Music->getLength(&lenMs, FMOD_TIMEUNIT_MS);
////	if (Check(r))
////	{
////		g_LengthSec = static_cast<float>(lenMs) / 1000.0f;
////	}
////	else
////	{
////		g_LengthSec = 0.0f;
////	}
////
////	return true;
////}
////
////void Sound::UnloadMusic()
////{
////	if (g_Channel)
////	{
////		g_Channel->stop();
////		g_Channel = nullptr;
////	}
////	if (g_Music)
////	{
////		g_Music->release();
////		g_Music = nullptr;
////	}
////	g_LengthSec = 0.0f;
////}
////
////void Sound::Play()
////{
////	if (!g_System || !g_Music) return;
////
////	// 이미 채널이 있으면 일시정지만 해제
////	if (g_Channel)
////	{
////		g_Channel->setPaused(false);
////		return;
////	}
////
////	FMOD_RESULT r = g_System->playSound(g_Music, nullptr, false, &g_Channel);
////	if (!Check(r))
////	{
////		g_Channel = nullptr;
////	}
////}
////
////void Sound::Pause(bool pause)
////{
////	if (!g_Channel) return;
////	g_Channel->setPaused(pause);
////}
////
////void Sound::Stop()
////{
////	if (!g_Channel) return;
////	g_Channel->stop();
////	g_Channel = nullptr;
////}
////
////bool Sound::IsPlaying()
////{
////	if (!g_Channel) return false;
////	bool playing = false;
////	if (!Check(g_Channel->isPlaying(&playing))) return false;
////	return playing;
////}
////
////bool Sound::IsPaused()
////{
////	if (!g_Channel) return false;
////	bool paused = false;
////	if (!Check(g_Channel->getPaused(&paused))) return false;
////	return paused;
////}
////
////bool Sound::SetTimeSeconds(float sec)
////{
////	if (!g_System || !g_Music) return false;
////
////	// 범위 클램프
////	if (sec < 0.0f) sec = 0.0f;
////	if (g_LengthSec > 0.0f && sec > g_LengthSec) sec = g_LengthSec;
////
////	// 채널이 없으면 일시정지 상태로 하나 만들어 둔다 (타임라인 스크럽용)
////	if (!g_Channel)
////	{
////		FMOD_RESULT rPlay = g_System->playSound(g_Music, nullptr, true, &g_Channel);
////		if (!Check(rPlay) || !g_Channel)
////		{
////			g_Channel = nullptr;
////			return false;
////		}
////	}
////
////	unsigned int posMs = static_cast<unsigned int>(sec * 1000.0f);
////	if (!Check(g_Channel->setPosition(posMs, FMOD_TIMEUNIT_MS))) return false;
////	return true;
////}
////
////float Sound::GetTimeSeconds()
////{
////	if (!g_Channel) return 0.0f;
////	unsigned int posMs = 0;
////	if (!Check(g_Channel->getPosition(&posMs, FMOD_TIMEUNIT_MS))) return 0.0f;
////	return static_cast<float>(posMs) / 1000.0f;
////}
////
////float Sound::GetLengthSeconds()
////{
////	return g_LengthSec;
////}
////
////void Sound::Update()
////{
////	if (!g_System) return;
////	g_System->update();
////}
////
////// ===================== SFX =====================
////bool Sound::LoadSfx(const std::string& key, const std::wstring& pathW, bool loop)
////{
////	if (!g_System && !Initialize()) return false;
////
////	// 기존 있으면 해제
////	UnloadSfx(key);
////
////	std::string pathU8 = Utf8FromWString(pathW);
////
////	FMOD::Sound* s = nullptr;
////	FMOD_RESULT r = g_System->createSound(
////		pathU8.c_str(),
////		FMOD_DEFAULT,   // SFX는 보통 stream 필요 없음
////		nullptr,
////		&s);
////
////	if (!Check(r) || !s) return false;
////
////	// loop 설정
////	s->setMode(loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
////
////	SfxEntry e;
////	e.sound = s;
////	e.channel = nullptr;
////	e.loop = loop;
////	g_Sfx[key] = e;
////	return true;
////}
////
////void Sound::UnloadSfx(const std::string& key)
////{
////	auto it = g_Sfx.find(key);
////	if (it == g_Sfx.end()) return;
////
////	if (it->second.channel)
////	{
////		it->second.channel->stop();
////		it->second.channel = nullptr;
////	}
////	if (it->second.sound)
////	{
////		it->second.sound->release();
////		it->second.sound = nullptr;
////	}
////	g_Sfx.erase(it);
////}
////
////void Sound::UnloadAllSfx()
////{
////	for (auto& kv : g_Sfx)
////	{
////		if (kv.second.channel) kv.second.channel->stop();
////		if (kv.second.sound)   kv.second.sound->release();
////		kv.second.channel = nullptr;
////		kv.second.sound = nullptr;
////	}
////	g_Sfx.clear();
////}
////
////bool Sound::IsSfxPlaying(const std::string& key)
////{
////	auto it = g_Sfx.find(key);
////	if (it == g_Sfx.end() || !it->second.channel) return false;
////
////	bool playing = false;
////	if (!Check(it->second.channel->isPlaying(&playing))) return false;
////	return playing;
////}
////
////bool Sound::PlaySfx(const std::string& key, float volume, bool restart, float pitch)
////{
////	auto it = g_Sfx.find(key);
////	if (it == g_Sfx.end() || !g_System || !it->second.sound) return false;
////
////	// pitch clamp(너무 극단 방지)
////	pitch = std::clamp(pitch, 0.5f, 2.0f);
////	volume = std::clamp(volume, 0.0f, 1.0f);
////
////	// 이미 재생 중이고 restart=false면 재시작하지 않음
////	if (!restart && IsSfxPlaying(key))
////	{
////		it->second.channel->setVolume(volume);
////		it->second.channel->setPitch(pitch);
////		return true;
////	}
////
////	// restart면 기존 채널 정지
////	if (it->second.channel)
////	{
////		it->second.channel->stop();
////		it->second.channel = nullptr;
////	}
////
////	FMOD::Channel* ch = nullptr;
////	FMOD_RESULT r = g_System->playSound(it->second.sound, nullptr, false, &ch);
////	if (!Check(r) || !ch) return false;
////
////	ch->setVolume(volume);
////	ch->setPitch(pitch);
////
////	it->second.channel = ch;
////	return true;
////}
////
////void Sound::StopSfx(const std::string& key)
////{
////	auto it = g_Sfx.find(key);
////	if (it == g_Sfx.end()) return;
////
////	if (it->second.channel)
////	{
////		it->second.channel->stop();
////		it->second.channel = nullptr;
////	}
////}
////
////void Sound::SetSfxPitch(const std::string& key, float pitch)
////{
////	auto it = g_Sfx.find(key);
////	if (it == g_Sfx.end() || !it->second.channel) return;
////	pitch = std::clamp(pitch, 0.5f, 2.0f);
////	it->second.channel->setPitch(pitch);
////}
////
////void Sound::SetSfxVolume(const std::string& key, float volume)
////{
////	auto it = g_Sfx.find(key);
////	if (it == g_Sfx.end() || !it->second.channel) return;
////	volume = std::clamp(volume, 0.0f, 1.0f);
////	it->second.channel->setVolume(volume);
////}
////
////
