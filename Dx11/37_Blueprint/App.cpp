#include "App.h"
#include "../Common/Vertex.h"
#include "../Common/Helper.h"
#include <d3dcompiler.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

// 노드안에서 줄바꿈 되는 그거 헬퍼
static void NodeHr(float w)
{
	auto* dl = ImGui::GetWindowDrawList();
	ImVec2 p = ImGui::GetCursorScreenPos();
	dl->AddLine(p, ImVec2(p.x + w, p.y), ImGui::GetColorU32(ImGuiCol_Separator));
	ImGui::Dummy(ImVec2(w, 1)); // 아이템 제출 + 줄바꿈
}

bool App::OnInitialize()
{
	if(!InitD3D() || !InitScene()) return false;

	// ImGui 초기화
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);

	// Node Editor 초기화
	ed::Config config;
	config.SettingsFile = "BlueprintEditor.json";
	m_pNodeEditorContext = ed::CreateEditor(&config);

	BuildInitialGraph();
	return true;
}

void App::OnUninitialize()
{
	// NodeEditor -> ImGui -> D3D 순서 권장
	if (m_pNodeEditorContext)
	{
		ed::DestroyEditor(m_pNodeEditorContext);
		m_pNodeEditorContext = nullptr;
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// 씬 릴리즈
	UninitScene();
	UninitD3D();
}

void App::OnUpdate(const float& dt)
{

}

void App::OnRender()
{
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &m_VertextBufferStride, &m_VertextBufferOffset);
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
	m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	RenderNodeEditor();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	m_pSwapChain->Present(0, 0);
}

bool App::InitD3D()
{
	//HRESULT hr = 0;
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;
	swapDesc.Windowed = true;
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;

	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	HR_T(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_pSwapChain, &m_pDevice, NULL, &m_pDeviceContext));

	ID3D11Texture2D* pBackBufferTexture = nullptr;
	HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));
	HR_T(m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_pRenderTargetView));
	SAFE_RELEASE(pBackBufferTexture);
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);

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

bool App::InitScene()
{
	//HRESULT hr = 0;
	VertexTriangle vertices[] =
	{
		{DirectX::XMFLOAT3(-0.5f,  0.5f, 0.5f), DirectX::XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f)},
		{DirectX::XMFLOAT3(0.5f,  0.5f, 0.5f), DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)},
		{DirectX::XMFLOAT3(-0.5f, -0.5f, 0.5f), DirectX::XMFLOAT4(1.0f, 0.2f, 1.0f, 1.0f)},
		{DirectX::XMFLOAT3(0.5f, -0.5f, 0.5f), DirectX::XMFLOAT4(0.0f, 0.6f, 1.0f, 1.0f)}
	};

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = sizeof(VertexTriangle) * ARRAYSIZE(vertices);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;	// �迭 ������ �Ҵ�.
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));
	m_VertextBufferStride = sizeof(VertexTriangle);		// ���ؽ� ���� ����
	m_VertextBufferOffset = 0;

	D3D11_INPUT_ELEMENT_DESC layout[] = // �Է� ���̾ƿ�.
	{   
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"37_BasicVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);

	WORD indices[] =
	{
		0, 1, 2,
		2, 1, 3
	};
	m_nIndices = ARRAYSIZE(indices);
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));

	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"37_BasicPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);
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

// ---------------- Node Editor Helpers ----------------

App::Pin* App::FindPin(ed::PinId id)
{
	if (!id) return nullptr;
	for (auto& n : m_Nodes)
	{
		for (auto& p : n->Inputs)  if (p.Id == id) return &p;
		for (auto& p : n->Outputs) if (p.Id == id) return &p;
	}
	return nullptr;
}

App::Node* App::FindNode(ed::NodeId id)
{
	if (!id) return nullptr;
	for (auto& n : m_Nodes)
		if (n->Id == id) return n.get();
	return nullptr;
}

bool App::IsPinLinkedAsInput(ed::PinId inputPin) const
{
	for (const auto& l : m_Links)
		if (l.EndPinId == inputPin) return true;
	return false;
}

bool App::CanCreateLink(const Pin* a, const Pin* b) const
{
	if (!a || !b) return false;
	if (a == b) return false;
	if (a->Parent == b->Parent) return false; // 같은 노드끼리 연결 금지

	// 핀 타입 체크
	if (a->Type != b->Type) return false;

	return true;
}

void App::RemoveLink(ed::LinkId linkId)
{
	m_Links.erase(
		std::remove_if(m_Links.begin(), m_Links.end(),
			[&](const Link& l) { return l.Id == linkId; }),
		m_Links.end());
}

void App::RemoveNode(ed::NodeId nodeId)
{
	Node* node = FindNode(nodeId);
	if (!node) return;

	// 노드에 속한 핀들로 연결된 링크 제거
	auto isPinOfNode = [&](ed::PinId pid) -> bool
	{
		for (auto& p : node->Inputs)  if (p.Id == pid) return true;
		for (auto& p : node->Outputs) if (p.Id == pid) return true;
		return false;
	};

	m_Links.erase(
		std::remove_if(m_Links.begin(), m_Links.end(),
			[&](const Link& l)
			{
				return isPinOfNode(l.StartPinId) || isPinOfNode(l.EndPinId);
			}),
		m_Links.end());

	// 노드 제거
	m_Nodes.erase(
		std::remove_if(m_Nodes.begin(), m_Nodes.end(),
			[&](const std::unique_ptr<Node>& n) { return n->Id == nodeId; }),
		m_Nodes.end());
}

App::Node* App::CreateSimpleNode(const char* name, const ImVec2& pos)
{
	auto node = std::make_unique<Node>();
	node->Id = NextId<ed::NodeId>();
	node->Name = name ? name : "Node";
	node->InitialPos = pos;

	// Input
	{
		Pin in;
		in.Id = NextId<ed::PinId>();
		in.Kind = ed::PinKind::Input;
		in.Type = PinType::Flow;
		in.Name = "In";
		in.Parent = node.get();
		node->Inputs.push_back(std::move(in));
	}

	// Output
	{
		Pin out;
		out.Id = NextId<ed::PinId>();
		out.Kind = ed::PinKind::Output;
		out.Type = PinType::Flow;
		out.Name = "Out";
		out.Parent = node.get();
		node->Outputs.push_back(std::move(out));
	}

	Node* raw = node.get();
	m_Nodes.push_back(std::move(node));
	return raw;
}

void App::BuildInitialGraph()
{
	m_Nodes.clear();
	m_Links.clear();
	m_NextId = 1;
	m_FirstFrame = true;

	CreateSimpleNode("Node A", ImVec2(40, 40));
	CreateSimpleNode("Node B", ImVec2(320, 120));
}

// 노드 에디터 UI 렌더링
void App::RenderNodeEditor()
{
	ImGui::SetNextWindowSize(ImVec2(900, 650), ImGuiCond_Once);
	ImGui::SetNextWindowPos(ImVec2(80, 60), ImGuiCond_Once);

	if (!ImGui::Begin("Blueprint Editor Demo"))
	{
		ImGui::End();
		return;
	}

	// 버튼으로 노드 추가
	if (ImGui::Button("Add Node"))
	{
		ed::SetCurrentEditor(m_pNodeEditorContext);
		const ImVec2 canvasPos = ed::ScreenToCanvas(ImGui::GetMousePos());
		CreateSimpleNode("New Node", canvasPos);
		ed::SetCurrentEditor(nullptr);
	}

	ImGui::SameLine();
	if (ImGui::Button("Reset Graph"))
		BuildInitialGraph();

	ImGui::Separator();

	// ---- Node Editor ----
	ed::SetCurrentEditor(m_pNodeEditorContext);
	ed::Begin("NodeEditor", ImVec2(0, 0));

	// 노드 렌더
	for (auto& nodePtr : m_Nodes)
	{
		Node& node = *nodePtr;

		// 최초 1회 위치 지정
		if (!node.PositionInitialized)
		{
			ed::SetNodePosition(node.Id, node.InitialPos);
			node.PositionInitialized = true;
		}

		ed::BeginNode(node.Id);

		ImGui::TextUnformatted(node.Name.c_str());
		//ImGui::Separator();

		float w = ImGui::CalcTextSize(node.Name.c_str()).x;
		NodeHr(w);

		// 왼쪽 Inputs, 오른쪽 Outputs
		ImGui::BeginGroup();
		for (auto& pin : node.Inputs)
		{
			ed::BeginPin(pin.Id, pin.Kind);
			ImGui::Text("-> %s", pin.Name.c_str());
			ed::EndPin();
		}
		ImGui::EndGroup();

		ImGui::SameLine(0.0f, 40.0f);

		ImGui::BeginGroup();
		for (auto& pin : node.Outputs)
		{
			ed::BeginPin(pin.Id, pin.Kind);
			ImGui::Text("%s ->", pin.Name.c_str());
			ed::EndPin();
		}
		ImGui::EndGroup();

		ed::EndNode();
	}

	// 링크 렌더 (중요: Start=Output, End=Input)
	for (const auto& link : m_Links)
		ed::Link(link.Id, link.StartPinId, link.EndPinId);

	// ---- 새 링크 생성 ----
	if (ed::BeginCreate())
	{
		ed::PinId startId, endId;
		if (ed::QueryNewLink(&startId, &endId))
		{
			Pin* a = FindPin(startId);
			Pin* b = FindPin(endId);

			if (!a || !b)
			{
				ed::RejectNewItem();
			}
			else
			{
				// 방향 정리: a=Output, b=Input
				if (a->Kind == ed::PinKind::Input && b->Kind == ed::PinKind::Output)
				{
					std::swap(a, b);
					std::swap(startId, endId);
				}

				const bool kindsOk = (a->Kind == ed::PinKind::Output && b->Kind == ed::PinKind::Input);
				if (!kindsOk)
				{
					ed::RejectNewItem(); // Input-Input / Output-Output 방지
				}
				else
				{
					// 정책: Input 핀은 1개 링크만 허용
					if (IsPinLinkedAsInput(b->Id))
					{
						ed::RejectNewItem();
					}
					else if (!CanCreateLink(a, b))
					{
						ed::RejectNewItem();
					}
					else
					{
						if (ed::AcceptNewItem())
						{
							Link l;
							l.Id = NextId<ed::LinkId>();
							l.StartPinId = startId; // Output
							l.EndPinId = endId;     // Input
							m_Links.push_back(l);
						}
					}
				}
			}
		}
		ed::EndCreate();
	}

	// ---- 링크/노드 삭제 ----
	if (ed::BeginDelete())
	{
		ed::LinkId linkId;
		while (ed::QueryDeletedLink(&linkId))
		{
			if (ed::AcceptDeletedItem())
				RemoveLink(linkId);
		}

		ed::NodeId nodeId;
		while (ed::QueryDeletedNode(&nodeId))
		{
			if (ed::AcceptDeletedItem())
				RemoveNode(nodeId);
		}

		ed::EndDelete();
	}

	if (m_FirstFrame)
	{
		ed::NavigateToContent(0.0f);
		m_FirstFrame = false;
	}

	ed::End();
	ed::SetCurrentEditor(nullptr);

	ImGui::End();
}
