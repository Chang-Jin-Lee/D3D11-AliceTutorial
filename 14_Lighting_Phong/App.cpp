/*
* @brief : 
* @details :Blinn-Phong ���� ����� ���� ��� �����Դϴ�.
*	 - Material�� �߰��߽��ϴ�.
* 	 - ���� ����� ���� ��� ���۸� Ȯ���߽��ϴ�.
*/

#include "App.h"
#include "../Common/Helper.h"
#include <d3dcompiler.h>
#include <directxtk/WICTextureLoader.h>
#include <directxtk/DDSTextureLoader.h>
#include <thread>
#include <filesystem>
#include <algorithm>
#include "../Common/StaticMesh.h"
#include "../Common/LineRenderer.h"
#include "../Common/Skybox.h"

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

static bool LoadTextureSRVAndSize(ID3D11Device* device, const std::wstring& path,
	ID3D11ShaderResourceView** outSRV, ImVec2* outSize)
{
	if (!std::filesystem::exists(path)) return false;
	if (FAILED(CreateTextureFromFile(device, path.c_str(), outSRV))) return false;
	if (outSRV && *outSRV)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> res;
		(*outSRV)->GetResource(res.GetAddressOf());
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
		if (SUCCEEDED(res.As(&tex2D)))
		{
			D3D11_TEXTURE2D_DESC desc{};
			tex2D->GetDesc(&desc);
			if (outSize) *outSize = ImVec2((float)desc.Width, (float)desc.Height);
		}
	}
	return true;
}


void App::PrepareSkyFaceSRVs()
{
	// �ٸ� ��ī�̹ڽ��� �ٲ� ���� ������ �����ϰ� �ٽ� �ε�
	for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_pSkyFaceSRV[i]);
    m_SkyFaceSize = ImVec2(0, 0);
    if (!m_pTextureSRV) return;

	Microsoft::WRL::ComPtr<ID3D11Resource> res;
    m_pTextureSRV->GetResource(res.GetAddressOf());
    if (!res) return;

	// �ı��ƴ��� �ȵƴ��� �Ǵ��� ���� Comptr�� �ʿ��ϴ�
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
	HR_T(res.As(&tex2D));

    D3D11_TEXTURE2D_DESC desc{};
    tex2D->GetDesc(&desc);
    // ť����� 6���� array slice�� ����. (���� ť��� 6�� ���)
    if ((desc.ArraySize < 6)) return;

    // ũ�� ��� (mip0 ����)
    m_SkyFaceSize = ImVec2((float)desc.Width, (float)desc.Height);

    for (UINT face = 0; face < 6; ++face)
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = desc.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        sd.Texture2DArray.MostDetailedMip = 0;
        sd.Texture2DArray.MipLevels = desc.MipLevels;
        sd.Texture2DArray.FirstArraySlice = face;
        sd.Texture2DArray.ArraySize = 1;
        ID3D11ShaderResourceView* faceSRV = nullptr;
        if (SUCCEEDED(m_pDevice->CreateShaderResourceView(tex2D.Get(), &sd, &faceSRV)))
        {
            m_pSkyFaceSRV[face] = faceSRV;
        }
    }
}

void App::ChangeSkyboxDDS(const wchar_t* ddsPath)
{
    if (m_Skybox)
    {
        if (m_Skybox->ChangeDDS(m_pDevice, ddsPath))
        {
            // also set for face view and PS binding
            m_pTextureSRV = m_Skybox->GetTexture();
            PrepareSkyFaceSRVs();
            wcscpy_s(m_CurrentSkyboxPath, ddsPath);
        }
    }
}

bool App::OnInitialize()
{
	if(!InitD3D()) return false;

	if(!InitBasicEffect()) return false;
	if(!InitSkyBoxEffect()) return false;

	if(!InitScene()) return false;
	if(!InitImGui()) return false;

	if (!InitTexture()) return false;

	if (!m_SystemInfo.InitSysInfomation(m_pDevice)) return false;

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
	// ���� ��ȯ ���� (���� Scene Graph)
	if(m_RotateCube)
	{
		// ť�� Yaw(��)�� �ʴ� 45�� ȸ��
		m_cubeRotation.y += 45.0f * dt;
		// -180~180 ����
		m_cubeRotation.y = std::fmod(m_cubeRotation.y + 180.0f, 360.0f) - 180.0f;
	}
	// ť�� ���� ȸ���� ��� (Yaw/Pitch/Roll, deg -> rad)
	XMMATRIX rotYaw   = XMMatrixRotationY(XMConvertToRadians(m_cubeRotation.y));
	XMMATRIX rotPitch = XMMatrixRotationX(XMConvertToRadians(m_cubeRotation.x));
	XMMATRIX rotRoll  = XMMatrixRotationZ(XMConvertToRadians(m_cubeRotation.z));
	XMMATRIX S = XMMatrixScaling(m_CubeScale, m_CubeScale, m_CubeScale);
	XMMATRIX local0 = S * rotPitch * rotYaw * rotRoll * XMMatrixTranslation(m_cubePos.x, m_cubePos.y, m_cubePos.z); // ��Ʈ
	XMMATRIX world0 = local0; // ��Ʈ

	// Camera update via Camera class (RMB look + WASD/E/Q movement)
	ImGuiIO& io = ImGui::GetIO();
	bool rmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.WantCaptureMouse;
	bool keyW = ImGui::IsKeyDown(ImGuiKey_W);
	bool keyS = ImGui::IsKeyDown(ImGuiKey_S);
	bool keyA = ImGui::IsKeyDown(ImGuiKey_A);
	bool keyD = ImGui::IsKeyDown(ImGuiKey_D);
	bool keyE = ImGui::IsKeyDown(ImGuiKey_E);
	bool keyQ = ImGui::IsKeyDown(ImGuiKey_Q);
	m_camera.UpdateFromUI(rmbDown && !io.WantCaptureKeyboard, io.MouseDelta.x, io.MouseDelta.y, keyW, keyS, keyA, keyD, keyE, keyQ, dt);

	XMFLOAT3 camForward3 = m_camera.GetForward();

	// View/Proj from Camera
	XMMATRIX view = XMMatrixTranspose(m_camera.GetViewMatrixXM());
	XMMATRIX proj = XMMatrixTranspose(m_camera.GetProjMatrixXM());
	m_baseProjection.world = XMMatrixTranspose(world0);
	m_baseProjection.view = view;
	m_baseProjection.proj = proj;

	m_baseProjection.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(world0)));
	{
		XMFLOAT3 dir = m_DirLight.direction;
		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&dir));
		XMStoreFloat3(&dir, v);
		// DirectionalLight �ʵ� ���� (����ȭ�� ����)
		m_baseProjection.dirLight = m_DirLight;
		m_baseProjection.dirLight.direction = dir;
		m_baseProjection.dirLight.pad = 0.0f;
	}
	m_baseProjection.eyePos = m_camera.GetPosition();
	m_baseProjection.pad = 0.0f;

	// ��Ƽ������ �⺻ ĳ�ÿ� �ݿ��� �д�
	m_baseProjection.material = m_Material;

	m_SystemInfo.Tick(dt);
}

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{
	return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}
inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) 
{
	return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

// Render() �Լ��� �߿��� �κ��� �� ����ֽ��ϴ�. ���⸦ ���� �˴ϴ�
void App::OnRender()
{
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	UINT stride = m_VertextBufferStride;	// ����Ʈ ��
	UINT offset = m_VertextBufferOffset;

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

	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	m_ConstantBuffer.world = m_baseProjection.world;
	m_ConstantBuffer.view  = m_baseProjection.view;
	m_ConstantBuffer.proj  = m_baseProjection.proj;
	{
		// worldInvTranspose ��� (CPU������ ���� world�� ��/��ġ ��, HLSL ����Ʈ�������� �Ծ࿡ �°� ����)
		// ��յ� �������� �ذ��� �ڵ�
		auto invWorlNormal = XMMatrixInverse(nullptr, m_baseProjection.world);
		m_ConstantBuffer.worldInvTranspose = XMMatrixTranspose(invWorlNormal);
	}
	// ���� �⺻ ����/ī�޶� (UI �ݿ�)
	{
		XMFLOAT3 dir = m_DirLight.direction;
		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&dir));
		XMStoreFloat3(&dir, v);
		// DirectionalLight �ʵ� ���� (����ȭ�� ����)
		m_ConstantBuffer.dirLight = m_DirLight;
		m_ConstantBuffer.dirLight.direction = dir;
		m_ConstantBuffer.dirLight.pad = 0.0f;
	}
	m_ConstantBuffer.eyePos = m_camera.GetPosition();
	m_ConstantBuffer.pad = 0.0f;
	// ��Ƽ���� ä���
	m_ConstantBuffer.material = m_Material;

	D3D11_MAPPED_SUBRESOURCE mappedData;
	HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
	memcpy_s(mappedData.pData, sizeof(ConstantBuffer), &m_ConstantBuffer, sizeof(ConstantBuffer));
	m_pDeviceContext->Unmap(m_pConstantBuffer, 0);

	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    // ť����� t1 ���Կ� ���ε� (�ȼ� ���̴����� g_TexCube : t1)
    if (m_pTextureSRV)
    {
        ID3D11ShaderResourceView* texCube = m_pTextureSRV;
        m_pDeviceContext->PSSetShaderResources(1, 1, &texCube);
    }

	// PNG ���� �ݿ�: ���� ON
	FLOAT blendFactor[4] = { 0,0,0,0 };
	UINT sampleMask = 0xFFFFFFFF;
	m_pDeviceContext->OMSetBlendState(m_pAlphaBlendState, blendFactor, sampleMask);
	for (int face = 0; face < 6; ++face)
	{
		ID3D11ShaderResourceView* srv = m_pCubeTextureSRVs[face];
		m_pDeviceContext->PSSetShaderResources(0, 1, &srv);
		m_pDeviceContext->DrawIndexed(6, face * 6, 0);
	}
	// �ʿ� ��: ���� OFF�� ����
	m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

	// ����Ʈ ��ġ ��Ŀ ť�� �׸��� (���� ������, ���)
	{
		ConstantBuffer marker = m_ConstantBuffer;
		XMMATRIX S = XMMatrixScaling(1.0f, 1.0f, 1.0f);
		XMMATRIX T = XMMatrixTranslation(m_LightPosition.x, m_LightPosition.y, m_LightPosition.z);
		marker.world = XMMatrixTranspose(S * T);
		marker.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(S * T)));
		marker.pad = 2.0f; // PS���� ��� ��� ���

		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &marker, sizeof(ConstantBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		UINT dbgStride = sizeof(VertexData);
		UINT dbgOffset = 0;
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pDebugBoxVB, &dbgStride, &dbgOffset);
		m_pDeviceContext->IASetInputLayout(m_pInputLayout);
		m_pDeviceContext->IASetIndexBuffer(m_pDebugBoxIB, DXGI_FORMAT_R32_UINT, 0);
		m_pDeviceContext->DrawIndexed(m_DebugBoxIndexCount, 0, 0);
	}

	// ����Ʈ ���� ǥ�� ���� �׸���(������)
	{
		// pad=3.0�� ���� ����׿� �̿�.
		ConstantBuffer lineCB = m_ConstantBuffer;
		lineCB.world = XMMatrixTranspose(XMMatrixIdentity());
		lineCB.worldInvTranspose = XMMatrixIdentity();
		lineCB.pad = 3.0f;
		D3D11_MAPPED_SUBRESOURCE mappedLine;
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedLine));
		memcpy_s(mappedLine.pData, sizeof(ConstantBuffer), &lineCB, sizeof(ConstantBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		// light direction (red)
		m_LineRenderer->DrawLightDirection(m_pDeviceContext, m_LightPosition, m_DirLight.direction, 2.0f, m_pInputLayout, m_pVertexShader, m_pPixelShader, m_pConstantBuffer);
		// symmetric axes centered at origin for better grid feel
		m_LineRenderer->DrawAxesSymmetric(m_pDeviceContext, 100.0f, m_pInputLayout, m_pVertexShader, m_pPixelShader, m_pConstantBuffer);
	}


    // SkyBox ������ (���� ����/����)
	{
		UINT stride = m_VertextBufferStride;
		UINT offset = m_VertextBufferOffset;
		m_Skybox->Render(m_pDeviceContext, m_pVertexBuffer, m_pIndexBuffer, m_nIndices, stride, offset, m_baseProjection.view, m_baseProjection.proj);
	}

	// ȭ�� �������� ��(NDC)�� �۰� ǥ��
	{
		// pad=3 ���� ���������� ��µǵ���
		ConstantBuffer overlayCB = m_ConstantBuffer;
		overlayCB.world = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.view = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.proj = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.worldInvTranspose = XMMatrixIdentity();
		overlayCB.pad = 3.0f;
		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &overlayCB, sizeof(ConstantBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		// �»�� ���� �� ǥ��
		m_LineRenderer->DrawAxesOverlay(m_pDeviceContext, XMMatrixTranspose(m_baseProjection.view), DirectX::XMFLOAT2(-0.9f, 0.85f), 0.08f, m_pInputLayout, m_pVertexShader, m_pPixelShader, m_pConstantBuffer);
	}

	// ImGui ������ �� UI ������
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (ImGui::Begin("Controls"))
	{
		// SkyBox ����
		{
			int cur = (m_SkyBoxChoice == SkyBoxChoice::Hanako) ? 0 : 1;
			const char* items[] = { "Hanako.dds", "cubemap.dds" };
			if (ImGui::Combo("SkyBox Choice", &cur, items, IM_ARRAYSIZE(items)))
			{
				m_SkyBoxChoice = (cur == 0) ? SkyBoxChoice::Hanako : SkyBoxChoice::CubeMap;
				const wchar_t* path = (m_SkyBoxChoice == SkyBoxChoice::Hanako) ? L"..\\Resource\\Hanako.dds" : L"..\\Resource\\cubemap.dds";
				ChangeSkyboxDDS(path);
			}
		}
		ImGui::Text("Mesh Transforms");
		ImGui::Checkbox("Rotate Cube", &m_RotateCube);
		ImGui::SliderFloat("Cube Scale", &m_CubeScale, 0.1f, 10.0f);
		ImGui::DragFloat3("Cube Pos (x,y,z)", &m_cubePos.x, 0.1f);
		// ť�� ȸ��(��) ����
		ImGui::DragFloat3("Cube Rotation (deg)", &m_cubeRotation.x, 1.0f, -360.0f, 360.0f, "%.1f");
		ImGui::Separator();
		ImGui::Text("Camera");
		{
			if (ImGui::Button("Reset"))
			{
				m_camera.Reset();
			}
			DirectX::XMFLOAT3 pos = m_camera.GetPosition();
			if (ImGui::DragFloat3("Camera Pos (x,y,z)", &pos.x, 0.1f))
			{
				m_camera.SetPosition(pos);
			}
			float fovDeg = XMConvertToDegrees(m_camera.GetFovYRad());
			if (ImGui::SliderFloat("Camera FOV (deg)", &fovDeg, 30.0f, 120.0f))
			{
				m_camera.SetFrustum(XMConvertToRadians(fovDeg), AspectRatio(), m_camera.GetNearZ(), m_camera.GetFarZ());
			}
			float nearZ = m_camera.GetNearZ();
			float farZ  = m_camera.GetFarZ();
			if (ImGui::DragFloatRange2("Near/Far", &nearZ, &farZ, 0.1f, 0.01f, 5000.0f, "Near: %.2f", "Far: %.2f"))
			{
				m_camera.SetFrustum(m_camera.GetFovYRad(), AspectRatio(), nearZ, farZ);
			}
		}
		ImGui::Separator();
		ImGui::Text("Light");
		ImGui::DragFloat3("Light Direction", &m_DirLight.direction.x, 0.05f);
		ImGui::ColorEdit4("Ambient", &m_DirLight.ambient.x);
		ImGui::ColorEdit4("Diffuse", &m_DirLight.diffuse.x);
		ImGui::ColorEdit4("Specular", &m_DirLight.specular.x);
		ImGui::Separator();
        ImGui::Text("Material");
        ImGui::ColorEdit4("Ambient (ka)", &m_Material.ambient.x);
        ImGui::ColorEdit4("Diffuse (kd)", &m_Material.diffuse.x);
        ImGui::ColorEdit4("Specular (ks)", &m_Material.specular.x);
		ImGui::DragFloat("Shininess (alpha)", &m_Material.specular.w, 0.05f, 1.0f, 256.0f);
        ImGui::ColorEdit4("Reflect (kr, a=roughness)", &m_Material.reflect.x);
	}
	ImGui::End();

	// ���� ī�޶� ������ ���� ��ī�̹ڽ� �� �̹����� ǥ��
	{
		int face = 0;
		using namespace DirectX;
		XMFLOAT3 fwd = m_camera.GetForward();
    	XMVECTOR f = XMLoadFloat3(&fwd);
    	XMVECTOR fn = XMVector3Normalize(f);
    	XMFLOAT3 v; XMStoreFloat3(&v, fn);
    	float ax = fabsf(v.x), ay = fabsf(v.y), az = fabsf(v.z);
    	if (ax >= ay && ax >= az) face = (v.x >= 0.0f) ? 0 : 1; // +X / -X
    	else if (ay >= ax && ay >= az) face = (v.y >= 0.0f) ? 2 : 3; // +Y / -Y
    	else face =  (v.z >= 0.0f) ? 4 : 5;     // +Z / -Z

		ID3D11ShaderResourceView* faceSRV = (face >= 0 && face < 6) ? m_pSkyFaceSRV[face] : nullptr;
		if (faceSRV)
		{
			ImGui::SetNextWindowPos(ImVec2(810, 210), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(m_HanakoDrawSize.x + 50, m_HanakoDrawSize.y + 80), ImGuiCond_Once);
			if (ImGui::Begin("Skybox Face"))
			{
				ImGui::BeginChild("SkyFaceView", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
				const ImVec2 tex = (m_HanakoDrawSize.x > 0 && m_HanakoDrawSize.y > 0) ? m_HanakoDrawSize : m_SkyFaceSize;
				ImVec2 avail = ImGui::GetContentRegionAvail();
				float sx = (tex.x > 0.f) ? (avail.x / tex.x) : 1.f;
				float sy = (tex.y > 0.f) ? (avail.y / tex.y) : 1.f;
				float scale = (sx > 0.f && sy > 0.f) ? min(sx, sy) : 1.f;
				ImVec2 draw = ImVec2(tex.x * scale, tex.y * scale);
				ImVec2 start = ImGui::GetCursorPos();
				ImVec2 offset = ImVec2((avail.x - draw.x) * 0.5f, (avail.y - draw.y) * 0.5f);
				ImGui::SetCursorPos(start + offset);
				ImGui::Image((ImTextureID)faceSRV, draw);
				ImVec2 r0 = ImGui::GetItemRectMin();
				ImVec2 r1 = ImGui::GetItemRectMax();
				ImGui::GetWindowDrawList()->AddRect(r0 - ImVec2(2, 2), r1 + ImVec2(2, 2), IM_COL32(255, 255, 255, 160), 8.0f, 0, 2.0f);
				ImGui::EndChild();
			}
			ImGui::End();
		}
	}


	m_SystemInfo.RenderUI();

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	m_pSwapChain->Present(0, 0);
}

bool App::InitD3D()
{
	HRESULT hr = S_OK;

	// ����ü���� ������ ������ ����ü�� ����ϴ�
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

	// �ø� ���� (��� ������/�ĸ� �ø�)
	CD3D11_RASTERIZER_DESC rasterizerDesc(CD3D11_DEFAULT{});
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.FrontCounterClockwise = false;
	HR_T(m_pDevice->CreateRasterizerState(&rasterizerDesc, &RSNoCull));
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FrontCounterClockwise = true;
	HR_T(m_pDevice->CreateRasterizerState(&rasterizerDesc, &RSCullClockWise));

	// ���� ���� ���� ���� (SrcAlpha/InvSrcAlpha)
	{
		D3D11_BLEND_DESC bd{};
		bd.AlphaToCoverageEnable = FALSE;
		bd.IndependentBlendEnable = FALSE;
		D3D11_RENDER_TARGET_BLEND_DESC& rt = bd.RenderTarget[0];
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		HR_T(m_pDevice->CreateBlendState(&bd, &m_pAlphaBlendState));
	}
 
	return true;
}

void App::UninitD3D()
{
	SAFE_RELEASE(m_pDepthStencilState);
	SAFE_RELEASE(m_pDepthStencilView);
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pAlphaBlendState);
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
	ID3D10Blob* errorMessage = nullptr;	 // ���� �޽����� ������ ����.

	// ***********************************************************************************************
	// ť�꼳��
	// 24�� ���� (�� �� 4��) + �ؽ�ó ��ǥ
	m_VertextBufferStride = sizeof(VertexData);
	m_VertextBufferOffset = 0;
	// ***********************************************************************************************
	// ���� ť�� ������ ����
	// ���� �ִ� �Ͱ� �ε��� ���� ���� �ִ°��� CreateBox �Լ� �ȿ� �ֽ��ϴ�
	StaticMeshData cubeData = StaticMesh::CreateBox(XMFLOAT4(1, 1, 1, 1));
	StaticMesh::AssignMemory(m_pDevice, m_pVertexBuffer, cubeData);
	StaticMesh::AssignIndexMemory(m_pDevice, m_pIndexBuffer, cubeData, m_nIndices);

	// ***********************************************************************************************
	// ��� ���� ����
	//
	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(ConstantBuffer);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	// ���� ��� ���� ���� (VS/PS ����, b0)
	HR_T(m_pDevice->CreateBuffer(&cbd, nullptr, &m_pConstantBuffer));

		// ***********************************************************************************************
	// ��ī�� �ڽ� ť�� ����
    HR_T(CreateDDSTextureFromFile(m_pDevice, L"..\\Resource\\Hanako.dds", nullptr, &m_pSkyHanakoSRV));
    HR_T(CreateDDSTextureFromFile(m_pDevice, L"..\\Resource\\cubemap.dds", nullptr, &m_pSkyCubeMapSRV));
	m_SkyBoxChoice = SkyBoxChoice::CubeMap;
	m_pTextureSRV = m_pSkyCubeMapSRV;
	
	// ���÷� ����
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HR_T(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState));

	// ***********************************************************************************************
	// ī�޶� ����
	// ���� ī�޶�(View/Proj)�� 3���� ��� ���� ��Ʈ���� �غ��մϴ�
	m_baseProjection.world = XMMatrixIdentity();
	// Set camera initial frustum using window aspect
	m_camera.SetFrustum(XMConvertToRadians(90.0f), AspectRatio(), 1.0f, 1000.0f);
	m_baseProjection.view = XMMatrixTranspose(m_camera.GetViewMatrixXM());
	m_baseProjection.proj = XMMatrixTranspose(m_camera.GetProjMatrixXM());
	m_baseProjection.worldInvTranspose = XMMatrixInverse(nullptr, XMMatrixTranspose(m_baseProjection.world));
	// DirectionalLight �ʱⰪ �ʵ� ����
	m_baseProjection.dirLight.ambient = DirectX::XMFLOAT4(0,0,0,1);
	m_baseProjection.dirLight.diffuse = DirectX::XMFLOAT4(1,1,1,1);
	m_baseProjection.dirLight.specular = DirectX::XMFLOAT4(1,1,1,1);
	m_baseProjection.dirLight.direction = DirectX::XMFLOAT3(0,-1,1);
	m_baseProjection.dirLight.pad = 0.0f;
	m_baseProjection.eyePos = m_camera.GetPosition();
	m_baseProjection.pad = 0.0f;

	// ***********************************************************************************************
	// ��ƿ �ʱ�ȭ: ���� ������, ��ī�̹ڽ�, ����� �ڽ�
	if (!m_LineRenderer) m_LineRenderer = new LineRenderer();
	m_LineRenderer->Initialize(m_pDevice);

	// Skybox: ���� Hanako�� �⺻���� �ʱ�ȭ (��ȣ DDS�� ����)
	if (!m_Skybox) m_Skybox = new Skybox();
	// Skybox�� �̹� CreateDDSTextureFromFile�� SRV�� �����Ǿ� �����Ƿ�, ���⼱ ���� ���õ� SRV�� ����ϵ��� Initialize�� ��� ��� ��� ��ŵ�� �� �ֽ��ϴ�.
	// ����ȭ�� ���� cubemap.dds�� �ʱ�ȭ
	m_Skybox->Initialize(m_pDevice, m_CurrentSkyboxPath, m_pSkyBoxVertexShader, m_pSkyBoxPixelShader, m_pSkyBoxInputLayout, m_pConstantBuffer);

	// Debug box buffers for light position marker
	StaticMesh::CreateDebugBoxBuffers(m_pDevice, XMFLOAT4(1,1,1,1), 0.2f, &m_pDebugBoxVB, &m_pDebugBoxIB, &m_DebugBoxIndexCount);

	return true;
}

void App::UninitScene()
{
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	SAFE_RELEASE(m_pConstantBuffer);

	SAFE_RELEASE(m_pSkyBoxInputLayout);
	SAFE_RELEASE(m_pSkyBoxVertexShader);
	SAFE_RELEASE(m_pSkyBoxPixelShader);

	SAFE_RELEASE(m_pSkyHanakoSRV);
	SAFE_RELEASE(m_pSkyCubeMapSRV);

	for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_pCubeTextureSRVs[i]);
    for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_pSkyFaceSRV[i]);

    SAFE_RELEASE(m_pDebugBoxVB);
    SAFE_RELEASE(m_pDebugBoxIB);
    if (m_LineRenderer) { m_LineRenderer->Release(); delete m_LineRenderer; m_LineRenderer = nullptr; }
    if (m_Skybox) { m_Skybox->Release(); delete m_Skybox; m_Skybox = nullptr; }
}

bool App::InitTexture()
{
	PrepareSkyFaceSRVs();

	const wchar_t* facePaths[6] = {
		//L"front.png", L"left.png", L"top.png", L"back.png", L"right.png", L"bottom.png"
		L"../Resource/Hanako.png", L"../Resource/Hanako.png", L"../Resource/Hanako.png", L"../Resource/Hanako.png", L"../Resource/Hanako.png", L"../Resource/Hanako.png"
	};
	for (int i = 0; i < 6; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> res;
		HR_T(CreateWICTextureFromFile(m_pDevice, facePaths[i], res.GetAddressOf(), &m_pCubeTextureSRVs[i]));
	}
	return true;
}

bool App::InitImGui()
{
	// ImGui �ʱ�ȭ
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);
	return true;
}

bool App::InitBasicEffect()
{
	// Vertex Shader -------------------------------------
	D3D11_INPUT_ELEMENT_DESC layout[] = // �Է� ���̾ƿ�.
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	ID3D10Blob* vertexShaderBuffer = nullptr;
    HR_T(CompileShaderFromFile(L"14_BasicVS.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));

	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// ������ ���� ����


	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
    HR_T(CompileShaderFromFile(L"14_BasicPS.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// �ȼ� ���̴� ���� ���̻� �ʿ����
	return true;
}

bool App::InitSkyBoxEffect()
{
	// Vertex Shader -------------------------------------
	D3D11_INPUT_ELEMENT_DESC layout[] = // �Է� ���̾ƿ�.
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	ID3D10Blob* vertexShaderBuffer = nullptr;
    HR_T(CompileShaderFromFile(L"14_SkyBoxVS.hlsl", "VS", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pSkyBoxInputLayout));

	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pSkyBoxVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// ������ ���� ����

	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
    HR_T(CompileShaderFromFile(L"14_SkyBoxPS.hlsl", "PS", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pSkyBoxPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// �ȼ� ���̴� ���� ���̻� �ʿ����
	return true;
}
