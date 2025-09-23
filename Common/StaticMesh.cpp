#include "pch.h"
#include "StaticMesh.h"
#include <vector>
#include "Helper.h"
#include <d3d11.h>

// 박스를 만드는 코드
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
	* @brief  정점(Vertex) 배열을 GPU 버퍼로 생성
	* @details
	*   - ByteWidth : 정점 전체 크기(정점 크기 × 개수)
	*   - BindFlags : D3D11_BIND_VERTEX_BUFFER
	*   - Usage     : DEFAULT (일반적 용도)
	*   - 초기 데이터 : vbData.pSysMem = vertices
	*   - Stride/Offset : IASetVertexBuffers용 파라미터
	*   - 주의 : VertexInfo(color=Vec3), 셰이더/InputLayout의 COLOR 형식 일치 필요
	*/

	// 정점 24개 Front, Left, Top, Back, Right, Bottom (6면 * 4점씩)
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
	* @brief  인덱스 버퍼(Index Buffer) 생성
	* @details
	*   - indices: 정점 재사용용 (사각형 = 삼각형 2개 = 인덱스 6개)
	*   - WORD 타입 → DXGI_FORMAT_R16_UINT 사용 예정
	*   - ByteWidth : 전체 인덱스 배열 크기
	*   - BindFlags : D3D11_BIND_INDEX_BUFFER
	*   - Usage     : DEFAULT (GPU 일반 접근)
	*   - 이 코드의 결과: m_pIndexBuffer 생성, m_nIndices에 개수 저장
	*/

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
	ZeroMemory(&vbDesc, sizeof(vbDesc));											// vbDesc에 0으로 전체 메모리 영역을 초기화 시킵니다
	vbDesc.ByteWidth = sizeof(VertexData) * meshData.vertices.size();				// 배열 전체의 바이트 크기를 바로 반환합니다
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA vbData = {};
	ZeroMemory(&vbData, sizeof(vbData));
	vbData.pSysMem = meshData.vertices.data();										// 배열 데이터 할당.
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));
}

void StaticMesh::AssignIndexMemory(ID3D11Device*& m_pDevice, ID3D11Buffer*& m_pIndexBuffer, StaticMeshData& meshData, int& m_nIndices)
{
	D3D11_BUFFER_DESC ibDesc = {};
	ZeroMemory(&ibDesc, sizeof(ibDesc));
	m_nIndices = meshData.indices.size();	// 인덱스 개수 저장.
	ibDesc.ByteWidth = sizeof(DWORD) * meshData.indices.size();
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = meshData.indices.data();
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));
}
