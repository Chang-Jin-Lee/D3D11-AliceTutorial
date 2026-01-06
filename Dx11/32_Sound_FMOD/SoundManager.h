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
	// @param path : 파일 경로 (Wide 문자열)
	// @param type : BGM(스트리밍) 또는 SFX(샘플)
	// @return 성공 여부
	bool Load(const std::wstring& path, Type type);

	// ================= BGM 제어 =================
	// @brief 해당 경로의 BGM 재생 (로드 안 되어 있으면 자동 로드)
	void PlayBGM(const std::wstring& path);

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

	// ================= SFX 제어 =================
	// @brief 해당 경로의 효과음 재생 (로드 안 되어 있으면 자동 로드, 중첩 재생 가능)
	void PlaySFX(const std::wstring& path);

	// @brief 현재 재생 중인 모든 SFX 정지
	void StopAllSFX();

	// @brief 가장 최근에 재생된 SFX 하나만 정지
    void StopLastSFX();

	// @brief SFX 볼륨 설정 (0.0 ~ 1.0)
	void SetSFXVolume(float volume);

	// @brief 경로로 SFX 중지 
	void StopSFX(const std::wstring& path);
}
