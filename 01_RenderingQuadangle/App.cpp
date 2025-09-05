/*
* @brief : d3d11�� ���簢���� �׸��� �����Դϴ�
* @details :
* 
		- NDC�� ����ó�� ��������ϴ�.
			Normalized Device Coordinates (NDC)
			x: left=-1  center=0  right=+1
			y: bottom=-1 center=0  top=+1

		- �ﰢ�� �ΰ��� �簢���� �׸��ϴ�
			0 ������������������ 1
			��        �� ��
			��     ��    ��
			��  ��       ��
			2 ������������������ 3
		
			indices (list): { 0, 1, 2,  2, 1, 3 }   // ���� �簢��, ���Ҹ� �ݴ�
*/

#include "App.h"
#include "../Common/Helper.h"
#include <d3dcompiler.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

// vertex structure
struct VertexInfo
{
	// ������ ��ġ, ���� ����
	Vector3 position;
	Vector3 color;
	
	VertexInfo(float x, float y, float z, float r, float g, float b) : position(x, y, z), color(r, g, b){}
	VertexInfo(Vector3 pos) : position(pos) {}
	VertexInfo(Vector3 pos, Vector4 col) : position(pos), color(col.x, col.y, col.z) {}
};

bool App::OnInitialize()
{
	if(!InitD3D() || !InitScene()) return false;
	return true;
}

void App::OnUninitialize()
{
	UninitD3D();
}

void App::OnUpdate()
{

}

// Render() �Լ��� �߿��� �κ��� �� ����ֽ��ϴ�. ���⸦ ���� �˴ϴ�
void App::OnRender()
{
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);

	// 1 ~ 3 . IA �ܰ� ����
	// ������ ��� �̾ �׸� �������� �����ϴ� �κ�
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// 1. ���۸� ����ֱ�
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &m_VertextBufferStride, &m_VertextBufferOffset);
	// 2. �Է� ���̾ƿ��� ����ֱ�
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	// 3. �ε��� ���۸� ����ֱ�
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// 4. Vertex Shader ����
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);

	// 4. Pixel Shader ����
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	// 6. �׸���
	m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);

	m_pSwapChain->Present(0, 0);
}

bool App::InitD3D()
{
	//HRESULT hr = 0;

	// ����ü���� ������ ������ ����ü�� ����ϴ�
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;
	swapDesc.Windowed = true;
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;

	// DXGI_RATIONAL? 
	// - �ֻ����� ���� ��
	// DXGI_RATIONAL ����ü�� ������(�м�) ���� ǥ���ϱ� ���� ����ü
	// RefreshRate = Numerator / Denominator
	// �ֻ���(Refresh Rate, Hz ����)�� �����մϴ�
	// �� ���� �� ���� 60 / 1 = 60 �� 60Hz�� �ǹ��մϴ�
	// ���� �ڵ忡�� 60���� �δ� ������ �������� 60Hz ����� ȯ�濡 ���߱� ���ؼ� �Դϴ�
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;

	// DXGI_SAMPLE_DESC?
	// - ��Ƽ���ø�(Multi-Sampling Anti-Aliasing, MSAA) ������ ���� ��
	// �� �ȼ��� �� �� ���ø��� ������ �����մϴ�
	// ��, MSAA�� ���� ������ ���մϴ�
	// Count�� 1�̸� ��Ƽ���ø��� ������� ������ �ǹ��մϴ�
	// �������� ���õǾ� ������ �� Ȯ���غ� ��!
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	// ����� â�� ���� �����Դϴ�.
	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	/*
	* @brief  Direct3D ����̽�, ����̽� ���ؽ�Ʈ, ����ü�� ����
	* @details
	*   - Adapter        : NULL �� �⺻ GPU ���
	*   - DriverType     : D3D_DRIVER_TYPE_HARDWARE �� �ϵ���� ����
	*   - Flags          : creationFlags (����� ��� ���� ����)
	*   - SwapChainDesc  : �����, �ֻ��� �� ����ü�� ����
	*   - ��ȯ           : m_pDevice, m_pDeviceContext, m_pSwapChain
	*/
	HR_T(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_pSwapChain, &m_pDevice, NULL, &m_pDeviceContext));

	/*
	* @brief  ����ü�� ����۷� RTV�� ����� OM ���������� ���ε��Ѵ�
	* @details
	*   - GetBuffer(0): �����(ID3D11Texture2D)�� ȹ��
	*   - CreateRenderTargetView: ����� ��� RTV ����(���ҽ� ���� ���� ����)
	*   - ���� �ؽ�ó �����ʹ� Release�� ���� (RTV�� ���� ����)
	*   - OMSetRenderTargets: ������ RTV�� ���� Ÿ���� ���� ��� ���������ο� ���ε�
	*/
	ID3D11Texture2D* pBackBufferTexture = nullptr;
	HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));
	HR_T(m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_pRenderTargetView));
	SAFE_RELEASE(pBackBufferTexture);
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);

	/*
	* @brief  ����Ʈ(Viewport) ����
	* @details
	*   - TopLeftX/Y : ��� ������ ���� ��ǥ (0,0 �� �»��)
	*   - Width/Height : ������ Ŭ���̾�Ʈ ũ�� ����
	*   - MinDepth/MaxDepth : ���� ���� (���� 0.0 ~ 1.0)
	*   - RSSetViewports : �����Ͷ����� ���������� ����Ʈ ���ε�
	*/
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)m_ClientWidth;
	viewport.Height = (float)m_ClientHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	m_pDeviceContext->RSSetViewports(1, &viewport);

	return true;
}

void App::UninitD3D()
{
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDevice);
}

/*
 * @brief InitScene() ��ü �帧
 *   1. ����(Vertex) �迭�� GPU ���۷� ����
 *   2. VS �Է� �ñ״�ó�� ���� InputLayout ����
 *   3. VS ����Ʈ�ڵ�� Vertex Shader ���� �� ���� ����
 *   4. �ε��� ����(Index Buffer) ����
 *   5. PS ����Ʈ�ڵ�� Pixel Shader ���� �� ���� ����
 */
bool App::InitScene()
{
	//HRESULT hr = 0;

	/*
	* @brief  ����(Vertex) �迭�� GPU ���۷� ����
	* @details
	*   - ByteWidth : ���� ��ü ũ��(���� ũ�� �� ����)
	*   - BindFlags : D3D11_BIND_VERTEX_BUFFER
	*   - Usage     : DEFAULT (�Ϲ��� �뵵)
	*   - �ʱ� ������ : vbData.pSysMem = vertices
	*   - Stride/Offset : IASetVertexBuffers�� �Ķ����
	*   - ���� : VertexInfo(color=Vec3), ���̴�/InputLayout�� COLOR ���� ��ġ �ʿ�
	*/
	VertexInfo vertices[] =
	{
		VertexInfo(Vector3(-0.5f,  0.5f, 0.5f), Vector4(1.0f, 0.0f, 1.0f, 1.0f)),
		VertexInfo(Vector3(0.5f,  0.5f, 0.5f), Vector4(0.0f, 1.0f, 0.0f, 1.0f)),
		VertexInfo(Vector3(-0.5f, -0.5f, 0.5f), Vector4(1.0f, 0.2f, 1.0f, 1.0f)),
		VertexInfo(Vector3(0.5f, -0.5f, 0.5f), Vector4(0.0f, 0.6f, 1.0f, 1.0f))
	};

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = sizeof(VertexInfo) * ARRAYSIZE(vertices);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;	// �迭 ������ �Ҵ�.
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));
	m_VertextBufferStride = sizeof(VertexInfo);		// ���ؽ� ���� ����
	m_VertextBufferOffset = 0;


	/*
	* @brief  VS �Է� �ñ״�ó�� ���� InputLayout ����
	* @details
	*   - POSITION: float3, COLOR: float4 (����ü/���̴��� ���ġ������� ��ġ �ʼ�)
	*   - InputSlot=0, Per-Vertex ������
	*   - D3D11_APPEND_ALIGNED_ELEMENT�� COLOR ������ �ڵ� ���
	*   - VS ����Ʈ�ڵ�(CompileShaderFromFile)�� �ñ״�ó ��Ī �� CreateInputLayout
	*/
	D3D11_INPUT_ELEMENT_DESC layout[] = // �Է� ���̾ƿ�.
	{   
		// ���� { SemanticName , SemanticIndex , Format , InputSlot , AlignedByteOffset , InputSlotClass , InstanceDataStepRate	}
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"BasicVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));


	/*
	* @brief  VS ����Ʈ�ڵ�� Vertex Shader ���� �� ������ ���� ����
	* @details
	*   - CreateVertexShader: �����ϵ� ����Ʈ�ڵ�(pointer/size)�� VS ��ü ����
	*   - ClassLinkage �̻��(NULL)
	*   - ���� ��, �� �̻� �ʿ� ���� vertexShaderBuffer�� ����
	*/
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// ������ ���� ����


	/*
	* @brief  �ε��� ����(Index Buffer) ����
	* @details
	*   - indices: ���� ����� (�簢�� = �ﰢ�� 2�� = �ε��� 6��)
	*   - WORD Ÿ�� �� DXGI_FORMAT_R16_UINT ��� ����
	*   - ByteWidth : ��ü �ε��� �迭 ũ��
	*   - BindFlags : D3D11_BIND_INDEX_BUFFER
	*   - Usage     : DEFAULT (GPU �Ϲ� ����)
	*   - �� �ڵ��� ���: m_pIndexBuffer ����, m_nIndices�� ���� ����
	*/
	WORD indices[] =
	{
		0, 1, 2,
		2, 1, 3
	};
	m_nIndices = ARRAYSIZE(indices);	// �ε��� ���� ����.
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));


	/*
	* @brief  PS ����Ʈ�ڵ�� Pixel Shader ���� �� ������ ���� ����
	* @details
	*   - CompileShaderFromFile: "BasicPixelShader.hlsl", entry="main", profile="ps_4_0"
	*   - CreatePixelShader: ����Ʈ�ڵ�(pointer/size)�� PS ��ü ���� (ClassLinkage �̻��)
	*   - ���� �� pixelShaderBuffer�� �� �̻� �ʿ� �����Ƿ� ����
	*/
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"BasicPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// �ȼ� ���̴� ���� ���̻� �ʿ����
	return true;
}

void App::UninitScene()
{
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
}
