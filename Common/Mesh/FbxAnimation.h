#pragma once

#include <vector>
#include <string>
#include <unordered_map>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct aiScene;
struct aiNodeAnim;

namespace DirectX { struct XMFLOAT4X4; struct XMMATRIX; }

// Controls animation state, builds bone palettes and uploads to GPU
class FbxAnimation
{
public:
	enum class AnimType { None = 0, Skinned = 1, Rigid = 2 };

	FbxAnimation();
	~FbxAnimation();

	void Clear();

	void InitMetadata(const aiScene* scene);
	void SetType(AnimType t) { m_Type = t; }
	AnimType GetType() const { return m_Type; }

	// Time/clip controls
	const std::vector<std::string>& GetNames() const { return m_Names; }
	int GetCurrentIndex() const { return m_Current; }
	void SetCurrentIndex(int idx);
	void SetPlaying(bool p) { m_Playing = p; }
	bool IsPlaying() const { return m_Playing; }
	double GetTimeSec() const { return m_TimeSec; }
	void SetTimeSec(double t);
	double GetClipDurationSec(int idx) const;

	ID3D11Buffer* GetBoneCB() const { return m_pBoneCB; }
	void EnsureBoneCB(ID3D11Device* device, int maxBones);

	// Evaluate and upload palette
	void UpdateAndUpload(
		ID3D11DeviceContext* ctx,
		double dtSec,
		const aiScene* scene,
		const std::unordered_map<std::string,int>& nodeIndexOfName,
		const std::vector<std::string>& boneNames,
		const std::vector<DirectX::XMFLOAT4X4>& boneOffsets,
		const DirectX::XMFLOAT4X4& globalInverse);

	// Rigid-only palette upload (no offsets)
	void UploadRigid(ID3D11DeviceContext* ctx,
		const aiScene* scene,
		const std::unordered_map<std::string,int>& nodeIndexOfName,
		const std::vector<std::string>& boneNames,
		const DirectX::XMFLOAT4X4& globalInverse);

private:
	void UploadPalette(ID3D11DeviceContext* ctx, const std::vector<DirectX::XMMATRIX>& pal);
	void EvaluateGlobals(const aiScene* scene,
		const std::unordered_map<std::wstring, const aiNodeAnim*>& channelOf,
		const std::unordered_map<std::string,int>& nodeIndexOfName,
		std::vector<DirectX::XMFLOAT4X4>& outGlobal) const;

private:
	AnimType m_Type = AnimType::None;
	std::vector<std::string> m_Names;
	std::vector<double> m_DurationSec;
	std::vector<double> m_TicksPerSec;
	int m_Current = -1;
	double m_TimeSec = 0.0;
	bool m_Playing = false;
	ID3D11Buffer* m_pBoneCB = nullptr;
};


