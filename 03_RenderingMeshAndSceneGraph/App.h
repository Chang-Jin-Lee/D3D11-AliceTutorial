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

	struct ConstantBuffer
	{
		DirectX::XMMATRIX world;
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX proj;
	};

public:
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

	ID3D11Buffer* m_pConstantBuffer;				// ��� ����
	//ConstantBuffer m_CBuffer;                       // GPU ��� ���۸� �����ϴ� �� ���Ǵ� ����
	std::vector<ConstantBuffer> m_CBuffers;
	ID3D11DepthStencilView* m_pDepthStencilView;    // ���� ���ø�
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;	// ���� ���ٽ� ����

	// ImGui ��Ʈ�� ���� ����
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

	bool InitScene();								// ���̴�,���ؽ�,�ε���
	void UninitScene();

private:
	bool InitEffect();								// ���̴��� �о���� �Լ��� ���� ����
};

