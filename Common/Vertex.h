#pragma once

struct VertexTriangle
{
	// 정점의 위치, 색상 정보
	Vector3 position;
	Vector3 color;

	VertexTriangle(float x, float y, float z, float r, float g, float b) : position(x, y, z), color(r, g, b) {}
	VertexTriangle(Vector3 pos) : position(pos) {}
	VertexTriangle(Vector3 pos, Vector4 col) : position(pos), color(col.x, col.y, col.z) {}
};



struct VertexTBN
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 n;
	DirectX::XMFLOAT3 t;
	DirectX::XMFLOAT3 b;
	DirectX::XMFLOAT4 c;
	DirectX::XMFLOAT2 uv;
};