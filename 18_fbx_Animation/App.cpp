/*
* @brief : fbx Animation을 재생하는 예제입니다
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
#include <commdlg.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "Comdlg32.lib")

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

// 모델용 파일 선택 대화상자 (fbx/obj/pmx)
static bool OpenFileDialogModel(std::wstring& outPath)
{
    wchar_t file[MAX_PATH] = {0};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GameApp::m_hWnd;
    ofn.lpstrFilter = L"Models (*.fbx;*.obj;*.pmx)\0*.fbx;*.obj;*.pmx\0All Files\0*.*\0\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) { outPath = file; return true; }
    return false;
}


void App::PrepareSkyFaceSRVs()
{
	// 다른 스카이박스로 바꿀 수도 있으니 해제하고 다시 로드
	for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_pSkyFaceSRV[i]);
    m_SkyFaceSize = ImVec2(0, 0);
    if (!m_pTextureSRV) return;

	Microsoft::WRL::ComPtr<ID3D11Resource> res;
    m_pTextureSRV->GetResource(res.GetAddressOf());
    if (!res) return;

	// 파괴됐는지 안됐는지 판단을 위해 Comptr이 필요하다
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
	HR_T(res.As(&tex2D));

    D3D11_TEXTURE2D_DESC desc{};
    tex2D->GetDesc(&desc);
    // 큐브맵은 6개의 array slice를 가짐. (여러 큐브면 6의 배수)
    if ((desc.ArraySize < 6)) return;

    // 크기 기록 (mip0 기준)
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

    // 값 타입 매니저 사용(동적 할당 없음)

	if (!m_SystemInfo.InitSysInfomation(m_pDevice)) return false;

	return true;
}

void App::OnUninitialize()
{
	// ImGui 종료
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// 씬 리소스 먼저 정리 
	UninitScene();
	UninitD3D();
}

void App::OnUpdate(const float& dt)
{
	// 로컬 변환
    if(m_RotateModel)
	{
        // 모델 Yaw(도)를 초당 45도 회전
        m_modelRotation.y += 45.0f * dt;
		// -180~180 래핑
        m_modelRotation.y = std::fmod(m_modelRotation.y + 180.0f, 360.0f) - 180.0f;
	}
    // 모델 고유 회전값 사용 Yaw/Pitch/Roll, deg -> rad
    XMMATRIX rotYaw   = XMMatrixRotationY(XMConvertToRadians(m_modelRotation.y));
    XMMATRIX rotPitch = XMMatrixRotationX(XMConvertToRadians(m_modelRotation.x));
    XMMATRIX rotRoll  = XMMatrixRotationZ(XMConvertToRadians(m_modelRotation.z));
    XMMATRIX S = XMMatrixScaling(m_modelScale.x, m_modelScale.y, m_modelScale.z);
    XMMATRIX local0 = S * rotPitch * rotYaw * rotRoll * XMMatrixTranslation(m_modelPos.x, m_modelPos.y, m_modelPos.z); // 루트
	XMMATRIX world0 = local0; // 루트

	// 카메라 업데이트
	ImGuiIO& io = ImGui::GetIO();
	bool rmbDown = ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.WantCaptureMouse;
	bool keyW = ImGui::IsKeyDown(ImGuiKey_W);
	bool keyS = ImGui::IsKeyDown(ImGuiKey_S);
	bool keyA = ImGui::IsKeyDown(ImGuiKey_A);
	bool keyD = ImGui::IsKeyDown(ImGuiKey_D);
	bool keyE = ImGui::IsKeyDown(ImGuiKey_E);
	bool keyQ = ImGui::IsKeyDown(ImGuiKey_Q);
	m_camera.UpdateFromUI(rmbDown && !io.WantCaptureKeyboard, io.MouseDelta.x, io.MouseDelta.y, keyW, keyS, keyA, keyD, keyE, keyQ, dt);

	// Camera의 View/Proj 
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
		// DirectionalLight 정규화된 방향으로 대입 
		m_baseProjection.dirLight = m_DirLight;
		m_baseProjection.dirLight.direction = dir;
		m_baseProjection.dirLight.pad = 0.0f;
	}
	m_baseProjection.eyePos = m_camera.GetPosition();
	m_baseProjection.pad = 0.0f;

	// 머티리얼을 기본 캐시에 반영해 둔다
	m_baseProjection.material = m_Material;

	m_SystemInfo.Tick(dt);

    // FBX 애니메이션 업데이트 (모델 모드+FBX일 때)
    // 애니메이션 업데이트: 본 팔레트만 갱신(기본 렌더 경로 유지)
    if (m_RenderMode == RenderMode::Model && m_ModelSource == ModelSource::FBX)
        m_FbxManager.UpdateAnimation(m_pDeviceContext, dt);
}

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
{
	return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}
inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) 
{
	return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

// Render() 함수에 중요한 부분이 다 들어있습니다. 여기를 보면 됩니다
void App::OnRender()
{
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	UINT stride = m_VertextBufferStride;	// 바이트 수
	UINT offset = m_VertextBufferOffset;

	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 1 ~ 3 . IA 단계 설정
	// 정점을 어떻게 이어서 그릴 것인지를 선택하는 부분
	// 1. 버퍼를 잡아주기
	// 2. 입력 레이아웃을 잡아주기
	// 3. 인덱스 버퍼를 잡아주기
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // 렌더 모드에 따라 VB/IB 바인딩 결정
    if (m_RenderMode == RenderMode::Model && m_pModelVB && m_pModelIB)
    {
        UINT s = m_ModelStride; UINT o = 0;
        m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pModelVB, &s, &o);
        // FBX 스키닝일 경우 스키닝용 InputLayout/VS로 교체
        if (m_ModelSource == ModelSource::FBX && m_FbxManager.HasSkeleton())
        {
            m_pDeviceContext->IASetInputLayout(m_pInputLayoutSkinned);
            m_pDeviceContext->VSSetShader(m_pVertexShaderSkinned, nullptr, 0);
            // b0 재바인딩: VS 교체 후 안전하게 카메라/월드 상수 재설정
            m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
            // 본 팔레트 상수버퍼 b1 바인딩
            ID3D11Buffer* cbBones = m_FbxManager.GetBoneConstantBuffer();
            if (cbBones) m_pDeviceContext->VSSetConstantBuffers(1, 1, &cbBones);
        }
        else
        {
            m_pDeviceContext->IASetInputLayout(m_pInputLayout);
            // 기본 VS로 복원
            m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
        }
        m_pDeviceContext->IASetIndexBuffer(m_pModelIB, DXGI_FORMAT_R32_UINT, 0);
    }
    else
    {
        // 기본 큐브
        m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
        m_pDeviceContext->IASetInputLayout(m_pInputLayout);
        m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    }

	// 컬러 클리어 및 스카이박스/배경 선택
	if (m_SkyBoxChoice == SkyBoxChoice::Off)
	{
		float clr[4] = { m_ClearColor.x, m_ClearColor.y, m_ClearColor.z, m_ClearColor.w };
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clr);
	}

	// 큐브는 CCW를 앞면으로 정의했으므로 FrontCounterClockwise=false, Back-face culling 사용
	//m_pDeviceContext->RSSetState(RSCullClockWise);
	m_pDeviceContext->RSSetState(RSNoCull);

    if (!(m_RenderMode == RenderMode::Model && m_ModelSource == ModelSource::FBX && m_FbxManager.HasSkeleton()))
        m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	m_ConstantBuffer.world = m_baseProjection.world;
	m_ConstantBuffer.view  = m_baseProjection.view;
	m_ConstantBuffer.proj  = m_baseProjection.proj;
	{
		// worldInvTranspose 계산 (CPU에서는 실제 world로 역/전치 후, HLSL 프리트랜스포즈 규약에 맞게 저장)
		// 비균등 스케일을 해결한 코드
		auto invWorlNormal = XMMatrixInverse(nullptr, m_baseProjection.world);
		m_ConstantBuffer.worldInvTranspose = XMMatrixTranspose(invWorlNormal);
	}
	// 간단 기본 광원/카메라 (UI 반영)
	{
		XMFLOAT3 dir = m_DirLight.direction;
		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&dir));
		XMStoreFloat3(&dir, v);
		// DirectionalLight 필드 대입 (정규화된 방향)
		m_ConstantBuffer.dirLight = m_DirLight;
		m_ConstantBuffer.dirLight.direction = dir;
		m_ConstantBuffer.dirLight.pad = 0.0f;
	}
	m_ConstantBuffer.eyePos = m_camera.GetPosition();
	m_ConstantBuffer.pad = 0.0f;
	// 셰이딩 모드 전달
	m_ConstantBuffer.shadingMode = (int)m_ShadingMode;
	m_ConstantBuffer.pad2 = XMFLOAT3(0,0,0);
	m_ConstantBuffer.enableNormalMap = m_EnableNormalMap;
	m_ConstantBuffer.pad3 = XMFLOAT3(0,0,0);
	m_ConstantBuffer.useSpecularMap = m_UseSpecularMap;
	m_ConstantBuffer.pad4 = XMFLOAT3(0,0,0);
	// 머티리얼 채우기
	m_ConstantBuffer.material = m_Material;

	D3D11_MAPPED_SUBRESOURCE mappedData;
	HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
	memcpy_s(mappedData.pData, sizeof(ConstantBuffer), &m_ConstantBuffer, sizeof(ConstantBuffer));
	m_pDeviceContext->Unmap(m_pConstantBuffer, 0);

	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);
    m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
    // 큐브맵을 t1 슬롯에 바인딩 (픽셀 셰이더에서 g_TexCube : t1)
    if (m_pTextureSRV && m_SkyBoxChoice != SkyBoxChoice::Off)
    {
        ID3D11ShaderResourceView* texCube = m_pTextureSRV;
        m_pDeviceContext->PSSetShaderResources(1, 1, &texCube);
    }
    else
    {
        ID3D11ShaderResourceView* nullSRV = nullptr;
        m_pDeviceContext->PSSetShaderResources(1, 1, &nullSRV);
    }

	// PNG 알파 반영: 블렌딩 ON
	FLOAT blendFactor[4] = { 0,0,0,0 };
	UINT sampleMask = 0xFFFFFFFF;
	m_pDeviceContext->OMSetBlendState(m_pAlphaBlendState, blendFactor, sampleMask);
    if (m_RenderMode == RenderMode::Model && m_pModelVB && m_pModelIB)
    {
        // PMX도 FBX/OBJ와 동일한 TBN 포함 레이아웃을 사용하도록 통일
        ID3D11VertexShader* prevVS = nullptr; 
        ID3D11InputLayout* prevIL = nullptr;
        m_pDeviceContext->VSGetShader(&prevVS, nullptr, nullptr);
        m_pDeviceContext->IAGetInputLayout(&prevIL);
        for (const auto& sub : m_ModelSubsets)
        {
            ID3D11ShaderResourceView* srvDiffuse = nullptr;
            if (sub.materialIndex < m_ModelMaterialSRVs.size()) srvDiffuse = m_ModelMaterialSRVs[sub.materialIndex];
            if (!srvDiffuse) srvDiffuse = m_pFallbackWhite;
            ID3D11ShaderResourceView* srvNormal = (m_EnableNormalMap != 0) ? m_pFallbackNormal : nullptr;
            ID3D11ShaderResourceView* srvSpec   = (m_UseSpecularMap != 0) ? m_pFallbackWhite : nullptr;
            m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
            m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
            m_pDeviceContext->PSSetShaderResources(3, 1, &srvSpec);
            m_pDeviceContext->DrawIndexed(sub.count, sub.start, 0);
        }
        // 복원
        if (prevVS) { m_pDeviceContext->VSSetShader(prevVS, nullptr, 0); prevVS->Release(); }
        if (prevIL) { m_pDeviceContext->IASetInputLayout(prevIL); prevIL->Release(); }
    }
    else if (m_RenderMode == RenderMode::Cube)
    {
        for (int face = 0; face < 6; ++face)
        {
            ID3D11ShaderResourceView* srvDiffuse = m_pCubeTextureSRVs[face];
            ID3D11ShaderResourceView* srvNormal  = m_pNormalSRVs[face] ? m_pNormalSRVs[face] : m_pCubeTextureSRVs[face];
            ID3D11ShaderResourceView* srvSpec    = m_pSpecularSRVs[face] ? m_pSpecularSRVs[face] : m_pCubeTextureSRVs[face];
            m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
            m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
            m_pDeviceContext->PSSetShaderResources(3, 1, &srvSpec);
            m_pDeviceContext->DrawIndexed(6, face * 6, 0);
        }
    }
	// 필요 시: 블렌딩 OFF로 복구
	m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    // Mirror Cube: 모델 모드일 때는 생략하고, 큐브 모드에서만 거울 큐브 표시
    if (m_RenderMode == RenderMode::Cube)
    {
		ConstantBuffer mirrorCB = m_ConstantBuffer;
		// 월드: 헤더 공개된 mirrorCube 트랜스폼 사용(스케일*회전*이동)
		XMMATRIX rotYaw   = XMMatrixRotationY(XMConvertToRadians(m_mirrorCubeRotation.y));
		XMMATRIX rotPitch = XMMatrixRotationX(XMConvertToRadians(m_mirrorCubeRotation.x));
		XMMATRIX rotRoll  = XMMatrixRotationZ(XMConvertToRadians(m_mirrorCubeRotation.z));
		XMMATRIX Sm = XMMatrixScaling(m_MirrorCubeScale, m_MirrorCubeScale, m_MirrorCubeScale);
		XMMATRIX Tm = XMMatrixTranslation(m_mirrorCubePos.x, m_mirrorCubePos.y, m_mirrorCubePos.z);
		Tm = Sm * rotPitch * rotYaw * rotRoll * Tm;
		mirrorCB.world = XMMatrixTranspose(Tm);
		mirrorCB.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(Tm)));
		// 재질: 헤더에 공개한 m_mirrorCubeMaterial 사용
		mirrorCB.material = m_mirrorCubeMaterial;
		// pad=4.0f : PS에서 반사 게이팅 override
		mirrorCB.pad = 4.0f;
		mirrorCB.shadingMode = (int)m_ShadingMode;
		mirrorCB.pad2 = XMFLOAT3(0,0,0);

		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &mirrorCB, sizeof(ConstantBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		// 블렌딩 ON으로 반사에도 부드러운 에지 허용
		FLOAT blendFactor2[4] = {0,0,0,0};
		m_pDeviceContext->OMSetBlendState(m_pAlphaBlendState, blendFactor2, 0xFFFFFFFF);

		// t0에 임의의 불투명 텍스처를 바인딩(컷아웃 통과용). 여기서는 face0 재사용
		ID3D11ShaderResourceView* srvFace0 = m_pCubeTextureSRVs[0];
		m_pDeviceContext->PSSetShaderResources(0, 1, &srvFace0);
		// 드로우: 동일 인덱스 범위를 6면 반복
        for (int face = 0; face < 6; ++face) m_pDeviceContext->DrawIndexed(6, face * 6, 0);
		m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		// pad 복원
		m_ConstantBuffer.pad = 0.0f;
	}

    // 라이트 위치 마커 큐브 그리기 (작은 스케일, 흰색) - 항상
    {
		ConstantBuffer marker = m_ConstantBuffer;
		XMMATRIX S = XMMatrixScaling(1.0f, 1.0f, 1.0f);
		XMMATRIX T = XMMatrixTranslation(m_LightPosition.x, m_LightPosition.y, m_LightPosition.z);
		marker.world = XMMatrixTranspose(S * T);
		marker.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(S * T)));
		marker.pad = 2.0f; // PS에서 흰색 출력 토글
		marker.shadingMode = (int)m_ShadingMode;
		marker.pad2 = XMFLOAT3(0,0,0);

		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &marker, sizeof(ConstantBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		UINT dbgStride = sizeof(VertexLightTex);
		UINT dbgOffset = 0;
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pDebugBoxVB, &dbgStride, &dbgOffset);
		m_pDeviceContext->IASetInputLayout(m_pInputLayout);
		m_pDeviceContext->IASetIndexBuffer(m_pDebugBoxIB, DXGI_FORMAT_R32_UINT, 0);
		m_pDeviceContext->DrawIndexed(m_DebugBoxIndexCount, 0, 0);
	}

	// 라이트 방향 표시 라인 그리기(빨간색)
	{
		// pad=3.0은 라인 디버그에 이용.
		ConstantBuffer lineCB = m_ConstantBuffer;
		lineCB.world = XMMatrixTranspose(XMMatrixIdentity());
		lineCB.view  = m_baseProjection.view;
		lineCB.proj  = m_baseProjection.proj;
		lineCB.worldInvTranspose = XMMatrixIdentity();
		lineCB.pad = 3.0f;
		lineCB.shadingMode = (int)m_ShadingMode;
		lineCB.pad2 = XMFLOAT3(0,0,0);
		D3D11_MAPPED_SUBRESOURCE mappedLine;
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedLine));
		memcpy_s(mappedLine.pData, sizeof(ConstantBuffer), &lineCB, sizeof(ConstantBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		// 라인 전용 VS/InputLayout로 컬러 보존
		ID3D11VertexShader* prevVS = m_pVertexShader;
		ID3D11InputLayout* prevIL = m_pInputLayout;
		m_pDeviceContext->VSSetShader(m_pLineVS, nullptr, 0);
		m_pDeviceContext->IASetInputLayout(m_pLineInputLayout);

		// light direction (red)
		m_LineRenderer->DrawLightDirection(m_pDeviceContext, m_LightPosition, m_DirLight.direction, 2.0f, m_pLineInputLayout, m_pLineVS, m_pPixelShader, m_pConstantBuffer);
		// symmetric axes centered at origin for better grid feel
		m_LineRenderer->DrawAxesSymmetric(m_pDeviceContext, 100.0f, m_pLineInputLayout, m_pLineVS, m_pPixelShader, m_pConstantBuffer);

		// 복원
		m_pDeviceContext->VSSetShader(prevVS, nullptr, 0);
		m_pDeviceContext->IASetInputLayout(prevIL);
	}


    // SkyBox 렌더링 (상태 보존/복구)
	if (m_SkyBoxChoice != SkyBoxChoice::Off)
	{
		UINT stride = m_VertextBufferStride;
		UINT offset = m_VertextBufferOffset;
		m_Skybox->Render(m_pDeviceContext, m_pVertexBuffer, m_pIndexBuffer, m_nIndices, stride, offset, m_baseProjection.view, m_baseProjection.proj);
	}

	// 화면 오버레이 축(NDC)에 작게 표시
	{
		// pad=3 설정 정점색으로 출력되도록
		ConstantBuffer overlayCB = m_ConstantBuffer;
		overlayCB.world = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.view = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.proj = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.worldInvTranspose = XMMatrixIdentity();
		overlayCB.pad = 3.0f;
		overlayCB.shadingMode = (int)m_ShadingMode;
		overlayCB.pad2 = XMFLOAT3(0,0,0);
		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &overlayCB, sizeof(ConstantBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		// 좌상단 작은 축 표시
		m_LineRenderer->DrawAxesOverlay(m_pDeviceContext, XMMatrixTranspose(m_baseProjection.view), DirectX::XMFLOAT2(-0.9f, 0.85f), 0.08f, m_pLineInputLayout, m_pLineVS, m_pPixelShader, m_pConstantBuffer);
	}

	// ImGui 프레임 및 UI 렌더링
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (ImGui::Begin("Controls"))
	{
		// SkyBox 선택
		{
			int cur = (m_SkyBoxChoice == SkyBoxChoice::Off) ? 0 : (m_SkyBoxChoice == SkyBoxChoice::Hanako ? 1 : 2);
			const char* items[] = { "Off", "Hanako.dds", "cubemap.dds" };
			if (ImGui::Combo("SkyBox Choice", &cur, items, IM_ARRAYSIZE(items)))
			{
				m_SkyBoxChoice = (cur == 0) ? SkyBoxChoice::Off : (cur == 1 ? SkyBoxChoice::Hanako : SkyBoxChoice::CubeMap);
				if (m_SkyBoxChoice == SkyBoxChoice::Off)
				{
					// Off: 배경 단색 사용
				}
				else
				{
					const wchar_t* path = (m_SkyBoxChoice == SkyBoxChoice::Hanako) ? L"..\\Resource\\Skybox\\Hanako.dds" : L"..\\Resource\\Skybox\\cubemap.dds";
					ChangeSkyboxDDS(path);
				}
			}
			if (m_SkyBoxChoice == SkyBoxChoice::Off)
			{
				ImGui::ColorEdit4("Background Color", &m_ClearColor.x);
			}
		}
    ImGui::Text("Model Transforms");
    ImGui::Checkbox("Rotate Model", &m_RotateModel);
    ImGui::DragFloat3("Model Scale", &m_modelScale.x, 0.1f, 20.0f);
    ImGui::DragFloat3("Model Pos (x,y,z)", &m_modelPos.x, 0.1f);
    // 모델 회전(도) 편집
    ImGui::DragFloat3("Model Rotation (deg)", &m_modelRotation.x, 1.0f, -360.0f, 360.0f, "%.1f");
		ImGui::Separator();
		ImGui::Text("Camera");
		{
			if (ImGui::Button("Reset"))
			{
				m_camera.Reset();
			}
			ImGui::SliderFloat("Camera Speed", &m_camera.m_MoveSpeed, 10.0f, 500.0f, "%.1f");
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
        ImGui::Text("Shading");
        {
            int mode = (int)m_ShadingMode;
            const char* modes[] = { "Phong", "Blinn-Phong", "Lambert", "Unlit", "TextureOnly" };
            if (ImGui::Combo("Shading Mode", &mode, modes, IM_ARRAYSIZE(modes)))
            {
                m_ShadingMode = (ShadingMode)mode;
            }
+			ImGui::Checkbox("Enable Normal Map", (bool*)&m_EnableNormalMap);
+			ImGui::Checkbox("Use Specular Map", (bool*)&m_UseSpecularMap);
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

        ImGui::Separator();
        ImGui::Text("Model Loader (FBX / OBJ / PMX)");
        // 렌더 모드 선택
        {
            int curMode = (m_RenderMode == RenderMode::None) ? 0 : (m_RenderMode == RenderMode::Cube ? 1 : 2);
            const char* items[] = { "None", "Cube", "Model" };
            if (ImGui::Combo("Render Mode", &curMode, items, IM_ARRAYSIZE(items)))
            {
                m_RenderMode = (curMode == 0) ? RenderMode::None : (curMode == 1 ? RenderMode::Cube : RenderMode::Model);
            }
        }
        if (ImGui::Button("Browse Model..."))
        {
            std::wstring pathW;
            if (OpenFileDialogModel(pathW))
            {
                if (LoadModelFromFile(pathW)) { m_RenderMode = RenderMode::Model; }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Unload Model"))
        {
            UnloadModel();
            m_RenderMode = RenderMode::None; // 요구사항: 시작/언로드시 아무것도 렌더 X
        }

        // FBX 본 트리와 애니메이션 UI
        if (m_ModelSource == ModelSource::FBX && m_RenderMode == RenderMode::Model)
        {
            if (ImGui::CollapsingHeader("FBX Skeleton & Animations", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // 애니메이션 리스트
                if (m_FbxManager.HasAnimations())
                {
                    const auto& names = m_FbxManager.GetAnimationNames();
                    if (m_UISelectedAnim < 0 || m_UISelectedAnim >= (int)names.size()) m_UISelectedAnim = m_FbxManager.GetCurrentAnimationIndex();
                    ImGui::Text("Animations");
                    if (ImGui::BeginListBox("##AnimList", ImVec2(-FLT_MIN, 6 * ImGui::GetTextLineHeightWithSpacing())))
                    {
                        for (int i = 0; i < (int)names.size(); ++i)
                        {
                            bool sel = (i == m_UISelectedAnim);
                            if (ImGui::Selectable(names[i].c_str(), sel))
                            {
                                m_UISelectedAnim = i;
                                m_FbxManager.SetCurrentAnimation(i);
                            }
                            if (sel) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndListBox();
                    }
                    ImGui::Checkbox("Play", &m_UIAnimPlaying);
                    m_FbxManager.SetAnimationPlaying(m_UIAnimPlaying);
                    double cur = m_FbxManager.GetAnimationTimeSeconds();
                    double dur = m_FbxManager.GetClipDurationSec(m_FbxManager.GetCurrentAnimationIndex());
                    float curF = (float)cur, durF = (float)dur;
                    if (durF > 0.0f)
                    {
                        if (ImGui::SliderFloat("Time (s)", &curF, 0.0f, durF))
                        {
                            m_FbxManager.SetAnimationTimeSeconds((double)curF);
                        }
                    }
                }
                else
                {
                    ImGui::TextDisabled("No animations found in FBX.");
                }

                // 본 트리
                if (m_FbxManager.HasSkeleton())
                {
                    const auto& skel = m_FbxManager.GetSkeleton();
                    int root = m_FbxManager.GetSkeletonRoot();
                    std::function<void(int)> drawNode = [&](int idx)
                    {
                        if (idx < 0 || idx >= (int)skel.size()) return;
                        const auto& n = skel[idx];
                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (n.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
                        if (n.isBone) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200,255,200,255));
                        bool open = ImGui::TreeNodeEx(n.name.c_str(), flags);
                        if (n.isBone) ImGui::PopStyleColor();
                        if (open)
                        {
                            for (int c : n.children) drawNode(c);
                            ImGui::TreePop();
                        }
                    };
                    ImGui::Text("Skeleton");
                    ImGui::BeginChild("##SkelTree", ImVec2(0, 200), true);
                    if (root >= 0) drawNode(root);
                    ImGui::EndChild();
                }
                else
                {
                    ImGui::TextDisabled("No skeleton (bones) found.");
                }
            }
        }
	}
	ImGui::End();

		// 현재 카메라 포워드 기준 스카이박스 면 이미지를 표시 (스카이박스 On일 때만)
	if (m_SkyBoxChoice != SkyBoxChoice::Off)
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
				float scale = (sx > 0.f && sy > 0.f) ? std::min(sx, sy) : 1.f;
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

	// 스왑체인의 값들을 설정할 구조체를 만듭니다
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
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

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

	// 깊이 스텐실 텍스처/뷰 생성
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

	// DepthStencilState 생성 및 설정
	D3D11_DEPTH_STENCIL_DESC dssDesc = {};
	dssDesc.DepthEnable = TRUE;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS;
	dssDesc.StencilEnable = FALSE;
	HR_T(m_pDevice->CreateDepthStencilState(&dssDesc, &m_pDepthStencilState));
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStencilState, 0);

	// 렌더 타겟/DSV 바인딩
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

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

	// 컬링 설정 (양면 렌더링/후면 컬링)
	CD3D11_RASTERIZER_DESC rasterizerDesc(CD3D11_DEFAULT{});
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.FrontCounterClockwise = false;
	HR_T(m_pDevice->CreateRasterizerState(&rasterizerDesc, &RSNoCull));
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FrontCounterClockwise = true;
	HR_T(m_pDevice->CreateRasterizerState(&rasterizerDesc, &RSCullClockWise));

	// 알파 블렌딩 상태 생성 (SrcAlpha/InvSrcAlpha)
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
	ID3D10Blob* errorMessage = nullptr;	 // 에러 메시지를 저장할 버퍼.

	// ***********************************************************************************************
	// 큐브설정
	// 24개 정점 (각 면 4개) + 텍스처 좌표
	m_VertextBufferStride = sizeof(VertexLightTex);
	m_VertextBufferOffset = 0;
	// ***********************************************************************************************
    // 작은 큐브 데이터 설정 (TBN 정점)
    StaticMeshData cube = StaticMesh::CreateBox(XMFLOAT4(1,1,1,1));
    m_VertextBufferStride = sizeof(VertexTBN);
    StaticMesh::AssignMemory(m_pDevice, m_pVertexBuffer, cube);
    StaticMesh::AssignIndexMemory(m_pDevice, m_pIndexBuffer, cube, m_nIndices);

	// ***********************************************************************************************
	// 상수 버퍼 설정
	//
	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.ByteWidth = sizeof(ConstantBuffer);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	// 단일 상수 버퍼 생성 (VS/PS 공용, b0)
	HR_T(m_pDevice->CreateBuffer(&cbd, nullptr, &m_pConstantBuffer));

		// ***********************************************************************************************
	// 스카이 박스 큐브 설정
    HR_T(CreateDDSTextureFromFile(m_pDevice, L"..\\Resource\\Skybox\\Hanako.dds", nullptr, &m_pSkyHanakoSRV));
    HR_T(CreateDDSTextureFromFile(m_pDevice, L"..\\Resource\\Skybox\\cubemap.dds", nullptr, &m_pSkyCubeMapSRV));
	m_pTextureSRV = m_pSkyCubeMapSRV;
	
	// 샘플러 생성
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HR_T(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState));

	// ***********************************************************************************************
	// 카메라 설정
	// 공통 카메라(View/Proj)로 3개의 상수 버퍼 엔트리를 준비합니다
	m_baseProjection.world = XMMatrixIdentity();
	// Set camera initial frustum using window aspect
	m_camera.SetFrustum(XMConvertToRadians(90.0f), AspectRatio(), 1.0f, 1000.0f);
	m_baseProjection.view = XMMatrixTranspose(m_camera.GetViewMatrixXM());
	m_baseProjection.proj = XMMatrixTranspose(m_camera.GetProjMatrixXM());
	m_baseProjection.worldInvTranspose = XMMatrixInverse(nullptr, XMMatrixTranspose(m_baseProjection.world));
	// DirectionalLight 초기값 필드 대입
	m_baseProjection.dirLight.ambient = DirectX::XMFLOAT4(0,0,0,1);
	m_baseProjection.dirLight.diffuse = DirectX::XMFLOAT4(1,1,1,1);
	m_baseProjection.dirLight.specular = DirectX::XMFLOAT4(1,1,1,1);
	m_baseProjection.dirLight.direction = DirectX::XMFLOAT3(0,-1,1);
	m_baseProjection.dirLight.pad = 0.0f;
	m_baseProjection.eyePos = m_camera.GetPosition();
	m_baseProjection.pad = 0.0f;

	// ***********************************************************************************************
	// 유틸 초기화: 라인 렌더러, 스카이박스, 디버그 박스
	if (!m_LineRenderer) m_LineRenderer = new LineRenderer();
	m_LineRenderer->Initialize(m_pDevice);

    // Skybox: 기존 Hanako를 기본으로 초기화 (선호 DDS를 설정)
	if (!m_Skybox) m_Skybox = new Skybox();
	// Skybox는 이미 CreateDDSTextureFromFile로 SRV가 생성되어 있으므로, 여기선 현재 선택된 SRV를 사용하도록 Initialize는 경로 기반 대신 스킵할 수 있습니다.
	// 간편화를 위해 cubemap.dds로 초기화
	m_Skybox->Initialize(m_pDevice, m_CurrentSkyboxPath, m_pSkyBoxVertexShader, m_pSkyBoxPixelShader, m_pSkyBoxInputLayout, m_pConstantBuffer);

	// Debug box buffers for light position marker
	StaticMesh::CreateDebugBoxBuffersLightTex(m_pDevice, XMFLOAT4(1,1,1,1), 0.2f, &m_pDebugBoxVB, &m_pDebugBoxIB, &m_DebugBoxIndexCount);

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
	SAFE_RELEASE(m_pSamplerState);

	SAFE_RELEASE(m_pSkyBoxInputLayout);
	SAFE_RELEASE(m_pSkyBoxVertexShader);
	SAFE_RELEASE(m_pSkyBoxPixelShader);

	SAFE_RELEASE(m_pSkyHanakoSRV);
	SAFE_RELEASE(m_pSkyCubeMapSRV);

	for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_pCubeTextureSRVs[i]);
    for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_pSkyFaceSRV[i]);

	SAFE_RELEASE(m_pFallbackWhite);
	SAFE_RELEASE(m_pFallbackNormal);
	SAFE_RELEASE(m_pFallbackBlack);

    SAFE_RELEASE(m_pDebugBoxVB);
    SAFE_RELEASE(m_pDebugBoxIB);
    if (m_LineRenderer) { m_LineRenderer->Release(); delete m_LineRenderer; m_LineRenderer = nullptr; }
    if (m_Skybox) { m_Skybox->Release(); delete m_Skybox; m_Skybox = nullptr; }

    // 모델 리소스 해제
    UnloadModel();

    // 값 타입 매니저 정리
    m_FbxManager.Release();
    m_ObjManager.Release();
    m_PmxManager.Release();
}

bool App::InitTexture()
{
	PrepareSkyFaceSRVs();

	/*const wchar_t* facePaths[6] = {
		L"..\\Resource\\Image\\Hanako.png", L"..\\Resource\\Image\\Hanako.png", L"..\\Resource\\Image\\Hanako.png",
		L"..\\Resource\\Image\\Hanako.png", L"..\\Resource\\Image\\Hanako.png", L"..\\Resource\\Image\\Hanako.png"
	};
	const wchar_t* normalPaths[6] = {
		L"..\\Resource\\Image\\Hanako_Normal.png", L"..\\Resource\\Image\\Hanako_Normal.png", L"..\\Resource\\Image\\Hanako_Normal.png",
		L"..\\Resource\\Image\\Hanako_Normal.png", L"..\\Resource\\Image\\Hanako_Normal.png", L"..\\Resource\\Image\\Hanako_Normal.png"
	};
	const wchar_t* specularPaths[6] = {
		L"..\\Resource\\Image\\Hanako_Specular.png", L"..\\Resource\\Image\\Hanako_Specular.png", L"..\\Resource\\Image\\Hanako_Specular.png",
		L"..\\Resource\\Image\\Hanako_Specular.png", L"..\\Resource\\Image\\Hanako_Specular.png", L"..\\Resource\\Image\\Hanako_Specular.png"
	};*/

	const wchar_t* facePaths[6] = {
	L"..\\Resource\\Image\\Bricks059_1K-JPG_Color.jpg",
	L"..\\Resource\\Image\\Bricks059_1K-JPG_Color.jpg",
	L"..\\Resource\\Image\\Bricks059_1K-JPG_Color.jpg",
	L"..\\Resource\\Image\\Bricks059_1K-JPG_Color.jpg",
	L"..\\Resource\\Image\\Bricks059_1K-JPG_Color.jpg",
	L"..\\Resource\\Image\\Bricks059_1K-JPG_Color.jpg"
	};

	const wchar_t* normalPaths[6] = {
		L"..\\Resource\\Image\\Bricks059_1K-JPG_NormalDX.jpg",
		L"..\\Resource\\Image\\Bricks059_1K-JPG_NormalDX.jpg",
		L"..\\Resource\\Image\\Bricks059_1K-JPG_NormalDX.jpg",
		L"..\\Resource\\Image\\Bricks059_1K-JPG_NormalDX.jpg",
		L"..\\Resource\\Image\\Bricks059_1K-JPG_NormalDX.jpg",
		L"..\\Resource\\Image\\Bricks059_1K-JPG_NormalDX.jpg"
	};

	const wchar_t* specularPaths[6] = {
		L"..\\Resource\\Image\\Bricks059_Specular.png",
		L"..\\Resource\\Image\\Bricks059_Specular.png",
		L"..\\Resource\\Image\\Bricks059_Specular.png",
		L"..\\Resource\\Image\\Bricks059_Specular.png",
		L"..\\Resource\\Image\\Bricks059_Specular.png",
		L"..\\Resource\\Image\\Bricks059_Specular.png"
	};

	for (int i = 0; i < 6; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> res;
		HR_T(CreateWICTextureFromFile(m_pDevice, facePaths[i], res.GetAddressOf(), &m_pCubeTextureSRVs[i]));
		res.Reset();
		CreateWICTextureFromFile(m_pDevice, normalPaths[i], res.GetAddressOf(), &m_pNormalSRVs[i]);
		res.Reset();
		CreateWICTextureFromFile(m_pDevice, specularPaths[i], res.GetAddressOf(), &m_pSpecularSRVs[i]);
	}
	return true;
}

bool App::InitImGui()
{
	// ImGui 초기화
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
	D3D11_INPUT_ELEMENT_DESC layout[] = // 입력 레이아웃.
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"18_BasicVS.hlsl", "main", "vs_5_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));

	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// 컴파일 버퍼 해제

	// PMX 전용: NoTBN 입력용 VS/IL 생성
	D3D11_INPUT_ELEMENT_DESC layoutNoTBN[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	ID3D10Blob* vsNoTBN = nullptr;
	HR_T(CompileShaderFromFile(L"18_BasicVS.hlsl", "VSNoTBN", "vs_5_0", &vsNoTBN));
	HR_T(m_pDevice->CreateInputLayout(layoutNoTBN, ARRAYSIZE(layoutNoTBN), vsNoTBN->GetBufferPointer(), vsNoTBN->GetBufferSize(), &m_pInputLayoutNoTBN));
	HR_T(m_pDevice->CreateVertexShader(vsNoTBN->GetBufferPointer(), vsNoTBN->GetBufferSize(), nullptr, &m_pVertexShaderNoTBN));
	SAFE_RELEASE(vsNoTBN);

	// FBX GPU 스키닝용 VS/IL 생성 (POSITION,NORMAL,TANGENT,BINORMAL,COLOR,TEXCOORD,BLENDINDICES,BLENDWEIGHT)
	{
		D3D11_INPUT_ELEMENT_DESC layoutSkinned[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		ID3D10Blob* vsSkinned = nullptr;
		HR_T(CompileShaderFromFile(L"18_BasicVS.hlsl", "VSSkinned", "vs_5_0", &vsSkinned));
		HR_T(m_pDevice->CreateInputLayout(layoutSkinned, ARRAYSIZE(layoutSkinned), vsSkinned->GetBufferPointer(), vsSkinned->GetBufferSize(), &m_pInputLayoutSkinned));
		HR_T(m_pDevice->CreateVertexShader(vsSkinned->GetBufferPointer(), vsSkinned->GetBufferSize(), nullptr, &m_pVertexShaderSkinned));
		SAFE_RELEASE(vsSkinned);
	}

	// Line VS -------------------------------------------
	D3D11_INPUT_ELEMENT_DESC lineLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	ID3D10Blob* vsLine = nullptr;
	HR_T(CompileShaderFromFile(L"18_BasicVS.hlsl", "VSLine", "vs_5_0", &vsLine));
	HR_T(m_pDevice->CreateInputLayout(lineLayout, ARRAYSIZE(lineLayout), vsLine->GetBufferPointer(), vsLine->GetBufferSize(), &m_pLineInputLayout));
	HR_T(m_pDevice->CreateVertexShader(vsLine->GetBufferPointer(), vsLine->GetBufferSize(), nullptr, &m_pLineVS));
	SAFE_RELEASE(vsLine);


	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
    HR_T(CompileShaderFromFile(L"18_BasicPS.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음
	return true;
}

bool App::InitSkyBoxEffect()
{
	// Vertex Shader -------------------------------------
	D3D11_INPUT_ELEMENT_DESC layout[] = // 입력 레이아웃.
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	ID3D10Blob* vertexShaderBuffer = nullptr;
    HR_T(CompileShaderFromFile(L"18_SkyBoxVS.hlsl", "VS", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pSkyBoxInputLayout));

	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pSkyBoxVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// 컴파일 버퍼 해제

	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
    HR_T(CompileShaderFromFile(L"18_SkyBoxPS.hlsl", "PS", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pSkyBoxPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음
	return true;
}

// ------------------------- Model Loader (FBX/OBJ/PMX via Assimp) -------------------------
bool App::LoadModelFromFile(const std::wstring& pathW)
{
    // 기존 리소스 정리
    UnloadModel();

    // 폴백 텍스처 생성(최초 1회)
    if (!m_pFallbackWhite)
    {
        UINT white = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC td{}; td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = &white; sd.SysMemPitch = sizeof(UINT);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex; HR_T(m_pDevice->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MipLevels = 1; srvd.Texture2D.MostDetailedMip = 0;
        HR_T(m_pDevice->CreateShaderResourceView(tex.Get(), &srvd, &m_pFallbackWhite));
    }
    if (!m_pFallbackBlack)
    {
        UINT black = 0x000000FF; // a=1
        D3D11_TEXTURE2D_DESC td{}; td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = &black; sd.SysMemPitch = sizeof(UINT);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex; HR_T(m_pDevice->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MipLevels = 1; srvd.Texture2D.MostDetailedMip = 0;
        HR_T(m_pDevice->CreateShaderResourceView(tex.Get(), &srvd, &m_pFallbackBlack));
    }
    if (!m_pFallbackNormal)
    {
        UINT neutral = 0x8080FFFF; // (0.5,0.5,1,1) in RGBA8
        D3D11_TEXTURE2D_DESC td{}; td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = &neutral; sd.SysMemPitch = sizeof(UINT);
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex; HR_T(m_pDevice->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MipLevels = 1; srvd.Texture2D.MostDetailedMip = 0;
        HR_T(m_pDevice->CreateShaderResourceView(tex.Get(), &srvd, &m_pFallbackNormal));
    }

    // 확장자 분기: 매니저 사용
    std::wstring ext;
    if (!pathW.empty())
    {
        size_t dot = pathW.find_last_of(L'.');
        if (dot != std::wstring::npos) { ext = pathW.substr(dot); std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower); }
    }

    bool ok = false;
    if (ext == L".fbx")
    {
        ok = m_FbxManager.Load(m_pDevice, pathW);
        if (ok)
        {
            m_ModelSource = ModelSource::FBX;
            m_ModelStride = m_FbxManager.GetVertexStride();
            m_pModelVB = m_FbxManager.GetVertexBuffer(); if (m_pModelVB) m_pModelVB->AddRef();
            m_pModelIB = m_FbxManager.GetIndexBuffer();  if (m_pModelIB) m_pModelIB->AddRef();
            m_ModelIndexCount = m_FbxManager.GetIndexCount();
            m_ModelSubsets.clear();
            for (auto& s : m_FbxManager.GetSubsets()) m_ModelSubsets.push_back({ s.startIndex, s.indexCount, s.materialIndex });
            m_ModelMaterialSRVs = m_FbxManager.GetMaterialSRVs();
        }
    }
    else if (ext == L".obj")
    {
        ok = m_ObjManager.Load(m_pDevice, pathW);
        if (ok)
        {
            m_ModelSource = ModelSource::OBJ;
            m_ModelStride = m_ObjManager.GetVertexStride();
            m_pModelVB = m_ObjManager.GetVertexBuffer(); if (m_pModelVB) m_pModelVB->AddRef();
            m_pModelIB = m_ObjManager.GetIndexBuffer();  if (m_pModelIB) m_pModelIB->AddRef();
            m_ModelIndexCount = m_ObjManager.GetIndexCount();
            m_ModelSubsets.clear();
            for (auto& s : m_ObjManager.GetSubsets()) m_ModelSubsets.push_back({ s.startIndex, s.indexCount, s.materialIndex });
            m_ModelMaterialSRVs = m_ObjManager.GetMaterialSRVs();
        }
    }
    else if (ext == L".pmx")
    {
        ok = m_PmxManager.Load(m_pDevice, pathW);
        if (ok)
        {
            m_ModelSource = ModelSource::PMX;
            m_ModelStride = m_PmxManager.GetVertexStride();
            m_pModelVB = m_PmxManager.GetVertexBuffer(); if (m_pModelVB) m_pModelVB->AddRef();
            m_pModelIB = m_PmxManager.GetIndexBuffer();  if (m_pModelIB) m_pModelIB->AddRef();
            m_ModelIndexCount = m_PmxManager.GetIndexCount();
            m_ModelSubsets.clear();
            for (auto& s : m_PmxManager.GetSubsets()) m_ModelSubsets.push_back({ s.startIndex, s.indexCount, s.materialIndex });
            m_ModelMaterialSRVs = m_PmxManager.GetMaterialSRVs();
        }
    }

    if (ok)
    {
        m_RenderMode = RenderMode::Model;
        // 모델 로드 완료 후 트랜스폼 초기화: pos(0,0,0), scale(1,1,1), rot(0,0,0)
        m_modelPos = { 0.0f, 0.0f, 0.0f };
        m_modelScale = { 1.0f, 1.0f, 1.0f };
        m_modelRotation = { 0.0f, 0.0f, 0.0f };
    }
    return ok;
}

void App::UnloadModel()
{
    SAFE_RELEASE(m_pModelVB);
    SAFE_RELEASE(m_pModelIB);
    m_ModelMaterialSRVs.clear();
    m_ModelSubsets.clear();
    m_ModelIndexCount = 0;
}
