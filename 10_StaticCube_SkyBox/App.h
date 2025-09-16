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

// 방향성 라이트(디퓨즈 전용 간소 버전)
struct DirectionalLight
{
	DirectionalLight() = default;

	DirectionalLight(const DirectionalLight&) = default;
	DirectionalLight& operator=(const DirectionalLight&) = default;

	DirectionalLight(DirectionalLight&&) = default;
	DirectionalLight& operator=(DirectionalLight&&) = default;

	DirectionalLight( const DirectX::XMFLOAT4& _color,
		const DirectX::XMFLOAT3& _direction) :
		 direction(_direction), color(_color), pad() {
	}

	DirectX::XMFLOAT4 color;      // 라이트 색상 (RGBA)
	DirectX::XMFLOAT3 direction;  // 라이트 방향 (정규화 권장)
	float pad; // HLSL cbuffer 16바이트 정렬용 패딩

};

class App :
	public GameApp
{
public:

	// 정점 레이아웃: POSITION/NORMAL/COLOR
	struct LightVertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT4 color;
		static const D3D11_INPUT_ELEMENT_DESC inputLayout[3];
	};

	// VS/PS 공용 상수버퍼(b0)
	struct ConstantBuffer
	{
		DirectX::XMMATRIX world;               // 월드 행렬(전치로 저장)
		DirectX::XMMATRIX view;                // 뷰 행렬(전치로 저장)
		DirectX::XMMATRIX proj;                // 프로젝션 행렬(전치로 저장)
		DirectX::XMMATRIX worldInvTranspose;   // (World^-1)^T

		DirectionalLight dirLight;             // 방향성 라이트(색/방향)
		DirectX::XMFLOAT3 eyePos;              // 카메라 위치
		float pad;                             // 디버그 토글 등에 사용
	};

public:
	// 필수 D3D 객체
	ID3D11Device* m_pDevice = nullptr; 						// 디바이스	
	ID3D11DeviceContext* m_pDeviceContext = nullptr; 		// 즉시 디바이스 컨텍스트
	IDXGISwapChain* m_pSwapChain = nullptr; 					// 스왑체인
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr; 	// 렌더 타겟뷰

	// 파이프라인 리소스
	ID3D11VertexShader* m_pVertexShader = nullptr; 	// 정점 셰이더
	ID3D11PixelShader* m_pPixelShader = nullptr; 	// 픽셀 셰이더(조명)	
	ID3D11PixelShader* m_pPixelShaderSolid = nullptr; 	// 픽셀 셰이더(마커용 흰색)

	ID3D11SamplerState* m_pSamplerState = nullptr;
	ID3D11VertexShader* m_pSkyBoxVertexShader = nullptr; 	// 스카이박스 정점 셰이더
	ID3D11PixelShader* m_pSkyBoxPixelShader = nullptr; 	// 스카이박스 픽셀 셰이더(조명)	
	ID3D11InputLayout* m_pSkyBoxInputLayout = nullptr; 	// 스카이박스 입력 레이아웃
	ID3D11ShaderResourceView* m_pTextureSRV = nullptr;
	// SkyBox 선택: Hanako.dds / cubemap.dds
	enum class SkyBoxChoice { Hanako = 0, CubeMap = 1 };
	SkyBoxChoice m_SkyBoxChoice = SkyBoxChoice::Hanako;
	ID3D11ShaderResourceView* m_pSkyHanakoSRV = nullptr;
	ID3D11ShaderResourceView* m_pSkyCubeMapSRV = nullptr;
	
	ID3D11InputLayout* m_pInputLayout = nullptr; 	// 입력 레이아웃
	ID3D11Buffer* m_pVertexBuffer = nullptr; 		// 버텍스 버퍼
	UINT m_VertextBufferStride = 0; 					// 버텍스 하나의 크기
	UINT m_VertextBufferOffset = 0; 					// 버텍스 버퍼 오프셋
	ID3D11Buffer* m_pIndexBuffer = nullptr; 			// 인덱스 버퍼
	int m_nIndices = 0; 								// 인덱스 개수

	ID3D11Buffer* m_pConstantBuffer = nullptr; 				// 상수 버퍼 (단일)
	ConstantBuffer m_ConstantBuffer; 						// CPU-side 상수 버퍼 데이터
	ID3D11Buffer* m_pLineVertexBuffer = nullptr; 			// 라이트 방향 표시용 라인 VB

	ID3D11DepthStencilView* m_pDepthStencilView; 	// 깊이 스텐실 뷰
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;  	// 깊이 스텐실 상태
	Microsoft::WRL::ComPtr<IDXGIAdapter3> m_Adapter3; // VRAM 조회용
	SIZE_T m_VideoMemoryTotal = 0; // 총 VRAM 바이트

	ID3D11RasterizerState*  RSNoCull;			            // 레스터 라이저 상태 : 백 클리핑 모드 없음
	ID3D11RasterizerState* RSCullClockWise;					// 레스터 라이저 상태 : 시계 방향 자르기 모드

	// 이미지 디버그/데모용 텍스처 (Hanako)
	ID3D11ShaderResourceView* m_TexHanakoSRV = nullptr;
	bool m_ShowHanako = false;
	ImVec2 m_HanakoDrawSize = ImVec2(128, 128);
	ImVec2 m_TexHanakoSize = ImVec2(0, 0);

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
	DirectX::XMFLOAT3 m_cubePos = { -1.5f, 0.0f, 0.0f }; // 루트 변위
	DirectX::XMFLOAT3 m_cameraPos = { 0.0f, 5.0f, -15.0f }; // 카메라 위치
	float m_CameraFovDeg = 90.0f;   // FOV(deg)
	float m_CameraNear = 1.0f;      // Near
	float m_CameraFar = 1000.0f;    // Far

	float m_YawDeg = 0.0f;          // 큐브 Yaw
	float m_PitchDeg = 0.0f;        // 큐브 Pitch
	DirectX::XMFLOAT3 m_LightDirection = { 0.0f, -1.0f, 1.0f }; // 라이트 방향(UI)
	DirectX::XMFLOAT3 m_LightColorRGB = { 1.0f, 1.0f, 1.0f };   // 라이트 색(UI)
	DirectX::XMFLOAT3 m_LightPosition = { 0.0f, 0.0f, 3.0f };   // 라이트 위치(마커용)
	DirectX::XMFLOAT3 m_CameraForward = { 0.0f, 0.0f, 1.0f };   // 카메라 앞방향(스카이박스용)

	ConstantBuffer m_baseProjection; // 기본 카메라/월드 캐시

	// 수명주기
	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate(const float& dt) override;
	void OnRender() override;

	// D3D 장치 생성/파괴
	bool InitD3D();
	void UninitD3D();

	// 리소스 생성/파괴
	bool InitScene(); 							// 쉐이더,버텍스,인덱스
	void UninitScene();

private:
	bool InitBasicEffect(); 							// 쉐이더를 읽어오는 함수는 따로 구현
	bool InitSkyBoxEffect();
};

