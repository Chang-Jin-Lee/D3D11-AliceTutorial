#pragma once
#include <dxgi1_4.h>

class SystemInfomation
{
public:
	void Tick(const float& deltaTime);
	bool InitSysInfomation(ID3D11Device*& m_pDevice);									// d3d11 ����̽��κ��� �ý��� ������ ����
	void RenderUI();																		// ImGui�� �ý��� ������ ǥ��
	std::wstring m_GPUName;										// GPU �̸�
	std::wstring m_CPUName;										// CPU �̸�
	UINT m_CPUCores = 0; 										// CPU �ھ� ����
	ULONGLONG m_RamTotal = 0; 									// �� RAM ����Ʈ
	ULONGLONG m_RamAvail = 0; 									// ��� ������ RAM ����Ʈ
	SIZE_T m_VideoMemoryTotal = 0;								// �� VRAM ����Ʈ

	// FPS ��� ĳ��
	float m_LastFps = 0.0f;
	float m_FpsAccum = 0.0f;
	float m_FpsTimer = 0.0f;

	Microsoft::WRL::ComPtr<IDXGIAdapter3> m_Adapter3;			// VRAM ��ȸ��
};

