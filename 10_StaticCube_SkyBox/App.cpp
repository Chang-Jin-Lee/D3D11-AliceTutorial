/*
* @brief : d3d11�� ť�긦 ���ߴ� ������ �����Դϴ�
* @details :
* 
*     1) Normal�� ���Ե� ����(Vertex)���� ť�긦 �����ϰ�,
*     2) VertexShader���� Local Normal�� World Normal�� ��ȯ,
*     3) PixelShader���� Directional Light�� Normal�� ��ǻ�� ���� ó��,
*     4) ������۸� �ϳ�(b0)�� ����Ͽ� VS/PS ���� ������ ����,
*     5) ImGui�� Light(Color/Direction/Position)�� ť�� ȸ��(Yaw/Pitch) ����,
*     6) ����׿� ����Ʈ ��ġ ��Ŀ(���� ��� ť��) ���.
*
*   - ���������� ����
*     IA: POSITION(float3), NORMAL(float3), COLOR(float4) �Է� ���̾ƿ�
*     VS: posW, normalW ��� (g_World, g_WorldInvTranspose ���), Ŭ������ ��ȯ
*     PS: baseColor * (ambient + diffuse)�� �ܼ� ����, g_Pad ��۷� ����� �÷�
*     CB: ConstantBuffer(b0) ����. world/view/proj/worldInvTranspose/dirLight/eyePos/pad ����
*
*   - �������
*     world/view/proj: �����Ӻ� ������Ʈ
*     worldInvTranspose: XMMatrixTranspose(XMMatrixInverse(...))�� ���
*     dirLight: ImGui �Է� �ݿ� (color, direction ����ȭ)
*     eyePos: ī�޶� ��ġ
*     pad: ����� ��� (1.0=�����, 2.0=��� ��Ŀ)
*
*   - ImGui
*     Mesh: Root Pos, Yaw(deg), Pitch(deg)
*     Camera: Position, FOV, Near/Far
*     Light: Color(RGB), Direction(x,y,z), Position(x,y,z)
*
*   - ����� ��Ŀ
*     ����Ʈ ��ġ�� ǥ���ϴ� ���� ť��(������ 0.2). ������� pad=2.0���� PS���� ��� ť�� ���� ���
*
*   - ���ǻ���
*     1) �ε��� ���� ���˰� DXGI_FORMAT(R32_UINT/R16_UINT) ��ġ
*     2) �Է� ���̾ƿ��� ���� ����ü, VS �Է� �ñ״�ó ���ռ� ����
*     3) ��� ��ġ �Ծ�(HLSL ��-�ֵ�) ��ġ: CPU���� Transpose ���� ������Ʈ
*/

#include "App.h"
#include "../Common/Helper.h"
#include <d3dcompiler.h>
#include <directxtk/WICTextureLoader.h>
#include <directxtk/DDSTextureLoader.h>
#include <thread>
#include <filesystem>
#include <algorithm>
#include "../Common/StaticMesh.h"

static bool g_RotateCube = true; // ImGui ���: ť�� �ڵ� ȸ�� on/off

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

static bool LoadTextureSRVAndSize(ID3D11Device* device, const std::wstring& path,
	ID3D11ShaderResourceView** outSRV, ImVec2* outSize)
{
	if (!std::filesystem::exists(path)) return false;
	if (FAILED(CreateTextureFromFile(device, path.c_str(), outSRV))) return false;
	if (outSRV && *outSRV)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> res;
		(*outSRV)->GetResource(res.GetAddressOf());
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
		if (SUCCEEDED(res.As(&tex2D)))
		{
			D3D11_TEXTURE2D_DESC desc{};
			tex2D->GetDesc(&desc);
			if (outSize) *outSize = ImVec2((float)desc.Width, (float)desc.Height);
		}
	}
	return true;
}


void App::PrepareSkyFaceSRVs()
{
	// �ٸ� ��ī�̹ڽ��� �ٲ� ���� ������ �����ϰ� �ٽ� �ε�
	for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_pSkyFaceSRV[i]);
    m_SkyFaceSize = ImVec2(0, 0);
    if (!m_pTextureSRV) return;

	Microsoft::WRL::ComPtr<ID3D11Resource> res;
    m_pTextureSRV->GetResource(res.GetAddressOf());
    if (!res) return;

	// �ı��ƴ��� �ȵƴ��� �Ǵ��� ���� Comptr�� �ʿ��ϴ�
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
	HR_T(res.As(&tex2D));

    D3D11_TEXTURE2D_DESC desc{};
    tex2D->GetDesc(&desc);
    // ť����� 6���� array slice�� ����. (���� ť��� 6�� ���)
    if ((desc.ArraySize < 6)) return;

    // ũ�� ��� (mip0 ����)
    m_SkyFaceSize = ImVec2((float)desc.Width, (float)desc.Height);

    for (UINT face = 0; face < 6; ++face)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = desc.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        sd.Texture2DArray.MostDetailedMip = 0;
        sd.Texture2DArray.MipLevels = desc.MipLevels;
        sd.Texture2DArray.FirstArraySlice = face;
        sd.Texture2DArray.ArraySize = 1;
        ID3D11ShaderResourceView* faceSRV = nullptr;
        if (SUCCEEDED(m_pDevice->CreateShaderResourceView(tex2D.Get(), &sd, &faceSRV)))
        {
            m_pSkyFaceSRV[face] = faceSRV;
        }
    }
}

bool App::OnInitialize()
{
	if(!InitD3D()) 
		return false;

	if (!InitBasicEffect()) 
		return false;

	if (!InitSkyBoxEffect()) 
		return false;

	if(!InitScene()) 
		return false;

	// ImGui �ʱ�ȭ
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);

    // Hanako.png�� ����׿��̾�����, ���� ��ī�̹ڽ� ���� ���� �׸� ����
    // �ʱ� ��ī�̹ڽ� �� SRV���� �غ��Ѵ�
    PrepareSkyFaceSRVs();

	// �ý��� ���� ���� (GPU)
	{
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		if (SUCCEEDED(m_pDevice->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()))))
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
			if (SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf())))
			{
				DXGI_ADAPTER_DESC desc{};
				if (SUCCEEDED(adapter->GetDesc(&desc)))
				{
					m_GPUName = desc.Description;
				}
				adapter.As(&m_Adapter3);
				if (m_Adapter3)
				{
					DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
					if (SUCCEEDED(m_Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)))
					{
						m_VideoMemoryTotal = memInfo.Budget; // ��� ���� ������ �ѷ� �ٻ�� ���
					}
				}
			}
		}
	}

	// �ý��� ���� ���� (CPU)
	{
		wchar_t cpuName[128] = L"";
		DWORD size = sizeof(cpuName);
		HKEY hKey;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			DWORD type = 0;
			RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, &type, reinterpret_cast<LPBYTE>(cpuName), &size);
			RegCloseKey(hKey);
		}
		m_CPUName = cpuName;
		m_CPUCores = std::thread::hardware_concurrency();
	}

	// �ý��� �޸� �ѷ�/���뷮 �ʱ�ȭ
	{
		MEMORYSTATUSEX ms{ sizeof(MEMORYSTATUSEX) };
		if (GlobalMemoryStatusEx(&ms))
		{
			m_RamTotal = ms.ullTotalPhys;
			m_RamAvail = ms.ullAvailPhys;
		}
	}

	return true;
}

void App::OnUninitialize()
{
	// ImGui ����
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	UninitD3D();
}

void App::OnUpdate(const float& dt)
{
	// ���� ��ȯ ���� (���� Scene Graph)
	if(g_RotateCube)
		m_YawDeg += 45.0f * dt; // Yaw ȸ�� (UI�� ���� ����)
	m_YawDeg = std::fmod(m_YawDeg + 180.0f, 360.0f) - 180.0f;
	XMMATRIX rotYaw   = XMMatrixRotationY(XMConvertToRadians(m_YawDeg));
	XMMATRIX rotPitch = XMMatrixRotationX(XMConvertToRadians(m_PitchDeg));
		XMMATRIX local0 = rotPitch * rotYaw * XMMatrixTranslation(m_cubePos.x, m_cubePos.y, m_cubePos.z); // ��Ʈ
	XMMATRIX world0 = local0; // ��Ʈ

	XMFLOAT3 camForward3 = m_CameraForward;
	static float sYaw = 0.0f, sPitch = 0.0f;
	ImGuiIO& io = ImGui::GetIO();
	bool rmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.WantCaptureMouse;
	float d1 = 0.0f, d2 = 0.0f, d3 = 0.0f;
	if (rmbDown && !io.WantCaptureKeyboard)
	{
		if (ImGui::IsKeyDown(ImGuiKey_W)) d1 += dt;
		if (ImGui::IsKeyDown(ImGuiKey_S)) d1 -= dt;
		if (ImGui::IsKeyDown(ImGuiKey_A)) d2 -= dt;
		if (ImGui::IsKeyDown(ImGuiKey_D)) d2 += dt;
		if (ImGui::IsKeyDown(ImGuiKey_E)) d3 += dt;
		if (ImGui::IsKeyDown(ImGuiKey_Q)) d3 -= dt;
	}

	if (rmbDown)
	{
		float rotSpeed = 0.005f;
		sYaw   += io.MouseDelta.x * rotSpeed;
		sPitch += -io.MouseDelta.y * rotSpeed;
		float limit = XMConvertToRadians(89.0f);
		sPitch = min(max(sPitch, -limit), limit);
	}
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR forward = XMVector3Normalize(XMVectorSet(cosf(sPitch) * sinf(sYaw), sinf(sPitch), cosf(sPitch) * cosf(sYaw), 0.0f));
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
	XMVECTOR posV = XMVectorSet(m_cameraPos.x, m_cameraPos.y, m_cameraPos.z, 0.0f);
	float moveSpeed = 5.0f;
	if (rmbDown)
	{
		posV = XMVectorAdd(posV, XMVectorScale(forward, d1 * moveSpeed));
		posV = XMVectorAdd(posV, XMVectorScale(right,   d2 * moveSpeed));
		posV = XMVectorAdd(posV, XMVectorScale(up,      d3 * moveSpeed));
	}
	XMStoreFloat3(&m_cameraPos, posV);
	XMStoreFloat3(&camForward3, forward);
	m_CameraForward = camForward3;

	// View/Proj�� UI�� �ݿ� (�� ������)
	XMMATRIX view = XMMatrixTranspose(XMMatrixLookAtLH(
		XMVectorSet(m_cameraPos.x, m_cameraPos.y, m_cameraPos.z, 0.0f),
		XMVectorAdd(XMVectorSet(m_cameraPos.x, m_cameraPos.y, m_cameraPos.z, 0.0f), XMVector3Normalize(XMVectorSet(camForward3.x, camForward3.y, camForward3.z, 0.0f))),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
	float fovRad = XMConvertToRadians(m_CameraFovDeg);
	XMMATRIX proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovRad, AspectRatio(), m_CameraNear, m_CameraFar));
	m_baseProjection.world = XMMatrixTranspose(world0);
	m_baseProjection.view = view;
	m_baseProjection.proj = proj;

	m_baseProjection.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(world0)));
	{
		XMFLOAT3 dir = m_LightDirection;
		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&dir));
		XMStoreFloat3(&dir, v);
		m_baseProjection.dirLight = DirectionalLight(
			DirectX::XMFLOAT4(m_LightColorRGB.x, m_LightColorRGB.y, m_LightColorRGB.z, 1.0f),
			dir
		);
	}
	m_baseProjection.eyePos = m_cameraPos;
	m_baseProjection.pad = 0.0f;

	// FPS 1�� ������Ʈ
	m_FpsTimer += dt;
	if (m_FpsTimer >= 1.0f)
	{
		m_LastFps = 1.0f / dt; // ������ �ֱ� �������� ���� ���
		m_FpsTimer = 0.0f;
	}

	// �ý��� �޸� ���뷮 ����
	{
		MEMORYSTATUSEX ms{ sizeof(MEMORYSTATUSEX) };
		if (GlobalMemoryStatusEx(&ms))
		{
			m_RamTotal = ms.ullTotalPhys;
			m_RamAvail = ms.ullAvailPhys;
		}
	}

	// VRAM ��뷮 ���� (������ ���)
	if (m_Adapter3)
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
		if (SUCCEEDED(m_Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)))
		{
			// Budget�� ��� ���� �ѵ�, CurrentUsage�� ���� ��뷮
			m_VideoMemoryTotal = memInfo.Budget;
			// ��뷮�� memInfo.CurrentUsage�� ���� ���, ǥ�� �� ��ȯ
		}
	}
}

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{
	return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}
inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) 
{
	return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

// Render() �Լ��� �߿��� �κ��� �� ����ֽ��ϴ�. ���⸦ ���� �˴ϴ�
void App::OnRender()
{
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	UINT stride = m_VertextBufferStride;	// ����Ʈ ��
	UINT offset = m_VertextBufferOffset;

	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 1 ~ 3 . IA �ܰ� ����
	// ������ ��� �̾ �׸� �������� �����ϴ� �κ�
	// 1. ���۸� ����ֱ�
	// 2. �Է� ���̾ƿ��� ����ֱ�
	// 3. �ε��� ���۸� ����ֱ�
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// 4. Vertex Shader ����
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	// 5. Pixel Shader ����
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	// 6. Constant Buffer ���� (���� GlobalCB, b0)
	// �����Ӻ� ����� �غ�
	m_ConstantBuffer.world = m_baseProjection.world;
	m_ConstantBuffer.view  = m_baseProjection.view;
	m_ConstantBuffer.proj  = m_baseProjection.proj;
	{
		// worldInvTranspose ��� (CPU������ ���� world�� ��/��ġ ��, HLSL ����Ʈ�������� �Ծ࿡ �°� ����)
		// ��յ� �������� �ذ��� �ڵ�
		auto invWorlNormal = XMMatrixInverse(nullptr, m_baseProjection.world);
		m_ConstantBuffer.worldInvTranspose = XMMatrixTranspose(invWorlNormal);
	}
	// ���� �⺻ ����/ī�޶� (UI �ݿ�)
	{
		XMFLOAT3 dir = m_LightDirection;
		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&dir));
		XMStoreFloat3(&dir, v);
		m_ConstantBuffer.dirLight = DirectionalLight(
			DirectX::XMFLOAT4(m_LightColorRGB.x, m_LightColorRGB.y, m_LightColorRGB.z, 1.0f),
			dir
		);
	}
	m_ConstantBuffer.eyePos = m_cameraPos;
	m_ConstantBuffer.pad = 0.0f;

	D3D11_MAPPED_SUBRESOURCE mappedData;
	HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
	memcpy_s(mappedData.pData, sizeof(ConstantBuffer), &m_ConstantBuffer, sizeof(ConstantBuffer));
	m_pDeviceContext->Unmap(m_pConstantBuffer, 0);

	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

	// 7. �׸���
	m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);

	// ����Ʈ ��ġ ��Ŀ ť�� �׸��� (���� ������, ���)
	{
		ConstantBuffer marker = m_ConstantBuffer;
		XMFLOAT3 dir = m_LightDirection;
		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&dir));
		XMMATRIX S = XMMatrixScaling(0.2f, 0.2f, 0.2f);
		XMMATRIX T = XMMatrixTranslation(m_LightPosition.x, m_LightPosition.y, m_LightPosition.z);
		marker.world = XMMatrixTranspose(S * T);
		// ���� �ι��� ��ġ ����
		marker.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(S * T)));
		// ��� ��Ŀ ����
		marker.dirLight.color = XMFLOAT4(1,1,1,1);
		marker.pad = 2.0f; // PS���� ��� ��� ���

		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &marker, sizeof(ConstantBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);
	}

	// ����Ʈ ���� ǥ�� ���� �׸���(������)
	{
		struct LineV { XMFLOAT3 pos; XMFLOAT3 normal; XMFLOAT4 color; };
		LineV line[2] = {};
		XMFLOAT3 dir = m_LightDirection;
		XMVECTOR vdir = XMVector3Normalize(XMLoadFloat3(&dir));
		XMVECTOR p0 = XMLoadFloat3(&m_LightPosition);
		XMVECTOR p1 = XMVectorAdd(p0, XMVectorScale(vdir, 2.0f));
		XMStoreFloat3(&line[0].pos, p0);
		XMStoreFloat3(&line[1].pos, p1);
		line[0].normal = XMFLOAT3(0,1,0);
		line[1].normal = XMFLOAT3(0,1,0);
		line[0].color = XMFLOAT4(1,0,0,1);
		line[1].color = XMFLOAT4(1,0,0,1);

		if (!m_pLineVertexBuffer)
		{
			D3D11_BUFFER_DESC bd{};
			bd.ByteWidth = sizeof(line);
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			bd.Usage = D3D11_USAGE_DYNAMIC;
			bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pLineVertexBuffer));
		}
		{
			D3D11_MAPPED_SUBRESOURCE mapped;
			HR_T(m_pDeviceContext->Map(m_pLineVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
			memcpy_s(mapped.pData, sizeof(line), line, sizeof(line));
			m_pDeviceContext->Unmap(m_pLineVertexBuffer, 0);
		}

		UINT lstride = sizeof(LineV);
		UINT loffset = 0;
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pLineVertexBuffer, &lstride, &loffset);
		m_pDeviceContext->IASetInputLayout(m_pInputLayout);
		// ��/������ ����� ����(�������) ���, pad=3.0 -> ������ ���� ���
		ConstantBuffer lineCB = m_ConstantBuffer;
		lineCB.world = XMMatrixTranspose(XMMatrixIdentity());
		lineCB.worldInvTranspose = XMMatrixIdentity();
		lineCB.pad = 3.0f;
		D3D11_MAPPED_SUBRESOURCE mappedLine;
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedLine));
		memcpy_s(mappedLine.pData, sizeof(ConstantBuffer), &lineCB, sizeof(ConstantBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
		m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
		m_pDeviceContext->Draw(2, 0);
	}


	// SkyBox ������ (���� ����/����)
	{
		ID3D11RasterizerState* prevRS = nullptr;
		ID3D11DepthStencilState* prevDS = nullptr; UINT prevRef = 0;
		m_pDeviceContext->RSGetState(&prevRS);
		m_pDeviceContext->OMGetDepthStencilState(&prevDS, &prevRef);

		static ID3D11DepthStencilState* dsSky = nullptr;
		if (!dsSky)
		{
			D3D11_DEPTH_STENCIL_DESC dsd{};
			dsd.DepthEnable = TRUE;
			dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
			dsd.StencilEnable = FALSE;
			HR_T(m_pDevice->CreateDepthStencilState(&dsd, &dsSky));
		}
		m_pDeviceContext->OMSetDepthStencilState(dsSky, 0);

		static ID3D11RasterizerState* rsSky = nullptr;
		if (!rsSky)
		{
			D3D11_RASTERIZER_DESC rd{};
			rd.FillMode = D3D11_FILL_SOLID;
			rd.CullMode = D3D11_CULL_NONE;         // ���θ鸸 ���̰� ����
			rd.FrontCounterClockwise = false;        // CCW�� Front�� ����
			rd.DepthClipEnable = TRUE;
			HR_T(m_pDevice->CreateRasterizerState(&rd, &rsSky));
		}
		m_pDeviceContext->RSSetState(rsSky);

		// ��ī�̹ڽ� WVP: world=I, view=translation ����, proj=�Ϲ�(Projt)
		XMMATRIX viewT = m_baseProjection.view; // transposed view
		XMMATRIX view  = XMMatrixTranspose(viewT);
		view.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, XMVectorGetW(view.r[3]));
		XMMATRIX viewNoTransT = XMMatrixTranspose(view);
		XMMATRIX projT = m_baseProjection.proj;
		XMMATRIX wvpT = XMMatrixMultiply(projT, viewNoTransT);
		// ���� CB(b0) ���: SkyBox VS�� ����ϴ� g_WorldViewProj ��ġ(���� 64����Ʈ)���� wvpT�� ���ش�
		{
			D3D11_MAPPED_SUBRESOURCE mapped{};
			HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
			memcpy_s(mapped.pData, sizeof(XMMATRIX), &wvpT, sizeof(XMMATRIX));
			m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
		}
		m_pDeviceContext->IASetInputLayout(m_pSkyBoxInputLayout);
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
		m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		m_pDeviceContext->VSSetShader(m_pSkyBoxVertexShader, nullptr, 0);
		m_pDeviceContext->PSSetShader(m_pSkyBoxPixelShader, nullptr, 0);
		static ID3D11SamplerState* skySamp = nullptr;
		if (!skySamp)
		{
			D3D11_SAMPLER_DESC sd{};
			sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.MaxLOD = D3D11_FLOAT32_MAX;
			HR_T(m_pDevice->CreateSamplerState(&sd, &skySamp));
		}
		m_pDeviceContext->PSSetSamplers(0, 1, &skySamp);
		m_pDeviceContext->PSSetShaderResources(0, 1, &m_pTextureSRV);
		m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);

		// ���� ����
		m_pDeviceContext->OMSetDepthStencilState(prevDS, prevRef);
		m_pDeviceContext->RSSetState(prevRS);
		SAFE_RELEASE(prevDS);
		SAFE_RELEASE(prevRS);
	}

	// ImGui ������ �� UI ������
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (ImGui::Begin("Controls"))
	{
		// SkyBox ����
		{
			int cur = (m_SkyBoxChoice == SkyBoxChoice::Hanako) ? 0 : 1;
			const char* items[] = { "Hanako.dds", "cubemap.dds" };
			if (ImGui::Combo("SkyBox Choice", &cur, items, IM_ARRAYSIZE(items)))
			{
				m_SkyBoxChoice = (cur == 0) ? SkyBoxChoice::Hanako : SkyBoxChoice::CubeMap;
				m_pTextureSRV = (m_SkyBoxChoice == SkyBoxChoice::Hanako) ? m_pSkyHanakoSRV : m_pSkyCubeMapSRV;
				PrepareSkyFaceSRVs();
			}
		}
		ImGui::Text("Mesh Transforms");
		ImGui::Checkbox("Rotate Cube", &g_RotateCube);
		ImGui::DragFloat3("Root Pos (x,y,z)", &m_cubePos.x, 0.1f);
		ImGui::SliderFloat("Yaw (deg)", &m_YawDeg, -180.0f, 180.0f);
		ImGui::SliderFloat("Pitch (deg)", &m_PitchDeg, -89.9f, 89.9f);
		ImGui::Separator();
		ImGui::Text("Camera");
		ImGui::DragFloat3("Camera Pos (x,y,z)", &m_cameraPos.x, 0.1f);
		ImGui::SliderFloat("Camera FOV (deg)", &m_CameraFovDeg, 30.0f, 120.0f);
		ImGui::DragFloatRange2("Near/Far", &m_CameraNear, &m_CameraFar, 0.1f, 0.01f, 5000.0f, "Near: %.2f", "Far: %.2f");
		ImGui::Separator();
		ImGui::Text("Light");
		ImGui::ColorEdit3("Light Color", &m_LightColorRGB.x);
		ImGui::DragFloat3("Light Dir", &m_LightDirection.x, 0.05f);
		ImGui::DragFloat3("Light Pos", &m_LightPosition.x, 0.1f);
	}
	ImGui::End();

	// ���� ī�޶� ������ ���� ��ī�̹ڽ� �� �̹����� ǥ��
	{
		int face = 0;
		using namespace DirectX;
    	XMVECTOR f = XMLoadFloat3(&m_CameraForward);
    	XMVECTOR fn = XMVector3Normalize(f);
    	XMFLOAT3 v; XMStoreFloat3(&v, fn);
    	float ax = fabsf(v.x), ay = fabsf(v.y), az = fabsf(v.z);
    	if (ax >= ay && ax >= az) face = (v.x >= 0.0f) ? 0 : 1; // +X / -X
    	else if (ay >= ax && ay >= az) face = (v.y >= 0.0f) ? 2 : 3; // +Y / -Y
    	else face =  (v.z >= 0.0f) ? 4 : 5;     // +Z / -Z

		ID3D11ShaderResourceView* faceSRV = (face >= 0 && face < 6) ? m_pSkyFaceSRV[face] : nullptr;
		if (faceSRV)
		{
			ImGui::SetNextWindowPos(ImVec2(810, 210), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(m_HanakoDrawSize.x + 50, m_HanakoDrawSize.y + 80), ImGuiCond_Once);
			if (ImGui::Begin("Skybox Face"))
			{
				ImGui::BeginChild("SkyFaceView", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
				const ImVec2 tex = (m_HanakoDrawSize.x > 0 && m_HanakoDrawSize.y > 0) ? m_HanakoDrawSize : m_SkyFaceSize;
				ImVec2 avail = ImGui::GetContentRegionAvail();
				float sx = (tex.x > 0.f) ? (avail.x / tex.x) : 1.f;
				float sy = (tex.y > 0.f) ? (avail.y / tex.y) : 1.f;
				float scale = (sx > 0.f && sy > 0.f) ? min(sx, sy) : 1.f;
				ImVec2 draw = ImVec2(tex.x * scale, tex.y * scale);
				ImVec2 start = ImGui::GetCursorPos();
				ImVec2 offset = ImVec2((avail.x - draw.x) * 0.5f, (avail.y - draw.y) * 0.5f);
				ImGui::SetCursorPos(start + offset);
				ImGui::Image((ImTextureID)faceSRV, draw);
				ImVec2 r0 = ImGui::GetItemRectMin();
				ImVec2 r1 = ImGui::GetItemRectMax();
				ImGui::GetWindowDrawList()->AddRect(r0 - ImVec2(2, 2), r1 + ImVec2(2, 2), IM_COL32(255, 255, 255, 160), 8.0f, 0, 2.0f);
				ImGui::EndChild();
			}
			ImGui::End();
		}
	}


	// ����: �ý��� ����(FPS/GPU/CPU)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 size(420.0f, 180.0f);
		ImVec2 pos(io.DisplaySize.x - size.x - 10.0f, 10.0f);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(size, ImGuiCond_Always);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
		if (ImGui::Begin("System Info", nullptr, flags))
		{
			ImGui::Text("FPS : %.1f", m_LastFps);
			ImGui::Separator();
			ImGui::Text("GPU : %ls", m_GPUName.c_str());
			ImGui::Text("CPU : %ls (%u cores)", m_CPUName.c_str(), m_CPUCores);
			ImGui::Separator();
			// RAM
			double ramTotalGB = (double)m_RamTotal / (1024.0 * 1024.0 * 1024.0);
			double ramUsedGB = (double)(m_RamTotal - m_RamAvail) / (1024.0 * 1024.0 * 1024.0);
			ImGui::Text("RAM : %.2f GB / %.2f GB", ramUsedGB, ramTotalGB);
			// VRAM (Budget/Usage�� �����Ǵ� ���)
			if (m_Adapter3)
			{
				DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
				if (SUCCEEDED(m_Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)))
				{
					double vramBudgetGB = (double)memInfo.Budget / (1024.0 * 1024.0 * 1024.0);
					double vramUsageGB = (double)memInfo.CurrentUsage / (1024.0 * 1024.0 * 1024.0);
					ImGui::Text("VRAM : %.2f GB / %.2f GB", vramUsageGB, vramBudgetGB);
				}
			}
		}
		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	m_pSwapChain->Present(0, 0);
}

bool App::InitD3D()
{
	HRESULT hr = S_OK;

	// ����ü���� ������ ������ ����ü�� ����ϴ�
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;
	swapDesc.Windowed = true;
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;

	// DXGI_RATIONAL? 
	// - �ֻ����� ���� ��
	// DXGI_RATIONAL ����ü�� ������(�м�) ���� ǥ���ϱ� ���� ����ü
	// RefreshRate = Numerator / Denominator
	// �ֻ���(Refresh Rate, Hz ����)�� �����մϴ�
	// �� ���� �� ���� 60 / 1 = 60 �� 60Hz�� �ǹ��մϴ�
	// ���� �ڵ忡�� 60���� �δ� ������ �������� 60Hz ����� ȯ�濡 ���߱� ���ؼ� �Դϴ�
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;

	// DXGI_SAMPLE_DESC?
	// - ��Ƽ���ø�(Multi-Sampling Anti-Aliasing, MSAA) ������ ���� ��
	// �� �ȼ��� �� �� ���ø��� ������ �����մϴ�
	// ��, MSAA�� ���� ������ ���մϴ�
	// Count�� 1�̸� ��Ƽ���ø��� ������� ������ �ǹ��մϴ�
	// �������� ���õǾ� ������ �� Ȯ���غ� ��!
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	// ����� â�� ���� �����Դϴ�.
	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	/*
	* @brief  Direct3D ����̽�, ����̽� ���ؽ�Ʈ, ����ü�� ����
	* @details
	*   - Adapter        : NULL �� �⺻ GPU ���
	*   - DriverType     : D3D_DRIVER_TYPE_HARDWARE �� �ϵ���� ����
	*   - Flags          : creationFlags (����� ��� ���� ����)
	*   - SwapChainDesc  : �����, �ֻ��� �� ����ü�� ����
	*   - ��ȯ           : m_pDevice, m_pDeviceContext, m_pSwapChain
	*/
	HR_T(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_pSwapChain, &m_pDevice, NULL, &m_pDeviceContext));

	/*
	* @brief  ����ü�� ����۷� RTV�� ����� OM ���������� ���ε��Ѵ�
	* @details
	*   - GetBuffer(0): �����(ID3D11Texture2D)�� ȹ��
	*   - CreateRenderTargetView: ����� ��� RTV ����(���ҽ� ���� ���� ����)
	*   - ���� �ؽ�ó �����ʹ� Release�� ���� (RTV�� ���� ����)
	*   - OMSetRenderTargets: ������ RTV�� ���� Ÿ���� ���� ��� ���������ο� ���ε�
	*/
	ID3D11Texture2D* pBackBufferTexture = nullptr;
	HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));
	HR_T(m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_pRenderTargetView));
	SAFE_RELEASE(pBackBufferTexture);

	// ���� ���ٽ� �ؽ�ó/�� ����
	D3D11_TEXTURE2D_DESC dsDesc = {};
	dsDesc.Width = m_ClientWidth;
	dsDesc.Height = m_ClientHeight;
	dsDesc.MipLevels = 1;
	dsDesc.ArraySize = 1;
	dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsDesc.SampleDesc.Count = swapDesc.SampleDesc.Count;
	dsDesc.SampleDesc.Quality = swapDesc.SampleDesc.Quality;
	dsDesc.Usage = D3D11_USAGE_DEFAULT;
	dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	dsDesc.CPUAccessFlags = 0;
	dsDesc.MiscFlags = 0;

	ID3D11Texture2D* pDepthStencil = nullptr;
	HR_T(m_pDevice->CreateTexture2D(&dsDesc, nullptr, &pDepthStencil));
	HR_T(m_pDevice->CreateDepthStencilView(pDepthStencil, nullptr, &m_pDepthStencilView));
	SAFE_RELEASE(pDepthStencil);

	// DepthStencilState ���� �� ����
	D3D11_DEPTH_STENCIL_DESC dssDesc = {};
	dssDesc.DepthEnable = TRUE;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS;
	dssDesc.StencilEnable = FALSE;
	HR_T(m_pDevice->CreateDepthStencilState(&dssDesc, &m_pDepthStencilState));
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStencilState, 0);

	// ���� Ÿ��/DSV ���ε�
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

	/*
	* @brief  ����Ʈ(Viewport) ����
	* @details
	*   - TopLeftX/Y : ��� ������ ���� ��ǥ (0,0 �� �»��)
	*   - Width/Height : ������ Ŭ���̾�Ʈ ũ�� ����
	*   - MinDepth/MaxDepth : ���� ���� (���� 0.0 ~ 1.0)
	*   - RSSetViewports : �����Ͷ����� ���������� ����Ʈ ���ε�
	*/
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)m_ClientWidth;
	viewport.Height = (float)m_ClientHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_pDeviceContext->RSSetViewports(1, &viewport);

	// �ø� ���� (��� ������/�ĸ� �ø�)
	CD3D11_RASTERIZER_DESC rasterizerDesc(CD3D11_DEFAULT{});
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.FrontCounterClockwise = false;
	HR_T(m_pDevice->CreateRasterizerState(&rasterizerDesc, &RSNoCull));
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FrontCounterClockwise = true;
	HR_T(m_pDevice->CreateRasterizerState(&rasterizerDesc, &RSCullClockWise));

	return true;
}

void App::UninitD3D()
{
	SAFE_RELEASE(m_pDepthStencilState);
	SAFE_RELEASE(m_pDepthStencilView);
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDevice);
}

/*
 * @brief InitScene() ��ü �帧
 *   1. ����(Vertex) �迭�� GPU ���۷� ����
 *   2. VS �Է� �ñ״�ó�� ���� InputLayout ����
 *   3. VS ����Ʈ�ڵ�� Vertex Shader ���� �� ���� ����
 *   4. �ε��� ����(Index Buffer) ����
 *   5. PS ����Ʈ�ڵ�� Pixel Shader ���� �� ���� ����
 */
bool App::InitScene()
{
	//HRESULT hr = 0;
	ID3D10Blob* errorMessage = nullptr;	 // ���� �޽����� ������ ����.

	// ***********************************************************************************************
	// ť�꼳��
	// 24�� ���� (�� �� 4��) + �ؽ�ó ��ǥ
	XMFLOAT4 hardColor = XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f);
	m_VertextBufferStride = sizeof(VertexData);
	m_VertextBufferOffset = 0;
	// ***********************************************************************************************
	// ���� ť�� ������ ����
	// ���� �ִ� �Ͱ� �ε��� ���� ���� �ִ°��� CreateBox �Լ� �ȿ� �ֽ��ϴ�
	StaticMeshData cubeData = StaticMesh::CreateBox(XMFLOAT4(1, 1, 1, 1));

	D3D11_BUFFER_DESC vbDesc = {};
	ZeroMemory(&vbDesc, sizeof(vbDesc));			// vbDesc�� 0���� ��ü �޸� ������ �ʱ�ȭ ��ŵ�ϴ�
	vbDesc.ByteWidth = sizeof(VertexData) * cubeData.vertices.size();				// �迭 ��ü�� ����Ʈ ũ�⸦ �ٷ� ��ȯ�մϴ�
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA vbData = {};
	ZeroMemory(&vbData, sizeof(vbData));
	vbData.pSysMem = cubeData.vertices.data();						// �迭 ������ �Ҵ�.
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));

	D3D11_BUFFER_DESC ibDesc = {};
	ZeroMemory(&ibDesc, sizeof(ibDesc));
	m_nIndices = cubeData.indices.size();	// �ε��� ���� ����.
	ibDesc.ByteWidth = sizeof(DWORD) * cubeData.indices.size();
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = cubeData.indices.data();
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));

	// ***********************************************************************************************
	// ��� ���� ����
	//
	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(ConstantBuffer);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	// ���� ��� ���� ���� (VS/PS ����, b0)
	HR_T(m_pDevice->CreateBuffer(&cbd, nullptr, &m_pConstantBuffer));

		// ***********************************************************************************************
	// ��ī�� �ڽ� ť�� ����
    HR_T(CreateDDSTextureFromFile(m_pDevice, L"..\\Resource\\Hanako.dds", nullptr, &m_pSkyHanakoSRV));
    HR_T(CreateDDSTextureFromFile(m_pDevice, L"..\\Resource\\cubemap.dds", nullptr, &m_pSkyCubeMapSRV));
	m_SkyBoxChoice = SkyBoxChoice::CubeMap;
	m_pTextureSRV = m_pSkyCubeMapSRV;
	
	// ���÷� ����
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HR_T(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState));

	// ***********************************************************************************************
	// ī�޶� ����
	// ���� ī�޶�(View/Proj)�� 3���� ��� ���� ��Ʈ���� �غ��մϴ�
	m_baseProjection.world = XMMatrixIdentity();
	m_baseProjection.view = XMMatrixTranspose(XMMatrixLookAtLH(
		XMVectorSet(m_cameraPos.x, m_cameraPos.y, m_cameraPos.z, 0.0f),
		XMVectorSet(m_cameraPos.x + 0.0f, m_cameraPos.y + 0.0f, m_cameraPos.z + 1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	));

	float fovRad = XMConvertToRadians(m_CameraFovDeg);
	m_baseProjection.proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovRad, AspectRatio(), m_CameraNear, m_CameraFar));
	m_baseProjection.worldInvTranspose = XMMatrixInverse(nullptr, XMMatrixTranspose(m_baseProjection.world));
	m_baseProjection.dirLight = DirectionalLight(DirectX::XMFLOAT4(1,1,1,1), DirectX::XMFLOAT3(0,-1,1));
	m_baseProjection.eyePos = m_cameraPos;
	m_baseProjection.pad = 0.0f;

	return true;
}

void App::UninitScene()
{
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	SAFE_RELEASE(m_pConstantBuffer);

	SAFE_RELEASE(m_pSkyBoxInputLayout);
	SAFE_RELEASE(m_pSkyBoxVertexShader);
	SAFE_RELEASE(m_pSkyBoxPixelShader);

	SAFE_RELEASE(m_pSkyHanakoSRV);
	SAFE_RELEASE(m_pSkyCubeMapSRV);

    for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_pSkyFaceSRV[i]);
}

bool App::InitBasicEffect()
{
	// Vertex Shader -------------------------------------
	/*
	* @brief  VS �Է� �ñ״�ó�� ���� InputLayout ����
	* @details
	*   - POSITION: float3, COLOR: float4 (����ü/���̴��� ���ġ������� ��ġ �ʼ�)
	*   - InputSlot=0, Per-Vertex ������
	*   - D3D11_APPEND_ALIGNED_ELEMENT�� COLOR ������ �ڵ� ���
	*   - VS ����Ʈ�ڵ�(CompileShaderFromFile)�� �ñ״�ó ��Ī �� CreateInputLayout
	*/
	D3D11_INPUT_ELEMENT_DESC layout[] = // �Է� ���̾ƿ�.
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"10_BasicVS.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));

	/*
	* @brief  VS ����Ʈ�ڵ�� Vertex Shader ���� �� ������ ���� ����
	* @details
	*   - CreateVertexShader: �����ϵ� ����Ʈ�ڵ�(pointer/size)�� VS ��ü ����
	*   - ClassLinkage �̻��(NULL)
	*   - ���� ��, �� �̻� �ʿ� ���� vertexShaderBuffer�� ����
	*/
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// ������ ���� ����


	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"10_BasicPS.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// �ȼ� ���̴� ���� ���̻� �ʿ����
	return true;
}

bool App::InitSkyBoxEffect()
{
	D3D11_INPUT_ELEMENT_DESC layout[] = // �Է� ���̾ƿ�.
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"10_SkyBoxVS.hlsl", "VS", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pSkyBoxInputLayout));

	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pSkyBoxVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// ������ ���� ����

	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"10_SkyboxPS.hlsl", "PS", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pSkyBoxPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// �ȼ� ���̴� ���� ���̻� �ʿ����
	return true;
}
