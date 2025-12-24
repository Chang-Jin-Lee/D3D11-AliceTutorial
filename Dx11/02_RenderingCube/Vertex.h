#pragma once

#include <d3d11_1.h>
#include <DirectXMath.h>

struct VertexPos
{
	VertexPos() = default;
	VertexPos(const VertexPos&) = default;
	VertexPos& operator=(const VertexPos&) = default;

	VertexPos(VertexPos&&) = default;
	VertexPos& operator=(VertexPos&&) = default;

	constexpr VertexPos(const DirectX::XMFLOAT3& _pos) : pos(_pos) {}

	DirectX::XMFLOAT3 pos;
	static const D3D11_INPUT_ELEMENT_DESC inputLayout[1];
};