#pragma once

// @brief  : FMOD 사운드 제어를 아주 단순하게 감싸는 헬퍼 (리소스 매니저 패턴)

#include <string>

namespace Sound
{
	// 사운드 로드 방식
	enum class Type
	{
		BGM, // 스트리밍 (긴 음악, 메모리 적게 먹음)
		SFX  // 샘플 (짧은 효과음, 메모리 로드, 중첩 재생 가능)
	};

	// @brief FMOD 시스템 초기화 (프로그램 시작 시 1회)
	bool Initialize();

	// @brief FMOD 시스템 해제 (프로그램 종료 시 1회)
	void Shutdown();

	// @brief 매 프레임 호출(FMOD::System::update)
	void Update();

	// @brief 사운드를 미리 로드 (메모리에 캐싱). 
	//        (실제 재생 시 렉을 줄이기 위해 스테이지 시작 전 호출 권장)
	// @param key : 사운드를 식별할 이름 (예: "Walk", "Shoot")
	// @param path : 파일 경로 (Wide 문자열)
	// @param type : BGM(스트리밍) 또는 SFX(샘플)
	// @return 성공 여부
	bool Load(const std::wstring& key, const std::wstring& path, Type type);

	// ================= BGM 제어 =================
	// @brief 해당 key의 BGM 재생 (로드 안 되어 있으면 자동 로드 불가, 먼저 Load 호출 필요)
	void PlayBGM(const std::wstring& key);

	// @brief BGM 일시정지/해제
	void PauseBGM(bool pause);

	// @brief BGM 정지
	void StopBGM();

	// @brief BGM 볼륨 설정 (0.0 ~ 1.0)
	void SetBGMVolume(float volume);

	// @brief 현재 BGM 재생 여부
	bool IsBGMPlaying();

	// @brief 현재 BGM 일시정지 여부
	bool IsBGMPaused();

	// @brief BGM 재생 위치 설정 (초 단위)
	bool SetBGMTimeSeconds(float sec);

	// @brief 현재 BGM 재생 위치 (초 단위)
	float GetBGMTimeSeconds();

	// @brief 현재 BGM 전체 길이 (초 단위)
	float GetBGMLengthSeconds();

	// @brief 현재 재생 중인 BGM key 반환 (재생 중이 아니면 빈 문자열)
	std::wstring GetCurrentBGMKey();

	// ================= SFX 제어 =================
	// @brief 해당 key의 효과음 재생 (로드 안 되어 있으면 자동 로드 불가, 먼저 Load 호출 필요)
	// @param key : 사운드 key
	// @param volume : 볼륨 (0.0 ~ 1.0, 기본값: 1.0)
	// @param pitch : 피치 (0.5 ~ 2.0, 기본값: 1.0)
	// @param loop : true면 루프 재생(계속 반복), false면 한 번 재생하고 끝 (기본값: false)
	// @return 재생 성공 여부
	bool PlaySFX(const std::wstring& key, float volume = 1.0f, float pitch = 1.0f, bool loop = false);

	// @brief 특정 key의 SFX 재생 여부 확인
	bool IsSfxPlaying(const std::wstring& key);

	// @brief 특정 key의 SFX 정지 (루프 SFX에 유용)
	void StopSfx(const std::wstring& key);

	// @brief 현재 재생 중인 모든 SFX 정지
	void StopAllSFX();

	// @brief 가장 최근에 재생된 SFX 하나만 정지
	void StopLastSFX();

	// @brief SFX 볼륨 설정 (0.0 ~ 1.0) - 전역 볼륨
	void SetSFXVolume(float volume);

	// @brief SFX 피치 설정 (0.5 ~ 2.0) - 전역 피치
	void SetSFXPitch(float pitch);

	// @brief 특정 key의 SFX 볼륨 설정 (0.0 ~ 1.0)
	void SetSfxVolume(const std::wstring& key, float volume);

	// @brief 특정 key의 SFX 피치 설정 (0.5 ~ 2.0)
	void SetSfxPitch(const std::wstring& key, float pitch);
}


//
//#pragma once
//
//// @brief  : FMOD 사운드 제어를 아주 단순하게 감싸는 헬퍼
//
//#include <string>
//
//namespace Sound
//{
//	// @brief FMOD 시스템 초기화 (프로그램 시작 시 1회)
//	bool Initialize();
//
//	// @brief FMOD 시스템 해제 (프로그램 종료 시 1회)
//	void Shutdown();
//
//	// @brief 배경 음악/노래 로드 (기존 사운드는 자동 해제)
//	// @param pathW : 파일 경로 (Wide 문자열, 윈도우 경로 그대로 사용)
//	bool LoadMusic(const std::wstring& pathW);
//
//	// @brief 로드된 사운드 해제
//	void UnloadMusic();
//
//	// @brief 재생 (채널이 없으면 새로 생성, 있으면 일시정지 해제)
//	void Play();
//
//	// @brief 일시정지/해제
//	void Pause(bool pause);
//
//	// @brief 정지 (채널을 멈추고 해제)
//	void Stop();
//
//	// @brief 현재 재생 여부
//	bool IsPlaying();
//
//	// @brief 현재 일시정지 여부
//	bool IsPaused();
//
//	// @brief 재생 위치 설정 (초 단위). 범위를 자동으로 0~길이로 클램프.
//	bool SetTimeSeconds(float sec);
//
//	// @brief 현재 재생 위치 (초 단위). 채널이 없으면 0 리턴.
//	float GetTimeSeconds();
//
//	// @brief 사운드 전체 길이 (초 단위). 로드되지 않았으면 0.
//	float GetLengthSeconds();
//
//	// @brief 매 프레임 호출(FMOD::System::update)
//	void Update();
//
//	// ===================== SFX =====================
//	// SFX
//	bool LoadSfx(const std::string& key, const std::wstring& pathW, bool loop = false);
//	void UnloadSfx(const std::string& key);
//	void UnloadAllSfx();
//
//	// restart=false면 "이미 재생중이면 그대로 유지" (발자국 루프에 유용)
//	bool PlaySfx(const std::string& key, float volume = 1.0f, bool restart = true, float pitch = 1.0f);
//	void StopSfx(const std::string& key);
//
//	bool IsSfxPlaying(const std::string& key);
//	void SetSfxPitch(const std::string& key, float pitch);
//	void SetSfxVolume(const std::string& key, float volume);
//}
//
//
