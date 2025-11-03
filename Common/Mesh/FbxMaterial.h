#pragma once

#include <string>
#include <vector>
#include <unordered_map>

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

	const std::vector<ID3D11ShaderResourceView*>& GetMaterialSRVs() const;

private:
	struct Impl; Impl* m_;
};


