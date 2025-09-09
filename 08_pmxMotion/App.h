#pragma once
#include <windows.h>
#include "../Common/GameApp.h"
#include <d3d11.h>
#include <directxtk/SimpleMath.h>
#include <vector>

#include <thread>
#include <atomic>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <Psapi.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string>

using namespace DirectX::SimpleMath;

class App :
	public GameApp
{
public:
	/*
	* @brief : 정점/상수 버퍼 데이터 형식 정의
	* @details :
	* 	- VertexPosTex : POSITION(float3) + TEXCOORD(float2)
	* 	- ConstantBuffer : 월드/뷰/프로젝션 행렬(HLSL과 전치행렬 일치 주의)
	*/
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
		uint16_t boneIndex[4] = {0,0,0,0};
		DirectX::XMFLOAT4 boneWeight = {1,0,0,0};
		static const D3D11_INPUT_ELEMENT_DESC inputLayout[2];
	};

	struct ConstantBuffer
	{
		DirectX::XMMATRIX world;
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX proj;
	};

public:
	/*
	* @brief : D3D11 핵심 객체들(렌더 파이프라인 자원)
	* @details :
	* 	- Device/Context/SwapChain/RTV : 기본 출력 경로
	* 	- VS/PS/InputLayout : 셰이더 및 입력 시그니처
	* 	- VB/IB/CB : 메시와 카메라/변환 데이터
	*/
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

	ID3D11Buffer* m_pConstantBuffer;                // 상수 버퍼
	ConstantBuffer m_CBuffer;                       // 단일 모델용 상수 버퍼
	ID3D11Buffer* m_pBoneCB = nullptr;             // 본 행렬 상수 버퍼 (b1)
	ID3D11DepthStencilView* m_pDepthStencilView;    // 깊이 템플릿
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;	// 깊이 스텐실 상태
	ID3D11SamplerState* m_pSamplerState = nullptr;
	ID3D11ShaderResourceView* m_pTextureSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
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

	/*
	* @brief : PMX 로딩 상태
	* @details :
	* 	- m_ModelPath : 읽을 PMX 경로
	* 	- m_ModelVertices/Indices : 병합된 메시 데이터(VB/IB 업로드용)
	*/
	// PMX 로딩 상태
	std::wstring m_ModelPath = L"../Resource/Nikke-Alice/alice-Apose.pmx";
	std::vector<VertexPosTex> m_ModelVertices;
	std::vector<uint32_t> m_ModelIndices;
	struct Subset { uint32_t startIndex; uint32_t indexCount; uint32_t materialIndex; };
	std::vector<Subset> m_Subsets;                       // 머티리얼별 드로우 서브셋
	std::vector<ID3D11ShaderResourceView*> m_MaterialSRVs; // 머티리얼별 텍스처 SRV(디퓨즈 우선)
	ID3D11ShaderResourceView* m_pWhiteSRV = nullptr;        // 기본 흰색 텍스처 SRV

	// 스켈레톤/스키닝 상태
	struct Bone
	{
		std::wstring name;
		int nodeIndex = -1;
		DirectX::XMMATRIX offset; // inverse bind pose
	};
	std::vector<Bone> m_Bones;                 // aiBone 순서대로
	std::vector<DirectX::XMMATRIX> m_BoneFinalMatrices; // 업로드용
	std::vector<DirectX::XMMATRIX> m_NodeLocalBind;     // 노드 바인드 로컬
	std::vector<DirectX::XMMATRIX> m_NodeLocalAnim;     // 애니 로컬(없으면 바인드)
	std::vector<DirectX::XMMATRIX> m_NodeGlobal;        // 노드 글로벌
	std::vector<int> m_NodeParent;                      // 부모 인덱스
	std::vector<std::wstring> m_NodeName;               // 노드 이름

	// VMD 재생 상태
	std::wstring m_MotionPath = L"../Resource/Motion/RabbitHole.vmd";
	struct VmdKey
	{
		uint32_t frame;
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT4 rot; // quaternion (x,y,z,w)
	};
	std::unordered_map<std::wstring, std::vector<VmdKey>> m_VmdTracks; // 본 이름 → 키
	std::vector<VmdKey> m_CenterKeys; // 간단 모션: 센터 본 키만 사용
	float m_PlayTime = 0.0f;   // 초
	float m_PlaySpeed = 1.0f;  // 배속
	bool m_Loop = true;
	uint32_t m_AnimMaxFrame = 0; // VMD 최대 프레임
	DirectX::XMMATRIX m_CenterMotion = DirectX::XMMatrixIdentity(); // 센터 본 모션(간단 적용)
 
	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate(const float& dt) override;
	void OnRender() override;

	bool InitD3D();
	void UninitD3D();

	/*
	* @brief : 장면 초기화/해제
	* @details :
	* 	- InitScene : PMX 로드 → 노드 계층 병합 → VB/IB/CB 생성
	* 	- UninitScene : 생성한 리소스 해제
	*/
	bool InitScene();							// 쉐이더,버텍스,인덱스
	void UninitScene();
 
 private:
 	bool LoadPMXWithSkinning();
 	void BuildNodeHierarchy(const aiScene* scene);
 	void UpdateAnimation(float dt);
 	bool LoadVMD(const std::wstring& path);

	/*
	* @brief : 셰이더/입력 레이아웃 초기화
	* @details :
	* 	- VS/PS 컴파일 및 객체 생성
	* 	- InputLayout을 POSITION/TEXCOORD로 구성
	*/
	bool InitEffect();								// 쉐이더를 읽어오는 함수는 따로 구현
};

