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
#include <directxtk/WICTextureLoader.h>
#include <thread>
#include <commdlg.h>
#include <filesystem>
#include <Rendering/D3D11/CubismRenderer_D3D11.hpp>
#include <CubismFramework.hpp>
#include <ICubismAllocator.hpp>
#include <CubismModelSettingJson.hpp>
#include <Id/CubismIdManager.hpp>
#include <Motion/CubismMotionManager.hpp>
#include <Motion/CubismMotion.hpp>
#include <Motion/CubismMotionQueueEntry.hpp>
#include <Physics/CubismPhysics.hpp>
#include <Model/CubismUserModel.hpp>
#include <Model/CubismModel.hpp>
#include <malloc.h>
#include <cstdio>
#include <string>

using namespace Live2D::Cubism::Framework;

// CubismFramework 파일 로더/해제 함수
static csmByte* CF_LoadFile(std::string path, csmSizeInt* outSize)
{
    if (outSize) *outSize = 0;
    if (path.empty()) return nullptr;
    FILE* f = nullptr; fopen_s(&f, path.c_str(), "rb");
    if (!f) return nullptr;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return nullptr; }
    csmByte* data = (csmByte*)malloc((size_t)sz);
    if (!data) { fclose(f); return nullptr; }
    size_t rd = fread(data, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz) { free(data); return nullptr; }
    if (outSize) *outSize = (csmSizeInt)sz;
    return data;
}

static void CF_ReleaseBytes(csmByte* bytes)
{
    if (bytes) free(bytes);
}

// 최소 Allocator
class MinimalAllocator : public ICubismAllocator
{
public:
	void* Allocate(const csmSizeType size) override { return malloc(size); }
	void Deallocate(void* memory) override { free(memory); }
	void* AllocateAligned(const csmSizeType size, const csmUint32 alignment) override {
		return _aligned_malloc(size, alignment);
	}
	void DeallocateAligned(void* alignedMemory) override { _aligned_free(alignedMemory); }
};

static MinimalAllocator g_alloc;

// 최소 UserModel: 모델 로드/모션/드로우 제공
class MinimalUserModel : public CubismUserModel
{
public:
	std::string baseDir;
	std::unique_ptr<CubismModelSettingJson> setting;
	std::vector<std::string> motionGroups;
	std::map<std::string, ACubismMotion*> motions; // 로드된 모션 캐시
	float userTimeSeconds = 0.0f;
    // Live2D 렌더러는 BindTexture 시 SRV의 참조를 증가시키지 않으므로, 수명 유지를 위해 여기서 소유한다
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textureSRVs;

	~MinimalUserModel() override
	{
		for (auto& kv : motions) { if (kv.second) ACubismMotion::Delete(kv.second); }
		motions.clear();
	}

	bool LoadFromModel3(const std::wstring& model3Path)
	{
		std::string spath(model3Path.begin(), model3Path.end());
		auto pos = spath.find_last_of("/\\");
		baseDir = (pos==std::string::npos) ? std::string() : spath.substr(0,pos+1);
		// 읽기
		FILE* fp = nullptr; fopen_s(&fp, spath.c_str(), "rb"); if(!fp) return false;
		fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
		std::vector<uint8_t> buf(sz); fread(buf.data(),1,sz,fp); fclose(fp);
		setting.reset(new CubismModelSettingJson(buf.data(), (csmSizeInt)buf.size()));
		// moc3
		{
			std::string moc = baseDir + setting->GetModelFileName();
			FILE* fpm=nullptr; fopen_s(&fpm, moc.c_str(), "rb"); if(!fpm) return false;
			fseek(fpm,0,SEEK_END); long msz = ftell(fpm); fseek(fpm,0,SEEK_SET);
			std::vector<uint8_t> mb(msz); fread(mb.data(),1,msz,fpm); fclose(fpm);
			LoadModel(mb.data(), (csmSizeInt)mb.size());
		}
		// physics(optional)
		if (const char* phys = setting->GetPhysicsFileName()) if (*phys){
			std::string p = baseDir + phys; FILE* f=nullptr; fopen_s(&f,p.c_str(),"rb"); if(f){ fseek(f,0,SEEK_END); long s=ftell(f); fseek(f,0,SEEK_SET); std::vector<uint8_t> b(s); fread(b.data(),1,s,f); fclose(f); LoadPhysics(b.data(),(csmSizeInt)b.size()); }}
		// pose(optional)
		if (const char* pose = setting->GetPoseFileName()) if (*pose){
			std::string p = baseDir + pose; FILE* f=nullptr; fopen_s(&f,p.c_str(),"rb"); if(f){ fseek(f,0,SEEK_END); long s=ftell(f); fseek(f,0,SEEK_SET); std::vector<uint8_t> b(s); fread(b.data(),1,s,f); fclose(f); LoadPose(b.data(),(csmSizeInt)b.size()); }}
		// motions group 이름 수집
		int gcount = setting->GetMotionGroupCount();
		motionGroups.clear();
		for(int gi=0; gi<gcount; ++gi){ motionGroups.push_back(setting->GetMotionGroupName(gi)); }
		CreateRenderer();
		return true;
	}

	bool LoadAndBindTextures(ID3D11Device* dev)
	{
		auto* r = GetRenderer<Rendering::CubismRenderer_D3D11>();
		if(!r) return false;
		int bound = 0;
        textureSRVs.clear();
        if (setting) { textureSRVs.reserve(setting->GetTextureCount()); }
		for (int i=0;i<setting->GetTextureCount();++i){
			const char* tf = setting->GetTextureFileName(i); if(!tf||!*tf) continue;
			std::wstring w = std::wstring(baseDir.begin(), baseDir.end()) + std::wstring(tf, tf+strlen(tf));
			Microsoft::WRL::ComPtr<ID3D11Resource> res;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
			if (SUCCEEDED(CreateWICTextureFromFile(dev, w.c_str(), res.GetAddressOf(), srv.GetAddressOf())))
			{
				r->BindTexture(i, srv.Get());
                // 수명 유지: 렌더 종료까지 해제되지 않도록 보관
                textureSRVs.push_back(srv);
				++bound;
			}
		}
		return bound>0;
	}

	// 간단 업데이트: 모션만 갱신하고 모델 업데이트
	void ModelParamUpdate()
	{
		float dt = ImGui::GetIO().DeltaTime;
		if (dt <= 0.0f) dt = 1.0f/60.0f;
		userTimeSeconds += dt;
		if (!GetModel()) return;
		GetModel()->LoadParameters();
		if (_motionManager)
		{
			_motionManager->UpdateMotion(GetModel(), dt);
		}
		GetModel()->SaveParameters();
		GetModel()->Update();
	}

	// 모션 시작
	Csm::CubismMotionQueueEntryHandle StartMotion(const char* group, int no, int priority)
	{
		if (!setting) return Csm::InvalidMotionQueueEntryHandleValue;
		int count = setting->GetMotionCount(group);
		if (no < 0 || no >= count) return Csm::InvalidMotionQueueEntryHandleValue;
		if (_motionManager)
		{
			if (priority == 3) { _motionManager->SetReservePriority(priority); }
			else if (!_motionManager->ReserveMotion(priority)) { return Csm::InvalidMotionQueueEntryHandleValue; }
		}
		std::string key = std::string(group) + "_" + std::to_string(no);
		ACubismMotion* motion = nullptr;
		auto it = motions.find(key);
		if (it != motions.end()) { motion = it->second; }
		else
		{
			const char* file = setting->GetMotionFileName(group, no);
			std::string path = baseDir + (file ? file : "");
			FILE* f=nullptr; fopen_s(&f, path.c_str(), "rb"); if (!f) return Csm::InvalidMotionQueueEntryHandleValue;
			fseek(f,0,SEEK_END); long sz = ftell(f); fseek(f,0,SEEK_SET);
			std::vector<uint8_t> buf(sz); fread(buf.data(),1,sz,f); fclose(f);
			motion = LoadMotion(
				buf.data(),
				(csmSizeInt)buf.size(),
				nullptr,                 // name
				nullptr,                 // FinishedMotionCallback
				nullptr,                 // BeganMotionCallback
				setting.get(),           // ICubismModelSetting*
				group,                   // group
				no,                      // index
				false                    // shouldCheckMotionConsistency
			);
			if (motion) motions[key] = motion;
		}
		if (!_motionManager || !motion) return Csm::InvalidMotionQueueEntryHandleValue;
		return _motionManager->StartMotionPriority(motion, false, priority);
	}

	void DrawModelD3D11(ID3D11Device* dev, ID3D11DeviceContext* ctx, int vpw, int vph)
	{
		// 매 프레임 간단 파라미터 업데이트
		ModelParamUpdate();
		Rendering::CubismRenderer_D3D11::StartFrame(dev, ctx, (csmUint32)vpw, (csmUint32)vph);
		// StartFrame이 s_context/viewport를 설정한 뒤에 기본 렌더 상태를 세팅
		Rendering::CubismRenderer_D3D11::SetDefaultRenderState();
		CubismMatrix44 proj; proj.LoadIdentity();
		if (GetModel()->GetCanvasWidth() > 1.0f && vpw < vph){ GetModelMatrix()->SetWidth(2.0f); proj.Scale(1.0f, (float)vpw/(float)vph);} else { proj.Scale((float)vph/(float)vpw, 1.0f);} 
		GetRenderer<Rendering::CubismRenderer_D3D11>()->SetMvpMatrix(&proj);
		// Anisotropy 최소 1로 설정 (0이면 샘플러 생성 시 유효하지 않아 예외 발생)
		GetRenderer<Rendering::CubismRenderer_D3D11>()->SetAnisotropy(1.0f);
		GetRenderer<Rendering::CubismRenderer_D3D11>()->DrawModel();
		Rendering::CubismRenderer_D3D11::EndFrame(dev);
	}
};

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

bool App::OnInitialize()
{
	if(!InitD3D()) return false;

	if (!InitEffect()) return false;

	if(!InitScene()) return false;

	// ImGui 초기화
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);

	// Live2D Framework 초기화 (Core 링크 필요)
	{
		CubismFramework::Option opt{};
		opt.LogFunction = nullptr;
		opt.LoggingLevel = CubismFramework::Option::LogLevel_Verbose;
		opt.LoadFileFunction = CF_LoadFile;
		opt.ReleaseBytesFunction = CF_ReleaseBytes;
		CubismFramework::StartUp(&g_alloc, &opt);
		CubismFramework::Initialize();
		Rendering::CubismRenderer_D3D11::InitializeConstantSettings(1, m_pDevice);
		// D3D11 셰이더 생성 (필수)
		Rendering::CubismRenderer_D3D11::GenerateShader(m_pDevice);
		m_L2DReady = true; m_L2DStatus = "Live2D 초기화 완료";
	}

	// 시스템 정보 수집 (GPU)
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
						m_VideoMemoryTotal = memInfo.Budget; // 사용 가능 예산을 총량 근사로 사용
					}
				}
			}
		}
	}

	// 시스템 정보 수집 (CPU)
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

	// 시스템 메모리 총량/가용량 초기화
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
	// ImGui 종료
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	// Live2D 종료
	delete m_L2D; m_L2D=nullptr; m_L2DLoaded=false;
	Rendering::CubismRenderer_D3D11::DeleteRenderStateManager();
	CubismFramework::Dispose();

	UninitD3D();
}

void App::OnUpdate(const float& dt)
{
	static float t0 = 0.0f, t1 = 0.0f, t2 = 0.0f;
	t0 += 0.6f * dt;   // 부모(루트) Yaw 속도
	t1 += 1.0f * dt;   // 두번째 메쉬(자식1) Yaw 속도 (루트와 다르게)
	t2 += 1.2f * dt;   // 세번째 메쉬(자식2) 공전 속도

	// 로컬 변환 정의 (간단 Scene Graph)
	XMMATRIX local0 = XMMatrixRotationY(t0) * XMMatrixTranslation(m_RootPos.x, m_RootPos.y, m_RootPos.z); // 루트
	XMMATRIX local1 = XMMatrixRotationY(t1) * XMMatrixTranslation(m_Child1Offset.x, m_Child1Offset.y, m_Child1Offset.z);  // 자식1 (루트 기준 상대)
	XMMATRIX local2 = XMMatrixRotationY(t2) * XMMatrixTranslation(m_Child2Offset.x, m_Child2Offset.y, m_Child2Offset.z);  // 자식2: 자식1을 중심으로 공전

	// 부모-자식 계층 world 계산: world = local * parentWorld
	XMMATRIX world0 = local0;                 // 루트
	XMMATRIX world1 = local1 * world0;        // 자식1
	XMMATRIX world2 = local2 * world1;        // 자식2 (자식1 주변 공전)

	// View/Proj도 UI값 반영 (매 프레임)
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

	// FPS 1초 업데이트
	m_FpsTimer += dt;
	if (m_FpsTimer >= 1.0f)
	{
		m_LastFps = 1.0f / dt; // 간단히 최근 프레임의 역수 사용
		m_FpsTimer = 0.0f;
	}

	// 시스템 메모리 가용량 갱신
	{
		MEMORYSTATUSEX ms{ sizeof(MEMORYSTATUSEX) };
		if (GlobalMemoryStatusEx(&ms))
		{
			m_RamTotal = ms.ullTotalPhys;
			m_RamAvail = ms.ullAvailPhys;
		}
	}

	// VRAM 사용량 갱신 (가능한 경우)
	if (m_Adapter3)
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
		if (SUCCEEDED(m_Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)))
		{
			// Budget은 사용 가능 한도, CurrentUsage는 현재 사용량
			m_VideoMemoryTotal = memInfo.Budget;
			// 사용량은 memInfo.CurrentUsage를 직접 사용, 표시 시 변환
		}
	}
}

// Render() 함수에 중요한 부분이 다 들어있습니다. 여기를 보면 됩니다
void App::OnRender()
{
	float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	UINT stride = sizeof(VertexPosTex);	// 바이트 수
	UINT offset = 0;

	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 1 ~ 3 . IA 단계 설정
	// 정점을 어떻게 이어서 그릴 것인지를 선택하는 부분
	// 1. 버퍼를 잡아주기
	// 2. 입력 레이아웃을 잡아주기
	// 3. 인덱스 버퍼를 잡아주기
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// 4. Vertex Shader 설정
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	// 5. Pixel Shader 설정
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	// 6. Constant Buffer 설정
	// 7. 그리기
	for (auto m_CBuffer : m_CBuffers)
	{
		D3D11_MAPPED_SUBRESOURCE mapped{};
		HR_T(m_pDeviceContext->Map(m_pConstantBuffer, 0,
			D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy(mapped.pData, &m_CBuffer, sizeof(m_CBuffer));
		m_pDeviceContext->Unmap(m_pConstantBuffer, 0);

		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
		for (int face = 0; face < 6; ++face)
		{
			ID3D11ShaderResourceView* srv = m_pTextureSRVs[face];
			m_pDeviceContext->PSSetShaderResources(0, 1, &srv);
			m_pDeviceContext->DrawIndexed(6, face * 6, 0);
		}
	}

	// ImGui 프레임 및 UI 렌더링
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

		// Live2D: model3.json 선택 및 상태 표시
		ImGui::Separator();
		ImGui::TextUnformatted("Live2D");
		if (ImGui::Button("Open model3.json"))
		{
			wchar_t file[1024] = {0};
			OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = m_hWnd;
			ofn.lpstrFilter = L"Live2D Model (*.model3.json)\0*.model3.json\0All Files\0*.*\0\0";
			ofn.lpstrFile = file; ofn.nMaxFile = 1024; ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
			if (GetOpenFileNameW(&ofn))
			{
				m_L2DModelJsonPath = file; m_L2DRequested = true; m_L2DStatus = "선택됨";
				for (auto* p : m_L2DTexSRVs) { SAFE_RELEASE(p); }
				m_L2DTexSRVs.clear(); m_L2DTexSizes.clear();
				// 실제 모델 로드
				if (m_L2DReady)
				{
					delete m_L2D; m_L2D=nullptr; m_L2DLoaded=false; m_L2DMotionGroups.clear();
					m_L2D = new MinimalUserModel();
					if (m_L2D->LoadFromModel3(m_L2DModelJsonPath) && m_L2D->LoadAndBindTextures(m_pDevice))
					{
						m_L2DLoaded = true; m_L2DStatus = "모델 로드 완료";
						m_L2DMotionGroups = m_L2D->motionGroups;
						// 마스크 렌더 타깃 보장: 크기/정밀도 설정
						if (auto* r = m_L2D->GetRenderer<Rendering::CubismRenderer_D3D11>())
						{
							r->UseHighPrecisionMask(false);
							r->SetClippingMaskBufferSize(256.0f, 256.0f);
							// 마스크용 오프스크린 버퍼 미리 생성 (백버퍼셋=0)
							int rtc = r->GetRenderTextureCount();
							for (int i = 0; i < rtc; ++i)
							{
								r->GetMaskBuffer(0, i)->CreateOffscreenSurface(m_pDevice, 256, 256);
							}
						}
					}
					else { m_L2DStatus = "모델 로드 실패"; }
				}
			}
		}
		if (!m_L2DModelJsonPath.empty()) { ImGui::Text("Model: %ls", m_L2DModelJsonPath.c_str()); }
		ImGui::Text("Status: %s", m_L2DStatus.empty() ? "-" : m_L2DStatus.c_str());
	}
	ImGui::End();

	// Live2D: 렌더 및 정보 패널
	if (m_L2DLoaded && m_L2D && m_ShowL2DInfo)
	{
		ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_Once);
		if (ImGui::Begin("Live2D Model Info", &m_ShowL2DInfo))
		{
			CubismModel* model = m_L2D->GetModel();
			int parts = model->GetPartCount();
			int params = model->GetParameterCount();
			int draws = model->GetDrawableCount();
			ImGui::Text("Parts: %d, Parameters: %d, Drawables: %d", parts, params, draws);
			ImGui::Separator();
			// 모션 그룹/아이템
			if (!m_L2DMotionGroups.empty())
			{
				ImGui::Text("Motion Groups: %d", (int)m_L2DMotionGroups.size());
				for (size_t i=0;i<m_L2DMotionGroups.size();++i)
				{
					ImGui::PushID((int)i);
					ImGui::TextUnformatted(m_L2DMotionGroups[i].c_str());
					int cnt = m_L2D->setting->GetMotionCount(m_L2DMotionGroups[i].c_str());
					ImGui::SameLine(); ImGui::Text("(%d)", cnt);
					for (int mi=0; mi<cnt; ++mi)
					{
						ImGui::SameLine();
						if (ImGui::Button(("Play "+std::to_string(mi)).c_str()))
						{
							m_L2D->StartMotion(m_L2DMotionGroups[i].c_str(), mi, 2);
						}
					}
					ImGui::PopID();
				}
			}
			ImGui::Separator();
			if (ImGui::TreeNode("Parameters"))
			{
				for (int i=0;i<params;++i)
				{
					auto id = model->GetParameterId(i);
					float v = model->GetParameterValue(i);
					float mn = model->GetParameterMinimumValue(i);
					float mx = model->GetParameterMaximumValue(i);
					ImGui::PushID(i);
					ImGui::SliderFloat(id->GetString().GetRawString(), &v, mn, mx);
					model->SetParameterValue(i, v);
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Parts"))
			{
				for (int i=0;i<parts;++i)
				{
					auto id = model->GetPartId(i);
					float op = model->GetPartOpacity(i);
					ImGui::PushID(10000+i);
					ImGui::SliderFloat(id->GetString().GetRawString(), &op, 0.0f, 1.0f);
					model->SetPartOpacity(i, op);
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}

	// 기존 이미지/시스템 창 렌더 이후, Live2D 모델 그리기
	if (m_L2DLoaded && m_L2D)
	{
		m_L2D->ModelParamUpdate(); // 모션/블링크/물리 등 업데이트 포함
		m_L2D->DrawModelD3D11(m_pDevice, m_pDeviceContext, m_ClientWidth, m_ClientHeight);
	}

	// Live2D 텍스처 미리보기 창
	if (m_ShowL2DWindow && !m_L2DTexSRVs.empty())
	{
		ImGui::SetNextWindowSize(ImVec2(520, 560), ImGuiCond_Once);
		if (ImGui::Begin("Live2D Textures", &m_ShowL2DWindow))
		{
			for (size_t i = 0; i < m_L2DTexSRVs.size(); ++i)
			{
				ImGui::Separator();
				ImGui::Text("Tex %u (%.0fx%.0f)", (unsigned)i, m_L2DTexSizes[i].x, m_L2DTexSizes[i].y);
				ImVec2 draw = m_L2DTexSizes[i];
				float maxW = ImGui::GetContentRegionAvail().x;
				if (draw.x > maxW && draw.x > 0) { float s = maxW / draw.x; draw.x *= s; draw.y *= s; }
				ImGui::Image((ImTextureID)m_L2DTexSRVs[i], draw);
			}
		}
		ImGui::End();
	}

	// 좌하단: Cube Description
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

	// 우상단: 시스템 정보(FPS/GPU/CPU)
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
			// VRAM (Budget/Usage가 제공되는 경우)
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
	// 24개 정점 (각 면 4개) + 텍스처 좌표
	VertexPosTex vertices[] =
	{
		// 앞면 (z = -1)
		{ XMFLOAT3(-1,-1,-1), XMFLOAT2(0,1) },
		{ XMFLOAT3(-1, 1,-1), XMFLOAT2(0,0) },
		{ XMFLOAT3( 1, 1,-1), XMFLOAT2(1,0) },
		{ XMFLOAT3( 1,-1,-1), XMFLOAT2(1,1) },
		// 왼쪽 (x = -1)
		{ XMFLOAT3(-1,-1, 1), XMFLOAT2(0,1) },
		{ XMFLOAT3(-1, 1, 1), XMFLOAT2(0,0) },
		{ XMFLOAT3(-1, 1,-1), XMFLOAT2(1,0) },
		{ XMFLOAT3(-1,-1,-1), XMFLOAT2(1,1) },
		// 위 (y = 1)
		{ XMFLOAT3(-1, 1,-1), XMFLOAT2(0,1) },
		{ XMFLOAT3(-1, 1, 1), XMFLOAT2(0,0) },
		{ XMFLOAT3( 1, 1, 1), XMFLOAT2(1,0) },
		{ XMFLOAT3( 1, 1,-1), XMFLOAT2(1,1) },
		// 뒤 (z = 1)
		{ XMFLOAT3( 1,-1, 1), XMFLOAT2(0,1) },
		{ XMFLOAT3( 1, 1, 1), XMFLOAT2(0,0) },
		{ XMFLOAT3(-1, 1, 1), XMFLOAT2(1,0) },
		{ XMFLOAT3(-1,-1, 1), XMFLOAT2(1,1) },
		// 오른쪽 (x = 1)
		{ XMFLOAT3( 1,-1,-1), XMFLOAT2(0,1) },
		{ XMFLOAT3( 1, 1,-1), XMFLOAT2(0,0) },
		{ XMFLOAT3( 1, 1, 1), XMFLOAT2(1,0) },
		{ XMFLOAT3( 1,-1, 1), XMFLOAT2(1,1) },
		// 아래 (y = -1)
		{ XMFLOAT3(-1,-1, 1), XMFLOAT2(0,1) },
		{ XMFLOAT3(-1,-1,-1), XMFLOAT2(0,0) },
		{ XMFLOAT3( 1,-1,-1), XMFLOAT2(1,0) },
		{ XMFLOAT3( 1,-1, 1), XMFLOAT2(1,1) }
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
		// 앞면 (0~3)
		0,1,2, 2,3,0,
		// 왼쪽 (4~7)
		4,5,6, 6,7,4,
		// 위 (8~11)
		8,9,10, 10,11,8,
		// 뒤 (12~15)
		12,13,14, 14,15,12,
		// 오른쪽 (16~19)
		16,17,18, 18,19,16,
		// 아래 (20~23)
		20,21,22, 22,23,20
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

	// 공통 카메라(View/Proj)로 3개의 상수 버퍼 엔트리를 준비합니다
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

	// 텍스처 6장 로드 (png/jpg 허용)
	const wchar_t* facePaths[6] = {
		//L"front.png", L"left.png", L"top.png", L"back.png", L"right.png", L"bottom.png"
		L"../Resource/Yuuka.png", L"../Resource/Hanako.png", L"../Resource/Hanako.png", L"../Resource/Hanako.png", L"../Resource/Hanako.png", L"../Resource/Hanako.png"
	};
	for (int i = 0; i < 6; ++i)
	{
		Microsoft::WRL::ComPtr<ID3D11Resource> res;
		HR_T(CreateWICTextureFromFile(m_pDevice, facePaths[i], res.GetAddressOf(), &m_pTextureSRVs[i]));
	}
	
	// 샘플러 생성
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HR_T(m_pDevice->CreateSamplerState(&sampDesc, &m_pSamplerState));

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
	SAFE_RELEASE(m_pSamplerState);
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
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"TexVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
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
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"TexPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음
	return true;
}
