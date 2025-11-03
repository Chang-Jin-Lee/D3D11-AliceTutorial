#pragma once

#include <vector>
#include <string>
#include "../Vertex.h"
#include "FbxTypes.h"

struct ID3D11Device;
struct ID3D11Buffer;
struct aiScene;

// Builds GPU buffers (VB/IB) and subset ranges from Assimp scene
class FbxGeometryBuilder
{
public:
	FbxGeometryBuilder();
	~FbxGeometryBuilder();

	bool Build(ID3D11Device* device, const aiScene* scene);
	void Clear();

	ID3D11Buffer* GetVB() const;
	ID3D11Buffer* GetIB() const;
	int GetIndexCount() const;
	UINT GetVertexStride() const;
	const std::vector<FbxSubset>& GetSubsets() const;

	// For rigid animation helper
	const std::vector<std::string>& GetVertexOwningNodeNames() const;
	std::vector<VertexSkinnedTBN>& GetCPUVertices();
	const std::vector<VertexSkinnedTBN>& GetCPUVertices() const;

	// Recreate VB from CPU-side vertices (after skin weights applied)
	bool RebuildVBFromCPU(ID3D11Device* device);

private:
	struct Impl; Impl* m_;
};


