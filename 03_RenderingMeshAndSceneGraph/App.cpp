/*
* @brief : d3d11�� ť�긦 �׸��� �����Դϴ�
* @details :
* 
		-  ť�긦 ����ϴ�
			//    5________ 6
			//    /|      /|
			//   /_|_____/ |
			//  1|4|_ _ 2|_|7
			//   | /     | /
			//   |/______|/
			//  0       3

		- �ε��� �迭�� ������ �����ϼ���
		 DWORD indices[] = {
		// ����
		0, 1, 2,
		2, 3, 0,
		// ����
		4, 5, 1,
		1, 0, 4,
		// �� ��
		1, 5, 6,
		6, 2, 1,
		// ���ʿ�
		7, 6, 5,
		5, 4, 7,
		// ������
		3, 2, 6,
		6, 7, 3,
		// �� �Ʒ�
		4, 0, 3,
		3, 7, 4
		};
*/

#include "App.h"
#include "../Common/Helper.h"
#include <d3dcompiler.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

bool App::OnInitialize()
{
	if(!InitD3D()) return false;

	if (!InitEffect()) return false;

	if(!InitScene()) return false;

	// ImGui �ʱ�ȭ
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);

	return true;
}

void App::OnUninitialize()
{
	// ImGui ����
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	UninitD3D();
}

void App::OnUpdate(const float& dt)
{
	static float t0 = 0.0f, t1 = 0.0f, t2 = 0.0f;
	t0 += 0.6f * dt;   // �θ�(��Ʈ) Yaw �ӵ�
	t1 += 1.0f * dt;   // �ι�° �޽�(�ڽ�1) Yaw �ӵ� (��Ʈ�� �ٸ���)
	t2 += 1.2f * dt;   // ����° �޽�(�ڽ�2) ���� �ӵ�

	// ���� ��ȯ ���� (���� Scene Graph)
	XMMATRIX local0 = XMMatrixRotationY(t0) * XMMatrixTranslation(m_RootPos.x, m_RootPos.y, m_RootPos.z); // ��Ʈ
	XMMATRIX local1 = XMMatrixRotationY(t1) * XMMatrixTranslation(m_Child1Offset.x, m_Child1Offset.y, m_Child1Offset.z);  // �ڽ�1 (��Ʈ ���� ���)
	XMMATRIX local2 = XMMatrixRotationY(t2) * XMMatrixTranslation(m_Child2Offset.x, m_Child2Offset.y, m_Child2Offset.z);  // �ڽ�2: �ڽ�1�� �߽����� ����

	// �θ�-�ڽ� ���� world ���: world = local * parentWorld
	XMMATRIX world0 = local0;                 // ��Ʈ
	XMMATRIX world1 = local1 * world0;        // �ڽ�1
	XMMATRIX world2 = local2 * world1;        // �ڽ�2 (�ڽ�1 �ֺ� ����)

	// View/Proj�� UI�� �ݿ� (�� ������)
	XMMATRIX view = XMMatrixTranspose(XMMatrixLookAtLH(
		XMVectorSet(m_CameraPos.x, m_CameraPos.y, m_CameraPos.z, 0.0f),
		XMVectorSet(m_CameraPos.x + 0.0f, m_CameraPos.y + 0.0f, m_CameraPos.z + 1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
	float fovRad = XMConvertToRadians(m_CameraFovDeg);
	XMMATRIX proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovRad, AspectRatio(), m_CameraNear, m_CameraFar));

	m_CBuffers[0].world = XMMatrixTranspose(world0);
	m_CBuffers[0].view = view;
	m_CBuffers[0].proj = proj;
	m_CBuffers[1].world = XMMatrixTranspose(world1);
	m_CBuffers[1].view = view;
	m_CBuffers[1].proj = proj;
	m_CBuffers[2].world = XMMatrixTranspose(world2);
	m_CBuffers[2].view = view;
	m_CBuffers[2].proj = proj;
}

// Render() �Լ��� �߿��� �κ��� �� ����ֽ��ϴ�. ���⸦ ���� �˴ϴ�
void App::OnRender()
{
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	UINT stride = sizeof(VertexPosColor);	// ����Ʈ ��
	UINT offset = 0;

	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 1 ~ 3 . IA �ܰ� ����
	// ������ ��� �̾ �׸� �������� �����ϴ� �κ�
	// 1. ���۸� ����ֱ�
	// 2. �Է� ���̾ƿ��� ����ֱ�
	// 3. �ε��� ���۸� ����ֱ�
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// 4. Vertex Shader ����
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	// 5. Pixel Shader ����
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	// 6. Constant Buffer ����
	// 7. �׸���
	for (auto m_CBuffer : m_CBuffers)
	{
		D3D11_MAPPED_SUBRESOURCE mapped{};
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0,
			D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy(mapped.pData, &m_CBuffer, sizeof(m_CBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);

		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);
	}

	// ImGui ������ �� UI ������
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (ImGui::Begin("Controls"))
	{
		ImGui::Text("Mesh Transforms");
		ImGui::DragFloat3("Root Pos (x,y,z)", &m_RootPos.x, 0.1f);
		ImGui::DragFloat3("Child1 Offset (x,y,z)", &m_Child1Offset.x, 0.1f);
		ImGui::DragFloat3("Child2 Offset (x,y,z)", &m_Child2Offset.x, 0.1f);
		ImGui::Separator();
		ImGui::Text("Camera");
		ImGui::DragFloat3("Camera Pos (x,y,z)", &m_CameraPos.x, 0.1f);
		ImGui::SliderFloat("Camera FOV (deg)", &m_CameraFovDeg, 30.0f, 120.0f);
		ImGui::DragFloatRange2("Near/Far", &m_CameraNear, &m_CameraFar, 0.1f, 0.01f, 5000.0f, "Near: %.2f", "Far: %.2f");
	}
	ImGui::End();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

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

	// ���� ���ٽ� �ؽ�ó/�� ����
	D3D11_TEXTURE2D_DESC dsDesc = {};
	dsDesc.Width = m_ClientWidth;
	dsDesc.Height = m_ClientHeight;
	dsDesc.MipLevels = 1;
	dsDesc.ArraySize = 1;
	dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsDesc.SampleDesc.Count = swapDesc.SampleDesc.Count;
	dsDesc.SampleDesc.Quality = swapDesc.SampleDesc.Quality;
	dsDesc.Usage = D3D11_USAGE_DEFAULT;
	dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	dsDesc.CPUAccessFlags = 0;
	dsDesc.MiscFlags = 0;

	ID3D11Texture2D* pDepthStencil = nullptr;
	HR_T(m_pDevice->CreateTexture2D(&dsDesc, nullptr, &pDepthStencil));
	HR_T(m_pDevice->CreateDepthStencilView(pDepthStencil, nullptr, &m_pDepthStencilView));
	SAFE_RELEASE(pDepthStencil);

	// DepthStencilState ���� �� ����
	D3D11_DEPTH_STENCIL_DESC dssDesc = {};
	dssDesc.DepthEnable = TRUE;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS;
	dssDesc.StencilEnable = FALSE;
	HR_T(m_pDevice->CreateDepthStencilState(&dssDesc, &m_pDepthStencilState));
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStencilState, 0);

	// ���� Ÿ��/DSV ���ε�
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

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
	SAFE_RELEASE(m_pDepthStencilState);
	SAFE_RELEASE(m_pDepthStencilView);
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
	VertexPosColor vertices[] =
	{
		{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
		{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) }
	};

	D3D11_BUFFER_DESC vbDesc = {};
	ZeroMemory(&vbDesc, sizeof(vbDesc));			// vbDesc�� 0���� ��ü �޸� ������ �ʱ�ȭ ��ŵ�ϴ�
	vbDesc.ByteWidth = sizeof vertices;				// �迭 ��ü�� ����Ʈ ũ�⸦ �ٷ� ��ȯ�մϴ�
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA vbData = {};
	ZeroMemory(&vbData, sizeof(vbData));
	vbData.pSysMem = vertices;						// �迭 ������ �Ҵ�.
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));


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
	DWORD indices[] = {
		// ����
		0, 1, 2,
		2, 3, 0,
		// ����
		4, 5, 1,
		1, 0, 4,
		// �� ��
		1, 5, 6,
		6, 2, 1,
		// ���ʿ�
		7, 6, 5,
		5, 4, 7,
		// ������
		3, 2, 6,
		6, 7, 3,
		// �� �Ʒ�
		4, 0, 3,
		3, 7, 4
	};
	D3D11_BUFFER_DESC ibDesc = {};
	ZeroMemory(&ibDesc, sizeof(ibDesc));
	m_nIndices = ARRAYSIZE(indices);	// �ε��� ���� ����.
	ibDesc.ByteWidth = sizeof indices;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));


	// ******************
	// ��� ���� ����
	//
	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(ConstantBuffer);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	// �ʱ� �����͸� ������� �ʰ� ���ο� ��� ���۸� ����ϴ�
	HR_T(m_pDevice->CreateBuffer(&cbd, nullptr, &m_pConstantBuffer));

	// ���� ī�޶�(View/Proj)�� 3���� ��� ���� ��Ʈ���� �غ��մϴ�
	ConstantBuffer base;
	base.world = XMMatrixIdentity();
	base.view = XMMatrixTranspose(XMMatrixLookAtLH(
		XMVectorSet(m_CameraPos.x, m_CameraPos.y, m_CameraPos.z, 0.0f),
		XMVectorSet(m_CameraPos.x + 0.0f, m_CameraPos.y + 0.0f, m_CameraPos.z + 1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	));
	float fovRad = XMConvertToRadians(m_CameraFovDeg);
	base.proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovRad, AspectRatio(), m_CameraNear, m_CameraFar));

	m_CBuffers.clear();
	m_CBuffers.reserve(3);
	m_CBuffers.push_back(base);
	m_CBuffers.push_back(base);
	m_CBuffers.push_back(base);

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

bool App::InitEffect()
{
	// Vertex Shader -------------------------------------
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
	HR_T(CompileShaderFromFile(L"03_BasicVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
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


	// Pixel Shader -------------------------------------
	/*
	* @brief  PS ����Ʈ�ڵ�� Pixel Shader ���� �� ������ ���� ����
	* @details
	*   - CompileShaderFromFile: "BasicPixelShader.hlsl", entry="main", profile="ps_4_0"
	*   - CreatePixelShader: ����Ʈ�ڵ�(pointer/size)�� PS ��ü ���� (ClassLinkage �̻��)
	*   - ���� �� pixelShaderBuffer�� �� �̻� �ʿ� �����Ƿ� ����
	*/
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"03_BasicPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// �ȼ� ���̴� ���� ���̻� �ʿ����
	return true;
}
