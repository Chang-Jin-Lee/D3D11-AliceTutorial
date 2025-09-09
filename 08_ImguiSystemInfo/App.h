#pragma once
#include <windows.h>
#include "../Common/GameApp.h"
#include <d3d11.h>
#include <directxtk/SimpleMath.h>
#include <vector>

#include <dxgi1_4.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <Psapi.h>
#include <string>

using namespace DirectX::SimpleMath;

class App :
	public GameApp
{
public:
	ID3D11Device* m_pDevice = nullptr;						// ����̽�	
	ID3D11DeviceContext* m_pDeviceContext = nullptr;		// ��� ����̽� ���ؽ�Ʈ
	IDXGISwapChain* m_pSwapChain = nullptr;					// ����ü��
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;	// ������ Ÿ�ٺ�

	Microsoft::WRL::ComPtr<IDXGIAdapter3> m_Adapter3; // VRAM ��ȸ��
	SIZE_T m_VideoMemoryTotal = 0; // �� VRAM ����Ʈ
	ID3D11RasterizerState* m_pRasterState = nullptr; // �����Ͷ����� ����

	// �ý��� ���� (ǥ�ÿ�)
	std::wstring m_GPUName;
	std::wstring m_CPUName;
	UINT m_CPUCores = 0;
	// FPS ��� ĳ��
	float m_LastFps = 0.0f;
	float m_FpsAccum = 0.0f;
	float m_FpsTimer = 0.0f;

	// �ý��� �޸�(����Ʈ)
	ULONGLONG m_RamTotal = 0;
	ULONGLONG m_RamAvail = 0;

	/*
	* @brief : UI ���� �Ķ����(��Ʈ/ī�޶�)
	* @details :
	* 	- RootPos : �� ��Ʈ�� ���� ��ġ
	* 	- Camera : ��/�������� ������ ���
	*/
	// ImGui ��Ʈ�� ���� ����
	DirectX::XMFLOAT3 m_RootPos = { -1.5f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_CameraPos = { 0.0f, 5.0f, -15.0f };
	float m_CameraFovDeg = 90.0f;
	float m_CameraNear = 1.0f;
	float m_CameraFar = 1000.0f;
 
	// �ؽ��� ���ҽ� (Hanako/Yuuka)
	ID3D11ShaderResourceView* m_TexHanakoSRV = nullptr;
	ID3D11ShaderResourceView* m_TexYuukaSRV = nullptr;
	bool m_ShowHanako = false;
	bool m_ShowYuuka = false;
	ImVec2 m_TexHanakoSize = ImVec2(0,0);
	ImVec2 m_TexYuukaSize = ImVec2(0,0);
	ImVec2 m_HanakoDrawSize = ImVec2(128, 128);
	ImVec2 m_YuukaDrawSize = ImVec2(128, 128);
	bool m_HanakoLockAR = true;
	bool m_YuukaLockAR = true;
	bool m_HanakoFitToWindow = false;
	bool m_YuukaFitToWindow = false;

	// �ӽ� �˸�(���� ����) ��������
	float m_NoticeTimeLeft = 0.0f;
	std::string m_NoticeText;

	// ���� �ε� �̹���(�� ��) ��� ����
	ID3D11ShaderResourceView* m_LoadedSRV = nullptr;
	ImVec2 m_LoadedSize = ImVec2(0,0);
	ImVec2 m_LoadedDrawSize = ImVec2(512,512);
	bool m_ShowLoaded = false;
	bool m_LoadedLockAR = true;
	bool m_LoadedFitToWindow = false;
	std::string m_LoadedTitle;

	// UI ���� ���°�
	float m_ExampleFloat = 0.5f;
	int   m_ExampleInt = 50;
	bool  m_ExampleToggle = true;
	int   m_ExampleCombo = 0;
	ImVec4 m_ExampleColor = ImVec4(0.4f, 0.7f, 0.0f, 1.0f);
	char  m_TextBuffer[128] = { 0 };

	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate(const float& dt) override;
	void OnRender() override;

	bool InitD3D();
	void UninitD3D();

	bool InitScene();							// ���̴�,���ؽ�,�ε���
	void UninitScene();

private:
	bool InitEffect();							// ���̴��� �о���� �Լ��� ���� ����
};

