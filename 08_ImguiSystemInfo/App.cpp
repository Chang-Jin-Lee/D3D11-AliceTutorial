/*
* @brief : 08_ImguiSystemInfo - ImGui 기반 시스템 정보/입력/이미지 뷰어 예제
* @details :
*   - 목적 : Direct3D 11 + ImGui로 간단한 시스템 정보 패널과 이미지 뷰어를 구성하는 최소 예제
*   - 주요 기능 :
*     · System Info 창: FPS, GPU/CPU, RAM/VRAM 실시간 표시
*     · Controls 창: 이미지 표시/사이즈(고정/맞춤), 파일 열기(탐색기)로 외부 이미지 로드
*     · 이미지 뷰어: Hanako/Yuuka 고정 슬롯 + 외부 파일 전용 슬롯(Loaded Image)
*       - Lock Aspect, Fit To Window, Fit 512, Fit Image
*       - 이미지 우하단 리사이즈 핸들, Ctrl+드래그(전체 영역) 리사이즈
*     · 오류/안내 오버레이: 중앙 빨간 배경으로 3초간 메시지 표시
*   - 학습 포인트 :
*     · D3D11 기본 초기화 + RTV/뷰포트 설정 + 스왑체인 Present
*     · ImGui 프레임 라이프사이클(NewFrame → RenderDrawData)
*     · WIC/DDSTextureLoader를 활용한 텍스처 SRV 로딩과 사이즈 조회
*     · Win32 OPENFILENAME으로 파일 탐색기 열기
*/

// @brief : 필수 포함 및 링크
// @details : D3D11/ImGui/파일대화상자 사용을 위한 헤더와 라이브러리 지정
#include "App.h"
#include "../Common/Helper.h"
#include "../Common/InputSystem.h"
#include <d3dcompiler.h>
#include <directxtk/WICTextureLoader.h>
#include <thread>
#include <unordered_set>
#include <filesystem>
#include <commdlg.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

// @brief : 텍스처 로드와 크기 조회
// @details : 파일 존재 확인 → SRV 생성 → 2D 텍스처 desc로 가로/세로 픽셀 추출
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

// @brief : 단일 경로 로드 시도
// @details : 폴백 없이 단일 경로만 확인해 SRV/사이즈를 채운다
static bool TryLoadTextureWithFallbacks(ID3D11Device* device, const std::wstring& path,
	ID3D11ShaderResourceView** outSRV, ImVec2* outSize)
{
	if (LoadTextureSRVAndSize(device, path, outSRV, outSize)) return true;
	return false;
}

// @brief : Windows 파일 열기 대화상자
// @details : png/jpg/jpeg/bmp/dds 필터, 성공 시 전체 경로를 반환
static bool OpenFileDialogImage(std::wstring& outPath)
{
	wchar_t file[1024] = {0};
	OPENFILENAMEW ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GameApp::m_hWnd;
	ofn.lpstrFilter = L"Images (*.png;*.jpg;*.jpeg;*.bmp;*.dds)\0*.png;*.jpg;*.jpeg;*.bmp;*.dds\0All Files\0*.*\0\0";
	ofn.lpstrFile = file;
	ofn.nMaxFile = 1024;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	if (GetOpenFileNameW(&ofn)) { outPath = file; return true; }
	return false;
}

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

	// 텍스쳐 로드(Hanako, Yuuka)
	TryLoadTextureWithFallbacks(m_pDevice, L"..\\Resource\\Image\\Hanako.png", &m_TexHanakoSRV, &m_TexHanakoSize);
	TryLoadTextureWithFallbacks(m_pDevice, L"..\\Resource\\Image\\Yuuka.png", &m_TexYuukaSRV, &m_TexYuukaSize);

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

	// 텍스쳐 해제
	SAFE_RELEASE(m_TexHanakoSRV);
	SAFE_RELEASE(m_TexYuukaSRV);
	SAFE_RELEASE(m_LoadedSRV);

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
		sPitch = min(max(sPitch, -limit), limit);
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
	// 렌더 타겟 바인딩 및 클리어, 뷰포트 갱신
	if (m_pDeviceContext && m_pRenderTargetView)
	{
		m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);
		D3D11_VIEWPORT vp{};
		vp.TopLeftX = 0.0f;
		vp.TopLeftY = 0.0f;
		vp.Width = static_cast<FLOAT>(m_ClientWidth);
		vp.Height = static_cast<FLOAT>(m_ClientHeight);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		m_pDeviceContext->RSSetViewports(1, &vp);
		const float clearColor[4] = { 0.10f, 0.12f, 0.15f, 1.0f };
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clearColor);
	}

	// ImGui 프레임 및 UI 렌더링
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (ImGui::Begin("Controls"))
	{
		// Images controls
		ImGui::Separator();
		ImGui::Text("Images");
		ImGui::Checkbox("Show Hanako", &m_ShowHanako); ImGui::SameLine(); ImGui::Checkbox("LockAR##hanako", &m_HanakoLockAR);
		ImGui::DragFloat2("Hanako Size", &m_HanakoDrawSize.x, 1.0f, 32.0f, 4096.0f, "%.0f");
		ImGui::SameLine(); if (ImGui::Button("Fit 512##hanako")) m_HanakoDrawSize = ImVec2(512,512);
		ImGui::SameLine(); if (ImGui::Button("Fit Image##hanako")) m_HanakoDrawSize = m_TexHanakoSize;
		ImGui::Checkbox("Show Yuuka", &m_ShowYuuka); ImGui::SameLine(); ImGui::Checkbox("LockAR##yuuka", &m_YuukaLockAR);
		ImGui::DragFloat2("Yuuka Size", &m_YuukaDrawSize.x, 1.0f, 32.0f, 4096.0f, "%.0f");
		ImGui::SameLine(); if (ImGui::Button("Fit 512##yuuka")) m_YuukaDrawSize = ImVec2(512,512);
		ImGui::SameLine(); if (ImGui::Button("Fit Image##yuuka")) m_YuukaDrawSize = m_TexYuukaSize;

		// Controls 맨 아래: LoadTexture 버튼
		ImGui::Separator();
		if (ImGui::Button("LoadTexture"))
		{
			std::wstring path;
			if (OpenFileDialogImage(path))
			{
				ID3D11ShaderResourceView* srv = nullptr; ImVec2 size(0,0);
				if (LoadTextureSRVAndSize(m_pDevice, path, &srv, &size))
				{
					SAFE_RELEASE(m_LoadedSRV);
					m_LoadedSRV = srv; m_LoadedSize = size; m_LoadedDrawSize = ImVec2(512,512);
					m_ShowLoaded = true; m_LoadedFitToWindow = false; m_LoadedLockAR = true;
					m_LoadedTitle = "Loaded Image";
				}
				else
				{
					m_NoticeText = "띄울 수 없습니다 (이미지 파일이 아님)";
					m_NoticeTimeLeft = 3.0f;
				}
			}
		}
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
			// VRAM
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

	// 단순 이미지 뷰어 창: 버튼 없이 즉시 표시
	if (m_TexHanakoSRV || m_TexYuukaSRV)
	{
		ImGui::SetNextWindowPos(ImVec2(510, 210), ImGuiCond_Once);
		ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_Once);
		if (ImGui::Begin("Images"))
		{
			ImGuiWindowFlags childFlags = ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar;
			// Hanako
			if (m_TexHanakoSRV)
			{
				ImGui::TextUnformatted("Hanako");
				ImGui::BeginChild("HanakoView", ImVec2(0, m_HanakoDrawSize.y + 40), true, childFlags);
				ImVec2 desired = (m_HanakoDrawSize.x > 0 && m_HanakoDrawSize.y > 0) ? m_HanakoDrawSize : m_TexHanakoSize;
				ImGui::Image((ImTextureID)m_TexHanakoSRV, desired);
				ImGui::EndChild();
			}
			// Yuuka
			if (m_TexYuukaSRV)
			{
				ImGui::Separator();
				ImGui::TextUnformatted("Yuuka");
				ImGui::BeginChild("YuukaView", ImVec2(0, m_YuukaDrawSize.y + 40), true, childFlags);
				ImVec2 desired2 = (m_YuukaDrawSize.x > 0 && m_YuukaDrawSize.y > 0) ? m_YuukaDrawSize : m_TexYuukaSize;
				ImGui::Image((ImTextureID)m_TexYuukaSRV, desired2);
				ImGui::EndChild();
			}
		}
		ImGui::End();
	}

	// 별도 로드 이미지 뷰어 (Hanako/Yuuka와 독립)
	if (m_ShowLoaded && m_LoadedSRV)
	{
		ImGui::SetNextWindowSize(ImVec2(560, 580), ImGuiCond_Once);
		const char* title = m_LoadedTitle.empty() ? "Loaded Image" : m_LoadedTitle.c_str();
		if (ImGui::Begin(title, &m_ShowLoaded))
		{
			ImGui::Checkbox("Lock Aspect##ld", &m_LoadedLockAR);
			ImGui::SameLine();
			ImGui::Checkbox("Fit To Window##ld", &m_LoadedFitToWindow);
			ImGui::SameLine();
			if (ImGui::Button("Fit 512##ld")) m_LoadedDrawSize = ImVec2(512,512);
			ImGui::SameLine();
			if (ImGui::Button("Fit Image##ld")) m_LoadedDrawSize = m_LoadedSize;
			ImGui::Separator();
			ImVec2 start = ImGui::GetCursorScreenPos();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 avail = ImGui::GetContentRegionAvail();
			ImVec2 draw = m_LoadedDrawSize;
			if (m_LoadedFitToWindow)
			{
				draw = avail;
				if (m_LoadedLockAR && m_LoadedSize.y > 0)
				{
					float ar = m_LoadedSize.x / m_LoadedSize.y;
					if (draw.x / draw.y > ar) draw.x = draw.y * ar; else draw.y = draw.x / ar;
				}
				m_LoadedDrawSize = draw;
			}
			else if (m_LoadedLockAR && m_LoadedSize.y > 0)
			{
				float ar = m_LoadedSize.x / m_LoadedSize.y;
				if (draw.x / draw.y > ar) draw.x = draw.y * ar; else draw.y = draw.x / ar;
			}
			dl->AddImage((ImTextureID)m_LoadedSRV, start, ImVec2(start.x + draw.x, start.y + draw.y));
		}
		ImGui::End();
	}

	// 이미지 창: Hanako
	if (m_ShowHanako && m_TexHanakoSRV)
	{
		ImGui::SetNextWindowSize(ImVec2(540, 560), ImGuiCond_Once);
		if (ImGui::Begin("Hanako", &m_ShowHanako))
		{
			ImGui::Checkbox("Lock Aspect", &m_HanakoLockAR);
			ImGui::SameLine();
			ImGui::Checkbox("Fit To Window", &m_HanakoFitToWindow);
			ImGui::SameLine();
			if (ImGui::Button("Fit 512")) { m_HanakoDrawSize = ImVec2(512,512); }
			ImGui::SameLine();
			if (ImGui::Button("Fit Image")) { m_HanakoDrawSize = m_TexHanakoSize; }
			ImGui::Separator();
			ImVec2 start = ImGui::GetCursorScreenPos();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 avail = ImGui::GetContentRegionAvail();
			ImVec2 draw = m_HanakoDrawSize;
			if (m_HanakoFitToWindow)
			{
				draw = avail;
				if (m_HanakoLockAR && m_TexHanakoSize.y > 0)
				{
					float ar = m_TexHanakoSize.x / m_TexHanakoSize.y;
					if (draw.x / draw.y > ar) draw.x = draw.y * ar; else draw.y = draw.x / ar;
				}
				m_HanakoDrawSize = draw; // 반영
			}
			else if (m_HanakoLockAR && m_TexHanakoSize.y > 0)
			{
				float ar = m_TexHanakoSize.x / m_TexHanakoSize.y;
				if (draw.x / draw.y > ar) draw.x = draw.y * ar; else draw.y = draw.x / ar;
			}
			dl->AddImage((ImTextureID)m_TexHanakoSRV, start, ImVec2(start.x + draw.x, start.y + draw.y));
			// 리사이즈 핸들
			ImGui::SetCursorScreenPos(ImVec2(start.x + draw.x - 12, start.y + draw.y - 12));
			ImGui::InvisibleButton("hanako_resizer", ImVec2(12,12));
			bool hovered = ImGui::IsItemHovered();
			bool held = ImGui::IsItemActive();
			if (hovered || held) dl->AddRectFilled(ImVec2(start.x + draw.x - 12, start.y + draw.y - 12), ImVec2(start.x + draw.x, start.y + draw.y), IM_COL32(255,255,255,128));
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				ImVec2 delta = ImGui::GetIO().MouseDelta;
				m_HanakoFitToWindow = false;
				m_HanakoDrawSize.x = max(32.0f, m_HanakoDrawSize.x + delta.x);
				m_HanakoDrawSize.y = max(32.0f, m_HanakoDrawSize.y + delta.y);
			}
			// Ctrl+드래그: 콘텐츠 어디서나 사이즈 조절
			if (ImGui::GetIO().KeyCtrl)
			{
				ImGui::SetItemAllowOverlap();
				ImGui::SetCursorScreenPos(start);
				ImGui::InvisibleButton("hanako_drag_area", ImVec2(draw.x, draw.y));
				if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					ImVec2 delta = ImGui::GetIO().MouseDelta;
					m_HanakoFitToWindow = false;
					m_HanakoDrawSize.x = max(32.0f, m_HanakoDrawSize.x + delta.x);
					m_HanakoDrawSize.y = max(32.0f, m_HanakoDrawSize.y + delta.y);
				}
			}
		}
		ImGui::End();
	}
	// 이미지 창: Yuuka
	if (m_ShowYuuka && m_TexYuukaSRV)
	{
		ImGui::SetNextWindowSize(ImVec2(540, 560), ImGuiCond_Once);
		if (ImGui::Begin("Yuuka", &m_ShowYuuka))
		{
			ImGui::Checkbox("Lock Aspect", &m_YuukaLockAR);
			ImGui::SameLine();
			ImGui::Checkbox("Fit To Window", &m_YuukaFitToWindow);
			ImGui::SameLine();
			if (ImGui::Button("Fit 512##y")) { m_YuukaDrawSize = ImVec2(512,512); }
			ImGui::SameLine();
			if (ImGui::Button("Fit Image##y")) { m_YuukaDrawSize = m_TexYuukaSize; }
			ImGui::Separator();
			ImVec2 start = ImGui::GetCursorScreenPos();
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 avail = ImGui::GetContentRegionAvail();
			ImVec2 draw = m_YuukaDrawSize;
			if (m_YuukaFitToWindow)
			{
				draw = avail;
				if (m_YuukaLockAR && m_TexYuukaSize.y > 0)
				{
					float ar = m_TexYuukaSize.x / m_TexYuukaSize.y;
					if (draw.x / draw.y > ar) draw.x = draw.y * ar; else draw.y = draw.x / ar;
				}
				m_YuukaDrawSize = draw;
			}
			else if (m_YuukaLockAR && m_TexYuukaSize.y > 0)
			{
				float ar = m_TexYuukaSize.x / m_TexYuukaSize.y;
				if (draw.x / draw.y > ar) draw.x = draw.y * ar; else draw.y = draw.x / ar;
			}
			dl->AddImage((ImTextureID)m_TexYuukaSRV, start, ImVec2(start.x + draw.x, start.y + draw.y));
			ImGui::SetCursorScreenPos(ImVec2(start.x + draw.x - 12, start.y + draw.y - 12));
			ImGui::InvisibleButton("yuuka_resizer", ImVec2(12,12));
			bool hovered = ImGui::IsItemHovered();
			bool held = ImGui::IsItemActive();
			if (hovered || held) dl->AddRectFilled(ImVec2(start.x + draw.x - 12, start.y + draw.y - 12), ImVec2(start.x + draw.x, start.y + draw.y), IM_COL32(255,255,255,128));
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				ImVec2 delta = ImGui::GetIO().MouseDelta;
				m_YuukaFitToWindow = false;
				m_YuukaDrawSize.x = max(32.0f, m_YuukaDrawSize.x + delta.x);
				m_YuukaDrawSize.y = max(32.0f, m_YuukaDrawSize.y + delta.y);
			}
			if (ImGui::GetIO().KeyCtrl)
			{
				ImGui::SetItemAllowOverlap();
				ImGui::SetCursorScreenPos(start);
				ImGui::InvisibleButton("yuuka_drag_area", ImVec2(draw.x, draw.y));
				if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				{
					ImVec2 delta = ImGui::GetIO().MouseDelta;
					m_YuukaFitToWindow = false;
					m_YuukaDrawSize.x = max(32.0f, m_YuukaDrawSize.x + delta.x);
					m_YuukaDrawSize.y = max(32.0f, m_YuukaDrawSize.y + delta.y);
				}
			}
		}
		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// 중앙 경고/알림 오버레이 (3초)
	if (m_NoticeTimeLeft > 0.0f)
	{
		m_NoticeTimeLeft -= ImGui::GetIO().DeltaTime;
		ImGuiIO& io = ImGui::GetIO();
		ImDrawList* fg = ImGui::GetForegroundDrawList();
		ImVec2 size = ImGui::CalcTextSize(m_NoticeText.c_str());
		ImVec2 p0 = ImVec2((io.DisplaySize.x - size.x) * 0.5f - 14, (io.DisplaySize.y - size.y) * 0.5f - 10);
		ImVec2 p1 = ImVec2(p0.x + size.x + 28, p0.y + size.y + 20);
		fg->AddRectFilled(p0, p1, IM_COL32(60, 0, 0, 200), 8.0f);
		fg->AddRect(p0, p1, IM_COL32(255, 60, 60, 255), 8.0f, 0, 2.0f);
		fg->AddText(ImVec2(p0.x + 14, p0.y + 10), IM_COL32(255, 80, 80, 255), m_NoticeText.c_str());
		if (m_NoticeTimeLeft <= 0.0f) { m_NoticeTimeLeft = 0.0f; m_NoticeText.clear(); }
	}

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
	// 디버그 창을 띄우기 위함입니다.
	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	// 스왑체인의 값들을 설정할 구조체를 만듭니다
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;
	swapDesc.Windowed = true;
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	HR_T(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_pSwapChain, &m_pDevice, NULL, &m_pDeviceContext));

	// 백버퍼 RTV 생성 및 뷰포트 설정
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
	HR_T(m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())));
	HR_T(m_pDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_pRenderTargetView));
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);

	D3D11_VIEWPORT vp{};
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width = static_cast<FLOAT>(m_ClientWidth);
	vp.Height = static_cast<FLOAT>(m_ClientHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	m_pDeviceContext->RSSetViewports(1, &vp);

	return true;
}

void App::UninitD3D()
{
	SAFE_RELEASE(m_pRasterState);
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDevice);
}

bool App::InitScene()
{
	return true;
}

void App::UninitScene()
{

}

bool App::InitEffect()
{
	return true;
}
