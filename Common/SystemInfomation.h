#pragma once
#include <dxgi1_4.h>

class SystemInfomation
{
public:
	void Tick(const float& deltaTime);
	bool InitSysInfomation(ID3D11Device*& m_pDevice);									// d3d11 디바이스로부터 시스템 정보를 얻어옴
	void RenderUI();																		// ImGui로 시스템 정보를 표시
	std::wstring m_GPUName;										// GPU 이름
	std::wstring m_CPUName;										// CPU 이름
	UINT m_CPUCores = 0; 										// CPU 코어 개수
	ULONGLONG m_RamTotal = 0; 									// 총 RAM 바이트
	ULONGLONG m_RamAvail = 0; 									// 사용 가능한 RAM 바이트
	SIZE_T m_VideoMemoryTotal = 0;								// 총 VRAM 바이트

	// FPS 출력 캐시
	float m_LastFps = 0.0f;
	float m_FpsAccum = 0.0f;
	float m_FpsTimer = 0.0f;

	Microsoft::WRL::ComPtr<IDXGIAdapter3> m_Adapter3;			// VRAM 조회용
};

