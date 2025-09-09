/*
* @brief : FBX Mesh �δ�/���� ���� (�ؽ�ó/��Ƽ���� ����� ����)
* @details :
*   - �ϴϿ�(Haniwa) �޽� (���� ����, ���� �翷���� ���̴� ����)
*
*         //        _____
*         //      /       \
*         //     |  o   o  |     <-- ��
*         //     |    ?    |     <-- ��(����)
*         //     |---------|     <-- �� ���(��)
*         //   --|  (   )  |--   <-- �翷 �� ����(��Ȧ)
*         //     |         |
*         //     |  ___    |     <-- �㸮 ���
*         //     | /   \   |
*         //     | |   |   |     <-- ������ ����
*         //     | |   |   |
*         //     | |___|   |
*         //     |_________|     <-- ��ħ��
*
*		- ����
*			FBX ������ �޽� ������ �о� ��ü ��� ������ ������ ���� ���� �������� �����ϰ�,
*			��Ƽ����(�ַ� DIFFUSE)�� ��������� ������ �ؽ�ó���� �������մϴ�.
*
*		- ���ҽ� ����(����)
*			../Resource/haniwa/haniwa.fbx		: FBX �޽�
*			../Resource/haniwa/*.png,*.jpg	: ��Ƽ���� �ؽ�ó(�ܺ� ����)
*
*		- ���� ���������� ����
*			1) InitD3D
*				- ����̽�/����ü��, RTV/DSV ���� �� ���ε�, Viewport ����
*				- Depth �׽�Ʈ Ȱ��ȭ(LESS), ������ Cull None (���ε� �̽� ���)
*			2) InitEffect
*				- VS: TexVertexShader.hlsl (POSITION, TEXCOORD)
*				- PS: TexPixelShader.hlsl (�ؽ�ó ����)
*				- InputLayout: POSITION(float3), TEXCOORD(float2)
*			3) InitScene (FBX �ε� �ٽ�)
*				- Assimp�� FBX �б� (Triangulate, JoinIdenticalVertices, ImproveCacheLocality,
*				  GenSmoothNormals, CalcTangentSpace, ConvertToLeftHanded)
*				- ��Ƽ���� �ؽ�ó �ε�
*					�� �ܺ� ����: �� ������ ��ġ�� ������ �������� ����� ����
*					�� �Ӻ����("*0" ��): �޸𸮿��� ���� �ؽ�ó ����
*					�� ���� �� 1x1 ��� �ؽ�ó�� ����
*				- �� ��Ʈ������ ��� ��ȸ�� �۷ι� ��ȯ�� ������ �����ϸ� ����
*				- �����(startIndex, indexCount, materialIndex) ���
*				- AABB(min/max) �� �߽� �̵� + ������ ����ȭ, ī�޶� �ڵ� ����
*				- ��� ����(����) ����: world/view/proj
*			4) OnUpdate
*				- ���� ���� ��ȯ(ȸ�� Yaw + �����̵� RootPos), view/proj ����
*			5) OnRender
*				- RTV/DSV Ŭ���� �� IA(Topology, VB/IB, InputLayout) ����
*				- VS/PS ���ε� �� ��� ���� ���ε�
*				- ����� ��ȸ: ��Ƽ���� SRV ���ε� �� DrawIndexed(indexCount, startIndex)
*				- ������� ���ٸ� ��ü DrawIndexed ����
*			6) ����
*				- ������ D3D ���ҽ� ����(Release)
*
*		- ��ǥ��/���ε�
*			DirectX 11�� �޼� ��ǥ��(LH). Assimp ConvertToLeftHanded ���.
*			FBX/���� ���� ���̿� ���� ���ε� ������ ���ɼ� �� Cull None ����.
*/

#include "App.h"
#include "../Common/Helper.h"
#include <d3dcompiler.h>
#include <directxtk/WICTextureLoader.h>
#include <thread>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

/*
* @brief : �� �ʱ�ȭ(������ ȯ�� + ȿ�� + ���)
* @details :
* 	- InitD3D 	: ����̽�/���ؽ�Ʈ/����ü��/RTV/DSV/������ ����
* 	- InitEffect	: VS/PS ������ �� �Է� ���̾ƿ� ����
* 	- InitScene 	: FBX �ε� �� �����/�ؽ�ó �غ� �� VB/IB/CB ����
*/
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

	// �ý��� ���� ���� (GPU)
	{
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		if (SUCCEEDED(m_pDevice->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()))))
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
			if (SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf())))
			{
				DXGI_ADAPTER_DESC desc{};
				if (SUCCEEDED(adapter->GetDesc(&desc)))
				{
					m_GPUName = desc.Description;
				}
				adapter.As(&m_Adapter3);
				if (m_Adapter3)
				{
					DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
					if (SUCCEEDED(m_Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)))
					{
						m_VideoMemoryTotal = memInfo.Budget; // ��� ���� ������ �ѷ� �ٻ�� ���
					}
				}
			}
		}
	}

	// �ý��� ���� ���� (CPU)
	{
		wchar_t cpuName[128] = L"";
		DWORD size = sizeof(cpuName);
		HKEY hKey;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			DWORD type = 0;
			RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, &type, reinterpret_cast<LPBYTE>(cpuName), &size);
			RegCloseKey(hKey);
		}
		m_CPUName = cpuName;
		m_CPUCores = std::thread::hardware_concurrency();
	}

	// �ý��� �޸� �ѷ�/���뷮 �ʱ�ȭ
	{
		MEMORYSTATUSEX ms{ sizeof(MEMORYSTATUSEX) };
		if (GlobalMemoryStatusEx(&ms))
		{
			m_RamTotal = ms.ullTotalPhys;
			m_RamAvail = ms.ullAvailPhys;
		}
	}

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

/*
* @brief : ������ ����(ī�޶�/���� ���, ���)
* @details :
* 	- world : ��Ʈ ��ġ �����̵� + Yaw ȸ��
* 	- view/proj : UI���� ������ ī�޶� �Ķ���� �ݿ�
* 	- FPS ǥ�ô� 1�� �������� ����
*/
void App::OnUpdate(const float& dt)
{
	static float t0 = 0.0f, t1 = 0.0f, t2 = 0.0f;
	t0 += 0.6f * dt;   // �θ�(��Ʈ) Yaw �ӵ�
	t1 += 1.0f * dt;   // �ι�° �޽�(�ڽ�1) Yaw �ӵ� (��Ʈ�� �ٸ���)
	t2 += 1.2f * dt;   // ����° �޽�(�ڽ�2) ���� �ӵ�

	// ���� �� ���� ��ȯ
	XMMATRIX world0 = XMMatrixRotationY(t0) * XMMatrixTranslation(m_RootPos.x, m_RootPos.y, m_RootPos.z);

	// View/Proj�� UI�� �ݿ� (�� ������)
	XMMATRIX view = XMMatrixTranspose(XMMatrixLookAtLH(
		XMVectorSet(m_CameraPos.x, m_CameraPos.y, m_CameraPos.z, 0.0f),
		XMVectorSet(m_CameraPos.x + 0.0f, m_CameraPos.y + 0.0f, m_CameraPos.z + 1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
	float fovRad = XMConvertToRadians(m_CameraFovDeg);
	XMMATRIX proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovRad, AspectRatio(), m_CameraNear, m_CameraFar));

	m_CBuffer.world = XMMatrixTranspose(world0);
	m_CBuffer.view = view;
	m_CBuffer.proj = proj;

	// FPS 1�� ������Ʈ
	m_FpsTimer += dt;
	if (m_FpsTimer >= 1.0f)
	{
		m_LastFps = 1.0f / dt; // ������ �ֱ� �������� ���� ���
		m_FpsTimer = 0.0f;
	}

	// �ý��� �޸� ���뷮 ����
	{
		MEMORYSTATUSEX ms{ sizeof(MEMORYSTATUSEX) };
		if (GlobalMemoryStatusEx(&ms))
		{
			m_RamTotal = ms.ullTotalPhys;
			m_RamAvail = ms.ullAvailPhys;
		}
	}

	// VRAM ��뷮 ���� (������ ���)
	if (m_Adapter3)
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
		if (SUCCEEDED(m_Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)))
		{
			// Budget�� ��� ���� �ѵ�, CurrentUsage�� ���� ��뷮
			m_VideoMemoryTotal = memInfo.Budget;
			// ��뷮�� memInfo.CurrentUsage�� ���� ���, ǥ�� �� ��ȯ
		}
	}
}

// Render() �Լ��� �߿��� �κ��� �� ����ֽ��ϴ�. ���⸦ ���� �˴ϴ�
/*
* @brief : ������ ����(�����/��Ƽ���� �ؽ�ó ��� ��ο�)
* @details :
* 	- OM/IA : Ŭ����, ��������, VB/IB, �Է� ���̾ƿ�
* 	- VS/PS : ���̴� ���ε�, ��� ���� ������Ʈ
* 	- Draw  : ��Ƽ���� ����� ��ȸ �� SRV ���ε� �� DrawIndexed
*/
void App::OnRender()
{
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	UINT stride = sizeof(VertexPosTex);	// ����Ʈ ��
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
	{
		D3D11_MAPPED_SUBRESOURCE mapped{};
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0,
			D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy(mapped.pData, &m_CBuffer, sizeof(m_CBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);

		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
		uint32_t drawn = 0;
		for (const auto& s : m_Subsets)
		{
			ID3D11ShaderResourceView* srv = nullptr;
			if (s.materialIndex < m_MaterialSRVs.size()) srv = m_MaterialSRVs[s.materialIndex];
			m_pDeviceContext->PSSetShaderResources(0, 1, &srv);
			m_pDeviceContext->DrawIndexed(s.indexCount, s.startIndex, 0);
			drawn += s.indexCount;
		}
		if (m_Subsets.empty())
		{
			m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);
		}
	}

	// ImGui ������ �� UI ������
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (ImGui::Begin("Controls"))
	{
		ImGui::Text("Mesh Transforms");
		ImGui::DragFloat3("Root Pos (x,y,z)", &m_RootPos.x, 0.1f);
		ImGui::Separator();
		ImGui::Text("Camera");
		ImGui::DragFloat3("Camera Pos (x,y,z)", &m_CameraPos.x, 0.1f);
		ImGui::SliderFloat("Camera FOV (deg)", &m_CameraFovDeg, 30.0f, 120.0f);
		ImGui::DragFloatRange2("Near/Far", &m_CameraNear, &m_CameraFar, 0.1f, 0.01f, 5000.0f, "Near: %.2f", "Far: %.2f");
	}
	ImGui::End();

	// ���ϴ�: Cube Description
	{
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 size(260.0f, 80.0f);
		ImVec2 pos(10.0f, io.DisplaySize.y - size.y - 10.0f);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(size, ImGuiCond_Always);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
		if (ImGui::Begin("Cube Description", nullptr, flags))
		{
			ImGui::Text("front : Yuuka");
			ImGui::Text("etc   : Hanako");
		}
		ImGui::End();
	}

	// ����: �ý��� ����(FPS/GPU/CPU)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 size(420.0f, 180.0f);
		ImVec2 pos(io.DisplaySize.x - size.x - 10.0f, 10.0f);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(size, ImGuiCond_Always);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
		if (ImGui::Begin("System Info", nullptr, flags))
		{
			ImGui::Text("FPS : %.1f", m_LastFps);
			ImGui::Separator();
			ImGui::Text("GPU : %ls", m_GPUName.c_str());
			ImGui::Text("CPU : %ls (%u cores)", m_CPUName.c_str(), m_CPUCores);
			ImGui::Separator();
			// RAM
			double ramTotalGB = (double)m_RamTotal / (1024.0 * 1024.0 * 1024.0);
			double ramUsedGB = (double)(m_RamTotal - m_RamAvail) / (1024.0 * 1024.0 * 1024.0);
			ImGui::Text("RAM : %.2f GB / %.2f GB", ramUsedGB, ramTotalGB);
			// VRAM (Budget/Usage�� �����Ǵ� ���)
			if (m_Adapter3)
			{
				DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
				if (SUCCEEDED(m_Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)))
				{
					double vramBudgetGB = (double)memInfo.Budget / (1024.0 * 1024.0 * 1024.0);
					double vramUsageGB = (double)memInfo.CurrentUsage / (1024.0 * 1024.0 * 1024.0);
					ImGui::Text("VRAM : %.2f GB / %.2f GB", vramUsageGB, vramBudgetGB);
				}
			}
		}
		ImGui::End();
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	m_pSwapChain->Present(0, 0);
}

/*
* @brief : D3D11 �ʱ�ȭ
* @details :
* 	- ����ü��/����̽� ���� �� RTV/DSV ���ε�
* 	- DepthStencilState(LESS), Rasterizer(Cull None) ����
* 	- Viewport ����
*/
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

	// �����Ͷ����� ���� (Cull None)
	{
		D3D11_RASTERIZER_DESC rs{};
		rs.FillMode = D3D11_FILL_SOLID;
		rs.CullMode = D3D11_CULL_NONE; // PMX/�¼��� ��ȯ���� ���ε� �̽� ����
		rs.FrontCounterClockwise = FALSE;
		rs.DepthClipEnable = TRUE;
		HR_T(m_pDevice->CreateRasterizerState(&rs, &m_pRasterState));
		m_pDeviceContext->RSSetState(m_pRasterState);
	}

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
	SAFE_RELEASE(m_pRasterState);
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
/*
* @brief : ��� �ʱ�ȭ(FBX �ε�, ����, ����/�ؽ�ó �غ�)
* @details :
* 	- FBX �ε� �� ��� ��ȯ �����ϸ� ���� ����
* 	- ��Ƽ���� �ؽ�ó �ε�(�ܺ�/�Ӻ����), ���� �� ��� ����
* 	- �����(start/count/materialIndex) ���
* 	- AABB �߽��̵�/������ ����ȭ �� ī�޶� �ڵ� ����
* 	- VB/IB/CB �� ���÷� ����
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
	// FBX �ε� (Assimp)
	Assimp::Importer importer;
	std::string pathA(m_ModelPath.begin(), m_ModelPath.end());
	const aiScene* scene = importer.ReadFile(pathA,
		aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
		aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded);
	if (!scene || !scene->HasMeshes())
	{
		throw std::runtime_error("Failed to load FBX or no mesh found");
	}

	// ��� ��带 ��ȸ�Ͽ� ����޽ø� �����ϰ�, ��� ��ȯ�� ����
	auto transformPoint = [](const aiVector3D& v, const aiMatrix4x4& m) -> aiVector3D {
		aiVector3D r;
		r.x = v.x * m.a1 + v.y * m.b1 + v.z * m.c1 + m.d1;
		r.y = v.x * m.a2 + v.y * m.b2 + v.z * m.c2 + m.d2;
		r.z = v.x * m.a3 + v.y * m.b3 + v.z * m.c3 + m.d3;
		return r;
	};

	m_ModelVertices.clear();
	m_ModelIndices.clear();
	std::vector<aiVector3D> posRaw; posRaw.reserve(1024);

	m_Subsets.clear();
	m_MaterialSRVs.clear();
	m_MaterialSRVs.resize(scene->mNumMaterials, nullptr);

	// ��Ƽ���� �ؽ�ó �ε� (DIFFUSE)
	auto wstring_from_utf8 = [](const std::string& s){ return std::wstring(s.begin(), s.end()); };
	std::wstring baseDir = L"../Resource/haniwa/"; // �� ���� ����(���� ó��)
	for (unsigned m = 0; m < scene->mNumMaterials; ++m)
	{
		aiMaterial* mat = scene->mMaterials[m];
		aiString texPath;
		if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
		{
			std::wstring wpath = baseDir + wstring_from_utf8(texPath.C_Str());
			Microsoft::WRL::ComPtr<ID3D11Resource> res;
			if (SUCCEEDED(CreateWICTextureFromFile(m_pDevice, wpath.c_str(), res.GetAddressOf(), &m_MaterialSRVs[m])))
			{
				// loaded
			}
		}
	}

	// ��� ��ȸ���� ����� ���(��Ƽ���� ����)
	std::function<void(const aiNode*, const aiMatrix4x4&)> traverse;
	traverse = [&](const aiNode* node, const aiMatrix4x4& parent) {
		aiMatrix4x4 global = parent * node->mTransformation;
		for (unsigned mi = 0; mi < node->mNumMeshes; ++mi)
		{
			const aiMesh* mesh = scene->mMeshes[node->mMeshes[mi]];
			size_t baseIndex = m_ModelVertices.size();
			for (unsigned i = 0; i < mesh->mNumVertices; ++i)
			{
				aiVector3D p = transformPoint(mesh->mVertices[i], global);
				aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][i] : aiVector3D(0,0,0);
				m_ModelVertices.push_back({ XMFLOAT3(p.x, p.y, p.z), XMFLOAT2(uv.x, uv.y) });
				posRaw.push_back(p);
			}
			uint32_t start = (uint32_t)m_ModelIndices.size();
			for (unsigned f = 0; f < mesh->mNumFaces; ++f)
			{
				const aiFace& face = mesh->mFaces[f];
				if (face.mNumIndices == 3)
				{
					m_ModelIndices.push_back(static_cast<uint32_t>(baseIndex + face.mIndices[0]));
					m_ModelIndices.push_back(static_cast<uint32_t>(baseIndex + face.mIndices[1]));
					m_ModelIndices.push_back(static_cast<uint32_t>(baseIndex + face.mIndices[2]));
				}
			}
			uint32_t count = (uint32_t)m_ModelIndices.size() - start;
			m_Subsets.push_back({ start, count, mesh->mMaterialIndex });
		}
		for (unsigned ci = 0; ci < node->mNumChildren; ++ci)
		{
			traverse(node->mChildren[ci], global);
		}
	};
	traverse(scene->mRootNode, aiMatrix4x4());

	// AABB ��� �� �߽� �̵�/������ ����ȭ
	aiVector3D minP(FLT_MAX, FLT_MAX, FLT_MAX), maxP(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	for (auto& p : posRaw)
	{
		minP.x = std::min(minP.x, p.x); minP.y = std::min(minP.y, p.y); minP.z = std::min(minP.z, p.z);
		maxP.x = max(maxP.x, p.x); maxP.y = max(maxP.y, p.y); maxP.z = max(maxP.z, p.z);
	}
	aiVector3D center = (minP + maxP) * 0.5f;
	aiVector3D extent = (maxP - minP) * 0.5f;
	float radius = max(extent.x, max(extent.y, extent.z));
	float targetRadius = 2.0f;
	float scale = (radius > 0.0f) ? (targetRadius / radius) : 1.0f;
	for (auto& v : m_ModelVertices)
	{
		XMFLOAT3& p = v.pos;
		p.x = (p.x - center.x) * scale;
		p.y = (p.y - center.y) * scale;
		p.z = (p.z - center.z) * scale;
	}

	// ī�޶� �ڵ� �¾�
	m_RootPos = XMFLOAT3(0,0,0);
	m_CameraNear = 0.1f;
	m_CameraFar = max(1000.0f, targetRadius * 20.0f);
	m_CameraPos = XMFLOAT3(0.0f, targetRadius * 0.5f, -targetRadius * 2.0f);

	D3D11_BUFFER_DESC vbDesc = {};
	ZeroMemory(&vbDesc, sizeof(vbDesc));			// vbDesc�� 0���� ��ü �޸� ������ �ʱ�ȭ ��ŵ�ϴ�
	vbDesc.ByteWidth = (UINT)(m_ModelVertices.size() * sizeof(VertexPosTex));				// �迭 ��ü�� ����Ʈ ũ��
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA vbData = {};
	ZeroMemory(&vbData, sizeof(vbData));
	vbData.pSysMem = m_ModelVertices.data();						// ������ �Ҵ�.
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
	D3D11_BUFFER_DESC ibDesc = {};
	ZeroMemory(&ibDesc, sizeof(ibDesc));
	m_nIndices = (int)m_ModelIndices.size();	// �ε��� ���� ����.
	ibDesc.ByteWidth = (UINT)(m_ModelIndices.size() * sizeof(uint32_t));
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = m_ModelIndices.data();
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

	// ���� ī�޶�(View/Proj)
	m_CBuffer.world = XMMatrixIdentity();
	m_CBuffer.view = XMMatrixTranspose(XMMatrixLookAtLH(
		XMVectorSet(m_CameraPos.x, m_CameraPos.y, m_CameraPos.z, 0.0f),
		XMVectorSet(m_CameraPos.x + 0.0f, m_CameraPos.y + 0.0f, m_CameraPos.z + 1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	));
	float fovRad = XMConvertToRadians(m_CameraFovDeg);
	m_CBuffer.proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovRad, AspectRatio(), m_CameraNear, m_CameraFar));

	// ���÷�/�ؽ�ó�� ������� ���� (��� �ȼ� ���̴�)
 
	// ���÷� ����
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HR_T(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState));

	// �⺻ ��� �ؽ�ó ����(�ʿ��)
	{
		UINT white = 0xFFFFFFFF;
		D3D11_TEXTURE2D_DESC td{};
		td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_IMMUTABLE;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = &white; sd.SysMemPitch = sizeof(UINT);
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
		HR_T(m_pDevice->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
		D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
		srvd.Format = td.Format;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MostDetailedMip = 0;
		srvd.Texture2D.MipLevels = 1;
		ID3D11ShaderResourceView* whiteSRV = nullptr;
		HR_T(m_pDevice->CreateShaderResourceView(tex.Get(), &srvd, &whiteSRV));
		for (auto& p : m_MaterialSRVs) if (p == nullptr) p = whiteSRV;
	}

	return true;
}

void App::UninitScene()
{
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_pTextureSRVs[i]);
	for (auto& srv : m_MaterialSRVs) SAFE_RELEASE(srv);
	SAFE_RELEASE(m_pSamplerState);
}

/*
* @brief : ���̴�/�Է� ���̾ƿ� �ʱ�ȭ
* @details :
* 	- VS/PS ������ �� ��ü ����
* 	- POSITION/TEXCOORD �Է��� �޴� InputLayout ����
*/
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
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"TexVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
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
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"TexPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// �ȼ� ���̴� ���� ���̻� �ʿ����
	return true;
}
