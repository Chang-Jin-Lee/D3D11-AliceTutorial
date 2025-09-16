#include "pch.h"
#include "StaticMesh.h"
#include <vector>

// �ڽ��� ����� �ڵ�
StaticMeshData StaticMesh::CreateBox(const XMFLOAT4& color, float width, float height, float depth)
{
	using namespace DirectX;

	StaticMeshData staticMeshData;

	staticMeshData.vertices.reserve(24);
	staticMeshData.indices.reserve(36);

	const float w2 = width * 0.5f;
	const float h2 = height * 0.5f;
	const float d2 = depth * 0.5f;

	/*
	* @brief  ����(Vertex) �迭�� GPU ���۷� ����
	* @details
	*   - ByteWidth : ���� ��ü ũ��(���� ũ�� �� ����)
	*   - BindFlags : D3D11_BIND_VERTEX_BUFFER
	*   - Usage     : DEFAULT (�Ϲ��� �뵵)
	*   - �ʱ� ������ : vbData.pSysMem = vertices
	*   - Stride/Offset : IASetVertexBuffers�� �Ķ����
	*   - ���� : VertexInfo(color=Vec3), ���̴�/InputLayout�� COLOR ���� ��ġ �ʿ�
	*/

	// ���� 24�� Front, Left, Top, Back, Right, Bottom (6�� * 4����)
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2,-d2), XMFLOAT3(0, 0,-1), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2,-d2), XMFLOAT3(0, 0,-1), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2,-d2), XMFLOAT3(0, 0,-1), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2,-d2), XMFLOAT3(0, 0,-1), color });

	// Left (x = -w2), N = (-1,0,0)
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2, d2), XMFLOAT3(-1, 0, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2, d2), XMFLOAT3(-1, 0, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2,-d2), XMFLOAT3(-1, 0, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2,-d2), XMFLOAT3(-1, 0, 0), color });

	// Top (y = +h2), N = (0,1,0)
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2,-d2), XMFLOAT3(0, 1, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2, d2), XMFLOAT3(0, 1, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2, d2), XMFLOAT3(0, 1, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2,-d2), XMFLOAT3(0, 1, 0), color });

	// Back (z = +d2), N = (0,0,1)
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2, d2), XMFLOAT3(0, 0, 1), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2, d2), XMFLOAT3(0, 0, 1), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2, d2), XMFLOAT3(0, 0, 1), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2, d2), XMFLOAT3(0, 0, 1), color });

	// Right (x = +w2), N = (1,0,0)
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2,-d2), XMFLOAT3(1, 0, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2,-d2), XMFLOAT3(1, 0, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2, d2), XMFLOAT3(1, 0, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2, d2), XMFLOAT3(1, 0, 0), color });

	// Bottom (y = -h2), N = (0,-1,0)
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2, d2), XMFLOAT3(0,-1, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2,-d2), XMFLOAT3(0,-1, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2,-d2), XMFLOAT3(0,-1, 0), color });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2, d2), XMFLOAT3(0,-1, 0), color });

	/*
	* @brief  �ε��� ����(Index Buffer) ����
	* @details
	*   - indices: ���� ����� (�簢�� = �ﰢ�� 2�� = �ε��� 6��)
	*   - WORD Ÿ�� �� DXGI_FORMAT_R16_UINT ��� ����
	*   - ByteWidth : ��ü �ε��� �迭 ũ��
	*   - BindFlags : D3D11_BIND_INDEX_BUFFER
	*   - Usage     : DEFAULT (GPU �Ϲ� ����)
	*   - �� �ڵ��� ���: m_pIndexBuffer ����, m_nIndices�� ���� ����
	*/

	staticMeshData.indices = {
		0, 1, 2, 2, 3, 0,		// ������ (+X ��)
		4, 5, 6, 6, 7, 4,		// ���� (X ��)
		8, 9, 10, 10, 11, 8,	// ��� ǥ�� (+y ǥ��)
		12, 13, 14, 14, 15, 12,	// �ٴ� (Y ����)
		16, 17, 18, 18, 19, 16, // �ڷ� (+z ��)
		20, 21, 22, 22, 23, 20	// ���� (Z ��)
	};

	return staticMeshData;
}
