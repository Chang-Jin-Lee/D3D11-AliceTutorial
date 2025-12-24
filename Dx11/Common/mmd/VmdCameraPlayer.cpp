#include "pch.h"
#include "VmdCameraPlayer.h"

#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
	// VMD 카메라 프레임 → DirectX 카메라(Eye / Center / Up) 변환
	//  - 22_VMD 예제의 VmdCameraToEyeCenterUp 를 옮겨온 코드입니다.
	static void VmdCameraToEyeCenterUp(const vmd::VmdCameraFrame& cam, float scale,
		XMFLOAT3& outEye, XMFLOAT3& outCenter, XMFLOAT3& outUp)
	{
		using std::cosf;
		using std::sinf;
		using std::fabsf;

		// 0) VMD 원본 값
		const float px = cam.position[0];
		const float py = cam.position[1];
		const float pz = cam.position[2];
		const float pitch = cam.orientation[0]; // X
		const float yaw = cam.orientation[1];   // Y
		const float roll = cam.orientation[2];  // Z
		const float dist = fabsf(cam.distance) * scale; // 항상 양수 + 스케일

		// 1) MMD → OpenGL(RH) 좌표 : 관심점 Z축 반전, 스케일 적용
		const float interestGL_x = px * scale;
		const float interestGL_y = py * scale;
		const float interestGL_z = -pz * scale;

		// 2) 회전 : MMD 기준 카메라 회전 순서 적용
		auto rotateMMD = [&](const XMFLOAT3& v) -> XMFLOAT3
		{
			float x = v.x;
			float y = v.y;
			float z = v.z;

			// pitch : +X 축 회전
			{
				const float cp = cosf(pitch);
				const float sp = sinf(pitch);
				const float ny = y * cp - z * sp;
				const float nz = y * sp + z * cp;
				y = ny; z = nz;
			}

			// roll : -Z 축 회전 == +Z 축으로 -roll 회전
			{
				const float cr = cosf(-roll);
				const float sr = sinf(-roll);
				const float nx = x * cr - y * sr;
				const float ny = x * sr + y * cr;
				x = nx; y = ny;
			}

			// yaw : +Y 축 회전
			{
				const float cy = cosf(yaw);
				const float sy = sinf(yaw);
				const float nx = x * cy + z * sy;
				const float nz = -x * sy + z * cy;
				x = nx; z = nz;
			}

			return XMFLOAT3(x, y, z);
		};

		// OpenGL(RH) 공간에서의 offset / forward / up
		const XMFLOAT3 localOffsetGL(0.0f, 0.0f, dist);     // 카메라가 관심점에서 +Z 방향으로 떨어진 거리
		const XMFLOAT3 forwardLocalGL(0.0f, 0.0f, -1.0f);   // 카메라가 바라보는 -Z
		const XMFLOAT3 upLocalGL(0.0f, 1.0f, 0.0f);         // +Y

		XMFLOAT3 offsetGL = rotateMMD(localOffsetGL);
		XMFLOAT3 forwardGL = rotateMMD(forwardLocalGL);
		XMFLOAT3 upGL = rotateMMD(upLocalGL);

		// 관심점 + offset 이 eye, forward 가 center-eye
		XMFLOAT3 eyeGL(
			interestGL_x + offsetGL.x,
			interestGL_y + offsetGL.y,
			interestGL_z + offsetGL.z
		);
		XMFLOAT3 centerGL(
			eyeGL.x + forwardGL.x,
			eyeGL.y + forwardGL.y,
			eyeGL.z + forwardGL.z
		);

		// 3) OpenGL(RH) → DirectX(LH) : Z축 반전
		outEye = XMFLOAT3(eyeGL.x, eyeGL.y, -eyeGL.z);
		outCenter = XMFLOAT3(centerGL.x, centerGL.y, -centerGL.z);
		outUp = XMFLOAT3(upGL.x, upGL.y, -upGL.z);
	}

	// VMD 파일에서 카메라 프레임만 읽어오는 내부 헬퍼
	static bool ReadVmdCameraFramesOnly(const std::wstring& vmdPath, std::vector<vmd::VmdCameraFrame>& outFrames)
	{
		outFrames.clear();

		try
		{
			std::filesystem::path path(vmdPath);
			std::ifstream stream(path, std::ios::binary);
			if (!stream || !stream.good())
				return false;

			// 헤더 확인
			char buffer[30];
			stream.read(buffer, 30);
			if (stream.gcount() != 30 || strncmp(buffer, "Vocaloid Motion Data", 20) != 0)
				return false;

			// 모델 이름(20바이트) 스킵
			stream.read(buffer, 20);

			// 본 키프레임 개수 스킵
			int boneCount = 0;
			stream.read((char*)&boneCount, sizeof(int));
			if (boneCount < 0 || boneCount > 10000000) return false;
			// 본 프레임 구조체 크기: 15+4+12+16+64 = 111 byte
			stream.seekg((std::streamoff)boneCount * 111, std::ios::cur);

			// 표정 키프레임 스킵
			int faceCount = 0;
			stream.read((char*)&faceCount, sizeof(int));
			if (faceCount < 0 || faceCount > 10000000) return false;
			// 표정 프레임 크기: 15+4+4 = 23 byte
			stream.seekg((std::streamoff)faceCount * 23, std::ios::cur);

			// 카메라 프레임 수
			int cameraCount = 0;
			stream.read((char*)&cameraCount, sizeof(int));
			if (cameraCount < 0 || cameraCount > 10000000) return false;

			outFrames.resize((size_t)cameraCount);
			for (int i = 0; i < cameraCount; ++i)
			{
				outFrames[(size_t)i].Read(&stream);
			}

			return !outFrames.empty();
		}
		catch (...)
		{
			return false;
		}
	}
}

namespace mmd
{
	bool LoadVmdCameraFromFile(const std::wstring& vmdPath, VmdCameraState& outState)
	{
		std::vector<vmd::VmdCameraFrame> frames;
		if (!ReadVmdCameraFramesOnly(vmdPath, frames) || frames.empty())
			return false;

		// 프레임 번호 기준 정렬
		std::sort(frames.begin(), frames.end(),
			[](const vmd::VmdCameraFrame& a, const vmd::VmdCameraFrame& b) { return a.frame < b.frame; });

		outState.frames = std::move(frames);
		outState.use = true;
		outState.playing = true;
		outState.currentFrame = static_cast<float>(outState.frames.front().frame);
		// frameRate/scale 은 기존 값 유지
		return true;
	}

	void UpdateVmdCamera(float dt, VmdCameraState& state)
	{
		if (!state.use || !state.playing || state.frames.empty())
			return;

		state.currentFrame += dt * state.frameRate;

		const int firstFrame = state.frames.front().frame;
		const int lastFrame = state.frames.back().frame;
		if (state.currentFrame > (float)lastFrame)
		{
			state.currentFrame = (float)firstFrame;
		}
	}

	bool EvaluateVmdCamera(
		const VmdCameraState& state,
		float defaultFovRad,
		float aspect,
		float nearZ,
		float farZ,
		XMMATRIX& outView,
		XMMATRIX& outProj,
		XMFLOAT3& outEye)
	{
		if (!state.use || state.frames.empty() || aspect <= 0.0f || nearZ <= 0.0f || farZ <= nearZ)
			return false;

		const auto& frames = state.frames;
		float curF = state.currentFrame;

		// curF 앞뒤 키프레임 찾기
		const vmd::VmdCameraFrame* a = &frames.front();
		const vmd::VmdCameraFrame* b = &frames.back();

		for (size_t i = 0; i + 1 < frames.size(); ++i)
		{
			if (curF >= (float)frames[i].frame && curF <= (float)frames[i + 1].frame)
			{
				a = &frames[i];
				b = &frames[i + 1];
				break;
			}
		}

		float t = 0.0f;
		if (b->frame != a->frame)
		{
			t = (curF - (float)a->frame) / (float)(b->frame - a->frame);
		}

		// 선형 보간
		vmd::VmdCameraFrame cur = *a;
		for (int i = 0; i < 3; ++i)
		{
			cur.position[i] = a->position[i] + (b->position[i] - a->position[i]) * t;
			cur.orientation[i] = a->orientation[i] + (b->orientation[i] - a->orientation[i]) * t;
		}
		cur.distance = a->distance + (b->distance - a->distance) * t;
		cur.angle = a->angle + (b->angle - a->angle) * t;

		XMFLOAT3 eyeDX{}, centerDX{}, upDX{};
		VmdCameraToEyeCenterUp(cur, state.scale, eyeDX, centerDX, upDX);

		outEye = eyeDX;
		outView = XMMatrixLookAtLH(XMLoadFloat3(&eyeDX), XMLoadFloat3(&centerDX), XMLoadFloat3(&upDX));

		// FOV: 0이면 기존 FOV 사용, 비정상 값이면 기본값 사용
		float fovRad = (cur.angle > 0.0f)
			? XMConvertToRadians(cur.angle)
			: defaultFovRad;
		if (!std::isfinite(fovRad) || std::fabs(fovRad) < 0.0001f)
		{
			fovRad = (defaultFovRad > 0.0f) ? defaultFovRad : XMConvertToRadians(45.0f);
		}

		outProj = XMMatrixPerspectiveFovLH(fovRad, aspect, nearZ, farZ);
		return true;
	}
}


