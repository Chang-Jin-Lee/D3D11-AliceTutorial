#pragma once

struct VertexData
{
	DirectX::XMFLOAT3 vertices;
	DirectX::XMFLOAT3 normals;
	DirectX::XMFLOAT4 colors;
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
};

