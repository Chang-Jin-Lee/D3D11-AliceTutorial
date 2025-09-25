/*
* @brief : 08_ImguiSystemInfo - ImGui ��� �ý��� ����/�Է�/�̹��� ��� ����
* @details :
*   - ���� : Direct3D 11 + ImGui�� ������ �ý��� ���� �гΰ� �̹��� �� �����ϴ� �ּ� ����
*   - �ֿ� ��� :
*     �� System Info â: FPS, GPU/CPU, RAM/VRAM �ǽð� ǥ��
*     �� Controls â: �̹��� ǥ��/������(����/����), ���� ����(Ž����)�� �ܺ� �̹��� �ε�
*     �� �̹��� ���: Hanako/Yuuka ���� ���� + �ܺ� ���� ���� ����(Loaded Image)
*       - Lock Aspect, Fit To Window, Fit 512, Fit Image
*       - �̹��� ���ϴ� �������� �ڵ�, Ctrl+�巡��(��ü ����) ��������
*     �� ����/�ȳ� ��������: �߾� ���� ������� 3�ʰ� �޽��� ǥ��
*   - �н� ����Ʈ :
*     �� D3D11 �⺻ �ʱ�ȭ + RTV/����Ʈ ���� + ����ü�� Present
*     �� ImGui ������ ����������Ŭ(NewFrame �� RenderDrawData)
*     �� WIC/DDSTextureLoader�� Ȱ���� �ؽ�ó SRV �ε��� ������ ��ȸ
*     �� Win32 OPENFILENAME���� ���� Ž���� ����
*/

// @brief : �ʼ� ���� �� ��ũ
// @details : D3D11/ImGui/���ϴ�ȭ���� ����� ���� ����� ���̺귯�� ����
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

// @brief : �ؽ�ó �ε�� ũ�� ��ȸ
// @details : ���� ���� Ȯ�� �� SRV ���� �� 2D �ؽ�ó desc�� ����/���� �ȼ� ����
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

// @brief : ���� ��� �ε� �õ�
// @details : ���� ���� ���� ��θ� Ȯ���� SRV/����� ä���
static bool TryLoadTextureWithFallbacks(ID3D11Device* device, const std::wstring& path,
	ID3D11ShaderResourceView** outSRV, ImVec2* outSize)
{
	if (LoadTextureSRVAndSize(device, path, outSRV, outSize)) return true;
	return false;
}

// @brief : Windows ���� ���� ��ȭ����
// @details : png/jpg/jpeg/bmp/dds ����, ���� �� ��ü ��θ� ��ȯ
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
* @brief : �� �ʱ�ȭ(������ ȯ�� + ȿ�� + ���)
* @details :
* 	- InitD3D  : ����̽�/���ؽ�Ʈ/����ü��/RTV/DSV/������ ����
* 	- InitEffect: VS/PS ������ �� �Է� ���̾ƿ� ����
* 	- InitScene : PMX �ε� �� ���� �� VB/IB/CB ����
*/
bool App::OnInitialize()
{
	if(!InitD3D()) return false;

	if (!InitEffect()) return false;

	if(!InitScene()) return false;

	// ImGui �ʱ�ȭ
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);

	// �ؽ��� �ε�(Hanako, Yuuka)
	TryLoadTextureWithFallbacks(m_pDevice, L"..\\Resource\\Image\\Hanako.png", &m_TexHanakoSRV, &m_TexHanakoSize);
	TryLoadTextureWithFallbacks(m_pDevice, L"..\\Resource\\Image\\Yuuka.png", &m_TexYuukaSRV, &m_TexYuukaSize);

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

	// �ؽ��� ����
	SAFE_RELEASE(m_TexHanakoSRV);
	SAFE_RELEASE(m_TexYuukaSRV);
	SAFE_RELEASE(m_LoadedSRV);

	UninitD3D();
}

/*
* @brief : ������ ����(ī�޶�/���� ���, ���)
* @details :
* 	- world : ��Ʈ ��ġ �����̵� + Yaw ȸ��
* 	- view/proj : UI ī�޶� �Ķ���� �ݿ�
* 	- FPS : 1�� ���� ����
*/
void App::OnUpdate(const float& dt)
{
	static float t0 = 0.0f, t1 = 0.0f, t2 = 0.0f;
	t0 += 0.6f * dt;   // �θ�(��Ʈ) Yaw �ӵ�
	t1 += 1.0f * dt;   // �ι�° �޽�(�ڽ�1) Yaw �ӵ� (��Ʈ�� �ٸ���)
	t2 += 1.2f * dt;   // ����° �޽�(�ڽ�2) ���� �ӵ�

	// ���� ��ȯ ���� (���� Scene Graph)
	XMMATRIX local0 = XMMatrixRotationY(t0) * XMMatrixTranslation(m_RootPos.x, m_RootPos.y, m_RootPos.z); // ��Ʈ
	// �θ�-�ڽ� ���� world ���: world = local * parentWorld
	XMMATRIX world0 = local0;                 // ��Ʈ

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
			float steps = (float)delta / WHEEL_DELTA; // ���� 120 ����
			posV = XMVectorAdd(posV, XMVectorScale(forward, steps * 0.5f));
		}
	}
	XMStoreFloat3(&m_CameraPos, posV);
	
	// View/Proj�� UI�� �ݿ� (�� ������)
	XMMATRIX view = XMMatrixTranspose(XMMatrixLookAtLH(
		posV,
		XMVectorAdd(posV, forward),
		up));
	float fovRad = XMConvertToRadians(m_CameraFovDeg);
	XMMATRIX proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovRad, AspectRatio(), m_CameraNear, m_CameraFar));

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

// Render() �Լ��� �߿��� �κ��� �� ����ֽ��ϴ�. ���⸦ ���� �˴ϴ�

/*
* @brief : ������ ����(���� DrawIndexed)
* @details :
* 	- OM/IA : Ŭ����, ��������, VB/IB, �Է� ���̾ƿ�
* 	- VS/PS : ���̴� ���ε�, ��� ���� ������Ʈ
* 	- Draw  : ���� �ε��� ��ο�
*/
void App::OnRender()
{
	// ���� Ÿ�� ���ε� �� Ŭ����, ����Ʈ ����
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

	// ImGui ������ �� UI ������
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

		// Controls �� �Ʒ�: LoadTexture ��ư
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
					m_NoticeText = "��� �� �����ϴ� (�̹��� ������ �ƴ�)";
					m_NoticeTimeLeft = 3.0f;
				}
			}
		}
	}
	ImGui::End();

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

	// �ܼ� �̹��� ��� â: ��ư ���� ��� ǥ��
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

	// ���� �ε� �̹��� ��� (Hanako/Yuuka�� ����)
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

	// �̹��� â: Hanako
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
				m_HanakoDrawSize = draw; // �ݿ�
			}
			else if (m_HanakoLockAR && m_TexHanakoSize.y > 0)
			{
				float ar = m_TexHanakoSize.x / m_TexHanakoSize.y;
				if (draw.x / draw.y > ar) draw.x = draw.y * ar; else draw.y = draw.x / ar;
			}
			dl->AddImage((ImTextureID)m_TexHanakoSRV, start, ImVec2(start.x + draw.x, start.y + draw.y));
			// �������� �ڵ�
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
			// Ctrl+�巡��: ������ ��𼭳� ������ ����
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
	// �̹��� â: Yuuka
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

	// �߾� ���/�˸� �������� (3��)
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
* @brief : D3D11 �ʱ�ȭ
* @details :
* 	- ����ü��/����̽� ���� �� RTV/DSV ���ε�
* 	- DepthStencilState(LESS), Rasterizer(Cull None)
* 	- Viewport ����
*/
bool App::InitD3D()
{
	// ����� â�� ���� �����Դϴ�.
	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	// ����ü���� ������ ������ ����ü�� ����ϴ�
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

	// ����� RTV ���� �� ����Ʈ ����
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
