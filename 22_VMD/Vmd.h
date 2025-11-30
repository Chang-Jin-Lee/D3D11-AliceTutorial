#pragma once

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
		/// 본 이름 (Shift-JIS, 15바이트, 널 종료)
		std::string name;
		/// 프레임 번호
		int frame;
		/// 위치 (x, y, z)
		float position[3];
		/// 회전 (쿼터니언 x, y, z, w)
		float orientation[4];
		/// 보간 커브 데이터 [4][4][4]
		char interpolation[4][4][4];

		void Read(std::istream* stream)
		{
			char buffer[15];
			stream->read((char*) buffer, sizeof(char)*15);
			name = std::string(buffer);
			stream->read((char*) &frame, sizeof(int));
			stream->read((char*) position, sizeof(float)*3);
			stream->read((char*) orientation, sizeof(float)*4);
			stream->read((char*) interpolation, sizeof(char) * 4 * 4 * 4);
		}

		void Write(std::ostream* stream)
		{
			stream->write((char*)name.c_str(), sizeof(char) * 15);
			stream->write((char*)&frame, sizeof(int));
			stream->write((char*)position, sizeof(float) * 3);
			stream->write((char*)orientation, sizeof(float) * 4);
			stream->write((char*)interpolation, sizeof(char) * 4 * 4 * 4);
		}
	};

	/// 표정(모프) 키프레임
	class VmdFaceFrame
	{
	public:
		/// 표정 이름 (Shift-JIS, 15바이트)
		std::string face_name;
		/// 가중치(0.0 ~ 1.0)
		float weight;
		/// 프레임 번호
		uint32_t frame;

		void Read(std::istream* stream)
		{
			char buffer[15];
			stream->read((char*) &buffer, sizeof(char) * 15);
			face_name = std::string(buffer);
			stream->read((char*) &frame, sizeof(int));
			stream->read((char*) &weight, sizeof(float));
		}

		void Write(std::ostream* stream)
		{
			stream->write((char*)face_name.c_str(), sizeof(char) * 15);
			stream->write((char*)&frame, sizeof(int));
			stream->write((char*)&weight, sizeof(float));
		}
	};

	/// 카메라 키프레임
	class VmdCameraFrame
	{
	public:
		/// 프레임 번호 (0부터 시작)
		int frame;                       // 4 byte
		/// 카메라와 타깃(관심점) 사이 거리
		float distance;                  // 4 byte
		/// 타깃(관심점) 위치 (x, y, z)
		float position[3];               // 12 byte
		/// 카메라 회전 (오일러, 라디안: pitch(x), yaw(y), roll(z))
		float orientation[3];            // 12 byte
		/// 보간 커브 데이터 (6채널 * 4바이트)
		char interpolation[6][4];        // 24 byte
		/// 시야각(FOV, 도 단위)
		float angle;                     // 4 byte
		/// 원근 사용 여부 (0: 원근 사용, 1: 원근 끔) - VMD 스펙상 1바이트
		std::uint8_t perspective;        // 1 byte

		void Read(std::istream *stream)
		{
			stream->read((char*) &frame, sizeof(int));
			stream->read((char*) &distance, sizeof(float));
			stream->read((char*) position, sizeof(float) * 3);
			stream->read((char*) orientation, sizeof(float) * 3);
			stream->read((char*) interpolation, sizeof(char) * 24);
			stream->read((char*) &angle, sizeof(float));
			// VMD 카메라 프레임 스펙은 여기서 1바이트만 갖습니다.
			// 과거 코드에서는 3바이트를 읽어 1프레임당 2바이트씩 오프셋이 밀리는 문제가 있었음.
			stream->read((char*) &perspective, sizeof(std::uint8_t));
		}

		void Write(std::ostream *stream)
		{
			stream->write((char*)&frame, sizeof(int));
			stream->write((char*)&distance, sizeof(float));
			stream->write((char*)position, sizeof(float) * 3);
			stream->write((char*)orientation, sizeof(float) * 3);
			stream->write((char*)interpolation, sizeof(char) * 24);
			stream->write((char*)&angle, sizeof(float));
			// 스펙에 맞게 1바이트만 기록
			stream->write((char*)&perspective, sizeof(std::uint8_t));
		}
	};

	/// 조명(라이트) 키프레임
	class VmdLightFrame
	{
	public:
		/// 프레임 번호
		int frame;
		/// 색상(RGB)
		float color[3];
		/// 위치 또는 방향 (MMD에서는 방향 벡터로 사용)
		float position[3];

		void Read(std::istream* stream)
		{
			stream->read((char*) &frame, sizeof(int));
			stream->read((char*) color, sizeof(float) * 3);
			stream->read((char*) position, sizeof(float) * 3);
		}

		void Write(std::ostream* stream)
		{
			stream->write((char*)&frame, sizeof(int));
			stream->write((char*)color, sizeof(float) * 3);
			stream->write((char*)position, sizeof(float) * 3);
		}
	};

	/// IK 온/오프 정보
	class VmdIkEnable
	{
	public:
		/// IK 이름 (Shift-JIS, 20바이트)
		std::string ik_name;
		/// 활성 여부
		bool enable;
	};

	/// IK 키프레임
	class VmdIkFrame
	{
	public:
		/// 프레임 번호
		int frame;
		/// IK 표시 여부
		bool display;
		/// 각 IK의 온/오프 목록
		std::vector<VmdIkEnable> ik_enable;

		void Read(std::istream *stream)
		{
			char buffer[20];
			stream->read((char*) &frame, sizeof(int));
			stream->read((char*) &display, sizeof(uint8_t));
			int ik_count;
			stream->read((char*) &ik_count, sizeof(int));
			ik_enable.resize(ik_count);
			for (int i = 0; i < ik_count; i++)
			{
				stream->read(buffer, 20);
				ik_enable[i].ik_name = std::string(buffer);
				stream->read((char*) &ik_enable[i].enable, sizeof(uint8_t));
			}
		}

		void Write(std::ostream *stream)
		{
			stream->write((char*)&frame, sizeof(int));
			stream->write((char*)&display, sizeof(uint8_t));
			int ik_count = static_cast<int>(ik_enable.size());
			stream->write((char*)&ik_count, sizeof(int));
			for (int i = 0; i < ik_count; i++)
			{
				const VmdIkEnable& ik_enable = this->ik_enable.at(i);
				stream->write(ik_enable.ik_name.c_str(), 20);
				stream->write((char*)&ik_enable.enable, sizeof(uint8_t));
			}
		}
	};

	/// VMD 모션 전체 데이터
	class VmdMotion
	{
	public:
		/// 모델 이름
		std::string model_name;
		/// VMD 버전
		int version;
		/// 본(뼈) 키프레임 목록
		std::vector<VmdBoneFrame> bone_frames;
		/// 표정(모프) 키프레임 목록
		std::vector<VmdFaceFrame> face_frames;
		/// 카메라 키프레임 목록
		std::vector<VmdCameraFrame> camera_frames;
		/// 조명(라이트) 키프레임 목록
		std::vector<VmdLightFrame> light_frames;
		/// IK 키프레임 목록
		std::vector<VmdIkFrame> ik_frames;

		static std::unique_ptr<VmdMotion> LoadFromFile(char const *filename)
		{
			std::ifstream stream(filename, std::ios::binary);
			auto result = LoadFromStream(&stream);
			stream.close();
			return result;
		}

		static std::unique_ptr<VmdMotion> LoadFromStream(std::ifstream *stream)
		{

			char buffer[30];
			auto result = std::make_unique<VmdMotion>();

			// magic and version
			stream->read((char*) buffer, 30);
			if (strncmp(buffer, "Vocaloid Motion Data", 20))
			{
				std::cerr << "invalid vmd file." << std::endl;
				return nullptr;
			}
			result->version = std::atoi(buffer + 20);

			// name
			stream->read(buffer, 20);
			result->model_name = std::string(buffer);

			// bone frames
			int bone_frame_num;
			stream->read((char*) &bone_frame_num, sizeof(int));
			result->bone_frames.resize(bone_frame_num);
			for (int i = 0; i < bone_frame_num; i++)
			{
				result->bone_frames[i].Read(stream);
			}

			// face frames
			int face_frame_num;
			stream->read((char*) &face_frame_num, sizeof(int));
			result->face_frames.resize(face_frame_num);
			for (int i = 0; i < face_frame_num; i++)
			{
				result->face_frames[i].Read(stream);
			}

			// camera frames
			int camera_frame_num;
			stream->read((char*) &camera_frame_num, sizeof(int));
			result->camera_frames.resize(camera_frame_num);
			for (int i = 0; i < camera_frame_num; i++)
			{
				result->camera_frames[i].Read(stream);
			}

			// light frames
			int light_frame_num;
			stream->read((char*) &light_frame_num, sizeof(int));
			result->light_frames.resize(light_frame_num);
			for (int i = 0; i < light_frame_num; i++)
			{
				result->light_frames[i].Read(stream);
			}

			// unknown2
			stream->read(buffer, 4);

			// ik frames
			if (stream->peek() != std::ios::traits_type::eof())
			{
				int ik_num;
				stream->read((char*) &ik_num, sizeof(int));
				result->ik_frames.resize(ik_num);
				for (int i = 0; i < ik_num; i++)
				{
					result->ik_frames[i].Read(stream);
				}
			}

			if (stream->peek() != std::ios::traits_type::eof())
			{
				std::cerr << "vmd stream has unknown data." << std::endl;
			}

			return result;
		}

        bool SaveToFile(const std::u16string& filename)
        {
            // use C++17 filesystem::path to support UTF-16 filenames on Windows
            std::filesystem::path p(filename);
            std::ofstream stream(p, std::ios::binary);
            auto result = SaveToStream(&stream);
            stream.close();
            return result;
        }

		bool SaveToStream(std::ofstream *stream)
		{
			std::string magic = "Vocaloid Motion Data 0002\0";
			magic.resize(30);

			// magic and version
			stream->write(magic.c_str(), 30);

			// name
			stream->write(model_name.c_str(), 20);

			// bone frames
			const int bone_frame_num = static_cast<int>(bone_frames.size());
			stream->write(reinterpret_cast<const char*>(&bone_frame_num), sizeof(int));
			for (int i = 0; i < bone_frame_num; i++)
			{
				bone_frames[i].Write(stream);
			}

			// face frames
			const int face_frame_num = static_cast<int>(face_frames.size());
			stream->write(reinterpret_cast<const char*>(&face_frame_num), sizeof(int));
			for (int i = 0; i < face_frame_num; i++)
			{
				face_frames[i].Write(stream);
			}

			// camera frames
			const int camera_frame_num = static_cast<int>(camera_frames.size());
			stream->write(reinterpret_cast<const char*>(&camera_frame_num), sizeof(int));
			for (int i = 0; i < camera_frame_num; i++)
			{
				camera_frames[i].Write(stream);
			}

			// light frames
			const int light_frame_num = static_cast<int>(light_frames.size());
			stream->write(reinterpret_cast<const char*>(&light_frame_num), sizeof(int));
			for (int i = 0; i < light_frame_num; i++)
			{
				light_frames[i].Write(stream);
			}

			// self shadow datas
			const int self_shadow_num = 0;
			stream->write(reinterpret_cast<const char*>(&self_shadow_num), sizeof(int));

			// ik frames
			const int ik_num = static_cast<int>(ik_frames.size());
			stream->write(reinterpret_cast<const char*>(&ik_num), sizeof(int));
			for (int i = 0; i < ik_num; i++)
			{
				ik_frames[i].Write(stream);
			}

			return true;
		}
	};
}