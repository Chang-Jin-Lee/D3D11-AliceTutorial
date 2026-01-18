#pragma once
#include <windows.h>
#include "../Common/GameApp.h"
#include <d3d11.h>
#include <directxtk/SimpleMath.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <imgui_node_editor.h>

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

using namespace DirectX::SimpleMath;

namespace ed = ax::NodeEditor;

class App : public GameApp
{
public:
	// D3D 관련 멤버
	ID3D11Device* m_pDevice = nullptr;
	ID3D11DeviceContext* m_pDeviceContext = nullptr;
	IDXGISwapChain* m_pSwapChain = nullptr;
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;

	ID3D11VertexShader* m_pVertexShader = nullptr;
	ID3D11PixelShader* m_pPixelShader = nullptr;
	ID3D11InputLayout* m_pInputLayout = nullptr;
	ID3D11Buffer* m_pVertexBuffer = nullptr;
	UINT m_VertextBufferStride = 0;
	UINT m_VertextBufferOffset = 0;
	ID3D11Buffer* m_pIndexBuffer = nullptr;
	int m_nIndices = 0;

	// Node Editor 관련
	ed::EditorContext* m_pNodeEditorContext = nullptr;

	// 안정적인 ID 생성기
	int m_NextId = 1;
	template<typename T>
	T NextId() { return T(m_NextId++); }

	enum class PinType : uint8_t { Flow };

	struct Node;
	struct Pin
	{
		ed::PinId    Id = 0;
		ed::PinKind  Kind = ed::PinKind::Input;
		PinType      Type = PinType::Flow;
		std::string  Name;
		Node*        Parent = nullptr;
	};

	struct Node
	{
		ed::NodeId          Id = 0;
		std::string         Name;
		std::vector<Pin>    Inputs;
		std::vector<Pin>    Outputs;

		ImVec2              InitialPos = ImVec2(0, 0);
		bool                PositionInitialized = false;
	};

	struct Link
	{
		ed::LinkId  Id = 0;
		ed::PinId   StartPinId = 0; // Output
		ed::PinId   EndPinId = 0;   // Input
	};

	std::vector<std::unique_ptr<Node>> m_Nodes;
	std::vector<Link> m_Links;

	bool m_FirstFrame = true;

public:
	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate(const float& dt) override;
	void OnRender() override;

	bool InitD3D();
	void UninitD3D();

	bool InitScene();
	void UninitScene();

	// Node Editor API
	void BuildInitialGraph();
	Node* CreateSimpleNode(const char* name, const ImVec2& pos);
	Pin*  FindPin(ed::PinId id);
	Node* FindNode(ed::NodeId id);

	bool  IsPinLinkedAsInput(ed::PinId inputPin) const;
	bool  CanCreateLink(const Pin* a, const Pin* b) const;

	void  RemoveLink(ed::LinkId linkId);
	void  RemoveNode(ed::NodeId nodeId);

	void RenderNodeEditor();
};
