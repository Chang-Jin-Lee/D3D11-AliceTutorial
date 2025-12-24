/**
	@file      TimeSystem.h
	@brief     Thanks Frank Luna, this class is based on his code.
	@author    SKYFISH
	@date      JUNE.2023
	@notice    물방울 책의 코드이며 네이밍 일관성 때문에 파일명만 변경			
**/

#pragma once

class GameTimer
{
public:
	GameTimer();

	float TotalTime()const;  // in seconds
	float DeltaTime()const; // in seconds

	void Reset(); // Call before message loop.
	void Start(); // Call when unpaused.
	void Stop();  // Call when paused.
	void Tick();  // Call every frame.
	static GameTimer* m_Instance;
private:
	double mSecondsPerCount;
	double mDeltaTime;

	__int64 mBaseTime;
	__int64 mPausedTime;
	__int64 mStopTime;
	__int64 mPrevTime;
	__int64 mCurrTime;

	bool mStopped;

};