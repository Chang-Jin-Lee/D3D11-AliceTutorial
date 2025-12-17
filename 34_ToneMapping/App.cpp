/*
* @brief  : ToneMapping
* @details: 톤 매핑, HDR 렌더링, 감마 보정 등을 구현한 데모입니다.
*/

#include "App.h"
#include "SoundManager.h"
#include "../Common/Helper.h"
#include "../Common/mmd/VmdCameraPlayer.h"
#include <windows.h>
#include <random>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxtk/WICTextureLoader.h>
#include <directxtk/DDSTextureLoader.h>
#include <directxtk/SimpleMath.h>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_set>
#include <cstring>
#include <cfloat>
#include "../Common/StaticMesh.h"
#include "../Common/LineRenderer.h"
#include "../Common/Skybox.h"
#include "../Common/SystemInfomation.h"
#include "../Common/Mesh/FbxModel.h"
#include "../Common/Mesh/FbxAnimation.h"
#include "../Common/ObjManager.h"
#include "../Common/PmxManager.h"
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <imgui_stdlib.h>
#include <imgui_internal.h>
#include <commdlg.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../Common/AssetManager.h"
#include "../Common/Transform.h"
#include "../Common/BaseObject.h"
#include "../Common/Ray.h"
#include "SceneA.h"
#include "SceneB.h"
#include <directxtk/GamePad.h>

#include <dxgi1_4.h>	// swapchain3 ToneMapping을 위한 것
#include <dxgi1_6.h>	// swapchain3 ToneMapping을 위한 것

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "Comdlg32.lib")

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 내부 전용 타입들
struct DirectionalLight { XMFLOAT4 ambient; XMFLOAT4 diffuse; XMFLOAT4 specular; XMFLOAT3 direction; float pad; };
struct Material { XMFLOAT4 ambient; XMFLOAT4 diffuse; XMFLOAT4 specular; XMFLOAT4 reflect; };
struct PBRMaterialCPU
{
	XMFLOAT4 baseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	float metalness = 0.0f;
	float roughness = 0.5f;
	float ambientOcclusion = 1.0f;
	float pad = 0.0f;
};
struct ConstantBuffer {
	XMMATRIX world; XMMATRIX view; XMMATRIX proj; XMMATRIX worldInvTranspose;
	Material material; DirectionalLight dirLight; XMFLOAT3 eyePos; int shadingMode = 0;
	int enableNormalMap = 1; int useSpecularMap = 0; int useDiffuseMap = 1; float pad = 0.0f;
	int useTextureColor = 1; XMFLOAT3 pbrPad = { 0,0,0 };
	XMFLOAT4 pbrBaseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	float pbrMetalness = 0.0f;
	float pbrRoughness = 0.5f;
	float pbrAO = 1.0f;
	float pbrPad2 = 0.0f;
	float outlineWidth = 0.15f; float outlinePow = 1.0f; float outlineThickness = 0.014f; float outlineStrength = 1.0f; XMFLOAT4 outlineColor = XMFLOAT4(0, 0, 0, 1);
	// Shadow params
	XMMATRIX lightViewProj;
	float    shadowBias = 0.0015f;
	float    shadowMapSize = 2048.0f;
	float    shadowPCFRadius = 1.0f;
	int      shadowEnabled = 1;
	// Debug/Lines
	int      boundsBoneIndex = -1; 
	XMFLOAT3 boundsPad = {0,0,0};
	float g_MaxHDRNits = 100.0f;  // 셰이더와 변수명 일치 (g_MaxHDRNits)
	float g_Exposure = 1.0f;      // 셰이더와 변수명 일치 (g_Exposure)
	float padding[2];
};
enum class ShadingMode { Phong = 0, BlinnPhong = 1, Lambert = 2, Unlit = 3, TextureOnly = 4, ToonShading = 5, PBR = 6 };
enum class ModelSource { FBX, OBJ, PMX, Custom };
struct ModelSubset { uint32_t start; uint32_t count; uint32_t materialIndex; };
// 메시 통계 구조체
struct MeshStats { uint32_t vertices = 0, edges = 0, faces = 0, triangles = 0; };

// 3D 모델 리소스
struct SharedModelData
{
	std::wstring pathW;
	ModelSource source = ModelSource::Custom;
	std::shared_ptr<FbxModel> fbx;		// FBX용 로더
	std::shared_ptr<ObjManager> obj;	// OBJ용 로더
	std::shared_ptr<PmxManager> pmx;	// PMX용 로더

	// 그리는 자원들. 리소스를 모두 공유함
	ID3D11Buffer* vb = nullptr;
	ID3D11Buffer* ib = nullptr;
	int           indexCount = 0;
	UINT          stride = 0;
	std::vector<ModelSubset> subsets;
	std::vector<ID3D11ShaderResourceView*> materialSRVs; // BaseColor/Albedo 텍스처
	std::vector<ID3D11ShaderResourceView*> normalSRVs;   // 노말맵 텍스처 (옵션)
};

// 여러 모델을 그리기 위한 구조체
struct ModelEntry
{
	std::wstring modelName{ L"" };
	ModelSource source = ModelSource::Custom;     // 공유 데이터 동일 경로 모델끼리
	std::shared_ptr<SharedModelData> shared;

	// 드로우는 shared의 자원을 사용

	// 트랜스폼
	XMFLOAT3 pos = { 0,0,0 };
	XMFLOAT3 scale = { 1,1,1 };
	XMFLOAT3 rotDeg = { 0,0,0 }; // yaw=pitch=roll(deg)
	bool     autoRotate = false;
	// 모델별 셰이딩 모드 개별로 선택가능함
	ShadingMode modelShading = ShadingMode::Phong;
	bool outlineEnabled = true;
	bool showBoneDetails = false;

	// 인스턴스 전용 애니메이터/머티리얼
	FbxAnimation animator; // FBX 전용: per-instance bone palette
	bool animatorInited = false;
	// 애니메이션 업데이트 LOD를 위한 누적 시간 (입력/ImGui 프리즈 방지용)
	float animUpdateAccum = 0.0f;
	Material instanceMaterial{ {1,1,1,1}, {1,1,1,1}, {1,1,1,32}, {0,0,0,0} };
	bool useInstanceMaterial = false;
	PBRMaterialCPU instancePbrMaterial{};
	bool useInstancePbrMaterial = false;

	// 사전 계산된 메시 통계
	MeshStats meshStats{};
	bool meshStatsValid = false;

	// FBX 전용 애니메이션 UI 상태
	// ImGui에서 보여주기 위함
	int  uiSelectedAnim = -1;
	bool uiAnimPlaying = false;

	// 본 트리 캐시 로드 시 1회 구축, UI는 이 캐시만 사용하여 빠르게 렌더링
	struct CachedSkelNode { std::wstring nameW; std::string nameU8; bool isBone = false; std::vector<int> children; };
	std::vector<CachedSkelNode> boneCache;
	int  boneRoot = -1;
	bool boneCacheValid = false;
	std::string boneDisplayText; // 캐싱된 UI 출력

	// 로컬 공간 AABB (모델의 원본 좌표계 기준)
	bool boundsValid = false;
	XMFLOAT3 boundsMin = { 0,0,0 };
	XMFLOAT3 boundsMax = { 0,0,0 };
	// 디버그 AABB에 적용할 기준 본 인덱스(-1이면 자동: 스켈레톤 있으면 0, 없으면 비활성)
	int boundsBoneIndex = -1;
    
    // 애니메이션 루트 변환을 반영한 AABB 샘플들(로컬 공간). 현재 클립 기준
    std::vector<XMFLOAT3> animAabbMinSamples;
    std::vector<XMFLOAT3> animAabbMaxSamples;
    float animAabbSampleDt = 0.0f;
    int   animAabbClip = -1;

	// PMX 애니메이션과 동기화할 사운드 경로 및 상태
	std::wstring audioPath;
	bool         audioLoaded = false;
};

struct CubeData
{
	std::wstring name;
	Transform tranform;
	ECubeType type;
};


// 본 캐시/텍스트 일괄 구축 함수
static void BuildBoneCacheStructure(ModelEntry& entry, const char* filter)
{
	entry.boneCache.clear();
	entry.boneRoot = -1;
	entry.boneCacheValid = false;
	entry.boneDisplayText.clear();

	// 캐시 생성
	if (entry.source == ModelSource::FBX && entry.shared && entry.shared->fbx && entry.shared->fbx->HasSkeleton())
	{
		const auto& sk = entry.shared->fbx->GetSkeleton();
		entry.boneCache.resize(sk.size());
		for (size_t si = 0; si < sk.size(); ++si)
		{
			const auto& s = sk[si];
			auto& c = entry.boneCache[si];
			c.nameW = s.nameW;
			c.nameU8 = Utf8FromWString(c.nameW);
			c.isBone = s.isBone;
			c.children = s.children;
		}
		entry.boneRoot = entry.shared->fbx->GetSkeletonRoot();
		entry.boneCacheValid = (entry.boneRoot >= 0) && ((size_t)entry.boneRoot < entry.boneCache.size());
	}
	else if (entry.source == ModelSource::PMX && entry.shared && entry.shared->pmx && entry.shared->pmx->HasSkeleton())
	{
		const auto& sk = entry.shared->pmx->GetSkeleton();
		entry.boneCache.resize(sk.size());
		for (size_t si = 0; si < sk.size(); ++si)
		{
			const auto& s = sk[si];
			auto& c = entry.boneCache[si];
			c.nameW = s.nameW;
			c.nameU8 = Utf8FromWString(c.nameW);
			c.isBone = s.isBone;
			c.children = s.children;
		}
		entry.boneRoot = entry.shared->pmx->GetSkeletonRoot();
		entry.boneCacheValid = (entry.boneRoot >= 0) && ((size_t)entry.boneRoot < entry.boneCache.size());
	}

	if (!entry.boneCacheValid || entry.boneRoot < 0 || entry.boneRoot >= (int)entry.boneCache.size()) return;

	// 출력 문자열 생성
	const auto& cache = entry.boneCache;
	const bool useFilter = (filter != nullptr && filter[0] != '\0');
	std::string f = useFilter ? std::string(filter) : std::string();

	entry.boneDisplayText += "Bone Count: ";
	entry.boneDisplayText += std::to_string((unsigned)cache.size());
	entry.boneDisplayText += "\n\n";

	std::function<bool(int)> subtreeContainsFilter = [&](int idx) -> bool {
		if (!useFilter) return true;
		if (idx < 0 || idx >= (int)cache.size()) return false;
		if (cache[idx].nameU8.find(f) != std::string::npos) return true;
		for (int ch : cache[idx].children) if (subtreeContainsFilter(ch)) return true;
		return false;
		};

	std::function<void(int, int)> dfs = [&](int idx, int depth)
		{
			if (idx < 0 || idx >= (int)cache.size()) return;
			if (useFilter && !subtreeContainsFilter(idx)) return;
			const auto& n = cache[idx];
			entry.boneDisplayText.append((size_t)depth * 2u, ' ');
			entry.boneDisplayText += n.nameU8;
			entry.boneDisplayText += '\n';
			for (int ch : n.children) dfs(ch, depth + 1);
		};

	dfs(entry.boneRoot, 0);
}


// 인덱스 버퍼를 스테이징으로 복사해 통계 계산 삼각형 리스트가 들어온다고 가정
static bool ComputeMeshStats(ID3D11Device* device, ID3D11DeviceContext* ctx,
	ID3D11Buffer* vb, UINT vertexStride, ID3D11Buffer* ib, int indexCount,
	MeshStats& out)
{
	if (!device || !ctx || !vb || !ib || vertexStride == 0 || indexCount <= 0) return false;

	D3D11_BUFFER_DESC vbd{}; vb->GetDesc(&vbd);
	out.vertices = vbd.ByteWidth / vertexStride;

	D3D11_BUFFER_DESC ibd{}; ib->GetDesc(&ibd);
	D3D11_BUFFER_DESC sd = ibd; sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; sd.MiscFlags = 0;
	ID3D11Buffer* staging = nullptr;
	if (FAILED(device->CreateBuffer(&sd, nullptr, &staging))) return false;
	ctx->CopyResource(staging, ib);

	D3D11_MAPPED_SUBRESOURCE mapped{};
	bool ok = false;
	if (SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)))
	{
		const uint32_t* idx = reinterpret_cast<const uint32_t*>(mapped.pData);
		int ic = indexCount;
		out.triangles = (ic >= 3) ? (uint32_t)(ic / 3) : 0;
		out.faces = out.triangles; // 삼각형으로 구성

		std::unordered_set<uint64_t> edges;
		edges.reserve(out.triangles * 2u);
		auto addEdge = [&](uint32_t a, uint32_t b) { uint32_t lo = (a < b) ? a : b; uint32_t hi = (a < b) ? b : a; uint64_t key = ((uint64_t)hi << 32) | (uint64_t)lo; edges.insert(key); };
		for (int i = 0; i + 2 < ic; i += 3)
		{
			uint32_t i0 = idx[i + 0], i1 = idx[i + 1], i2 = idx[i + 2];
			addEdge(i0, i1); addEdge(i1, i2); addEdge(i2, i0);
		}
		out.edges = (uint32_t)edges.size();
		ctx->Unmap(staging, 0);
		ok = true;
	}
	SAFE_RELEASE(staging);
	return ok;
}

// pImpl 정의
struct App::Impl {
	// D3D에서 사용하는 중요한 객체
	ID3D11Device*					m_pDevice = nullptr;
	ID3D11DeviceContext*			m_pDeviceContext = nullptr;
	IDXGISwapChain*					m_pSwapChain = nullptr;
	ID3D11RenderTargetView*			m_pRenderTargetView = nullptr;

	// 파이프라인 셰이더/입력 레이아웃. 기본/PMX/스카이박스/라인
	ID3D11VertexShader*				m_pVertexShader = nullptr;
	ID3D11PixelShader*				m_pPixelShader = nullptr;
	ID3D11PixelShader*				m_pPixelShaderSolid = nullptr;     // 마커용 흰색 출력
	ID3D11VertexShader*				m_pVertexShaderNoTBN = nullptr;    // PMX 전용 VS
	ID3D11InputLayout*				m_pInputLayoutNoTBN = nullptr;     // PMX 전용 IL
	// FBX GPU 스키닝용 VS/IL
	ID3D11VertexShader*				m_pVertexShaderSkinned = nullptr;
	ID3D11InputLayout*				m_pInputLayoutSkinned = nullptr;
	ID3D11VertexShader*				m_pVertexShaderOutline = nullptr;
	ID3D11VertexShader*				m_pVertexShaderSkinnedOutline = nullptr;
	ID3D11VertexShader*				m_pSkyBoxVertexShader = nullptr;
	ID3D11PixelShader*				m_pSkyBoxPixelShader = nullptr;
	ID3D11InputLayout*				m_pSkyBoxInputLayout = nullptr;
	ID3D11VertexShader*				m_pLineVS = nullptr;
	ID3D11InputLayout*				m_pLineInputLayout = nullptr;
	ID3D11PixelShader*				m_pPixelShaderOutline = nullptr;
	ID3D11InputLayout*				m_pOutlineInputLayout = nullptr;

	// 샘플러/블렌드 상태
	ID3D11SamplerState*				m_pSamplerState = nullptr;
	ID3D11BlendState*				m_pAlphaBlendState = nullptr;

	// Skybox/큐브맵 자원 및 옵션
	enum class SkyBoxChoice { Off = 0, bridge = 1, indoor = 2, Baker };
	// IBL 예제에서는 기본값을 Sample 환경맵으로 켜 둔다.
	SkyBoxChoice					m_SkyBoxChoice = SkyBoxChoice::Baker;
	ID3D11ShaderResourceView*		m_pSkyHanakoSRV = nullptr;
	ID3D11ShaderResourceView*		m_pSkyCubeMapSRV = nullptr;
	ID3D11ShaderResourceView*		m_pTextureSRV = nullptr;           // 현재 스카이박스 SRV
	ID3D11ShaderResourceView*		m_pSkyFaceSRV[6] = {};
	ImVec2							m_SkyFaceSize = ImVec2(0, 0);
	// 초기 스카이박스는 IBL Baker Sample 환경맵을 사용
	wchar_t							m_CurrentSkyboxPath[260] = L"..\\Resource\\Skybox\\Sample\\BakerSampleEnvHDR.dds";

	// Cube 텍스처 경로는 CubeObject 안으로 이동
	// 기본 메시 버퍼/입력 레이아웃
	ID3D11InputLayout*				m_pInputLayout = nullptr;
	ID3D11Buffer*					m_pVertexBuffer = nullptr;
	UINT							m_VertextBufferStride = 0;
	UINT							m_VertextBufferOffset = 0;
	ID3D11Buffer*					m_pIndexBuffer = nullptr;
	int								m_nIndices = 0;

	// 공용 상수 버퍼 (b0)
	ID3D11Buffer* m_pConstantBuffer = nullptr;
	ConstantBuffer                m_ConstantBuffer{};                // CPU 캐시

	// 유틸 렌더러/디버그 박스
	class LineRenderer*				m_LineRenderer = nullptr;
	class Skybox*					m_Skybox = nullptr;
	ID3D11Buffer*					m_pDebugBoxVB = nullptr;
	ID3D11Buffer*					m_pDebugBoxIB = nullptr;
	int								m_DebugBoxIndexCount = 0;

	// 깊이/래스터라이저 상태
	ID3D11DepthStencilView*			m_pDepthStencilView = nullptr;
	ID3D11DepthStencilState*		m_pDepthStencilState = nullptr;
	ID3D11DepthStencilState*		m_pDepthStencilStateReadOnly = nullptr; // Outline용 깊이 읽기 전용
	ID3D11RasterizerState*			RSNoCull = nullptr;
	ID3D11RasterizerState*			RSCullClockWise = nullptr;
	ID3D11RasterizerState*			RSCullFront = nullptr;

	// Shadow map
	ID3D11Texture2D*				m_pShadowTex = nullptr;
	ID3D11DepthStencilView*			m_pShadowDSV = nullptr;
	ID3D11ShaderResourceView*		m_pShadowSRV = nullptr;
	D3D11_VIEWPORT					m_ShadowViewport{};
	ID3D11RasterizerState*			RSShadowBias = nullptr;
	ID3D11SamplerState*				m_pShadowSampler = nullptr;
	ID3D11VertexShader*				m_pVSShadow = nullptr;
	ID3D11VertexShader*				m_pVSSkinnedShadow = nullptr;
	// Shadow params UI에서 띄우기 위함
	int								m_ShadowEnabled = 1;
	int								m_ShadowSize = 4096;
	float							m_ShadowBias = 0.0015f;
	float							m_ShadowPCFRadius = 1.0f;
	float							m_ShadowOrthoRadius = 1000.0f; // 카메라 중심 반경(m)


	// 데모/디버그용 텍스처 및 UI 표시 크기
	ID3D11ShaderResourceView*		m_TexHanakoSRV = nullptr;
	bool							m_ShowHanako = false;
	ImVec2							m_HanakoDrawSize = ImVec2(128, 128);
	ImVec2							m_TexHanakoSize = ImVec2(0, 0);

	// FMOD 오디오 데모용 상태 (전역 오디오 컨트롤)
	std::wstring					m_AudioPath;
	bool							m_AudioLoaded = false;

	// 큐브 각 면 텍스처 Diffuse/Normal/Specular
	ID3D11ShaderResourceView*		m_pCubeTextureSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	ID3D11ShaderResourceView*		m_pNormalSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	ID3D11ShaderResourceView*		m_pSpecularSRVs[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

	// IBL(Image Based Lighting)용 텍스처들
	// - Diffuse  : Irradiance map (간접 난반사)
	// - Specular : Prefiltered env map (거칠기별 반사)
	// - BRDF LUT : (NdotV, Roughness)에 대한 F,G 적분 평균값
	ID3D11ShaderResourceView*		m_pIblDiffuseSRV = nullptr;
	ID3D11ShaderResourceView*		m_pIblSpecularSRV = nullptr;
	ID3D11ShaderResourceView*		m_pIblBrdfLutSRV = nullptr;

	// 시스템 정보
	SystemInfomation				m_SystemInfo;
	bool							m_RotateModel = false;

	// 조명/재질
	DirectionalLight				m_DirLight = { {0,0,0,1}, {1,1,1,1}, {0.7f,0.7f,0.7f,1}, {0,-1.1f,1}, 0.0f };
	Material						m_Material = { {1,1,1,1}, {1,1,1,1}, {1,1,1,32}, {0,0,0,0} };
	Material						m_mirrorCubeMaterial = { {0,0,0,1}, {0,0,0,1}, {0,0,0,32}, {1,1,1,0.02f} };
	PBRMaterialCPU					m_DefaultPbrMaterial{};

	// 라이트 마커 위치 / 카메라 기반 기본 행렬
	XMFLOAT3						m_LightPosition = { 4.0f, 4.0f, 0.0f };
	ConstantBuffer					m_baseProjection{};

	// 셰이딩 옵션 / 클리어 컬러
	ShadingMode						m_ShadingMode = ShadingMode::PBR;
	// Outline params ImGui에서 제어하는 용도도
	// Rim 파라미터 제거. 멀티패스 지오메트리 아웃라인만 사용
	float							m_OutlineThickness = 0.08f;
	XMFLOAT4						m_OutlineColor = XMFLOAT4(1.0, 0.7286f, 0, 1);
	float							m_OutlineStrength = 1.0f;
	int								m_EnableNormalMapForCube = 0;
	int								m_UseSpecularMapForCube = 0;
	int								m_UseTextureColor = 1;		// 0: 텍스처 색 무시, 1: 텍스처 색 사용(PBR)
	int								m_LegacyShading = 1;
	XMFLOAT4						m_ClearColor = { 0.125f, 0.125f, 0.125f, 1.0f };

	// 모델 로딩 및 렌더링 FBX/OBJ/PMX
	std::vector<std::unique_ptr<ModelEntry>>		m_Models;            // 모델들
	ID3D11ShaderResourceView*						m_pFallbackWhite = nullptr;
	ID3D11ShaderResourceView*						m_pFallbackNormal = nullptr;
	ID3D11ShaderResourceView*						m_pFallbackBlack = nullptr;
	std::string										m_ModelPathInputUTF8;

	// 경로 기반 공유 모델 캐시
	std::unordered_map<std::wstring, std::weak_ptr<SharedModelData>> m_ModelCache;

	// 본 에디터 관련
	int                           m_SelectedBoneIdx = -1;         // 선택된 본 인덱스
	int                           m_SelectedModelIdx = -1;        // 선택된 모델 인덱스
	int                           m_SelectedStaticMeshIdx = -1;   // 선택된 큐브 인덱스

    // 통합 객체 컨테이너 (상속 기반)
    std::vector<std::unique_ptr<BaseObject>> m_Objects;
    int                           m_SelectedItem = -1;            // 통합 선택 인덱스

	// DockSpace 레이아웃
	bool                          m_EnableDock = false;
	bool                          m_DockBuilt = false;
	ImGuiID                       m_DockMain = 0;
	ImGuiID                       m_DockLeft = 0;
	ImGuiID                       m_DockRight = 0;
	ImGuiID                       m_DockBottom = 0;
	ImGuiID                       m_DockCenter = 0;

	// Bone 필터/옵션
	char                          m_BoneFilter[128] = { 0 };
	bool                          m_BoneExpandAll = false;

	// Console 로그
	std::vector<std::string>      m_LogLines;
	bool                          m_LogAutoScroll = true;
	char                          m_LogFilter[128] = { 0 };

	std::function<void(std::string)> PushLog = [&](const std::string& s) { m_LogLines.push_back(s); };

	// 스폰 운터
	int                          m_SpawnTotal = 0;

	// 씬 이미지 창 관련
	std::wstring                 m_CurrentSceneImagePath = L"..\\Resource\\Image\\SceneA.png";
	ID3D11ShaderResourceView*    m_pSceneImageSRV = nullptr;
	ImVec2                       m_SceneImageSize = ImVec2(0, 0);
	bool                         m_ShowSceneImageWindow = true;
	
	// 씬 변경 팝업 관련
	bool                         m_ShowScenePopup = false;
	float                        m_ScenePopupTimer = 0.0f;
	std::string                  m_ScenePopupMessage;

	// VMD 카메라 상태 (공용)
	mmd::VmdCameraState            m_VmdCamera;

	// HDR 관련 변수
	// Quad를 그려야함
	float m_MonitorMaxNits = 0.0f;
	float m_Exposure = 0.0f;
	bool m_isHDRSupported = false;
	bool m_forceLDR = false;
	DXGI_FORMAT m_format = DXGI_FORMAT_R8G8B8A8_UNORM;

	ID3D11Texture2D* m_pHdrRenderTarget = nullptr;	// 렌더 타겟 텍스처
	ID3D11RenderTargetView* m_pHdrRenderTargetView = nullptr;
	ID3D11ShaderResourceView* m_pHdrShaderResourceView = nullptr;
	ID3D11SamplerState* m_pSamplerLinear = nullptr;			// 선형 필터링 샘플러 상태 객체

	ID3D11VertexShader* m_pQuadVertexShader = nullptr;
	ID3D11PixelShader* m_pPS_ToneMappingLDR = nullptr;
	ID3D11PixelShader* m_pPS_ToneMappingHDR = nullptr;
	ID3D11InputLayout* m_pQuadInputLayout = nullptr;
	ID3D11Buffer* m_pQuadVertexBuffer = nullptr;
	UINT m_QuadVertexBufferStride = 0;
	UINT m_QuadVertexBufferOffset = 0;
	ID3D11Buffer* m_pQuadIndexBuffer = nullptr;
	int m_nQuadIndices = 0;
};

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
	wchar_t file[MAX_PATH] = { 0 };
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

// VMD 카메라용 파일 선택 대화상자
static bool OpenFileDialogVMD(std::wstring& outPath)
{
	wchar_t file[MAX_PATH] = { 0 };
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
	if (!InitD3D()) return false;

	if (!InitBasicEffect()) return false;
	if (!InitSkyBoxEffect()) return false;
	if (!CreateQuad()) return false;

	if (!InitScene()) return false;
	if (!InitImGui()) return false;

	if (!InitTexture()) return false;

	// 값 타입 매니저 사용(동적 할당 없음)

	if (!m_->m_SystemInfo.InitSysInfomation(m_->m_pDevice)) return false;

	AssetManager::Create();

	// ====================================== FMOD 사운드 초기화 ======================================
	if (!Sound::Initialize())
	{
		m_->PushLog("[ERR] FMOD initialize failed");
	}
	else
	{
		m_->PushLog("[OK] FMOD initialized");
	}

	// ====================================== 씬 이미지 초기 로드 ======================================
	LoadSceneImage(m_->m_CurrentSceneImagePath);

	// ====================================== IBL 텍스처 로드 (Sample 세트) ======================================
	//  - BakerSampleDiffuseHDR.dds  : Irradiance(난반사) 맵   → Diffuse IBL
	//  - BakerSampleSpecularHDR.dds : Prefiltered Env 맵      → Specular IBL
	//  - BakerSampleBrdf.dds        : BRDF LUT (NdotV,Roughness → A,B)
	ChangeIBLSkyBox(L"..\\Resource\\Skybox\\Sample\\BakerSample");
	m_->m_SkyBoxChoice = App::Impl::SkyBoxChoice::Baker;

	// ====================================== 3D 모델 ======================================
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\char\\char.fbx"); // 0
	//LoadModelFromFile(L"..\\Resource\\fbx\\Alice_UmaUma.fbx"); // 0
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\sphere.fbx"); // 1
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\sphere.fbx"); // 2
	LoadModelFromFile(L"..\\Resource\\fbx\\Neon.fbx"); // 3
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Ground.fbx"); // 4

	m_->m_Objects.clear();
	for (int mi = 0; mi < (int)m_->m_Models.size(); ++mi)
	{
		auto mo = std::make_unique<ModelObject>(m_->m_Models[mi]->modelName, mi);
		m_->m_Objects.push_back(std::move(mo));
	}
	m_->m_Models[0]->modelShading = ShadingMode::PBR;
	m_->m_Models[1]->modelShading = ShadingMode::PBR;
	m_->m_Models[2]->modelShading = ShadingMode::PBR;
	m_->m_Models[3]->modelShading = ShadingMode::PBR;

	m_->m_Models[0]->pos = XMFLOAT3(0, 0.0f, 0.0f);
	m_->m_Models[1]->pos = XMFLOAT3(130, 50.0f, 0.0f);
	m_->m_Models[2]->pos = XMFLOAT3(-130, 50.0f, 0.0f);
	m_->m_Models[3]->pos = XMFLOAT3(90, 0.0f, 70.0f);
	m_->m_Models[3]->scale = XMFLOAT3(0.5f, 0.5f, 0.5f);

	m_->m_Models[4]->scale = XMFLOAT3(2.0f, 1.0f, 8.0f);
	m_->m_Models[4]->pos = XMFLOAT3(0.0f, -2.0f, 0.0f);
	m_->m_Models[4]->useInstancePbrMaterial = true;
	m_->m_Models[4]->instancePbrMaterial.baseColor = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	m_->m_Models[4]->instancePbrMaterial.metalness = 0.01f;
	m_->m_Models[4]->instancePbrMaterial.roughness = 1.0f;
	m_->m_Models[4]->instancePbrMaterial.ambientOcclusion = 1.0f;

	// ====================================== 큐브 ======================================
	auto co = std::make_unique<CubeObject>(
		L"Cube" + std::to_wstring(1),
		Transform({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }),
		ECubeType::Texture);
	for (int i = 0; i < 6; ++i)
	{
		co->LoadTexture2DAt(m_->m_pDevice, i, L"..\\Resource\\Image\\Bricks059_1K-JPG_Color.jpg");
		co->LoadTextureNormalAt(m_->m_pDevice, i, L"..\\Resource\\Image\\Bricks059_1K-JPG_NormalDX.jpg");
		co->LoadTextureSpecularAt(m_->m_pDevice, i, L"..\\Resource\\Image\\Bricks059_Specular.png");
	}
	m_->m_Objects.push_back(std::move(co));

	auto co2 = std::make_unique<CubeObject>(
		L"Cube" + std::to_wstring(2),
		Transform({ 4.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f }),
		ECubeType::Basic);
	m_->m_Objects.push_back(std::move(co2));

	// ====================================== 카메라 ======================================
	m_Camera.SetPosition(XMFLOAT3(14.0f, 114.0f, -108.0f));
	m_Camera.SetSpeed(150.5f);
	m_Camera.SetRotation(XMFLOAT3(24.0f, -4.5f, 0.0f));

	m_->m_OutlineThickness = 0.3f;
	m_->m_OutlineColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1);

	return true;
}

void App::OnUninitialize()
{
	// FMOD 종료
	Sound::Shutdown();

	// 씬 이미지 리소스 정리
	SAFE_RELEASE(m_->m_pSceneImageSRV);

	// ImGui 종료
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// 씬 리소스 먼저 정리 
	UninitScene();
	UninitD3D();
	AssetManager::Destroy();
}

void App::OnUpdate(const float& dt)
{
	// Shadow light view-projection (directional, orthographic, scene-anchored with texel snapping)
	auto UpdateShadow = [this](XMFLOAT3& focusF) {
		// 모델 중심을 포커스로 사용하여 카메라 움직임과 무관하게 안정화
		XMVECTOR focus = XMLoadFloat3(&focusF);

		// 1. 원본 방향 벡터 로드
		XMVECTOR rawDir = XMLoadFloat3(&m_->m_DirLight.direction);

		// 2. 벡터의 길이(제곱)를 구해서 0에 가까운지 확인 (Epsilon 체크)
		XMVECTOR lenSq = XMVector3LengthSq(rawDir);
		float lenSqVal = XMVectorGetX(lenSq);
		XMVECTOR fwd;
		// 1e-6f는 0에 매우 가까운 작은 수 (FLT_EPSILON 등을 써도 됨)
		if (lenSqVal < 1.0e-6f)
		{
			// 예외 처리: 방향이 0이면 기본값(예: 수직 아래)으로 강제 설정
			fwd = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
		}
		else
		{
			// 정상이면 정규화 수행
			fwd = XMVector3Normalize(rawDir);
		}

		float r = m_->m_ShadowOrthoRadius;
		// 큰 오브젝트에서도 라이트 카메라가 항상 AABB 뒤쪽에 위치하도록 반경만큼 뒤로 물린다
		float backDist = r;
		XMVECTOR lightPos = XMVectorSubtract(focus, XMVectorScale(fwd, backDist));
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);
		if (fabsf(XMVectorGetX(XMVector3Dot(up, fwd))) > 0.99f) up = XMVectorSet(0, 0, 1, 0);
		XMMATRIX LView = XMMatrixLookToLH(lightPos, fwd, up);

		// 라이트 뷰 공간에서 포커스 주변 AABB(±r)를 투영해 동적으로 near/far 계산
		float minZ = 1e9f, maxZ = -1e9f;
		for (int sx = -1; sx <= 1; sx += 2)
			for (int sy = -1; sy <= 1; sy += 2)
				for (int sz = -1; sz <= 1; sz += 2)
				{
					XMVECTOR cornerWS = XMVectorSet(focusF.x + sx * r, focusF.y + sy * r, focusF.z + sz * r, 1.0f);
					XMVECTOR cornerLS = XMVector3TransformCoord(cornerWS, LView);
					float z = XMVectorGetZ(cornerLS);
					minZ = (z < minZ) ? z : minZ;
					maxZ = (z > maxZ) ? z : maxZ;
				}
		// 여유 패딩(5%)을 주고 near는 0.01 이상으로 고정
		float zPad = r * 0.05f;
		float zn = (minZ - zPad);
		if (zn < 0.01f) zn = 0.01f;
		float zf = maxZ + zPad;
		if (zf <= zn) zf = zn + 0.01f;
		XMMATRIX LProj = XMMatrixOrthographicOffCenterLH(-r, r, -r, r, zn, zf);

		// 텍셀 스냅: 라이트 뷰 공간 XY를 섀도우맵 텍셀 그리드에 정렬(near/far에는 영향 없음)
		XMVECTOR focusLS = XMVector3TransformCoord(focus, LView);
		float texelWorld = (2.0f * r) / (float)m_->m_ShadowSize;
		float fx = XMVectorGetX(focusLS);
		float fy = XMVectorGetY(focusLS);
		float snapX = floorf(fx / texelWorld) * texelWorld;
		float snapY = floorf(fy / texelWorld) * texelWorld;
		XMMATRIX snap = XMMatrixTranslation(snapX - fx, snapY - fy, 0.0f);
		LView = snap * LView;

		XMMATRIX LVP = XMMatrixMultiply(LView, LProj);
		m_->m_baseProjection.lightViewProj = XMMatrixTranspose(LVP);
		};


	// 3D 모델 애니메이션 업데이트 (공유 데이터는 중복 업데이트 안함)
	// 동시에 섀도우용 포커스 위치를 누적해서 한 번만 섀도우 행렬 계산
	{
		std::unordered_set<SharedModelData*> updated;
		const int totalModels = (int)m_->m_Models.size();
		// 애니메이션 업데이트 간격. 모델이 많을수록 업데이트 주기를 늘려 CPU 부하를 제한.
		float animStep = 1.0f / 120.0f;
		if (totalModels > 60)  animStep = 1.0f / 60.0f;
		if (totalModels > 120) animStep = 1.0f / 30.0f;

		// 섀도우용 씬 중심 누적
		XMFLOAT3 sceneCenter = { 0.0f, 0.0f, 0.0f };
		int      sceneModelCount = 0;

		for (auto& mdlPtr : m_->m_Models)
		{
			auto& mdl = *mdlPtr;
			if (mdl.autoRotate)
			{
				mdl.rotDeg.y += 45.0f * dt;
				mdl.rotDeg.y = std::fmod(mdl.rotDeg.y + 180.0f, 360.0f) - 180.0f;
			}
			if (!mdl.shared) continue;
			if (updated.find(mdl.shared.get()) != updated.end()) continue;

			// 섀도우용 씬 중심 누적
			sceneCenter.x += mdl.pos.x;
			sceneCenter.y += mdl.pos.y;
			sceneCenter.z += mdl.pos.z;
			sceneModelCount++;

			if (mdl.source == ModelSource::FBX && mdl.shared->fbx)
			{
				// 인스턴스별 애니메이션 업데이트 (공유 지오메트리/스켈레톤 사용)
				if (!mdl.animatorInited)
				{
					mdl.animator.InitMetadata(mdl.shared->fbx->GetScenePtr());
					mdl.animator.SetSharedContext(
						mdl.shared->fbx->GetScenePtr(),
						mdl.shared->fbx->GetNodeIndexOfName(),
						&mdl.shared->fbx->GetBoneNames(),
						&mdl.shared->fbx->GetBoneOffsets(),
						&mdl.shared->fbx->GetGlobalInverse());
					auto t = mdl.shared->fbx->GetCurrentAnimationType();
					mdl.animator.SetType(t == FbxModel::AnimationType::Rigid ? FbxAnimation::AnimType::Rigid : (t == FbxModel::AnimationType::Skinned ? FbxAnimation::AnimType::Skinned : FbxAnimation::AnimType::None));
					mdl.animatorInited = true;
				}
				// 애니메이션 LOD: 누적 시간 기반으로 일정 주기마다만 팔레트 평가/업로드
				mdl.animUpdateAccum += dt;
				if (mdl.animUpdateAccum >= animStep)
				{
					const double dtAnim = (double)mdl.animUpdateAccum;
					mdl.animUpdateAccum = 0.0f;

					mdl.animator.SetPlaying(mdl.uiAnimPlaying);
					mdl.animator.EnsureBoneCB(m_->m_pDevice, 1023);
					mdl.animator.UpdateAndUpload(
						m_->m_pDeviceContext,
						dtAnim,
						mdl.shared->fbx->GetScenePtr(),
						mdl.shared->fbx->GetNodeIndexOfName(),
						mdl.shared->fbx->GetBoneNames(),
						mdl.shared->fbx->GetBoneOffsets(),
						mdl.shared->fbx->GetGlobalInverse());
				}
			}
			else if (mdl.source == ModelSource::PMX && mdl.shared->pmx)
			{
				// PMX도 적용 팔레트이므로 1회만 업데이트
				auto it = updated.find(mdl.shared.get());
				if (it == updated.end())
				{
					mdl.animUpdateAccum += dt;
					if (mdl.animUpdateAccum >= animStep)
					{
						const double dtAnim = (double)mdl.animUpdateAccum;
						mdl.animUpdateAccum = 0.0f;
						mdl.shared->pmx->UpdateAnimation(m_->m_pDeviceContext, dtAnim);
						updated.insert(mdl.shared.get());
					}
				}
			}

	// FMOD 갱신 (매 프레임)
	Sound::Update();
		}

		// 섀도우 행렬은 씬 전체에 대해 한 번만 계산 (성능 최적화)
		if (sceneModelCount > 0)
		{
			sceneCenter.x /= (float)sceneModelCount;
			sceneCenter.y /= (float)sceneModelCount;
			sceneCenter.z /= (float)sceneModelCount;
			UpdateShadow(sceneCenter);
		}
		else
		{
			// 모델이 없을 경우 기본 위치 사용
			XMFLOAT3 defaultPos = { 0.0f, 0.0f, 0.0f };
			UpdateShadow(defaultPos);
		}
	}

	// Camera의 View/Proj 
	XMMATRIX model = XMMatrixIdentity();
	m_->m_baseProjection.world = XMMatrixTranspose(model);

	// VMD 카메라 업데이트 및 적용
	mmd::UpdateVmdCamera(dt, m_->m_VmdCamera);
	XMMATRIX view{}, proj{};
	XMFLOAT3 eye{};
	bool useVmdCam = mmd::EvaluateVmdCamera(
		m_->m_VmdCamera,
		m_Camera.GetFovYRad(),
		AspectRatio(),
		m_Camera.GetNearZ(),
		m_Camera.GetFarZ(),
		view, proj, eye);

	if (useVmdCam)
	{
		m_->m_baseProjection.view = XMMatrixTranspose(view);
		m_->m_baseProjection.proj = XMMatrixTranspose(proj);
		m_->m_baseProjection.eyePos = eye;
	}
	else
	{
		m_->m_baseProjection.view = XMMatrixTranspose(m_Camera.GetViewMatrixXM());
		m_->m_baseProjection.proj = XMMatrixTranspose(m_Camera.GetProjMatrixXM());
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

	// 씬 변경 팝업 타이머 업데이트
	if (m_->m_ShowScenePopup)
	{
		m_->m_ScenePopupTimer -= dt;
		if (m_->m_ScenePopupTimer <= 0.0f)
		{
			m_->m_ShowScenePopup = false;
			m_->m_ScenePopupTimer = 0.0f;
		}
	}

	if (ImGui::IsKeyPressed(ImGuiKey_F6, true))
	{
		TrimVideoMemory(); // 전환 직전 Trim
		if (m_SceneIndex == 0)
		{
			m_->m_SpawnTotal = 0;
			m_->PushLog("[OK] Cleared all model instances");
			LoadSceneImage(m_->m_CurrentSceneImagePath);
			// 팝업 표시
			m_->m_ShowScenePopup = true;
			m_->m_ScenePopupTimer = 2.0f;
			m_->m_ScenePopupMessage = Utf8FromWString(L"안녕하세요 토끼씨!");
		}
		else
		{
			m_->m_SpawnTotal = 0;
			m_->PushLog("[Scene Change] Change Scane to <SceneA>");
			m_SceneIndex = 0;
			// 씬 이미지 경로 변경
			m_->m_CurrentSceneImagePath = L"..\\Resource\\Image\\SceneA.png";
			LoadSceneImage(m_->m_CurrentSceneImagePath);
			// 팝업 표시
			m_->m_ShowScenePopup = true;
			m_->m_ScenePopupTimer = 2.0f;
			m_->m_ScenePopupMessage = Utf8FromWString(L"기뻐요 토끼씨!");
		}
	}

	// ====================================== 간단 마우스 피킹 (FBX/모델 위주) ======================================
	if (InputSystem::Instance && !ImGui::GetIO().WantCaptureMouse)
	{
		auto& input = *InputSystem::Instance;
		if (input.m_MouseStateTracker.leftButton == Mouse::ButtonStateTracker::PRESSED)
		{
			PickingRay ray = PickingRay::ScreenPointToRay(
				m_Camera,
				(float)input.m_MouseState.x,
				(float)input.m_MouseState.y,
				(float)m_ClientWidth,
				(float)m_ClientHeight);

			int   pickedModel = -1;
			float bestT = FLT_MAX;

			for (auto it = m_->m_Models.begin(); it != m_->m_Models.end(); ++it)
			{
				auto& mdl = **it;

				XMFLOAT3 mn = mdl.boundsMin, mx = mdl.boundsMax;
				XMFLOAT3 bmin{}, bmax{};

				// 로컬 AABB(min,max)에 스케일과 위치를 그대로 적용해서 월드 AABB를 만든다 (회전은 무시)
				float x0 = mn.x * mdl.scale.x + mdl.pos.x;
				float x1 = mx.x * mdl.scale.x + mdl.pos.x;
				bmin.x = std::min(x0, x1);
				bmax.x = (std::max)(x0, x1);

				float y0 = mn.y * mdl.scale.y + mdl.pos.y;
				float y1 = mx.y * mdl.scale.y + mdl.pos.y;
				bmin.y = std::min(y0, y1);
				bmax.y = (std::max)(y0, y1);

				float z0 = mn.z * mdl.scale.z + mdl.pos.z;
				float z1 = mx.z * mdl.scale.z + mdl.pos.z;
				bmin.z = std::min(z0, z1);
				bmax.z = (std::max)(z0, z1);

				float t;
				if (ray.HitAABB(bmin, bmax, t) && t < bestT)
				{
					bestT = t;
					pickedModel = (int)(it - m_->m_Models.begin());
				}
			}

			if (pickedModel >= 0)
			{
				m_->m_SelectedModelIdx = pickedModel;
				auto itObj = std::find_if(
					m_->m_Objects.begin(), m_->m_Objects.end(),
					[pickedModel](const std::unique_ptr<BaseObject>& up)
					{
						if (!up || up->kind != ObjectKind::Model) return false;
						return static_cast<ModelObject*>(up.get())->modelIndex == pickedModel;
					});
				if (itObj != m_->m_Objects.end()) m_->m_SelectedItem = (int)std::distance(m_->m_Objects.begin(), itObj);
				else m_->m_SelectedItem = -1;
			}
		}
	}
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
	float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	UINT stride = m_->m_VertextBufferStride;	// 바이트 수
	UINT offset = m_->m_VertextBufferOffset;

	//m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pRenderTargetView, color);
	if (m_->m_pHdrRenderTargetView)
	{
		// 렌더 타겟을 HDR 텍스처로 설정. 이후 호출되는 모든 Draw 함수는 자동으로 이 텍스처에 그려집니다.
		float clr[4] = { m_->m_ClearColor.x, m_->m_ClearColor.y, m_->m_ClearColor.z, m_->m_ClearColor.w };
		m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pHdrRenderTargetView, clr);
		m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pHdrRenderTargetView, m_->m_pDepthStencilView);
	}
	else
	{
		// 만약 HDR 리소스가 없다면 기존대로 설정.
		m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pRenderTargetView, color);
	}

	m_->m_pDeviceContext->ClearDepthStencilView(m_->m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	m_->m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	
	// 컬러 클리어 및 스카이박스/배경 선택
	if (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Off)
	{
		float clr[4] = { m_->m_ClearColor.x, m_->m_ClearColor.y, m_->m_ClearColor.z, m_->m_ClearColor.w };
		m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pRenderTargetView, clr);
	}

	// ====================================== Shadow Pass (depth-only) ======================================
	if (m_->m_ShadowEnabled && !m_->m_Models.empty() && m_->m_pShadowDSV)
	{
		// SRV/DSV 동일 리소스(섀도우 맵)를 DSV로 세팅하기 전에
		// PS에서 사용 중인 SRV(t4)를 명시적으로 언바인드한다.
		ID3D11ShaderResourceView* nullSRV = nullptr;
		m_->m_pDeviceContext->PSSetShaderResources(4, 1, &nullSRV);

		// Save viewport
		UINT oldCount = 1; D3D11_VIEWPORT oldVP{}; m_->m_pDeviceContext->RSGetViewports(&oldCount, &oldVP);
		m_->m_pDeviceContext->RSSetViewports(1, &m_->m_ShadowViewport);
		ID3D11RenderTargetView* nullRTV = nullptr;
		m_->m_pDeviceContext->OMSetRenderTargets(0, &nullRTV, m_->m_pShadowDSV);
		m_->m_pDeviceContext->ClearDepthStencilView(m_->m_pShadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
		if (m_->RSShadowBias) m_->m_pDeviceContext->RSSetState(m_->RSShadowBias);
		// PS off
		m_->m_pDeviceContext->PSSetShader(nullptr, nullptr, 0);

		for (auto& mdlPtr : m_->m_Models)
		{
			if (!mdlPtr->shared || !mdlPtr->shared->vb || !mdlPtr->shared->ib) continue;
			UINT s = mdlPtr->shared->stride; UINT o = 0;
			m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &mdlPtr->shared->vb, &s, &o);
			m_->m_pDeviceContext->IASetIndexBuffer(mdlPtr->shared->ib, DXGI_FORMAT_R32_UINT, 0);

			// Choose VS
			bool hasSkeleton = false; ID3D11Buffer* cbBones = nullptr;
			if (mdlPtr->source == ModelSource::FBX && mdlPtr->shared && mdlPtr->shared->fbx)
			{
				hasSkeleton = mdlPtr->shared->fbx->HasSkeleton(); cbBones = mdlPtr->animator.GetBoneCB();
			}
			else if (mdlPtr->source == ModelSource::PMX && mdlPtr->shared && mdlPtr->shared->pmx)
			{
				hasSkeleton = mdlPtr->shared->pmx->HasSkeleton(); cbBones = mdlPtr->shared->pmx->GetBoneConstantBuffer();
			}

			bool useSkinnedShadow = hasSkeleton
				&& (mdlPtr->shared && (mdlPtr->shared->stride == sizeof(VertexSkinnedTBN)))
				&& (m_->m_pInputLayoutSkinned != nullptr)
				&& (m_->m_pVSSkinnedShadow != nullptr)
				&& (cbBones != nullptr);
			if (useSkinnedShadow)
			{
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayoutSkinned);
				m_->m_pDeviceContext->VSSetShader(m_->m_pVSSkinnedShadow, nullptr, 0);
				if (cbBones) m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &cbBones);
			}
			else
			{
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayout);
				m_->m_pDeviceContext->VSSetShader(m_->m_pVSShadow, nullptr, 0);
				ID3D11Buffer* nullCB = nullptr; m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &nullCB);
			}

			// 회전 행렬 생성
			XMMATRIX S = XMMatrixScaling(mdlPtr->scale.x, mdlPtr->scale.y, mdlPtr->scale.z);
			XMMATRIX R = XMMatrixRotationQuaternion(XMQuaternionRotationRollPitchYaw(
				XMConvertToRadians(mdlPtr->rotDeg.x),
				XMConvertToRadians(mdlPtr->rotDeg.y),
				XMConvertToRadians(mdlPtr->rotDeg.z)
			));
			XMMATRIX T = XMMatrixTranslation(mdlPtr->pos.x, mdlPtr->pos.y, mdlPtr->pos.z);
			XMMATRIX W = S * R * T;

			// Fill CB
			ConstantBuffer cb = m_->m_ConstantBuffer;
			cb.world = XMMatrixTranspose(W);
			cb.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
			cb.lightViewProj = m_->m_baseProjection.lightViewProj;
			cb.shadowBias = m_->m_ShadowBias;
			cb.shadowMapSize = (float)m_->m_ShadowSize;
			cb.shadowPCFRadius = m_->m_ShadowPCFRadius;
			cb.shadowEnabled = m_->m_ShadowEnabled;
			D3D11_MAPPED_SUBRESOURCE mapped{};
			HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
			memcpy(mapped.pData, &cb, sizeof(ConstantBuffer));
			m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
			m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
			for (const auto& sub : mdlPtr->shared->subsets) m_->m_pDeviceContext->DrawIndexed(sub.count, sub.start, 0);
		}

		// Restore
		m_->m_pDeviceContext->RSSetState(nullptr);
		m_->m_pDeviceContext->RSSetViewports(1, &oldVP);
		//m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pRenderTargetView, m_->m_pDepthStencilView);
		m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pHdrRenderTargetView, m_->m_pDepthStencilView);
	}

	/// ====================================== 카메라 ======================================
	m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShader, nullptr, 0);
	m_->m_pDeviceContext->PSSetShader(m_->m_pPixelShader, nullptr, 0);
	m_->m_ConstantBuffer.world = m_->m_baseProjection.world;
	m_->m_ConstantBuffer.view = m_->m_baseProjection.view;
	m_->m_ConstantBuffer.proj = m_->m_baseProjection.proj;
	// Shadow params to CB
	m_->m_ConstantBuffer.lightViewProj = m_->m_baseProjection.lightViewProj;
	m_->m_ConstantBuffer.shadowBias = m_->m_ShadowBias;
	m_->m_ConstantBuffer.shadowMapSize = (float)m_->m_ShadowSize;
	m_->m_ConstantBuffer.shadowPCFRadius = m_->m_ShadowPCFRadius;
	m_->m_ConstantBuffer.shadowEnabled = m_->m_ShadowEnabled;
	// 비균등 스케일을 해결한 코드. 역전치 곱하기
	auto invWorlNormal = XMMatrixInverse(nullptr, m_->m_baseProjection.world);
	m_->m_ConstantBuffer.worldInvTranspose = XMMatrixTranspose(invWorlNormal);

	// 기본 광원/카메라 UI 반영
	XMFLOAT3 lightDir = m_->m_DirLight.direction;
	XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&lightDir));
	XMStoreFloat3(&lightDir, v);
	// DirectionalLight 필드 대입 정규화된 방향
	m_->m_ConstantBuffer.dirLight = m_->m_DirLight;
	m_->m_ConstantBuffer.dirLight.direction = lightDir;
	m_->m_ConstantBuffer.dirLight.pad = 0.0f;
	m_->m_ConstantBuffer.eyePos = m_Camera.GetPosition();
	m_->m_ConstantBuffer.pad = 0.0f;
	m_->m_ConstantBuffer.useTextureColor = m_->m_UseTextureColor;
	// skipGammaCorrection은 HDR RT 렌더링 여부에 따라 OnRender() 시작 부분에서 설정됨
	const PBRMaterialCPU& defPbr = m_->m_DefaultPbrMaterial;
	m_->m_ConstantBuffer.pbrBaseColor = defPbr.baseColor;
	m_->m_ConstantBuffer.pbrMetalness = defPbr.metalness;
	m_->m_ConstantBuffer.pbrRoughness = defPbr.roughness;
	m_->m_ConstantBuffer.pbrAO = defPbr.ambientOcclusion;
	// 셰이딩 모드 전달 (맵 플래그는 오브젝트별로 설정)
	m_->m_ConstantBuffer.shadingMode = (int)m_->m_ShadingMode;
	m_->m_ConstantBuffer.enableNormalMap = 0;
	m_->m_ConstantBuffer.useSpecularMap = 0;
	m_->m_ConstantBuffer.useDiffuseMap = 1;
	// Outline params 업데이트
	m_->m_ConstantBuffer.outlineThickness = m_->m_OutlineThickness;
	m_->m_ConstantBuffer.outlineColor = m_->m_OutlineColor;
	m_->m_ConstantBuffer.outlineStrength = m_->m_OutlineStrength;
	// 머티리얼 채우기
	m_->m_ConstantBuffer.material = m_->m_Material;

	D3D11_MAPPED_SUBRESOURCE mappedData;
	HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedData));
	memcpy_s(mappedData.pData, sizeof(ConstantBuffer), &m_->m_ConstantBuffer, sizeof(ConstantBuffer));
	m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
	m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetSamplers(0, 1, &m_->m_pSamplerState);
	// Bind shadow sampler/SRV
	if (m_->m_pShadowSampler) m_->m_pDeviceContext->PSSetSamplers(1, 1, &m_->m_pShadowSampler);
	if (m_->m_pShadowSRV)     m_->m_pDeviceContext->PSSetShaderResources(4, 1, &m_->m_pShadowSRV);
    // 큐브맵을 t1 슬롯에 바인딩 (픽셀 셰이더에서 g_TexCube : t1)
    m_->m_pDeviceContext->PSSetShaderResources(1, 1, &m_->m_pTextureSRV);
	// IBL 텍스처를 고정 슬롯에 바인딩
	if (m_->m_pIblDiffuseSRV)   m_->m_pDeviceContext->PSSetShaderResources(5, 1, &m_->m_pIblDiffuseSRV);   // g_IBL_Diffuse
	if (m_->m_pIblSpecularSRV)  m_->m_pDeviceContext->PSSetShaderResources(6, 1, &m_->m_pIblSpecularSRV);  // g_IBL_Specular
	if (m_->m_pIblBrdfLutSRV)   m_->m_pDeviceContext->PSSetShaderResources(7, 1, &m_->m_pIblBrdfLutSRV);   // g_IBL_BRDF_LUT

    /// ====================================== 큐브 ======================================
    for (auto& objPtr : m_->m_Objects)
    {
        if (!objPtr || objPtr->kind != ObjectKind::Cube) continue;
        auto* cubeObj = static_cast<CubeObject*>(objPtr.get());
        const Transform& mc = cubeObj->cubeTransform;

        // IA 준비
        m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pVertexBuffer, &stride, &offset);
        m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayout);
        m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		// 월드 행렬 세팅 + CB 업로드
		ConstantBuffer cb = m_->m_ConstantBuffer;
        XMMATRIX Sm = XMMatrixScaling(mc.scale.x, mc.scale.y, mc.scale.z);
		XMMATRIX Rm = XMMatrixRotationQuaternion(XMQuaternionRotationRollPitchYaw(
			XMConvertToRadians(mc.rotationDeg.x),
			XMConvertToRadians(mc.rotationDeg.y),
			XMConvertToRadians(mc.rotationDeg.z)
		));
        XMMATRIX Tm = XMMatrixTranslation(mc.position.x, mc.position.y, mc.position.z);
        XMMATRIX W = Sm * Rm * Tm;
        cb.world = XMMatrixTranspose(W);
        cb.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
        // 큐브 개별 머티리얼 적용
        cb.material.ambient = cubeObj->matAmbient;
        cb.material.diffuse = cubeObj->matDiffuse;
        cb.material.specular = cubeObj->matSpecular;
        cb.material.reflect = cubeObj->matReflect;
		cb.pad = 0.0f;
		cb.shadingMode = (int)m_->m_ShadingMode;
		// 큐브는 텍스처가 없어도 머티리얼 색으로 그려지도록 맵 사용 비활성화
		cb.enableNormalMap = (cubeObj->useNormalMap != 0) ? 1 : 0;
		cb.useSpecularMap = (cubeObj->useSpecularMap != 0) ? 1 : 0;
		const PBRMaterialCPU& cubePbr = m_->m_DefaultPbrMaterial;
		cb.pbrBaseColor = cubePbr.baseColor;
		cb.pbrMetalness = cubePbr.metalness;
		cb.pbrRoughness = cubePbr.roughness;
		cb.pbrAO = cubePbr.ambientOcclusion;

        // 면별 텍스처 유무에 따라 useDiffuseMap 업데이트 후 CB 업로드 및 드로우
		for (int face = 0; face < 6; ++face)
		{
            bool isTexCube = (cubeObj->cubeType == ECubeType::Texture);
            ID3D11ShaderResourceView* srvDiffuse = cubeObj->faceSRV[face] ? cubeObj->faceSRV[face] : m_->m_pFallbackWhite;
            ID3D11ShaderResourceView* srvNormal = (isTexCube && cubeObj->useNormalMap != 0) ? (cubeObj->normalSRV[face] ? cubeObj->normalSRV[face] : m_->m_pFallbackNormal) : nullptr;
            ID3D11ShaderResourceView* srvSpec = (isTexCube && cubeObj->useSpecularMap != 0) ? (cubeObj->specSRV[face] ? cubeObj->specSRV[face] : m_->m_pFallbackWhite) : nullptr;

            cb.useDiffuseMap = (cubeObj->faceSRV[face] != nullptr) ? 1 : 0;
            cb.enableNormalMap = (isTexCube && cubeObj->useNormalMap != 0) ? 1 : 0;
            cb.useSpecularMap = (isTexCube && cubeObj->useSpecularMap != 0) ? 1 : 0;

			D3D11_MAPPED_SUBRESOURCE mapped;
			HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
			memcpy_s(mapped.pData, sizeof(ConstantBuffer), &cb, sizeof(ConstantBuffer));
			m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
			m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
			m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
			m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
			m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
			m_->m_pDeviceContext->PSSetShaderResources(3, 1, &srvSpec);
			m_->m_pDeviceContext->DrawIndexed(6, face * 6, 0);
		}
    }

	/// ====================================== 3D 모델 ======================================
	if (!m_->m_Models.empty())
	{
		// 모든 모델 렌더
		for (auto& mdlPtr : m_->m_Models)
		{
			// IA 바인딩
			UINT s = mdlPtr->shared ? mdlPtr->shared->stride : 0; UINT o = 0;
			if (!mdlPtr->shared || !mdlPtr->shared->vb || !mdlPtr->shared->ib) continue;
			m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &mdlPtr->shared->vb, &s, &o);
			// FBX/PMX 스켈레톤 여부 확인
			ID3D11Buffer* cbBones = nullptr;
			bool hasSkeleton = false;
			if (mdlPtr->source == ModelSource::FBX && mdlPtr->shared && mdlPtr->shared->fbx)
			{
				cbBones = mdlPtr->animator.GetBoneCB();
				hasSkeleton = mdlPtr->shared->fbx->HasSkeleton();
			}
			else if (mdlPtr->source == ModelSource::PMX && mdlPtr->shared && mdlPtr->shared->pmx)
			{
				cbBones = mdlPtr->shared->pmx->GetBoneConstantBuffer();
				hasSkeleton = mdlPtr->shared->pmx->HasSkeleton();
			}
			bool useSkinned = hasSkeleton
				&& (mdlPtr->shared && (mdlPtr->shared->stride == sizeof(VertexSkinnedTBN)))
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
					if (mdlPtr->source == ModelSource::PMX && mdlPtr->shared && mdlPtr->shared->pmx && !mdlPtr->shared->pmx->HasAnimations())
					{
						mdlPtr->shared->pmx->UploadIdentityPalette(m_->m_pDeviceContext);
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
			if (mdlPtr->shared) m_->m_pDeviceContext->IASetIndexBuffer(mdlPtr->shared->ib, DXGI_FORMAT_R32_UINT, 0);

			// 월드 행렬 per-model
			XMMATRIX S = XMMatrixScaling(mdlPtr->scale.x, mdlPtr->scale.y, mdlPtr->scale.z);
			XMMATRIX R = XMMatrixRotationQuaternion(XMQuaternionRotationRollPitchYaw(
				XMConvertToRadians(mdlPtr->rotDeg.x),
				XMConvertToRadians(mdlPtr->rotDeg.y),
				XMConvertToRadians(mdlPtr->rotDeg.z)
			));
			XMMATRIX T = XMMatrixTranslation(mdlPtr->pos.x, mdlPtr->pos.y, mdlPtr->pos.z);
			XMMATRIX W = S * R * T;

			ConstantBuffer cb = m_->m_ConstantBuffer;
			cb.world = XMMatrixTranspose(W);
			cb.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
			cb.material = (mdlPtr->useInstanceMaterial ? mdlPtr->instanceMaterial : m_->m_Material);
			cb.shadingMode = (int)mdlPtr->modelShading;
			cb.enableNormalMap = m_->m_EnableNormalMapForCube;
			cb.useSpecularMap = m_->m_UseSpecularMapForCube;
			const PBRMaterialCPU* activePbrPtr = nullptr;
			if (mdlPtr->useInstancePbrMaterial) activePbrPtr = &mdlPtr->instancePbrMaterial;
			else activePbrPtr = &m_->m_DefaultPbrMaterial;
			const PBRMaterialCPU& activePbr = *activePbrPtr;
			cb.pbrBaseColor = activePbr.baseColor;
			cb.pbrMetalness = activePbr.metalness;
			cb.pbrRoughness = activePbr.roughness;
			cb.pbrAO = activePbr.ambientOcclusion;

			D3D11_MAPPED_SUBRESOURCE mapped;
			HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
			memcpy_s(mapped.pData, sizeof(ConstantBuffer), &cb, sizeof(ConstantBuffer));
			m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
			m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
			m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

			// ====================================== 서브셋 텍스처 및 드로우 ======================================
			for (const auto& sub : (mdlPtr->shared ? mdlPtr->shared->subsets : std::vector<ModelSubset>{}))
			{
				ID3D11ShaderResourceView* srvDiffuse = nullptr;
				if (mdlPtr->shared && sub.materialIndex < mdlPtr->shared->materialSRVs.size())
					srvDiffuse = mdlPtr->shared->materialSRVs[sub.materialIndex];
				if (!srvDiffuse) srvDiffuse = m_->m_pFallbackWhite;
				ID3D11ShaderResourceView* srvNormal = nullptr;
				if (m_->m_EnableNormalMapForCube != 0)
				{
					if (mdlPtr->shared && sub.materialIndex < mdlPtr->shared->normalSRVs.size())
					{
						srvNormal = mdlPtr->shared->normalSRVs[sub.materialIndex];
					}
					if (!srvNormal) srvNormal = m_->m_pFallbackNormal;
				}
				ID3D11ShaderResourceView* srvSpec = (m_->m_UseSpecularMapForCube != 0) ? m_->m_pFallbackWhite : nullptr;
				m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
				m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
				m_->m_pDeviceContext->PSSetShaderResources(3, 1, &srvSpec);
				m_->m_pDeviceContext->DrawIndexed(sub.count, sub.start, 0);
			}
		}
	}

	// 스키닝 VS가 남아있을 수 있으므로 안전하게 기본 VS로 강제 전환하고 b1 해제
	m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShader, nullptr, 0);
	ID3D11Buffer* nullCB = nullptr;
	m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &nullCB);

	// ====================================== SkyBox ======================================
	// SkyBox 렌더링 (상태 보존/복구)
	if (m_->m_SkyBoxChoice != App::Impl::SkyBoxChoice::Off && m_->m_Skybox)
	{
		UINT stride = m_->m_VertextBufferStride;
		UINT offset = m_->m_VertextBufferOffset;
		m_->m_Skybox->Render(m_->m_pDeviceContext, m_->m_pVertexBuffer, m_->m_pIndexBuffer, m_->m_nIndices, stride, offset, m_->m_baseProjection.view, m_->m_baseProjection.proj);
	}

	// ====================================== SkyBox 화면 미리보기 ======================================
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
		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &overlayCB, sizeof(ConstantBuffer));
		m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
		m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
		m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
		// 좌상단 작은 축 표시
		m_->m_LineRenderer->DrawAxesOverlay(m_->m_pDeviceContext, XMMatrixTranspose(m_->m_baseProjection.view), DirectX::XMFLOAT2(-0.9f, 0.85f), 0.08f, m_->m_pLineInputLayout, m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);
	}

	// ====================================== Tone Mapping ======================================
	// 34_Shared.fxh에서 g_SceneHDR은 register(t8), g_SamplerLinear는 register(s2)로 선언됨
	// 메인 렌더링의 t0~t7과 충돌을 피하기 위해 t8/s2를 사용
	ID3D11ShaderResourceView* nullSRV_First[1] = { nullptr };
	m_->m_pDeviceContext->PSSetShaderResources(8, 1, nullSRV_First);

	// 2. 렌더 타겟을 '화면(BackBuffer)'으로 변경
	// 깊이 버퍼(DSV)는 필요 없으므로 nullptr로 설정하여 깊이 테스트를 끕니다.
	ID3D11RenderTargetView* nullRTV = nullptr;
	m_->m_pDeviceContext->OMSetRenderTargets(1, &nullRTV, nullptr); // 기존 타겟 해제
	m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pRenderTargetView, nullptr); // 백버퍼 연결

	// 3. 화면 클리어
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pRenderTargetView, color);

	// 4. 톤 매핑을 위한 상수 버퍼(Constant Buffer) 업데이트
	{
		ConstantBuffer cb = m_->m_ConstantBuffer; // 기존 값 복사

		// Quad는 2D이므로 월드/뷰/프로젝션 행렬은 Identity로
		cb.world = XMMatrixIdentity();
		cb.view = XMMatrixIdentity();
		cb.proj = XMMatrixIdentity();

		// 톤 매핑 셰이더가 사용하는 파라미터 주입 
		// 셰이더 안에서 (Color * Exposure) 연산을 하는데 Exposure가 0이면 검은색이 됨.
		cb.g_Exposure = m_->m_Exposure;                // 노출값 (예: 1.0 ~ 2.0)
		cb.g_MaxHDRNits = m_->m_MonitorMaxNits;       // 모니터 최대 밝기 (예: 1000.0f)

		// GPU로 업로드
		D3D11_MAPPED_SUBRESOURCE mapped;
		HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
		memcpy_s(mapped.pData, sizeof(ConstantBuffer), &cb, sizeof(ConstantBuffer));
		m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);

		// VS와 PS에 버퍼 연결
		m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
		m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	}

	// 5. Quad 그리기 설정 (Input Layout, Vertex Buffer 등)
	m_->m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pQuadVertexBuffer, &m_->m_QuadVertexBufferStride, &m_->m_QuadVertexBufferOffset);
	m_->m_pDeviceContext->IASetInputLayout(m_->m_pQuadInputLayout);
	m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pQuadIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	m_->m_pDeviceContext->VSSetShader(m_->m_pQuadVertexShader, nullptr, 0);

	// 6. 셰이더 선택 (HDR vs LDR)
	switch (m_->m_format)
	{
	case DXGI_FORMAT_R10G10B10A2_UNORM:
		m_->m_pDeviceContext->PSSetShader(m_->m_pPS_ToneMappingHDR, nullptr, 0);
		break;
	default:
		m_->m_pDeviceContext->PSSetShader(m_->m_pPS_ToneMappingLDR, nullptr, 0);
		break;
	}

	// 7. 텍스처(SRV)와 샘플러 바인딩
	// 7. 텍스쳐, 샘플러를 바인딩 g_SceneHDR : register(t8), g_SamplerLinear : register(s2)
	m_->m_pDeviceContext->PSSetShaderResources(8, 1, &m_->m_pHdrShaderResourceView);
	m_->m_pDeviceContext->PSSetSamplers(2, 1, &m_->m_pSamplerLinear);

	// 8. 그리기
	m_->m_pDeviceContext->DrawIndexed(m_->m_nQuadIndices, 0, 0);

	// 9. 다음 프레임을 위해 SRV 해제
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	m_->m_pDeviceContext->PSSetShaderResources(8, 1, nullSRV);


	// ====================================== ImGui ======================================
	// ImGui 프레임 및 UI 렌더링
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	RenderControlPannel();
	RenderSceneCollection();
	RenderModelPannel();
	RenderConsolPannel();
	m_->m_SystemInfo.RenderUI();
	RenderSceneImageWindow();

	ImGui::Render();

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	m_->m_pSwapChain->Present(0, 0);
}

bool App::InitD3D()
{
	// HDR 지원 여부를 확인함
	DXGI_FORMAT result;
	m_->m_isHDRSupported = CheckHDRSupportAndGetMaxNits(m_->m_MonitorMaxNits, result);

	if (!m_->m_forceLDR && m_->m_isHDRSupported)
		CreateSwapChainAndBackBuffer(DXGI_FORMAT_R10G10B10A2_UNORM); // HDR
	else
		CreateSwapChainAndBackBuffer(DXGI_FORMAT_R8G8B8A8_UNORM); // LDR

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

	// Outline용: 깊이 읽기 전용(LESS_EQUAL), 깊이 쓰기 금지
	dssDesc = {};
	dssDesc.DepthEnable = TRUE;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	dssDesc.StencilEnable = FALSE;
	HR_T(m_->m_pDevice->CreateDepthStencilState(&dssDesc, &m_->m_pDepthStencilStateReadOnly));

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
	// Outline용 Front Cull (백페이스 표시)
	rasterizerDesc.CullMode = D3D11_CULL_FRONT;
	rasterizerDesc.FrontCounterClockwise = true;
	HR_T(m_->m_pDevice->CreateRasterizerState(&rasterizerDesc, &m_->RSCullFront));

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

	// ====================================== HDR Render Target 생성 ======================================
	// HDR 씬 렌더링을 위한 R16G16B16A16_FLOAT 포맷의 렌더 타겟 생성
	// 이 텍스처는 1.0을 넘는 HDR 값을 저장할 수 있으며, 이후 톤매핑 패스에서 샘플링됨
	D3D11_TEXTURE2D_DESC hdrRTDesc = {};
	hdrRTDesc.Width = static_cast<UINT>(m_ClientWidth);
	hdrRTDesc.Height = static_cast<UINT>(m_ClientHeight);
	hdrRTDesc.MipLevels = 1;
	hdrRTDesc.ArraySize = 1;
	hdrRTDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR 값을 저장할 수 있는 포맷
	hdrRTDesc.SampleDesc.Count = 1;   // MSAA 없음
	hdrRTDesc.SampleDesc.Quality = 0;
	hdrRTDesc.Usage = D3D11_USAGE_DEFAULT;
	hdrRTDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // RTV와 SRV 모두 가능

	HR_T(m_->m_pDevice->CreateTexture2D(&hdrRTDesc, nullptr, &m_->m_pHdrRenderTarget));
	HR_T(m_->m_pDevice->CreateRenderTargetView(m_->m_pHdrRenderTarget, nullptr, &m_->m_pHdrRenderTargetView));
	HR_T(m_->m_pDevice->CreateShaderResourceView(m_->m_pHdrRenderTarget, nullptr, &m_->m_pHdrShaderResourceView));

	// ====================================== 톤매핑용 선형 샘플러 생성 ======================================
	// 톤매핑 패스에서 HDR 텍스처를 샘플링할 때 사용할 선형 필터링 샘플러
	D3D11_SAMPLER_DESC samplerLinearDesc = {};
	samplerLinearDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerLinearDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; // 가장자리 클램프
	samplerLinearDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerLinearDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerLinearDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerLinearDesc.MinLOD = 0;
	samplerLinearDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HR_T(m_->m_pDevice->CreateSamplerState(&samplerLinearDesc, &m_->m_pSamplerLinear));

	return true;
}

void App::UninitD3D()
{
	// 파이프라인 바인딩 참조 제거(잔존 참조로 인한 라이브 오브젝트 감소)
	if (m_->m_pDeviceContext)
	{
		m_->m_pDeviceContext->ClearState();
		m_->m_pDeviceContext->Flush();
	}

	// HDR 관련 리소스 해제
	SAFE_RELEASE(m_->m_pSamplerLinear);
	SAFE_RELEASE(m_->m_pHdrShaderResourceView);
	SAFE_RELEASE(m_->m_pHdrRenderTargetView);
	SAFE_RELEASE(m_->m_pHdrRenderTarget);

	SAFE_RELEASE(m_->m_pDepthStencilState);
	SAFE_RELEASE(m_->m_pDepthStencilStateReadOnly);
	SAFE_RELEASE(m_->m_pDepthStencilView);
	SAFE_RELEASE(m_->m_pRenderTargetView);
	SAFE_RELEASE(m_->RSNoCull);
	SAFE_RELEASE(m_->RSCullClockWise);
	SAFE_RELEASE(m_->RSCullFront);
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
	StaticMeshData cube = StaticMesh::CreateBox(XMFLOAT4(1, 1, 1, 1));
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

	// Shadow resources
	{
		// Texture2D typeless for DSV+SRV
		D3D11_TEXTURE2D_DESC td{};
		td.Width = (UINT)m_->m_ShadowSize;
		td.Height = (UINT)m_->m_ShadowSize;
		td.MipLevels = 1; td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R32_TYPELESS;
		td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		HR_T(m_->m_pDevice->CreateTexture2D(&td, nullptr, &m_->m_pShadowTex));

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
		dsvd.Format = DXGI_FORMAT_D32_FLOAT;
		dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		HR_T(m_->m_pDevice->CreateDepthStencilView(m_->m_pShadowTex, &dsvd, &m_->m_pShadowDSV));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
		srvd.Format = DXGI_FORMAT_R32_FLOAT;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MipLevels = 1;
		HR_T(m_->m_pDevice->CreateShaderResourceView(m_->m_pShadowTex, &srvd, &m_->m_pShadowSRV));

		// Shadow viewport
		m_->m_ShadowViewport = { 0.0f, 0.0f, (float)m_->m_ShadowSize, (float)m_->m_ShadowSize, 0.0f, 1.0f };

		// Depth-bias rasterizer
		D3D11_RASTERIZER_DESC rd{}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_BACK;
		rd.FrontCounterClockwise = true; rd.DepthBias = 1000; rd.SlopeScaledDepthBias = 1.0f; rd.DepthBiasClamp = 0.0f;
		rd.DepthClipEnable = TRUE; rd.MultisampleEnable = FALSE; rd.ScissorEnable = FALSE; rd.AntialiasedLineEnable = FALSE;
		HR_T(m_->m_pDevice->CreateRasterizerState(&rd, &m_->RSShadowBias));

		// Shadow sampler (linear, clamp)
		D3D11_SAMPLER_DESC ssd{}; ssd.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		ssd.AddressU = ssd.AddressV = ssd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; ssd.MaxLOD = D3D11_FLOAT32_MAX;
		ssd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		HR_T(m_->m_pDevice->CreateSamplerState(&ssd, &m_->m_pShadowSampler));
	}

	// ***********************************************************************************************
	// 카메라 설정
	// 카메라(View/Proj)로 상수 버퍼를 준비합니다
	m_->m_baseProjection.world = XMMatrixIdentity();
	// 카메라 초기 프러스텀 값들 설정
	m_Camera.SetFrustum(XMConvertToRadians(90.0f), AspectRatio(), 1.0f, 10000.0f);
	m_->m_baseProjection.view = XMMatrixTranspose(m_Camera.GetViewMatrixXM());
	m_->m_baseProjection.proj = XMMatrixTranspose(m_Camera.GetProjMatrixXM());
	m_->m_baseProjection.worldInvTranspose = XMMatrixInverse(nullptr, XMMatrixTranspose(m_->m_baseProjection.world));
	// DirectionalLight 초기값 필드 대입
	m_->m_baseProjection.dirLight.ambient = DirectX::XMFLOAT4(0, 0, 0, 1);
	m_->m_baseProjection.dirLight.diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
	m_->m_baseProjection.dirLight.specular = DirectX::XMFLOAT4(1, 1, 1, 1);
	m_->m_baseProjection.dirLight.direction = DirectX::XMFLOAT3(0, -1, 1);
	m_->m_baseProjection.dirLight.pad = 0.0f;
	m_->m_baseProjection.eyePos = m_Camera.GetPosition();
	m_->m_baseProjection.pad = 0.0f;

	// ***********************************************************************************************
	// 유틸 초기화. 라인 렌더러, 스카이박스, 디버그 박스
	if (!m_->m_LineRenderer) m_->m_LineRenderer = new LineRenderer();
	m_->m_LineRenderer->Initialize(m_->m_pDevice);

	// Skybox는 기존 Hanako를 기본으로 초기화
	if (!m_->m_Skybox) m_->m_Skybox = new Skybox();
	// Skybox는 이미 CreateDDSTextureFromFile로 SRV가 생성되어 있으므로, 여기선 현재 선택된 SRV를 사용하도록 Initialize는 경로 기반 대신 스킵할 수 있습니다.
	// 간편화를 위해 cubemap.dds로 초기화
	m_->m_Skybox->Initialize(m_->m_pDevice, m_->m_CurrentSkyboxPath, m_->m_pSkyBoxVertexShader, m_->m_pSkyBoxPixelShader, m_->m_pSkyBoxInputLayout, m_->m_pConstantBuffer);

	// Debug box buffers for light position marker
	StaticMesh::CreateDebugBoxBuffersLightTex(m_->m_pDevice, XMFLOAT4(1, 1, 1, 1), 0.2f, &m_->m_pDebugBoxVB, &m_->m_pDebugBoxIB, &m_->m_DebugBoxIndexCount);

	return true;
}

void App::UninitScene()
{
	SAFE_RELEASE(m_->m_pVertexBuffer);
	SAFE_RELEASE(m_->m_pIndexBuffer);
	SAFE_RELEASE(m_->m_pInputLayout);
	SAFE_RELEASE(m_->m_pInputLayoutNoTBN);
	SAFE_RELEASE(m_->m_pInputLayoutSkinned);
	SAFE_RELEASE(m_->m_pVertexShader);
	SAFE_RELEASE(m_->m_pVertexShaderNoTBN);
	SAFE_RELEASE(m_->m_pVertexShaderSkinned);
	SAFE_RELEASE(m_->m_pVertexShaderOutline);
	SAFE_RELEASE(m_->m_pVertexShaderSkinnedOutline);
	SAFE_RELEASE(m_->m_pPixelShaderOutline);
	SAFE_RELEASE(m_->m_pOutlineInputLayout);
	SAFE_RELEASE(m_->m_pLineVS);
	SAFE_RELEASE(m_->m_pLineInputLayout);
	SAFE_RELEASE(m_->m_pPixelShaderSolid);
	SAFE_RELEASE(m_->m_pPixelShader);
	SAFE_RELEASE(m_->m_pConstantBuffer);
	SAFE_RELEASE(m_->m_pSamplerState);

	// Shadow
	SAFE_RELEASE(m_->m_pVSShadow);
	SAFE_RELEASE(m_->m_pVSSkinnedShadow);
	SAFE_RELEASE(m_->m_pShadowSampler);
	SAFE_RELEASE(m_->RSShadowBias);
	SAFE_RELEASE(m_->m_pShadowSRV);
	SAFE_RELEASE(m_->m_pShadowDSV);
	SAFE_RELEASE(m_->m_pShadowTex);

	SAFE_RELEASE(m_->m_pSkyBoxInputLayout);
	SAFE_RELEASE(m_->m_pSkyBoxVertexShader);
	SAFE_RELEASE(m_->m_pSkyBoxPixelShader);

	SAFE_RELEASE(m_->m_pSkyHanakoSRV);
	SAFE_RELEASE(m_->m_pSkyCubeMapSRV);

	// IBL 텍스처 해제
	SAFE_RELEASE(m_->m_pIblDiffuseSRV);
	SAFE_RELEASE(m_->m_pIblSpecularSRV);
	SAFE_RELEASE(m_->m_pIblBrdfLutSRV);

    // 큐브 텍스처 해제: 개별 CubeObject 내 SRV 해제 (폴백과 중복 해제 방지)
    for (auto& obj : m_->m_Objects)
    {
        if (!obj || obj->kind != ObjectKind::Cube) continue;
        auto* co = static_cast<CubeObject*>(obj.get());
        for (int i = 0; i < 6; ++i)
        {
            if (co->faceSRV[i]   && co->faceSRV[i]   != m_->m_pFallbackWhite  && co->faceSRV[i]   != m_->m_pFallbackNormal && co->faceSRV[i]   != m_->m_pFallbackBlack)   { SAFE_RELEASE(co->faceSRV[i]); }
            if (co->normalSRV[i] && co->normalSRV[i] != m_->m_pFallbackWhite  && co->normalSRV[i] != m_->m_pFallbackNormal && co->normalSRV[i] != m_->m_pFallbackBlack) { SAFE_RELEASE(co->normalSRV[i]); }
            if (co->specSRV[i]   && co->specSRV[i]   != m_->m_pFallbackWhite  && co->specSRV[i]   != m_->m_pFallbackNormal && co->specSRV[i]   != m_->m_pFallbackBlack)   { SAFE_RELEASE(co->specSRV[i]); }
        }
    }
	for (int i = 0; i < 6; ++i) SAFE_RELEASE(m_->m_pSkyFaceSRV[i]);

	SAFE_RELEASE(m_->m_pFallbackWhite);
	SAFE_RELEASE(m_->m_pFallbackNormal);
	SAFE_RELEASE(m_->m_pFallbackBlack);

	SAFE_RELEASE(m_->m_pDebugBoxVB);
	SAFE_RELEASE(m_->m_pDebugBoxIB);
	if (m_->m_LineRenderer) { m_->m_LineRenderer->Release(); delete m_->m_LineRenderer; m_->m_LineRenderer = nullptr; }
	if (m_->m_Skybox) { m_->m_Skybox->Release(); delete m_->m_Skybox; m_->m_Skybox = nullptr; }

	SAFE_RELEASE(m_->m_pPS_ToneMappingLDR);
	SAFE_RELEASE(m_->m_pPS_ToneMappingHDR);

	// 모델 리소스 해제
	UnloadModel();
}

bool App::InitTexture()
{
	PrepareSkyFaceSRVs();

    // CubeObject 내부 경로 기반으로 텍스처 로드(있을 때만)
    for (auto& obj : m_->m_Objects)
    {
        if (!obj || obj->kind != ObjectKind::Cube) continue;
        auto* co = static_cast<CubeObject*>(obj.get());
        for (int i = 0; i < 6; ++i)
        {
            Microsoft::WRL::ComPtr<ID3D11Resource> res;
            if (co->facePaths[i])    { HR_T(CreateWICTextureFromFile(m_->m_pDevice, co->facePaths[i],    res.GetAddressOf(), &co->faceSRV[i])); res.Reset(); }
            if (co->normalPaths[i])  { HR_T(CreateWICTextureFromFile(m_->m_pDevice, co->normalPaths[i],  res.GetAddressOf(), &co->normalSRV[i])); res.Reset(); }
            if (co->specularPaths[i]){ HR_T(CreateWICTextureFromFile(m_->m_pDevice, co->specularPaths[i],res.GetAddressOf(), &co->specSRV[i])); res.Reset(); }
        }
    }
	return true;
}

bool App::InitImGui()
{
	// ImGui 초기화
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// 한글/일본어 표시를 위한 방법
	{
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontDefault();
		ImFontConfig cfg{}; cfg.MergeMode = true; cfg.PixelSnapH = true; cfg.OversampleH = 2; cfg.OversampleV = 2;
		// 한글: 맑은 고딕
		const ImWchar* rangeKR = io.Fonts->GetGlyphRangesKorean();
		io.Fonts->AddFontFromFileTTF("..\\Resource\\Font\\NotoSansKR-Regular.ttf", 17.0f, &cfg, rangeKR);
		// 일본어: Meiryo
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
	HR_T(CompileShaderFromFile(L"34_BasicVS.hlsl", "main", "vs_5_0", &vertexShaderBuffer));
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
	HR_T(CompileShaderFromFile(L"34_BasicVS.hlsl", "VSNoTBN", "vs_5_0", &vsNoTBN));
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
	HR_T(CompileShaderFromFile(L"34_BasicVS.hlsl", "VSLine", "vs_5_0", &vsLine));

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
		HR_T(CompileShaderFromFile(L"34_BasicVS.hlsl", "VSSkinned", "vs_5_0", &vsSkinned));
		HR_T(m_->m_pDevice->CreateInputLayout(layoutSkinned, ARRAYSIZE(layoutSkinned), vsSkinned->GetBufferPointer(), vsSkinned->GetBufferSize(), &m_->m_pInputLayoutSkinned));
		HR_T(m_->m_pDevice->CreateVertexShader(vsSkinned->GetBufferPointer(), vsSkinned->GetBufferSize(), nullptr, &m_->m_pVertexShaderSkinned));
		SAFE_RELEASE(vsSkinned);
	}
	HR_T(m_->m_pDevice->CreateInputLayout(lineLayout, ARRAYSIZE(lineLayout), vsLine->GetBufferPointer(), vsLine->GetBufferSize(), &m_->m_pLineInputLayout));
	HR_T(m_->m_pDevice->CreateVertexShader(vsLine->GetBufferPointer(), vsLine->GetBufferSize(), nullptr, &m_->m_pLineVS));
	SAFE_RELEASE(vsLine);

	// Outline VS
	{
		ID3D10Blob* vsOutline = nullptr;
		HR_T(CompileShaderFromFile(L"34_BasicVS.hlsl", "VSOutline", "vs_5_0", &vsOutline));
		HR_T(m_->m_pDevice->CreateVertexShader(vsOutline->GetBufferPointer(), vsOutline->GetBufferSize(), nullptr, &m_->m_pVertexShaderOutline));
		SAFE_RELEASE(vsOutline);

		ID3D10Blob* vsSkinnedOutline = nullptr;
		HR_T(CompileShaderFromFile(L"34_BasicVS.hlsl", "VSSkinnedOutline", "vs_5_0", &vsSkinnedOutline));
		HR_T(m_->m_pDevice->CreateVertexShader(vsSkinnedOutline->GetBufferPointer(), vsSkinnedOutline->GetBufferSize(), nullptr, &m_->m_pVertexShaderSkinnedOutline));
		SAFE_RELEASE(vsSkinnedOutline);
	}

	// Shadow VS (depth-only)
	{
		ID3D10Blob* vsShadow = nullptr;
		HR_T(CompileShaderFromFile(L"34_BasicVS.hlsl", "VSShadow", "vs_5_0", &vsShadow));
		HR_T(m_->m_pDevice->CreateVertexShader(vsShadow->GetBufferPointer(), vsShadow->GetBufferSize(), nullptr, &m_->m_pVSShadow));
		SAFE_RELEASE(vsShadow);

		ID3D10Blob* vsSkinnedShadow = nullptr;
		HR_T(CompileShaderFromFile(L"34_BasicVS.hlsl", "VSSkinnedShadow", "vs_5_0", &vsSkinnedShadow));
		HR_T(m_->m_pDevice->CreateVertexShader(vsSkinnedShadow->GetBufferPointer(), vsSkinnedShadow->GetBufferSize(), nullptr, &m_->m_pVSSkinnedShadow));
		SAFE_RELEASE(vsSkinnedShadow);
	}


	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"34_BasicPS.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_->m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_->m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음

	// Outline 전용 Pixel Shader (PSOutline)
	{
		ID3D10Blob* psOutline = nullptr;
		HR_T(CompileShaderFromFile(L"34_BasicPS.hlsl", "PSOutline", "ps_4_0", &psOutline));
		HR_T(m_->m_pDevice->CreatePixelShader(psOutline->GetBufferPointer(), psOutline->GetBufferSize(), nullptr, &m_->m_pPixelShaderOutline));
		SAFE_RELEASE(psOutline);
	}

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
	HR_T(CompileShaderFromFile(L"34_SkyBoxVS.hlsl", "VS", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_->m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_->m_pSkyBoxInputLayout));

	HR_T(m_->m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_->m_pSkyBoxVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// 컴파일 버퍼 해제

	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"34_SkyBoxPS.hlsl", "PS", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_->m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_->m_pSkyBoxPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음
	return true;
}

bool App::CreateQuad()
{
	HRESULT hr = 0; // 결과값.
	ID3D10Blob* errorMessage = nullptr;	 // 에러 메시지를 저장할 버퍼.	
	// 정점 선언.
	struct QuadVertex
	{
		Vector3 position;		// Normalized Device coordinate position
		Vector2 uv;				// Texture coordinate position

		QuadVertex(float x, float y, float z, float u, float v) : position(x, y, z), uv(u, v) {}
		QuadVertex(Vector3 p, Vector2 u) : position(p), uv(u) {}
	};

	QuadVertex QuadVertices[] =
	{
		QuadVertex(Vector3(-1.0f,  1.0f, 1.0f), Vector2(0.0f,0.0f)),	// Left Top 
		QuadVertex(Vector3(1.0f,  1.0f, 1.0f), Vector2(1.0f, 0.0f)),	// Right Top
		QuadVertex(Vector3(-1.0f, -1.0f, 1.0f), Vector2(0.0f, 1.0f)),	// Left Bottom
		QuadVertex(Vector3(1.0f, -1.0f, 1.0f), Vector2(1.0f, 1.0f))		// Right Bottom
	};

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = sizeof(QuadVertex) * ARRAYSIZE(QuadVertices);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = QuadVertices;	// 배열 데이터 할당.
	HR_T(m_->m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_->m_pQuadVertexBuffer));
	m_->m_QuadVertexBufferStride = sizeof(QuadVertex);		// 버텍스 버퍼 정보
	m_->m_QuadVertexBufferOffset = 0;

	// InputLayout 생성 	
	D3D11_INPUT_ELEMENT_DESC layout[] = // 입력 레이아웃.
	{   // SemanticName , SemanticIndex , Format , InputSlot , AlignedByteOffset , InputSlotClass , InstanceDataStepRate	
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"34_QuadVS.hlsl", "main", "vs_5_0", &vertexShaderBuffer));
	HR_T(m_->m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_->m_pQuadInputLayout));

	// 버텍스 셰이더 생성
	HR_T(m_->m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_->m_pQuadVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// 버퍼 해제.

	// 인덱스 버퍼 생성
	WORD indices[] =
	{
		0, 1, 2,
		2, 1, 3
	};
	m_->m_nQuadIndices = ARRAYSIZE(indices);	// 인덱스 개수 저장.
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;
	HR_T(m_->m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_->m_pQuadIndexBuffer));

	// 픽셀 셰이더 생성
	ID3D10Blob* pixelShaderBuffer = nullptr;


	HR_T(CompileShaderFromFile(L"34_ToneMappingPS_LDR.hlsl", "main", "ps_5_0", &pixelShaderBuffer));
	HR_T(m_->m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_->m_pPS_ToneMappingLDR));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음.

	HR_T(CompileShaderFromFile(L"34_ToneMappingPS_HDR.hlsl", "main", "ps_5_0", &pixelShaderBuffer));
	HR_T(m_->m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_->m_pPS_ToneMappingHDR));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음.

	return true;
}

// ------------------------- Model Loader (FBX/OBJ/PMX via Assimp) -------------------------
bool App::LoadModelFromFile(const std::wstring& pathW)
{
	// 새 모델델 추가

	// 폴백 텍스처(화이트/블랙/노멀) 생성: 각각 최초 1회만 생성
	auto createFallbackIfNull = [&](ID3D11ShaderResourceView** targetSRV, UINT rgba)
	{
		if (*targetSRV) return;
		D3D11_TEXTURE2D_DESC td{}; td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA sd{}; sd.pSysMem = &rgba; sd.SysMemPitch = sizeof(UINT);
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
		HR_T(m_->m_pDevice->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
		D3D11_SHADER_RESOURCE_VIEW_DESC srvd{}; srvd.Format = td.Format; srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; srvd.Texture2D.MipLevels = 1; srvd.Texture2D.MostDetailedMip = 0;
		HR_T(m_->m_pDevice->CreateShaderResourceView(tex.Get(), &srvd, targetSRV));
	};
	createFallbackIfNull(&m_->m_pFallbackWhite,  0xFFFFFFFF);
	createFallbackIfNull(&m_->m_pFallbackBlack,  0x000000FF); // a=1
	createFallbackIfNull(&m_->m_pFallbackNormal, 0x8080FFFF); // (0.5,0.5,1,1) in RGBA8

	// 받은 경로에서 이름, 확장자 추출
	std::wstring ext{ L"" }, fileName{ L"" };
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

	// 캐시 확인
	std::shared_ptr<SharedModelData> shared;
	if (auto it = m_->m_ModelCache.find(pathW); it != m_->m_ModelCache.end()) shared = it->second.lock();

	if (!shared)
	{
		shared = std::make_shared<SharedModelData>();
		shared->pathW = pathW;
		// 로드 경로에 따라 매니저 준비
		if (ext == L".fbx")
		{
			shared->source = ModelSource::FBX;
			// 공용 AssetManager를 통해 FBX 모델 공유/캐시
			shared->fbx = AssetManager::GetInstance().GetFbxModel(m_->m_pDevice, pathW);
			if (ok = (shared->fbx != nullptr))
			{
				m_->PushLog("[OK] Loaded FBX(shared): " + Utf8FromWString(fileName));
				shared->stride = shared->fbx->GetVertexStride();
				shared->vb = shared->fbx->GetVertexBuffer();
				shared->ib = shared->fbx->GetIndexBuffer();
				shared->indexCount = shared->fbx->GetIndexCount();
				shared->subsets.clear();
				for (auto& s : shared->fbx->GetSubsets()) shared->subsets.push_back({ s.startIndex, s.indexCount, s.materialIndex });
				shared->materialSRVs = shared->fbx->GetMaterialSRVs();
				shared->normalSRVs   = shared->fbx->GetNormalSRVs();
			}
			else { m_->PushLog("[ERR] Failed FBX: " + Utf8FromWString(fileName)); }
		}
		else if (ext == L".obj")
		{
			shared->source = ModelSource::OBJ;
			shared->obj = std::make_shared<ObjManager>();
			if (ok = shared->obj->Load(m_->m_pDevice, pathW))
			{
				m_->PushLog("[OK] Loaded OBJ(shared): " + Utf8FromWString(fileName));
				shared->stride = shared->obj->GetVertexStride();
				shared->vb = shared->obj->GetVertexBuffer();
				shared->ib = shared->obj->GetIndexBuffer();
				shared->indexCount = shared->obj->GetIndexCount();
				shared->subsets.clear();
				const auto& subs = shared->obj->GetSubsets();
				for (auto& s : subs) shared->subsets.push_back({ s.startIndex, s.indexCount, s.materialIndex });
				shared->materialSRVs = shared->obj->GetMaterialSRVs();
			}
			else { m_->PushLog("[ERR] Failed OBJ: " + Utf8FromWString(fileName)); }
		}
		else if (ext == L".pmx")
		{
			shared->source = ModelSource::PMX;
			shared->pmx = std::make_shared<PmxManager>();
			if (ok = shared->pmx->Load(m_->m_pDevice, pathW))
			{
				m_->PushLog("[OK] Loaded PMX(shared): " + Utf8FromWString(fileName));
				shared->stride = shared->pmx->GetVertexStride();
				shared->vb = shared->pmx->GetVertexBuffer();
				shared->ib = shared->pmx->GetIndexBuffer();
				shared->indexCount = shared->pmx->GetIndexCount();
				shared->subsets.clear();
				const auto& subs = shared->pmx->GetSubsets();
				for (auto& s : subs) shared->subsets.push_back({ s.startIndex, s.indexCount, s.materialIndex });
				shared->materialSRVs = shared->pmx->GetMaterialSRVs();
			}
			else { m_->PushLog("[ERR] Failed PMX: " + Utf8FromWString(fileName)); }
		}

		if (ok)
		{
			m_->m_ModelCache[pathW] = shared; // 캐시 등록
		}
	}
	else
	{
		ok = true;
		m_->PushLog("[OK] Reused cached model: " + Utf8FromWString(fileName));
	}

	if (ok && shared)
	{
		entry->shared = shared;
		entry->source = shared->source;
		// 본 캐시/출력텍스트 일괄 구축
		BuildBoneCacheStructure(*entry, nullptr);
		// 메시 통계는 UI에서 필요할 때 계산
		entry->meshStatsValid = false;
		entry->meshStats = MeshStats{};
		// 모델별 셰이딩 초기값 = 현재 글로벌 셰이딩, 아웃라인 기본은 Toon일 때만 ON
		entry->modelShading = m_->m_ShadingMode;
		entry->outlineEnabled = (entry->modelShading == ShadingMode::ToonShading);
		entry->instancePbrMaterial = m_->m_DefaultPbrMaterial;

		// FBX 인스턴스 애니메이터를 로드 시점에 1회 초기화
		if (entry->source == ModelSource::FBX && entry->shared && entry->shared->fbx)
		{
			entry->animator.InitMetadata(entry->shared->fbx->GetScenePtr());
			entry->animator.SetSharedContext(
				entry->shared->fbx->GetScenePtr(),
				entry->shared->fbx->GetNodeIndexOfName(),
				&entry->shared->fbx->GetBoneNames(),
				&entry->shared->fbx->GetBoneOffsets(),
				&entry->shared->fbx->GetGlobalInverse());
			auto t = entry->shared->fbx->GetCurrentAnimationType();
			entry->animator.SetType(t == FbxModel::AnimationType::Rigid ? FbxAnimation::AnimType::Rigid : (t == FbxModel::AnimationType::Skinned ? FbxAnimation::AnimType::Skinned : FbxAnimation::AnimType::None));
			entry->animatorInited = true;
		}

		m_->m_Models.push_back(std::move(entry));
	}

	return ok;
}

void App::UnloadModel()
{
	m_->m_Models.clear();
	m_->m_SelectedModelIdx = -1;
	m_->m_SelectedBoneIdx = -1;
}

void App::RenderControlPannel()
{
	// Control 패널
	ImGuiIO& ioUI = ImGui::GetIO();
	const float W = ioUI.DisplaySize.x;
	const float H = ioUI.DisplaySize.y;

	ImGui::SetNextWindowPos(ImVec2(10, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 360), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Controls"))
	{
		ImGui::SeparatorText("Tone Mapping Parameter");
		ImGui::SliderFloat("Exposure", &m_->m_Exposure, -2.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Monitor Max Nits", &m_->m_MonitorMaxNits, 0.0f, 50000.0f, "%.2f");

		ImGui::SeparatorText("PBR Parameter");
		// 텍스처 색 사용 여부 (PBR 전용)
		ImGui::Checkbox("Use Texture Color (PBR)", (bool*)&m_->m_UseTextureColor);
		// 노말맵 사용 여부 (PBR / 모델 공통)
		{
			bool useNormalMap = (m_->m_EnableNormalMapForCube != 0);
			if (ImGui::Checkbox("Use Normal Map (PBR)", &useNormalMap))
			{
				m_->m_EnableNormalMapForCube = useNormalMap ? 1 : 0;
			}
		}

		ImGui::ColorEdit3("Base Color##PBR", &m_->m_DefaultPbrMaterial.baseColor.x);
		ImGui::SliderFloat("Metalness##PBR", &m_->m_DefaultPbrMaterial.metalness, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Roughness##PBR", &m_->m_DefaultPbrMaterial.roughness, 0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Ambient Occlusion##PBR", &m_->m_DefaultPbrMaterial.ambientOcclusion, 0.0f, 1.0f, "%.2f");
		if (ImGui::Button("Reset PBR Material"))
		{
			m_->m_DefaultPbrMaterial = PBRMaterialCPU{};
		}

		ImGui::Separator();

		// SkyBox 선택
		{
			int cur = static_cast<int>(m_->m_SkyBoxChoice);

			const char* items[] = { "Off", "bridge", "indoor", "baker" };

			// 2. 콤보 선택 UI
			if (ImGui::Combo("SkyBox Choice", &cur, items, IM_ARRAYSIZE(items)))
			{
				// 3. int → enum 캐스팅
				m_->m_SkyBoxChoice = static_cast<App::Impl::SkyBoxChoice>(cur);

				if (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Off)
				{
					// Off: 배경 단색 사용
				}
				else
				{
					switch (m_->m_SkyBoxChoice)
					{
					case App::Impl::SkyBoxChoice::bridge:
						ChangeIBLSkyBox(L"..\\Resource\\Skybox\\Bridge\\bridge");
						break;

					case App::Impl::SkyBoxChoice::indoor:
						ChangeIBLSkyBox(L"..\\Resource\\Skybox\\Indoor\\indoor");
						break;

					case App::Impl::SkyBoxChoice::Baker:
						ChangeIBLSkyBox(L"..\\Resource\\Skybox\\Sample\\BakerSample");
						break;

					case App::Impl::SkyBoxChoice::Off:
					default:
						break;
					}

				}
			}

			// 4. Off일 때만 배경색 편집
			if (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Off)
			{
				ImGui::ColorEdit4("Background Color", &m_->m_ClearColor.x);
			}
		}
		ImGui::Separator();
		ImGui::Text("Camera");
		{
			if (ImGui::Button("Reset"))
			{
				m_Camera.Reset();
			}
			ImGui::SliderFloat("Camera Speed", &m_Camera.m_MoveSpeed, 1.0f, 500.0f, "%.1f");
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
			if (ImGui::DragFloatRange2("Near/Far", &nearZ, &farZ, 0.1f, 0.01f, 10000.0f, "Near: %.2f", "Far: %.2f"))
			{
				m_Camera.SetFrustum(m_Camera.GetFovYRad(), AspectRatio(), nearZ, farZ);
			}
			// 카메라 회전(도) 표시 및 편집: pitch, yaw, roll (API로 일원화)
			{
				XMFLOAT3 rotDeg = m_Camera.GetRotation();
				if (ImGui::DragFloat3("Camera Rot (deg)", &rotDeg.x, 1.0f, -180.0f, 180.0f, "%.1f"))
				{
					m_Camera.SetRotation(rotDeg);
				}
			}
		}
		ImGui::Separator();
		ImGui::Text("VMD Camera");
		if (ImGui::Button("Load VMD Camera and Play"))
		{
			std::wstring vmdPath;
			if (OpenFileDialogVMD(vmdPath))
			{
				if (mmd::LoadVmdCameraFromFile(vmdPath, m_->m_VmdCamera))
				{
					m_->PushLog("[OK] Loaded VMD Camera");
				}
				else
				{
					m_->PushLog("[ERR] Failed to load VMD Camera");
				}
			}
		}
		if (!m_->m_VmdCamera.frames.empty())
		{
			ImGui::Checkbox("Use VMD Camera", &m_->m_VmdCamera.use);
			ImGui::SameLine();
			if (ImGui::Button(m_->m_VmdCamera.playing ? "Pause##VMD" : "Play##VMD"))
			{
				m_->m_VmdCamera.playing = !m_->m_VmdCamera.playing;
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop##VMD"))
			{
				m_->m_VmdCamera.playing = false;
				if (!m_->m_VmdCamera.frames.empty())
					m_->m_VmdCamera.currentFrame = (float)m_->m_VmdCamera.frames.front().frame;
			}

			int firstFrame = m_->m_VmdCamera.frames.front().frame;
			int lastFrame = m_->m_VmdCamera.frames.back().frame;
			float curFrame = m_->m_VmdCamera.currentFrame;
			ImGui::Text("Frame: %.1f (%d ~ %d)", curFrame, firstFrame, lastFrame);
			ImGui::SliderFloat("VMD Camera Scale", &m_->m_VmdCamera.scale, 0.1f, 5.0f, "%.2f");
		}
		ImGui::Separator();
		ImGui::Text("Shading");
		{
			int mode = (int)m_->m_ShadingMode;
			const char* modes[] = { "Phong", "Blinn-Phong", "Lambert", "Unlit", "TextureOnly", "ToonShading", "PBR" };
			if (ImGui::Combo("Shading Mode", &mode, modes, IM_ARRAYSIZE(modes)))
			{
				m_->m_ShadingMode = (ShadingMode)mode;
			}

			{
				ImGui::Separator();
				ImGui::Text("Outline (Toon + Multipass)");
				// Multipass 아웃라인 파라미터
				ImGui::DragFloat("Thickness", &m_->m_OutlineThickness, 0.0f, 2.0f);
				ImGui::ColorEdit3("Color", &m_->m_OutlineColor.x);
				ImGui::SliderFloat("Strength", &m_->m_OutlineStrength, 0.0f, 4.0f, "%.2f");
			}
		}
		ImGui::Separator();
		ImGui::Text("Light");
		ImGui::DragFloat3("Light Direction", &m_->m_DirLight.direction.x, 0.05f);
		ImGui::ColorEdit4("Ambient", &m_->m_DirLight.ambient.x);
		ImGui::ColorEdit4("Diffuse", &m_->m_DirLight.diffuse.x);
		ImGui::ColorEdit4("Specular", &m_->m_DirLight.specular.x);
		if (ImGui::Button("Reset Light"))
		{
			m_->m_DirLight = { XMFLOAT4(0,0,0,1), XMFLOAT4(1,1,1,1), XMFLOAT4(0.7f,0.7f,0.7f,1), XMFLOAT3(0,0,1), 0.0f };
		}
		ImGui::Separator();

		ImGui::Text("Material");
		ImGui::ColorEdit4("Ambient (ka)", &m_->m_Material.ambient.x);
		ImGui::ColorEdit4("Diffuse (kd)", &m_->m_Material.diffuse.x);
		ImGui::ColorEdit4("Specular (ks)", &m_->m_Material.specular.x);
		ImGui::DragFloat("Shininess (alpha)", &m_->m_Material.specular.w, 0.05f, 1.0f, 256.0f);
		ImGui::ColorEdit4("Reflect (R=metal, A=roughness)", &m_->m_Material.reflect.x);
		if (ImGui::Button("Reset Material"))
		{
			m_->m_Material = { XMFLOAT4(1,1,1,1), XMFLOAT4(1,1,1,1), XMFLOAT4(1,1,1,32), XMFLOAT4(0,0,0,0) };
		}

	}
	ImGui::End();

	auto& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(10, 390), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(370, 360), ImGuiCond_FirstUseEver);
	// Shadow controls and debug view
	if (ImGui::Begin("ShadowMap (Directional)"))
	{
		ImGui::Checkbox("Enable Shadow", (bool*)&m_->m_ShadowEnabled);
		ImGui::DragFloat("Bias", &m_->m_ShadowBias, 0.0001f, 0.0f, .1f, "%.5f");
		ImGui::SliderFloat("PCF Radius(Texel)", &m_->m_ShadowPCFRadius, 0.0f, 3.0f, "%.2f");
		ImGui::SliderFloat("Ortho Radius(m)", &m_->m_ShadowOrthoRadius, 1.0f, 3000.0f, "%.1f");
		if (m_->m_pShadowSRV)
		{
			ImGui::Separator();
			ImGui::Text("ShadowMap Debug");
			ImVec2 avail = ImGui::GetContentRegionAvail();
			float sz = std::min(avail.x, 256.0f);
			ImGui::Image((ImTextureID)m_->m_pShadowSRV, ImVec2(sz, sz));
		}
	}
	ImGui::End();
}

void App::RenderModelPannel()
{
	// Models 독립 창
	auto& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 330, 380), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320, 360), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Details"))
    {
        if (m_->m_SelectedItem >= 0 && m_->m_SelectedItem < (int)m_->m_Objects.size())
        {
            auto* obj = m_->m_Objects[m_->m_SelectedItem].get();
            if (obj->kind == ObjectKind::Cube)
            {
                auto* co = static_cast<CubeObject*>(obj);
        ImGui::Text("Cube : %s", Utf8FromWString(co->name).c_str());
                ImGui::Separator();
                auto& t = co->cubeTransform;
        // Maps (per cube, Texture type only)
        if (co->cubeType == ECubeType::Texture)
        {
            bool nm = (co->useNormalMap != 0);
            if (ImGui::Checkbox("Enable Normal Map", &nm))
            {
                co->useNormalMap = nm ? 1 : 0;
            }
            bool sm = (co->useSpecularMap != 0);
            if (ImGui::Checkbox("Use Specular Map", &sm))
            {
                co->useSpecularMap = sm ? 1 : 0;
            }
        }
                ImGui::DragFloat3("Position", &t.position.x, 0.1f);
                ImGui::DragFloat3("Rotation (deg)", &t.rotationDeg.x, 1.0f, -360.0f, 360.0f, "%.1f");
                ImGui::DragFloat3("Scale", &t.scale.x, 0.01f, 0.001f, 100.0f, "%.3f");

                ImGui::Separator();
                ImGui::TextUnformatted("Material");
                ImGui::ColorEdit4("Ambient (ka)", &co->matAmbient.x);
                ImGui::ColorEdit4("Diffuse (kd)", &co->matDiffuse.x);
                ImGui::ColorEdit4("Specular (ks)", &co->matSpecular.x);
                ImGui::DragFloat("Shininess (alpha)", &co->matSpecular.w, 0.05f, 1.0f, 256.0f);
                ImGui::ColorEdit4("Reflect (kr,a)", &co->matReflect.x);
            }
            else
            {
                auto* mo = static_cast<ModelObject*>(obj);
                int i = mo->modelIndex;
                if (i >= 0 && i < (int)m_->m_Models.size())
                {
                    auto& mdl = *m_->m_Models[i];
                    ImGui::Text("Model #%d : %s", i, Utf8FromWString(m_->m_Models[i]->modelName).c_str());
                    ImGui::Separator();
                    ImGui::PushID(i);
                    ImGui::DragFloat3("Position", &mdl.pos.x, 0.1f);
                    ImGui::DragFloat3("Rotation (deg)", &mdl.rotDeg.x, 1.0f, -360.0f, 360.0f, "%.1f");
                    ImGui::DragFloat3("Scale", &mdl.scale.x, 0.01f, 0.001f, 100.0f, "%.3f");
                    ImGui::Checkbox("Auto Rotate (Yaw)", &mdl.autoRotate);
                    if (mdl.source == ModelSource::FBX && mdl.shared && mdl.shared->fbx)
                    {
                        if (mdl.shared->fbx->HasAnimations())
                        {
                            const auto& names = mdl.shared->fbx->GetAnimationNames();
                            if (mdl.uiSelectedAnim < 0 || mdl.uiSelectedAnim >= (int)names.size())
                                mdl.uiSelectedAnim = mdl.animator.GetCurrentIndex();

                            ImGui::Text("FBX Animations");
                            if (ImGui::BeginListBox("##AnimList", ImVec2(-FLT_MIN, 4 * ImGui::GetTextLineHeightWithSpacing())))
                            {
                                for (int a = 0; a < (int)names.size(); ++a)
                                {
                                    bool sel = (a == mdl.uiSelectedAnim);
                                    if (ImGui::Selectable(names[a].c_str(), sel))
                                    {
                                        mdl.uiSelectedAnim = a;
                                        mdl.animator.SetCurrentIndex(a);
                                        m_->PushLog(std::string("[OK] FBX Anim -> ") + names[a]);
                                    }
                                    if (sel) ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndListBox();
                            }

                            // FBX용 오디오 로드 (FMOD)
                            if (ImGui::Button("Load Audio (FMOD)..."))
                            {
                                wchar_t file[MAX_PATH] = { 0 };
                                OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn);
                                ofn.hwndOwner = GameApp::m_hWnd;
                                ofn.lpstrFilter = L"Audio Files (*.wav;*.mp3;*.ogg)\0*.wav;*.mp3;*.ogg\0All Files\0*.*\0\0";
                                ofn.lpstrFile = file;
                                ofn.nMaxFile = MAX_PATH;
                                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                                if (GetOpenFileNameW(&ofn))
                                {
                                    mdl.audioPath = file;
                                    mdl.audioLoaded = Sound::LoadMusic(mdl.audioPath);
                                    if (mdl.audioLoaded)
                                    {
                                        m_->PushLog("[OK] Audio loaded (FMOD)");
                                    }
                                    else
                                    {
                                        m_->PushLog("[ERR] Audio load failed (FMOD)");
                                    }
                                }
                            }

                            bool playFBX = mdl.animator.IsPlaying();
                            if (ImGui::Checkbox("Play", &playFBX))
                            {
                                mdl.animator.SetPlaying(playFBX);
                                mdl.uiAnimPlaying = playFBX;

                                // 애니메이션 재생 상태에 맞춰 사운드도 같이 제어
                                if (mdl.audioLoaded)
                                {
                                    if (playFBX)
                                    {
                                        Sound::Play();
                                    }
                                    else
                                    {
                                        Sound::Pause(true);
                                    }
                                }
                            }

                            double cur = mdl.animator.GetTimeSec();
                            double dur = mdl.animator.GetClipDurationSec(mdl.animator.GetCurrentIndex());
                            float curF = (float)cur, durF = (float)dur;
                            if (durF > 0.0f)
                            {
                                // 애니메이션 타임라인과 사운드 재생 위치를 동일한 초 단위로 맞춘다.
                                if (ImGui::SliderFloat("Time (s)", &curF, 0.0f, durF))
                                {
                                    mdl.animator.SetTimeSec((double)curF);
                                    if (mdl.audioLoaded)
                                    {
                                        Sound::SetTimeSeconds(curF);
                                    }
                                }
                            }

                            // 정지 / 현재 시간 출력 (애니+오디오 동시 제어)
                            if (mdl.audioLoaded)
                            {
                                ImGui::SeparatorText("Audio Sync (FMOD)");
                                if (ImGui::Button("Stop (Anim + Audio)##FBX"))
                                {
                                    mdl.animator.SetPlaying(false);
                                    mdl.animator.SetTimeSec(0.0);
                                    Sound::Stop();
                                    Sound::SetTimeSeconds(0.0f);
                                }
                                float curAudio = Sound::GetTimeSeconds();
                                float lenAudio = Sound::GetLengthSeconds();
                                ImGui::Text("Audio Time: %.2f / %.2f sec", curAudio, lenAudio);
                            }
                        }
                    }
                    else if (mdl.source == ModelSource::PMX && mdl.shared && mdl.shared->pmx)
                    {
                        if (ImGui::Button("Load VMD..."))
                        {
                            std::wstring vmdPath;
                            wchar_t file[MAX_PATH] = { 0 };
                            OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn);
                            ofn.hwndOwner = GameApp::m_hWnd;
                            ofn.lpstrFilter = L"VMD Files (*.vmd)\0*.vmd\0All Files\0*.*\0\0";
                            ofn.lpstrFile = file;
                            ofn.nMaxFile = MAX_PATH;
                            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                            if (GetOpenFileNameW(&ofn))
                            {
                                if (mdl.shared->pmx->LoadVMD(m_->m_pDevice, file))
                                {
                                    m_->PushLog("[OK] VMD loaded");
                                    mdl.uiAnimPlaying = true;
                                }
                                else
                                {
                                    m_->PushLog("[ERR] VMD load failed");
                                }
                            }
                        }

                        bool playPMX = mdl.shared->pmx->IsAnimationPlaying();
                        if (ImGui::Checkbox("Play##PMX", &playPMX))
                        {
                            mdl.shared->pmx->SetAnimationPlaying(playPMX);
                        }

                        double cur = mdl.shared->pmx->GetAnimationTimeSeconds();
                        double dur = mdl.shared->pmx->GetClipDurationSec();
                        float curF = (float)cur, durF = (float)dur;
                        if (durF > 0.0f)
                        {
                            if (ImGui::SliderFloat("Time (s)##PMX", &curF, 0.0f, durF))
                            {
                                mdl.shared->pmx->SetAnimationTimeSeconds((double)curF);
                            }
                        }
                    }

                    // 셰이딩 모드 선택 UI 
                    {
                        int modePer = (int)mdl.modelShading;
                        const char* modes[] = { "Phong", "Blinn-Phong", "Lambert", "Unlit", "TextureOnly", "ToonShading", "PBR" };
                        if (ImGui::Combo("Shading Mode", &modePer, modes, IM_ARRAYSIZE(modes)))
                        {
                            mdl.modelShading = (ShadingMode)modePer;
                        }
                    }
                    // Outline 적용 토글
                    ImGui::Checkbox("Outline", &mdl.outlineEnabled);

                    // 인스턴스 머티리얼
                    ImGui::Checkbox("Use Instance Material", &mdl.useInstanceMaterial);
                    if (mdl.useInstanceMaterial)
                    {
                        ImGui::ColorEdit4("Ambient (ka)##inst", &mdl.instanceMaterial.ambient.x);
                        ImGui::ColorEdit4("Diffuse (kd)##inst", &mdl.instanceMaterial.diffuse.x);
                        ImGui::ColorEdit4("Specular (ks)##inst", &mdl.instanceMaterial.specular.x);
                        ImGui::DragFloat("Shininess (alpha)##inst", &mdl.instanceMaterial.specular.w, 0.05f, 1.0f, 256.0f);
                        ImGui::ColorEdit4("Reflect (kr,a)##inst", &mdl.instanceMaterial.reflect.x);
                    }

					ImGui::SeparatorText("PBR Material");
					ImGui::Checkbox("Use Instance PBR Material", &mdl.useInstancePbrMaterial);
					if (mdl.useInstancePbrMaterial)
					{
						ImGui::ColorEdit3("Base Color##instPBR", &mdl.instancePbrMaterial.baseColor.x);
						ImGui::SliderFloat("Metalness##instPBR", &mdl.instancePbrMaterial.metalness, 0.0f, 1.0f, "%.2f");
						ImGui::SliderFloat("Roughness##instPBR", &mdl.instancePbrMaterial.roughness, 0.04f, 1.0f, "%.2f");
						ImGui::SliderFloat("Ambient Occlusion##instPBR", &mdl.instancePbrMaterial.ambientOcclusion, 0.0f, 1.0f, "%.2f");
					}
					else
					{
						const auto& defPbr = m_->m_DefaultPbrMaterial;
						ImGui::Text("Base Color: (%.2f, %.2f, %.2f)", defPbr.baseColor.x, defPbr.baseColor.y, defPbr.baseColor.z);
						ImGui::Text("Metalness: %.2f", defPbr.metalness);
						ImGui::Text("Roughness: %.2f", defPbr.roughness);
						ImGui::Text("AO: %.2f", defPbr.ambientOcclusion);
					}

                    ImGui::Separator();
                    // 디버그 AABB 기준 본 인덱스 설정 (-1: Auto)
                    ImGui::TextUnformatted("Debug AABB");
                    ImGui::DragInt("Bounds Bone Index (-1:auto)", &mdl.boundsBoneIndex, 1.0f, -1, 1023);
                    // 선택된 인덱스의 본 이름 표시 (FBX만)
                    if (mdl.source == ModelSource::FBX && mdl.shared && mdl.shared->fbx)
                    {
                        const auto& boneNames = mdl.shared->fbx->GetBoneNames();
                        const char* name = "<auto>";
                        if (mdl.boundsBoneIndex >= 0 && (size_t)mdl.boundsBoneIndex < boneNames.size())
                        {
                            name = boneNames[(size_t)mdl.boundsBoneIndex].c_str();
                        }
                        ImGui::Text("Bone: %s", name);
                    }

                    ImGui::Separator();
                    // 메시 통계는 최초에 한 번만 계산
                    if (!mdl.meshStatsValid && mdl.shared && mdl.shared->vb && mdl.shared->ib && mdl.shared->indexCount > 0)
                    {
                        mdl.meshStatsValid = ComputeMeshStats(
                            m_->m_pDevice,
                            m_->m_pDeviceContext,
                            mdl.shared->vb,
                            mdl.shared->stride,
                            mdl.shared->ib,
                            mdl.shared->indexCount,
                            mdl.meshStats);
                    }
                    if (mdl.meshStatsValid)
                    {
                        ImGui::Text("Vertex: %u   Edge: %u   Face: %u   Tri: %u", mdl.meshStats.vertices, mdl.meshStats.edges, mdl.meshStats.faces, mdl.meshStats.triangles);
                    }

                    // 본 구조 카드
                    const auto& cache = mdl.boneCache;
                    int rootIdx = mdl.boneRoot;
                    bool hasSkeleton = mdl.boneCacheValid && rootIdx >= 0 && rootIdx < (int)cache.size();
                    if (hasSkeleton && rootIdx >= 0)
                    {
                        ImGui::Checkbox("Show Bone Details", &mdl.showBoneDetails);
                        if (mdl.showBoneDetails)
                        {
                            ImGui::BeginChild("BoneCard", ImVec2(0, 240), true, ImGuiWindowFlags_HorizontalScrollbar);
                            ImGui::TextUnformatted(mdl.boneDisplayText.c_str());
                            ImGui::EndChild();
                        }
                    }

                    ImGui::PopID();
                }
            }
        }
        else
        {
            ImGui::TextUnformatted("Select an object in Scene Collection.");
        }
    }
    ImGui::End();
}

void App::RenderSceneCollection()
{
	auto& io = ImGui::GetIO();
	// Scene Collection 블렌더의 Hierarchy창
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 330, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320, 360), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Scene Collection"))
	{
		if (ImGui::Button("Browse Model..."))
		{
			std::wstring pathW;
			if (OpenFileDialogModel(pathW))
			{
				if (LoadModelFromFile(pathW))
				{
					m_->PushLog("[OK] Model Selected " + Utf8FromWString(pathW));
					m_->m_SelectedModelIdx = (int)m_->m_Models.size() - 1;
					int newIndex = (int)m_->m_Models.size() - 1;
					auto mo = std::make_unique<ModelObject>(m_->m_Models[(size_t)newIndex]->modelName, newIndex);
					m_->m_Objects.push_back(std::move(mo));
				}
				else { m_->PushLog("[ERR] Load failed (Browse) | file extension must be fbx, obj, pmx"); }
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Unload Model All"))
		{
			UnloadModel();
			m_->PushLog("[OK] Unloaded models All");
		}

		// ======================== Audio (FMOD) 전역 제어 ========================
		ImGui::Separator();
		ImGui::TextUnformatted("Audio (FMOD)");

		// 노래(오디오) 로드
		if (ImGui::Button("Load Audio..."))
		{
			wchar_t file[MAX_PATH] = { 0 };
			OPENFILENAMEW ofn{};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = GameApp::m_hWnd;
			ofn.lpstrFilter = L"Audio Files (*.wav;*.mp3;*.ogg)\0*.wav;*.mp3;*.ogg\0All Files\0*.*\0\0";
			ofn.lpstrFile = file;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
			if (GetOpenFileNameW(&ofn))
			{
				m_->m_AudioPath = file;
				m_->m_AudioLoaded = Sound::LoadMusic(m_->m_AudioPath);
				if (m_->m_AudioLoaded)
				{
					m_->PushLog("[OK] Audio loaded (FMOD) : " + Utf8FromWString(m_->m_AudioPath));
				}
				else
				{
					m_->PushLog("[ERR] Audio load failed (FMOD)");
				}
			}
		}

		ImGui::SameLine();
		bool audioLoaded = m_->m_AudioLoaded;
		ImGui::BeginDisabled(!audioLoaded);
		if (ImGui::Button("Play##GlobalAudio"))
		{
			Sound::Play();
		}
		ImGui::SameLine();
		if (ImGui::Button("Pause##GlobalAudio"))
		{
			Sound::Pause(true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop##GlobalAudio"))
		{
			Sound::Stop();
			Sound::SetTimeSeconds(0.0f);
		}
		ImGui::EndDisabled();

		if (audioLoaded)
		{
			float cur = Sound::GetTimeSeconds();
			float len = Sound::GetLengthSeconds();
			ImGui::Text("Time: %.2f / %.2f sec", cur, len);
			if (len > 0.0f)
			{
				if (ImGui::SliderFloat("Time (s)##GlobalAudio", &cur, 0.0f, len))
				{
					Sound::SetTimeSeconds(cur);
				}
			}
		}

        ImGui::Separator();
        ImGui::Text("Scene Collection");
        // 통합 목록: m_Objects 자체가 소스이므로 매 프레임 재구성하지 않음

        ImGui::BeginChild("##SceneList", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        for (int i = 0; i < (int)m_->m_Objects.size(); ++i)
        {
            ImGui::PushID(i);
            std::wstring wlabel = std::to_wstring(i) + L"  " + m_->m_Objects[i]->name;
            std::string label = Utf8FromWString(wlabel);
            bool sel = (i == m_->m_SelectedItem);
            if (ImGui::Selectable(label.c_str(), sel, ImGuiSelectableFlags_AllowItemOverlap)) m_->m_SelectedItem = i;
            ImGui::SameLine();
            ImGui::SetItemAllowOverlap();
            if (ImGui::SmallButton("Unload"))
            {
                if (m_->m_Objects[i]->kind == ObjectKind::Cube)
                {
                    // 큐브는 통합 컨테이너에서만 제거
                    m_->m_Objects.erase(m_->m_Objects.begin() + i);
                }
                else
                {
                    // 모델은 렌더 소스인 m_Models도 함께 제거
                    auto* mo = static_cast<ModelObject*>(m_->m_Objects[i].get());
                    int removed = mo->modelIndex;
                    if (removed >= 0 && removed < (int)m_->m_Models.size()) m_->m_Models.erase(m_->m_Models.begin() + removed);
                    // 인덱스 재정렬
                    for (auto& up : m_->m_Objects)
                    {
                        if (up && up->kind == ObjectKind::Model)
                        {
                            auto* om = static_cast<ModelObject*>(up.get());
                            if (om->modelIndex > removed) om->modelIndex -= 1;
                        }
                    }
                    m_->m_Objects.erase(m_->m_Objects.begin() + i);
                }
                m_->m_SelectedItem = -1;
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
	}
	ImGui::End();
}

void App::RenderConsolPannel()
{
	// Console
	{
		auto& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x / 2 - 415, io.DisplaySize.y - 210), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(830, 200), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("Console"))
		{
			ImGui::Checkbox("Auto Scroll", &m_->m_LogAutoScroll); ImGui::SameLine();
			ImGui::InputTextWithHint("##LogFilter", "filter...", m_->m_LogFilter, 70); ImGui::SameLine();
			if (ImGui::Button("Clear")) m_->m_LogLines.clear();
			ImGui::Separator();
			ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
			for (const auto& ln : m_->m_LogLines)
			{
				if (m_->m_LogFilter[0] != '\0')
				{
					if (ln.find(m_->m_LogFilter) == std::string::npos) continue;
				}
				ImGui::TextUnformatted(ln.c_str());
			}
			if (m_->m_LogAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				ImGui::SetScrollHereY(1.0f);
			ImGui::EndChild();
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
		else face = (v.z >= 0.0f) ? 4 : 5;     // +Z / -Z

		ID3D11ShaderResourceView* faceSRV = (face >= 0 && face < 6) ? m_->m_pSkyFaceSRV[face] : nullptr;
		if (faceSRV)
		{
			ImGuiIO& io = ImGui::GetIO();
			ImVec2 pos(io.DisplaySize.x - m_->m_HanakoDrawSize.x - 330.0f, 20.0f);
			ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
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
}

void App::RenderSceneImageWindow()
{
	if (!m_->m_ShowSceneImageWindow) return;

	// 씬 이미지 창 위치/크기 설정 (08_ImguiSystemInfo 참고)
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 size(130.0f, 260.0f);
	ImVec2 pos(io.DisplaySize.x - size.x - 330.0f, 20.0f);
	ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
	
	if (ImGui::Begin("Scene Image", &m_->m_ShowSceneImageWindow))
	{
		// 현재 씬 정보 표시
		const char* sceneName = (m_SceneIndex == 0) ? "SceneA" : "SceneB";
		ImGui::Text("Current Scene: %s", sceneName);
		ImGui::Separator();
		
		// 이미지 표시
		if (m_->m_pSceneImageSRV)
		{
			ImVec2 avail = ImGui::GetContentRegionAvail();
			float aspect = (m_->m_SceneImageSize.y > 0) ? (m_->m_SceneImageSize.x / m_->m_SceneImageSize.y) : 1.0f;
			float displayWidth = avail.x;
			float displayHeight = displayWidth / aspect;
			if (displayHeight > avail.y)
			{
				displayHeight = avail.y;
				displayWidth = displayHeight * aspect;
			}
			
			// 이미지 위치 계산
			ImVec2 imagePos = ImGui::GetCursorScreenPos();
			ImVec2 imageSize(displayWidth, displayHeight);
			ImGui::Image((ImTextureID)m_->m_pSceneImageSRV, imageSize);
			
			// 팝업 표시 (이미지 위에 오버레이)
			if (m_->m_ShowScenePopup)
			{
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				
				// 팝업 배경 (반투명)
				ImVec2 popupSize(300.0f, 80.0f);
				ImVec2 popupPos(
					imagePos.x + (imageSize.x - popupSize.x) * 0.5f,
					imagePos.y + (imageSize.y - popupSize.y) * 0.5f
				);
				ImVec2 popupEnd = ImVec2(popupPos.x + popupSize.x, popupPos.y + popupSize.y);
				
				// 배경 그리기
				drawList->AddRectFilled(popupPos, popupEnd, IM_COL32(0, 0, 0, 200), 10.0f);
				drawList->AddRect(popupPos, popupEnd, IM_COL32(255, 255, 255, 255), 10.0f, 0, 2.0f);
				
				// 텍스트 그리기 (중앙 정렬)
				ImVec2 textSize = ImGui::CalcTextSize(m_->m_ScenePopupMessage.c_str());
				ImVec2 textPos(
					popupPos.x + (popupSize.x - textSize.x) * 0.5f,
					popupPos.y + (popupSize.y - textSize.y) * 0.5f
				);
				drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), m_->m_ScenePopupMessage.c_str());
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "Image not loaded");
		}
	}
	ImGui::End();
}

void App::LoadSceneImage(const std::wstring& path)
{
	// 기존 이미지 해제
	SAFE_RELEASE(m_->m_pSceneImageSRV);
	m_->m_SceneImageSize = ImVec2(0, 0);

	// 새 이미지 로드
	if (LoadTextureSRVAndSize(m_->m_pDevice, path, &m_->m_pSceneImageSRV, &m_->m_SceneImageSize))
	{
		m_->PushLog("[OK] Scene image loaded: " + Utf8FromWString(path));
	}
	else
	{
		m_->PushLog("[ERR] Failed to load scene image: " + Utf8FromWString(path));
	}
}

void App::ChangeIBLSkyBox(const std::wstring& path)
{
	m_->m_pIblDiffuseSRV = nullptr;
	m_->m_pIblSpecularSRV = nullptr;
	m_->m_pIblBrdfLutSRV = nullptr;
	{
		HR_T(CreateDDSTextureFromFile(
			m_->m_pDevice,
			(path + L"DiffuseHDR.dds").c_str(),
			nullptr,
			&m_->m_pIblDiffuseSRV));

		HR_T(CreateDDSTextureFromFile(
			m_->m_pDevice,
			(path + L"SpecularHDR.dds").c_str(),
			nullptr,
			&m_->m_pIblSpecularSRV));

		HR_T(CreateDDSTextureFromFile(
			m_->m_pDevice,
			(path + L"Brdf.dds").c_str(),
			nullptr,
			&m_->m_pIblBrdfLutSRV));

		m_->PushLog("[OK] Loaded IBL(Sample): Diffuse / Specular / BRDF LUT");
	}

	// IBL과 동일한 환경맵을 스카이박스에도 사용 (배경과 반사가 일치하도록)
	ChangeSkyboxDDS((path + L"EnvHDR.dds").c_str());
}

void App::ChangeScene(std::unique_ptr<Scene> next)
{
	if (!next) return;
	if (m_CurrentScene) m_CurrentScene->OnExit();
	m_CurrentScene = std::move(next);
	m_CurrentScene->OnEnter();
}

void App::TrimVideoMemory()
{
	Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
	if (m_->m_pDevice && SUCCEEDED(m_->m_pDevice->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()))))
	{
		Microsoft::WRL::ComPtr<IDXGIDevice3> dxgiDevice3;
		if (SUCCEEDED(dxgiDevice.As(&dxgiDevice3)) && dxgiDevice3)
		{
			dxgiDevice3->Trim();
		}
	}
}

bool App::CheckHDRSupportAndGetMaxNits(float& outMaxNits, DXGI_FORMAT& outFormat)
{
	using namespace Microsoft::WRL;
	ComPtr<IDXGIFactory4> pFactory;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
	if (FAILED(hr))
	{
		LOG_ERRORA("ERROR: DXGI Factory 생성 실패.\n");
		return false;
	}
	// 2. 주 그래픽 어댑터 (0번) 열거
	ComPtr<IDXGIAdapter1> pAdapter;
	UINT adapterIndex = 0;
	while (pFactory->EnumAdapters1(adapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC1 desc;
		pAdapter->GetDesc1(&desc);

		// WARP 어댑터(소프트웨어)를 건너뛰고 주 어댑터만 사용하도록 선택할 수 있습니다.
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			adapterIndex++;
			pAdapter.Reset();
			continue;
		}
		break;
	}

	if (!pAdapter)
	{
		LOG_ERRORA("ERROR: 유효한 하드웨어 어댑터를 찾을 수 없습니다.\n");
		return false;
	}

	// 3. 주 모니터 출력 (0번) 열거
	ComPtr<IDXGIOutput> pOutput;
	hr = pAdapter->EnumOutputs(0, &pOutput); // 0번 출력
	if (FAILED(hr))
	{
		LOG_ERRORA("ERROR: 주 모니터 출력(Output 0)을 찾을 수 없습니다.\n");
		return false;
	}

	// 4. HDR 정보를 얻기 위해 IDXGIOutput6으로 쿼리
	ComPtr<IDXGIOutput6> pOutput6;
	hr = pOutput.As(&pOutput6);
	if (FAILED(hr))
	{
		printf("INFO: IDXGIOutput6 인터페이스를 얻을 수 없습니다. HDR 정보를 얻을 수 없습니다.\n");
		outMaxNits = 100.0f;
		outFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		return false;
	}

	// 5. DXGI_OUTPUT_DESC1에서 HDR 정보 확인
	DXGI_OUTPUT_DESC1 desc1 = {};
	hr = pOutput6->GetDesc1(&desc1);
	if (FAILED(hr))
	{
		printf("ERROR: GetDesc1 호출 실패.\n");
		return false;
	}

	// 6. HDR 활성화 조건 분석
	bool isHDRColorSpace = (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
	outMaxNits = (float)desc1.MaxLuminance;

	// OS가 HDR을 켰을 때 MaxLuminance는 100 Nits(SDR 기준)를 초과합니다.
	bool isHDRActive = outMaxNits > 100.0f;

	if (isHDRColorSpace && isHDRActive)
	{
		// 최종 판단: HDR 지원 및 OS 활성화
		outFormat = DXGI_FORMAT_R10G10B10A2_UNORM; // HDR 포맷 설정
		printf("SUCCESS: HDR 활성화됨. MaxNits: %.1f, Format: R10G10B10A2_UNORM\n", outMaxNits);
		return true;
	}
	else
	{
		// HDR 지원 안함 또는 OS에서 비활성화
		outMaxNits = 100.0f; // SDR 기본값
		outFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // SDR 포맷 설정
		printf("INFO: HDR 비활성화. MaxNits: 100.0, Format: R8G8B8A8_UNORM\n");
		return false;
	}
	return true;
}

//  DXGI_FORMAT_R8G8B8A8_UNORM : LDR
//  DXGI_FORMAT_R10G10B10A2_UNORM   : HDR
void App::CreateSwapChainAndBackBuffer(DXGI_FORMAT format)
{
	m_->m_format = format;
	if (!(format == DXGI_FORMAT_R8G8B8A8_UNORM ||
		format == DXGI_FORMAT_R10G10B10A2_UNORM))
	{
		m_->m_format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	HRESULT hr = 0;	// 결과값.

	// 스왑체인 속성 설정 구조체 생성.
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 2;
	swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;	// 스왑체인 출력할 창 핸들 값.
	swapDesc.Windowed = true;		// 창 모드 여부 설정.
	swapDesc.BufferDesc.Format = m_->m_format;
	// 백버퍼(텍스처)의 가로/세로 크기 설정.
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;
	// 화면 주사율 설정.
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;
	// 샘플링 관련 설정.
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	// 1. 장치 생성.   2.스왑체인 생성. 3.장치 컨텍스트 생성.
	HR_T(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_->m_pSwapChain, &m_->m_pDevice, NULL, &m_->m_pDeviceContext));

	// 4. 렌더타겟뷰 생성.  (백버퍼를 이용하는 렌더타겟뷰)	
	ID3D11Texture2D* pBackBufferTexture = nullptr;
	HR_T(m_->m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));
	HR_T(m_->m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_->m_pRenderTargetView));  // 텍스처는 내부 참조 증가
	SAFE_RELEASE(pBackBufferTexture);	//외부 참조 카운트를 감소시킨다.

	Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
	HR_T(m_->m_pSwapChain->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&swapChain3));
	if (m_->m_format == DXGI_FORMAT_R10G10B10A2_UNORM)
	{
		// EOTF = PQ (ST.2084 / G2084)  , 색역(Primaries) = Rec.2020 , RGB Full Range
		// 이 스왑체인의 0.0~1.0 값은 선형 RGB나 감마 값이 아니라 PQ로 인코딩된 HDR10 신호로 해석하라”
		HR_T(swapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020));
	}
}