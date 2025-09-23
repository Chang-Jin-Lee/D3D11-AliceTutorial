#include "pch.h"
#include "StaticMesh.h"
#include <vector>
#include "Helper.h"
#include <d3d11.h>

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
	// Front (N = (0,0,-1))
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2,-d2), XMFLOAT3(0, 0,-1), color, XMFLOAT2(0.0f, 1.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2,-d2), XMFLOAT3(0, 0,-1), color, XMFLOAT2(0.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2,-d2),  XMFLOAT3(0, 0,-1), color, XMFLOAT2(1.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2,-d2),  XMFLOAT3(0, 0,-1), color, XMFLOAT2(1.0f, 1.0f) });

	// Left (x = -w2), N = (-1,0,0)
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2, d2), XMFLOAT3(-1, 0, 0), color, XMFLOAT2(0.0f, 1.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2, d2), XMFLOAT3(-1, 0, 0), color, XMFLOAT2(0.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2,-d2), XMFLOAT3(-1, 0, 0), color, XMFLOAT2(1.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2,-d2), XMFLOAT3(-1, 0, 0), color, XMFLOAT2(1.0f, 1.0f) });

	// Top (y = +h2), N = (0,1,0)
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2,-d2), XMFLOAT3(0, 1, 0), color, XMFLOAT2(0.0f, 1.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2, d2), XMFLOAT3(0, 1, 0), color, XMFLOAT2(0.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2, d2),  XMFLOAT3(0, 1, 0), color, XMFLOAT2(1.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2,-d2),  XMFLOAT3(0, 1, 0), color, XMFLOAT2(1.0f, 1.0f) });

	// Back (z = +d2), N = (0,0,1)
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2, d2),  XMFLOAT3(0, 0, 1), color, XMFLOAT2(0.0f, 1.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2, d2),  XMFLOAT3(0, 0, 1), color, XMFLOAT2(0.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2, h2, d2), XMFLOAT3(0, 0, 1), color, XMFLOAT2(1.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2, d2), XMFLOAT3(0, 0, 1), color, XMFLOAT2(1.0f, 1.0f) });

	// Right (x = +w2), N = (1,0,0)
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2,-d2), XMFLOAT3(1, 0, 0), color, XMFLOAT2(0.0f, 1.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2,-d2),  XMFLOAT3(1, 0, 0), color, XMFLOAT2(0.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2, h2, d2),  XMFLOAT3(1, 0, 0), color, XMFLOAT2(1.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2, d2),  XMFLOAT3(1, 0, 0), color, XMFLOAT2(1.0f, 1.0f) });

	// Bottom (y = -h2), N = (0,-1,0)
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2, d2), XMFLOAT3(0,-1, 0), color, XMFLOAT2(0.0f, 1.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(-w2,-h2,-d2), XMFLOAT3(0,-1, 0), color, XMFLOAT2(0.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2,-d2),  XMFLOAT3(0,-1, 0), color, XMFLOAT2(1.0f, 0.0f) });
	staticMeshData.vertices.push_back({ XMFLOAT3(w2,-h2, d2),  XMFLOAT3(0,-1, 0), color, XMFLOAT2(1.0f, 1.0f) });

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

void StaticMesh::AssignMemory(ID3D11Device*& m_pDevice, ID3D11Buffer*& m_pVertexBuffer, StaticMeshData& meshData)
{
	D3D11_BUFFER_DESC vbDesc = {};
	ZeroMemory(&vbDesc, sizeof(vbDesc));											// vbDesc�� 0���� ��ü �޸� ������ �ʱ�ȭ ��ŵ�ϴ�
	vbDesc.ByteWidth = sizeof(VertexData) * meshData.vertices.size();				// �迭 ��ü�� ����Ʈ ũ�⸦ �ٷ� ��ȯ�մϴ�
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA vbData = {};
	ZeroMemory(&vbData, sizeof(vbData));
	vbData.pSysMem = meshData.vertices.data();										// �迭 ������ �Ҵ�.
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));
}

void StaticMesh::AssignIndexMemory(ID3D11Device*& m_pDevice, ID3D11Buffer*& m_pIndexBuffer, StaticMeshData& meshData, int& m_nIndices)
{
	D3D11_BUFFER_DESC ibDesc = {};
	ZeroMemory(&ibDesc, sizeof(ibDesc));
	m_nIndices = meshData.indices.size();	// �ε��� ���� ����.
	ibDesc.ByteWidth = sizeof(DWORD) * meshData.indices.size();
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = meshData.indices.data();
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));
}
