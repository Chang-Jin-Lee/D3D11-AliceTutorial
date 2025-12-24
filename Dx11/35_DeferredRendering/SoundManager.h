#pragma once

// @brief  : FMOD 사운드 제어를 아주 단순하게 감싸는 헬퍼

#include <string>

namespace Sound
{
	// @brief FMOD 시스템 초기화 (프로그램 시작 시 1회)
	bool Initialize();

	// @brief FMOD 시스템 해제 (프로그램 종료 시 1회)
	void Shutdown();

	// @brief 배경 음악/노래 로드 (기존 사운드는 자동 해제)
	// @param pathW : 파일 경로 (Wide 문자열, 윈도우 경로 그대로 사용)
	bool LoadMusic(const std::wstring& pathW);

	// @brief 로드된 사운드 해제
	void UnloadMusic();

	// @brief 재생 (채널이 없으면 새로 생성, 있으면 일시정지 해제)
	void Play();

	// @brief 일시정지/해제
	void Pause(bool pause);

	// @brief 정지 (채널을 멈추고 해제)
	void Stop();

	// @brief 현재 재생 여부
	bool IsPlaying();

	// @brief 현재 일시정지 여부
	bool IsPaused();

	// @brief 재생 위치 설정 (초 단위). 범위를 자동으로 0~길이로 클램프.
	bool SetTimeSeconds(float sec);

	// @brief 현재 재생 위치 (초 단위). 채널이 없으면 0 리턴.
	float GetTimeSeconds();

	// @brief 사운드 전체 길이 (초 단위). 로드되지 않았으면 0.
	float GetLengthSeconds();

	// @brief 매 프레임 호출(FMOD::System::update)
	void Update();
}


