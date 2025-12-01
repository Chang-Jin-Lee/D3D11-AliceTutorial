/*
* @brief : FBX / OBJ / PMX 모델과 VMD 애니메이션을 로딩해 실시간으로 렌더링하는 데모입니다.
* @details :
*		- 여러 개의 3D 모델을 동시에 불러와 위치/회전/스케일을 개별 조정할 수 있습니다
*		- FBX/PMX 스켈레톤을 GPU 스키닝으로 재생하며, PMX에는 VMD 모션 파일을 적용할 수 있습니다
*		- 큐브(노말/스페큘러 맵), 스카이박스, 조명·머티리얼, 카메라를 ImGui UI로 실시간 조작할 수 있습니다
*/

#include "App.h"
#include "../Common/Helper.h"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxtk/WICTextureLoader.h>
#include <directxtk/DDSTextureLoader.h>
#include <directxtk/SimpleMath.h>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <fstream>
#include "../Common/StaticMesh.h"
#include "../Common/LineRenderer.h"
#include "../Common/Skybox.h"
#include "../Common/SystemInfomation.h"
#include "../Common/FbxManager.h"
#include "../Common/ObjManager.h"
#include "../Common/PmxManager.h"
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <imgui_stdlib.h>
#include <commdlg.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../22_VMD/Vmd.h"

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "Comdlg32.lib")

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 내부 전용 타입들
struct DirectionalLight { XMFLOAT4 ambient; XMFLOAT4 diffuse; XMFLOAT4 specular; XMFLOAT3 direction; float pad; };
struct Material { XMFLOAT4 ambient; XMFLOAT4 diffuse; XMFLOAT4 specular; XMFLOAT4 reflect; };
struct ConstantBuffer {
    XMMATRIX world; XMMATRIX view; XMMATRIX proj; XMMATRIX worldInvTranspose;
    Material material; DirectionalLight dirLight; XMFLOAT3 eyePos; float pad;
    int shadingMode = 0; XMFLOAT3 pad2 = {0,0,0}; int enableNormalMap = 1; XMFLOAT3 pad3 = {0,0,0}; int useSpecularMap = 0; XMFLOAT3 pad4 = {0,0,0};
};
enum class ShadingMode { Phong=0, BlinnPhong=1, Lambert=2, Unlit=3, TextureOnly=4 };
enum class RenderMode { None = 0, Cube = 1, Model = 2 };
enum class ModelSource { FBX, OBJ, PMX, Custom };
struct ModelSubset { uint32_t start; uint32_t count; uint32_t materialIndex; };

// 여러 모델을 그리기 위한 구조체
struct ModelEntry
{
	std::wstring modelName{ L"" };
	ModelSource source = ModelSource::Custom;
	// 로더 매니저 (FBX/OBJ/PMX 중 선택해 사용)
	FbxManager fbx;
	ObjManager obj;
	PmxManager pmx;

	// GPU에서 사용될 것들 
	ID3D11Buffer* vb = nullptr;
	ID3D11Buffer* ib = nullptr;
	int           indexCount = 0;
	UINT          stride = 0;
	std::vector<ModelSubset> subsets;
	std::vector<ID3D11ShaderResourceView*> materialSRVs; // AddRef됨

	// 트랜스폼
	XMFLOAT3 pos = {0,0,0};
	XMFLOAT3 scale = {1,1,1};
	XMFLOAT3 rotDeg = {0,0,0}; // yaw=pitch=roll(deg)
	bool     autoRotate = false;

	// FBX 전용 애니메이션 UI 상태
	// ImGui에서 보여주기 위함
	int  uiSelectedAnim = -1;
	bool uiAnimPlaying = false;
};

// pImpl 정의
struct App::Impl {
    // D3D 핵심 객체
    ID3D11Device*                 m_pDevice = nullptr;
    ID3D11DeviceContext*          m_pDeviceContext = nullptr;
    IDXGISwapChain*               m_pSwapChain = nullptr;
    ID3D11RenderTargetView*       m_pRenderTargetView = nullptr;

    // 파이프라인 셰이더/입력 레이아웃. 기본/PMX/스카이박스/라인
    ID3D11VertexShader*           m_pVertexShader = nullptr;
    ID3D11PixelShader*            m_pPixelShader = nullptr;
    ID3D11PixelShader*            m_pPixelShaderSolid = nullptr;     // 마커용 흰색 출력
    ID3D11VertexShader*           m_pVertexShaderNoTBN = nullptr;    // PMX 전용 VS
    ID3D11InputLayout*            m_pInputLayoutNoTBN = nullptr;     // PMX 전용 IL
    // FBX GPU 스키닝용 VS/IL
    ID3D11VertexShader*           m_pVertexShaderSkinned = nullptr;
    ID3D11InputLayout*            m_pInputLayoutSkinned = nullptr;
    ID3D11VertexShader*           m_pSkyBoxVertexShader = nullptr;
    ID3D11PixelShader*            m_pSkyBoxPixelShader = nullptr;
    ID3D11InputLayout*            m_pSkyBoxInputLayout = nullptr;
    ID3D11VertexShader*           m_pLineVS = nullptr;
    ID3D11InputLayout*            m_pLineInputLayout = nullptr;

    // 샘플러/블렌드 상태
    ID3D11SamplerState*           m_pSamplerState = nullptr;
    ID3D11BlendState*             m_pAlphaBlendState = nullptr;

    // Skybox/큐브맵 자원 및 옵션
    enum class SkyBoxChoice { Off = 0, Hanako = 1, CubeMap = 2 };
    SkyBoxChoice                  m_SkyBoxChoice = SkyBoxChoice::Off;
    ID3D11ShaderResourceView*     m_pSkyHanakoSRV = nullptr;
    ID3D11ShaderResourceView*     m_pSkyCubeMapSRV = nullptr;
    ID3D11ShaderResourceView*     m_pTextureSRV = nullptr;           // 현재 스카이박스 SRV
    ID3D11ShaderResourceView*     m_pSkyFaceSRV[6] = {};
    ImVec2                        m_SkyFaceSize = ImVec2(0, 0);
    wchar_t                       m_CurrentSkyboxPath[260] = L"..\\Resource\\Skybox\\cubemap.dds";

    // 기본 메시 버퍼/입력 레이아웃
    ID3D11InputLayout*            m_pInputLayout = nullptr;
    ID3D11Buffer*                 m_pVertexBuffer = nullptr;
    UINT                          m_VertextBufferStride = 0;
    UINT                          m_VertextBufferOffset = 0;
    ID3D11Buffer*                 m_pIndexBuffer = nullptr;
    int                           m_nIndices = 0;

    // 공용 상수 버퍼 (b0)
    ID3D11Buffer*                 m_pConstantBuffer = nullptr;
    ConstantBuffer                m_ConstantBuffer{};                // CPU 캐시

    // 유틸 렌더러/디버그 박스
    class LineRenderer*           m_LineRenderer = nullptr;
    class Skybox*                 m_Skybox = nullptr;
    ID3D11Buffer*                 m_pDebugBoxVB = nullptr;
    ID3D11Buffer*                 m_pDebugBoxIB = nullptr;
    int                           m_DebugBoxIndexCount = 0;

    // 깊이/래스터라이저 상태
    ID3D11DepthStencilView*       m_pDepthStencilView = nullptr;
    ID3D11DepthStencilState*      m_pDepthStencilState = nullptr;
    ID3D11RasterizerState*        RSNoCull = nullptr;
    ID3D11RasterizerState*        RSCullClockWise = nullptr;

    // 데모/디버그용 텍스처 및 UI 표시 크기
    ID3D11ShaderResourceView*     m_TexHanakoSRV = nullptr;
    bool                          m_ShowHanako = false;
    ImVec2                        m_HanakoDrawSize = ImVec2(128, 128);
    ImVec2                        m_TexHanakoSize = ImVec2(0, 0);

    // 큐브 각 면 텍스처 Diffuse/Normal/Specular
    ID3D11ShaderResourceView*     m_pCubeTextureSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    ID3D11ShaderResourceView*     m_pNormalSRVs[6]      = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    ID3D11ShaderResourceView*     m_pSpecularSRVs[6]    = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

    // 시스템 정보
    SystemInfomation              m_SystemInfo;

    // 큐브 트랜스폼
    XMFLOAT3                      m_cubePos = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3                      m_cubeScale = { 1.0f, 1.0f, 1.0f };
    XMFLOAT3                      m_cubeRotation = { 0.0f, 0.0f, 0.0f };
    bool                          m_RotateModel = false;

    // 미러 큐브 트랜스폼
    XMFLOAT3                      m_mirrorCubePos = { 4.5f, 0.0f, 0.0f };
    XMFLOAT3                      m_mirrorCubeRotation = { 0.0f, 0.0f, 0.0f };
    float                         m_MirrorCubeScale = 2.0f;

    // 조명/재질
    DirectionalLight              m_DirLight = { {0,0,0,1}, {1,1,1,1}, {1,1,1,1}, {0,0,1}, 0.0f };
    Material                      m_Material = { {1,1,1,1}, {1,1,1,1}, {1,1,1,32}, {0,0,0,0} };
    Material                      m_mirrorCubeMaterial = { {0,0,0,1}, {0,0,0,1}, {0,0,0,32}, {1,1,1,0.02f} };

    // 라이트 마커 위치 / 카메라 기반 기본 행렬
    XMFLOAT3                      m_LightPosition = { 4.0f, 4.0f, 0.0f };
    ConstantBuffer                m_baseProjection{};

    // 셰이딩 옵션 / 클리어 컬러
    ShadingMode                   m_ShadingMode = ShadingMode::Phong;
    int                           m_EnableNormalMapForCube = 1;
    int                           m_UseSpecularMapForCube = 0;
    int                           m_LegacyShading = 1;
    XMFLOAT4                      m_ClearColor = { 0.125f, 0.125f, 0.125f, 1.0f };
    // 알파 가림(DepthWrite) 버그 재현 토글
    bool                          m_ReproAlphaOcclusion = false;

    // 모델 로딩 및 렌더링 FBX/OBJ/PMX
    RenderMode                    m_RenderMode = RenderMode::None;
    std::vector<std::unique_ptr<ModelEntry>> m_Models;            // 모델들
    ID3D11ShaderResourceView*     m_pFallbackWhite = nullptr;
    ID3D11ShaderResourceView*     m_pFallbackNormal = nullptr;
    ID3D11ShaderResourceView*     m_pFallbackBlack = nullptr;
    std::string                   m_ModelPathInputUTF8;

    // VMD 카메라 애니메이션 (간단한 재생용 상태)
    std::vector<vmd::VmdCameraFrame> m_VmdCameraFrames;
    bool                           m_VmdCameraUse = false;      // VMD 카메라 사용 여부
    bool                           m_VmdCameraPlaying = false;  // 재생 중 여부
    float                          m_VmdCameraCurrentFrame = 0.0f; // 현재 VMD 프레임(부동소수)
    float                          m_VmdCameraFrameRate = 30.0f;   // 초당 30프레임 기준
    float                          m_VmdCameraScale = 1.0f;       // MMD->엔진 스케일
};

// 멤버 매핑 매크로 제거됨: 직접 m_-> 멤버 접근을 사용합니다.

// ctor/dtor
App::App() : m_(new Impl) {}
App::~App() {}

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

// VMD 파일 선택 대화상자
static bool OpenFileDialogVMD(std::wstring& outPath)
{
    wchar_t file[MAX_PATH] = {0};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GameApp::m_hWnd;
    ofn.lpstrFilter = L"VMD Files (*.vmd)\0*.vmd\0All Files\0*.*\0\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) { outPath = file; return true; }
    return false;
}

// VMD 카메라 프레임을 DirectX 카메라(Eye/Center/Up)로 변환하는 간단한 헬퍼
// - scale: MMD 좌표를 엔진 좌표로 맞추기 위한 스케일 (1.0이면 그대로)
// - MMD: Y-up, Z-forward(오른손)
// - DirectX: Y-up, Z-forward(왼손)
static void VmdCameraToEyeCenterUp(const vmd::VmdCameraFrame& cam, float scale, XMFLOAT3& outEye, XMFLOAT3& outCenter, XMFLOAT3& outUp)
{
    // 1. 관심점(interest) Z축 반전
    XMFLOAT3 interestDX(cam.position[0] * scale, cam.position[1] * scale, -cam.position[2] * scale);

    // 2. 회전 행렬 (Y -> Z(반전) -> X), cam.orientation는 라디안
    //    - MMD(OpenGL, 오른손)과 DirectX(왼손) yaw 방향이 반대라서 Y 회전은 부호를 반대로 적용
    XMMATRIX rotY = XMMatrixRotationY(-cam.orientation[1]);
    XMMATRIX rotZ = XMMatrixRotationZ(-cam.orientation[2]);
    XMMATRIX rotX = XMMatrixRotationX(cam.orientation[0]);
    XMMATRIX rot  = rotY * rotZ * rotX;

    // 3. 카메라 위치: 타깃에서 distance만큼 뒤로 이동
    //    - VMD에서는 distance가 보통 음수로 들어오므로, -distance * scale을 사용
    float dist = -cam.distance * scale;
    XMMATRIX viewOffset = XMMatrixTranslation(0, 0, dist);
    viewOffset = rot * viewOffset;

    XMFLOAT3 eyeDX;
    XMStoreFloat3(&eyeDX, viewOffset.r[3] + XMLoadFloat3(&interestDX));

    // 4. Forward / Up 벡터 계산
    XMVECTOR forward = XMVector3TransformNormal(XMVectorSet(0, 0, -1, 0), rot);
    XMVECTOR up      = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0),  rot);
    XMVECTOR eyeVec  = XMLoadFloat3(&eyeDX);
    XMVECTOR centerVec = eyeVec + forward;

    XMFLOAT3 centerDX, upDX;
    XMStoreFloat3(&centerDX, centerVec);
    XMStoreFloat3(&upDX, up);

    outEye    = eyeDX;
    outCenter = centerDX;
    outUp     = upDX;
}

// VMD 파일에서 카메라 프레임만 안전하게 읽는 함수
// 직접 바이너리 파싱으로 구현 (VmdMotion::LoadFromStream의 "vector too long" 오류 방지)
static bool ReadVMDCameraFramesOnly(const std::wstring& vmdPath, std::vector<vmd::VmdCameraFrame>& outFrames)
{
    outFrames.clear();
    
    try
    {
        // std::filesystem::path를 사용하여 유니코드 경로 처리
        std::filesystem::path path(vmdPath);
        
        // 파일 스트림 열기
        std::ifstream stream(path, std::ios::binary);
        if (!stream || !stream.good())
        {
            char buf[512];
            sprintf_s(buf, "VMD 파일을 열 수 없습니다: %ls\n", vmdPath.c_str());
            OutputDebugStringA(buf);
            return false;
        }

        // 헤더 읽기 (30바이트: "Vocaloid Motion Data" + 버전)
        char buffer[30];
        stream.read(buffer, 30);
        if (stream.gcount() != 30 || strncmp(buffer, "Vocaloid Motion Data", 20) != 0)
        {
            OutputDebugStringA("VMD 파일 헤더가 올바르지 않습니다.\n");
            return false;
        }

        // 모델 이름 건너뛰기 (20바이트)
        stream.read(buffer, 20);
        if (stream.gcount() != 20)
        {
            OutputDebugStringA("VMD 파일 읽기 오류: 모델 이름\n");
            return false;
        }

        // 본 프레임 개수 읽기
        int32_t bone_frame_num = 0;
        stream.read((char*)&bone_frame_num, sizeof(int32_t));
        if (!stream.good())
        {
            OutputDebugStringA("VMD 파일 읽기 오류: 본 프레임 개수\n");
            return false;
        }

        // 디버그: 본 프레임 개수와 현재 위치 출력
        {
            char buf[256];
            std::streampos pos = stream.tellg();
            sprintf_s(buf, "디버그: 본 프레임 개수 = %d, 현재 파일 위치 = %lld\n", bone_frame_num, (long long)pos);
            OutputDebugStringA(buf);
        }

        // 본 프레임을 실제로 읽어서 정확한 오프셋 보장 (Read 메서드 사용)
        if (bone_frame_num < 0 || bone_frame_num > 1000000)
        {
            char buf[256];
            sprintf_s(buf, "경고: 본 프레임 개수가 비정상적입니다 (%d). 파일이 손상되었을 수 있습니다.\n", bone_frame_num);
            OutputDebugStringA(buf);
            return false;
        }

        for (int32_t i = 0; i < bone_frame_num; ++i)
        {
            vmd::VmdBoneFrame boneFrame;
            boneFrame.Read(&stream);
            if (!stream.good())
            {
                char buf[256];
                sprintf_s(buf, "경고: 본 프레임 %d/%d 읽기 실패\n", i + 1, bone_frame_num);
                OutputDebugStringA(buf);
                return false;
            }
        }

        // 표정 프레임 개수 읽기
        int32_t face_frame_num = 0;
        stream.read((char*)&face_frame_num, sizeof(int32_t));
        if (!stream.good())
        {
            OutputDebugStringA("VMD 파일 읽기 오류: 표정 프레임 개수\n");
            return false;
        }

        // 디버그: 표정 프레임 개수와 현재 위치 출력
        {
            char buf[256];
            std::streampos pos = stream.tellg();
            sprintf_s(buf, "디버그: 표정 프레임 개수 = %d, 현재 파일 위치 = %lld\n", face_frame_num, (long long)pos);
            OutputDebugStringA(buf);
        }

        // 표정 프레임을 실제로 읽어서 정확한 오프셋 보장 (Read 메서드 사용)
        if (face_frame_num < 0 || face_frame_num > 1000000)
        {
            char buf[256];
            sprintf_s(buf, "경고: 표정 프레임 개수가 비정상적입니다 (%d). 파일이 손상되었을 수 있습니다.\n", face_frame_num);
            OutputDebugStringA(buf);
            return false;
        }

        for (int32_t i = 0; i < face_frame_num; ++i)
        {
            vmd::VmdFaceFrame faceFrame;
            faceFrame.Read(&stream);
            if (!stream.good())
            {
                char buf[256];
                sprintf_s(buf, "경고: 표정 프레임 %d/%d 읽기 실패\n", i + 1, face_frame_num);
                OutputDebugStringA(buf);
                return false;
            }
        }

        // 카메라 프레임 개수 읽기
        int32_t camera_frame_num = 0;
        stream.read((char*)&camera_frame_num, sizeof(int32_t));
        if (!stream.good())
        {
            OutputDebugStringA("VMD 파일 읽기 오류: 카메라 프레임 개수\n");
            return false;
        }

        // 디버그: 카메라 프레임 개수와 현재 위치 출력
        {
            char buf[256];
            std::streampos pos = stream.tellg();
            sprintf_s(buf, "디버그: 카메라 프레임 개수 = %d, 현재 파일 위치 = %lld\n", camera_frame_num, (long long)pos);
            OutputDebugStringA(buf);
        }

        // 합리적인 범위 검증
        if (camera_frame_num < 0 || camera_frame_num > 100000)
        {
            char buf[256];
            sprintf_s(buf, "경고: 카메라 프레임 개수가 비정상적입니다 (%d). 파일이 손상되었을 수 있습니다.\n", camera_frame_num);
            OutputDebugStringA(buf);
            if (camera_frame_num < 0) return false;
            if (camera_frame_num > 100000) camera_frame_num = 100000;
        }

        if (camera_frame_num == 0)
        {
            OutputDebugStringA("VMD 파일에 카메라 프레임이 없습니다.\n");
            return true; // 카메라 프레임이 없는 것은 오류가 아님
        }

        // 카메라 프레임 읽기 (각 63바이트: 4(frame) + 4(distance) + 12(position) + 12(orientation) + 24(interpolation) + 4(angle) + 3(unknown))
        outFrames.reserve(camera_frame_num);
        for (int32_t i = 0; i < camera_frame_num; ++i)
        {
            vmd::VmdCameraFrame frame;
            frame.Read(&stream);
            
            if (!stream.good())
            {
                char buf[256];
                sprintf_s(buf, "경고: 카메라 프레임 %d/%d 읽기 실패. 파일이 손상되었을 수 있습니다.\n", i + 1, camera_frame_num);
                OutputDebugStringA(buf);
                break; // 읽기 실패 시 중단
            }
            
            outFrames.push_back(frame);
        }

        stream.close();

        // 디버그: 처음 몇 개 프레임의 원시 데이터 출력
        if (!outFrames.empty())
        {
            char buf[512];
            sprintf_s(buf, "디버그: 첫 번째 카메라 프레임 원시 데이터 - Frame: %d, Distance: %.6f, Position: (%.6f, %.6f, %.6f), Rotation: (%.6f, %.6f, %.6f), Angle: %.6f\n",
                outFrames[0].frame, outFrames[0].distance,
                outFrames[0].position[0], outFrames[0].position[1], outFrames[0].position[2],
                outFrames[0].orientation[0], outFrames[0].orientation[1], outFrames[0].orientation[2],
                outFrames[0].angle);
            OutputDebugStringA(buf);
        }

        char buf[256];
        sprintf_s(buf, "성공: %zu개의 카메라 프레임을 읽었습니다.\n", outFrames.size());
        OutputDebugStringA(buf);

        return true;
    }
    catch (const std::bad_alloc&)
    {
        OutputDebugStringA("메모리 할당 실패: VMD 파일이 너무 큽니다.\n");
        return false;
    }
    catch (const std::exception& e)
    {
        char buf[512];
        sprintf_s(buf, "VMD 파일 읽기 오류: %s\n", e.what());
        OutputDebugStringA(buf);
        return false;
    }
    catch (...)
    {
        OutputDebugStringA("알 수 없는 오류가 발생했습니다.\n");
        return false;
    }
}

// VMD 파일에서 카메라 프레임들을 읽어서 콘솔에 출력하는 함수
static void PrintVMDCameraFrames(const std::wstring& vmdPath)
{
    try
    {
        // 카메라 프레임만 안전하게 읽기
        std::vector<vmd::VmdCameraFrame> cameraFrames;
        if (!ReadVMDCameraFramesOnly(vmdPath, cameraFrames))
        {
            OutputDebugStringA("VMD 파일을 읽을 수 없거나 카메라 데이터가 없습니다.\n");
            return;
        }

        if (cameraFrames.empty())
        {
            OutputDebugStringA("VMD 파일에 카메라 프레임이 없습니다.\n");
            return;
        }

        // 카메라 프레임들을 프레임 번호 순으로 정렬
        std::sort(cameraFrames.begin(), cameraFrames.end(), 
            [](const vmd::VmdCameraFrame& a, const vmd::VmdCameraFrame& b) {
                return a.frame < b.frame;
            });

        // 각 카메라 프레임 정보 출력
        char buffer[1024];
        sprintf_s(buffer, "=== VMD 카메라 프레임 정보 (총 %zu개) ===\n", cameraFrames.size());
        OutputDebugStringA(buffer);

        // 출력 개수 제한 (너무 많으면 처음 1000개만 출력)
        const size_t MAX_PRINT_COUNT = 1000;
        size_t printCount = (cameraFrames.size() > MAX_PRINT_COUNT) ? MAX_PRINT_COUNT : cameraFrames.size();
        
        if (cameraFrames.size() > MAX_PRINT_COUNT)
        {
            sprintf_s(buffer, "참고: 프레임이 많아 처음 %zu개만 출력합니다.\n", MAX_PRINT_COUNT);
            OutputDebugStringA(buffer);
        }

        // MMD 좌표계 정보 및 변환 공식 출력
        OutputDebugStringA("=== MMD 좌표계 정보 및 DirectX 변환 공식 ===\n");
        OutputDebugStringA("MMD 좌표계: Y-up, Z-forward (오른손 좌표계)\n");
        OutputDebugStringA("DirectX 좌표계: Y-up, Z-forward (왼손 좌표계)\n\n");
        OutputDebugStringA("VMD 카메라 데이터:\n");
        OutputDebugStringA("  - position: interest(관심점, look-at 위치) - 미터 단위\n");
        OutputDebugStringA("  - distance: 카메라와 관심점 사이의 거리 - 미터 단위 (음수 가능)\n");
        OutputDebugStringA("  - orientation: 오일러 각도 (라디안, X=pitch, Y=yaw, Z=roll)\n");
        OutputDebugStringA("  - angle: 시야각 (도 단위, 0~180)\n\n");
        OutputDebugStringA("MMD -> DirectX 변환 공식 (saba 라이브러리 기준):\n");
        OutputDebugStringA("  1. interest = position * (1, 1, -1)  // Z축 반전\n");
        OutputDebugStringA("  2. 회전 순서: Y축 회전 -> Z축 회전(반전) -> X축 회전\n");
        OutputDebugStringA("  3. 카메라 위치 계산:\n");
        OutputDebugStringA("     - view = translate(0, 0, abs(distance))\n");
        OutputDebugStringA("     - rot = rotateY(yaw) * rotateZ(-roll) * rotateX(pitch)\n");
        OutputDebugStringA("     - view = rot * view\n");
        OutputDebugStringA("     - eye = view[3] + interest\n");
        OutputDebugStringA("     - center = rot * (0, 0, -1) + eye\n");
        OutputDebugStringA("     - up = rot * (0, 1, 0)\n\n");

        // 정상적인 프레임만 필터링
        std::vector<size_t> validFrameIndices;
        for (size_t i = 0; i < cameraFrames.size(); ++i)
        {
            const auto& cam = cameraFrames[i];
            
            // 정상적인 프레임인지 체크
            bool isValid = true;
            
            // 프레임 번호: 0 이상, 합리적인 범위
            if (!std::isfinite((float)cam.frame) || cam.frame < 0 || cam.frame > 1000000)
            {
                isValid = false;
            }
            
            // 거리: 합리적인 범위 (-1000 ~ 1000 미터)
            if (!std::isfinite(cam.distance) || cam.distance < -1000.0f || cam.distance > 1000.0f)
            {
                isValid = false;
            }
            
            // FOV: 0 ~ 180 도
            if (!std::isfinite(cam.angle) || cam.angle < 0.0f || cam.angle > 180.0f)
            {
                isValid = false;
            }
            
            // 위치: 합리적인 범위 (-1000 ~ 1000 미터)
            for (int j = 0; j < 3; ++j)
            {
                if (!std::isfinite(cam.position[j]) || 
                    cam.position[j] < -1000.0f || cam.position[j] > 1000.0f)
                {
                    isValid = false;
                    break;
                }
            }
            
            // 회전: 합리적인 범위 (-10 ~ 10 라디안, 약 -573도 ~ 573도)
            for (int j = 0; j < 3; ++j)
            {
                if (!std::isfinite(cam.orientation[j]) ||
                    cam.orientation[j] < -10.0f || cam.orientation[j] > 10.0f)
                {
                    isValid = false;
                    break;
                }
            }
            
            if (isValid)
            {
                validFrameIndices.push_back(i);
            }
        }
        
        // 정상적인 프레임만 출력
        sprintf_s(buffer, "정상적인 프레임: %zu개 (전체: %zu개)\n\n", validFrameIndices.size(), cameraFrames.size());
        OutputDebugStringA(buffer);
        
        if (validFrameIndices.empty())
        {
            OutputDebugStringA("정상적인 카메라 프레임이 없습니다. VMD 파일이 손상되었을 수 있습니다.\n");
            OutputDebugStringA("=== 출력 완료 ===\n");
            return;
        }
        
        // 출력 개수 제한
        size_t validPrintCount = (validFrameIndices.size() > MAX_PRINT_COUNT) ? MAX_PRINT_COUNT : validFrameIndices.size();
        
        for (size_t idx = 0; idx < validPrintCount; ++idx)
        {
            size_t i = validFrameIndices[idx];
            const auto& cam = cameraFrames[i];
            
            // 회전은 오일러 각도(라디안), FOV는 도 단위
            float rotX_deg = XMConvertToDegrees(cam.orientation[0]);
            float rotY_deg = XMConvertToDegrees(cam.orientation[1]);
            float rotZ_deg = XMConvertToDegrees(cam.orientation[2]);
            
            // MMD -> DirectX 변환 (saba 라이브러리 기준)
            // 1. interest Z축 반전
            XMFLOAT3 interestDX(cam.position[0], cam.position[1], -cam.position[2]);
            
            // 2. 회전 순서: Y -> Z(반전) -> X
            XMMATRIX rotY = XMMatrixRotationY(cam.orientation[1]);
            XMMATRIX rotZ = XMMatrixRotationZ(-cam.orientation[2]);  // Z축 반전
            XMMATRIX rotX = XMMatrixRotationX(cam.orientation[0]);
            XMMATRIX rot = rotY * rotZ * rotX;
            
            // 3. 카메라 위치 계산
            XMMATRIX view = XMMatrixTranslation(0, 0, std::abs(cam.distance));
            view = rot * view;
            XMFLOAT3 eyeDX;
            XMStoreFloat3(&eyeDX, view.r[3] + XMLoadFloat3(&interestDX));
            
            // 4. Look-at 방향 계산
            XMVECTOR forward = XMVector3TransformNormal(XMVectorSet(0, 0, -1, 0), rot);
            XMVECTOR up = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), rot);
            XMVECTOR eyeVec = XMLoadFloat3(&eyeDX);
            XMVECTOR centerVec = eyeVec + forward;
            XMFLOAT3 centerDX, upDX;
            XMStoreFloat3(&centerDX, centerVec);
            XMStoreFloat3(&upDX, up);
            
            // FOV 변환 (도 -> 라디안)
            float fovRad = XMConvertToRadians(cam.angle);
            
            // 프레임 번호, 원본 데이터, DirectX 변환된 값 출력
            sprintf_s(buffer, 
                "[%zu] Frame: %d\n"
                "  원본: Interest(%.3f, %.3f, %.3f) | Rot(%.3f, %.3f, %.3f deg) | Dist: %.3f | FOV: %.3f deg\n"
                "  DirectX: Eye(%.3f, %.3f, %.3f) | Center(%.3f, %.3f, %.3f) | Up(%.3f, %.3f, %.3f) | FOV: %.3f deg\n",
                idx, cam.frame,
                cam.position[0], cam.position[1], cam.position[2],
                rotX_deg, rotY_deg, rotZ_deg,
                cam.distance, cam.angle,
                eyeDX.x, eyeDX.y, eyeDX.z,
                centerDX.x, centerDX.y, centerDX.z,
                upDX.x, upDX.y, upDX.z,
                cam.angle);
            OutputDebugStringA(buffer);
        }

        if (validFrameIndices.size() > validPrintCount)
        {
            sprintf_s(buffer, "... (정상 프레임 %zu개 중 %zu개 출력됨)\n", validFrameIndices.size(), validPrintCount);
            OutputDebugStringA(buffer);
        }

        OutputDebugStringA("=== 출력 완료 ===\n");
    }
    catch (const std::exception& e)
    {
        char buffer[512];
        sprintf_s(buffer, "오류 발생: %s\n", e.what());
        OutputDebugStringA(buffer);
    }
    catch (...)
    {
        OutputDebugStringA("알 수 없는 오류가 발생했습니다.\n");
    }
}

void App::PrepareSkyFaceSRVs()
{
	// 다른 스카이박스로 바꿀 수도 있으니 해제하고 다시 로드
	for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_->m_pSkyFaceSRV[i]);
	    m_->m_SkyFaceSize = ImVec2(0, 0);
	    if (!m_->m_pTextureSRV) return;

	Microsoft::WRL::ComPtr<ID3D11Resource> res;
	m_->m_pTextureSRV->GetResource(res.GetAddressOf());
    if (!res) return;

	// 파괴됐는지 안됐는지 판단을 위해 Comptr이 필요하다
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
	HR_T(res.As(&tex2D));

    D3D11_TEXTURE2D_DESC desc{};
    tex2D->GetDesc(&desc);
	// 큐브맵은 6개의 array slice를 가짐. (여러 큐브면 6의 배수)
    if ((desc.ArraySize < 6)) return;

	// 크기 기록 (mip0 기준)
	m_->m_SkyFaceSize = ImVec2((float)desc.Width, (float)desc.Height);

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
        if (SUCCEEDED(m_->m_pDevice->CreateShaderResourceView(tex2D.Get(), &sd, &faceSRV)))
        {
            m_->m_pSkyFaceSRV[face] = faceSRV;
        }
    }
}

void App::ChangeSkyboxDDS(const wchar_t* ddsPath)
{
    if (m_->m_Skybox)
    {
        if (m_->m_Skybox->ChangeDDS(m_->m_pDevice, ddsPath))
        {
            // also set for face view and PS binding
            m_->m_pTextureSRV = m_->m_Skybox->GetTexture();
            PrepareSkyFaceSRVs();
            wcscpy_s(m_->m_CurrentSkyboxPath, ddsPath);
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

	if (!m_->m_SystemInfo.InitSysInfomation(m_->m_pDevice)) return false;

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
	// 자동 회전은 모델 내부의 autoRotate 변수를 사용함
	for (auto& mdlPtr : m_->m_Models)
	{
		auto& mdl = *mdlPtr;
		if (mdl.autoRotate)
		{
			mdl.rotDeg.y += 45.0f * dt;
			mdl.rotDeg.y = std::fmod(mdl.rotDeg.y + 180.0f, 360.0f) - 180.0f;
		}
		if (mdl.source == ModelSource::FBX)
		{
			// FBX 애니메이션 (팔레트 업데이트만 수행)
			mdl.fbx.UpdateAnimation(m_->m_pDeviceContext, dt);
		}
		else if (mdl.source == ModelSource::PMX)
		{
			// PMX + VMD 애니메이션 실행
			mdl.pmx.UpdateAnimation(m_->m_pDeviceContext, dt);
		}
	}
	// 기본 카메라용 world0 (원점 단위행렬)
	XMMATRIX model = XMMatrixIdentity();

	// ============================== 카메라 행렬 업데이트 ==============================
	m_->m_baseProjection.world = XMMatrixTranspose(model);

	// 1) VMD 카메라 애니메이션이 켜져 있으면, 그 값을 사용
	if (m_->m_VmdCameraUse && !m_->m_VmdCameraFrames.empty())
	{
		// 재생 중이면 프레임 증가 (초당 m_VmdCameraFrameRate 프레임)
		if (m_->m_VmdCameraPlaying)
		{
			m_->m_VmdCameraCurrentFrame += dt * m_->m_VmdCameraFrameRate;

			const int firstFrame = m_->m_VmdCameraFrames.front().frame;
			const int lastFrame  = m_->m_VmdCameraFrames.back().frame;

			// 루프 재생 (마지막 프레임을 넘으면 다시 처음으로)
			if (m_->m_VmdCameraCurrentFrame > (float)lastFrame)
			{
				m_->m_VmdCameraCurrentFrame = (float)firstFrame;
			}
		}

		// 현재 프레임에 해당하는 두 키를 찾아서 선형 보간
		const auto& frames = m_->m_VmdCameraFrames;
		float curF = m_->m_VmdCameraCurrentFrame;

		// curF보다 바로 앞/뒤에 있는 키프레임을 찾는다.
		const vmd::VmdCameraFrame* a = &frames.front();
		const vmd::VmdCameraFrame* b = &frames.back();

		for (size_t i = 0; i + 1 < frames.size(); ++i)
		{
			if (curF >= (float)frames[i].frame && curF <= (float)frames[i + 1].frame)
			{
				a = &frames[i];
				b = &frames[i + 1];
				break;
			}
		}

		float t = 0.0f;
		if (b->frame != a->frame)
		{
			t = (curF - (float)a->frame) / (float)(b->frame - a->frame);
		}

		// 간단한 선형 보간 (위치/각도/거리/FOV 모두 직선 보간)
		vmd::VmdCameraFrame cur = *a;
		for (int i = 0; i < 3; ++i)
		{
			cur.position[i]    = a->position[i]    + (b->position[i]    - a->position[i])    * t;
			cur.orientation[i] = a->orientation[i] + (b->orientation[i] - a->orientation[i]) * t;
		}
		cur.distance = a->distance + (b->distance - a->distance) * t;
		cur.angle    = a->angle    + (b->angle    - a->angle)    * t;

		// VMD 카메라 → DirectX 카메라(Eye/Center/Up)
		XMFLOAT3 eyeDX{}, centerDX{}, upDX{};
		VmdCameraToEyeCenterUp(cur, m_->m_VmdCameraScale, eyeDX, centerDX, upDX);

		XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3(&eyeDX), XMLoadFloat3(&centerDX), XMLoadFloat3(&upDX));

        // FOV: VMD에서 0이면 기존 카메라 FOV를 사용, 0 또는 비정상 값이면 안전한 기본값으로 대체
        float fovRad = (cur.angle > 0.0f)
            ? XMConvertToRadians(cur.angle)
            : m_Camera.GetFovYRad();
        if (!std::isfinite(fovRad) || std::fabs(fovRad) < 0.0001f)
        {
            // DirectX의 XMMatrixPerspectiveFovLH는 0 또는 너무 작은 FOV를 허용하지 않으므로
            // 안전한 기본값(45도)을 사용한다.
            fovRad = XMConvertToRadians(45.0f);
        }

		float aspect = AspectRatio();
		float nearZ  = m_Camera.GetNearZ();
		float farZ   = m_Camera.GetFarZ();
		XMMATRIX proj = XMMatrixPerspectiveFovLH(fovRad, aspect, nearZ, farZ);

		m_->m_baseProjection.view = XMMatrixTranspose(view);
		m_->m_baseProjection.proj = XMMatrixTranspose(proj);
		m_->m_baseProjection.eyePos = eyeDX;
	}
	else
	{
		// 2) 평소에는 마우스로 조작하는 기본 카메라 사용
		m_->m_baseProjection.view  = XMMatrixTranspose(m_Camera.GetViewMatrixXM());
		m_->m_baseProjection.proj  = XMMatrixTranspose(m_Camera.GetProjMatrixXM());
		m_->m_baseProjection.eyePos = m_Camera.GetPosition();
	}

	m_->m_baseProjection.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(model)));

	XMFLOAT3 lightDir = m_->m_DirLight.direction;
	XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&lightDir));
	XMStoreFloat3(&lightDir, v);

	// DirectionalLight 정규화된 방향으로 대입 
	m_->m_baseProjection.dirLight = m_->m_DirLight;
	m_->m_baseProjection.dirLight.direction = lightDir;
	m_->m_baseProjection.dirLight.pad = 0.0f;

	m_->m_baseProjection.pad = 0.0f;

	// 머티리얼을 기본 캐시에 반영해 둔다
	m_->m_baseProjection.material = m_->m_Material;

	m_->m_SystemInfo.Tick(dt);
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
	// ============================== D3D11 백버퍼/깊이 버퍼 클리어 ==============================
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	UINT stride = m_->m_VertextBufferStride;	// 바이트 수
	UINT offset = m_->m_VertextBufferOffset;

	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pRenderTargetView, color);
	m_->m_pDeviceContext->ClearDepthStencilView(m_->m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// ============================== 기본 큐브/모델 렌더 상태 설정(IA/파이프라인) ==============================
	// 1 ~ 3 . IA 단계 설정
	// 정점을 어떻게 이어서 그릴 것인지를 선택하는 부분
	// 1. 버퍼를 잡아주기
	// 2. 입력 레이아웃을 잡아주기
	// 3. 인덱스 버퍼를 잡아주기
	m_->m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    // 렌더 모드에 따라 VB/IB 바인딩 결정
	if (!(m_->m_RenderMode == RenderMode::Model && !m_->m_Models.empty()))
	{
		// 기본 큐브
		m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pVertexBuffer, &stride, &offset);
		m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayout);
		m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	}

	// 컬러 클리어 및 스카이박스/배경 선택
	if (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Off)
	{
		float clr[4] = { m_->m_ClearColor.x, m_->m_ClearColor.y, m_->m_ClearColor.z, m_->m_ClearColor.w };
		m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pRenderTargetView, clr);
	}

	m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShader, nullptr, 0);
	m_->m_pDeviceContext->PSSetShader(m_->m_pPixelShader, nullptr, 0);

	m_->m_ConstantBuffer.world = m_->m_baseProjection.world;
	m_->m_ConstantBuffer.view  = m_->m_baseProjection.view;
	m_->m_ConstantBuffer.proj  = m_->m_baseProjection.proj;
	{
		// 비균등 스케일을 해결한 코드. 역전치 곱하기
		auto invWorlNormal = XMMatrixInverse(nullptr, m_->m_baseProjection.world);
		m_->m_ConstantBuffer.worldInvTranspose = XMMatrixTranspose(invWorlNormal);
	}

	// 기본 광원/카메라 UI 반영
	{
		XMFLOAT3 lightDir = m_->m_DirLight.direction;
		XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&lightDir));
		XMStoreFloat3(&lightDir, v);
		// DirectionalLight 필드 대입 정규화된 방향
		m_->m_ConstantBuffer.dirLight = m_->m_DirLight;
		m_->m_ConstantBuffer.dirLight.direction = lightDir;
		m_->m_ConstantBuffer.dirLight.pad = 0.0f;
	}

	m_->m_ConstantBuffer.eyePos = m_Camera.GetPosition();
	m_->m_ConstantBuffer.pad = 0.0f;
	// 셰이딩 모드 전달
	m_->m_ConstantBuffer.shadingMode = (int)m_->m_ShadingMode;
	m_->m_ConstantBuffer.pad2 = XMFLOAT3(0,0,0);
	m_->m_ConstantBuffer.enableNormalMap = m_->m_EnableNormalMapForCube;
	m_->m_ConstantBuffer.pad3 = XMFLOAT3(0,0,0);
	m_->m_ConstantBuffer.useSpecularMap = m_->m_UseSpecularMapForCube;
	m_->m_ConstantBuffer.pad4 = XMFLOAT3(0,0,0);
	// 머티리얼 채우기
	m_->m_ConstantBuffer.material = m_->m_Material;

	D3D11_MAPPED_SUBRESOURCE mappedData;
	HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
	memcpy_s(mappedData.pData, sizeof(ConstantBuffer), &m_->m_ConstantBuffer, sizeof(ConstantBuffer));
	m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);

	m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetSamplers(0, 1, &m_->m_pSamplerState);
    // 큐브맵을 t1 슬롯에 바인딩 (픽셀 셰이더에서 g_TexCube : t1)
    m_->m_pDeviceContext->PSSetShaderResources(1, 1, &m_->m_pTextureSRV);

	
    if (m_->m_RenderMode == RenderMode::Model && !m_->m_Models.empty())
    {
        // 모든 모델 렌더
		for (auto& mdlPtr : m_->m_Models)
		{
			// IA 바인딩
			UINT s = mdlPtr->stride; UINT o = 0;
			if (!mdlPtr->vb || !mdlPtr->ib) continue;
			m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &mdlPtr->vb, &s, &o);
            // FBX 스켈레톤이 있으면 스키닝 VS/IL로 교체, 아니면 기본
			ID3D11Buffer* cbBones = nullptr;
			bool hasSkeleton = false;
			if (mdlPtr->source == ModelSource::FBX)
			{
				cbBones = mdlPtr->fbx.GetBoneConstantBuffer();
				hasSkeleton = mdlPtr->fbx.HasSkeleton();
			}
			else if (mdlPtr->source == ModelSource::PMX)
			{
				cbBones = mdlPtr->pmx.GetBoneConstantBuffer();
				hasSkeleton = mdlPtr->pmx.HasSkeleton();
			}
            bool useSkinned = hasSkeleton
				&& (mdlPtr->stride == sizeof(VertexSkinnedTBN))
				&& (m_->m_pInputLayoutSkinned != nullptr)
				&& (m_->m_pVertexShaderSkinned != nullptr)
				&& (cbBones != nullptr);
			if (useSkinned)
			{
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayoutSkinned);
				m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShaderSkinned, nullptr, 0);
				// 본 팔레트 상수버퍼에 바인딩하기
                if (cbBones)
                {
					// PMX, VMD 애니메이션이 없는 경우는 단위 팔레트 업로드
                    if (mdlPtr->source == ModelSource::PMX && !mdlPtr->pmx.HasAnimations())
                    {
                        mdlPtr->pmx.UploadIdentityPalette(m_->m_pDeviceContext);
                    }
                    m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &cbBones);
                }
			}
			else
			{
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayout);
				m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShader, nullptr, 0);
                // 스키닝 미사용 시 VS b1 해제(안전)
				ID3D11Buffer* nullCB = nullptr; m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &nullCB);
			}
			m_->m_pDeviceContext->IASetIndexBuffer(mdlPtr->ib, DXGI_FORMAT_R32_UINT, 0);

			// 월드 행렬 (per-model)
			XMMATRIX rotYaw = XMMatrixRotationY(XMConvertToRadians(mdlPtr->rotDeg.y));
			XMMATRIX rotPitch = XMMatrixRotationX(XMConvertToRadians(mdlPtr->rotDeg.x));
			XMMATRIX rotRoll = XMMatrixRotationZ(XMConvertToRadians(mdlPtr->rotDeg.z));
			XMMATRIX S = XMMatrixScaling(mdlPtr->scale.x, mdlPtr->scale.y, mdlPtr->scale.z);
			XMMATRIX T = XMMatrixTranslation(mdlPtr->pos.x, mdlPtr->pos.y, mdlPtr->pos.z);
			XMMATRIX W = S * rotPitch * rotYaw * rotRoll * T;

			ConstantBuffer cb = m_->m_ConstantBuffer;
			cb.world = XMMatrixTranspose(W);
			cb.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
			cb.material = m_->m_Material;
			cb.shadingMode = (int)m_->m_ShadingMode;
			cb.enableNormalMap = m_->m_EnableNormalMapForCube;
			cb.useSpecularMap = m_->m_UseSpecularMapForCube;

			D3D11_MAPPED_SUBRESOURCE mapped;
			HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
			memcpy_s(mapped.pData, sizeof(ConstantBuffer), &cb, sizeof(ConstantBuffer));
			m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
			m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
			m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

			// 서브셋 텍스처 및 드로우
            for (const auto& sub : mdlPtr->subsets)
			{
				ID3D11ShaderResourceView* srvDiffuse = nullptr;
				if (sub.materialIndex < mdlPtr->materialSRVs.size()) srvDiffuse = mdlPtr->materialSRVs[sub.materialIndex];
				if (!srvDiffuse) srvDiffuse = m_->m_pFallbackWhite;
				ID3D11ShaderResourceView* srvNormal = (m_->m_EnableNormalMapForCube != 0) ? m_->m_pFallbackNormal : nullptr;
				ID3D11ShaderResourceView* srvSpec = (m_->m_UseSpecularMapForCube != 0) ? m_->m_pFallbackWhite : nullptr;
				m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
				m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
				m_->m_pDeviceContext->PSSetShaderResources(3, 1, &srvSpec);

				m_->m_pDeviceContext->DrawIndexed(sub.count, sub.start, 0);
			}
            // 모델별 루프 끝에서 VS/IL 복원은 다음 모델에서 다시 설정되므로 별도 복원 불필요
		}
    }
	else if (m_->m_RenderMode == RenderMode::Cube)
	{
		for (int face = 0; face < 6; ++face)
		{
			ID3D11ShaderResourceView* srvDiffuse = m_->m_pCubeTextureSRVs[face];
			ID3D11ShaderResourceView* srvNormal = m_->m_pNormalSRVs[face] ? m_->m_pNormalSRVs[face] : m_->m_pCubeTextureSRVs[face];
			ID3D11ShaderResourceView* srvSpec = m_->m_pSpecularSRVs[face] ? m_->m_pSpecularSRVs[face] : m_->m_pCubeTextureSRVs[face];
			m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
			m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
			m_->m_pDeviceContext->PSSetShaderResources(3, 1, &srvSpec);
			m_->m_pDeviceContext->DrawIndexed(6, face * 6, 0);
		}
	}
	

    // Mirror Cube: 모델 모드일 때는 생략하고, 큐브 모드에서만 거울 큐브 표시
	if (m_->m_RenderMode == RenderMode::Cube)
	{
		ConstantBuffer mirrorCB = m_->m_ConstantBuffer;
		// 월드: 헤더 공개된 mirrorCube 트랜스폼 사용(스케일*회전*이동)
		XMMATRIX rotYaw = XMMatrixRotationY(XMConvertToRadians(m_->m_mirrorCubeRotation.y));
		XMMATRIX rotPitch = XMMatrixRotationX(XMConvertToRadians(m_->m_mirrorCubeRotation.x));
		XMMATRIX rotRoll = XMMatrixRotationZ(XMConvertToRadians(m_->m_mirrorCubeRotation.z));
		XMMATRIX Sm = XMMatrixScaling(m_->m_MirrorCubeScale, m_->m_MirrorCubeScale, m_->m_MirrorCubeScale);
		XMMATRIX Tm = XMMatrixTranslation(m_->m_mirrorCubePos.x, m_->m_mirrorCubePos.y, m_->m_mirrorCubePos.z);
		Tm = Sm * rotPitch * rotYaw * rotRoll * Tm;
		mirrorCB.world = XMMatrixTranspose(Tm);
		mirrorCB.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(Tm)));
		// 재질: 헤더에 공개한 m_mirrorCubeMaterial 사용
		mirrorCB.material = m_->m_mirrorCubeMaterial;
		// pad=4.0f : PS에서 반사 게이팅 override
		mirrorCB.pad = 4.0f;
		mirrorCB.shadingMode = (int)m_->m_ShadingMode;
		mirrorCB.pad2 = XMFLOAT3(0, 0, 0);

		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &mirrorCB, sizeof(ConstantBuffer));
		m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
		m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
		m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

		// 블렌딩 ON으로 반사에도 부드러운 에지 허용
		FLOAT blendFactor2[4] = { 0,0,0,0 };
		m_->m_pDeviceContext->OMSetBlendState(m_->m_pAlphaBlendState, blendFactor2, 0xFFFFFFFF);

		// t0에 임의의 불투명 텍스처를 바인딩(컷아웃 통과용). 여기서는 face0 재사용
		ID3D11ShaderResourceView* srvFace0 = m_->m_pCubeTextureSRVs[0];
		m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvFace0);
		// 드로우: 동일 인덱스 범위를 6면 반복
		for (int face = 0; face < 6; ++face) m_->m_pDeviceContext->DrawIndexed(6, face * 6, 0);
		m_->m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		// pad 복원
		m_->m_ConstantBuffer.pad = 0.0f;
	}

    // 라이트 위치 마커 큐브 그리기 (작은 스케일, 흰색) - 항상
	{
		ConstantBuffer marker = m_->m_ConstantBuffer;
		XMMATRIX S = XMMatrixScaling(1.0f, 1.0f, 1.0f);
		XMMATRIX T = XMMatrixTranslation(m_->m_LightPosition.x, m_->m_LightPosition.y, m_->m_LightPosition.z);
		marker.world = XMMatrixTranspose(S * T);
		marker.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(S * T)));
		marker.pad = 2.0f; // PS에서 흰색 출력 토글
		marker.shadingMode = (int)m_->m_ShadingMode;
		marker.pad2 = XMFLOAT3(0, 0, 0);

		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &marker, sizeof(ConstantBuffer));
		m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
		m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
		m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

		UINT dbgStride = sizeof(VertexLightTex);
		UINT dbgOffset = 0;
		m_->m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pDebugBoxVB, &dbgStride, &dbgOffset);
		m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayout);
		m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pDebugBoxIB, DXGI_FORMAT_R32_UINT, 0);
		m_->m_pDeviceContext->DrawIndexed(m_->m_DebugBoxIndexCount, 0, 0);
	}

	// 라이트 방향 표시 라인 그리기(빨간색)
	{
		// pad=3.0은 라인 디버그에 이용.
		ConstantBuffer lineCB = m_->m_ConstantBuffer;
		lineCB.world = XMMatrixTranspose(XMMatrixIdentity());
		lineCB.view = m_->m_baseProjection.view;
		lineCB.proj = m_->m_baseProjection.proj;
		lineCB.worldInvTranspose = XMMatrixIdentity();
		lineCB.pad = 3.0f;
		lineCB.shadingMode = (int)m_->m_ShadingMode;
		lineCB.pad2 = XMFLOAT3(0, 0, 0);
		D3D11_MAPPED_SUBRESOURCE mappedLine;
		HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedLine));
		memcpy_s(mappedLine.pData, sizeof(ConstantBuffer), &lineCB, sizeof(ConstantBuffer));
		m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
		m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
		m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

		// 라인 전용 VS/InputLayout로 컬러 보존
		ID3D11VertexShader* prevVS = m_->m_pVertexShader;
		ID3D11InputLayout* prevIL = m_->m_pInputLayout;
		m_->m_pDeviceContext->VSSetShader(m_->m_pLineVS, nullptr, 0);
		m_->m_pDeviceContext->IASetInputLayout(m_->m_pLineInputLayout);

		// light direction (red)
		m_->m_LineRenderer->DrawLightDirection(m_->m_pDeviceContext, m_->m_LightPosition, m_->m_DirLight.direction, 2.0f, m_->m_pLineInputLayout, m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);
		// symmetric axes centered at origin for better grid feel
		m_->m_LineRenderer->DrawAxesSymmetric(m_->m_pDeviceContext, 100.0f, m_->m_pLineInputLayout, m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);

		// 복원
		m_->m_pDeviceContext->VSSetShader(prevVS, nullptr, 0);
		m_->m_pDeviceContext->IASetInputLayout(prevIL);
	}

    // SkyBox 렌더링 (상태 보존/복구)
	if (m_->m_SkyBoxChoice != App::Impl::SkyBoxChoice::Off)
	{
		UINT stride = m_->m_VertextBufferStride;
		UINT offset = m_->m_VertextBufferOffset;
		m_->m_Skybox->Render(m_->m_pDeviceContext, m_->m_pVertexBuffer, m_->m_pIndexBuffer, m_->m_nIndices, stride, offset, m_->m_baseProjection.view, m_->m_baseProjection.proj);
	}

	// 화면 오버레이 축(NDC)에 작게 표시
	{
		// pad=3 설정 정점색으로 출력되도록
		ConstantBuffer overlayCB = m_->m_ConstantBuffer;
		overlayCB.world = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.view = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.proj = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.worldInvTranspose = XMMatrixIdentity();
		overlayCB.pad = 3.0f;
		overlayCB.shadingMode = (int)m_->m_ShadingMode;
		overlayCB.pad2 = XMFLOAT3(0, 0, 0);
		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &overlayCB, sizeof(ConstantBuffer));
		m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
		m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
		m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

		// 좌상단 작은 축 표시
		m_->m_LineRenderer->DrawAxesOverlay(m_->m_pDeviceContext, XMMatrixTranspose(m_->m_baseProjection.view), DirectX::XMFLOAT2(-0.9f, 0.85f), 0.08f, m_->m_pLineInputLayout, m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);
	}

	// ImGui 프레임 및 UI 렌더링
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (ImGui::Begin("Controls"))
	{
		// SkyBox 선택
		{
			int cur = (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Off) ? 0 : (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Hanako ? 1 : 2);
			const char* items[] = { "Off", "Hanako.dds", "cubemap.dds" };
			if (ImGui::Combo("SkyBox Choice", &cur, items, IM_ARRAYSIZE(items)))
			{
				m_->m_SkyBoxChoice = (cur == 0) ? App::Impl::SkyBoxChoice::Off : (cur == 1 ? App::Impl::SkyBoxChoice::Hanako : App::Impl::SkyBoxChoice::CubeMap);
				if (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Off)
				{
					// Off: 배경 단색 사용
				}
				else
				{
					const wchar_t* path = (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Hanako) ? L"..\\Resource\\Skybox\\Hanako.dds" : L"..\\Resource\\Skybox\\cubemap.dds";
					ChangeSkyboxDDS(path);
				}
			}
			if (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Off)
			{
				ImGui::ColorEdit4("Background Color", &m_->m_ClearColor.x);
			}
		}
		ImGui::Text("Cube Transforms");
		ImGui::Checkbox("Rotate Cube", &m_->m_RotateModel);
		ImGui::DragFloat3("Cube Scale", &m_->m_cubeScale.x, 0.1f, 20.0f);
		ImGui::DragFloat3("Cube Pos (x,y,z)", &m_->m_cubePos.x, 0.1f);
		// 모델 회전(도) 편집
		ImGui::DragFloat3("Cube Rotation (deg)", &m_->m_cubeRotation.x, 1.0f, -360.0f, 360.0f, "%.1f");
		ImGui::Separator();
		ImGui::Text("Camera");
		{
			if (ImGui::Button("Reset"))
			{
				m_Camera.Reset();
			}
			ImGui::SliderFloat("Camera Speed", &m_Camera.m_MoveSpeed, 10.0f, 500.0f, "%.1f");
			DirectX::XMFLOAT3 pos = m_Camera.GetPosition();
			if (ImGui::DragFloat3("Camera Pos (x,y,z)", &pos.x, 0.1f))
			{
				m_Camera.SetPosition(pos);
			}
			float fovDeg = XMConvertToDegrees(m_Camera.GetFovYRad());
			if (ImGui::SliderFloat("Camera FOV (deg)", &fovDeg, 30.0f, 120.0f))
			{
				m_Camera.SetFrustum(XMConvertToRadians(fovDeg), AspectRatio(), m_Camera.GetNearZ(), m_Camera.GetFarZ());
			}
			float nearZ = m_Camera.GetNearZ();
			float farZ = m_Camera.GetFarZ();
			if (ImGui::DragFloatRange2("Near/Far", &nearZ, &farZ, 0.1f, 0.01f, 5000.0f, "Near: %.2f", "Far: %.2f"))
			{
				m_Camera.SetFrustum(m_Camera.GetFovYRad(), AspectRatio(), nearZ, farZ);
			}
		}
		ImGui::Separator();
		ImGui::Text("Shading");
		{
            int mode = (int)m_->m_ShadingMode;
			const char* modes[] = { "Phong", "Blinn-Phong", "Lambert", "Unlit", "TextureOnly" };
			if (ImGui::Combo("Shading Mode", &mode, modes, IM_ARRAYSIZE(modes)))
			{
				m_->m_ShadingMode = (ShadingMode)mode;
			}
            ImGui::Checkbox("Enable Normal Map", (bool*)&m_->m_EnableNormalMapForCube);
            ImGui::Checkbox("Use Specular Map", (bool*)&m_->m_UseSpecularMapForCube);
		}
		ImGui::Separator();
		ImGui::Text("Light");
		ImGui::DragFloat3("Light Direction", &m_->m_DirLight.direction.x, 0.05f);
		ImGui::ColorEdit4("Ambient", &m_->m_DirLight.ambient.x);
		ImGui::ColorEdit4("Diffuse", &m_->m_DirLight.diffuse.x);
		ImGui::ColorEdit4("Specular", &m_->m_DirLight.specular.x);
		ImGui::Separator();
		ImGui::Text("Material");
		ImGui::ColorEdit4("Ambient (ka)", &m_->m_Material.ambient.x);
		ImGui::ColorEdit4("Diffuse (kd)", &m_->m_Material.diffuse.x);
		ImGui::ColorEdit4("Specular (ks)", &m_->m_Material.specular.x);
		ImGui::DragFloat("Shininess (alpha)", &m_->m_Material.specular.w, 0.05f, 1.0f, 256.0f);
		ImGui::ColorEdit4("Reflect (kr, a=roughness)", &m_->m_Material.reflect.x);

		ImGui::Separator();
	}
	ImGui::End();

    // Models 독립 창
    {
        ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);

        ImGui::Begin("Model Loader (FBX / OBJ / PMX)");
        // 버그 재현 토글 UI
        ImGui::Checkbox("Repro alpha occlusion (DepthWrite+ConstBlend)", &m_->m_ReproAlphaOcclusion);
        if (m_->m_ReproAlphaOcclusion)
        {
            ImGui::TextDisabled("Tip: Draw near-to-far to see rear hidden by alpha");
        }
		// 렌더 모드 선택
		{
			int curMode = (m_->m_RenderMode == RenderMode::None) ? 0 : (m_->m_RenderMode == RenderMode::Cube ? 1 : 2);
			const char* items[] = { "None", "Cube", "Model" };
			if (ImGui::Combo("Render Mode", &curMode, items, IM_ARRAYSIZE(items)))
			{
				m_->m_RenderMode = (curMode == 0) ? RenderMode::None : (curMode == 1 ? RenderMode::Cube : RenderMode::Model);
			}
		}
		if (ImGui::Button("Browse Model..."))
		{
			std::wstring pathW;
			if (OpenFileDialogModel(pathW))
			{
				if (LoadModelFromFile(pathW)) { m_->m_RenderMode = RenderMode::Model; }
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Unload Model"))
		{
			UnloadModel();
			m_->m_RenderMode = RenderMode::None; // 요구사항: 시작/언로드시 아무것도 렌더 X
		}

		// VMD 카메라 로드/재생
		ImGui::Separator();
		ImGui::Text("VMD Camera");
		if (ImGui::Button("Load VMD Camera and Play"))
		{
			std::wstring vmdPath;
			if (OpenFileDialogVMD(vmdPath))
			{
				std::vector<vmd::VmdCameraFrame> frames;
				if (ReadVMDCameraFramesOnly(vmdPath, frames) && !frames.empty())
				{
					// 프레임 번호 기준으로 정렬
					std::sort(frames.begin(), frames.end(),
						[](const vmd::VmdCameraFrame& a, const vmd::VmdCameraFrame& b) { return a.frame < b.frame; });

					m_->m_VmdCameraFrames = std::move(frames);
					m_->m_VmdCameraUse = true;
					m_->m_VmdCameraPlaying = true;
					m_->m_VmdCameraCurrentFrame = (float)m_->m_VmdCameraFrames.front().frame;
				}
				else
				{
					OutputDebugStringA("VMD 카메라 데이터를 읽지 못했습니다.\n");
				}
			}
		}
		if (!m_->m_VmdCameraFrames.empty())
		{
			ImGui::Checkbox("Use VMD Camera", &m_->m_VmdCameraUse);
			ImGui::SameLine();
			if (ImGui::Button(m_->m_VmdCameraPlaying ? "Pause" : "Play"))
			{
				m_->m_VmdCameraPlaying = !m_->m_VmdCameraPlaying;
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop"))
			{
				m_->m_VmdCameraPlaying = false;
				m_->m_VmdCameraCurrentFrame = (float)m_->m_VmdCameraFrames.front().frame;
			}

			int firstFrame = m_->m_VmdCameraFrames.front().frame;
			int lastFrame  = m_->m_VmdCameraFrames.back().frame;
			float curFrame = m_->m_VmdCameraCurrentFrame;
			ImGui::Text("Frame: %.1f (%d ~ %d)", curFrame, firstFrame, lastFrame);

            // MMD 좌표계와 엔진 좌표계의 스케일을 맞추기 위한 간단한 스케일 팩터
            ImGui::SliderFloat("VMD Camera Scale", &m_->m_VmdCameraScale, 0.1f, 5.0f, "%.2f");
		}

		if (!m_->m_Models.empty())
		{
			ImGui::Text("Models");
			for (size_t i = 0; i < m_->m_Models.size(); ++i)
			{
				auto& mdl = *m_->m_Models[i];
				ImGui::PushID((int)i);
				ImGui::Separator();
                ImGui::Text("Model #%d : %s", (int)i, Utf8FromWString(m_->m_Models[i]->modelName).c_str());
				ImGui::DragFloat3("Position", &mdl.pos.x, 0.1f);
				ImGui::DragFloat3("Rotation (deg)", &mdl.rotDeg.x, 1.0f, -360.0f, 360.0f, "%.1f");
				ImGui::DragFloat3("Scale", &mdl.scale.x, 0.01f, 0.001f, 100.0f, "%.3f");
				ImGui::Checkbox("Auto Rotate (Yaw)", &mdl.autoRotate);
				if (mdl.source == ModelSource::FBX)
				{
					if (mdl.fbx.HasAnimations())
					{
						const auto& names = mdl.fbx.GetAnimationNames();
						if (mdl.uiSelectedAnim < 0 || mdl.uiSelectedAnim >= (int)names.size()) mdl.uiSelectedAnim = mdl.fbx.GetCurrentAnimationIndex();
						ImGui::Text("FBX Animations");
						if (ImGui::BeginListBox("##AnimList", ImVec2(-FLT_MIN, 4 * ImGui::GetTextLineHeightWithSpacing())))
						{
							for (int a = 0; a < (int)names.size(); ++a)
							{
								bool sel = (a == mdl.uiSelectedAnim);
								if (ImGui::Selectable(names[a].c_str(), sel))
								{
									mdl.uiSelectedAnim = a;
									mdl.fbx.SetCurrentAnimation(a);
								}
								if (sel) ImGui::SetItemDefaultFocus();
							}
							ImGui::EndListBox();
						}
						ImGui::Checkbox("Play", &mdl.uiAnimPlaying);
						mdl.fbx.SetAnimationPlaying(mdl.uiAnimPlaying);
						double cur = mdl.fbx.GetAnimationTimeSeconds();
						double dur = mdl.fbx.GetClipDurationSec(mdl.fbx.GetCurrentAnimationIndex());
						float curF = (float)cur, durF = (float)dur;
						if (durF > 0.0f)
						{
							if (ImGui::SliderFloat("Time (s)", &curF, 0.0f, durF)) mdl.fbx.SetAnimationTimeSeconds((double)curF);
						}
					}
				}
						else if (mdl.source == ModelSource::PMX)
						{
							if (ImGui::Button("Load VMD..."))
							{
								std::wstring vmdPath;
								wchar_t file[MAX_PATH] = {0};
								OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = GameApp::m_hWnd; ofn.lpstrFilter = L"VMD Files (*.vmd)\0*.vmd\0All Files\0*.*\0\0"; ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
								if (GetOpenFileNameW(&ofn))
								{
									if (mdl.pmx.LoadVMD(m_->m_pDevice, file))
									{
										// ?ε? ???? ?? ??? ????
										mdl.uiAnimPlaying = true;
										mdl.pmx.SetAnimationPlaying(true);
									}
								}
							}
							ImGui::Checkbox("Play##PMX", &mdl.uiAnimPlaying);
							mdl.pmx.SetAnimationPlaying(mdl.uiAnimPlaying);
							// PMX???? ???? ???(?ε?? VMD)???? ???
							double cur = mdl.pmx.GetAnimationTimeSeconds();
							double dur = mdl.pmx.GetClipDurationSec();
							float curF = (float)cur, durF = (float)dur;
							if (durF > 0.0f)
							{
								if (ImGui::SliderFloat("Time (s)##PMX", &curF, 0.0f, durF)) mdl.pmx.SetAnimationTimeSeconds((double)curF);
							}
						}
				if (ImGui::Button("Remove"))
				{
					// release resources
					m_->m_Models.erase(m_->m_Models.begin() + (ptrdiff_t)i);
					ImGui::PopID();
					break;
				}
				ImGui::PopID();
			}
		}
        ImGui::End();
    }

		// 현재 카메라 포워드 기준 스카이박스 면 이미지를 표시 (스카이박스 On일 때만)
	if (m_->m_SkyBoxChoice != App::Impl::SkyBoxChoice::Off)
	{
		int face = 0;
		using namespace DirectX;
		XMFLOAT3 fwd = m_Camera.GetForward();
		XMVECTOR f = XMLoadFloat3(&fwd);
		XMVECTOR fn = XMVector3Normalize(f);
		XMFLOAT3 v; XMStoreFloat3(&v, fn);
		float ax = fabsf(v.x), ay = fabsf(v.y), az = fabsf(v.z);
		if (ax >= ay && ax >= az) face = (v.x >= 0.0f) ? 0 : 1; // +X / -X
		else if (ay >= ax && ay >= az) face = (v.y >= 0.0f) ? 2 : 3; // +Y / -Y
		else face =  (v.z >= 0.0f) ? 4 : 5;     // +Z / -Z

		ID3D11ShaderResourceView* faceSRV = (face >= 0 && face < 6) ? m_->m_pSkyFaceSRV[face] : nullptr;
		if (faceSRV)
		{
			ImGui::SetNextWindowPos(ImVec2(810, 210), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(m_->m_HanakoDrawSize.x + 50, m_->m_HanakoDrawSize.y + 80), ImGuiCond_Once);
			if (ImGui::Begin("Skybox Face"))
			{
				ImGui::BeginChild("SkyFaceView", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
				const ImVec2 tex = (m_->m_HanakoDrawSize.x > 0 && m_->m_HanakoDrawSize.y > 0) ? m_->m_HanakoDrawSize : m_->m_SkyFaceSize;
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
	m_->m_SystemInfo.RenderUI();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	m_->m_pSwapChain->Present(0, 0);
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
		D3D11_SDK_VERSION, &swapDesc, &m_->m_pSwapChain, &m_->m_pDevice, NULL, &m_->m_pDeviceContext));

	/*
	* @brief  스왑체인 백버퍼로 RTV를 만들고 OM 스테이지에 바인딩한다
	* @details
	*   - GetBuffer(0): 백버퍼(ID3D11Texture2D)를 획득
	*   - CreateRenderTargetView: 백버퍼 기반 RTV 생성(리소스 내부 참조 증가)
	*   - 로컬 텍스처 포인터는 Release로 정리 (RTV가 수명 관리)
	*   - OMSetRenderTargets: 생성한 RTV를 렌더 타겟을 최종 출력 파이프라인에 바인딩
	*/
	ID3D11Texture2D* pBackBufferTexture = nullptr;
	HR_T(m_->m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));
	HR_T(m_->m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_->m_pRenderTargetView));
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
	HR_T(m_->m_pDevice->CreateTexture2D(&dsDesc, nullptr, &pDepthStencil));
	HR_T(m_->m_pDevice->CreateDepthStencilView(pDepthStencil, nullptr, &m_->m_pDepthStencilView));
	SAFE_RELEASE(pDepthStencil);

	// DepthStencilState 생성 및 설정
	D3D11_DEPTH_STENCIL_DESC dssDesc = {};
	dssDesc.DepthEnable = TRUE;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS;
	dssDesc.StencilEnable = FALSE;
	HR_T(m_->m_pDevice->CreateDepthStencilState(&dssDesc, &m_->m_pDepthStencilState));
	m_->m_pDeviceContext->OMSetDepthStencilState(m_->m_pDepthStencilState, 0);

	// 렌더 타겟/DSV 바인딩
	m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pRenderTargetView, m_->m_pDepthStencilView);

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
	m_->m_pDeviceContext->RSSetViewports(1, &viewport);

	// 컬링 설정 (양면 렌더링/후면 컬링)
	CD3D11_RASTERIZER_DESC rasterizerDesc(CD3D11_DEFAULT{});
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_NONE;
	rasterizerDesc.FrontCounterClockwise = false;
	HR_T(m_->m_pDevice->CreateRasterizerState(&rasterizerDesc, &m_->RSNoCull));
	rasterizerDesc.FillMode = D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_BACK;
	rasterizerDesc.FrontCounterClockwise = true;
	HR_T(m_->m_pDevice->CreateRasterizerState(&rasterizerDesc, &m_->RSCullClockWise));

	// 알파 블렌딩 상태 생성 SrcAlpha/InvSrcAlpha
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
		HR_T(m_->m_pDevice->CreateBlendState(&bd, &m_->m_pAlphaBlendState));
	}
 
	return true;
}

void App::UninitD3D()
{
	SAFE_RELEASE(m_->m_pDepthStencilState);
	SAFE_RELEASE(m_->m_pDepthStencilView);
	SAFE_RELEASE(m_->m_pRenderTargetView);
	SAFE_RELEASE(m_->m_pAlphaBlendState);
	SAFE_RELEASE(m_->m_pDeviceContext);
	SAFE_RELEASE(m_->m_pSwapChain);
	SAFE_RELEASE(m_->m_pDevice);
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
	m_->m_VertextBufferStride = sizeof(VertexLightTex);
	m_->m_VertextBufferOffset = 0;
	// ***********************************************************************************************
    // 작은 큐브 데이터 설정 (TBN 정점)
    StaticMeshData cube = StaticMesh::CreateBox(XMFLOAT4(1,1,1,1));
    m_->m_VertextBufferStride = sizeof(VertexTBN);
    StaticMesh::AssignMemory(m_->m_pDevice, m_->m_pVertexBuffer, cube);
    StaticMesh::AssignIndexMemory(m_->m_pDevice, m_->m_pIndexBuffer, cube, m_->m_nIndices);

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
	HR_T(m_->m_pDevice->CreateBuffer(&cbd, nullptr, &m_->m_pConstantBuffer));

		// ***********************************************************************************************
	// 스카이 박스 큐브 설정
    HR_T(CreateDDSTextureFromFile(m_->m_pDevice, L"..\\Resource\\Skybox\\Hanako.dds", nullptr, &m_->m_pSkyHanakoSRV));
    HR_T(CreateDDSTextureFromFile(m_->m_pDevice, L"..\\Resource\\Skybox\\cubemap.dds", nullptr, &m_->m_pSkyCubeMapSRV));
    m_->m_pTextureSRV = m_->m_pSkyCubeMapSRV;
	
	// 샘플러 생성
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HR_T(m_->m_pDevice->CreateSamplerState(&sampDesc, &m_->m_pSamplerState));

	// ***********************************************************************************************
	// 카메라 설정
	// 카메라(View/Proj)로 상수 버퍼를 준비합니다
	m_->m_baseProjection.world = XMMatrixIdentity();
	// 카메라 초기 프러스텀 값들 설정
	m_Camera.SetFrustum(XMConvertToRadians(90.0f), AspectRatio(), 1.0f, 1000.0f);
	m_->m_baseProjection.view = XMMatrixTranspose(m_Camera.GetViewMatrixXM());
	m_->m_baseProjection.proj = XMMatrixTranspose(m_Camera.GetProjMatrixXM());
	m_->m_baseProjection.worldInvTranspose = XMMatrixInverse(nullptr, XMMatrixTranspose(m_->m_baseProjection.world));
	// DirectionalLight 초기값 필드 대입
	m_->m_baseProjection.dirLight.ambient = DirectX::XMFLOAT4(0,0,0,1);
	m_->m_baseProjection.dirLight.diffuse = DirectX::XMFLOAT4(1,1,1,1);
	m_->m_baseProjection.dirLight.specular = DirectX::XMFLOAT4(1,1,1,1);
	m_->m_baseProjection.dirLight.direction = DirectX::XMFLOAT3(0,-1,1);
	m_->m_baseProjection.dirLight.pad = 0.0f;
	m_->m_baseProjection.eyePos = m_Camera.GetPosition();
	m_->m_baseProjection.pad = 0.0f;

	// ***********************************************************************************************
	// 유틸 초기화. 라인 렌더러, 스카이박스, 디버그 박스
	if (!m_->m_LineRenderer) m_->m_LineRenderer = new LineRenderer();
	m_->m_LineRenderer->Initialize(m_->m_pDevice);

    // Skybox: 기존 Hanako를 기본으로 초기화 (선호 DDS를 설정)
	if (!m_->m_Skybox) m_->m_Skybox = new Skybox();
	// Skybox는 이미 CreateDDSTextureFromFile로 SRV가 생성되어 있으므로, 여기선 현재 선택된 SRV를 사용하도록 Initialize는 경로 기반 대신 스킵할 수 있습니다.
	// 간편화를 위해 cubemap.dds로 초기화
	m_->m_Skybox->Initialize(m_->m_pDevice, m_->m_CurrentSkyboxPath, m_->m_pSkyBoxVertexShader, m_->m_pSkyBoxPixelShader, m_->m_pSkyBoxInputLayout, m_->m_pConstantBuffer);

	// Debug box buffers for light position marker
	StaticMesh::CreateDebugBoxBuffersLightTex(m_->m_pDevice, XMFLOAT4(1,1,1,1), 0.2f, &m_->m_pDebugBoxVB, &m_->m_pDebugBoxIB, &m_->m_DebugBoxIndexCount);

	return true;
}

void App::UninitScene()
{
	SAFE_RELEASE(m_->m_pVertexBuffer);
	SAFE_RELEASE(m_->m_pIndexBuffer);
	SAFE_RELEASE(m_->m_pInputLayout);
	SAFE_RELEASE(m_->m_pVertexShader);
	SAFE_RELEASE(m_->m_pPixelShader);
	SAFE_RELEASE(m_->m_pConstantBuffer);
	SAFE_RELEASE(m_->m_pSamplerState);

	SAFE_RELEASE(m_->m_pSkyBoxInputLayout);
	SAFE_RELEASE(m_->m_pSkyBoxVertexShader);
	SAFE_RELEASE(m_->m_pSkyBoxPixelShader);

	SAFE_RELEASE(m_->m_pSkyHanakoSRV);
	SAFE_RELEASE(m_->m_pSkyCubeMapSRV);

	for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_->m_pCubeTextureSRVs[i]);
	    for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_->m_pSkyFaceSRV[i]);

	SAFE_RELEASE(m_->m_pFallbackWhite);
	SAFE_RELEASE(m_->m_pFallbackNormal);
	SAFE_RELEASE(m_->m_pFallbackBlack);

	SAFE_RELEASE(m_->m_pDebugBoxVB);
	SAFE_RELEASE(m_->m_pDebugBoxIB);
	if (m_->m_LineRenderer) { m_->m_LineRenderer->Release(); delete m_->m_LineRenderer; m_->m_LineRenderer = nullptr; }
	if (m_->m_Skybox) { m_->m_Skybox->Release(); delete m_->m_Skybox; m_->m_Skybox = nullptr; }

    // 모델 리소스 해제
	UnloadModel();
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
		HR_T(CreateWICTextureFromFile(m_->m_pDevice, facePaths[i], res.GetAddressOf(), &m_->m_pCubeTextureSRVs[i]));
		res.Reset();
		CreateWICTextureFromFile(m_->m_pDevice, normalPaths[i], res.GetAddressOf(), &m_->m_pNormalSRVs[i]);
		res.Reset();
		CreateWICTextureFromFile(m_->m_pDevice, specularPaths[i], res.GetAddressOf(), &m_->m_pSpecularSRVs[i]);
	}
	return true;
}

bool App::InitImGui()
{
	// ImGui 초기화
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	// 한글/일본어 표시를 위한 폰트 설정
	{
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontDefault();
		ImFontConfig cfg{};
		cfg.MergeMode = true;
		cfg.PixelSnapH = true;
		cfg.OversampleH = 2;
		cfg.OversampleV = 2;
		const ImWchar* rangeKR = io.Fonts->GetGlyphRangesKorean();
		io.Fonts->AddFontFromFileTTF("..\\Resource\\Font\\NotoSansKR-Regular.ttf", 17.0f, &cfg, rangeKR);
		const ImWchar* rangeJP = io.Fonts->GetGlyphRangesJapanese();
		io.Fonts->AddFontFromFileTTF("..\\Resource\\Font\\meiryo.ttc", 17.0f, &cfg, rangeJP);
	}
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_->m_pDevice, m_->m_pDeviceContext);
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
    HR_T(CompileShaderFromFile(L"22_BasicVS.hlsl", "main", "vs_5_0", &vertexShaderBuffer));
	HR_T(m_->m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_->m_pInputLayout));

	HR_T(m_->m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_->m_pVertexShader));
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
    HR_T(CompileShaderFromFile(L"22_BasicVS.hlsl", "VSNoTBN", "vs_5_0", &vsNoTBN));
	HR_T(m_->m_pDevice->CreateInputLayout(layoutNoTBN, ARRAYSIZE(layoutNoTBN), vsNoTBN->GetBufferPointer(), vsNoTBN->GetBufferSize(), &m_->m_pInputLayoutNoTBN));
	HR_T(m_->m_pDevice->CreateVertexShader(vsNoTBN->GetBufferPointer(), vsNoTBN->GetBufferSize(), nullptr, &m_->m_pVertexShaderNoTBN));
	SAFE_RELEASE(vsNoTBN);

	// Line VS -------------------------------------------
	D3D11_INPUT_ELEMENT_DESC lineLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	ID3D10Blob* vsLine = nullptr;
    HR_T(CompileShaderFromFile(L"22_BasicVS.hlsl", "VSLine", "vs_5_0", &vsLine));

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
        HR_T(CompileShaderFromFile(L"22_BasicVS.hlsl", "VSSkinned", "vs_5_0", &vsSkinned));
        HR_T(m_->m_pDevice->CreateInputLayout(layoutSkinned, ARRAYSIZE(layoutSkinned), vsSkinned->GetBufferPointer(), vsSkinned->GetBufferSize(), &m_->m_pInputLayoutSkinned));
        HR_T(m_->m_pDevice->CreateVertexShader(vsSkinned->GetBufferPointer(), vsSkinned->GetBufferSize(), nullptr, &m_->m_pVertexShaderSkinned));
        SAFE_RELEASE(vsSkinned);
    }
	HR_T(m_->m_pDevice->CreateInputLayout(lineLayout, ARRAYSIZE(lineLayout), vsLine->GetBufferPointer(), vsLine->GetBufferSize(), &m_->m_pLineInputLayout));
	HR_T(m_->m_pDevice->CreateVertexShader(vsLine->GetBufferPointer(), vsLine->GetBufferSize(), nullptr, &m_->m_pLineVS));
	SAFE_RELEASE(vsLine);


	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
    HR_T(CompileShaderFromFile(L"22_BasicPS.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_->m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_->m_pPixelShader));
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
    HR_T(CompileShaderFromFile(L"22_SkyBoxVS.hlsl", "VS", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_->m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_->m_pSkyBoxInputLayout));

	HR_T(m_->m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_->m_pSkyBoxVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// 컴파일 버퍼 해제

	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
    HR_T(CompileShaderFromFile(L"22_SkyBoxPS.hlsl", "PS", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_->m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_->m_pSkyBoxPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음
	return true;
}

// ------------------------- Model Loader (FBX/OBJ/PMX via Assimp) -------------------------
bool App::LoadModelFromFile(const std::wstring& pathW)
{
    // 새 모델델 추가

    // 폴백 텍스처 생성(최초 1회)
	UINT fallBackColor = 0x000000FF;
    if (!m_->m_pFallbackWhite) fallBackColor = 0xFFFFFFFF;
    if (!m_->m_pFallbackBlack) fallBackColor = 0x000000FF; // a=1
    if (!m_->m_pFallbackNormal) fallBackColor = 0x8080FFFF; // (0.5,0.5,1,1) in RGBA8

	D3D11_TEXTURE2D_DESC td{}; td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = &fallBackColor; sd.SysMemPitch = sizeof(UINT);
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex; HR_T(m_->m_pDevice->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
	D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MipLevels = 1; srvd.Texture2D.MostDetailedMip = 0;
	HR_T(m_->m_pDevice->CreateShaderResourceView(tex.Get(), &srvd, &m_->m_pFallbackNormal));

	// 받은 경로에서 이름, 확장자 추출
	std::wstring ext{L""}, fileName{L""};
    if (!pathW.empty())
    {
        size_t dot = pathW.find_last_of(L'.');
        if (dot != std::wstring::npos) 
		{ 
			ext = pathW.substr(dot); std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
			size_t sep = pathW.find_last_of(L"\\/");
			if (sep != std::wstring::npos) fileName = pathW.substr(sep + 1, dot - sep - 1);
			else fileName = pathW.substr(0, dot);
		}
    }

    bool ok = false;
	auto entry = std::make_unique<ModelEntry>();
	entry->modelName = fileName;

    if (ext == L".fbx")
    {
		entry->source = ModelSource::FBX;
        if (ok = entry->fbx.Load(m_->m_pDevice, pathW))
        {
            entry->stride = entry->fbx.GetVertexStride();
            entry->vb = entry->fbx.GetVertexBuffer(); if (entry->vb) entry->vb->AddRef();
            entry->ib = entry->fbx.GetIndexBuffer();  if (entry->ib) entry->ib->AddRef();
            entry->indexCount = entry->fbx.GetIndexCount();
            entry->subsets.clear();
            for (auto& s : entry->fbx.GetSubsets()) entry->subsets.push_back({ s.startIndex, s.indexCount, s.materialIndex });
            entry->materialSRVs = entry->fbx.GetMaterialSRVs();
            m_->m_Models.push_back(std::move(entry));
			m_->m_RenderMode = RenderMode::Model;
        }
    }
    else if (ext == L".obj")
    {
		entry->source = ModelSource::OBJ;
        if (ok = entry->obj.Load(m_->m_pDevice, pathW))
        {
            entry->stride = entry->obj.GetVertexStride();
            entry->vb = entry->obj.GetVertexBuffer(); if (entry->vb) entry->vb->AddRef();
            entry->ib = entry->obj.GetIndexBuffer();  if (entry->ib) entry->ib->AddRef();
            entry->indexCount = entry->obj.GetIndexCount();
            entry->subsets.clear();
            for (auto& s : entry->obj.GetSubsets()) entry->subsets.push_back({ s.startIndex, s.indexCount, s.materialIndex });
            entry->materialSRVs = entry->obj.GetMaterialSRVs();
            m_->m_Models.push_back(std::move(entry));
			m_->m_RenderMode = RenderMode::Model;
        }
    }
    else if (ext == L".pmx")
    {
		entry->source = ModelSource::PMX;
        if (ok = entry->pmx.Load(m_->m_pDevice, pathW))
        {
            entry->stride = entry->pmx.GetVertexStride();
            entry->vb = entry->pmx.GetVertexBuffer(); if (entry->vb) entry->vb->AddRef();
            entry->ib = entry->pmx.GetIndexBuffer();  if (entry->ib) entry->ib->AddRef();
            entry->indexCount = entry->pmx.GetIndexCount();
            entry->subsets.clear();
            for (auto& s : entry->pmx.GetSubsets()) entry->subsets.push_back({ s.startIndex, s.indexCount, s.materialIndex });
            entry->materialSRVs = entry->pmx.GetMaterialSRVs();
            m_->m_Models.push_back(std::move(entry));
			m_->m_RenderMode = RenderMode::Model;
        }
    }

    return ok;
}

void App::UnloadModel()
{
    // 로드된 모델이 없으면 아무 것도 하지 않는다
    if (m_->m_Models.empty())
    {
        m_->m_RenderMode = RenderMode::None;
        return;
    }

    m_->m_Models.clear();
    m_->m_RenderMode = RenderMode::None;
}
