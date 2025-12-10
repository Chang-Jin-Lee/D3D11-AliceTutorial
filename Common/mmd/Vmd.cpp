#include "pch.h"
#include "Vmd.h"

namespace vmd
{
	// ----- VmdBoneFrame -----

	void VmdBoneFrame::Read(std::istream* stream)
	{
		char buffer[15];
		stream->read(buffer, sizeof(char) * 15);
		name = std::string(buffer);
		stream->read((char*)&frame, sizeof(int));
		stream->read((char*)position, sizeof(float) * 3);
		stream->read((char*)orientation, sizeof(float) * 4);
		stream->read((char*)interpolation, sizeof(char) * 4 * 4 * 4);
	}

	void VmdBoneFrame::Write(std::ostream* stream)
	{
		stream->write(name.c_str(), sizeof(char) * 15);
		stream->write((char*)&frame, sizeof(int));
		stream->write((char*)position, sizeof(float) * 3);
		stream->write((char*)orientation, sizeof(float) * 4);
		stream->write((char*)interpolation, sizeof(char) * 4 * 4 * 4);
	}

	// ----- VmdFaceFrame -----

	void VmdFaceFrame::Read(std::istream* stream)
	{
		char buffer[15];
		stream->read(buffer, sizeof(char) * 15);
		face_name = std::string(buffer);
		stream->read((char*)&frame, sizeof(int));
		stream->read((char*)&weight, sizeof(float));
	}

	void VmdFaceFrame::Write(std::ostream* stream)
	{
		stream->write(face_name.c_str(), sizeof(char) * 15);
		stream->write((char*)&frame, sizeof(int));
		stream->write((char*)&weight, sizeof(float));
	}

	// ----- VmdCameraFrame -----

	void VmdCameraFrame::Read(std::istream* stream)
	{
		stream->read((char*)&frame, sizeof(int));
		stream->read((char*)&distance, sizeof(float));
		stream->read((char*)position, sizeof(float) * 3);
		stream->read((char*)orientation, sizeof(float) * 3);
		stream->read((char*)interpolation, sizeof(char) * 24);
		stream->read((char*)&angle, sizeof(float));
		stream->read((char*)&perspective, sizeof(std::uint8_t));
	}

	void VmdCameraFrame::Write(std::ostream* stream)
	{
		stream->write((char*)&frame, sizeof(int));
		stream->write((char*)&distance, sizeof(float));
		stream->write((char*)position, sizeof(float) * 3);
		stream->write((char*)orientation, sizeof(float) * 3);
		stream->write((char*)interpolation, sizeof(char) * 24);
		stream->write((char*)&angle, sizeof(float));
		stream->write((char*)&perspective, sizeof(std::uint8_t));
	}

	// ----- VmdLightFrame -----

	void VmdLightFrame::Read(std::istream* stream)
	{
		stream->read((char*)&frame, sizeof(int));
		stream->read((char*)color, sizeof(float) * 3);
		stream->read((char*)position, sizeof(float) * 3);
	}

	void VmdLightFrame::Write(std::ostream* stream)
	{
		stream->write((char*)&frame, sizeof(int));
		stream->write((char*)color, sizeof(float) * 3);
		stream->write((char*)position, sizeof(float) * 3);
	}

	// ----- VmdIkFrame -----

	void VmdIkFrame::Read(std::istream* stream)
	{
		char buffer[20];
		stream->read((char*)&frame, sizeof(int));
		stream->read((char*)&display, sizeof(uint8_t));
		int ik_count;
		stream->read((char*)&ik_count, sizeof(int));
		ik_enable.resize(ik_count);
		for (int i = 0; i < ik_count; i++)
		{
			stream->read(buffer, 20);
			ik_enable[i].ik_name = std::string(buffer);
			stream->read((char*)&ik_enable[i].enable, sizeof(uint8_t));
		}
	}

	void VmdIkFrame::Write(std::ostream* stream)
	{
		stream->write((char*)&frame, sizeof(int));
		stream->write((char*)&display, sizeof(uint8_t));
		int ik_count = static_cast<int>(ik_enable.size());
		stream->write((char*)&ik_count, sizeof(int));
		for (int i = 0; i < ik_count; i++)
		{
			const VmdIkEnable& ike = ik_enable.at(i);
			stream->write(ike.ik_name.c_str(), 20);
			stream->write((char*)&ike.enable, sizeof(uint8_t));
		}
	}

	// ----- VmdMotion -----

	std::unique_ptr<VmdMotion> VmdMotion::LoadFromFile(char const* filename)
	{
		std::ifstream stream(filename, std::ios::binary);
		auto result = LoadFromStream(&stream);
		stream.close();
		return result;
	}

	std::unique_ptr<VmdMotion> VmdMotion::LoadFromStream(std::ifstream* stream)
	{
		char buffer[30];
		auto result = std::make_unique<VmdMotion>();

		// magic and version
		stream->read(buffer, 30);
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
		stream->read((char*)&bone_frame_num, sizeof(int));
		result->bone_frames.resize(bone_frame_num);
		for (int i = 0; i < bone_frame_num; i++)
		{
			result->bone_frames[i].Read(stream);
		}

		// face frames
		int face_frame_num;
		stream->read((char*)&face_frame_num, sizeof(int));
		result->face_frames.resize(face_frame_num);
		for (int i = 0; i < face_frame_num; i++)
		{
			result->face_frames[i].Read(stream);
		}

		// camera frames
		int camera_frame_num;
		stream->read((char*)&camera_frame_num, sizeof(int));
		result->camera_frames.resize(camera_frame_num);
		for (int i = 0; i < camera_frame_num; i++)
		{
			result->camera_frames[i].Read(stream);
		}

		// light frames
		int light_frame_num;
		stream->read((char*)&light_frame_num, sizeof(int));
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
			stream->read((char*)&ik_num, sizeof(int));
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

	bool VmdMotion::SaveToFile(const std::u16string& filename)
	{
		std::filesystem::path p(filename);
		std::ofstream stream(p, std::ios::binary);
		auto result = SaveToStream(&stream);
		stream.close();
		return result;
	}

	bool VmdMotion::SaveToStream(std::ofstream* stream)
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
}


