#pragma once

#include "SoundBox.h"
#include <vector>
#include <DirectXMath.h>
#include <string>
#include "SoundManager.h"

using namespace DirectX;
// SoundBox 시스템
class SoundBoxSystem
{
private:
	std::vector<SoundBox> m_SoundBoxes;
	std::vector<bool> m_WasInside;  // 이전 프레임에 박스 안에 있었는지 추적

public:
	// @brief SoundBox를 시스템에 등록
	// - box를 값으로 받아서 내부 벡터에 보관한다.
	// - 외부에서 instanceId를 지정하지 않았다면 여기서 "SoundBox#N" 형태로 자동 생성한다.
	// - 예전에는 여기서 바로 3D 사운드를 Play3DInstance로 재생했지만,
	//   이제는 App 쪽 onEnter/onExit 콜백에서 재생/정지를 제어한다.
	void AddBox(SoundBox box)
	{
		// 외부에서 instanceId를 이미 지정했다면 그대로 사용
		if (box.instanceId.empty())
		{
			box.instanceId = L"SoundBox#" + std::to_wstring(m_SoundBoxes.size());
		}

		m_SoundBoxes.push_back(std::move(box));
		m_WasInside.push_back(false);  // 초기값: 박스 밖

		// 예전 코드(자동 재생)는 주석 처리
		//    - 박스 밖에서도 시간이 계속 흘러가서, 다시 들어갈 때 중간부터 재생되는 문제가 있었다.
		//    - 이제는 onEnter에서 Stop3DInstance + Play3DInstance 를 호출해
		//      "항상 0초부터 재생" 되도록 한다.
		// Sound::Play3DInstance(m_SoundBoxes.back().instanceId, m_SoundBoxes.back().bgmKey, true);
	}

	void Clear()
	{
		// 모든 3D 인스턴스 정지
		for (auto& box : m_SoundBoxes)
		{
			Sound::Stop3DInstance(box.instanceId);
		}

		m_SoundBoxes.clear();
		m_WasInside.clear();
	}

	// 매 프레임 호출
	void Update(const XMFLOAT3& playerPos)
	{
		for (size_t i = 0; i < m_SoundBoxes.size(); ++i)
		{
			auto& box = m_SoundBoxes[i];
			bool isInside = box.Contains(playerPos);

			// 진입 감지: 이전 프레임에는 밖에 있었고, 지금은 안에 있음
			if (isInside && !m_WasInside[i])
			{
				if (box.onEnter)
				{
					box.onEnter();
				}
			}
			// 이탈 감지: 이전 프레임에는 안에 있었고, 지금은 밖에 있음
			else if (!isInside && m_WasInside[i])
			{
				if (box.onExit)
				{
					box.onExit();
				}
			}

			// 상태 업데이트
			m_WasInside[i] = isInside;

			// ===== 볼륨 계산 (경계->중심) =====
			float vol = 0.0f;
			if (isInside)
			{
				float w = box.CenterWeight01(playerPos);
				vol = box.edgeVolume + (box.centerVolume - box.edgeVolume) * w;
			}

			// ===== 3D 소스 위치(박스 중심) =====
			// 간단하게 position을 중심으로 사용함. bounds가 비대칭이면 CenterWeight01처럼 중심 계산해서 쓰면 더 정확함
			XMFLOAT3 srcPos = box.position;

			// 테스트용 좌/우 오프셋
			if (box.lrMaxMeters > 0.0f)
				srcPos.x += box.lrWeight * box.lrMaxMeters;

			Sound::Update3DInstance(box.instanceId, srcPos, vol, box.minDist, box.maxDist);
		}
	}

	const std::vector<SoundBox>& GetBoxes() const { return m_SoundBoxes; }
	std::vector<SoundBox>& GetBoxes() { return m_SoundBoxes; }
};