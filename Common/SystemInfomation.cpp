#include "pch.h"
#include "SystemInfomation.h"
#include <thread>
#include <d3d11.h>
#include <Psapi.h>
#include <dxgi1_4.h>

void SystemInfomation::Tick(const float& deltaTime)
{
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

	// FPS 1초 업데이트
	m_FpsTimer += deltaTime;
	if (m_FpsTimer >= 1.0f)
	{
		m_LastFps = 1.0f / deltaTime; // 간단히 최근 프레임의 역수 사용
		m_FpsTimer = 0.0f;
	}
}

bool SystemInfomation::InitSysInfomation(ID3D11Device*& m_pDevice)
{
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

void SystemInfomation::RenderUI()
{
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
}
