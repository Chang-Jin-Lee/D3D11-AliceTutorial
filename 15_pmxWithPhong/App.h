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
#include "../Common/SystemInfomation.h"
#include "../Common/StaticMesh.h"
#include "../Common/PmxManager.h"

using namespace DirectX::SimpleMath;

// ���� ����Ʈ
struct DirectionalLight
{
	DirectX::XMFLOAT4 ambient;								// ȯ�汤
	DirectX::XMFLOAT4 diffuse;								// Ȯ�� �ݻ籤, ���ݻ�
	DirectX::XMFLOAT4 specular;								// ���� �ݻ籤
	DirectX::XMFLOAT3 direction;							// ����Ʈ ����
	float pad;												// HLSL cbuffer 16����Ʈ ���Ŀ� �е�
};

// ������ ����
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
	// ���̵� ���
	enum class ShadingMode : int { Phong = 0, BlinnPhong = 1, Lambert = 2, Unlit = 3, TextureOnly = 4 };

	// ���� ���̾ƿ�: POSITION/NORMAL/COLOR
	struct LightVertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT4 color;
		static const D3D11_INPUT_ELEMENT_DESC inputLayout[3];
	};

	// VS/PS ���� �������(b0)
	struct ConstantBuffer
	{
		DirectX::XMMATRIX world;								// ���� ���(��ġ�� ����)
		DirectX::XMMATRIX view;								// �� ���(��ġ�� ����)
		DirectX::XMMATRIX proj;								// �������� ���(��ġ�� ����)
		DirectX::XMMATRIX worldInvTranspose;					// (World^-1)^T (��յ� ������)

		Material material;									// ����
		DirectionalLight dirLight;							// ���⼺ ����Ʈ(��/����)
		DirectX::XMFLOAT3 eyePos;							// ī�޶� ��ġ
		float pad;											// ����� ��� � ���
		int shadingMode; 									// 0:Phong,1:Blinn,2:Lambert,3:Unlit
		DirectX::XMFLOAT3 pad2; 								// 16����Ʈ ����
	};

public:
	// �ʼ� D3D ��ü
	ID3D11Device* m_pDevice = nullptr; 							// ����̽�	
	ID3D11DeviceContext* m_pDeviceContext = nullptr; 			// ��� ����̽� ���ؽ�Ʈ
	IDXGISwapChain* m_pSwapChain = nullptr; 						// ����ü��
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr; 		// ���� Ÿ�ٺ�

	// ���������� ���ҽ�
	ID3D11VertexShader* m_pVertexShader = nullptr; 				// ���� ���̴�
	ID3D11PixelShader* m_pPixelShader = nullptr; 					// �ȼ� ���̴�(����)	
	ID3D11PixelShader* m_pPixelShaderSolid = nullptr; 				// �ȼ� ���̴�(��Ŀ�� ���)

	ID3D11SamplerState* m_pSamplerState = nullptr;
	ID3D11BlendState* m_pAlphaBlendState = nullptr; 						// ���� ���� ����
	ID3D11VertexShader* m_pSkyBoxVertexShader = nullptr; 			// ��ī�̹ڽ� ���� ���̴�
	ID3D11PixelShader* m_pSkyBoxPixelShader = nullptr; 				// ��ī�̹ڽ� �ȼ� ���̴�(����)	
	ID3D11InputLayout* m_pSkyBoxInputLayout = nullptr; 				// ��ī�̹ڽ� �Է� ���̾ƿ�
	ID3D11ShaderResourceView* m_pTextureSRV = nullptr;

	// SkyBox ����: Hanako.dds / cubemap.dds
	enum class SkyBoxChoice { Hanako = 0, CubeMap = 1 };
	SkyBoxChoice m_SkyBoxChoice = SkyBoxChoice::Hanako;
	ID3D11ShaderResourceView* m_pSkyHanakoSRV = nullptr;
	ID3D11ShaderResourceView* m_pSkyCubeMapSRV = nullptr;
	
	ID3D11InputLayout* m_pInputLayout = nullptr; 					// �Է� ���̾ƿ�
	// ť��(��ī�̹ڽ�/����׿�) ����
	ID3D11Buffer* m_pVertexBuffer = nullptr; 						// ���ؽ� ����
	UINT m_VertextBufferStride = 0; 							// ���ؽ� �ϳ��� ũ��
	UINT m_VertextBufferOffset = 0; 							// ���ؽ� ���� ������
	ID3D11Buffer* m_pIndexBuffer = nullptr; 						// �ε��� ����
	int m_nIndices = 0; 										// �ε��� ����

	// PMX ������
	PmxManager m_Pmx;
	std::wstring m_ModelPath = L"..\\Resource\\Nikke-Alice\\alice-Apose.pmx";

	ID3D11Buffer* m_pConstantBuffer = nullptr; 						// ��� ���� (����)
	std::vector<ConstantBuffer> m_CBuffers;						// �ؽ��ĵ� �׸��� ���� ��� ���� ĳ��
	ConstantBuffer m_ConstantBuffer; 							// CPU-side ��� ���� ������
	ID3D11Buffer* m_pLineVertexBuffer = nullptr; 					// ����Ʈ ���� ǥ�ÿ� ���� VB

    // �и��� ��ƿ��
    class LineRenderer* m_LineRenderer = nullptr;           // ��/��ǥ�� ������
    class Skybox* m_Skybox = nullptr;                       // ��ī�̹ڽ�
    // ����� �ڽ� ����
    ID3D11Buffer* m_pDebugBoxVB = nullptr;
    ID3D11Buffer* m_pDebugBoxIB = nullptr;
    int m_DebugBoxIndexCount = 0;

	ID3D11DepthStencilView* m_pDepthStencilView; 				// ���� ���ٽ� ��
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;  	// ���� ���ٽ� ����

	ID3D11RasterizerState*  RSNoCull; 								// ������ ������ ���� : �� Ŭ���� ��� ����
	ID3D11RasterizerState* RSCullClockWise; 							// ������ ������ ���� : �ð� ���� �ڸ��� ���

	// �̹��� �����/����� �ؽ�ó (Hanako)
	ID3D11ShaderResourceView* m_TexHanakoSRV = nullptr;
	bool m_ShowHanako = false;
	ImVec2 m_HanakoDrawSize = ImVec2(128, 128);
	ImVec2 m_TexHanakoSize = ImVec2(0, 0);

	// ��ī�̹ڽ� ť��� �� SRV�� (0:+X,1:-X,2:+Y,3:-Y,4:+Z,5:-Z)
	// ImGui���� ���� ī�޶� ���� �ִ� ��ī�� �ڽ��� �̹����� �׷��ֱ� ���� ������
	ID3D11ShaderResourceView* m_pSkyFaceSRV[6] = {};
	ImVec2 m_SkyFaceSize = ImVec2(0, 0);

	// Skybox current dds (path)
	wchar_t m_CurrentSkyboxPath[260] = L"..\\Resource\\cubemap.dds";

	// �ؽ��� 
	ID3D11ShaderResourceView* m_pCubeTextureSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

	// ImGui ��Ʈ�� ���� ����
	SystemInfomation m_SystemInfo;
	Camera m_camera;										// ī�޶�
	DirectX::XMFLOAT3 m_cubePos = { 0.0f, 0.0f, 0.0f }; 			// ť�� ��Ʈ ��ġ
	DirectX::XMFLOAT3 m_cubeRotation = { 0.0f, 0.0f, 0.0f }; 	// ť�� ȸ��(Yaw/Pitch/Roll, deg)
	bool m_RotateCube = false; 									// ImGui ���: ť�� �ڵ� ȸ�� on/off
	ShadingMode m_ShadingMode = ShadingMode::Phong; 				// �⺻ Phong

	// Mirror Cube transform (����: �⺻ ť��� ������ ����)
	DirectX::XMFLOAT3 m_mirrorCubePos = { 4.5f, 0.0f, 0.0f };   // �ſ� ť�� ��ġ (x+8 �⺻)
	DirectX::XMFLOAT3 m_mirrorCubeRotation = { 0.0f, 0.0f, 0.0f }; // �ſ� ť�� ȸ��(Yaw/Pitch/Roll, deg)
	float m_MirrorCubeScale = 2.0f;                                 // �ſ� ť�� ������

	// DirectionalLight
	DirectionalLight m_DirLight = {
		/*ambient*/ { 0.03f, 0.03f, 0.03f, 0.1f },
		/*diffuse*/ { 1.0f, 1.0f, 1.0f, 1.0f },
		/*specular*/{ 1.0f, 1.0f, 1.0f, 1.0f },
		/*direction*/{ 0.0f, 0.0f, 1.0f },
		/*pad*/ 0.0f
	};
	// Material
	Material m_Material = {
		/*ambient*/ { 1.0f, 1.0f, 1.0f, 1.0f },
		/*diffuse*/ { 1.0f, 1.0f, 1.0f, 1.0f },
		/*specular*/{ 1.0f, 1.0f, 1.0f, 32.0f }, // w = shininess
		/*reflect*/ { 0.0f, 0.0f, 0.0f, 0.0f }
	};
	// Mirror Cube Material (��Ż�� �ſ� ����)
	Material m_mirrorCubeMaterial = {
		/*ambient*/ { 0.0f, 0.0f, 0.0f, 1.0f },
		/*diffuse*/ { 0.0f, 0.0f, 0.0f, 1.0f },
		/*specular*/{ 0.0f, 0.0f, 0.0f, 32.0f },
		/*reflect*/ { 1.0f, 1.0f, 1.0f, 0.02f }   // a=roughness (�������� ����)
	};

	DirectX::XMFLOAT3 m_LightPosition = { 4.0f, 4.0f, 0.0f };   // ����Ʈ ��ġ(��Ŀ��)

	ConstantBuffer m_baseProjection;									// �⺻ ī�޶�/���� ĳ��

	// Cube scale to better visualize specular highlights
	float m_CubeScale = 2.0f;
 
	// �����ֱ�
	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate(const float& dt) override;
	void OnRender() override;

	// D3D ��ġ ����/�ı�
	bool InitD3D();
	void UninitD3D();

	// ���ҽ� ����/�ı�
	// ���̴�,���ؽ�,�ε���
	bool InitScene(); 											
	void UninitScene();

	// �ؽ��� ����
	bool InitTexture();

	//ImGui ����
	bool InitImGui();

private:
	// ���̴��� �о���� �Լ��� ���� ����
	bool InitBasicEffect(); 										
	bool InitSkyBoxEffect();

	// ť��� �� SRV �غ�/���� �� �� ���� ���
	void PrepareSkyFaceSRVs();
	// Skybox change helper
	void ChangeSkyboxDDS(const wchar_t* ddsPath);
};

