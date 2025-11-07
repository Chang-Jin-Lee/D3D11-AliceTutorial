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

// 디버그 라인 메쉬
namespace {
    static inline VertexCubePosColor MakeV(const DirectX::XMFLOAT3& p, const DirectX::XMFLOAT4& c)
    {
        VertexCubePosColor v{};
        v.pos = p;
        v.color = c;
        return v;
    }
}

DebugLineMeshData StaticMesh::CreateBoxLines(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& extents, const DirectX::XMFLOAT4& color)
{
    DebugLineMeshData data;
    data.vertices.reserve(8);
    data.indices.reserve(24);

    const float ex = extents.x;
    const float ey = extents.y;
    const float ez = extents.z;
    const DirectX::XMFLOAT3 c = center;

    DirectX::XMFLOAT3 corners[8] = {
        { c.x - ex, c.y - ey, c.z - ez }, // 0
        { c.x + ex, c.y - ey, c.z - ez }, // 1
        { c.x + ex, c.y - ey, c.z + ez }, // 2
        { c.x - ex, c.y - ey, c.z + ez }, // 3
        { c.x - ex, c.y + ey, c.z - ez }, // 4
        { c.x + ex, c.y + ey, c.z - ez }, // 5
        { c.x + ex, c.y + ey, c.z + ez }, // 6
        { c.x - ex, c.y + ey, c.z + ez }  // 7
    };

    for (int i = 0; i < 8; ++i)
    {
        data.vertices.emplace_back(MakeV(corners[i], color));
    }

    static const DWORD s_edges[] = {
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    };
    data.indices.assign(std::begin(s_edges), std::end(s_edges));
    return data;
}

DebugLineMeshData StaticMesh::CreateRing(const DirectX::XMFLOAT3& origin,
    const DirectX::XMFLOAT3& majorAxis,
    const DirectX::XMFLOAT3& minorAxis,
    const DirectX::XMFLOAT4& color,
    int segments)
{
    DebugLineMeshData data;
    segments = (segments < 3) ? 3 : segments;

    data.vertices.resize(static_cast<size_t>(segments));
    data.indices.reserve(static_cast<size_t>(segments) * 2);

    const float delta = DirectX::XM_2PI / static_cast<float>(segments);
    for (int i = 0; i < segments; ++i)
    {
        const float a = delta * static_cast<float>(i);
        const float cs = cosf(a);
        const float sn = sinf(a);

        DirectX::XMFLOAT3 pos;
        pos.x = origin.x + majorAxis.x * cs + minorAxis.x * sn;
        pos.y = origin.y + majorAxis.y * cs + minorAxis.y * sn;
        pos.z = origin.z + majorAxis.z * cs + minorAxis.z * sn;

        data.vertices[static_cast<size_t>(i)] = MakeV(pos, color);
    }

    for (int i = 0; i < segments; ++i)
    {
        DWORD a = static_cast<DWORD>(i);
        DWORD b = static_cast<DWORD>((i + 1) % segments);
        data.indices.push_back(a);
        data.indices.push_back(b);
    }

    return data;
}

DebugLineMeshData StaticMesh::CreateRay(const DirectX::XMFLOAT3& origin,
    const DirectX::XMFLOAT3& direction,
    float length,
    const DirectX::XMFLOAT4& color,
    bool addArrowHead)
{
    DebugLineMeshData data;

    DirectX::XMVECTOR dir = DirectX::XMLoadFloat3(&direction);
    DirectX::XMVECTOR o = DirectX::XMLoadFloat3(&origin);
    DirectX::XMVECTOR n = DirectX::XMVector3Normalize(dir);
    DirectX::XMVECTOR endV = DirectX::XMVectorMultiplyAdd(n, DirectX::XMVectorReplicate(length), o);

    DirectX::XMFLOAT3 end;
    DirectX::XMStoreFloat3(&end, endV);

    data.vertices.emplace_back(MakeV(origin, color));
    data.vertices.emplace_back(MakeV(end, color));
    data.indices.push_back(0); data.indices.push_back(1);

    if (addArrowHead)
    {
        DirectX::XMVECTOR worldY = DirectX::g_XMIdentityR1;
        DirectX::XMVECTOR perp = DirectX::XMVector3Cross(n, worldY);
        if (DirectX::XMVector3LessOrEqual(DirectX::XMVector3LengthSq(perp), DirectX::XMVectorZero()))
        {
            perp = DirectX::XMVector3Cross(n, DirectX::g_XMIdentityR2);
        }
        perp = DirectX::XMVector3Normalize(perp);

        const float wingScale = length * 0.075f;
        const float backScale = length * 0.20f;

        DirectX::XMVECTOR back = DirectX::XMVectorMultiplyAdd(n, DirectX::XMVectorReplicate(-backScale), endV);
        DirectX::XMVECTOR w1 = DirectX::XMVectorMultiplyAdd(perp, DirectX::XMVectorReplicate(wingScale), back);
        DirectX::XMVECTOR w2 = DirectX::XMVectorMultiplyAdd(perp, DirectX::XMVectorReplicate(-wingScale), back);

        DirectX::XMFLOAT3 p1, p2;
        DirectX::XMStoreFloat3(&p1, w1);
        DirectX::XMStoreFloat3(&p2, w2);

        data.vertices.emplace_back(MakeV(p1, color));
        data.vertices.emplace_back(MakeV(p2, color));

        data.indices.push_back(1); data.indices.push_back(2);
        data.indices.push_back(1); data.indices.push_back(3);
    }

    return data;
}

DebugLineMeshData StaticMesh::CreateGrid(
    const DirectX::XMFLOAT3& xAxis,
    const DirectX::XMFLOAT3& yAxis,
    const DirectX::XMFLOAT3& origin,
    int xDivs,
    int yDivs,
    const DirectX::XMFLOAT4& color)
{
    DebugLineMeshData data;
    if (xDivs < 1) xDivs = 1;
    if (yDivs < 1) yDivs = 1;

    data.vertices.reserve(static_cast<size_t>((xDivs + 1) * 2 + (yDivs + 1) * 2));
    data.indices.reserve(static_cast<size_t>((xDivs + 1) * 2 * 2 + (yDivs + 1) * 2 * 2));

    for (int i = 0; i <= xDivs; ++i)
    {
        float percent = static_cast<float>(i) / static_cast<float>(xDivs);
        percent = percent * 2.0f - 1.0f;

        DirectX::XMFLOAT3 a, b;
        a.x = origin.x + xAxis.x * percent - yAxis.x;
        a.y = origin.y + xAxis.y * percent - yAxis.y;
        a.z = origin.z + xAxis.z * percent - yAxis.z;

        b.x = origin.x + xAxis.x * percent + yAxis.x;
        b.y = origin.y + xAxis.y * percent + yAxis.y;
        b.z = origin.z + xAxis.z * percent + yAxis.z;

        DWORD base = static_cast<DWORD>(data.vertices.size());
        data.vertices.emplace_back(MakeV(a, color));
        data.vertices.emplace_back(MakeV(b, color));
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 1);
    }

    for (int i = 0; i <= yDivs; ++i)
    {
        float percent = static_cast<float>(i) / static_cast<float>(yDivs);
        percent = percent * 2.0f - 1.0f;

        DirectX::XMFLOAT3 a, b;
        a.x = origin.x - xAxis.x;
        a.y = origin.y - xAxis.y;
        a.z = origin.z - xAxis.z;

        b.x = origin.x + xAxis.x;
        b.y = origin.y + xAxis.y;
        b.z = origin.z + xAxis.z;

        a.x += yAxis.x * percent; a.y += yAxis.y * percent; a.z += yAxis.z * percent;
        b.x += yAxis.x * percent; b.y += yAxis.y * percent; b.z += yAxis.z * percent;

        DWORD base = static_cast<DWORD>(data.vertices.size());
        data.vertices.emplace_back(MakeV(a, color));
        data.vertices.emplace_back(MakeV(b, color));
        data.indices.push_back(base + 0);
        data.indices.push_back(base + 1);
    }

    return data;
}

void StaticMesh::AssignMemoryLines(ID3D11Device*& device, ID3D11Buffer*& outVB, DebugLineMeshData& meshData)
{
    D3D11_BUFFER_DESC vbDesc = {};
    ZeroMemory(&vbDesc, sizeof(vbDesc));
    vbDesc.ByteWidth = static_cast<UINT>(sizeof(VertexCubePosColor) * meshData.vertices.size());
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA vbData = {};
    ZeroMemory(&vbData, sizeof(vbData));
    vbData.pSysMem = meshData.vertices.data();
    HR_T(device->CreateBuffer(&vbDesc, &vbData, &outVB));
}

void StaticMesh::AssignIndexMemoryLines(ID3D11Device*& device, ID3D11Buffer*& outIB, DebugLineMeshData& meshData, int& outIndexCount)
{
    D3D11_BUFFER_DESC ibDesc = {};
    ZeroMemory(&ibDesc, sizeof(ibDesc));
    outIndexCount = static_cast<int>(meshData.indices.size());
    ibDesc.ByteWidth = static_cast<UINT>(sizeof(DWORD) * meshData.indices.size());
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.Usage = D3D11_USAGE_DEFAULT;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ZeroMemory(&ibData, sizeof(ibData));
    ibData.pSysMem = meshData.indices.data();
    HR_T(device->CreateBuffer(&ibDesc, &ibData, &outIB));
}