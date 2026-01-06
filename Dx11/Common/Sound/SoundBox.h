#pragma once
#include <functional>
#include <DirectXMath.h>
#include <string>
#include <algorithm>
#include <cmath>
using namespace DirectX;

// 여러 모델을 그리기 위한 구조체
// SoundBox 구조체 (영역 기반 BGM 재생)
struct SoundBox
{
	std::wstring bgmKey;      // 이 구역에서 틀어야 할 BGM 키

	// Transform 정보
	XMFLOAT3 position = { 0, 0, 0 };
	XMFLOAT3 scale = { 1, 1, 1 };

	// Local AABB (기본 크기)
	XMFLOAT3 boundsMin = { -10.0f, -10.0f, -10.0f };
	XMFLOAT3 boundsMax = { 10.0f, 10.0f, 10.0f };

	// 콜백 함수
	std::function<void()> onEnter;  // 박스에 들어왔을 때 호출
	std::function<void()> onExit;   // 박스에서 나갔을 때 호출

	// ====== 중심점으로 가까이 갈수록록 감쇄효과 ======
	float edgeVolume = 0.0f;   // 박스 경계에서의 볼륨(0~1)
	float centerVolume = 1.0f; // 박스 중심에서의 볼륨(0~1)
	float curve = 1.0f;         // 1=선형, >1이면 중심 근처에서 더 빨리 커짐

	// 3D 거리 감쇄
	float minDist = 1.0f;
	float maxDist = 50.0f;

	// ====== 테스트용 좌/우 오프셋(3D) ======
	float lrWeight = 0.0f;      // -1(왼쪽) ~ +1(오른쪽)
	float lrMaxMeters = 0.0f;   // lrWeight를 몇 m까지 밀지 판단함(0이면 비활성)

	// ====== 런타임용 인스턴스 키 ======
	std::wstring instanceId;

	// 플레이어가 내부에 있는지 확인하는 함수
	bool Contains(const XMFLOAT3& playerPos) const
	{
		// 1. Local AABB -> World AABB 변환
		XMFLOAT3 worldMin, worldMax;

		float x0 = boundsMin.x * scale.x + position.x;
		float x1 = boundsMax.x * scale.x + position.x;
		worldMin.x = std::min(x0, x1);
		worldMax.x = std::max(x0, x1);

		float y0 = boundsMin.y * scale.y + position.y;
		float y1 = boundsMax.y * scale.y + position.y;
		worldMin.y = std::min(y0, y1);
		worldMax.y = std::max(y0, y1);

		float z0 = boundsMin.z * scale.z + position.z;
		float z1 = boundsMax.z * scale.z + position.z;
		worldMin.z = std::min(z0, z1);
		worldMax.z = std::max(z0, z1);

		// 2. Point(플레이어) vs AABB(박스) 충돌 검사
		if (playerPos.x >= worldMin.x && playerPos.x <= worldMax.x &&
			playerPos.y >= worldMin.y && playerPos.y <= worldMax.y &&
			playerPos.z >= worldMin.z && playerPos.z <= worldMax.z)
		{
			return true;
		}

		return false;
	}

	// ====== 경계->중심 가중치(0~1) 계산 ======
	float CenterWeight01(const XMFLOAT3& playerPos) const
	{
		// 월드 AABB
		XMFLOAT3 wmin, wmax;

		float x0 = boundsMin.x * scale.x + position.x;
		float x1 = boundsMax.x * scale.x + position.x;
		wmin.x = std::min(x0, x1);
		wmax.x = std::max(x0, x1);

		float y0 = boundsMin.y * scale.y + position.y;
		float y1 = boundsMax.y * scale.y + position.y;
		wmin.y = std::min(y0, y1);
		wmax.y = std::max(y0, y1);

		float z0 = boundsMin.z * scale.z + position.z;
		float z1 = boundsMax.z * scale.z + position.z;
		wmin.z = std::min(z0, z1);
		wmax.z = std::max(z0, z1);

		// 중심/반-크기
		XMFLOAT3 c = { (wmin.x + wmax.x) * 0.5f, (wmin.y + wmax.y) * 0.5f, (wmin.z + wmax.z) * 0.5f };
		XMFLOAT3 e = { (wmax.x - wmin.x) * 0.5f, (wmax.y - wmin.y) * 0.5f, (wmax.z - wmin.z) * 0.5f };

		// 안전 처리(0 나눗셈 방지)
		e.x = (e.x < 1e-6f) ? 1e-6f : e.x;
		e.y = (e.y < 1e-6f) ? 1e-6f : e.y;
		e.z = (e.z < 1e-6f) ? 1e-6f : e.z;

		float nx = std::fabs(playerPos.x - c.x) / e.x;
		float ny = std::fabs(playerPos.y - c.y) / e.y;
		float nz = std::fabs(playerPos.z - c.z) / e.z;

		// 박스 경계에서 max(nx,ny,nz)=1, 중심에서 0
		float u = std::max(nx, std::max(ny, nz));
		u = std::clamp(u, 0.0f, 1.0f);

		float w = 1.0f - u;                 // 경계 0 -> 중심 1
		w = std::pow(w, std::max(curve, 0.0001f));
		return std::clamp(w, 0.0f, 1.0f);
	}
};