#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

#include "FbxTypes.h"
#include "ModelTransparency.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;

// High-level FBX model loader composed of sub-systems (materials, geometry, skeleton, animation)
// API intentionally mirrors existing FbxManager to ease migration
class FbxModel
{
public:
	enum class AnimationType { None = 0, Skinned = 1, Rigid = 2 };

	FbxModel();
	~FbxModel();

	bool Load(ID3D11Device* device, const std::wstring& pathW);
	void Release();

	// Mesh
	bool HasMesh() const;
	ID3D11Buffer* GetVertexBuffer() const;
	ID3D11Buffer* GetIndexBuffer() const;
	int GetIndexCount() const;
	UINT GetVertexStride() const;
	UINT GetVertexOffset() const { return 0; }
	const std::vector<FbxSubset>& GetSubsets() const;
	const std::vector<ID3D11ShaderResourceView*>& GetMaterialSRVs() const;      // BaseColor / Diffuse
	const std::vector<ID3D11ShaderResourceView*>& GetMetallicSRVs() const;      // PBR Metallic
	const std::vector<ID3D11ShaderResourceView*>& GetRoughnessSRVs() const;     // PBR Roughness
	const std::vector<ID3D11ShaderResourceView*>& GetNormalSRVs() const;        // Normal maps
	const std::vector<ModelMaterialProcessing::MaterialAlphaInfo>& GetMaterialAlphaInfos() const;

	// Skeleton
	bool HasSkeleton() const;
	bool HasAnimations() const;
	const std::vector<FbxSkeletonNode>& GetSkeleton() const;
	int GetSkeletonRoot() const;
	ID3D11Buffer* GetBoneConstantBuffer() const;
	UINT GetBoneCount() const;

	// Animation controls
	const std::vector<std::string>& GetAnimationNames() const;
	int GetCurrentAnimationIndex() const;
	void SetCurrentAnimation(int idx);
	void SetAnimationPlaying(bool playing);
	bool IsAnimationPlaying() const;
	double GetAnimationTimeSeconds() const;
	void SetAnimationTimeSeconds(double t);
	void UpdateAnimation(ID3D11DeviceContext* ctx, double dtSec);
	double GetClipDurationSec(int idx) const;

	AnimationType GetCurrentAnimationType() const;

	// Shared data accessors for per-instance animators
	const struct aiScene* GetScenePtr() const;
	const std::unordered_map<std::string,int>& GetNodeIndexOfName() const;
	const std::vector<std::string>& GetBoneNames() const;
	const std::vector<DirectX::XMFLOAT4X4>& GetBoneOffsets() const;
	const DirectX::XMFLOAT4X4& GetGlobalInverse() const;

private:
	struct Impl; std::unique_ptr<Impl> m_;
};


