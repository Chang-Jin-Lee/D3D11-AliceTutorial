/*
* @brief : D3D11 + Live2D ������ ���� ������Ʈ
* @details :
* 
*   - Live2D Cubism(Core/Framework)�� Direct3D 11�� �����Ͽ�
*     model3.json�� ����, �ε�, �ؽ�ó ���ε�, ����ũ ó��, ��ο����
*     �� ����Ŭ�� ������ �����Դϴ�.
*
*   - ImGui UI�� ���� ������ �����մϴ�
*       1) model3.json ���� ���� �� �ε�
*       2) ��� �׷�/�ε��� ���� �� ���
*       3) �Ķ����/��Ʈ ���� �ǽð����� ����
*       4) �ý��� ����(FPS/GPU/CPU/RAM/VRAM) ǥ��
*
*   - ������ ���������� ���
*       1) D3D11 �ʱ�ȭ �� ���� Ÿ��/���̽��ٽ�/����Ʈ ����
*       2) Live2D Framework �ʱ�ȭ �� D3D11 ���̴� ����
*       3) �� ������ Live2D �� ������Ʈ/��ο�(����ũ ����)
*       4) ImGui UI ���� �� ����ü�� Present
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

/*
* @brief : CubismFramework ���� �δ�/���� �Լ� ��Ͽ� �ݹ�
* @details : Live2D SDK�� ���̴�/��� �� �ܺ� ������ ���� �� ����ϴ� IO �ݹ��� �����մϴ�.
*/
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

// �ּ� Allocator
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

/*
* @brief : Live2D �� �ε�/���/��ο츦 �����ϴ� �ּ� ����
* @details :
*   - model3.json �ε� �� Core/Framework ������ ���� �� Renderer ����/���ε� ó��
*   - �ؽ�ó �ε� �� BindTexture(index, SRV) ����
*   - ��� �׷�(������ auto ��ĵ), ��� ��� API ����
*   - �� ������ Update() ȣ�� �� DrawModel() ȣ��
*/
class MinimalUserModel : public CubismUserModel
{
public:
	std::string baseDir;
	std::unique_ptr<CubismModelSettingJson> setting;
	std::vector<std::string> motionGroups;
	std::map<std::string, ACubismMotion*> motions; // �ε�� ��� ĳ��
	float userTimeSeconds = 0.0f;
    // Live2D �������� BindTexture �� SRV�� ������ ������Ű�� �����Ƿ�, ���� ������ ���� ���⼭ �����Ѵ�
    std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> textureSRVs;
    // �𵨿� ��� �׷��� ���� �� �������� �ڵ� �˻��� ��� ���� ���
    std::vector<std::string> autoMotionFiles; // ��� ���ϸ� (baseDir ����)
    const char* AutoGroupName() const { return "auto"; }
    // �ܺο��� ���� �߰��� ��� ����(���� ���)
    std::vector<std::string> extraMotionPaths; // �� ���� �� ���Ͽ�
    const char* ExtraGroupName() const { return "extra"; }

    // ���� �׷��� ��� ����(���� ��� �Ǵ� auto �׷�)
    int GetMotionCountForGroup(const char* group) const
    {
        if (!group) return 0;
        if (std::string(group) == AutoGroupName())
        {
            return (int)autoMotionFiles.size();
        }
        if (std::string(group) == ExtraGroupName())
        {
            return (int)extraMotionPaths.size();
        }
        if (!setting) return 0;
        return setting->GetMotionCount(group);
    }

    // baseDir���� *.motion3.json �ڵ� ��ĵ
    bool PopulateAutoMotions()
    {
        autoMotionFiles.clear();
        if (baseDir.empty()) return false;
        try
        {
            for (const auto& entry : std::filesystem::directory_iterator(baseDir))
            {
                if (!entry.is_regular_file()) continue;
                auto path = entry.path();
                auto ext = path.extension().wstring();
                // Ȯ���� ��ġ
                if (_wcsicmp(ext.c_str(), L".json") == 0 || _wcsicmp(ext.c_str(), L".motion3.json") == 0)
                {
                    auto stem = path.filename().wstring();
                    // motion3.json �̸� ����
                    std::wstring wname = stem;
                    std::string name(wname.begin(), wname.end());
                    if (name.size() >= 12 && name.find("motion3.json") != std::string::npos)
                    {
                        autoMotionFiles.push_back(std::string(path.filename().u8string()));
                    }
                }
            }
        }
        catch(...)
        {
            return false;
        }
        return !autoMotionFiles.empty();
    }

    // �ܺ� ��� �߰�: �� ���� ���θ� auto�� ����η�, �ܺθ� extra�� �����η� ���
    void AddExtraMotionFromPath(const std::wstring& fullPathW)
    {
        std::string fullPath(fullPathW.begin(), fullPathW.end());
        // baseDir�� �����ϴ��� �˻�(��ҹ��� �ܼ� ��)
        if (!baseDir.empty())
        {
            std::string base = baseDir;
            // ��� ������ ����ȭ
            for (char& c : fullPath) if (c=='\\') c='/';
            for (char& c : base) if (c=='\\') c='/';
            if (fullPath.size() >= base.size() && std::equal(base.begin(), base.end(), fullPath.begin()))
            {
                std::string rel = fullPath.substr(base.size());
                // �ߺ� ����
                if (std::find(autoMotionFiles.begin(), autoMotionFiles.end(), rel) == autoMotionFiles.end())
                {
                    autoMotionFiles.push_back(rel);
                }
                // auto �׷� ����
                if (std::find(motionGroups.begin(), motionGroups.end(), AutoGroupName()) == motionGroups.end())
                {
                    motionGroups.push_back(AutoGroupName());
                }
                return;
            }
        }
        // �ܺ� ��� �� extra �׷�
        if (std::find(extraMotionPaths.begin(), extraMotionPaths.end(), fullPath) == extraMotionPaths.end())
        {
            extraMotionPaths.push_back(fullPath);
        }
        if (std::find(motionGroups.begin(), motionGroups.end(), ExtraGroupName()) == motionGroups.end())
        {
            motionGroups.push_back(ExtraGroupName());
        }
    }

	// ��� ��� �׷��� ��ǵ��� ���ε�(ĳ��)�Ѵ�
	void PreloadAllMotions()
	{
		if (!setting) return;
		// ���� ��� �׷��
		for (const std::string& groupName : motionGroups)
		{
			int cnt = GetMotionCountForGroup(groupName.c_str());
			for (int i = 0; i < cnt; ++i)
			{
				std::string key = groupName + "_" + std::to_string(i);
				if (motions.find(key) != motions.end()) continue; // �̹� �ε��
				std::string path;
				if (groupName == AutoGroupName())
				{
					if (i < 0 || i >= (int)autoMotionFiles.size()) continue;
					path = baseDir + autoMotionFiles[i];
				}
				else if (groupName == ExtraGroupName())
				{
					if (i < 0 || i >= (int)extraMotionPaths.size()) continue;
					path = extraMotionPaths[i];
				}
				else
				{
					const char* file = setting->GetMotionFileName(groupName.c_str(), i);
					path = baseDir + (file ? file : "");
				}
				FILE* f=nullptr; fopen_s(&f, path.c_str(), "rb"); if (!f) continue;
				fseek(f,0,SEEK_END); long sz = ftell(f); fseek(f,0,SEEK_SET);
				std::vector<uint8_t> buf(sz); fread(buf.data(),1,sz,f); fclose(f);
				ACubismMotion* motion = LoadMotion(
					buf.data(),
					(csmSizeInt)buf.size(),
					nullptr,
					nullptr,
					nullptr,
					setting.get(),
					groupName.c_str(),
					i,
					false);
				if (motion) motions[key] = motion;
			}
		}
	}

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
		// �б�
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
		// motions group �̸� ����
		int gcount = setting->GetMotionGroupCount();
		motionGroups.clear();
		for(int gi=0; gi<gcount; ++gi){ motionGroups.push_back(setting->GetMotionGroupName(gi)); }
        // �׷��� �ϳ��� ������ �ڵ� ��ĵ�Ͽ� ��ü
        if (motionGroups.empty())
        {
            if (PopulateAutoMotions())
            {
                motionGroups.push_back(AutoGroupName());
            }
        }
		CreateRenderer();
		// ��� ��� �׷��� ����� ���ε�(������ ĳ�õ�)
		PreloadAllMotions();
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
                // ���� ����: ���� ������� �������� �ʵ��� ����
                textureSRVs.push_back(srv);
				++bound;
			}
		}
		return bound>0;
	}

	// ���� ������Ʈ: ��Ǹ� �����ϰ� �� ������Ʈ
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

	// ��� ����
	Csm::CubismMotionQueueEntryHandle StartMotion(const char* group, int no, int priority)
	{
		if (!setting) return Csm::InvalidMotionQueueEntryHandleValue;
		int count = GetMotionCountForGroup(group);
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
			std::string path;
			if (std::string(group) == AutoGroupName())
			{
				path = baseDir + (no >=0 && no < (int)autoMotionFiles.size() ? autoMotionFiles[no] : std::string());
			}
            else if (std::string(group) == ExtraGroupName())
            {
                path = (no >= 0 && no < (int)extraMotionPaths.size()) ? extraMotionPaths[no] : std::string();
            }
			else
			{
				const char* file = setting->GetMotionFileName(group, no);
				path = baseDir + (file ? file : "");
			}
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
		// �� ������ ���� �Ķ���� ������Ʈ
		ModelParamUpdate();
		Rendering::CubismRenderer_D3D11::StartFrame(dev, ctx, (csmUint32)vpw, (csmUint32)vph);
		// StartFrame�� s_context/viewport�� ������ �ڿ� �⺻ ���� ���¸� ����
		Rendering::CubismRenderer_D3D11::SetDefaultRenderState();
		CubismMatrix44 proj; proj.LoadIdentity();
		if (GetModel()->GetCanvasWidth() > 1.0f && vpw < vph){ GetModelMatrix()->SetWidth(2.0f); proj.Scale(1.0f, (float)vpw/(float)vph);} else { proj.Scale((float)vph/(float)vpw, 1.0f);} 
		GetRenderer<Rendering::CubismRenderer_D3D11>()->SetMvpMatrix(&proj);
		// Anisotropy �ּ� 1�� ���� (0�̸� ���÷� ���� �� ��ȿ���� �ʾ� ���� �߻�)
		GetRenderer<Rendering::CubismRenderer_D3D11>()->SetAnisotropy(1.0f);
		GetRenderer<Rendering::CubismRenderer_D3D11>()->DrawModel();
		Rendering::CubismRenderer_D3D11::EndFrame(dev);
	}
};

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

/*
* @brief : �� �ʱ�ȭ(Direct3D/ImGui/Live2D)
* @details :
*   - D3D11 ���� Ÿ��/���̽��ٽ�/����Ʈ ����
*   - ImGui �鿣�� �ʱ�ȭ
*   - CubismFramework ���� �� D3D11 ���̴� ����
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

	// Live2D Framework �ʱ�ȭ (Core ��ũ �ʿ�)
	{
		CubismFramework::Option opt{};
		opt.LogFunction = nullptr;
		opt.LoggingLevel = CubismFramework::Option::LogLevel_Verbose;
		opt.LoadFileFunction = CF_LoadFile;
		opt.ReleaseBytesFunction = CF_ReleaseBytes;
		CubismFramework::StartUp(&g_alloc, &opt);
		CubismFramework::Initialize();
		Rendering::CubismRenderer_D3D11::InitializeConstantSettings(1, m_pDevice);
		// D3D11 ���̴� ���� (�ʼ�)
		Rendering::CubismRenderer_D3D11::GenerateShader(m_pDevice);
		m_L2DReady = true; m_L2DStatus = "Live2D �ʱ�ȭ �Ϸ�";
	}

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

/*
* @brief : �� ���� ó��
* @details : ImGui/Live2D/D3D11 ���ҽ��� �������� �����մϴ�
*/
void App::OnUninitialize()
{
	// ImGui ����
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	// Live2D ����
	delete m_L2D; m_L2D=nullptr; m_L2DLoaded=false;
	Rendering::CubismRenderer_D3D11::DeleteRenderStateManager();
	CubismFramework::Dispose();

	UninitD3D();
}

/*
* @brief : �� ������ ����
* @details : FPS/�ý��� ���� ���� �� VRAM ���� ��ȸ�� �����մϴ�(Live2D ������Ʈ�� Draw �ܰ� ����)
*/
void App::OnUpdate(const float& dt)
{
	static float t0 = 0.0f, t1 = 0.0f, t2 = 0.0f;
	t0 += 0.6f * dt;   // �θ�(��Ʈ) Yaw �ӵ�
	t1 += 1.0f * dt;   // �ι�° �޽�(�ڽ�1) Yaw �ӵ� (��Ʈ�� �ٸ���)
	t2 += 1.2f * dt;   // ����° �޽�(�ڽ�2) ���� �ӵ�


	// View/Proj�� UI�� �ݿ� (�� ������)
	XMMATRIX view = XMMatrixTranspose(XMMatrixLookAtLH(
		XMVectorSet(m_CameraPos.x, m_CameraPos.y, m_CameraPos.z, 0.0f),
		XMVectorSet(m_CameraPos.x + 0.0f, m_CameraPos.y + 0.0f, m_CameraPos.z + 1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)));
	float fovRad = XMConvertToRadians(m_CameraFovDeg);
	XMMATRIX proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovRad, AspectRatio(), m_CameraNear, m_CameraFar));

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

/*
* @brief : ���� ���� ����
* @details :
*   - �����/���̽��ٽ� Ŭ���� �� ImGui UI �� Live2D ������Ʈ/��ο� �� �ý��� ����â �� Present
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
	m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerState);
	m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);

	// ImGui ������ �� UI ������
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	/*
	* @brief : Live2D �δ�/���� UI
	* @details : model3.json ���� ���� �� ��/�ؽ�ó �ε� �� ����ũ ���� �غ�
	*/
	ImGui::SetNextWindowSize(ImVec2(250,280), ImGuiCond_Once);
	if (ImGui::Begin("Live2D"))
	{
		ImGui::Text("Camera");
		ImGui::DragFloat3("Camera Pos (x,y,z)", &m_CameraPos.x, 0.1f);
		ImGui::SliderFloat("Camera FOV (deg)", &m_CameraFovDeg, 30.0f, 120.0f);
		ImGui::DragFloatRange2("Near/Far", &m_CameraNear, &m_CameraFar, 0.1f, 0.01f, 5000.0f, "Near: %.2f", "Far: %.2f");

		// Live2D: model3.json ���� �� ���� ǥ��
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
				m_L2DModelJsonPath = file; m_L2DRequested = true; m_L2DStatus = "���õ�";
				for (auto* p : m_L2DTexSRVs) { SAFE_RELEASE(p); }
				m_L2DTexSRVs.clear(); m_L2DTexSizes.clear();
				// ���� �� �ε�
				if (m_L2DReady)
				{
					delete m_L2D; m_L2D=nullptr; m_L2DLoaded=false; m_L2DMotionGroups.clear();
					m_L2D = new MinimalUserModel();
					if (m_L2D->LoadFromModel3(m_L2DModelJsonPath) && m_L2D->LoadAndBindTextures(m_pDevice))
					{
						m_L2DLoaded = true; m_L2DStatus = "�� �ε� �Ϸ�";
						m_L2DMotionGroups = m_L2D->motionGroups;
						// ����ũ ���� Ÿ�� ����: ũ��/���е� ����
						if (auto* r = m_L2D->GetRenderer<Rendering::CubismRenderer_D3D11>())
						{
							r->UseHighPrecisionMask(false);
							r->SetClippingMaskBufferSize(256.0f, 256.0f);
							// ����ũ�� ������ũ�� ���� �̸� ���� (����ۼ�=0)
							int rtc = r->GetRenderTextureCount();
							for (int i = 0; i < rtc; ++i)
							{
								r->GetMaskBuffer(0, i)->CreateOffscreenSurface(m_pDevice, 256, 256);
							}
						}
					}
					else { m_L2DStatus = "�� �ε� ����"; }
				}
			}
		}
		if (!m_L2DModelJsonPath.empty()) { ImGui::Text("Model: %ls", m_L2DModelJsonPath.c_str()); }
		ImGui::Text("Status: %s", m_L2DStatus.empty() ? "-" : m_L2DStatus.c_str());

        // �ܺ� ��� �߰� ��ư
        if (m_L2DLoaded && m_L2D)
        {
            if (ImGui::Button("Add Motion JSON..."))
            {
                wchar_t file[1024] = {0};
                OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = m_hWnd;
                ofn.lpstrFilter = L"Motion3 JSON (*.motion3.json)\0*.motion3.json\0All Files\0*.*\0\0";
                ofn.lpstrFile = file; ofn.nMaxFile = 1024; ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameW(&ofn))
                {
                    m_L2D->AddExtraMotionFromPath(file);
                }
            }
        }
	}
	ImGui::End();

	/*
	* @brief : Live2D �� ����/���� UI
	* @details : ��� �׷�/��� �ε��� ���� �� ���, �Ķ����/��Ʈ ���� UI ����
	*/
	if (m_L2DLoaded && m_L2D && m_ShowL2DInfo)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 size(380, 540.0f);
		ImVec2 pos(io.DisplaySize.x - size.x - 10.0f, 210.0f);
		ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(size, ImGuiCond_Once);
		if (ImGui::Begin("Live2D Model Info", &m_ShowL2DInfo))
		{
			CubismModel* model = m_L2D->GetModel();
			int parts = model->GetPartCount();
			int params = model->GetParameterCount();
			int draws = model->GetDrawableCount();
			ImGui::Text("Parts: %d, Parameters: %d, Drawables: %d", parts, params, draws);
			ImGui::Separator();

			// Motion Player: �׷�/��� ���� �� ���
			if (!m_L2DMotionGroups.empty() && m_L2D && m_L2D->setting)
			{
				const int groupCount = (int)m_L2DMotionGroups.size();
				if (m_L2DMotionGroupIndex < 0 || m_L2DMotionGroupIndex >= groupCount) m_L2DMotionGroupIndex = 0;
				const char* currentGroup = m_L2DMotionGroups[m_L2DMotionGroupIndex].c_str();
				ImGui::TextUnformatted("Motion Player");
				if (ImGui::BeginCombo("Group", currentGroup))
				{
					for (int i = 0; i < groupCount; ++i)
					{
						bool selected = (i == m_L2DMotionGroupIndex);
						ImGui::PushID(i);
						if (ImGui::Selectable(m_L2DMotionGroups[i].c_str(), selected))
						{
							m_L2DMotionGroupIndex = i;
							m_L2DMotionIndex = 0;
						}
						if (selected) ImGui::SetItemDefaultFocus();
						ImGui::PopID();
					}
					ImGui::EndCombo();
				}

				int motionCount = m_L2D->GetMotionCountForGroup(currentGroup);
				if (motionCount <= 0)
				{
					ImGui::TextUnformatted("(No motions in group)");
				}
				else
				{
					if (m_L2DMotionIndex < 0) m_L2DMotionIndex = 0;
					if (m_L2DMotionIndex >= motionCount) m_L2DMotionIndex = motionCount - 1;
					ImGui::SliderInt("Motion Index", &m_L2DMotionIndex, 0, motionCount - 1);
					ImGui::SameLine();
					if (ImGui::Button("Play Selected"))
					{
						m_L2D->StartMotion(currentGroup, m_L2DMotionIndex, 2);
					}

					// ����Ʈ�� ��� ����� ���ϸ�(�Ǵ� �ĺ���)���� �����Ͽ� ����/���
					ImGui::Text("Motions (%d)", motionCount);
					ImGui::BeginChild("MotionList", ImVec2(0, 180), true);
					for (int mi = 0; mi < motionCount; ++mi)
					{
						const char* nameC = nullptr;
						std::string line;
						if (std::string(currentGroup) == m_L2D->AutoGroupName())
						{
							if (mi >= 0 && mi < (int)m_L2D->autoMotionFiles.size())
							{
								line = m_L2D->autoMotionFiles[mi];
							}
						}
						else if (std::string(currentGroup) == m_L2D->ExtraGroupName())
						{
							if (mi >= 0 && mi < (int)m_L2D->extraMotionPaths.size())
							{
								// ���ϸ� ǥ��
								std::filesystem::path p = std::filesystem::path(m_L2D->extraMotionPaths[mi]);
								line = p.filename().u8string();
							}
						}
						else
						{
							nameC = m_L2D->setting->GetMotionFileName(currentGroup, mi);
							line = nameC ? nameC : "(unnamed)";
						}
						bool selected = (mi == m_L2DMotionIndex);
						if (ImGui::Selectable((line + "##motion_" + std::to_string(mi)).c_str(), selected))
						{
							m_L2DMotionIndex = mi;
						}
						ImGui::SameLine();
						if (ImGui::SmallButton(("Play##p_" + std::to_string(mi)).c_str()))
						{
							m_L2D->StartMotion(currentGroup, mi, 2);
						}
					}
					ImGui::EndChild();
				}
			}

			ImGui::Separator();
			// ��� �׷�/������
			if (!m_L2DMotionGroups.empty())
			{
				ImGui::Text("Motion Groups: %d", (int)m_L2DMotionGroups.size());
				for (size_t i=0;i<m_L2DMotionGroups.size();++i)
				{
					ImGui::PushID((int)i);
					ImGui::TextUnformatted(m_L2DMotionGroups[i].c_str());
					int cnt = m_L2D->GetMotionCountForGroup(m_L2DMotionGroups[i].c_str());
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
			// �Ķ���� ����
			if (ImGui::TreeNode("Parameters"))
			{
				for (int i=0;i<params;++i)
				{
					auto id = model->GetParameterId(i);
					float v = model->GetParameterValue(i);
					float mn = model->GetParameterMinimumValue(i);
					float mx = model->GetParameterMaximumValue(i);
					ImGui::PushID(i);
					ImGui::Text("[%d] %s", i, id->GetString().GetRawString());
					ImGui::SliderFloat("##param", &v, mn, mx);
					model->SetParameterValue(i, v);
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			// ���� ���� ����
			if (ImGui::TreeNode("Parts"))
			{
				for (int i=0;i<parts;++i)
				{
					auto id = model->GetPartId(i);
					float op = model->GetPartOpacity(i);
					ImGui::PushID(10000+i);
					ImGui::Text("[%d] %s", i, id->GetString().GetRawString());
					ImGui::SliderFloat("##part", &op, 0.0f, 1.0f);
					model->SetPartOpacity(i, op);
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
		}
		ImGui::End();
	}

	// ���� �̹���/�ý��� â ���� ����, Live2D �� �׸���
	if (m_L2DLoaded && m_L2D)
	{
		m_L2D->ModelParamUpdate(); // ���/��ũ/���� �� ������Ʈ ����
		m_L2D->DrawModelD3D11(m_pDevice, m_pDeviceContext, m_ClientWidth, m_ClientHeight);
	}

	// Live2D �ؽ�ó �̸����� â
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
			// RAM/VRAM
			double ramTotalGB = (double)m_RamTotal / (1024.0 * 1024.0 * 1024.0);
			double ramUsedGB = (double)(m_RamTotal - m_RamAvail) / (1024.0 * 1024.0 * 1024.0);
			ImGui::Text("RAM : %.2f GB / %.2f GB", ramUsedGB, ramTotalGB);
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
* @brief : D3D11 ��ġ/����ü��/RTV/DSV/����Ʈ �ʱ�ȭ
* @details : Live2D�� ImGui �������� ���� �⺻ ��� ���¸� �����մϴ�
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
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;

	// ��Ƽ���ø� ����
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	// ����� �÷���
	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	/*
	* @brief  Direct3D ����̽�, ����̽� ���ؽ�Ʈ, ����ü�� ����
	*/
	HR_T(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_pSwapChain, &m_pDevice, NULL, &m_pDeviceContext));

	/*
	* @brief  ����ü�� ����۷� RTV�� ����� OM ���������� ���ε��Ѵ�
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

	// ����Ʈ(Viewport) ����
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

/*
* @brief : D3D11 ���ҽ� ����
* @details : RTV/DSV/����/����̽�/���ؽ�Ʈ/����ü���� �������� �����մϴ�
*/
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
* @brief : ������ ���ҽ� �ʱ�ȭ �ڸ�
*/
bool App::InitScene()
{
	// ť��/�ؽ�ó/������� �ʱ�ȭ ����: Live2D�� ���

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
	// 24�� ���� (�� �� 4��) + �ؽ�ó ��ǥ
	VertexPosTex vertices[] =
	{
		// �ո� (z = -1)
		{ XMFLOAT3(-1,-1,-1), XMFLOAT2(0,1) },
		{ XMFLOAT3(-1, 1,-1), XMFLOAT2(0,0) },
		{ XMFLOAT3( 1, 1,-1), XMFLOAT2(1,0) },
		{ XMFLOAT3( 1,-1,-1), XMFLOAT2(1,1) },
		// ���� (x = -1)
		{ XMFLOAT3(-1,-1, 1), XMFLOAT2(0,1) },
		{ XMFLOAT3(-1, 1, 1), XMFLOAT2(0,0) },
		{ XMFLOAT3(-1, 1,-1), XMFLOAT2(1,0) },
		{ XMFLOAT3(-1,-1,-1), XMFLOAT2(1,1) },
		// �� (y = 1)
		{ XMFLOAT3(-1, 1,-1), XMFLOAT2(0,1) },
		{ XMFLOAT3(-1, 1, 1), XMFLOAT2(0,0) },
		{ XMFLOAT3( 1, 1, 1), XMFLOAT2(1,0) },
		{ XMFLOAT3( 1, 1,-1), XMFLOAT2(1,1) },
		// �� (z = 1)
		{ XMFLOAT3( 1,-1, 1), XMFLOAT2(0,1) },
		{ XMFLOAT3( 1, 1, 1), XMFLOAT2(0,0) },
		{ XMFLOAT3(-1, 1, 1), XMFLOAT2(1,0) },
		{ XMFLOAT3(-1,-1, 1), XMFLOAT2(1,1) },
		// ������ (x = 1)
		{ XMFLOAT3( 1,-1,-1), XMFLOAT2(0,1) },
		{ XMFLOAT3( 1, 1,-1), XMFLOAT2(0,0) },
		{ XMFLOAT3( 1, 1, 1), XMFLOAT2(1,0) },
		{ XMFLOAT3( 1,-1, 1), XMFLOAT2(1,1) },
		// �Ʒ� (y = -1)
		{ XMFLOAT3(-1,-1, 1), XMFLOAT2(0,1) },
		{ XMFLOAT3(-1,-1,-1), XMFLOAT2(0,0) },
		{ XMFLOAT3( 1,-1,-1), XMFLOAT2(1,0) },
		{ XMFLOAT3( 1,-1, 1), XMFLOAT2(1,1) }
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
		// �ո� (0~3)
		0,1,2, 2,3,0,
		// ���� (4~7)
		4,5,6, 6,7,4,
		// �� (8~11)
		8,9,10, 10,11,8,
		// �� (12~15)
		12,13,14, 14,15,12,
		// ������ (16~19)
		16,17,18, 18,19,16,
		// �Ʒ� (20~23)
		20,21,22, 22,23,20
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
	m_CBuffer.world = XMMatrixIdentity();
	m_CBuffer.view = XMMatrixTranspose(XMMatrixLookAtLH(
		XMVectorSet(m_CameraPos.x, m_CameraPos.y, m_CameraPos.z, 0.0f),
		XMVectorSet(m_CameraPos.x + 0.0f, m_CameraPos.y + 0.0f, m_CameraPos.z + 1.0f, 0.0f),
		XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
	));
	float fovRad = XMConvertToRadians(m_CameraFovDeg);
	m_CBuffer.proj = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovRad, AspectRatio(), m_CameraNear, m_CameraFar));
	
	// ���÷� ����
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
	SAFE_RELEASE(m_pSamplerState);
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
