#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include "ModelTransparency.h"

struct ID3D11Device;
struct ID3D11ShaderResourceView;
struct aiScene;

// Loads material textures (embedded, indexed, or file-based) and maintains a cache
class FbxMaterialLoader
{
public:
	FbxMaterialLoader();
	~FbxMaterialLoader();

	bool Load(ID3D11Device* device, const aiScene* scene, const std::wstring& baseDir);
	void Clear();

	// Legacy diffuse/baseColor map list (index == aiMaterial index)
	const std::vector<ID3D11ShaderResourceView*>& GetMaterialSRVs() const;
	// PBR 확장을 위한 metallic / roughness 텍스처 슬롯 (index == aiMaterial index)
	const std::vector<ID3D11ShaderResourceView*>& GetMetallicSRVs() const;
	const std::vector<ID3D11ShaderResourceView*>& GetRoughnessSRVs() const;
	const std::vector<ID3D11ShaderResourceView*>& GetNormalSRVs() const;
	const std::vector<ModelMaterialProcessing::MaterialAlphaInfo>& GetMaterialAlphaInfos() const;
private:
	struct Impl; Impl* m_;
};


