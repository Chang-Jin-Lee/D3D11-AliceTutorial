#pragma once

// @brief  : VMD 카메라 프레임을 이용해 DirectX 카메라를 구동하는 간단한 헬퍼

#include <vector>
#include <string>
#include <DirectXMath.h>

#include "Vmd.h"

namespace mmd
{
	// @brief VMD 카메라 재생 상태
	struct VmdCameraState
	{
		std::vector<vmd::VmdCameraFrame> frames; // 정렬된 카메라 키프레임
		bool    use = false;                     // 사용 여부 (false면 기본 카메라)
		bool    playing = false;                 // 재생 중 여부
		float   currentFrame = 0.0f;             // 현재 프레임 (부동소수)
		float   frameRate = 30.0f;               // 초당 프레임 수
		float   scale = 1.0f;                    // MMD → 엔진 좌표 스케일
	};

	// @brief  VMD 파일에서 카메라 프레임만 안전하게 읽는다.
	// @param  vmdPath : *.vmd 파일 경로(UTF-16)
	// @return 성공 시 true, 실패 시 false
	bool LoadVmdCameraFromFile(const std::wstring& vmdPath, VmdCameraState& outState);

	// @brief  재생 상태를 dt 만큼 업데이트 (루프 재생 포함)
	void UpdateVmdCamera(float dt, VmdCameraState& state);

	// @brief  현재 상태에서 View/Proj/Eye 를 계산한다.
	// @param  defaultFovRad : cur.angle 이 0 또는 비정상일 때 사용할 기본 FOV(rad)
	// @return 사용 가능하면 true (outView/outProj/outEye 채워짐), 아니면 false
	bool EvaluateVmdCamera(
		const VmdCameraState& state,
		float defaultFovRad,
		float aspect,
		float nearZ,
		float farZ,
		DirectX::XMMATRIX& outView,
		DirectX::XMMATRIX& outProj,
		DirectX::XMFLOAT3& outEye);
}


