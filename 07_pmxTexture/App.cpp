/*
* @brief : PMX 텍스쳐 렌더 예제 (머티리얼 DIFFUSE 적용)
* @details :
*   - 무엇을 하는가
*     · PMX를 읽어서, 머티리얼의 DIFFUSE 텍스쳐를 찾아서, 픽셀 셰이더에서 샘플링해 그린다
*   - 어떻게 처리하는가
*     · 경로: PMX 파일 위치를 기준으로 상대경로를 풀고, 실패하면 같은 폴더의 "Alice.fbm/" 폴더에서 다시 찾는다
*     · 임베디드: "*0" 같은 형식이면 aiTexture에서 직접 읽어 메모리로 텍스쳐를 만든다
*     · 서브셋: 머티리얼 인덱스별로 인덱스 범위를 저장해, 렌더 때 해당 텍스쳐를 바인딩하고 부분 그리기를 한다
*     · 셰이더: VS는 TEXCOORD를 넘기고, PS는 tex0.Sample(samp0, uv)로 실제 텍스쳐를 샘플한다
*   - 기본 세팅
*     · Cull None, Depth Enable(LESS), 선형 필터 샘플러, 텍스쳐 없을 땐 1x1 흰색 폴백
*/

#include "App.h"
#include "../Common/Helper.h"
#include "../Common/InputSystem.h"
#include <d3dcompiler.h>
#include <directxtk/WICTextureLoader.h>
#include <thread>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <unordered_set>
#include <filesystem>

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

/*
* @brief : 앱 초기화(렌더링 환경 + 효과 + 장면)
* @details :
* 	- InitD3D  : 디바이스/컨텍스트/스왑체인/RTV/DSV/래스터 상태
* 	- InitEffect: VS/PS 컴파일 및 입력 레이아웃 생성
* 	- InitScene : PMX 로드 → 병합 → VB/IB/CB 생성
*/
bool App::OnInitialize()
{
	if(!InitD3D()) return false;

	if (!InitEffect()) return false;

	if(!InitScene()) return false;

	// ImGui 초기화
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);

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

/*
* @brief : 프레임 갱신(카메라/월드 행렬, 통계)
* @details :
* 	- world : 루트 위치 평행이동 + Yaw 회전
* 	- view/proj : UI 카메라 파라미터 반영
* 	- FPS : 1초 간격 갱신
*/
void App::OnUpdate(const float& dt)
{
	static float t0 = 0.0f, t1 = 0.0f, t2 = 0.0f;
	t0 += 0.6f * dt;   // 부모(루트) Yaw 속도
	t1 += 1.0f * dt;   // 두번째 메쉬(자식1) Yaw 속도 (루트와 다르게)
	t2 += 1.2f * dt;   // 세번째 메쉬(자식2) 공전 속도

	// 로컬 변환 정의 (간단 Scene Graph)
	XMMATRIX local0 = XMMatrixRotationY(t0) * XMMatrixTranslation(m_RootPos.x, m_RootPos.y, m_RootPos.z); // 루트
	// 부모-자식 계층 world 계산: world = local * parentWorld
	XMMATRIX world0 = local0;                 // 루트

	static float sYaw = 0.0f, sPitch = 0.0f;
	ImGuiIO& io = ImGui::GetIO();
	bool imguiWantsMouse = io.WantCaptureMouse;
	float d1 = 0.0f, d2 = 0.0f, d3 = 0.0f;
	if (ImGui::IsKeyDown(ImGuiKey_W)) d1 += dt;
	if (ImGui::IsKeyDown(ImGuiKey_S)) d1 -= dt;
	if (ImGui::IsKeyDown(ImGuiKey_A)) d2 -= dt;
	if (ImGui::IsKeyDown(ImGuiKey_D)) d2 += dt;
	if (ImGui::IsKeyDown(ImGuiKey_E)) d3 += dt;
	if (ImGui::IsKeyDown(ImGuiKey_Q)) d3 -= dt;
	
	/* wheel handled below along view dir */
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
	{
		float rotSpeed = 0.005f;
		sYaw   += io.MouseDelta.x * rotSpeed;
		sPitch += -io.MouseDelta.y * rotSpeed;
		float limit = XMConvertToRadians(89.0f);
		sPitch = std::min(max(sPitch, -limit), limit);
	}
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR forward = XMVector3Normalize(XMVectorSet(cosf(sPitch)*sinf(sYaw), sinf(sPitch), cosf(sPitch)*cosf(sYaw), 0.0f));
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
	XMVECTOR posV = XMVectorSet(m_CameraPos.x, m_CameraPos.y, m_CameraPos.z, 0.0f);
	float moveSpeed = 5.0f;
	posV = XMVectorAdd(posV, XMVectorScale(forward, d1 * moveSpeed));
	posV = XMVectorAdd(posV, XMVectorScale(right, d2 * moveSpeed));
	posV = XMVectorAdd(posV, XMVectorScale(up, d3 * moveSpeed));
	// mouse wheel dolly along view direction (use DirectXTK mouse wheel)
	if (!imguiWantsMouse)
	{
		int wheel = InputSystem::Instance->m_MouseState.scrollWheelValue;
		static int lastWheel = wheel;
		int delta = wheel - lastWheel;
		lastWheel = wheel;
		if (delta != 0)
		{
			float steps = (float)delta / WHEEL_DELTA; // 보통 120 단위
			posV = XMVectorAdd(posV, XMVectorScale(forward, steps * 0.5f));
		}
	}
	XMStoreFloat3(&m_CameraPos, posV);
	
	// View/Proj도 UI값 반영 (매 프레임)
	XMMATRIX view = XMMatrixTranspose(XMMatrixLookAtLH(
		posV,
		XMVectorAdd(posV, forward),
		up));
	float fovRad = XMConvertToRadians(m_CameraFovDeg);
	XMMATRIX proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovRad, AspectRatio(), m_CameraNear, m_CameraFar));

	m_CBuffer.world = XMMatrixTranspose(world0);
	m_CBuffer.view = view;
	m_CBuffer.proj = proj;

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

// Render() 함수에 중요한 부분이 다 들어있습니다. 여기를 보면 됩니다

/*
* @brief : 프레임 렌더(단일 DrawIndexed)
* @details :
* 	- OM/IA : 클리어, 토폴로지, VB/IB, 입력 레이아웃
* 	- VS/PS : 셰이더 바인딩, 상수 버퍼 업데이트
* 	- Draw  : 단일 인덱스 드로우
*/
void App::OnRender()
{
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	UINT stride = sizeof(VertexPosTex);	// 바이트 수
	UINT offset = 0;

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

	// 6. Constant Buffer 설정
	// 7. 그리기
	{
		D3D11_MAPPED_SUBRESOURCE mapped{};
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0,
			D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy(mapped.pData, &m_CBuffer, sizeof(m_CBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);

		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
		if (!m_Subsets.empty())
		{
			/*
			* @brief : 렌더 서브셋 드로우
			* @details :
			*   - 입력: m_Subsets(인덱스 범위/머티리얼 인덱스), m_MaterialSRVs
			*   - 출력: 없음(그리기 호출)
			*   - 알고리즘: 서브셋 순회 → PS SRV 바인딩 → 해당 인덱스 범위 DrawIndexed
			*/
			// 서브셋별로 해당 머티리얼의 SRV를 바꿔 끼우고, 그 인덱스 범위만 그리기
			for (const auto& s : m_Subsets)
			{
				ID3D11ShaderResourceView* srv = nullptr;
				if (s.materialIndex < m_MaterialSRVs.size()) srv = m_MaterialSRVs[s.materialIndex];
				m_pDeviceContext->PSSetShaderResources(0, 1, &srv);
				m_pDeviceContext->DrawIndexed(s.indexCount, s.startIndex, 0);
			}
		}
		else
		{
			// 서브셋 정보가 없으면 전체 인덱스 범위를 한 번에 그린다
			m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);
		}
	}

	// ImGui 프레임 및 UI 렌더링
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (ImGui::Begin("Controls"))
	{
		ImGui::Text("Mesh Transforms");
		ImGui::DragFloat3("Root Pos (x,y,z)", &m_RootPos.x, 0.1f);
		ImGui::Separator();
		ImGui::Text("Camera");
		ImGui::DragFloat3("Camera Pos (x,y,z)", &m_CameraPos.x, 0.1f);
		ImGui::SliderFloat("Camera FOV (deg)", &m_CameraFovDeg, 30.0f, 120.0f);
		ImGui::DragFloatRange2("Near/Far", &m_CameraNear, &m_CameraFar, 0.1f, 0.01f, 5000.0f, "Near: %.2f", "Far: %.2f");
	}
	ImGui::End();

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

	// 우하단: 모델 정보
	{
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 size(460.0f, 220.0f);
		ImVec2 pos(io.DisplaySize.x - size.x - 10.0f, io.DisplaySize.y - size.y - 10.0f);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(size, ImGuiCond_Always);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
		if (ImGui::Begin("Model Info", nullptr, flags))
		{
			// 숫자 포맷 함수(3자리 콤마)
			auto fmt = [](uint64_t v) {
				wchar_t buf[64];
				_locale_t loc = _create_locale(LC_ALL, "C");
				_swprintf_s_l(buf, 64, L"%llu", loc, (unsigned long long)v);
				std::wstring s(buf);
				for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert(i, L",");
				return s;
			};
			// 정점/인덱스/삼각형
			ImGui::Text("Polygon");
			ImGui::Text("Vertices  : %ls", fmt(m_ModelVertices.size()).c_str());
			ImGui::Text("Indices   : %ls", fmt(m_ModelIndices.size()).c_str());
			ImGui::Text("Triangles : %ls", fmt(m_ModelIndices.size() / 3).c_str());
			ImGui::Separator();
			// 서브셋/머티리얼
			ImGui::Text("Subsets/Materials");
			ImGui::Text("Subsets   : %ls", fmt(m_Subsets.size()).c_str());
			ImGui::Text("Materials : %ls", fmt(m_MaterialSRVs.size()).c_str());
			// 고유 텍스처 수(화이트 폴백 제외)를 대략 추정: 포인터 기준 중복 제거
			std::unordered_set<ID3D11ShaderResourceView*> uniq;
			for (auto* p : m_MaterialSRVs) if (p) uniq.insert(p);
			unsigned uniqueTex = (unsigned)uniq.size();
			unsigned fallbackTex = 0;
			for (auto* p : m_MaterialSRVs) if (p == m_pWhiteSRV) ++fallbackTex;
			ImGui::Text("Textures Path : %ls unique (fallback %ls)", fmt(uniqueTex).c_str(), fmt(fallbackTex).c_str());
			ImGui::Separator();
			// 경로
			ImGui::TextWrapped("%ls", m_ModelPath.c_str());
			// 텍스처 폴더(폴더명만)
			{
				std::filesystem::path model(m_ModelPath);
				std::filesystem::path texdir = model.parent_path() / L"Alice.fbm";
				ImGui::Text("Texture Folder: %ls", texdir.filename().c_str());
			}
		}
		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	m_pSwapChain->Present(0, 0);
}

/*
* @brief : D3D11 초기화
* @details :
* 	- 스왑체인/디바이스 생성 → RTV/DSV 바인딩
* 	- DepthStencilState(LESS), Rasterizer(Cull None)
* 	- Viewport 설정
*/
bool App::InitD3D()
{
	//HRESULT hr = 0;

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

	// 래스터라이저 상태 (Cull None)
	{
		D3D11_RASTERIZER_DESC rs{};
		rs.FillMode = D3D11_FILL_SOLID;
		rs.CullMode = D3D11_CULL_NONE; // PMX/좌수계 변환에서 와인딩 이슈 방지
		rs.FrontCounterClockwise = FALSE;
		rs.DepthClipEnable = TRUE;
		HR_T(m_pDevice->CreateRasterizerState(&rs, &m_pRasterState));
		m_pDeviceContext->RSSetState(m_pRasterState);
	}

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

	return true;
}

void App::UninitD3D()
{
	SAFE_RELEASE(m_pDepthStencilState);
	SAFE_RELEASE(m_pRasterState);
	SAFE_RELEASE(m_pDepthStencilView);
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDevice);
}

/*
* @brief : 장면 초기화(PMX 로드/병합/버퍼 생성)
* @details :
* 	- Assimp로 PMX 읽기 + ConvertToLeftHanded
* 	- 노드 재귀 순회로 글로벌 변환을 정점에 적용하며 병합
* 	- AABB 기반 중심 이동/스케일 정규화, 카메라 자동 설정
* 	- VB/IB/CB 생성
*/
bool App::InitScene()
{
	//HRESULT hr = 0;

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
	// PMX 로드 (Assimp)
	Assimp::Importer importer;
	std::string pathA(m_ModelPath.begin(), m_ModelPath.end());
	const aiScene* scene = importer.ReadFile(pathA,
		aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
		aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded);
	if (!scene || !scene->HasMeshes())
	{
		throw std::runtime_error("Failed to load PMX or no mesh found");
	}

	// 모든 노드를 순회하여 서브메시를 병합하고, 노드 변환을 적용
	auto transformPoint = [](const aiVector3D& v, const aiMatrix4x4& m) -> aiVector3D {
		aiVector3D r;
		r.x = v.x * m.a1 + v.y * m.b1 + v.z * m.c1 + m.d1;
		r.y = v.x * m.a2 + v.y * m.b2 + v.z * m.c2 + m.d2;
		r.z = v.x * m.a3 + v.y * m.b3 + v.z * m.c3 + m.d3;
		return r;
	};

	m_ModelVertices.clear();
	m_ModelIndices.clear();
	std::vector<aiVector3D> posRaw; posRaw.reserve(1024);

	/*
	* @brief : PMX 머티리얼 텍스처 로드
	* @details :
	*   - 입력: aiScene의 materials, 모델 파일 경로(baseDir)
	*   - 출력: m_MaterialSRVs[m] (머티리얼 인덱스 → SRV)
	*   - 알고리즘: material → DIFFUSE 경로 조회 → (임베디드|외부) 로드 분기 → 실패 시 "Alice.fbm/" 폴더 재시도 → 배열 저장
	*/
	// 머티리얼 텍스처 로드 (DIFFUSE)
	// - 머티리얼에서 DIFFUSE 텍스쳐 경로를 꺼내고, 외부/임베디드 모두 처리한다
	// - 경로는 PMX 파일 폴더 기준으로 해석하고, 실패하면 "Alice.fbm/" 하위에서 재시도한다
	// - 결과는 머티리얼 인덱스 → SRV 배열(m_MaterialSRVs[m])에 저장한다
	m_Subsets.clear();
	m_MaterialSRVs.clear();
	m_MaterialSRVs.resize(scene->mNumMaterials, nullptr);
	auto wstring_from_utf8 = [](const std::string& s){ return std::wstring(s.begin(), s.end()); };
	std::wstring modelPathW = m_ModelPath;
	size_t slash = modelPathW.find_last_of(L"/\\");
	std::wstring baseDir = (slash == std::wstring::npos) ? L"" : modelPathW.substr(0, slash + 1);
	for (unsigned m = 0; m < scene->mNumMaterials; ++m)
	{
		aiMaterial* mat = scene->mMaterials[m];
		aiString texPath;
		if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
		{
			ID3D11ShaderResourceView* srv = nullptr;
			std::string t = texPath.C_Str();
			if (!t.empty() && t[0] == '*')
			{
				// "*0" 형식: Assimp가 제공하는 임베디드 텍스쳐 인덱스
				// 압축/비압축 2가지 케이스를 나눠서 생성한다
				int idx = atoi(t.c_str() + 1);
				if (idx >= 0 && (unsigned)idx < scene->mNumTextures)
				{
					const aiTexture* at = scene->mTextures[idx];
					if (at)
					{
						if (at->mHeight == 0)
						{
							Microsoft::WRL::ComPtr<ID3D11Resource> res;
							if (SUCCEEDED(CreateWICTextureFromMemory(m_pDevice, reinterpret_cast<const uint8_t*>(at->pcData), at->mWidth, res.GetAddressOf(), &srv)))
								m_MaterialSRVs[m] = srv;
						}
						else
						{
							D3D11_TEXTURE2D_DESC td{};
							td.Width = at->mWidth; td.Height = at->mHeight; td.MipLevels = 1; td.ArraySize = 1;
							td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // aiTexel은 BGRA
							td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
							D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = at->pcData; sd.SysMemPitch = at->mWidth * sizeof(aiTexel);
							Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
							if (SUCCEEDED(m_pDevice->CreateTexture2D(&td, &sd, tex.GetAddressOf())))
							{
								D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
								srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
								srvd.Texture2D.MostDetailedMip = 0; srvd.Texture2D.MipLevels = 1;
								if (SUCCEEDED(m_pDevice->CreateShaderResourceView(tex.Get(), &srvd, &srv))) m_MaterialSRVs[m] = srv;
							}
						}
					}
				}
			}
			else
			{
				// 외부 텍스쳐 파일: 모델 폴더 기준 상대경로 + 실패 시 "Alice.fbm/" 폴더 재시도
				std::wstring wtex = wstring_from_utf8(t);
				bool isAbs = (!wtex.empty() && (wtex.find(L":") != std::wstring::npos || wtex[0] == L'/' || wtex[0] == L'\\'));
				std::wstring full = isAbs ? wtex : (baseDir + wtex);
				Microsoft::WRL::ComPtr<ID3D11Resource> res;
				HRESULT hrLoad = CreateWICTextureFromFile(m_pDevice, full.c_str(), res.GetAddressOf(), &srv);
				if (FAILED(hrLoad))
				{
					// 예: "head_face_M_BC.jpg" 처럼 파일명만 올 때 → baseDir + "Alice.fbm/" + 파일명
					std::wstring fbm = L"Alice.fbm/" + wtex;
					std::wstring full2 = baseDir + fbm;
					res.Reset(); srv = nullptr;
					hrLoad = CreateWICTextureFromFile(m_pDevice, full2.c_str(), res.GetAddressOf(), &srv);
				}
				if (SUCCEEDED(hrLoad)) m_MaterialSRVs[m] = srv;
			}
		}
	}

	/*
	* @brief : 노드 트래버스 + 서브메시 병합
	* @details :
	*   - 입력: aiScene 노드 트리, 각 노드의 로컬 트랜스폼
	*   - 출력: m_ModelVertices / m_ModelIndices / m_Subsets
	*   - 알고리즘: DFS 순회(부모*자식) 글로벌 변환 적용 → 정점/인덱스 이어붙이기 → 서브셋 범위 기록
	*/
	std::function<void(const aiNode*, const aiMatrix4x4&)> traverse;
	traverse = [&](const aiNode* node, const aiMatrix4x4& parent) {
		aiMatrix4x4 global = parent * node->mTransformation;
		for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
		{
			const aiMesh* mesh = scene->mMeshes[node->mMeshes[mi]];
			size_t baseIndex = m_ModelVertices.size();
			for (unsigned i = 0; i < mesh->mNumVertices; ++i)
			{
				aiVector3D p = transformPoint(mesh->mVertices[i], global);
				aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0,0,0);
				m_ModelVertices.push_back({ XMFLOAT3(p.x, p.y, p.z), XMFLOAT2(uv.x, uv.y) });
				posRaw.push_back(p);
			}
			uint32_t start = (uint32_t)m_ModelIndices.size();
			// 이 메쉬에 해당하는 인덱스 범위를 기록해둔다(나중에 머티리얼 SRV 바인딩 후 부분 드로우)

			for (unsigned f = 0; f < mesh->mNumFaces; ++f)
			{
				const aiFace& face = mesh->mFaces[f];
				if (face.mNumIndices == 3)
				{
					m_ModelIndices.push_back(static_cast<uint32_t>(baseIndex + face.mIndices[0]));
					m_ModelIndices.push_back(static_cast<uint32_t>(baseIndex + face.mIndices[1]));
					m_ModelIndices.push_back(static_cast<uint32_t>(baseIndex + face.mIndices[2]));
				}
			}
			uint32_t count = (uint32_t)m_ModelIndices.size() - start;
			// materialIndex를 같이 보관해 PS에서 SRV를 바꾼 뒤 해당 범위만 그리기
			m_Subsets.push_back({ start, count, mesh->mMaterialIndex });
		}
		for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
		{
			traverse(node->mChildren[ci], global);
		}
	};
	traverse(scene->mRootNode, aiMatrix4x4());

	/*
	* @brief : AABB 정규화
	* @details :
	*   - 입력: posRaw(글로벌 적용된 원본 좌표)
	*   - 출력: m_ModelVertices 위치를 원점 중심, 반경 targetRadius로 스케일링
	*   - 알고리즘: min/max → center/extent → radius → scale → 정점 적용
	*/
	// AABB 계산 → 중심 이동/스케일 정규화
	aiVector3D minP(FLT_MAX, FLT_MAX, FLT_MAX), maxP(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for (auto& p : posRaw)
	{
		minP.x = std::min(minP.x, p.x); minP.y = std::min(minP.y, p.y); minP.z = std::min(minP.z, p.z);
		maxP.x = max(maxP.x, p.x); maxP.y = max(maxP.y, p.y); maxP.z = max(maxP.z, p.z);
	}
	aiVector3D center = (minP + maxP) * 0.5f;
	aiVector3D extent = (maxP - minP) * 0.5f;
	float radius = max(extent.x, max(extent.y, extent.z));
	float targetRadius = 2.0f;
	float scale = (radius > 0.0f) ? (targetRadius / radius) : 1.0f;
	for (auto& v : m_ModelVertices)
	{
		XMFLOAT3& p = v.pos;
		p.x = (p.x - center.x) * scale;
		p.y = (p.y - center.y) * scale;
		p.z = (p.z - center.z) * scale;
	}

	/*
	* @brief : 카메라 자동 셋업
	* @details :
	*   - 입력: targetRadius(정규화 기준)
	*   - 출력: m_RootPos / m_CameraPos / m_CameraNear / m_CameraFar
	*   - 알고리즘: 반경 기반 기본 시야 범위와 카메라 위치 지정
	*/
	// 카메라 자동 셋업
	m_RootPos = XMFLOAT3(0,0,0);
	m_CameraNear = 0.1f;
	m_CameraFar = max(1000.0f, targetRadius * 20.0f);
	m_CameraPos = XMFLOAT3(0.0f, targetRadius * 0.5f, -targetRadius * 2.0f);

	D3D11_BUFFER_DESC vbDesc = {};
	ZeroMemory(&vbDesc, sizeof(vbDesc));			// vbDesc에 0으로 전체 메모리 영역을 초기화 시킵니다
	vbDesc.ByteWidth = (UINT)(m_ModelVertices.size() * sizeof(VertexPosTex));				// 배열 전체의 바이트 크기
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA vbData = {};
	ZeroMemory(&vbData, sizeof(vbData));
	vbData.pSysMem = m_ModelVertices.data();						// 데이터 할당.
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));


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
	D3D11_BUFFER_DESC ibDesc = {};
	ZeroMemory(&ibDesc, sizeof(ibDesc));
	m_nIndices = (int)m_ModelIndices.size();	// 인덱스 개수 저장.
	ibDesc.ByteWidth = (UINT)(m_ModelIndices.size() * sizeof(uint32_t));
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = m_ModelIndices.data();
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));


	/*
	* @brief : 상수 버퍼/뷰/프로젝션 준비
	* @details :
	*   - 입력: 없음(UI 상태 사용)
	*   - 출력: m_pConstantBuffer, m_CBuffer(world/view/proj)
	*   - 알고리즘: 동적 상수 버퍼 생성 → LookAtLH(전방 고정) / PerspectiveFovLH 셋업
	*/
	// ******************
	// 상수 버퍼 설정
	//
	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(ConstantBuffer);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	// 초기 데이터를 사용하지 않고 새로운 상수 버퍼를 만듭니다
	HR_T(m_pDevice->CreateBuffer(&cbd, nullptr, &m_pConstantBuffer));

	// 공통 카메라(View/Proj)
	m_CBuffer.world = XMMatrixIdentity();
	m_CBuffer.view = XMMatrixTranspose(XMMatrixLookAtLH(
		XMVectorSet(m_CameraPos.x, m_CameraPos.y, m_CameraPos.z, 0.0f),
		XMVectorSet(m_CameraPos.x + 0.0f, m_CameraPos.y + 0.0f, m_CameraPos.z + 1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	));
	float fovRad = XMConvertToRadians(m_CameraFovDeg);
	m_CBuffer.proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovRad, AspectRatio(), m_CameraNear, m_CameraFar));

	/*
	* @brief : 샘플러/폴백 텍스처 생성
	* @details :
	*   - 입력: 없음
	*   - 출력: m_pSamplerState, m_pWhiteSRV(필요 시)
	*   - 알고리즘: 선형 필터 샘플러 생성 → 1x1 흰색 텍스처 생성 → 실패 머티리얼 흰색 채움
	*/
	// 샘플러 생성 및 기본 흰색 텍스처 폴백 준비
	{
		D3D11_SAMPLER_DESC sampDesc = {};
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
		HR_T(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState));

		if (!m_pWhiteSRV)
		{
			UINT white = 0xFFFFFFFF;
			D3D11_TEXTURE2D_DESC td{}; td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = &white; sd.SysMemPitch = sizeof(UINT);
			Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
			HR_T(m_pDevice->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
			D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MostDetailedMip = 0; srvd.Texture2D.MipLevels = 1;
			HR_T(m_pDevice->CreateShaderResourceView(tex.Get(), &srvd, &m_pWhiteSRV));
		}
		for (auto& p : m_MaterialSRVs) { if (p == nullptr) { p = m_pWhiteSRV; } if (p) p->AddRef(); }
	}
 
	return true;
}

void App::UninitScene()
{
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_pTextureSRVs[i]);
	for (auto& srv : m_MaterialSRVs) SAFE_RELEASE(srv);
	SAFE_RELEASE(m_pWhiteSRV);
	SAFE_RELEASE(m_pSamplerState);
}

/*
* @brief : 셰이더/입력 레이아웃 초기화
* @details :
* 	- VS/PS 컴파일 및 객체 생성
* 	- POSITION/TEXCOORD 입력 레이아웃 구성
*/
bool App::InitEffect()
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
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"07_TexVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
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
	HR_T(CompileShaderFromFile(L"07_TexPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음
	return true;
}
