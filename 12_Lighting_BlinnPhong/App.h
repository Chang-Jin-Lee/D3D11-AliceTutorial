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

// ���⼺ ����Ʈ
struct DirectionalLight
{
	DirectX::XMFLOAT4 ambient;
	DirectX::XMFLOAT4 diffuse;
	DirectX::XMFLOAT4 specular;
	DirectX::XMFLOAT3 direction;  // ����Ʈ ���� (����ȭ ����)
	float pad; // HLSL cbuffer 16����Ʈ ���Ŀ� �е�
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
		DirectX::XMMATRIX world;               // ���� ���(��ġ�� ����)
		DirectX::XMMATRIX view;                // �� ���(��ġ�� ����)
		DirectX::XMMATRIX proj;                // �������� ���(��ġ�� ����)
		DirectX::XMMATRIX worldInvTranspose;   // (World^-1)^T

		Material material;						// ����
		DirectionalLight dirLight;             // ���⼺ ����Ʈ(��/����)
		DirectX::XMFLOAT3 eyePos;              // ī�޶� ��ġ
		float pad;                             // ����� ��� � ���
	};

public:
	// �ʼ� D3D ��ü
	ID3D11Device* m_pDevice = nullptr; 						// ����̽�	
	ID3D11DeviceContext* m_pDeviceContext = nullptr; 		// ��� ����̽� ���ؽ�Ʈ
	IDXGISwapChain* m_pSwapChain = nullptr; 					// ����ü��
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr; 	// ���� Ÿ�ٺ�

	// ���������� ���ҽ�
	ID3D11VertexShader* m_pVertexShader = nullptr; 	// ���� ���̴�
	ID3D11PixelShader* m_pPixelShader = nullptr; 	// �ȼ� ���̴�(����)	
	ID3D11PixelShader* m_pPixelShaderSolid = nullptr; 	// �ȼ� ���̴�(��Ŀ�� ���)

	ID3D11SamplerState* m_pSamplerState = nullptr;
	ID3D11VertexShader* m_pSkyBoxVertexShader = nullptr; 	// ��ī�̹ڽ� ���� ���̴�
	ID3D11PixelShader* m_pSkyBoxPixelShader = nullptr; 	// ��ī�̹ڽ� �ȼ� ���̴�(����)	
	ID3D11InputLayout* m_pSkyBoxInputLayout = nullptr; 	// ��ī�̹ڽ� �Է� ���̾ƿ�
	ID3D11ShaderResourceView* m_pTextureSRV = nullptr;
	// SkyBox ����: Hanako.dds / cubemap.dds
	enum class SkyBoxChoice { Hanako = 0, CubeMap = 1 };
	SkyBoxChoice m_SkyBoxChoice = SkyBoxChoice::Hanako;
	ID3D11ShaderResourceView* m_pSkyHanakoSRV = nullptr;
	ID3D11ShaderResourceView* m_pSkyCubeMapSRV = nullptr;
	
	ID3D11InputLayout* m_pInputLayout = nullptr; 	// �Է� ���̾ƿ�
	ID3D11Buffer* m_pVertexBuffer = nullptr; 		// ���ؽ� ����
	UINT m_VertextBufferStride = 0; 					// ���ؽ� �ϳ��� ũ��
	UINT m_VertextBufferOffset = 0; 					// ���ؽ� ���� ������
	ID3D11Buffer* m_pIndexBuffer = nullptr; 			// �ε��� ����
	int m_nIndices = 0; 								// �ε��� ����

	ID3D11Buffer* m_pConstantBuffer = nullptr; 				// ��� ���� (����)
	ConstantBuffer m_ConstantBuffer; 						// CPU-side ��� ���� ������
	ID3D11Buffer* m_pLineVertexBuffer = nullptr; 			// ����Ʈ ���� ǥ�ÿ� ���� VB

	ID3D11DepthStencilView* m_pDepthStencilView; 	// ���� ���ٽ� ��
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;  	// ���� ���ٽ� ����
	Microsoft::WRL::ComPtr<IDXGIAdapter3> m_Adapter3; // VRAM ��ȸ��
	SIZE_T m_VideoMemoryTotal = 0; // �� VRAM ����Ʈ

	ID3D11RasterizerState*  RSNoCull;			            // ������ ������ ���� : �� Ŭ���� ��� ����
	ID3D11RasterizerState* RSCullClockWise;					// ������ ������ ���� : �ð� ���� �ڸ��� ���

	// �̹��� �����/����� �ؽ�ó (Hanako)
	ID3D11ShaderResourceView* m_TexHanakoSRV = nullptr;
	bool m_ShowHanako = false;
	ImVec2 m_HanakoDrawSize = ImVec2(128, 128);
	ImVec2 m_TexHanakoSize = ImVec2(0, 0);

	// ��ī�̹ڽ� ť��� �� SRV�� (0:+X,1:-X,2:+Y,3:-Y,4:+Z,5:-Z)
	// ImGui���� ���� ī�޶� ���� �ִ� ��ī�� �ڽ��� �̹����� �׷��ֱ� ���� ������
	ID3D11ShaderResourceView* m_pSkyFaceSRV[6] = {};
	ImVec2 m_SkyFaceSize = ImVec2(0, 0);

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

	// ImGui ��Ʈ�� ���� ����
	DirectX::XMFLOAT3 m_cubePos = { -0.0f, 0.0f, 0.0f }; // ��Ʈ ��ġ
	DirectX::XMFLOAT3 m_cameraPos = { 0.0f, 0.0f, -10.0f }; // ī�޶� ��ġ
	float m_CameraFovDeg = 90.0f;   // FOV(deg)
	float m_CameraNear = 1.0f;      // Near
	float m_CameraFar = 1000.0f;    // Far

	bool m_RotateCube = false;		// ImGui ���: ť�� �ڵ� ȸ�� on/off
	float m_YawDeg = 0.0f;          // ť�� Yaw
	float m_PitchDeg = 0.0f;        // ť�� Pitch
	DirectX::XMFLOAT3 m_LightDirection = { 0.0f, -1.0f, 1.0f }; // ����Ʈ ����(UI)
	DirectX::XMFLOAT3 m_LightColorRGB = { 1.0f, 1.0f, 1.0f };   // ����Ʈ ��(UI)
	DirectX::XMFLOAT3 m_LightPosition = { 4.0f, 4.0f, 0.0f };   // ����Ʈ ��ġ(��Ŀ��)
	DirectX::XMFLOAT3 m_CameraForward = { 0.0f, 0.0f, 1.0f };   // ī�޶� �չ���(��ī�̹ڽ���)

	ConstantBuffer m_baseProjection; // �⺻ ī�޶�/���� ĳ��

	// Material parameters (ImGui)
	DirectX::XMFLOAT3 m_MaterialAmbientRGB = { 0.2f, 0.2f, 0.2f };
	DirectX::XMFLOAT3 m_MaterialDiffuseRGB = { 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 m_MaterialSpecularRGB = { 1.0f, 1.0f, 1.0f };
	float m_MaterialShininess = 32.0f; // g_Material.specular.w

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
	bool InitScene(); 							// ���̴�,���ؽ�,�ε���
	void UninitScene();

private:
	bool InitBasicEffect(); 							// ���̴��� �о���� �Լ��� ���� ����
	bool InitSkyBoxEffect();

	// ť��� �� SRV �غ�/���� �� �� ���� ���
	void PrepareSkyFaceSRVs();
};

