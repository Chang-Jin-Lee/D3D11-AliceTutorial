#pragma once

#include <DirectXMath.h>

struct VertexTriangle
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
};

struct VertexCubePosColor
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
};

struct VertexCubePosTex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT2 tex;
};

struct VertexLight
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT4 color;
};

struct VertexLightTex
{
	DirectX::XMFLOAT3 vertices;
	DirectX::XMFLOAT3 normals;
	DirectX::XMFLOAT4 colors;
	DirectX::XMFLOAT2 texcoord;
};

struct VertexTBN
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 n;
	DirectX::XMFLOAT3 t;
	DirectX::XMFLOAT3 b;
	DirectX::XMFLOAT4 color;
	DirectX::XMFLOAT2 uv;
};