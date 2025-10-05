#include "pch.h"
#include "StaticMesh.h"
#include <vector>
#include "Helper.h"
#include <d3d11.h>

// 박스를 만드는 코드 (TBN)
StaticMeshData StaticMesh::CreateBox(const XMFLOAT4& color, float width, float height, float depth)
{
	using namespace DirectX;

	StaticMeshData staticMeshData;

	staticMeshData.vertices.reserve(24);
	staticMeshData.indices.reserve(36);

	const float w2 = width * 0.5f;
	const float h2 = height * 0.5f;
	const float d2 = depth * 0.5f;

	// Front (-Z)
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2, -h2, -d2), XMFLOAT3(0, 0, -1), XMFLOAT3(1, 0, 0), XMFLOAT3(0, -1, 0), color, XMFLOAT2(0, 1) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2,  h2, -d2), XMFLOAT3(0, 0, -1), XMFLOAT3(1, 0, 0), XMFLOAT3(0, -1, 0), color, XMFLOAT2(0, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2,  h2, -d2), XMFLOAT3(0, 0, -1), XMFLOAT3(1, 0, 0), XMFLOAT3(0, -1, 0), color, XMFLOAT2(1, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2, -h2, -d2), XMFLOAT3(0, 0, -1), XMFLOAT3(1, 0, 0), XMFLOAT3(0, -1, 0), color, XMFLOAT2(1, 1) });

	// Left (-X)
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2, -h2,  d2), XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 0, -1), XMFLOAT3(0, 1, 0), color, XMFLOAT2(0, 1) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2,  h2,  d2), XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 0, -1), XMFLOAT3(0, 1, 0), color, XMFLOAT2(0, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2,  h2, -d2), XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 0, -1), XMFLOAT3(0, 1, 0), color, XMFLOAT2(1, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2, -h2, -d2), XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 0, -1), XMFLOAT3(0, 1, 0), color, XMFLOAT2(1, 1) });

	// Top (+Y)
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2,  h2, -d2), XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1), color, XMFLOAT2(0, 1) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2,  h2,  d2), XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1), color, XMFLOAT2(0, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2,  h2,  d2), XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1), color, XMFLOAT2(1, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2,  h2, -d2), XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1), color, XMFLOAT2(1, 1) });

	// Back (+Z)
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2, -h2,  d2), XMFLOAT3(0, 0, 1), XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 1, 0), color, XMFLOAT2(0, 1) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2,  h2,  d2), XMFLOAT3(0, 0, 1), XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 1, 0), color, XMFLOAT2(0, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2,  h2,  d2), XMFLOAT3(0, 0, 1), XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 1, 0), color, XMFLOAT2(1, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2, -h2,  d2), XMFLOAT3(0, 0, 1), XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 1, 0), color, XMFLOAT2(1, 1) });

	// Right (+X)
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2, -h2, -d2), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1), XMFLOAT3(0, 1, 0), color, XMFLOAT2(0, 1) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2,  h2, -d2), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1), XMFLOAT3(0, 1, 0), color, XMFLOAT2(0, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2,  h2,  d2), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1), XMFLOAT3(0, 1, 0), color, XMFLOAT2(1, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2, -h2,  d2), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1), XMFLOAT3(0, 1, 0), color, XMFLOAT2(1, 1) });

	// Bottom (-Y)
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2, -h2,  d2), XMFLOAT3(0, -1, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, -1), color, XMFLOAT2(0, 1) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(-w2, -h2, -d2), XMFLOAT3(0, -1, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, -1), color, XMFLOAT2(0, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2, -h2, -d2), XMFLOAT3(0, -1, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, -1), color, XMFLOAT2(1, 0) });
	staticMeshData.vertices.push_back(VertexTBN{ XMFLOAT3(w2, -h2,  d2), XMFLOAT3(0, -1, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, -1), color, XMFLOAT2(1, 1) });

	staticMeshData.indices = {
		0, 1, 2, 2, 3, 0,		// 오른쪽 (+X 쪽)
		4, 5, 6, 6, 7, 4,		// 왼쪽 (X 쪽)
		8, 9, 10, 10, 11, 8,	// 상부 표면 (+y 표면)
		12, 13, 14, 14, 15, 12,	// 바닥 (Y 측면)
		16, 17, 18, 18, 19, 16, // 뒤로 (+z 측)
		20, 21, 22, 22, 23, 20	// 전면 (Z 측)
	};

	return staticMeshData;
}

void StaticMesh::AssignMemory(ID3D11Device*& m_pDevice, ID3D11Buffer*& m_pVertexBuffer, StaticMeshData& meshData)
{
	D3D11_BUFFER_DESC vbDesc = {};
	ZeroMemory(&vbDesc, sizeof(vbDesc));												// vbDesc에 0으로 전체 메모리 영역을 초기화 시킵니다
	vbDesc.ByteWidth = (UINT)(sizeof(VertexTBN) * meshData.vertices.size());				// 배열 전체의 바이트 크기를 바로 반환합니다
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA vbData = {};
	ZeroMemory(&vbData, sizeof(vbData));
	vbData.pSysMem = meshData.vertices.data();												// 배열 데이터 할당.
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));
}

void StaticMesh::AssignIndexMemory(ID3D11Device*& m_pDevice, ID3D11Buffer*& m_pIndexBuffer, StaticMeshData& meshData, int& m_nIndices)
{
	D3D11_BUFFER_DESC ibDesc = {};
	ZeroMemory(&ibDesc, sizeof(ibDesc));
	m_nIndices = (int)meshData.indices.size();	// 인덱스 개수 저장.
	ibDesc.ByteWidth = (UINT)(sizeof(DWORD) * meshData.indices.size());
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = meshData.indices.data();
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));
}

// ---------------- LightTex 버전 ----------------
StaticMeshDataLightTex StaticMesh::CreateBoxLightTex(const XMFLOAT4& color, float width, float height, float depth)
{
	using namespace DirectX;
	StaticMeshDataLightTex data;
	data.vertices.reserve(24);
	data.indices.reserve(36);

	const float w2 = width * 0.5f;
	const float h2 = height * 0.5f;
	const float d2 = depth * 0.5f;

	// Front (-Z)
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2, -h2, -d2), XMFLOAT3(0, 0, -1), color, XMFLOAT2(0, 1) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2,  h2, -d2), XMFLOAT3(0, 0, -1), color, XMFLOAT2(0, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2,  h2, -d2), XMFLOAT3(0, 0, -1), color, XMFLOAT2(1, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2, -h2, -d2), XMFLOAT3(0, 0, -1), color, XMFLOAT2(1, 1) });

	// Left (-X)
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2, -h2,  d2), XMFLOAT3(-1, 0, 0), color, XMFLOAT2(0, 1) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2,  h2,  d2), XMFLOAT3(-1, 0, 0), color, XMFLOAT2(0, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2,  h2, -d2), XMFLOAT3(-1, 0, 0), color, XMFLOAT2(1, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2, -h2, -d2), XMFLOAT3(-1, 0, 0), color, XMFLOAT2(1, 1) });

	// Top (+Y)
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2, h2, -d2), XMFLOAT3(0, 1, 0), color, XMFLOAT2(0, 1) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2, h2,  d2), XMFLOAT3(0, 1, 0), color, XMFLOAT2(0, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2, h2,  d2), XMFLOAT3(0, 1, 0), color, XMFLOAT2(1, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2, h2, -d2), XMFLOAT3(0, 1, 0), color, XMFLOAT2(1, 1) });

	// Back (+Z)
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2, -h2,  d2), XMFLOAT3(0, 0, 1), color, XMFLOAT2(0, 1) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2,  h2,  d2), XMFLOAT3(0, 0, 1), color, XMFLOAT2(0, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2, h2,  d2), XMFLOAT3(0, 0, 1), color, XMFLOAT2(1, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2, -h2,  d2), XMFLOAT3(0, 0, 1), color, XMFLOAT2(1, 1) });

	// Right (+X)
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2, -h2, -d2), XMFLOAT3(1, 0, 0), color, XMFLOAT2(0, 1) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2,  h2, -d2), XMFLOAT3(1, 0, 0), color, XMFLOAT2(0, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2, h2,  d2), XMFLOAT3(1, 0, 0), color, XMFLOAT2(1, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2, -h2,  d2), XMFLOAT3(1, 0, 0), color, XMFLOAT2(1, 1) });

	// Bottom (-Y)
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2, -h2,  d2), XMFLOAT3(0, -1, 0), color, XMFLOAT2(0, 1) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(-w2, -h2, -d2), XMFLOAT3(0, -1, 0), color, XMFLOAT2(0, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2, -h2, -d2), XMFLOAT3(0, -1, 0), color, XMFLOAT2(1, 0) });
	data.vertices.push_back(VertexLightTex{ XMFLOAT3(w2, -h2,  d2), XMFLOAT3(0, -1, 0), color, XMFLOAT2(1, 1) });

	data.indices = {
		0, 1, 2, 2, 3, 0,		// 오른쪽 (+X 쪽)
		4, 5, 6, 6, 7, 4,		// 왼쪽 (X 쪽)
		8, 9, 10, 10, 11, 8,	// 상부 표면 (+y 표면)
		12, 13, 14, 14, 15, 12,	// 바닥 (Y 측면)
		16, 17, 18, 18, 19, 16, // 뒤로 (+z 측)
		20, 21, 22, 22, 23, 20	// 전면 (Z 측)
	};

	return data;
}


void StaticMesh::AssignMemoryLightTex(ID3D11Device*& device, ID3D11Buffer*& outVB, StaticMeshDataLightTex& meshData)
{
	D3D11_BUFFER_DESC vbDesc = {};
	ZeroMemory(&vbDesc, sizeof(vbDesc));
	vbDesc.ByteWidth = sizeof(VertexLightTex) * meshData.vertices.size();
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA vbData = {};
	ZeroMemory(&vbData, sizeof(vbData));
	vbData.pSysMem = meshData.vertices.data();
	HR_T(device->CreateBuffer(&vbDesc, &vbData, &outVB));
}

void StaticMesh::AssignIndexMemoryLightTex(ID3D11Device*& device, ID3D11Buffer*& outIB, StaticMeshDataLightTex& meshData, int& outIndexCount)
{
	D3D11_BUFFER_DESC ibDesc = {};
	ZeroMemory(&ibDesc, sizeof(ibDesc));
	outIndexCount = meshData.indices.size();
	ibDesc.ByteWidth = sizeof(DWORD) * meshData.indices.size();
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = meshData.indices.data();
	HR_T(device->CreateBuffer(&ibDesc, &ibData, &outIB));
}
