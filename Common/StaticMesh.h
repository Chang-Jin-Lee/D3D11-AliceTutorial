#pragma once

#include <d3d11.h>
#include "Vertex.h"
#include <directxtk/SimpleMath.h>
    #include <vector>

// TBN 정점
struct StaticMeshData
{
	std::vector<VertexTBN> vertices; // 24
	std::vector<DWORD>     indices;  // 36
};

// LightTex 정점
struct StaticMeshDataLightTex
{
	std::vector<VertexLightTex> vertices; // 24
	std::vector<DWORD>          indices;  // 36
};

class StaticMesh
{
public:
	// 정적으로 TBN이 적용된 박스를 만듭니다 
	static StaticMeshData CreateBox(const DirectX::XMFLOAT4& color, float width = 2, float height = 2, float depth = 2);
	static void AssignMemory(ID3D11Device*& device, ID3D11Buffer*& outVB, StaticMeshData& meshData);
	static void AssignIndexMemory(ID3D11Device*& device, ID3D11Buffer*& outIB, StaticMeshData& meshData, int& outIndexCount);

	// 정적으로 TBN이 적용되지 않은 박스를 만듭니다. LightTex
    static StaticMeshDataLightTex CreateBoxLightTex(const DirectX::XMFLOAT4& color, float width = 2, float height = 2, float depth = 2);
	static void AssignMemoryLightTex(ID3D11Device*& device, ID3D11Buffer*& outVB, StaticMeshDataLightTex& meshData);
	static void AssignIndexMemoryLightTex(ID3D11Device*& device, ID3D11Buffer*& outIB, StaticMeshDataLightTex& meshData, int& outIndexCount);

	// 디버그 박스
	static StaticMeshData CreateDebugBox(const DirectX::XMFLOAT4& color, float size = 0.2f)
	{
		return CreateBox(color, size, size, size);
	}
	static void CreateDebugBoxBuffers(ID3D11Device* device, const DirectX::XMFLOAT4& color, float size,
		ID3D11Buffer** outVB, ID3D11Buffer** outIB, int* outIndexCount)
	{
		StaticMeshData data = CreateDebugBox(color, size);
		AssignMemory(const_cast<ID3D11Device*&>(device), *outVB, data);
		AssignIndexMemory(const_cast<ID3D11Device*&>(device), *outIB, data, *outIndexCount);
	}

	// 디버그 박스(LightTex)
	static void CreateDebugBoxBuffersLightTex(ID3D11Device* device, const DirectX::XMFLOAT4& color, float size,
		ID3D11Buffer** outVB, ID3D11Buffer** outIB, int* outIndexCount)
	{
		StaticMeshDataLightTex data = CreateBoxLightTex(color, size, size, size);
		AssignMemoryLightTex(const_cast<ID3D11Device*&>(device), *outVB, data);
		AssignIndexMemoryLightTex(const_cast<ID3D11Device*&>(device), *outIB, data, *outIndexCount);
	}
};

