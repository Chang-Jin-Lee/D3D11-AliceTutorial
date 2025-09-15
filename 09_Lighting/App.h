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

	DirectX::XMFLOAT4 color;
	DirectX::XMFLOAT3 direction;
	float pad; // 구조 크기가 16의 배수를 충족 시키도록 부동 소수점 번호를 채우기

};

class App :
	public GameApp
{
public:

	struct LightVertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT4 color;
		static const D3D11_INPUT_ELEMENT_DESC inputLayout[3];
	};

	struct ConstantBuffer
	{
		DirectX::XMMATRIX world;
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX proj;
		DirectX::XMMATRIX worldInvTranspose;

		DirectionalLight dirLight;
		DirectX::XMFLOAT3 eyePos;
		float pad;
	};

public:
	// 렌더링 파이프라인을 구성하는 필수 객체의 인터페이스 (  뎊스 스텐실 뷰도 있지만 아직 사용하지 않는다.)
	ID3D11Device* m_pDevice = nullptr; 						// 디바이스	
	ID3D11DeviceContext* m_pDeviceContext = nullptr; 		// 즉시 디바이스 컨텍스트
	IDXGISwapChain* m_pSwapChain = nullptr; 					// 스왑체인
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr; 	// 렌더링 타겟뷰

	// 렌더링 파이프라인에 적용하는  객체와 정보
	ID3D11VertexShader* m_pVertexShader = nullptr; 	// 정점 셰이더.
	ID3D11PixelShader* m_pPixelShader = nullptr; 	// 픽셀 셰이더.	
	ID3D11PixelShader* m_pPixelShaderSolid = nullptr; 	// 라이트 표식용 솔리드 PS
	ID3D11InputLayout* m_pInputLayout = nullptr; 	// 입력 레이아웃.
	ID3D11Buffer* m_pVertexBuffer = nullptr; 		// 버텍스 버퍼.
	UINT m_VertextBufferStride = 0; 					// 버텍스 하나의 크기.
	UINT m_VertextBufferOffset = 0; 					// 버텍스 버퍼의 오프셋.
	ID3D11Buffer* m_pIndexBuffer = nullptr; 			// 버텍스 버퍼.
	int m_nIndices = 0; 								// 인덱스 개수.

	ID3D11Buffer* m_pConstantBuffer = nullptr; 				// 상수 버퍼 (단일)
	ConstantBuffer m_ConstantBuffer; 						// CPU-side 상수 버퍼 데이터

	ID3D11DepthStencilView* m_pDepthStencilView; 	// 깊이 템플릿
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr; 	// 깊이 스텐실 상태
	Microsoft::WRL::ComPtr<IDXGIAdapter3> m_Adapter3; // VRAM 조회용
	SIZE_T m_VideoMemoryTotal = 0; // 총 VRAM 바이트

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
	DirectX::XMFLOAT3 m_cubePos = { -1.5f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_cameraPos = { 0.0f, 5.0f, -15.0f };
	float m_CameraFovDeg = 90.0f;
	float m_CameraNear = 1.0f;
	float m_CameraFar = 1000.0f;

	float m_YawDeg = 0.0f;
	float m_PitchDeg = 0.0f;
	DirectX::XMFLOAT3 m_LightDirection = { 0.0f, -1.0f, 1.0f };
	DirectX::XMFLOAT3 m_LightColorRGB = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 m_LightPosition = { 0.0f, 0.0f, 3.0f }; // 디버그용 라이트 위치

	ConstantBuffer m_baseProjection;

	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate(const float& dt) override;
	void OnRender() override;

	bool InitD3D();
	void UninitD3D();

	bool InitScene(); 							// 쉐이더,버텍스,인덱스
	void UninitScene();

private:
	bool InitEffect(); 							// 쉐이더를 읽어오는 함수는 따로 구현
};

