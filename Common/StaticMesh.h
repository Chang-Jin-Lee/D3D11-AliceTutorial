#pragma once

struct VertexData
{
	DirectX::XMFLOAT3 vertices;
	DirectX::XMFLOAT3 normals;
	DirectX::XMFLOAT4 colors;
	DirectX::XMFLOAT2 texcoord;
	static const D3D11_INPUT_ELEMENT_DESC inputLayout[3];
};

struct StaticMeshData
{
	std::vector<VertexData> vertices;	// 24
	std::vector<DWORD>    indices;   // 36
};

class StaticMesh
{
public:
	// 정적으로 박스를 만드는 코드입니다
	static StaticMeshData CreateBox(const XMFLOAT4& color,float width = 2, float height = 2, float depth = 2);
	static void AssignMemory(ID3D11Device*& m_pDevice, ID3D11Buffer*& m_pVertexBuffer, StaticMeshData& meshData);
	static void AssignIndexMemory(ID3D11Device*& m_pDevice, ID3D11Buffer*& m_pIndexBuffer, StaticMeshData& meshData, int& m_nIndices);

    // 디버그용 작은 상자 생성/그리기
    static StaticMeshData CreateDebugBox(const XMFLOAT4& color, float size = 0.2f)
    {
        return CreateBox(color, size, size, size);
    }
    static void CreateDebugBoxBuffers(ID3D11Device* device, const XMFLOAT4& color, float size,
        ID3D11Buffer** outVB, ID3D11Buffer** outIB, int* outIndexCount)
    {
        StaticMeshData data = CreateDebugBox(color, size);
        AssignMemory(const_cast<ID3D11Device*&>(device), *outVB, data);
        AssignIndexMemory(const_cast<ID3D11Device*&>(device), *outIB, data, *outIndexCount);
    }
};

