/*
* @brief : d3d11로 큐브를 비추는 라이팅 예제입니다
* @details :
* 
*     1) Normal이 포함된 정점(Vertex)으로 큐브를 구성하고,
*     2) VertexShader에서 Local Normal을 World Normal로 변환,
*     3) PixelShader에서 Directional Light와 Normal로 디퓨즈 음영 처리,
*     4) 상수버퍼를 하나(b0)만 사용하여 VS/PS 공용 데이터 관리,
*     5) ImGui로 Light(Color/Direction/Position)과 큐브 회전(Yaw/Pitch) 제어,
*     6) 디버그용 라이트 위치 마커(작은 흰색 큐브) 출력.
*
*   - 파이프라인 구성
*     IA: POSITION(float3), NORMAL(float3), COLOR(float4) 입력 레이아웃
*     VS: posW, normalW 계산 (g_World, g_WorldInvTranspose 사용), 클립공간 변환
*     PS: baseColor * (ambient + diffuse)로 단순 조명, g_Pad 토글로 디버그 컬러
*     CB: ConstantBuffer(b0) 단일. world/view/proj/worldInvTranspose/dirLight/eyePos/pad 포함
*
*   - 상수버퍼
*     world/view/proj: 프레임별 업데이트
*     worldInvTranspose: XMMatrixTranspose(XMMatrixInverse(...))로 계산
*     dirLight: ImGui 입력 반영 (color, direction 정규화)
*     eyePos: 카메라 위치
*     pad: 디버그 토글 (1.0=보라색, 2.0=흰색 마커)
*
*   - ImGui
*     Mesh: Root Pos, Yaw(deg), Pitch(deg)
*     Camera: Position, FOV, Near/Far
*     Light: Color(RGB), Direction(x,y,z), Position(x,y,z)
*
*   - 디버그 마커
*     라이트 위치를 표시하는 작은 큐브(스케일 0.2). 상수버퍼 pad=2.0으로 PS에서 흰색 큐브 강제 출력
*
*   - 주의사항
*     1) 인덱스 버퍼 포맷과 DXGI_FORMAT(R32_UINT/R16_UINT) 일치
*     2) 입력 레이아웃과 정점 구조체, VS 입력 시그니처 정합성 유지
*     3) 행렬 전치 규약(HLSL 열-주도) 일치: CPU에서 Transpose 포함 업데이트
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

static bool g_RotateCube = true; // ImGui 토글: 큐브 자동 회전 on/off

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
	// 다른 스카이박스로 바꿀 수도 있으니 해제하고 다시 로드
	for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_pSkyFaceSRV[i]);
    m_SkyFaceSize = ImVec2(0, 0);
    if (!m_pTextureSRV) return;

	Microsoft::WRL::ComPtr<ID3D11Resource> res;
    m_pTextureSRV->GetResource(res.GetAddressOf());
    if (!res) return;

	// 파괴됐는지 안됐는지 판단을 위해 Comptr이 필요하다
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
	HR_T(res.As(&tex2D));

    D3D11_TEXTURE2D_DESC desc{};
    tex2D->GetDesc(&desc);
    // 큐브맵은 6개의 array slice를 가짐. (여러 큐브면 6의 배수)
    if ((desc.ArraySize < 6)) return;

    // 크기 기록 (mip0 기준)
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

	// ImGui 초기화
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);

    // Hanako.png는 디버그용이었으나, 이제 스카이박스 현재 면을 그릴 예정
    // 초기 스카이박스 면 SRV들을 준비한다
    PrepareSkyFaceSRVs();

	// 시스템 정보 수집 (GPU)
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
						m_VideoMemoryTotal = memInfo.Budget; // 사용 가능 예산을 총량 근사로 사용
					}
				}
			}
		}
	}

	// 시스템 정보 수집 (CPU)
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

	// 시스템 메모리 총량/가용량 초기화
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
	// ImGui 종료
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	UninitD3D();
}

void App::OnUpdate(const float& dt)
{
	// 로컬 변환 정의 (간단 Scene Graph)
	if(g_RotateCube)
		m_YawDeg += 45.0f * dt; // Yaw 회전 (UI로 조절 가능)
	m_YawDeg = std::fmod(m_YawDeg + 180.0f, 360.0f) - 180.0f;
	XMMATRIX rotYaw   = XMMatrixRotationY(XMConvertToRadians(m_YawDeg));
	XMMATRIX rotPitch = XMMatrixRotationX(XMConvertToRadians(m_PitchDeg));
		XMMATRIX local0 = rotPitch * rotYaw * XMMatrixTranslation(m_cubePos.x, m_cubePos.y, m_cubePos.z); // 루트
	XMMATRIX world0 = local0; // 루트

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

	// View/Proj도 UI값 반영 (매 프레임)
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

	// FPS 1초 업데이트
	m_FpsTimer += dt;
	if (m_FpsTimer >= 1.0f)
	{
		m_LastFps = 1.0f / dt; // 간단히 최근 프레임의 역수 사용
		m_FpsTimer = 0.0f;
	}

	// 시스템 메모리 가용량 갱신
	{
		MEMORYSTATUSEX ms{ sizeof(MEMORYSTATUSEX) };
		if (GlobalMemoryStatusEx(&ms))
		{
			m_RamTotal = ms.ullTotalPhys;
			m_RamAvail = ms.ullAvailPhys;
		}
	}

	// VRAM 사용량 갱신 (가능한 경우)
	if (m_Adapter3)
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
		if (SUCCEEDED(m_Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)))
		{
			// Budget은 사용 가능 한도, CurrentUsage는 현재 사용량
			m_VideoMemoryTotal = memInfo.Budget;
			// 사용량은 memInfo.CurrentUsage를 직접 사용, 표시 시 변환
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

// Render() 함수에 중요한 부분이 다 들어있습니다. 여기를 보면 됩니다
void App::OnRender()
{
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	UINT stride = m_VertextBufferStride;	// 바이트 수
	UINT offset = m_VertextBufferOffset;

	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 1 ~ 3 . IA 단계 설정
	// 정점을 어떻게 이어서 그릴 것인지를 선택하는 부분
	// 1. 버퍼를 잡아주기
	// 2. 입력 레이아웃을 잡아주기
	// 3. 인덱스 버퍼를 잡아주기
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// 4. Vertex Shader 설정
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	// 5. Pixel Shader 설정
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	// 6. Constant Buffer 설정 (단일 GlobalCB, b0)
	// 프레임별 상수값 준비
	m_ConstantBuffer.world = m_baseProjection.world;
	m_ConstantBuffer.view  = m_baseProjection.view;
	m_ConstantBuffer.proj  = m_baseProjection.proj;
	{
		// worldInvTranspose 계산 (CPU에서는 실제 world로 역/전치 후, HLSL 프리트랜스포즈 규약에 맞게 저장)
		// 비균등 스케일을 해결한 코드
		auto invWorlNormal = XMMatrixInverse(nullptr, m_baseProjection.world);
		m_ConstantBuffer.worldInvTranspose = XMMatrixTranspose(invWorlNormal);
	}
	// 간단 기본 광원/카메라 (UI 반영)
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

	// 7. 그리기
	m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);

	// 라이트 위치 마커 큐브 그리기 (작은 스케일, 흰색)
	{
		ConstantBuffer marker = m_ConstantBuffer;
		XMFLOAT3 dir = m_LightDirection;
		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&dir));
		XMMATRIX S = XMMatrixScaling(0.2f, 0.2f, 0.2f);
		XMMATRIX T = XMMatrixTranslation(m_LightPosition.x, m_LightPosition.y, m_LightPosition.z);
		marker.world = XMMatrixTranspose(S * T);
		// 월드 인버스 전치 갱신
		marker.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(S * T)));
		// 흰색 마커 강제
		marker.dirLight.color = XMFLOAT4(1,1,1,1);
		marker.pad = 2.0f; // PS에서 흰색 출력 토글

		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &marker, sizeof(ConstantBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);
	}

	// 라이트 방향 표시 라인 그리기(빨간색)
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
		// 뷰/투영만 적용된 월드(단위행렬) 사용, pad=3.0 -> 빨간색 라인 출력
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


	// SkyBox 렌더링 (상태 보존/복구)
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
			rd.CullMode = D3D11_CULL_NONE;         // 내부면만 보이게 끄기
			rd.FrontCounterClockwise = false;        // CCW를 Front로 간주
			rd.DepthClipEnable = TRUE;
			HR_T(m_pDevice->CreateRasterizerState(&rd, &rsSky));
		}
		m_pDeviceContext->RSSetState(rsSky);

		// 스카이박스 WVP: world=I, view=translation 제거, proj=일반(Projt)
		XMMATRIX viewT = m_baseProjection.view; // transposed view
		XMMATRIX view  = XMMatrixTranspose(viewT);
		view.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, XMVectorGetW(view.r[3]));
		XMMATRIX viewNoTransT = XMMatrixTranspose(view);
		XMMATRIX projT = m_baseProjection.proj;
		XMMATRIX wvpT = XMMatrixMultiply(projT, viewNoTransT);
		// 단일 CB(b0) 사용: SkyBox VS가 사용하는 g_WorldViewProj 위치(선두 64바이트)에만 wvpT를 써준다
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

		// 상태 복구
		m_pDeviceContext->OMSetDepthStencilState(prevDS, prevRef);
		m_pDeviceContext->RSSetState(prevRS);
		SAFE_RELEASE(prevDS);
		SAFE_RELEASE(prevRS);
	}

	// ImGui 프레임 및 UI 렌더링
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (ImGui::Begin("Controls"))
	{
		// SkyBox 선택
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

	// 현재 카메라 포워드 기준 스카이박스 면 이미지를 표시
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


	// 우상단: 시스템 정보(FPS/GPU/CPU)
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
			// VRAM (Budget/Usage가 제공되는 경우)
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

	// 스왑체인의 값들을 설정할 구조체를 만듭니다
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;
	swapDesc.Windowed = true;
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;

	// DXGI_RATIONAL? 
	// - 주사율을 위한 것
	// DXGI_RATIONAL 구조체는 유리수(분수) 값을 표현하기 위한 구조체
	// RefreshRate = Numerator / Denominator
	// 주사율(Refresh Rate, Hz 단위)을 지정합니다
	// 즉 밑의 두 줄은 60 / 1 = 60 → 60Hz를 의미합니다
	// 샘플 코드에서 60으로 두는 이유는 보편적인 60Hz 모니터 환경에 맞추기 위해서 입니다
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;

	// DXGI_SAMPLE_DESC?
	// - 멀티샘플링(Multi-Sampling Anti-Aliasing, MSAA) 설정을 위한 것
	// 한 픽셀을 몇 번 샘플링할 것인지 지정합니다
	// 즉, MSAA의 샘플 개수를 뜻합니다
	// Count가 1이면 멀티샘플링을 사용하지 않음을 의미합니다
	// 계단현상과 관련되어 있으니 잘 확인해볼 것!
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	// 디버그 창을 띄우기 위함입니다.
	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	/*
	* @brief  Direct3D 디바이스, 디바이스 컨텍스트, 스왑체인 생성
	* @details
	*   - Adapter        : NULL → 기본 GPU 사용
	*   - DriverType     : D3D_DRIVER_TYPE_HARDWARE → 하드웨어 가속
	*   - Flags          : creationFlags (디버그 모드 여부 포함)
	*   - SwapChainDesc  : 백버퍼, 주사율 등 스왑체인 설정
	*   - 반환           : m_pDevice, m_pDeviceContext, m_pSwapChain
	*/
	HR_T(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_pSwapChain, &m_pDevice, NULL, &m_pDeviceContext));

	/*
	* @brief  스왑체인 백버퍼로 RTV를 만들고 OM 스테이지에 바인딩한다
	* @details
	*   - GetBuffer(0): 백버퍼(ID3D11Texture2D)를 획득
	*   - CreateRenderTargetView: 백버퍼 기반 RTV 생성(리소스 내부 참조 증가)
	*   - 로컬 텍스처 포인터는 Release로 정리 (RTV가 수명 관리)
	*   - OMSetRenderTargets: 생성한 RTV를 렌더 타겟을 최종 출력 파이프라인에 바인딩
	*/
	ID3D11Texture2D* pBackBufferTexture = nullptr;
	HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));
	HR_T(m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_pRenderTargetView));
	SAFE_RELEASE(pBackBufferTexture);

	// 깊이 스텐실 텍스처/뷰 생성
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

	// DepthStencilState 생성 및 설정
	D3D11_DEPTH_STENCIL_DESC dssDesc = {};
	dssDesc.DepthEnable = TRUE;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS;
	dssDesc.StencilEnable = FALSE;
	HR_T(m_pDevice->CreateDepthStencilState(&dssDesc, &m_pDepthStencilState));
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStencilState, 0);

	// 렌더 타겟/DSV 바인딩
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

	/*
	* @brief  뷰포트(Viewport) 설정
	* @details
	*   - TopLeftX/Y : 출력 영역의 시작 좌표 (0,0 → 좌상단)
	*   - Width/Height : 윈도우 클라이언트 크기 기준
	*   - MinDepth/MaxDepth : 깊이 범위 (보통 0.0 ~ 1.0)
	*   - RSSetViewports : 래스터라이저 스테이지에 뷰포트 바인딩
	*/
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)m_ClientWidth;
	viewport.Height = (float)m_ClientHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_pDeviceContext->RSSetViewports(1, &viewport);

	// 컬링 설정 (양면 렌더링/후면 컬링)
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
 * @brief InitScene() 전체 흐름
 *   1. 정점(Vertex) 배열을 GPU 버퍼로 생성
 *   2. VS 입력 시그니처에 맞춰 InputLayout 생성
 *   3. VS 바이트코드로 Vertex Shader 생성 및 버퍼 해제
 *   4. 인덱스 버퍼(Index Buffer) 생성
 *   5. PS 바이트코드로 Pixel Shader 생성 및 버퍼 해제
 */
bool App::InitScene()
{
	//HRESULT hr = 0;
	ID3D10Blob* errorMessage = nullptr;	 // 에러 메시지를 저장할 버퍼.

	// ***********************************************************************************************
	// 큐브설정
	// 24개 정점 (각 면 4개) + 텍스처 좌표
	XMFLOAT4 hardColor = XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f);
	m_VertextBufferStride = sizeof(VertexData);
	m_VertextBufferOffset = 0;
	// ***********************************************************************************************
	// 작은 큐브 데이터 설정
	// 정점 넣는 것과 인덱스 버퍼 값을 넣는것은 CreateBox 함수 안에 있습니다
	StaticMeshData cubeData = StaticMesh::CreateBox(XMFLOAT4(1, 1, 1, 1));

	D3D11_BUFFER_DESC vbDesc = {};
	ZeroMemory(&vbDesc, sizeof(vbDesc));			// vbDesc에 0으로 전체 메모리 영역을 초기화 시킵니다
	vbDesc.ByteWidth = sizeof(VertexData) * cubeData.vertices.size();				// 배열 전체의 바이트 크기를 바로 반환합니다
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA vbData = {};
	ZeroMemory(&vbData, sizeof(vbData));
	vbData.pSysMem = cubeData.vertices.data();						// 배열 데이터 할당.
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));

	D3D11_BUFFER_DESC ibDesc = {};
	ZeroMemory(&ibDesc, sizeof(ibDesc));
	m_nIndices = cubeData.indices.size();	// 인덱스 개수 저장.
	ibDesc.ByteWidth = sizeof(DWORD) * cubeData.indices.size();
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = cubeData.indices.data();
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));

	// ***********************************************************************************************
	// 상수 버퍼 설정
	//
	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(ConstantBuffer);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	// 단일 상수 버퍼 생성 (VS/PS 공용, b0)
	HR_T(m_pDevice->CreateBuffer(&cbd, nullptr, &m_pConstantBuffer));

		// ***********************************************************************************************
	// 스카이 박스 큐브 설정
    HR_T(CreateDDSTextureFromFile(m_pDevice, L"..\\Resource\\Hanako.dds", nullptr, &m_pSkyHanakoSRV));
    HR_T(CreateDDSTextureFromFile(m_pDevice, L"..\\Resource\\cubemap.dds", nullptr, &m_pSkyCubeMapSRV));
	m_SkyBoxChoice = SkyBoxChoice::CubeMap;
	m_pTextureSRV = m_pSkyCubeMapSRV;
	
	// 샘플러 생성
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HR_T(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState));

	// ***********************************************************************************************
	// 카메라 설정
	// 공통 카메라(View/Proj)로 3개의 상수 버퍼 엔트리를 준비합니다
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
	* @brief  VS 입력 시그니처에 맞춰 InputLayout 생성
	* @details
	*   - POSITION: float3, COLOR: float4 (구조체/셰이더와 형식·오프셋 일치 필수)
	*   - InputSlot=0, Per-Vertex 데이터
	*   - D3D11_APPEND_ALIGNED_ELEMENT로 COLOR 오프셋 자동 계산
	*   - VS 바이트코드(CompileShaderFromFile)로 시그니처 매칭 후 CreateInputLayout
	*/
	D3D11_INPUT_ELEMENT_DESC layout[] = // 입력 레이아웃.
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
	* @brief  VS 바이트코드로 Vertex Shader 생성 및 컴파일 버퍼 해제
	* @details
	*   - CreateVertexShader: 컴파일된 바이트코드(pointer/size)로 VS 객체 생성
	*   - ClassLinkage 미사용(NULL)
	*   - 생성 후, 더 이상 필요 없는 vertexShaderBuffer는 해제
	*/
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// 컴파일 버퍼 해제


	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"10_BasicPS.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음
	return true;
}

bool App::InitSkyBoxEffect()
{
	D3D11_INPUT_ELEMENT_DESC layout[] = // 입력 레이아웃.
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"10_SkyBoxVS.hlsl", "VS", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pSkyBoxInputLayout));

	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pSkyBoxVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// 컴파일 버퍼 해제

	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"10_SkyboxPS.hlsl", "PS", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pSkyBoxPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음
	return true;
}
