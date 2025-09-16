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
#include <map>

class MinimalUserModel; // App.cpp에 구현

using namespace DirectX::SimpleMath;

class App :
	public GameApp
{
public:
	struct VertexPosColor
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT4 color;
		static const D3D11_INPUT_ELEMENT_DESC inputLayout[2];
	};

	struct VertexPosTex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT2 tex;
		static const D3D11_INPUT_ELEMENT_DESC inputLayout[2];
	};

	struct ConstantBuffer
	{
		DirectX::XMMATRIX world;
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX proj;
	};

public:
	// 렌더링 파이프라인을 구성하는 필수 객체의 인터페이스 (  뎊스 스텐실 뷰도 있지만 아직 사용하지 않는다.)
	ID3D11Device* m_pDevice = nullptr;						// 디바이스	
	ID3D11DeviceContext* m_pDeviceContext = nullptr;		// 즉시 디바이스 컨텍스트
	IDXGISwapChain* m_pSwapChain = nullptr;					// 스왑체인
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;	// 렌더링 타겟뷰

	// 렌더링 파이프라인에 적용하는  객체와 정보
	ID3D11VertexShader* m_pVertexShader = nullptr;	// 정점 셰이더.
	ID3D11PixelShader* m_pPixelShader = nullptr;	// 픽셀 셰이더.	
	ID3D11InputLayout* m_pInputLayout = nullptr;	// 입력 레이아웃.
	ID3D11Buffer* m_pVertexBuffer = nullptr;		// 버텍스 버퍼.
	UINT m_VertextBufferStride = 0;					// 버텍스 하나의 크기.
	UINT m_VertextBufferOffset = 0;					// 버텍스 버퍼의 오프셋.
	ID3D11Buffer* m_pIndexBuffer = nullptr;			// 버텍스 버퍼.
	int m_nIndices = 0;								// 인덱스 개수.

	ID3D11Buffer* m_pConstantBuffer;				// 상수 버퍼
	//ConstantBuffer m_CBuffer;                       // GPU 상수 버퍼를 수정하는 데 사용되는 변수
	std::vector<ConstantBuffer> m_CBuffers;
	ID3D11DepthStencilView* m_pDepthStencilView;    // 깊이 템플릿
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;	// 깊이 스텐실 상태
	ID3D11SamplerState* m_pSamplerState = nullptr;
	ID3D11ShaderResourceView* m_pTextureSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	Microsoft::WRL::ComPtr<IDXGIAdapter3> m_Adapter3; // VRAM 조회용
	SIZE_T m_VideoMemoryTotal = 0; // 총 VRAM 바이트

	// Live2D 간단 렌더(텍스처 미리보기 용)
	std::wstring m_L2DModelJsonPath; // 선택한 model3.json 전체 경로
	bool m_L2DRequested = false;     // 사용자가 파일을 선택했는지
	bool m_L2DReady = false;         // Live2D Core/Framework 준비 여부
	std::string m_L2DStatus;         // 간단 상태 텍스트
	std::vector<ID3D11ShaderResourceView*> m_L2DTexSRVs; // 모델 텍스처들
	std::vector<ImVec2> m_L2DTexSizes;                   // 텍스처 크기(px)
	bool m_ShowL2DWindow = true;                         // Live2D 미리보기 창 on/off

	// Live2D 실제 모델/렌더 상태
	MinimalUserModel* m_L2D = nullptr;                   // 모델 핸들
	bool m_L2DLoaded = false;                            // 모델 로드 여부
	int m_L2DMotionGroupIndex = 0;                       // UI: 그룹 선택
	int m_L2DMotionIndex = 0;                            // UI: 그룹 내 모션 선택
	std::vector<std::string> m_L2DMotionGroups;          // 그룹 이름 캐시
	bool m_ShowL2DInfo = true;                           // Live2D 정보창 on/off

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

	// ImGui 컨트롤 상태 변수
	DirectX::XMFLOAT3 m_RootPos = { -1.5f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_Child1Offset = { 8.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_Child2Offset = { 3.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_CameraPos = { 0.0f, 5.0f, -15.0f };
	float m_CameraFovDeg = 90.0f;
	float m_CameraNear = 1.0f;
	float m_CameraFar = 1000.0f;

	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate(const float& dt) override;
	void OnRender() override;

	bool InitD3D();
	void UninitD3D();

	bool InitScene();								// 쉐이더,버텍스,인덱스
	void UninitScene();

private:
	bool InitEffect();								// 쉐이더를 읽어오는 함수는 따로 구현
};

