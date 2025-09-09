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
	* @brief : ����/��� ���� ������ ���� ����
	* @details :
	* 	- VertexPosTex : POSITION(float3) + TEXCOORD(float2)
	* 	- ConstantBuffer : ����/��/�������� ���(HLSL�� ��ġ��� ��ġ ����)
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
	* @brief : D3D11 �ٽ� ��ü��(���� ���������� �ڿ�)
	* @details :
	* 	- Device/Context/SwapChain/RTV : �⺻ ��� ���
	* 	- VS/PS/InputLayout : ���̴� �� �Է� �ñ״�ó
	* 	- VB/IB/CB : �޽ÿ� ī�޶�/��ȯ ������
	*/
	// ������ ������������ �����ϴ� �ʼ� ��ü�� �������̽� (  �X�� ���ٽ� �䵵 ������ ���� ������� �ʴ´�.)
	ID3D11Device* m_pDevice = nullptr;						// ����̽�	
	ID3D11DeviceContext* m_pDeviceContext = nullptr;		// ��� ����̽� ���ؽ�Ʈ
	IDXGISwapChain* m_pSwapChain = nullptr;					// ����ü��
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;	// ������ Ÿ�ٺ�

	// ������ ���������ο� �����ϴ�  ��ü�� ����
	ID3D11VertexShader* m_pVertexShader = nullptr;	// ���� ���̴�.
	ID3D11PixelShader* m_pPixelShader = nullptr;	// �ȼ� ���̴�.	
	ID3D11InputLayout* m_pInputLayout = nullptr;	// �Է� ���̾ƿ�.
	ID3D11Buffer* m_pVertexBuffer = nullptr;		// ���ؽ� ����.
	UINT m_VertextBufferStride = 0;					// ���ؽ� �ϳ��� ũ��.
	UINT m_VertextBufferOffset = 0;					// ���ؽ� ������ ������.
	ID3D11Buffer* m_pIndexBuffer = nullptr;			// ���ؽ� ����.
	int m_nIndices = 0;								// �ε��� ����.

	ID3D11Buffer* m_pConstantBuffer;                // ��� ����
	ConstantBuffer m_CBuffer;                       // ���� �𵨿� ��� ����
	ID3D11Buffer* m_pBoneCB = nullptr;             // �� ��� ��� ���� (b1)
	ID3D11DepthStencilView* m_pDepthStencilView;    // ���� ���ø�
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;	// ���� ���ٽ� ����
	ID3D11SamplerState* m_pSamplerState = nullptr;
	ID3D11ShaderResourceView* m_pTextureSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
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

	/*
	* @brief : PMX �ε� ����
	* @details :
	* 	- m_ModelPath : ���� PMX ���
	* 	- m_ModelVertices/Indices : ���յ� �޽� ������(VB/IB ���ε��)
	*/
	// PMX �ε� ����
	std::wstring m_ModelPath = L"../Resource/Nikke-Alice/alice-Apose.pmx";
	std::vector<VertexPosTex> m_ModelVertices;
	std::vector<uint32_t> m_ModelIndices;
	struct Subset { uint32_t startIndex; uint32_t indexCount; uint32_t materialIndex; };
	std::vector<Subset> m_Subsets;                       // ��Ƽ���� ��ο� �����
	std::vector<ID3D11ShaderResourceView*> m_MaterialSRVs; // ��Ƽ���� �ؽ�ó SRV(��ǻ�� �켱)
	ID3D11ShaderResourceView* m_pWhiteSRV = nullptr;        // �⺻ ��� �ؽ�ó SRV

	// ���̷���/��Ű�� ����
	struct Bone
	{
		std::wstring name;
		int nodeIndex = -1;
		DirectX::XMMATRIX offset; // inverse bind pose
	};
	std::vector<Bone> m_Bones;                 // aiBone �������
	std::vector<DirectX::XMMATRIX> m_BoneFinalMatrices; // ���ε��
	std::vector<DirectX::XMMATRIX> m_NodeLocalBind;     // ��� ���ε� ����
	std::vector<DirectX::XMMATRIX> m_NodeLocalAnim;     // �ִ� ����(������ ���ε�)
	std::vector<DirectX::XMMATRIX> m_NodeGlobal;        // ��� �۷ι�
	std::vector<int> m_NodeParent;                      // �θ� �ε���
	std::vector<std::wstring> m_NodeName;               // ��� �̸�

	// VMD ��� ����
	std::wstring m_MotionPath = L"../Resource/Motion/RabbitHole.vmd";
	struct VmdKey
	{
		uint32_t frame;
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT4 rot; // quaternion (x,y,z,w)
	};
	std::unordered_map<std::wstring, std::vector<VmdKey>> m_VmdTracks; // �� �̸� �� Ű
	std::vector<VmdKey> m_CenterKeys; // ���� ���: ���� �� Ű�� ���
	float m_PlayTime = 0.0f;   // ��
	float m_PlaySpeed = 1.0f;  // ���
	bool m_Loop = true;
	uint32_t m_AnimMaxFrame = 0; // VMD �ִ� ������
	DirectX::XMMATRIX m_CenterMotion = DirectX::XMMatrixIdentity(); // ���� �� ���(���� ����)
 
	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate(const float& dt) override;
	void OnRender() override;

	bool InitD3D();
	void UninitD3D();

	/*
	* @brief : ��� �ʱ�ȭ/����
	* @details :
	* 	- InitScene : PMX �ε� �� ��� ���� ���� �� VB/IB/CB ����
	* 	- UninitScene : ������ ���ҽ� ����
	*/
	bool InitScene();							// ���̴�,���ؽ�,�ε���
	void UninitScene();
 
 private:
 	bool LoadPMXWithSkinning();
 	void BuildNodeHierarchy(const aiScene* scene);
 	void UpdateAnimation(float dt);
 	bool LoadVMD(const std::wstring& path);

	/*
	* @brief : ���̴�/�Է� ���̾ƿ� �ʱ�ȭ
	* @details :
	* 	- VS/PS ������ �� ��ü ����
	* 	- InputLayout�� POSITION/TEXCOORD�� ����
	*/
	bool InitEffect();								// ���̴��� �о���� �Լ��� ���� ����
};

