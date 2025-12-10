#pragma once

// @brief   : VMD(Motion Data) 포맷 파서 (카메라/본/표정 등)
// @details : 22_VMD 예제에서 사용하던 VMD 파서를 Common으로 옮긴 버전입니다.

#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <ostream>
#include <cstdint>

namespace vmd
{
	/// 본(뼈) 키프레임
	class VmdBoneFrame
	{
	public:
		std::string name;           // 본 이름 (Shift-JIS, 15바이트)
		int frame;                  // 프레임 번호
		float position[3];          // 위치 (x, y, z)
		float orientation[4];       // 회전 (쿼터니언 x, y, z, w)
		char interpolation[4][4][4];

		void Read(std::istream* stream);
		void Write(std::ostream* stream);
	};

	/// 표정(모프) 키프레임
	class VmdFaceFrame
	{
	public:
		std::string face_name;      // 표정 이름 (Shift-JIS, 15바이트)
		float      weight;          // 가중치(0.0 ~ 1.0)
		uint32_t   frame;           // 프레임 번호

		void Read(std::istream* stream);
		void Write(std::ostream* stream);
	};

	/// 카메라 키프레임
	class VmdCameraFrame
	{
	public:
		int         frame;          // 프레임 번호 (0부터 시작)
		float       distance;       // 카메라-타깃 거리
		float       position[3];    // 타깃 위치 (x, y, z)
		float       orientation[3]; // 카메라 회전 (pitch, yaw, roll, 라디안)
		char        interpolation[6][4];
		float       angle;          // FOV(도)
		std::uint8_t perspective;   // 0: 원근, 1: 원근 끔

		void Read(std::istream* stream);
		void Write(std::ostream* stream);
	};

	/// 조명(라이트) 키프레임
	class VmdLightFrame
	{
	public:
		int   frame;
		float color[3];
		float position[3];

		void Read(std::istream* stream);
		void Write(std::ostream* stream);
	};

	/// IK 온/오프 정보
	class VmdIkEnable
	{
	public:
		std::string ik_name;
		bool        enable;
	};

	/// IK 키프레임
	class VmdIkFrame
	{
	public:
		int                     frame;
		bool                    display;
		std::vector<VmdIkEnable> ik_enable;

		void Read(std::istream* stream);
		void Write(std::ostream* stream);
	};

	/// VMD 모션 전체 데이터
	class VmdMotion
	{
	public:
		std::string model_name;
		int         version;

		std::vector<VmdBoneFrame>   bone_frames;
		std::vector<VmdFaceFrame>   face_frames;
		std::vector<VmdCameraFrame> camera_frames;
		std::vector<VmdLightFrame>  light_frames;
		std::vector<VmdIkFrame>     ik_frames;

		static std::unique_ptr<VmdMotion> LoadFromFile(char const* filename);
		static std::unique_ptr<VmdMotion> LoadFromStream(std::ifstream* stream);

		bool SaveToFile(const std::u16string& filename);
		bool SaveToStream(std::ofstream* stream);
	};
}


