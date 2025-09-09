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
	ID3D11Device* m_pDevice = nullptr;						// 디바이스	
	ID3D11DeviceContext* m_pDeviceContext = nullptr;		// 즉시 디바이스 컨텍스트
	IDXGISwapChain* m_pSwapChain = nullptr;					// 스왑체인
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;	// 렌더링 타겟뷰

	Microsoft::WRL::ComPtr<IDXGIAdapter3> m_Adapter3; // VRAM 조회용
	SIZE_T m_VideoMemoryTotal = 0; // 총 VRAM 바이트
	ID3D11RasterizerState* m_pRasterState = nullptr; // 래스터라이저 상태

	// 시스템 정보 (표시용)
	std::wstring m_GPUName;
	std::wstring m_CPUName;
	UINT m_CPUCores = 0;
	// FPS 출력 캐시
	float m_LastFps = 0.0f;
	float m_FpsAccum = 0.0f;
	float m_FpsTimer = 0.0f;

	// 시스템 메모리(바이트)
	ULONGLONG m_RamTotal = 0;
	ULONGLONG m_RamAvail = 0;

	/*
	* @brief : UI 조작 파라미터(루트/카메라)
	* @details :
	* 	- RootPos : 모델 루트의 월드 위치
	* 	- Camera : 뷰/프로젝션 구성에 사용
	*/
	// ImGui 컨트롤 상태 변수
	DirectX::XMFLOAT3 m_RootPos = { -1.5f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_CameraPos = { 0.0f, 5.0f, -15.0f };
	float m_CameraFovDeg = 90.0f;
	float m_CameraNear = 1.0f;
	float m_CameraFar = 1000.0f;
 
	// 텍스쳐 리소스 (Hanako/Yuuka)
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

	// 임시 알림(오류 포함) 오버레이
	float m_NoticeTimeLeft = 0.0f;
	std::string m_NoticeText;

	// 별도 로드 이미지(한 장) 뷰어 상태
	ID3D11ShaderResourceView* m_LoadedSRV = nullptr;
	ImVec2 m_LoadedSize = ImVec2(0,0);
	ImVec2 m_LoadedDrawSize = ImVec2(512,512);
	bool m_ShowLoaded = false;
	bool m_LoadedLockAR = true;
	bool m_LoadedFitToWindow = false;
	std::string m_LoadedTitle;

	// UI 예제 상태값
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

	bool InitScene();							// 쉐이더,버텍스,인덱스
	void UninitScene();

private:
	bool InitEffect();							// 쉐이더를 읽어오는 함수는 따로 구현
};

