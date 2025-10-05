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
#include <imgui_stdlib.h>
#include <Psapi.h>
#include <string>
#include "../Common/SystemInfomation.h"
#include "../Common/FbxManager.h"
#include "../Common/ObjManager.h"
#include "../Common/PmxManager.h"

using namespace DirectX::SimpleMath;

// 방향 라이트
struct DirectionalLight
{
	DirectX::XMFLOAT4 ambient;									// 환경광
	DirectX::XMFLOAT4 diffuse;									// 확산 반사광, 난반사
	DirectX::XMFLOAT4 specular;									// 직접 반사광
	DirectX::XMFLOAT3 direction;								// 라이트 방향
	float pad;													// HLSL cbuffer 16바이트 정렬용 패딩
};

// 물질의 재질
struct Material
{
	DirectX::XMFLOAT4 ambient;
	DirectX::XMFLOAT4 diffuse;
	DirectX::XMFLOAT4 specular;
	DirectX::XMFLOAT4 reflect;
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
		DirectX::XMMATRIX world;								// 월드 행렬(전치로 저장)
		DirectX::XMMATRIX view;								// 뷰 행렬(전치로 저장)
		DirectX::XMMATRIX proj;								// 프로젝션 행렬(전치로 저장)
		DirectX::XMMATRIX worldInvTranspose;					// (World^-1)^T (비균등 스케일)

		Material material;									// 재질
		DirectionalLight dirLight;							// 방향성 라이트(색/방향)
		DirectX::XMFLOAT3 eyePos;							// 카메라 위치
		float pad;											// 디버그 토글 등에 사용
		int   shadingMode = 0;                      // 기본 Phong
		DirectX::XMFLOAT3 pad2 = {0,0,0};
		int   enableNormalMap = 1;
		DirectX::XMFLOAT3 pad3 = {0,0,0};
		int   useSpecularMap = 0;            // 15와 맞추기: 기본 미사용
		DirectX::XMFLOAT3 pad4 = {0,0,0};
	};

public:
	// 필수 D3D 객체
	ID3D11Device* m_pDevice = nullptr;							// 디바이스	
	ID3D11DeviceContext* m_pDeviceContext = nullptr;			// 즉시 디바이스 컨텍스트
	IDXGISwapChain* m_pSwapChain = nullptr;					// 스왑체인
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;		// 렌더 타겟뷰

	// 파이프라인 리소스
	ID3D11VertexShader* m_pVertexShader = nullptr;					// 정점 셰이더
	ID3D11PixelShader* m_pPixelShader = nullptr;				// 픽셀 셰이더(조명)	
	ID3D11PixelShader* m_pPixelShaderSolid = nullptr;			// 픽셀 셰이더(마커용 흰색)
	// PMX 전용: TBN 없는 입력용 VS/IL
	ID3D11VertexShader* m_pVertexShaderNoTBN = nullptr;
	ID3D11InputLayout* m_pInputLayoutNoTBN = nullptr;

	ID3D11SamplerState* m_pSamplerState = nullptr;
	ID3D11BlendState* m_pAlphaBlendState = nullptr;					// 알파 블렌딩 상태
	ID3D11VertexShader* m_pSkyBoxVertexShader = nullptr;		// 스카이박스 정점 셰이더
	ID3D11PixelShader* m_pSkyBoxPixelShader = nullptr;			// 스카이박스 픽셀 셰이더(조명)	
	ID3D11InputLayout* m_pSkyBoxInputLayout = nullptr;			// 스카이박스 입력 레이아웃
	ID3D11ShaderResourceView* m_pTextureSRV = nullptr;

	// Debug lines 전용 VS/InputLayout (POSITION,NORMAL,COLOR 전용)
	ID3D11VertexShader* m_pLineVS = nullptr;
	ID3D11InputLayout* m_pLineInputLayout = nullptr;

	// SkyBox 선택: Off / Hanako.dds / cubemap.dds
	enum class SkyBoxChoice { Off = 0, Hanako = 1, CubeMap = 2 };
	SkyBoxChoice m_SkyBoxChoice = SkyBoxChoice::CubeMap;
	ID3D11ShaderResourceView* m_pSkyHanakoSRV = nullptr;
	ID3D11ShaderResourceView* m_pSkyCubeMapSRV = nullptr;
	
	ID3D11InputLayout* m_pInputLayout = nullptr;					// 입력 레이아웃
	ID3D11Buffer* m_pVertexBuffer = nullptr;						// 버텍스 버퍼
	UINT m_VertextBufferStride = 0;									// 버텍스 하나의 크기
	UINT m_VertextBufferOffset = 0;									// 버텍스 버퍼 오프셋
	ID3D11Buffer* m_pIndexBuffer = nullptr;							// 인덱스 버퍼
	int m_nIndices = 0;												// 인덱스 개수

	ID3D11Buffer* m_pConstantBuffer = nullptr;						// 상수 버퍼 (단일)
	std::vector<ConstantBuffer> m_CBuffers;							// 텍스쳐들 그리기 위한 상수 버퍼 캐시
	ConstantBuffer m_ConstantBuffer;								// CPU-side 상수 버퍼 데이터
	ID3D11Buffer* m_pLineVertexBuffer = nullptr;					// 라이트 방향 표시용 라인 VB

    // 분리된 유틸들
    class LineRenderer* m_LineRenderer = nullptr;					// 선/좌표축 렌더러
    class Skybox* m_Skybox = nullptr;								// 스카이박스
    // 디버그 박스 버퍼
    ID3D11Buffer* m_pDebugBoxVB = nullptr;
    ID3D11Buffer* m_pDebugBoxIB = nullptr;
    int m_DebugBoxIndexCount = 0;

	ID3D11DepthStencilView* m_pDepthStencilView;					// 깊이 스텐실 뷰
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;		// 깊이 스텐실 상태

	ID3D11RasterizerState*  RSNoCull; 								// 레스터 라이저 상태 : 백 클리핑 모드 없음
	ID3D11RasterizerState* RSCullClockWise;							// 레스터 라이저 상태 : 시계 방향 자르기 모드

	// 이미지 디버그/데모용 텍스처 (Hanako)
	ID3D11ShaderResourceView* m_TexHanakoSRV = nullptr;
	bool m_ShowHanako = false;
	ImVec2 m_HanakoDrawSize = ImVec2(128, 128);
	ImVec2 m_TexHanakoSize = ImVec2(0, 0);

	// 스카이박스 큐브맵 면 SRV들 (0:+X,1:-X,2:+Y,3:-Y,4:+Z,5:-Z)
	// ImGui에서 현재 카메라가 보고 있는 스카이 박스의 이미지를 그려주기 위한 변수들
	ID3D11ShaderResourceView* m_pSkyFaceSRV[6] = {};
	ImVec2 m_SkyFaceSize = ImVec2(0, 0);

	// Skybox current dds (path)
	wchar_t m_CurrentSkyboxPath[260] = L"..\\Resource\\Skybox\\cubemap.dds";

	// 텍스쳐 
	ID3D11ShaderResourceView* m_pCubeTextureSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	ID3D11ShaderResourceView* m_pNormalSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	ID3D11ShaderResourceView* m_pSpecularSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

	// ImGui 컨트롤 상태 변수
	SystemInfomation m_SystemInfo;
	Camera m_camera;													// 카메라
	DirectX::XMFLOAT3 m_modelPos = { 0.0f, 0.0f, 0.0f };				// 모델 루트 위치
	DirectX::XMFLOAT3 m_modelScale = { 1.0f, 1.0f, 1.0f };				// 모델 스케일
	DirectX::XMFLOAT3 m_modelRotation = { 0.0f, 0.0f, 0.0f };			// 모델 회전(Yaw/Pitch/Roll, deg)
	bool m_RotateModel = false;											// ImGui 토글: 모델 자동 회전 on/off

	// Mirror Cube transform (참고: 기본 큐브와 동일한 구성)
	DirectX::XMFLOAT3 m_mirrorCubePos = { 4.5f, 0.0f, 0.0f };			// 거울 큐브 위치 (x+8 기본)
	DirectX::XMFLOAT3 m_mirrorCubeRotation = { 0.0f, 0.0f, 0.0f };		// 거울 큐브 회전(Yaw/Pitch/Roll, deg)
	float m_MirrorCubeScale = 2.0f;										// 거울 큐브 스케일

	// DirectionalLight
	DirectionalLight m_DirLight = {
		/*ambient*/ { 0.0f, 0.0f, 0.0f, 1.0f },
		/*diffuse*/ { 1.0f, 1.0f, 1.0f, 1.0f },
		/*specular*/{ 1.0f, 1.0f, 1.0f, 1.0f },
		/*direction*/{ 0.0f, 0.0f, 1.0f },
		/*pad*/ 0.0f
	};
	// Material
	Material m_Material = {
		/*ambient*/ { 1.0f, 1.0f, 1.0f, 1.0f },
		/*diffuse*/ { 1.0f, 1.0f, 1.0f, 1.0f },
		/*specular*/{ 1.0f, 1.0f, 1.0f, 32.0f },						// w = shininess
		/*reflect*/ { 0.0f, 0.0f, 0.0f, 0.0f }
	};
	// Mirror Cube Material (메탈릭 거울 느낌)
	Material m_mirrorCubeMaterial = {
		/*ambient*/ { 0.0f, 0.0f, 0.0f, 1.0f },
		/*diffuse*/ { 0.0f, 0.0f, 0.0f, 1.0f },
		/*specular*/{ 0.0f, 0.0f, 0.0f, 32.0f },
		/*reflect*/ { 1.0f, 1.0f, 1.0f, 0.02f }							// a=roughness (작을수록 샤프)
	};

	DirectX::XMFLOAT3 m_LightPosition = { 4.0f, 4.0f, 0.0f };			// 라이트 위치(마커용)

	ConstantBuffer m_baseProjection;									// 기본 카메라/월드 캐시

	// Shader selection (like 15)
	enum class ShadingMode { Phong=0, BlinnPhong=1, Lambert=2, Unlit=3, TextureOnly=4 };
	ShadingMode m_ShadingMode = ShadingMode::Phong;
	int m_EnableNormalMap = 1;
	int m_UseSpecularMap = 0;
	int m_LegacyShading = 1;
	
	// 배경 색상 (스카이박스 Off일 때 Clear 색)
	DirectX::XMFLOAT4 m_ClearColor = { 0.02f, 0.02f, 0.02f, 1.0f };
 
	// 수명주기
	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate(const float& dt) override;
	void OnRender() override;

	// D3D 장치 생성/파괴
	bool InitD3D();
	void UninitD3D();

	// 리소스 생성/파괴
	// 쉐이더,버텍스,인덱스
	bool InitScene(); 										
	void UninitScene();

	// 텍스쳐 관련
	bool InitTexture();

	//ImGui 관련
	bool InitImGui();

private:
	// 쉐이더를 읽어오는 함수는 따로 구현
	bool InitBasicEffect(); 									
	bool InitSkyBoxEffect();

	// 큐브맵 면 SRV 준비/정리 및 면 선택 계산
	void PrepareSkyFaceSRVs();
	// Skybox change helper
	void ChangeSkyboxDDS(const wchar_t* ddsPath);

    // ---------------- Model loading (FBX/OBJ/PMX via Assimp) ----------------
public:
    enum class RenderMode { None = 0, Cube = 1, Model = 2 };
    RenderMode m_RenderMode = RenderMode::None;                 // 시작 시 아무것도 렌더링 안 함

    struct ModelSubset { uint32_t start; uint32_t count; uint32_t materialIndex; };

    // 모델 버퍼/서브셋/텍스처 상태
    ID3D11Buffer* m_pModelVB = nullptr;
    ID3D11Buffer* m_pModelIB = nullptr;
    int           m_ModelIndexCount = 0;
    UINT          m_ModelStride = 0;
    std::vector<ModelSubset> m_ModelSubsets;
    std::vector<ID3D11ShaderResourceView*> m_ModelMaterialSRVs; // DIFFUSE 전용

    // 폴백 텍스처들(흰색, 뉴트럴 노멀(0.5,0.5,1), 검정)
    ID3D11ShaderResourceView* m_pFallbackWhite  = nullptr;
    ID3D11ShaderResourceView* m_pFallbackNormal = nullptr;
    ID3D11ShaderResourceView* m_pFallbackBlack  = nullptr;

    // ImGui 입력 경로(UTF-8 문자열 사용 후 변환)
    std::string m_ModelPathInputUTF8;

    // 로더 API
    bool LoadModelFromFile(const std::wstring& pathW);
    void UnloadModel();

    // 매니저들 및 소스 구분
    FbxManager   m_FbxManager;   // 값 타입(포인터/ new 사용 안 함)
    ObjManager   m_ObjManager;
    PmxManager   m_PmxManager;
    enum class ModelSource { FBX, OBJ, PMX, Custom };
    ModelSource m_ModelSource = ModelSource::Custom;
};

