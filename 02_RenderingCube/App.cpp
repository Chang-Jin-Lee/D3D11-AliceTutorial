/*
* @brief : d3d11로 큐브를 그리는 예제입니다
* @details :
* 
		-  큐브를 만듭니다
			//    5________ 6
			//    /|      /|
			//   /_|_____/ |
			//  1|4|_ _ 2|_|7
			//   | /     | /
			//   |/______|/
			//  0       3

		- 인덱스 배열은 다음을 참고하세요
		 DWORD indices[] = {
		// 앞쪽
		0, 1, 2,
		2, 3, 0,
		// 왼쪽
		4, 5, 1,
		1, 0, 4,
		// 맨 위
		1, 5, 6,
		6, 2, 1,
		// 뒤쪽에
		7, 6, 5,
		5, 4, 7,
		// 오른쪽
		3, 2, 6,
		6, 7, 3,
		// 맨 아래
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

	if(!InitEffect()) return false;

	if(!InitScene()) return false;

	return true;
}

void App::OnUninitialize()
{
	UninitD3D();
}

void App::OnUpdate(const float& dt)
{
	static float phi = 0.0f, theta = 0.0f;
	phi += 0.3f * dt, theta += 0.37f * dt;
	//m_CBuffer.world = XMMatrixTranspose(XMMatrixRotationX(phi) * XMMatrixRotationY(theta));
	m_CBuffer.world = XMMatrixTranspose(XMMatrixRotationY(theta));
	// 상수 버퍼를 업데이트하여 큐브를 돌립니다

	D3D11_MAPPED_SUBRESOURCE mappedData;
	HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
	memcpy_s(mappedData.pData, sizeof(m_CBuffer), &m_CBuffer, sizeof(m_CBuffer));
	m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
}

// Render() 함수에 중요한 부분이 다 들어있습니다. 여기를 보면 됩니다
void App::OnRender()
{
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	UINT stride = sizeof(VertexPosColor);	// 바이트 수
	UINT offset = 0;

	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	//m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 1 ~ 3 . IA 단계 설정
	// 정점을 어떻게 이어서 그릴 것인지를 선택하는 부분
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// 1. 버퍼를 잡아주기
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
	// 2. 입력 레이아웃을 잡아주기
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	// 3. 인덱스 버퍼를 잡아주기
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// 4. Vertex Shader 설정
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);

	// 5. Pixel Shader 설정
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	// 6. Constant Buffer 설정
	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);

	// 7. 그리기
	m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);

	m_pSwapChain->Present(0, 0);
}

bool App::InitD3D()
{
	//HRESULT hr = 0;

	// 스왑체인의 값들을 설정할 구조체를 만듭니다
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;
	swapDesc.Windowed = true;
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;

	// DXGI_RATIONAL? 
	// - 주사율을 위한 것
	// DXGI_RATIONAL 구조체는 유리수(분수) 값을 표현하기 위한 구조체
	// RefreshRate = Numerator / Denominator
	// 주사율(Refresh Rate, Hz 단위)을 지정합니다
	// 즉 밑의 두 줄은 60 / 1 = 60 → 60Hz를 의미합니다
	// 샘플 코드에서 60으로 두는 이유는 보편적인 60Hz 모니터 환경에 맞추기 위해서 입니다
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;

	// DXGI_SAMPLE_DESC?
	// - 멀티샘플링(Multi-Sampling Anti-Aliasing, MSAA) 설정을 위한 것
	// 한 픽셀을 몇 번 샘플링할 것인지 지정합니다
	// 즉, MSAA의 샘플 개수를 뜻합니다
	// Count가 1이면 멀티샘플링을 사용하지 않음을 의미합니다
	// 계단현상과 관련되어 있으니 잘 확인해볼 것!
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	// 디버그 창을 띄우기 위함입니다.
	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	/*
	* @brief  Direct3D 디바이스, 디바이스 컨텍스트, 스왑체인 생성
	* @details
	*   - Adapter        : NULL → 기본 GPU 사용
	*   - DriverType     : D3D_DRIVER_TYPE_HARDWARE → 하드웨어 가속
	*   - Flags          : creationFlags (디버그 모드 여부 포함)
	*   - SwapChainDesc  : 백버퍼, 주사율 등 스왑체인 설정
	*   - 반환           : m_pDevice, m_pDeviceContext, m_pSwapChain
	*/
	HR_T(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_pSwapChain, &m_pDevice, NULL, &m_pDeviceContext));

	/*
	* @brief  스왑체인 백버퍼로 RTV를 만들고 OM 스테이지에 바인딩한다
	* @details
	*   - GetBuffer(0): 백버퍼(ID3D11Texture2D)를 획득
	*   - CreateRenderTargetView: 백버퍼 기반 RTV 생성(리소스 내부 참조 증가)
	*   - 로컬 텍스처 포인터는 Release로 정리 (RTV가 수명 관리)
	*   - OMSetRenderTargets: 생성한 RTV를 렌더 타겟을 최종 출력 파이프라인에 바인딩
	*/
	ID3D11Texture2D* pBackBufferTexture = nullptr;
	HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));
	HR_T(m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_pRenderTargetView));
	SAFE_RELEASE(pBackBufferTexture);
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);

	/*
	* @brief  뷰포트(Viewport) 설정
	* @details
	*   - TopLeftX/Y : 출력 영역의 시작 좌표 (0,0 → 좌상단)
	*   - Width/Height : 윈도우 클라이언트 크기 기준
	*   - MinDepth/MaxDepth : 깊이 범위 (보통 0.0 ~ 1.0)
	*   - RSSetViewports : 래스터라이저 스테이지에 뷰포트 바인딩
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
 * @brief InitScene() 전체 흐름
 *   1. 정점(Vertex) 배열을 GPU 버퍼로 생성
 *   2. VS 입력 시그니처에 맞춰 InputLayout 생성
 *   3. VS 바이트코드로 Vertex Shader 생성 및 버퍼 해제
 *   4. 인덱스 버퍼(Index Buffer) 생성
 *   5. PS 바이트코드로 Pixel Shader 생성 및 버퍼 해제
 */
bool App::InitScene()
{
	//HRESULT hr = 0;

	/*
	* @brief  정점(Vertex) 배열을 GPU 버퍼로 생성
	* @details
	*   - ByteWidth : 정점 전체 크기(정점 크기 × 개수)
	*   - BindFlags : D3D11_BIND_VERTEX_BUFFER
	*   - Usage     : DEFAULT (일반적 용도)
	*   - 초기 데이터 : vbData.pSysMem = vertices
	*   - Stride/Offset : IASetVertexBuffers용 파라미터
	*   - 주의 : VertexInfo(color=Vec3), 셰이더/InputLayout의 COLOR 형식 일치 필요
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
	ZeroMemory(&vbDesc, sizeof(vbDesc));			// vbDesc에 0으로 전체 메모리 영역을 초기화 시킵니다
	vbDesc.ByteWidth = sizeof vertices;				// 배열 전체의 바이트 크기를 바로 반환합니다
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA vbData = {};
	ZeroMemory(&vbData, sizeof(vbData));
	vbData.pSysMem = vertices;						// 배열 데이터 할당.
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));


	/*
	* @brief  인덱스 버퍼(Index Buffer) 생성
	* @details
	*   - indices: 정점 재사용용 (사각형 = 삼각형 2개 = 인덱스 6개)
	*   - WORD 타입 → DXGI_FORMAT_R16_UINT 사용 예정
	*   - ByteWidth : 전체 인덱스 배열 크기
	*   - BindFlags : D3D11_BIND_INDEX_BUFFER
	*   - Usage     : DEFAULT (GPU 일반 접근)
	*   - 이 코드의 결과: m_pIndexBuffer 생성, m_nIndices에 개수 저장
	*/
	DWORD indices[] = {
		// 앞쪽
		0, 1, 2,
		2, 3, 0,
		// 왼쪽
		4, 5, 1,
		1, 0, 4,
		// 맨 위
		1, 5, 6,
		6, 2, 1,
		// 뒤쪽에
		7, 6, 5,
		5, 4, 7,
		// 오른쪽
		3, 2, 6,
		6, 7, 3,
		// 맨 아래
		4, 0, 3,
		3, 7, 4
	};
	D3D11_BUFFER_DESC ibDesc = {};
	ZeroMemory(&ibDesc, sizeof(ibDesc));
	m_nIndices = ARRAYSIZE(indices);	// 인덱스 개수 저장.
	ibDesc.ByteWidth = sizeof indices;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));


	// ******************
	// 상수 버퍼 설정
	//
	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(ConstantBuffer);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	// 초기 데이터를 사용하지 않고 새로운 상수 버퍼를 만듭니다
	HR_T(m_pDevice->CreateBuffer(&cbd, nullptr, &m_pConstantBuffer));


	m_CBuffer.world = XMMatrixIdentity();	// 단위 매트릭스의 조기는 그 자체입니다

	m_CBuffer.view = XMMatrixTranspose(XMMatrixLookAtLH(
		XMVectorSet(0.0f, 0.0f, -5.0f, 0.0f),
		XMVectorSet(0.0f + 0.0f, 0.0f + 0.0f, -5.0f + 1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	));
	m_CBuffer.proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(XM_PIDIV2, AspectRatio(), 1.0f, 1000.0f));


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
	* @brief  VS 입력 시그니처에 맞춰 InputLayout 생성
	* @details
	*   - POSITION: float3, COLOR: float4 (구조체/셰이더와 형식·오프셋 일치 필수)
	*   - InputSlot=0, Per-Vertex 데이터
	*   - D3D11_APPEND_ALIGNED_ELEMENT로 COLOR 오프셋 자동 계산
	*   - VS 바이트코드(CompileShaderFromFile)로 시그니처 매칭 후 CreateInputLayout
	*/
	D3D11_INPUT_ELEMENT_DESC layout[] = // 입력 레이아웃.
	{
		// 포맷 { SemanticName , SemanticIndex , Format , InputSlot , AlignedByteOffset , InputSlotClass , InstanceDataStepRate	}
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"02_BasicVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));

	/*
	* @brief  VS 바이트코드로 Vertex Shader 생성 및 컴파일 버퍼 해제
	* @details
	*   - CreateVertexShader: 컴파일된 바이트코드(pointer/size)로 VS 객체 생성
	*   - ClassLinkage 미사용(NULL)
	*   - 생성 후, 더 이상 필요 없는 vertexShaderBuffer는 해제
	*/
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// 컴파일 버퍼 해제


	// Pixel Shader -------------------------------------
	/*
	* @brief  PS 바이트코드로 Pixel Shader 생성 및 컴파일 버퍼 해제
	* @details
	*   - CompileShaderFromFile: "BasicPixelShader.hlsl", entry="main", profile="ps_4_0"
	*   - CreatePixelShader: 바이트코드(pointer/size)로 PS 객체 생성 (ClassLinkage 미사용)
	*   - 생성 후 pixelShaderBuffer는 더 이상 필요 없으므로 해제
	*/
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"02_BasicPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음
	return true;
}
