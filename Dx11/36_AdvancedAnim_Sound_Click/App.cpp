/*
 * @brief  : ToneMapping
 * @details: 톤 매핑, HDR 렌더링, 감마 보정 등을 구현한 데모입니다.
 */

#include "App.h"
#include "../Common/AssetManager.h"
#include "../Common/BaseObject.h"
#include "../Common/Helper.h"
#include "../Common/LineRenderer.h"
#include "../Common/Mesh/FbxAnimation.h"
#include "../Common/Mesh/FbxModel.h"
#include "../Common/ObjManager.h"
#include "../Common/PmxManager.h"
#include "../Common/Ray.h"
#include "../Common/Skybox.h"
#include "../Common/StaticMesh.h"
#include "../Common/SystemInfomation.h"
#include "../Common/Transform.h"
#include "../Common/mmd/VmdCameraPlayer.h"
#include "SceneA.h"
#include "SceneB.h"
#include "SoundManager.h"
#include <algorithm>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <cfloat>
#include <cmath>
#include <cctype>
#include <commdlg.h>
#include <cstring>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxtk/DDSTextureLoader.h>
#include <directxtk/GamePad.h>
#include <directxtk/SimpleMath.h>
#include <directxtk/WICTextureLoader.h>
#include <dxgi1_4.h>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <memory>
#include <random>
#include <thread>
#include <unordered_set>
#include <windows.h>
#include <wrl/client.h>


#include <dxgi1_4.h> // swapchain3 ToneMapping을 위한 것
#include <dxgi1_6.h> // swapchain3 ToneMapping을 위한 것
#include "../Common/Animation/Animator.h"
#include "../Common/Animation/CharacterAnimController.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "Comdlg32.lib")

using namespace DirectX;
using namespace DirectX::SimpleMath;

// 내부 전용 타입들
struct DirectionalLight {
	XMFLOAT4 ambient;
	XMFLOAT4 diffuse;
	XMFLOAT4 specular;
	XMFLOAT3 direction;
	float intensity;
};
struct Material {
	XMFLOAT4 ambient;
	XMFLOAT4 diffuse;
	XMFLOAT4 specular;
	XMFLOAT4 reflect;
};
struct PBRMaterialCPU {
	XMFLOAT4 baseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	float metalness = 0.0f;
	float roughness = 0.5f;
	float ambientOcclusion = 1.0f;
	float pad = 0.0f;
};
struct ConstantBuffer {
	XMMATRIX world;
	XMMATRIX view;
	XMMATRIX proj;
	XMMATRIX worldInvTranspose;
	Material material;
	DirectionalLight dirLight;
	XMFLOAT3 eyePos;
	int shadingMode = 0;
	int enableNormalMap = 1;
	int useSpecularMap = 0;
	int useDiffuseMap = 1;
	float pad = 0.0f;
	int useTextureColor = 1;
	XMFLOAT3 pbrPad = { 0, 0, 0 };
	XMFLOAT4 pbrBaseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	float pbrMetalness = 0.0f;
	float pbrRoughness = 0.5f;
	float pbrAO = 1.0f;
	float pbrPad2 = 0.0f;
	float outlineWidth = 0.15f;
	float outlinePow = 1.0f;
	float outlineThickness = 0.014f;
	float outlineStrength = 1.0f;
	XMFLOAT4 outlineColor = XMFLOAT4(0, 0, 0, 1);
	// Shadow params
	XMMATRIX lightViewProj;
	float shadowBias = 0.0015f;
	float shadowMapSize = 2048.0f;
	float shadowPCFRadius = 1.0f;
	int shadowEnabled = 1;
	// Debug/Lines
	int boundsBoneIndex = -1;
	XMFLOAT3 boundsPad = { 0, 0, 0 };
};

struct PostProcessConstantBuffer {
	float g_Exposure;
	float g_MaxHDRNits;
	float g_Padding[2];
};
enum class ShadingMode {
	Phong = 0,
	BlinnPhong = 1,
	Lambert = 2,
	Unlit = 3,
	TextureOnly = 4,
	ToonShading = 5,
	PBR = 6
};
enum class ModelSource { FBX, OBJ, PMX, Custom };
struct ModelSubset {
	uint32_t start;
	uint32_t count;
	uint32_t materialIndex;
};
// 메시 통계 구조체
struct MeshStats {
	uint32_t vertices = 0, edges = 0, faces = 0, triangles = 0;
};

// 3D 모델 리소스
struct SharedModelData {
	std::wstring pathW;
	ModelSource source = ModelSource::Custom;
	std::shared_ptr<FbxModel> fbx;   // FBX용 로더
	std::shared_ptr<ObjManager> obj; // OBJ용 로더
	std::shared_ptr<PmxManager> pmx; // PMX용 로더

	// 그리는 자원들. 리소스를 모두 공유함
	ID3D11Buffer* vb = nullptr;
	ID3D11Buffer* ib = nullptr;
	int indexCount = 0;
	UINT stride = 0;
	std::vector<ModelSubset> subsets;
	std::vector<ID3D11ShaderResourceView*>
		materialSRVs;                                   // BaseColor/Albedo 텍스처
	std::vector<ID3D11ShaderResourceView*> normalSRVs; // 노말맵 텍스처 (옵션)
};

// 여러 모델을 그리기 위한 구조체
struct ModelEntry {
	std::wstring modelName{ L"" };
	ModelSource source = ModelSource::Custom; // 공유 데이터 동일 경로 모델끼리
	std::shared_ptr<SharedModelData> shared;

	// 드로우는 shared의 자원을 사용

	// 트랜스폼
	XMFLOAT3 pos = { 0, 0, 0 };
	XMFLOAT3 scale = { 1, 1, 1 };
	XMFLOAT3 rotDeg = { 0, 0, 0 }; // yaw=pitch=roll(deg)
	bool autoRotate = false;
	// 모델별 셰이딩 모드 개별로 선택가능함
	ShadingMode modelShading = ShadingMode::Phong;
	bool outlineEnabled = true;
	bool showBoneDetails = false;

	// 인스턴스 전용 애니메이터/머티리얼
	FbxAnimation fbxBaseAnimator; // FBX 전용: per-instance bone palette
	bool animatorInited = false;
	// 애니메이션 업데이트 LOD를 위한 누적 시간 (입력/ImGui 프리즈 방지용)
	float animUpdateAccum = 0.0f;
	Material instanceMaterial{{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 32}, {0, 0, 0, 0} };
	bool useInstanceMaterial = false;
	PBRMaterialCPU instancePbrMaterial{};
	bool useInstancePbrMaterial = false;

	// 사전 계산된 메시 통계
	MeshStats meshStats{};
	bool meshStatsValid = false;

	// FBX 전용 애니메이션 UI 상태
	// ImGui에서 보여주기 위함
	int uiSelectedAnim = -1;
	bool uiAnimPlaying = false;

	// 본 트리 캐시 로드 시 1회 구축, UI는 이 캐시만 사용하여 빠르게 렌더링
	struct CachedSkelNode {
		std::wstring nameW;
		std::string nameU8;
		bool isBone = false;
		std::vector<int> children;
	};
	std::vector<CachedSkelNode> boneCache;
	int boneRoot = -1;
	bool boneCacheValid = false;
	std::string boneDisplayText; // 캐싱된 UI 출력

	// 로컬 공간 AABB (모델의 원본 좌표계 기준)
	bool boundsValid = false;
	XMFLOAT3 boundsMin = { 0, 0, 0 };
	XMFLOAT3 boundsMax = { 0, 0, 0 };
	// 디버그 AABB에 적용할 기준 본 인덱스(-1이면 자동: 스켈레톤 있으면 0, 없으면
	// 비활성)
	int boundsBoneIndex = -1;

	// 애니메이션 루트 변환을 반영한 AABB 샘플들(로컬 공간). 현재 클립 기준
	std::vector<XMFLOAT3> animAabbMinSamples;
	std::vector<XMFLOAT3> animAabbMaxSamples;
	float animAabbSampleDt = 0.0f;
	int animAabbClip = -1;

	// PMX 애니메이션과 동기화할 사운드 경로 및 상태
	std::wstring audioPath;
	bool audioLoaded = false;

	XMMATRIX GetWorldMatrix() const {
		XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
		XMMATRIX R = XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotDeg.x),
			XMConvertToRadians(rotDeg.y),
			XMConvertToRadians(rotDeg.z));
		XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);
		return S * R * T;
	}
};

struct CubeData {
	std::wstring name;
	Transform tranform;
	ECubeType type;
};

// TPS 카메라 헬퍼
static float SignedYawXZ(XMVECTOR fromDirWS, XMVECTOR toDirWS)
{
    // Y 제거하고 XZ에서만 각도
    XMVECTOR a = XMVectorSet(XMVectorGetX(fromDirWS), 0.0f, XMVectorGetZ(fromDirWS), 0.0f);
    XMVECTOR b = XMVectorSet(XMVectorGetX(toDirWS),   0.0f, XMVectorGetZ(toDirWS),   0.0f);

    float la = XMVectorGetX(XMVector3Length(a));
    float lb = XMVectorGetX(XMVector3Length(b));
    if (la < 1e-6f || lb < 1e-6f) return 0.0f;

    a = XMVector3Normalize(a);
    b = XMVector3Normalize(b);

    float dot = XMVectorGetX(XMVector3Dot(a, b));
    dot = std::clamp(dot, -1.0f, 1.0f);

    float ang = std::acos(dot);

    // 부호: cross.y
    float cy = XMVectorGetY(XMVector3Cross(a, b));
    float sign = (cy >= 0.0f) ? 1.0f : -1.0f;

    return ang * sign;
}

// 본 캐시/텍스트 일괄 구축 함수
static void BuildBoneCacheStructure(ModelEntry& entry, const char* filter) {
	entry.boneCache.clear();
	entry.boneRoot = -1;
	entry.boneCacheValid = false;
	entry.boneDisplayText.clear();

	// 캐시 생성
	if (entry.source == ModelSource::FBX && entry.shared && entry.shared->fbx &&
		entry.shared->fbx->HasSkeleton()) {
		const auto& sk = entry.shared->fbx->GetSkeleton();
		entry.boneCache.resize(sk.size());
		for (size_t si = 0; si < sk.size(); ++si) {
			const auto& s = sk[si];
			auto& c = entry.boneCache[si];
			c.nameW = s.nameW;
			c.nameU8 = Utf8FromWString(c.nameW);
			c.isBone = s.isBone;
			c.children = s.children;
		}
		entry.boneRoot = entry.shared->fbx->GetSkeletonRoot();
		entry.boneCacheValid = (entry.boneRoot >= 0) &&
			((size_t)entry.boneRoot < entry.boneCache.size());
	}
	else if (entry.source == ModelSource::PMX && entry.shared &&
		entry.shared->pmx && entry.shared->pmx->HasSkeleton()) {
		const auto& sk = entry.shared->pmx->GetSkeleton();
		entry.boneCache.resize(sk.size());
		for (size_t si = 0; si < sk.size(); ++si) {
			const auto& s = sk[si];
			auto& c = entry.boneCache[si];
			c.nameW = s.nameW;
			c.nameU8 = Utf8FromWString(c.nameW);
			c.isBone = s.isBone;
			c.children = s.children;
		}
		entry.boneRoot = entry.shared->pmx->GetSkeletonRoot();
		entry.boneCacheValid = (entry.boneRoot >= 0) &&
			((size_t)entry.boneRoot < entry.boneCache.size());
	}

	if (!entry.boneCacheValid || entry.boneRoot < 0 ||
		entry.boneRoot >= (int)entry.boneCache.size())
		return;

	// 출력 문자열 생성
	const auto& cache = entry.boneCache;
	const bool useFilter = (filter != nullptr && filter[0] != '\0');
	std::string f = useFilter ? std::string(filter) : std::string();

	entry.boneDisplayText += "Bone Count: ";
	entry.boneDisplayText += std::to_string((unsigned)cache.size());
	entry.boneDisplayText += "\n\n";

	std::function<bool(int)> subtreeContainsFilter = [&](int idx) -> bool {
		if (!useFilter)
			return true;
		if (idx < 0 || idx >= (int)cache.size())
			return false;
		if (cache[idx].nameU8.find(f) != std::string::npos)
			return true;
		for (int ch : cache[idx].children)
			if (subtreeContainsFilter(ch))
				return true;
		return false;
		};

	std::function<void(int, int)> dfs = [&](int idx, int depth) {
		if (idx < 0 || idx >= (int)cache.size())
			return;
		if (useFilter && !subtreeContainsFilter(idx))
			return;
		const auto& n = cache[idx];
		entry.boneDisplayText.append((size_t)depth * 2u, ' ');
		entry.boneDisplayText += n.nameU8;
		entry.boneDisplayText += '\n';
		for (int ch : n.children)
			dfs(ch, depth + 1);
		};

	dfs(entry.boneRoot, 0);
}

// 인덱스 버퍼를 스테이징으로 복사해 통계 계산 삼각형 리스트가 들어온다고 가정
static bool ComputeMeshStats(ID3D11Device* device, ID3D11DeviceContext* ctx,
	ID3D11Buffer* vb, UINT vertexStride,
	ID3D11Buffer* ib, int indexCount, MeshStats& out) {
	if (!device || !ctx || !vb || !ib || vertexStride == 0 || indexCount <= 0)
		return false;

	D3D11_BUFFER_DESC vbd{};
	vb->GetDesc(&vbd);
	out.vertices = vbd.ByteWidth / vertexStride;

	D3D11_BUFFER_DESC ibd{};
	ib->GetDesc(&ibd);
	D3D11_BUFFER_DESC sd = ibd;
	sd.Usage = D3D11_USAGE_STAGING;
	sd.BindFlags = 0;
	sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	sd.MiscFlags = 0;
	ID3D11Buffer* staging = nullptr;
	if (FAILED(device->CreateBuffer(&sd, nullptr, &staging)))
		return false;
	ctx->CopyResource(staging, ib);

	D3D11_MAPPED_SUBRESOURCE mapped{};
	bool ok = false;
	if (SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
		const uint32_t* idx = reinterpret_cast<const uint32_t*>(mapped.pData);
		int ic = indexCount;
		out.triangles = (ic >= 3) ? (uint32_t)(ic / 3) : 0;
		out.faces = out.triangles; // 삼각형으로 구성

		std::unordered_set<uint64_t> edges;
		edges.reserve(out.triangles * 2u);
		auto addEdge = [&](uint32_t a, uint32_t b) {
			uint32_t lo = (a < b) ? a : b;
			uint32_t hi = (a < b) ? b : a;
			uint64_t key = ((uint64_t)hi << 32) | (uint64_t)lo;
			edges.insert(key);
			};
		for (int i = 0; i + 2 < ic; i += 3) {
			uint32_t i0 = idx[i + 0], i1 = idx[i + 1], i2 = idx[i + 2];
			addEdge(i0, i1);
			addEdge(i1, i2);
			addEdge(i2, i0);
		}
		out.edges = (uint32_t)edges.size();
		ctx->Unmap(staging, 0);
		ok = true;
	}
	SAFE_RELEASE(staging);
	return ok;
}

// GPU VB를 스테이징으로 복사해 로컬 공간 AABB(min/max) 계산 (pos는 오프셋 0에 float3로 가정)
static bool ComputeLocalAABB(ID3D11Device* device, ID3D11DeviceContext* ctx,
	ID3D11Buffer* vb, UINT vertexStride,
	DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax)
{
	if (!device || !ctx || !vb || vertexStride < sizeof(float) * 3) return false;
	D3D11_BUFFER_DESC vbd{}; vb->GetDesc(&vbd);
	if (vbd.ByteWidth == 0) return false;

	D3D11_BUFFER_DESC sd = vbd; sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; sd.MiscFlags = 0;
	ID3D11Buffer* staging = nullptr;
	if (FAILED(device->CreateBuffer(&sd, nullptr, &staging))) return false;
	ctx->CopyResource(staging, vb);

	bool ok = false;
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)))
	{
		const uint8_t* base = reinterpret_cast<const uint8_t*>(mapped.pData);
		size_t vcount = vbd.ByteWidth / vertexStride;
		using namespace DirectX;
		XMFLOAT3 mn(FLT_MAX, FLT_MAX, FLT_MAX), mx(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (size_t i = 0; i < vcount; ++i)
		{
			const float* p = reinterpret_cast<const float*>(base + i * vertexStride);
			float x = p[0], y = p[1], z = p[2];
			if (x < mn.x) mn.x = x; if (y < mn.y) mn.y = y; if (z < mn.z) mn.z = z;
			if (x > mx.x) mx.x = x; if (y > mx.y) mx.y = y; if (z > mx.z) mx.z = z;
		}
		ctx->Unmap(staging, 0);
		outMin = mn; outMax = mx; ok = true;
	}
	SAFE_RELEASE(staging);
	return ok;
}

// pImpl 정의
struct App::Impl {
	// D3D에서 사용하는 객체
	ID3D11Device* m_pDevice = nullptr;
	ID3D11DeviceContext* m_pDeviceContext = nullptr;
	IDXGISwapChain* m_pSwapChain = nullptr;
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;

	// 파이프라인 셰이더/입력 레이아웃. 기본/PMX/스카이박스/라인
	ID3D11VertexShader* m_pVertexShader = nullptr;
	ID3D11PixelShader* m_pPixelShader = nullptr;
	ID3D11PixelShader* m_pPixelShaderSolid = nullptr;   // 마커용 흰색 출력
	ID3D11VertexShader* m_pVertexShaderNoTBN = nullptr; // PMX 전용 VS
	ID3D11InputLayout* m_pInputLayoutNoTBN = nullptr;   // PMX 전용 IL

	// G-Buffer (월드좌표, 월드Normal, 금속성, 거칠기, BaseColor)
	static const int GBufferCount = 5;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pGBufferTextures[GBufferCount] = {};
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pGBufferRTVs[GBufferCount] = {};
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pGBufferSRVs[GBufferCount] = {};

	// 디퍼드 렌더링용 셰이더
	ID3D11VertexShader* m_pGBufferVS = nullptr;
	ID3D11PixelShader* m_pGBufferPS = nullptr;
	ID3D11PixelShader* m_pDeferredLightPS = nullptr;
	ID3D11InputLayout* m_pGBufferInputLayout = nullptr;

	// 디퍼드 렌더링용 블렌드 상태 (가산 블렌딩)
	ID3D11BlendState* m_pBlendStateAdditive = nullptr;

	// 디퍼드 렌더링 활성화 플래그
	bool m_UseDeferredRendering = true;

	// FBX GPU 스키닝용 VS/IL
	ID3D11VertexShader* m_pVertexShaderSkinned = nullptr;
	ID3D11InputLayout* m_pInputLayoutSkinned = nullptr;
	ID3D11VertexShader* m_pVertexShaderOutline = nullptr;
	ID3D11VertexShader* m_pVertexShaderSkinnedOutline = nullptr;
	ID3D11VertexShader* m_pSkyBoxVertexShader = nullptr;
	ID3D11PixelShader* m_pSkyBoxPixelShader = nullptr;
	ID3D11InputLayout* m_pSkyBoxInputLayout = nullptr;
	ID3D11VertexShader* m_pLineVS = nullptr;
	ID3D11InputLayout* m_pLineInputLayout = nullptr;
	ID3D11PixelShader* m_pPixelShaderOutline = nullptr;
	ID3D11InputLayout* m_pOutlineInputLayout = nullptr;

	// 샘플러/블렌드 상태
	ID3D11SamplerState* m_pSamplerState = nullptr;
	ID3D11BlendState* m_pAlphaBlendState = nullptr;

	// Skybox/큐브맵 자원 및 옵션
	enum class SkyBoxChoice { Off = 0, bridge = 1, indoor = 2, Baker };
	// IBL 예제에서는 기본값을 Baker 환경맵으로 켜 둔다.
	SkyBoxChoice m_SkyBoxChoice = SkyBoxChoice::Baker;
	ID3D11ShaderResourceView* m_pSkyHanakoSRV = nullptr;
	ID3D11ShaderResourceView* m_pSkyCubeMapSRV = nullptr;
	ID3D11ShaderResourceView* m_pTextureSRV = nullptr; // 현재 스카이박스 SRV
	ID3D11ShaderResourceView* m_pSkyFaceSRV[6] = {};
	ImVec2 m_SkyFaceSize = ImVec2(0, 0);
	// 초기 스카이박스는 IBL Baker Sample 환경맵을 사용
	wchar_t m_CurrentSkyboxPath[260] = L"..\\Resource\\Skybox\\Sample\\BakerSampleEnvHDR.dds";

	// Cube 텍스처 경로는 CubeObject 안으로 이동
	// 기본 메시 버퍼/입력 레이아웃
	ID3D11InputLayout* m_pInputLayout = nullptr;
	ID3D11Buffer* m_pVertexBuffer = nullptr;
	UINT m_VertextBufferStride = 0;
	UINT m_VertextBufferOffset = 0;
	ID3D11Buffer* m_pIndexBuffer = nullptr;
	int m_nIndices = 0;

	// 공용 상수 버퍼 (b0)
	ID3D11Buffer* m_pConstantBuffer = nullptr;
	ID3D11Buffer* m_pPostProcessConstantBuffer = nullptr;
	ID3D11Buffer* m_pDirectionalLightBuffer =
		nullptr;                       // 디퍼드 라이트 패스용 (b3)
	ConstantBuffer m_ConstantBuffer{}; // CPU 캐시
	PostProcessConstantBuffer m_PostProcessConstantBuffer{};

	// 유틸 렌더러/디버그 박스
	class LineRenderer* m_LineRenderer = nullptr;
	class Skybox* m_Skybox = nullptr;
	ID3D11Buffer* m_pDebugBoxVB = nullptr;
	ID3D11Buffer* m_pDebugBoxIB = nullptr;
	int m_DebugBoxIndexCount = 0;

	// 깊이/래스터라이저 상태
	ID3D11DepthStencilView* m_pDepthStencilView = nullptr;
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;
	ID3D11DepthStencilState* m_pDepthStencilStateReadOnly =
		nullptr; // Outline용 깊이 읽기 전용
	ID3D11RasterizerState* RSNoCull = nullptr;
	ID3D11RasterizerState* RSCullClockWise = nullptr;
	ID3D11RasterizerState* RSCullFront = nullptr;

	// Shadow map
	ID3D11Texture2D* m_pShadowTex = nullptr;
	ID3D11DepthStencilView* m_pShadowDSV = nullptr;
	ID3D11ShaderResourceView* m_pShadowSRV = nullptr;
	D3D11_VIEWPORT m_ShadowViewport{};
	ID3D11RasterizerState* RSShadowBias = nullptr;
	ID3D11SamplerState* m_pShadowSampler = nullptr;
	ID3D11VertexShader* m_pVSShadow = nullptr;
	ID3D11VertexShader* m_pVSSkinnedShadow = nullptr;
	// Shadow params UI에서 띄우기 위함
	int m_ShadowEnabled = 1;
	int m_ShadowSize = 4096;
	float m_ShadowBias = 0.0015f;
	float m_ShadowPCFRadius = 1.0f;
	float m_ShadowOrthoRadius = 1000.0f; // 카메라 중심 반경(m)

	// 데모/디버그용 텍스처 및 UI 표시 크기
	ID3D11ShaderResourceView* m_TexHanakoSRV = nullptr;
	bool m_ShowHanako = false;
	ImVec2 m_HanakoDrawSize = ImVec2(128, 128);
	ImVec2 m_TexHanakoSize = ImVec2(0, 0);

	// FMOD 오디오 데모용 상태 (전역 오디오 컨트롤)
	std::wstring m_AudioPath;
	bool m_AudioLoaded = false;

	// 큐브 각 면 텍스처 Diffuse/Normal/Specular
	ID3D11ShaderResourceView* m_pCubeTextureSRVs[6] = { nullptr, nullptr, nullptr,
													   nullptr, nullptr, nullptr };
	ID3D11ShaderResourceView* m_pNormalSRVs[6] = { nullptr, nullptr, nullptr,
												  nullptr, nullptr, nullptr };
	ID3D11ShaderResourceView* m_pSpecularSRVs[6] = { nullptr, nullptr, nullptr,
													nullptr, nullptr, nullptr };

	// IBL(Image Based Lighting)용 텍스처들
	// - Diffuse  : Irradiance map (간접 난반사)
	// - Specular : Prefiltered env map (거칠기별 반사)
	// - BRDF LUT : (NdotV, Roughness)에 대한 F,G 적분 평균값
	ID3D11ShaderResourceView* m_pIblDiffuseSRV = nullptr;
	ID3D11ShaderResourceView* m_pIblSpecularSRV = nullptr;
	ID3D11ShaderResourceView* m_pIblBrdfLutSRV = nullptr;

	// 시스템 정보
	SystemInfomation m_SystemInfo;
	bool m_RotateModel = false;

	// 조명/재질
	DirectionalLight m_DirLight = { {0, 0, 0, 1}, {1, 1, 1, 1}, {0.7f, 0.7f, 0.7f, 1}, {0, -1.1f, 1}, 1.0f };
	Material m_Material = { {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 32}, {0, 0, 0, 0} };
	Material m_mirrorCubeMaterial = { {0, 0, 0, 1}, {0, 0, 0, 1}, {0, 0, 0, 32}, {1, 1, 1, 0.02f} };
	PBRMaterialCPU m_DefaultPbrMaterial{};

	// 라이트 마커 위치 / 카메라 기반 기본 행렬
	XMFLOAT3 m_LightPosition = { 4.0f, 4.0f, 0.0f };
	ConstantBuffer m_baseProjection{};

	// 셰이딩 옵션 / 클리어 컬러
	ShadingMode m_ShadingMode = ShadingMode::PBR;
	// Outline params ImGui에서 제어하는 용도도
	// Rim 파라미터 제거. 멀티패스 지오메트리 아웃라인만 사용
	float m_OutlineThickness = 0.08f;
	XMFLOAT4 m_OutlineColor = XMFLOAT4(1.0, 0.7286f, 0, 1);
	float m_OutlineStrength = 1.0f;
	int m_EnableNormalMapForCube = 0;
	int m_UseSpecularMapForCube = 0;
	int m_UseTextureColor = 1; // 0: 텍스처 색 무시, 1: 텍스처 색 사용(PBR)
	int m_LegacyShading = 1;
	XMFLOAT4 m_ClearColor = { 0.125f, 0.125f, 0.125f, 1.0f };

	// 모델 로딩 및 렌더링 FBX/OBJ/PMX
	std::vector<std::unique_ptr<ModelEntry>> m_Models; // 모델들
	ID3D11ShaderResourceView* m_pFallbackWhite = nullptr;
	ID3D11ShaderResourceView* m_pFallbackNormal = nullptr;
	ID3D11ShaderResourceView* m_pFallbackBlack = nullptr;
	std::string m_ModelPathInputUTF8;

	// 경로 기반 공유 모델 캐시
	std::unordered_map<std::wstring, std::weak_ptr<SharedModelData>> m_ModelCache;

	// 본 에디터 관련
	int m_SelectedBoneIdx = -1;       // 선택된 본 인덱스
	int m_SelectedModelIdx = -1;      // 선택된 모델 인덱스
	int m_SelectedStaticMeshIdx = -1; // 선택된 큐브 인덱스

	// 통합 객체 컨테이너 (상속 기반)
	std::vector<std::unique_ptr<BaseObject>> m_Objects;
	int m_SelectedItem = -1; // 통합 선택 인덱스

	// DockSpace 레이아웃
	bool m_EnableDock = false;
	bool m_DockBuilt = false;
	ImGuiID m_DockMain = 0;
	ImGuiID m_DockLeft = 0;
	ImGuiID m_DockRight = 0;
	ImGuiID m_DockBottom = 0;
	ImGuiID m_DockCenter = 0;

	// Bone 필터/옵션
	char m_BoneFilter[128] = { 0 };
	bool m_BoneExpandAll = false;

	// Console 로그
	std::vector<std::string> m_LogLines;
	bool m_LogAutoScroll = true;
	char m_LogFilter[128] = { 0 };

	std::function<void(std::string)> PushLog = [&](const std::string& s) {
		m_LogLines.push_back(s);
		};

	// 스폰 운터
	int m_SpawnTotal = 0;

	// 씬 이미지 창 관련
	std::wstring m_CurrentSceneImagePath = L"..\\Resource\\Image\\SceneA.png";
	std::wstring m_OriginalSceneImagePath =
		L"..\\Resource\\Image\\SceneA.png"; // 원본 이미지 경로
	std::wstring m_TempSceneImagePath;      // 임시 이미지 경로
	ID3D11ShaderResourceView* m_pSceneImageSRV = nullptr;
	ImVec2 m_SceneImageSize = ImVec2(0, 0);
	bool m_ShowSceneImageWindow = true;
	bool m_IsUsingTempImage = false; // 임시 이미지 사용 중인지 여부

	// 씬 변경 팝업 관련
	bool m_ShowScenePopup = false;
	float m_ScenePopupTimer = 0.0f;
	std::string m_ScenePopupMessage;

	// VMD 카메라 상태 (공용)
	mmd::VmdCameraState m_VmdCamera;

	// ===================== Advanced Character Rig (Socket/Blend/Layer/IK) =====================
	// - CharacterAnimController가 모든 로직을 처리
	bool m_UseAdvancedRig = true;
	bool m_CharRigInited = false;
	int m_CharModelIndex = 0;
	int m_WeaponModelIndex = 1;

	CharacterAnimController m_CharCtrl;

	// ===================== TPS Camera Follow =====================
	bool  m_TpsCamAttached = false;     // V키 토글
	float m_TpsYawRad   = 0.0f;
	float m_TpsPitchRad = XMConvertToRadians(15.0f);

	float m_TpsDist     = 220.0f;
	float m_TpsDistMin  = 80.0f;
	float m_TpsDistMax  = 450.0f;
	float m_TpsZoomStep = 12.0f;        // 휠 1칸 당 거리 변화

	float m_TpsRotSpeed = 0.004f;       // 마우스 회전 민감도
	float m_TpsPitchMin = XMConvertToRadians(-35.0f);
	float m_TpsPitchMax = XMConvertToRadians(60.0f);

	float m_TpsTargetHeightStand  = 95.0f;
	float m_TpsTargetHeightCrouch = 70.0f;

	int   m_TpsLastWheel = 0;

	// Aim
	float m_AimYawSmoothed = 0.0f;
	float m_AimSmoothing   = 18.0f;         // 클수록 빨리 따라감
	float m_AimMaxYawDeg   = 70.0f;         // 좌우 제한
	float m_AimFarDist     = 2000.0f;       // 조준점 생성용 거리

	// ===================== TPS Character Control =====================
	float m_CharWalkSpeed = 180.0f;   // 걷기 속도 (units/sec) - UI에서 조절
	float m_CharRunMul    = 1.7f;     // 달리기 배율 - UI에서 조절
	float m_CharTurnSpeed = 12.0f;    // 회전 따라가기(라디안/초) - UI에서 조절
	bool  m_CharRotateToMove = true; // 이동 방향으로 몸통 회전

	// ===================== Recoil(UI) : 앉은 상태 클릭 흔들림 줄이기 =====================
	float m_RecoilKickUi  = 0.25f;    // 기본값을 낮춰서 "덜 흔들리게" (UI에서 조절)
	float m_RecoilDecayUi = 12.0f;    // 감쇠 속도 (UI에서 조절)

	// ===================== Sniper Charge Shot =====================
	bool  m_SniperEnabled = true;

	bool  m_SniperCharging = false;
	float m_SniperCharge01 = 0.0f;      // 0..1
	float m_SniperChargeTimeSec = 1.2f; // 풀차지까지 시간 (UI에서 조절)

	ImVec2 m_SniperAimPos = ImVec2(0, 0);
	float  m_SniperAimRadius = 12.0f;
	bool   m_SniperLastShotCharged = false;

	// ===================== Movement SFX state =====================
	bool m_LastMove = false;
	bool m_LastRun  = false;

	// HDR 관련 변수
	// Quad를 그려야함
	float m_MonitorMaxNits = 1000.0f; // HDR 모니터 기본값 (1000 nits)
	float m_Exposure = 0.0f;          // Exposure 기본값 (0 = 1.0배, 변화 없음)
	bool m_isHDRSupported = false;
	DXGI_FORMAT m_format = DXGI_FORMAT_R8G8B8A8_UNORM;

	ID3D11Texture2D* m_pHdrRenderTarget = nullptr; // 렌더 타겟 텍스처
	ID3D11RenderTargetView* m_pHdrRenderTargetView = nullptr;
	ID3D11ShaderResourceView* m_pHdrShaderResourceView = nullptr;
	ID3D11SamplerState* m_pSamplerLinear =
		nullptr; // 선형 필터링 샘플러 상태 객체

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

static bool LoadTextureSRVAndSize(ID3D11Device* device,
	const std::wstring& path,
	ID3D11ShaderResourceView** outSRV,
	ImVec2* outSize) {
	if (!std::filesystem::exists(path))
		return false;
	if (FAILED(CreateTextureFromFile(device, path.c_str(), outSRV)))
		return false;
	if (outSRV && *outSRV) {
		Microsoft::WRL::ComPtr<ID3D11Resource> res;
		(*outSRV)->GetResource(res.GetAddressOf());
		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
		if (SUCCEEDED(res.As(&tex2D))) {
			D3D11_TEXTURE2D_DESC desc{};
			tex2D->GetDesc(&desc);
			if (outSize)
				*outSize = ImVec2((float)desc.Width, (float)desc.Height);
		}
	}
	return true;
}

// 모델용 파일 선택 대화상자 (fbx/obj/pmx)
static bool OpenFileDialogModel(std::wstring& outPath) {
	wchar_t file[MAX_PATH] = { 0 };
	OPENFILENAMEW ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GameApp::m_hWnd;
	ofn.lpstrFilter =
		L"Models (*.fbx;*.obj;*.pmx)\0*.fbx;*.obj;*.pmx\0All Files\0*.*\0\0";
	ofn.lpstrFile = file;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	if (GetOpenFileNameW(&ofn)) {
		outPath = file;
		return true;
	}
	return false;
}

// VMD 카메라용 파일 선택 대화상자
static bool OpenFileDialogVMD(std::wstring& outPath) {
	wchar_t file[MAX_PATH] = { 0 };
	OPENFILENAMEW ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GameApp::m_hWnd;
	ofn.lpstrFilter = L"VMD Files (*.vmd)\0*.vmd\0All Files\0*.*\0\0";
	ofn.lpstrFile = file;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	if (GetOpenFileNameW(&ofn)) {
		outPath = file;
		return true;
	}
	return false;
}

void App::PrepareSkyFaceSRVs() {
	// 다른 스카이박스로 바꿀 수도 있으니 해제하고 다시 로드
	for (int i = 0; i < 6; ++i)
		SAFE_RELEASE(m_->m_pSkyFaceSRV[i]);
	m_->m_SkyFaceSize = ImVec2(0, 0);
	if (!m_->m_pTextureSRV)
		return;

	Microsoft::WRL::ComPtr<ID3D11Resource> res;
	m_->m_pTextureSRV->GetResource(res.GetAddressOf());
	if (!res)
		return;

	// 파괴됐는지 안됐는지 판단을 위해 Comptr이 필요하다
	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D;
	HR_T(res.As(&tex2D));

	D3D11_TEXTURE2D_DESC desc{};
	tex2D->GetDesc(&desc);
	// 큐브맵은 6개의 array slice를 가짐. (여러 큐브면 6의 배수)
	if ((desc.ArraySize < 6))
		return;

	// 크기 기록 (mip0 기준)
	m_->m_SkyFaceSize = ImVec2((float)desc.Width, (float)desc.Height);

	for (UINT face = 0; face < 6; ++face) {
		D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
		sd.Format = desc.Format;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		sd.Texture2DArray.MostDetailedMip = 0;
		sd.Texture2DArray.MipLevels = desc.MipLevels;
		sd.Texture2DArray.FirstArraySlice = face;
		sd.Texture2DArray.ArraySize = 1;
		ID3D11ShaderResourceView* faceSRV = nullptr;
		if (SUCCEEDED(m_->m_pDevice->CreateShaderResourceView(tex2D.Get(), &sd,
			&faceSRV))) {
			m_->m_pSkyFaceSRV[face] = faceSRV;
		}
	}
}

void App::ChangeSkyboxDDS(const wchar_t* ddsPath) {
	if (m_->m_Skybox) {
		if (m_->m_Skybox->ChangeDDS(m_->m_pDevice, ddsPath)) {
			// also set for face view and PS binding
			m_->m_pTextureSRV = m_->m_Skybox->GetTexture();
			PrepareSkyFaceSRVs();
			wcscpy_s(m_->m_CurrentSkyboxPath, ddsPath);
		}
	}
}

bool App::OnInitialize() {
	if (!InitD3D())
		return false;

	if (!InitBasicEffect())
		return false;
	if (!InitSkyBoxEffect())
		return false;
	if (!CreateQuad())
		return false;

	// G-Buffer 생성 (디퍼드 렌더링용)
	if (!CreateGBuffer())
		return false;

	if (!InitScene())
		return false;
	if (!InitImGui())
		return false;

	if (!InitTexture())
		return false;

	// 값 타입 매니저 사용(동적 할당 없음)

	if (!m_->m_SystemInfo.InitSysInfomation(m_->m_pDevice))
		return false;

	AssetManager::Create();

	// ====================================== FMOD 사운드 초기화 ======================================
	if (!Sound::Initialize()) {
		m_->PushLog("[ERR] FMOD initialize failed");
	}
	else {
		m_->PushLog("[OK] FMOD initialized");
		// SFX 로드(경로는 프로젝트에 맞게 조정)
		Sound::LoadSfx("Walk",     L"..\\Resource\\Sound\\Walk.mp3", true);   // 루프 발자국
		Sound::LoadSfx("RunVoice", L"..\\Resource\\Sound\\Run_voice.mp3", false);
		Sound::LoadSfx("Shoot",    L"..\\Resource\\Sound\\Shoot.mp3", false);
		Sound::LoadSfx("Reload",    L"..\\Resource\\Sound\\Reload.mp3", false);
	}

	// ====================================== 씬 이미지 초기 로드  ====================================== 
	// 원본 이미지 경로 초기화
	m_->m_OriginalSceneImagePath = m_->m_CurrentSceneImagePath;
	LoadSceneImage(m_->m_CurrentSceneImagePath);

	// ====================================== IBL 텍스처 로드 (Sample 세트) ======================================
	//  - BakerSampleDiffuseHDR.dds  : Irradiance(난반사) 맵   → Diffuse IBL
	//  - BakerSampleSpecularHDR.dds : Prefiltered Env 맵      → Specular IBL
	//  - BakerSampleBrdf.dds        : BRDF LUT (NdotV,Roughness → A,B)
	ChangeIBLSkyBox(L"..\\Resource\\Skybox\\Sample\\BakerSample");
	m_->m_SkyBoxChoice = App::Impl::SkyBoxChoice::Baker;

	// ====================================== 3D 모델 ======================================
	//LoadModelFromFile(L"..\\Resource\\fbx\\Study\\char\\char.fbx"); // 0
	// LoadModelFromFile(L"..\\Resource\\fbx\\Alice_UmaUma.fbx"); // 0
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Alice3DGame\\Alice.fbx"); // 0
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Alice3DGame\\AmazingWonderland.fbx"); // 1
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\sphere.fbx"); // 2
	// LoadModelFromFile(L"..\\Resource\\fbx\\Neon.fbx"); // 3
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\sphere.fbx"); // 3
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Ground.fbx"); // 4

	m_->m_Objects.clear();
	for (int mi = 0; mi < (int)m_->m_Models.size(); ++mi) {
		auto mo = std::make_unique<ModelObject>(m_->m_Models[mi]->modelName, mi);
		m_->m_Objects.push_back(std::move(mo));
	}
	m_->m_Models[0]->modelShading = ShadingMode::PBR;
	m_->m_Models[1]->modelShading = ShadingMode::PBR;
	m_->m_Models[2]->modelShading = ShadingMode::PBR;
	m_->m_Models[3]->modelShading = ShadingMode::PBR;

	m_->m_Models[0]->pos = XMFLOAT3(0, 0.0f, 0.0f);
	m_->m_Models[0]->rotDeg = XMFLOAT3(0, 0.0f, 0.0f);
	m_->m_Models[1]->pos = XMFLOAT3(-26, 66.5f, -29.3f);
	m_->m_Models[1]->rotDeg = XMFLOAT3(13, 90.0f, 2.0f);
	m_->m_Models[2]->pos = XMFLOAT3(-130, 50.0f, 0.0f);
	m_->m_Models[3]->pos = XMFLOAT3(90, 0.0f, 70.0f);
	m_->m_Models[3]->scale = XMFLOAT3(0.5f, 0.5f, 0.5f);

	m_->m_Models[2]->useInstancePbrMaterial = true;
	m_->m_Models[2]->instancePbrMaterial.metalness = 1.0f;
	m_->m_Models[2]->instancePbrMaterial.roughness = 0.01f;
	m_->m_Models[2]->instancePbrMaterial.ambientOcclusion = 1.0f;

	m_->m_Models[3]->useInstancePbrMaterial = true;
	m_->m_Models[3]->instancePbrMaterial.metalness = 1.0f;
	m_->m_Models[3]->instancePbrMaterial.roughness = 0.01f;
	m_->m_Models[3]->instancePbrMaterial.ambientOcclusion = 1.0f;

	m_->m_Models[4]->scale = XMFLOAT3(2.0f, 1.0f, 8.0f);
	m_->m_Models[4]->pos = XMFLOAT3(0.0f, -2.0f, 0.0f);
	m_->m_Models[4]->useInstancePbrMaterial = true;
	m_->m_Models[4]->instancePbrMaterial.baseColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	m_->m_Models[4]->instancePbrMaterial.metalness = 0.01f;
	m_->m_Models[4]->instancePbrMaterial.roughness = 1.0f;
	m_->m_Models[4]->instancePbrMaterial.ambientOcclusion = 1.0f;

	// ====================================== 큐브  ======================================
	auto co = std::make_unique<CubeObject>(
		L"Cube" + std::to_wstring(1),
		Transform({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }),
		ECubeType::Texture);
	for (int i = 0; i < 6; ++i) {
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

		// ====================================== Advanced Rig 초기화 (CharacterAnimController) ======================================
		// - Alice(0): 캐릭터
		// - AmazingWonderland(1): 라이플
		// - CharacterAnimController가 모든 애니메이션 로직을 처리
		if (m_->m_UseAdvancedRig && m_->m_Models.size() >= 2) {
			const int ci = m_->m_CharModelIndex;
			if (ci >= 0 && ci < (int)m_->m_Models.size()) {
				auto& alice = *m_->m_Models[(size_t)ci];
				if (alice.shared && alice.shared->fbx && alice.shared->fbx->HasSkeleton()) {
					// 컨트롤러 설정
					m_->m_CharCtrl.config.weaponSocket.socketName = "WeaponPoint";
					m_->m_CharCtrl.config.weaponSocket.parentBone = "Hand_R";
					m_->m_CharCtrl.config.weaponSocket.pos = { 0.1f, 0.05f, 0.0f };
					m_->m_CharCtrl.config.weaponSocket.rotDeg = { 0.0f, 90.0f, 0.0f };
					m_->m_CharCtrl.config.weaponSocket.scale = { 1.0f, 1.0f, 1.0f };

					// 전환 테이블(원하는 곳만 override) - Locomotion은 즉시 반응
					m_->m_CharCtrl.config.baseTransitions["Idle"]["Run"] = { 0.18f, false, 0.0f, 0.0f, true };
					m_->m_CharCtrl.config.baseTransitions["Run"]["Idle"] = { 0.12f, false, 0.0f, 0.0f, true };

					// Upper도 전환 테이블로(예: AnyState->Reload 빠르게)
					m_->m_CharCtrl.config.upperTransitions["*"]["Reload"] = { 0.10f, false, 0.0f, 0.0f, true };
					m_->m_CharCtrl.config.upperTransitions["Reload"]["None"] = { 0.10f, false, 0.0f, 0.0f, true };

					// 컨트롤러 초기화
					if (m_->m_CharCtrl.InitializeRig(
						m_->m_pDevice,
						alice.shared->fbx->GetScenePtr(),
						alice.shared->fbx->GetNodeIndexOfName(),
						alice.shared->fbx->GetGlobalInverse(),
						alice.shared->fbx->GetBoneNames(),
						alice.shared->fbx->GetBoneOffsets(),
						&alice.shared->fbx->GetAnimationNames()))
					{
						m_->m_CharRigInited = true;
						m_->PushLog("[OK] CharacterAnimController: Initialized");
					}
					else
					{
						m_->m_CharRigInited = false;
						m_->PushLog("[WARN] CharacterAnimController: Initialize failed");
					}
				}
				else {
					m_->m_CharRigInited = false;
					m_->PushLog("[WARN] AdvancedRig: Alice model has no skeleton (socket disabled)");
				}
			}
		}
	// ====================================== (임시) Idle-only 테스트 ======================================
	// AdvancedRig가 정상 동작하는 것이 확인되어, Idle-only 테스트는 비활성화한다.
	/*
	// - CharacterAnimator를 사용해서 Idle 애니메이션만 실행
	if (m_->m_Models.size() > 0) {
		const int ci = m_->m_CharModelIndex;
		if (ci >= 0 && ci < (int)m_->m_Models.size()) {
			auto& alice = *m_->m_Models[(size_t)ci];
			if (alice.shared && alice.shared->fbx && alice.shared->fbx->HasSkeleton()) {
				// Idle 애니메이션 찾기
				auto ToLower = [](std::string s) {
					for (char& c : s) c = (char)std::tolower((unsigned char)c);
					return s;
				};
				auto FindByContains = [&](const std::string& key) -> int {
					const auto& names = alice.shared->fbx->GetAnimationNames();
					const std::string k = ToLower(key);
					for (int i = 0; i < (int)names.size(); ++i) {
						if (ToLower(names[(size_t)i]).find(k) != std::string::npos)
							return i;
					}
					return -1;
				};
				int idxIdle = FindByContains("idle");
				if (idxIdle >= 0) {
					m_->m_CharAnimIdxIdle = idxIdle;
				}

				// CharacterAnimator 초기화
				m_->m_CharRig.Initialize(
					m_->m_pDevice,
					alice.shared->fbx->GetScenePtr(),
					alice.shared->fbx->GetNodeIndexOfName(),
					alice.shared->fbx->GetGlobalInverse(),
					alice.shared->fbx->GetBoneNames(),
					alice.shared->fbx->GetBoneOffsets());

				m_->m_CharRigInited = true;
				m_->PushLog("[OK] CharacterAnimator: Idle animation initialized");
			}
			else {
				m_->m_CharRigInited = false;
				m_->PushLog("[WARN] CharacterAnimator: Model has no skeleton");
			}
		}
	}
	*/

	return true;
}

void App::OnUninitialize() {
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

void App::OnInputProcess(const Keyboard::State& KeyState,
                         const Keyboard::KeyboardStateTracker& KeyTracker,
                         const Mouse::State& MouseState,
                         const Mouse::ButtonStateTracker& MouseTracker)
{
    // 1) V키 토글 (ImGui가 키보드 잡고 있으면 무시)
    if (!ImGui::GetIO().WantCaptureKeyboard && KeyTracker.IsKeyPressed(Keyboard::V))
    {
        m_->m_TpsCamAttached = !m_->m_TpsCamAttached;

        // 켤 때 현재 카메라 각도에서 이어서 시작
        if (m_->m_TpsCamAttached)
        {
            XMFLOAT3 rotDeg = m_Camera.GetRotation();
            m_->m_TpsYawRad   = XMConvertToRadians(rotDeg.y);
            m_->m_TpsPitchRad = XMConvertToRadians(rotDeg.x);
            m_->m_TpsLastWheel = MouseState.scrollWheelValue;
        }
    }

    // 2) TPS 부착 모드면: 자유카메라 입력은 막고, TPS 입력만 처리
    if (m_->m_TpsCamAttached)
    {
        // RMB 누르면 relative
        if (InputSystem::Instance && InputSystem::Instance->m_Mouse)
        {
            InputSystem::Instance->m_Mouse->SetMode(MouseState.rightButton ? Mouse::MODE_RELATIVE : Mouse::MODE_ABSOLUTE);
        }

        // RMB 드래그로 yaw/pitch
        if (!ImGui::GetIO().WantCaptureMouse && MouseState.positionMode == Mouse::MODE_RELATIVE)
        {
            m_->m_TpsYawRad   += float(MouseState.x) * m_->m_TpsRotSpeed;
            m_->m_TpsPitchRad += float(MouseState.y) * m_->m_TpsRotSpeed;
            m_->m_TpsPitchRad = std::clamp(m_->m_TpsPitchRad, m_->m_TpsPitchMin, m_->m_TpsPitchMax);
        }

        // 휠 줌
        if (!ImGui::GetIO().WantCaptureMouse)
        {
            int wheel = MouseState.scrollWheelValue;
            int delta = wheel - m_->m_TpsLastWheel;
            m_->m_TpsLastWheel = wheel;

            if (delta != 0)
            {
                // DirectXTK wheel: 보통 120 단위
                float notches = float(delta) / 120.0f;
                m_->m_TpsDist -= notches * m_->m_TpsZoomStep;
                m_->m_TpsDist = std::clamp(m_->m_TpsDist, m_->m_TpsDistMin, m_->m_TpsDistMax);
            }
        }

        // TPS 모드에서는 기본 카메라 입력 처리 X
        (void)KeyState; (void)MouseTracker;
        return;
    }

    // 3) TPS 꺼져 있으면 원래대로(자유 카메라)
    GameApp::OnInputProcess(KeyState, KeyTracker, MouseState, MouseTracker);
}

void App::OnUpdate(const float& dt) {
	// Shadow light view-projection (directional, orthographic, scene-anchored
	// with texel snapping)
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
		if (lenSqVal < 1.0e-6f) {
			// 예외 처리: 방향이 0이면 기본값(예: 수직 아래)으로 강제 설정
			fwd = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
		}
		else {
			// 정상이면 정규화 수행
			fwd = XMVector3Normalize(rawDir);
		}

		float r = m_->m_ShadowOrthoRadius;
		// 큰 오브젝트에서도 라이트 카메라가 항상 AABB 뒤쪽에 위치하도록 반경만큼
		// 뒤로 물린다
		float backDist = r;
		XMVECTOR lightPos = XMVectorSubtract(focus, XMVectorScale(fwd, backDist));
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);
		if (fabsf(XMVectorGetX(XMVector3Dot(up, fwd))) > 0.99f)
			up = XMVectorSet(0, 0, 1, 0);
		XMMATRIX LView = XMMatrixLookToLH(lightPos, fwd, up);

		// 라이트 뷰 공간에서 포커스 주변 AABB(±r)를 투영해 동적으로 near/far 계산
		float minZ = 1e9f, maxZ = -1e9f;
		for (int sx = -1; sx <= 1; sx += 2)
			for (int sy = -1; sy <= 1; sy += 2)
				for (int sz = -1; sz <= 1; sz += 2) {
					XMVECTOR cornerWS = XMVectorSet(focusF.x + sx * r, focusF.y + sy * r,
						focusF.z + sz * r, 1.0f);
					XMVECTOR cornerLS = XMVector3TransformCoord(cornerWS, LView);
					float z = XMVectorGetZ(cornerLS);
					minZ = (z < minZ) ? z : minZ;
					maxZ = (z > maxZ) ? z : maxZ;
				}
		// 여유 패딩(5%)을 주고 near는 0.01 이상으로 고정
		float zPad = r * 0.05f;
		float zn = (minZ - zPad);
		if (zn < 0.01f)
			zn = 0.01f;
		float zf = maxZ + zPad;
		if (zf <= zn)
			zf = zn + 0.01f;
		XMMATRIX LProj = XMMatrixOrthographicOffCenterLH(-r, r, -r, r, zn, zf);

		// 텍셀 스냅: 라이트 뷰 공간 XY를 섀도우맵 텍셀 그리드에 정렬(near/far에는
		// 영향 없음)
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
		// 애니메이션 업데이트 간격. 모델이 많을수록 업데이트 주기를 늘려 CPU 부하를
		// 제한.
		float animStep = 1.0f / 120.0f;
		if (totalModels > 60)
			animStep = 1.0f / 60.0f;
		if (totalModels > 120)
			animStep = 1.0f / 30.0f;

		// 섀도우용 씬 중심 누적
		XMFLOAT3 sceneCenter = { 0.0f, 0.0f, 0.0f };
		int sceneModelCount = 0;

		// ===================== AdvancedRig: Alice 애니메이션 + 소켓으로 Rifle 장착 =====================
		// - 이 블록은 "공유 데이터" 최적화와 별개로, 캐릭터(0)만 특별 처리한다.
		// - 이후 일반 루프에서는 캐릭터(0)의 기본 FbxAnimation::UpdateAndUpload를 스킵한다.
		if (m_->m_UseAdvancedRig && m_->m_CharRigInited && m_->m_Models.size() >= 2) {
			const int ci = m_->m_CharModelIndex;
			const int wi = m_->m_WeaponModelIndex;
			if (ci < 0 || wi < 0 || ci >= (int)m_->m_Models.size() || wi >= (int)m_->m_Models.size()) {
				// invalid indices -> fallback to normal update path
			}
			else {
				auto& alice = *m_->m_Models[(size_t)ci];
				auto& rifle = *m_->m_Models[(size_t)wi];

			// 입력 수집
			CharacterInputState input{};

			XMVECTOR moveDirWS = XMVectorZero();
			bool wantMove = false;
			bool wantRun  = false;

			if (InputSystem::Instance)
			{
				const auto& ks = InputSystem::Instance->m_KeyboardState;
				const auto& kt = InputSystem::Instance->m_KeyboardStateTracker;
				const auto& ms = InputSystem::Instance->m_MouseState;
				const auto& mt = InputSystem::Instance->m_MouseStateTracker;

				const bool lockMove = m_->m_CharCtrl.IsMovementLocked();
				const bool inStance = m_->m_CharCtrl.IsInShootStance();

				if (!ImGui::GetIO().WantCaptureKeyboard)
				{
					input.crouchTogglePressed = kt.IsKeyPressed(Keyboard::LeftControl) || kt.IsKeyPressed(Keyboard::RightControl);

					// TPS 카메라가 붙어 있을 때만 WASD로 캐릭터를 조작 (자유카메라와 충돌 방지)
					if (m_->m_TpsCamAttached && !lockMove)
					{
						// 카메라 yaw 기준 이동(전형적인 TPS)
						const float yaw = m_->m_TpsYawRad;
						XMVECTOR fwd = XMVectorSet(std::sinf(yaw), 0.0f, std::cosf(yaw), 0.0f);
						fwd = XMVector3Normalize(fwd);
						XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0,1,0,0), fwd));

						if (ks.IsKeyDown(Keyboard::W)) moveDirWS = XMVectorAdd(moveDirWS, fwd);
						if (ks.IsKeyDown(Keyboard::S)) moveDirWS = XMVectorSubtract(moveDirWS, fwd);
						if (ks.IsKeyDown(Keyboard::D)) moveDirWS = XMVectorAdd(moveDirWS, right);
						if (ks.IsKeyDown(Keyboard::A)) moveDirWS = XMVectorSubtract(moveDirWS, right);

						const float lenSq = XMVectorGetX(XMVector3LengthSq(moveDirWS));
						wantMove = (lenSq > 1.0e-6f);
						if (wantMove) moveDirWS = XMVector3Normalize(moveDirWS);

						const bool shift = ks.IsKeyDown(Keyboard::LeftShift) || ks.IsKeyDown(Keyboard::RightShift);
						wantRun = wantMove && shift;

						input.move = wantMove;
						input.run  = wantRun;

						// ===== 실제 캐릭터 이동 적용 =====
						float speed = m_->m_CharWalkSpeed * (wantRun ? m_->m_CharRunMul : 1.0f);

						XMVECTOR delta = XMVectorScale(moveDirWS, speed * dt);
						XMFLOAT3 d{}; XMStoreFloat3(&d, delta);

						alice.pos.x += d.x;
						alice.pos.z += d.z;

						// ===== 이동 방향으로 캐릭터 회전(선택) =====
						if (m_->m_CharRotateToMove)
						{
							auto WrapPi = [](float a)
							{
								while (a > XM_PI)  a -= XM_2PI;
								while (a < -XM_PI) a += XM_2PI;
								return a;
							};

							const float targetYaw = std::atan2(
								XMVectorGetX(moveDirWS),
								XMVectorGetZ(moveDirWS)) + XM_PI;

							float curYaw = XMConvertToRadians(alice.rotDeg.y);
							float diff = WrapPi(targetYaw - curYaw);

							float k = 1.0f - std::exp(-m_->m_CharTurnSpeed * dt);
							curYaw = curYaw + diff * k;

							alice.rotDeg.y = XMConvertToDegrees(curYaw);
						}
					}
					else
					{
						// TPS OFF 또는 lockMove면 캐릭터 locomotion 입력을 꺼서 "카메라 WASD"와 충돌 방지
						input.move = false;
						input.run  = false;
					}

					// R은 Shoot_Stance에서만 의미
					input.reloadPressed = inStance && kt.IsKeyPressed(Keyboard::R);
				}

				if (!ImGui::GetIO().WantCaptureMouse)
				{
					// [OLD] one-shot fire (keep commented)
					// input.firePressed = inStance &&
					//     (mt.leftButton == Mouse::ButtonStateTracker::PRESSED);

					// 스나이퍼 모드: 눌러서 차지, 떼면 발사
					if (m_->m_SniperEnabled && inStance)
					{
						// 마우스 위치(absolute면 마우스 위치, relative면 화면 중앙)
						ImVec2 curPos;
						if (ms.positionMode == Mouse::MODE_RELATIVE)
							curPos = ImVec2(m_ClientWidth * 0.5f, m_ClientHeight * 0.5f);
						else
							curPos = ImVec2((float)ms.x, (float)ms.y);

						if (mt.leftButton == Mouse::ButtonStateTracker::PRESSED)
						{
							m_->m_SniperCharging = true;
							m_->m_SniperCharge01 = 0.0f;
							m_->m_SniperAimPos   = curPos;
						}

						if (m_->m_SniperCharging)
						{
							// 조준점은 현재 마우스 위치를 따라가게(원하면 press 순간 고정도 가능)
							m_->m_SniperAimPos = curPos;

							if (mt.leftButton == Mouse::ButtonStateTracker::HELD)
							{
								float t = (m_->m_SniperChargeTimeSec > 0.001f) ? (dt / m_->m_SniperChargeTimeSec) : 1.0f;
								m_->m_SniperCharge01 = std::clamp(m_->m_SniperCharge01 + t, 0.0f, 1.0f);
							}

							if (mt.leftButton == Mouse::ButtonStateTracker::RELEASED)
							{
								m_->m_SniperLastShotCharged = (m_->m_SniperCharge01 >= 1.0f - 1e-4f);

								// 떼는 순간 "발사 트리거"
								input.firePressed = true;

								// 차지 종료
								m_->m_SniperCharging = false;
								m_->m_SniperCharge01 = 0.0f;
							}
						}
					}
				}
			}

			// ===== 이동 SFX =====
			if (m_->m_TpsCamAttached) // TPS 조작 중일 때만
			{
				if (input.move)
				{
					// 발자국 루프 재생(이미 재생 중이면 유지)
					float pitch = input.run ? m_->m_CharRunMul : 1.0f;
					Sound::PlaySfx("Walk", 0.9f, false, pitch);
					Sound::SetSfxPitch("Walk", pitch);

					// 달리기 시작 순간 목소리 1회
					if (input.run && !m_->m_LastRun)
					{
						Sound::PlaySfx("RunVoice", 1.0f, true, 1.0f);
					}
				}
				else
				{
					// 멈추면 발자국 정지
					Sound::StopSfx("Walk");
				}

				m_->m_LastMove = input.move;
				m_->m_LastRun  = input.run;
			}

			// ===== 리로드/사격 SFX =====
			if (input.reloadPressed)
			{
				Sound::PlaySfx("Reload", 1.0f, true, 1.0f);
			}

			if (input.firePressed)
			{
				// 풀차지면 다른 소리:
				// 1) 전용 파일이 있으면 그걸 재생
				// 2) 없으면 Shoot.wav를 pitch로 차별화
				if (m_->m_SniperLastShotCharged)
				{
					// if (!Sound::PlaySfx("ShootCharged", 1.0f, true, 1.0f)) // 파일 있을 때
					Sound::PlaySfx("Shoot", 1.0f, true, 0.85f); // 파일 없으면 대체(저음/묵직)
				}
				else
				{
					Sound::PlaySfx("Shoot", 1.0f, true, 1.0f);
				}
			}

			// === NEW: TPS 카메라 붙어있으면, 이번 프레임 카메라 pose + aim yaw 계산 ===
			AimInputState aim{};
			if (m_->m_TpsCamAttached)
			{
				const XMMATRIX charWorld = alice.GetWorldMatrix();
				const XMVECTOR charPosWS = XMVector3TransformCoord(XMVectorZero(), charWorld);
				const XMVECTOR upWS      = XMVectorSet(0, 1, 0, 0);

				// 카메라가 바라볼 피벗(서있을 때/앉을 때 높이 다르게)
				const bool crouchLike = m_->m_CharCtrl.IsInShootStance(); // 이전 프레임 기준이라도 충분히 자연스럽습니다.
				const float pivotH = crouchLike ? m_->m_TpsTargetHeightCrouch : m_->m_TpsTargetHeightStand;
				const XMVECTOR pivotWS = XMVectorAdd(charPosWS, XMVectorScale(upWS, pivotH));

				// 카메라 방향(저장된 yaw/pitch로)
				const XMMATRIX camR = XMMatrixRotationRollPitchYaw(m_->m_TpsPitchRad, m_->m_TpsYawRad, 0.0f);
				const XMVECTOR camForwardWS = XMVector3Normalize(camR.r[2]);

				// 카메라 위치: 피벗에서 뒤로 dist만큼
				const XMVECTOR camPosWS = XMVectorSubtract(pivotWS, XMVectorScale(camForwardWS, m_->m_TpsDist));

				// 카메라 실제 적용
				XMFLOAT3 cp{}; XMStoreFloat3(&cp, camPosWS);
				m_Camera.SetPosition(cp);

				XMFLOAT3 rotDeg{
					XMConvertToDegrees(m_->m_TpsPitchRad),
					XMConvertToDegrees(m_->m_TpsYawRad),
					0.0f
				};
				m_Camera.SetRotation(rotDeg);
				m_Camera.Update(0.0f); // world 재계산

				// ---- AimYaw 계산(±70도 제한) ----
				auto WrapPi = [](float a)
				{
					while (a > XM_PI)  a -= XM_2PI;
					while (a < -XM_PI) a += XM_2PI;
					return a;
				};

				// 캐릭터 yaw(몸통 방향)
				float charYaw = XMConvertToRadians(alice.rotDeg.y);

				// 카메라 yaw(조준 방향)
				float camYaw  = m_->m_TpsYawRad;

				// 차이 = 상체가 돌아야 할 yaw
				float yaw = WrapPi(camYaw - charYaw);

				const float maxYawRad = XMConvertToRadians(m_->m_AimMaxYawDeg);
				yaw = std::clamp(yaw, -maxYawRad, +maxYawRad);

				// 스무딩(튀는 것 방지)
				float a = 1.0f - std::exp(-m_->m_AimSmoothing * dt);
				m_->m_AimYawSmoothed = m_->m_AimYawSmoothed + (yaw - m_->m_AimYawSmoothed) * a;

				aim.enabled = true;
				aim.yawRad  = m_->m_AimYawSmoothed;
				aim.weight  = 1.0f;
			}
			else
			{
				// TPS 끄면 aim도 0으로 복귀
				float a = 1.0f - std::exp(-m_->m_AimSmoothing * dt);
				m_->m_AimYawSmoothed = m_->m_AimYawSmoothed + (0.0f - m_->m_AimYawSmoothed) * a;
			}

			// UI로 조절하는 리코일 파라미터를 컨트롤러에 반영
			m_->m_CharCtrl.config.recoil.kick  = m_->m_RecoilKickUi;
			m_->m_CharCtrl.config.recoil.decay = m_->m_RecoilDecayUi;

			// "완벽 목표": update + palette upload + weapon apply 까지 컨트롤러가 처리
			m_->m_CharCtrl.TickAndApply(dt, input, alice, &rifle, m_->m_pDevice, m_->m_pDeviceContext,
			                            m_->m_TpsCamAttached ? &aim : nullptr);
			}
		}

		// ===================== 간단한 Idle 애니메이션 테스트 =====================
		// - CharacterAnimator를 사용해서 Idle 애니메이션만 실행
		// AdvancedRig가 정상 동작하는 것이 확인되어, Idle-only 테스트는 비활성화한다.
		/*
		if (m_->m_CharRigInited && m_->m_Models.size() > 0) {
			const int ci = m_->m_CharModelIndex;
			if (ci >= 0 && ci < (int)m_->m_Models.size()) {
				auto& alice = *m_->m_Models[(size_t)ci];
				if (alice.shared && alice.shared->fbx && alice.shared->fbx->HasSkeleton()) {
					// 시간 업데이트
					m_->m_CharTimeSec += dt;

					// Idle 애니메이션 포인터 획득
					const aiScene* sc = alice.shared->fbx->GetScenePtr();
					const aiAnimation* animIdle = nullptr;
					if (sc && sc->mNumAnimations > 0) {
						const int iIdle = m_->m_CharAnimIdxIdle;
						if (iIdle >= 0 && (unsigned)iIdle < sc->mNumAnimations) {
							animIdle = sc->mAnimations[iIdle];
						}
					}

					// CharacterAnimator로 Idle 애니메이션만 실행 (블렌딩 없음)
					if (animIdle) {
						m_->m_CharRig.UpdateAnimation(
							dt,
							animIdle, m_->m_CharTimeSec,  // animA, timeA
							animIdle, m_->m_CharTimeSec,  // animB, timeB (같은 애니메이션)
							0.0f,                        // blendFactor (블렌딩 없음)
							nullptr, 0.0f,              // upperAnim, timeUpper 없음
							nullptr, 0.0f,              // addAnim, timeAdd 없음
							nullptr,                    // refAnim 없음
							0.0f, 0u,                    // proceduralAdditiveAlpha, proceduralSeed 없음
							false, nullptr, 0, XMVectorZero(), 0.0f  // IK 없음
						);

						// GPU 업로드
						alice.fbxBaseAnimator.EnsureBoneCB(m_->m_pDevice, 1023);
						alice.fbxBaseAnimator.UploadPalette(m_->m_pDeviceContext, m_->m_CharRig.finalTransforms);
					}
				}
			}
		}
		*/

		for (auto& mdlPtr : m_->m_Models) {
			auto& mdl = *mdlPtr;
			if (mdl.autoRotate) {
				mdl.rotDeg.y += 45.0f * dt;
				mdl.rotDeg.y = std::fmod(mdl.rotDeg.y + 180.0f, 360.0f) - 180.0f;
			}
			if (!mdl.shared)
				continue;
			if (updated.find(mdl.shared.get()) != updated.end())
				continue;

			// 섀도우용 씬 중심 누적
			sceneCenter.x += mdl.pos.x;
			sceneCenter.y += mdl.pos.y;
			sceneCenter.z += mdl.pos.z;
			sceneModelCount++;

			// AdvancedRig 대상 캐릭터는 여기서 기본 업데이트를 하지 않는다.
			const bool isRigCharacter = (m_->m_CharModelIndex >= 0 &&
				m_->m_CharModelIndex < (int)m_->m_Models.size() &&
				mdlPtr.get() == m_->m_Models[(size_t)m_->m_CharModelIndex].get());
			if (isRigCharacter && m_->m_UseAdvancedRig && m_->m_CharRigInited) {
				// shared 중복 업데이트 방지 마킹은 유지
				updated.insert(mdl.shared.get());
				continue;
			}

			// 2. 애니메이션 로직 실행
			//if (isInit) {
			//	static float totalTime = 0.0f;
			//	static float blendWeight = 0.0f;
			//	static bool isShoot = false;
			//	totalTime += dt;
			//
			//	// 키 입력
			//	bool keyRun = InputSystem::Instance->m_KeyboardState.IsKeyDown(DirectX::Keyboard::Z);
			//	bool keyShoot = InputSystem::Instance->m_KeyboardState.IsKeyDown(DirectX::Keyboard::X);
			//
			//	// 블렌딩 가중치 부드럽게 변경 (0:Idle <-> 1:Run)
			//	float targetW = keyRun ? 1.0f : 0.0f;
			//	blendWeight = blendWeight + (targetW - blendWeight) * dt * 5.0f;
			//
			//	// Assimp 씬 데이터
			//	const aiScene* scene = m_->m_Models[0]->shared->fbx->GetScenePtr();
			//
			//	// 애니메이션 인덱스 가정 (실제 파일 순서 확인 필요)
			//	// 0:Idle, 2:Run, 3:ShootStance, 5:Reload
			//	const aiAnimation* animIdle = scene->mAnimations[0];
			//	const aiAnimation* animRun = scene->mAnimations[2];
			//	const aiAnimation* animShoot = scene->mAnimations[3];
			//	const aiAnimation* animReload = scene->mAnimations[5]; // IK 타겟용
			//
			//	// [핵심] 통합 업데이트 호출
			//	charAnim.UpdateAnimation(
			//		dt,
			//		animIdle, totalTime,      // A: Idle
			//		animRun, totalTime,       // B: Run
			//		blendWeight,              // 0~1 보간
			//		keyShoot ? animShoot : nullptr, totalTime, // 상체 레이어 (사격 자세)
			//		nullptr, 0.f, nullptr     // Additive (생략)
			//	);
			//
			//	// 3. 결과 행렬 GPU 업로드
			//	m_->m_Models[0]->animator.UploadPalette(m_->m_pDeviceContext, charAnim.finalTransforms);
			//
			//	// 4. 소켓을 이용한 라이플 트랜스폼 동기화
			//	// 앨리스의 현재 월드 행렬
			//	XMMATRIX aliceWorld = aliceObj->GetWorldMatrix();
			//
			//	// 소켓의 최종 월드 행렬 계산 (소켓 로컬 * 부모 본 * 앨리스 월드)
			//	XMMATRIX rifleMat = charAnim.GetSocketWorldMatrix("WeaponPoint", aliceWorld);
			//
			//	// 라이플 모델에 분해하여 적용 (Decompose)
			//	XMVECTOR S, R, T;
			//	XMMatrixDecompose(&S, &R, &T, rifleMat);
			//
			//	XMStoreFloat3(&rifleObj->scale, S);
			//	XMStoreFloat3(&rifleObj->pos, T);
			//
			//	// 회전은 Quaternion -> Euler 변환이 필요하거나, 
			//	// 모델 클래스가 Quaternion을 지원하도록 수정해야 함.
			//	// 여기서는 예시로 Quaternion 값을 직접 사용한다고 가정.
			//	// rifleObj->rotQuat = R; 
			//}

			if (mdl.source == ModelSource::FBX && mdl.shared->fbx) {
				// 인스턴스별 애니메이션 업데이트 (공유 지오메트리/스켈레톤 사용)
				if (!mdl.animatorInited) {
					mdl.fbxBaseAnimator.InitMetadata(mdl.shared->fbx->GetScenePtr());
					mdl.fbxBaseAnimator.SetSharedContext(mdl.shared->fbx->GetScenePtr(),
						mdl.shared->fbx->GetNodeIndexOfName(),
						&mdl.shared->fbx->GetBoneNames(),
						&mdl.shared->fbx->GetBoneOffsets(),
						&mdl.shared->fbx->GetGlobalInverse());
					auto t = mdl.shared->fbx->GetCurrentAnimationType();
					mdl.fbxBaseAnimator.SetType(t == FbxModel::AnimationType::Rigid
						? FbxAnimation::AnimType::Rigid
						: (t == FbxModel::AnimationType::Skinned
							? FbxAnimation::AnimType::Skinned
							: FbxAnimation::AnimType::None));
					mdl.animatorInited = true;
				}
				// 애니메이션 LOD: 누적 시간 기반으로 일정 주기마다만 팔레트 평가/업로드
				mdl.animUpdateAccum += dt;
				if (mdl.animUpdateAccum >= animStep) {
					const double dtAnim = (double)mdl.animUpdateAccum;
					mdl.animUpdateAccum = 0.0f;

					mdl.fbxBaseAnimator.SetPlaying(mdl.uiAnimPlaying);
					mdl.fbxBaseAnimator.EnsureBoneCB(m_->m_pDevice, 1023);
					mdl.fbxBaseAnimator.UpdateAndUpload(m_->m_pDeviceContext, dtAnim,
						mdl.shared->fbx->GetScenePtr(),
						mdl.shared->fbx->GetNodeIndexOfName(),
						mdl.shared->fbx->GetBoneNames(),
						mdl.shared->fbx->GetBoneOffsets(),
						mdl.shared->fbx->GetGlobalInverse());
				}
			}
			else if (mdl.source == ModelSource::PMX && mdl.shared->pmx) {
				// PMX도 적용 팔레트이므로 1회만 업데이트
				auto it = updated.find(mdl.shared.get());
				if (it == updated.end()) {
					mdl.animUpdateAccum += dt;
					if (mdl.animUpdateAccum >= animStep) {
						const double dtAnim = (double)mdl.animUpdateAccum;
						mdl.animUpdateAccum = 0.0f;
						mdl.shared->pmx->UpdateAnimation(m_->m_pDeviceContext, dtAnim);
						updated.insert(mdl.shared.get());
					}
				}
			}

		}

		// FMOD 갱신 (매 프레임) - 모델 루프 밖에서 1회만 호출해야 한다.
		Sound::Update();

		// 섀도우 행렬은 씬 전체에 대해 한 번만 계산 (성능 최적화)
		if (sceneModelCount > 0) {
			sceneCenter.x /= (float)sceneModelCount;
			sceneCenter.y /= (float)sceneModelCount;
			sceneCenter.z /= (float)sceneModelCount;
			UpdateShadow(sceneCenter);
		}
		else {
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
		m_->m_VmdCamera, m_Camera.GetFovYRad(), AspectRatio(),
		m_Camera.GetNearZ(), m_Camera.GetFarZ(), view, proj, eye);

	if (m_->m_TpsCamAttached)
		useVmdCam = false;

	if (useVmdCam) {
		m_->m_baseProjection.view = XMMatrixTranspose(view);
		m_->m_baseProjection.proj = XMMatrixTranspose(proj);
		m_->m_baseProjection.eyePos = eye;
	}
	else {
		m_->m_baseProjection.view = XMMatrixTranspose(m_Camera.GetViewMatrixXM());
		m_->m_baseProjection.proj = XMMatrixTranspose(m_Camera.GetProjMatrixXM());
		m_->m_baseProjection.eyePos = m_Camera.GetPosition();
	}
	m_->m_baseProjection.worldInvTranspose =
		XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(model)));

	XMFLOAT3 lightDir = m_->m_DirLight.direction;
	XMVECTOR v = XMVector3Normalize(XMLoadFloat3(&lightDir));
	XMStoreFloat3(&lightDir, v);

	// DirectionalLight 정규화된 방향으로 대입
	m_->m_baseProjection.dirLight = m_->m_DirLight;
	m_->m_baseProjection.dirLight.direction = lightDir;
	m_->m_baseProjection.pad = 0.0f;

	// 머티리얼을 기본 캐시에 반영해 둔다
	m_->m_baseProjection.material = m_->m_Material;

	m_->m_SystemInfo.Tick(dt);

	// 씬 변경 팝업 타이머 업데이트
	// 씬 이미지 팝업 및 임시 이미지 타이머 업데이트
	if (m_->m_ShowScenePopup) {
		m_->m_ScenePopupTimer -= dt;
		if (m_->m_ScenePopupTimer <= 0.0f) {
			m_->m_ShowScenePopup = false;
			m_->m_ScenePopupTimer = 0.0f;

			// 임시 이미지 사용 중이었다면 원본 이미지로 복원
			if (m_->m_IsUsingTempImage) {
				m_->m_IsUsingTempImage = false;
				LoadSceneImage(m_->m_OriginalSceneImagePath);
				m_->m_CurrentSceneImagePath = m_->m_OriginalSceneImagePath;
			}
		}
	}

	// ====================================== 간단 마우스 피킹 (FBX/모델 위주) ======================================
	if (m_->m_UseAdvancedRig && m_->m_CharRigInited && m_->m_CharCtrl.IsMovementLocked())
	{
		// 앉아있는(혹은 앉기/서기/사격/리로드 중) LMB는 게임 입력으로 사용 => picking 스킵
	}
	else if (InputSystem::Instance && !ImGui::GetIO().WantCaptureMouse) {
		auto& input = *InputSystem::Instance;
		if (input.m_MouseStateTracker.leftButton ==
			Mouse::ButtonStateTracker::PRESSED) {
			PickingRay ray = PickingRay::ScreenPointToRay(
				m_Camera, (float)input.m_MouseState.x, (float)input.m_MouseState.y,
				(float)m_ClientWidth, (float)m_ClientHeight);

			int pickedModel = -1;
			float bestT = FLT_MAX;

			for (auto it = m_->m_Models.begin(); it != m_->m_Models.end(); ++it) {
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
				if (ray.HitAABB(bmin, bmax, t) && t < bestT) {
					bestT = t;
					pickedModel = (int)(it - m_->m_Models.begin());
				}
			}

			if (pickedModel >= 0) {
				m_->m_SelectedModelIdx = pickedModel;
				auto itObj = std::find_if(m_->m_Objects.begin(), m_->m_Objects.end(),
					[pickedModel](const std::unique_ptr<BaseObject>& up) {
						if (!up || up->kind != ObjectKind::Model) return false;
						return static_cast<ModelObject*>(up.get())->modelIndex == pickedModel;
					});
				if (itObj != m_->m_Objects.end())
					m_->m_SelectedItem = (int)std::distance(m_->m_Objects.begin(), itObj);
				else
					m_->m_SelectedItem = -1;
			}
		}
	}
}

inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) {
	return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
}
inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) {
	return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

// 상수 버퍼 업데이트
template <typename T> void App::UpdateCB(ID3D11Buffer* buffer, const T& data) {
	D3D11_MAPPED_SUBRESOURCE mapped;
	HR_T(m_->m_pDeviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
		&mapped));
	memcpy(mapped.pData, &data, sizeof(T));
	m_->m_pDeviceContext->Unmap(buffer, 0);
}

void App::OnRender() {
	// 1. 초기화 (HDR RT 및 Depth 클리어)
	PassClear();

	// 2. 렌더 패스 실행
	PassShadow();      // 섀도우 맵 생성
	PassMainScene();   // 3D 오브젝트, 스카이박스, 오버레이
	PassPostProcess(); // 톤매핑 (HDR -> BackBuffer)

	// 3. UI 및 Present
	PassUI();
	m_->m_pSwapChain->Present(0, 0);
}

void App::PassClear() {
	float clearColor[4] = { m_->m_ClearColor.x, m_->m_ClearColor.y,
						   m_->m_ClearColor.z, m_->m_ClearColor.w };
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pHdrRenderTargetView,
		clearColor);
	m_->m_pDeviceContext->ClearDepthStencilView(m_->m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f,0);
	m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pHdrRenderTargetView, m_->m_pDepthStencilView);
	m_->m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// ====================================== 디버그 박스: 각 3D 모델의 로컬 AABB를 선으로 표시 ======================================
void App::PassDebugDraw()
{
	for (auto& mdlPtr : m_->m_Models)
	{
		// AABB 계산 및 그리기

		if (!mdlPtr->boundsValid && mdlPtr->shared && mdlPtr->shared->vb ) { 
			// AABB 계산 로직 
			XMFLOAT3 mn, mx;
			if (ComputeLocalAABB(m_->m_pDevice, m_->m_pDeviceContext, mdlPtr->shared->vb, mdlPtr->shared->stride, mn, mx))
			{
				mdlPtr->boundsMin = mn; mdlPtr->boundsMax = mx; mdlPtr->boundsValid = true;
			}
		}
		if (mdlPtr->boundsValid && mdlPtr->source == ModelSource::FBX && mdlPtr->shared && mdlPtr->shared->fbx) {
			// 애니메이션 샘플링
			int curClip = mdlPtr->fbxBaseAnimator.GetCurrentIndex();
			const aiScene* sc = mdlPtr->shared->fbx->GetScenePtr();
			if (sc && curClip >= 0 && (size_t)curClip < sc->mNumAnimations)
			{
				if (mdlPtr->animAabbClip != curClip || mdlPtr->animAabbMinSamples.empty() || mdlPtr->animAabbMaxSamples.empty())
				{
					mdlPtr->animAabbClip = curClip;
					mdlPtr->animAabbMinSamples.clear();
					mdlPtr->animAabbMaxSamples.clear();
					const aiAnimation* a = sc->mAnimations[(size_t)curClip];
					double tps = (a && a->mTicksPerSecond != 0.0) ? a->mTicksPerSecond : 25.0;
					double dur = a ? (a->mDuration / (tps != 0.0 ? tps : 25.0)) : 0.0;
					int sps = 30; // 샘플링 주파수
					int numSamples = (dur > 0.0) ? (int)std::ceil(dur * sps) : 1;
					if (numSamples < 1) numSamples = 1;
					mdlPtr->animAabbSampleDt = (float)(1.0 / (double)sps);
					// 타겟 채널 선택: 루트 노드 이름이 있으면 우선, 없으면 첫 채널
					const char* rootName = sc->mRootNode ? sc->mRootNode->mName.C_Str() : nullptr;
					const aiNodeAnim* ch = nullptr;
					if (a)
					{
						for (unsigned i = 0; i < a->mNumChannels; ++i)
						{
							if (!a->mChannels[i]) continue;
							if (rootName && strcmp(a->mChannels[i]->mNodeName.C_Str(), rootName) == 0) { ch = a->mChannels[i]; break; }
						}
						if (!ch && a->mNumChannels > 0) ch = a->mChannels[0];
					}
					auto vInterp = [](const aiVectorKey* k, unsigned n, double t) { if (n == 0) return aiVector3D(0, 0, 0); if (n == 1) return k[0].mValue; unsigned i = 0; while (i + 1 < n && t >= k[i + 1].mTime) ++i; unsigned j = (i + 1 < n) ? i + 1 : i; double dt = k[j].mTime - k[i].mTime; double a = (dt > 0.0) ? (t - k[i].mTime) / dt : 0.0; aiVector3D v0 = k[i].mValue, v1 = k[j].mValue; return v0 + (float)a * (v1 - v0); };
					auto qInterp = [](const aiQuatKey* k, unsigned n, double t) { if (n == 0) return aiQuaternion(); if (n == 1) return k[0].mValue; unsigned i = 0; while (i + 1 < n && t >= k[i + 1].mTime) ++i; unsigned j = (i + 1 < n) ? i + 1 : i; double dt = k[j].mTime - k[i].mTime; double a = (dt > 0.0) ? (t - k[i].mTime) / dt : 0.0; aiQuaternion q; aiQuaternion::Interpolate(q, k[i].mValue, k[j].mValue, (float)a); q.Normalize(); return q; };
					mdlPtr->animAabbMinSamples.resize((size_t)numSamples);
					mdlPtr->animAabbMaxSamples.resize((size_t)numSamples);
					// 8 코너 미리 구성(로컬)
					auto mn0 = mdlPtr->boundsMin; auto mx0 = mdlPtr->boundsMax;
					XMFLOAT3 corners[8] = {
						{mn0.x, mn0.y, mn0.z}, {mx0.x, mn0.y, mn0.z}, {mx0.x, mn0.y, mx0.z}, {mn0.x, mn0.y, mx0.z},
						{mn0.x, mx0.y, mn0.z}, {mx0.x, mx0.y, mn0.z}, {mx0.x, mx0.y, mx0.z}, {mn0.x, mx0.y, mx0.z}
					};
					for (int si = 0; si < numSamples; ++si)
					{
						double tSec = (double)si * (1.0 / (double)sps);
						double tt = tSec * (tps != 0.0 ? tps : 25.0);
						aiVector3D S = ch ? ((ch->mNumScalingKeys > 0) ? vInterp(ch->mScalingKeys, ch->mNumScalingKeys, tt) : aiVector3D(1, 1, 1)) : aiVector3D(1, 1, 1);
						aiVector3D T = ch ? ((ch->mNumPositionKeys > 0) ? vInterp(ch->mPositionKeys, ch->mNumPositionKeys, tt) : aiVector3D(0, 0, 0)) : aiVector3D(0, 0, 0);
						aiQuaternion R = ch ? ((ch->mNumRotationKeys > 0) ? qInterp(ch->mRotationKeys, ch->mNumRotationKeys, tt) : aiQuaternion()) : aiQuaternion();
						aiMatrix4x4 mS; mS.Scaling(S, mS); aiMatrix4x4 mR = aiMatrix4x4(R.GetMatrix()); aiMatrix4x4 mT; mT.Translation(T, mT);
						aiMatrix4x4 mA = mT * mR * mS;
						XMFLOAT4X4 am; am._11 = (float)mA.a1; am._12 = (float)mA.a2; am._13 = (float)mA.a3; am._14 = (float)mA.a4;
						am._21 = (float)mA.b1; am._22 = (float)mA.b2; am._23 = (float)mA.b3; am._24 = (float)mA.b4;
						am._31 = (float)mA.c1; am._32 = (float)mA.c2; am._33 = (float)mA.c3; am._34 = (float)mA.c4;
						am._41 = (float)mA.d1; am._42 = (float)mA.d2; am._43 = (float)mA.d3; am._44 = (float)mA.d4;
						XMMATRIX A = XMLoadFloat4x4(&am);
						// 8 코너 변환 후 min/max
						XMFLOAT3 mn{ FLT_MAX, FLT_MAX, FLT_MAX };
						XMFLOAT3 mx{ -FLT_MAX, -FLT_MAX, -FLT_MAX };
						for (int ci = 0; ci < 8; ++ci)
						{
							XMVECTOR p = XMVectorSet(corners[ci].x, corners[ci].y, corners[ci].z, 1.0f);
							p = XMVector4Transform(p, A);
							XMFLOAT4 pf; XMStoreFloat4(&pf, p);
							mn.x = (std::min)(mn.x, pf.x); mn.y = (std::min)(mn.y, pf.y); mn.z = (std::min)(mn.z, pf.z);
							mx.x = (std::max)(mx.x, pf.x); mx.y = (std::max)(mx.y, pf.y); mx.z = (std::max)(mx.z, pf.z);
						}
						mdlPtr->animAabbMinSamples[(size_t)si] = mn;
						mdlPtr->animAabbMaxSamples[(size_t)si] = mx;
					}
				}
			}

		}

		if (mdlPtr->boundsValid && m_->m_LineRenderer && m_->m_pLineVS && m_->m_pLineInputLayout)
		{
			//라인 렌더링
			XMMATRIX S = XMMatrixScaling(mdlPtr->scale.x, mdlPtr->scale.y, mdlPtr->scale.z);
			XMMATRIX R = XMMatrixRotationQuaternion(XMQuaternionRotationRollPitchYaw(
				XMConvertToRadians(mdlPtr->rotDeg.x),
				XMConvertToRadians(mdlPtr->rotDeg.y),
				XMConvertToRadians(mdlPtr->rotDeg.z)
			));
			XMMATRIX T = XMMatrixTranslation(mdlPtr->pos.x, mdlPtr->pos.y, mdlPtr->pos.z);
			XMMATRIX W = S * R * T;
			// ConstantBuffer 설정, DrawLine 호출 등
			ID3D11Buffer* cbBones = mdlPtr->fbxBaseAnimator.GetBoneCB();
			// 라인용 상수버퍼: 이 모델의 월드 행렬 사용
			ConstantBuffer lineCB = m_->m_ConstantBuffer;
			lineCB.world = XMMatrixTranspose(W);
			lineCB.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
			lineCB.view = m_->m_baseProjection.view;
			lineCB.proj = m_->m_baseProjection.proj;
			lineCB.pad = 3.0f; // 라인 마커용
			int useBoneIdx = -1;
			if (cbBones != nullptr)
			{
				useBoneIdx = (mdlPtr->boundsBoneIndex >= 0) ? mdlPtr->boundsBoneIndex : 0;
			}
			lineCB.boundsBoneIndex = useBoneIdx;
			D3D11_MAPPED_SUBRESOURCE mappedLine;
			HR_T(m_->m_pDeviceContext->Map(m_->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedLine));
			memcpy_s(mappedLine.pData, sizeof(ConstantBuffer), &lineCB, sizeof(ConstantBuffer));
			m_->m_pDeviceContext->Unmap(m_->m_pConstantBuffer, 0);
			m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
			m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

			// 라인 VS/IL/b1 바인딩 저장/설정
			ID3D11VertexShader* prevVS = nullptr; m_->m_pDeviceContext->VSGetShader(&prevVS, nullptr, nullptr);
			ID3D11Buffer* prevVSb1 = nullptr; m_->m_pDeviceContext->VSGetConstantBuffers(1, 1, &prevVSb1);
			ID3D11InputLayout* prevIL = nullptr; m_->m_pDeviceContext->IAGetInputLayout(&prevIL);
			m_->m_pDeviceContext->VSSetShader(m_->m_pLineVS, nullptr, 0);
			m_->m_pDeviceContext->IASetInputLayout(m_->m_pLineInputLayout);
			// 선택 본 인덱스가 유효하면 스키닝 팔레트(b1)를 바인딩하여 본 변환을 적용
			if (useBoneIdx >= 0 && cbBones) m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &cbBones);

			// 로컬 AABB 8 코너 계산
			auto mn = mdlPtr->boundsMin; auto mx = mdlPtr->boundsMax;
			// 팔레트 기반 본 선택(-1 아님)일 때는 샘플 AABB를 사용하지 않음
			if (useBoneIdx < 0 && !mdlPtr->animAabbMinSamples.empty() && !mdlPtr->animAabbMaxSamples.empty() && mdlPtr->animAabbSampleDt > 0.0f)
			{
				double t = mdlPtr->fbxBaseAnimator.GetTimeSec();
				int idx = (int)std::floor(t / (double)mdlPtr->animAabbSampleDt + 0.5);
				if (idx < 0) idx = 0; if (idx >= (int)mdlPtr->animAabbMinSamples.size()) idx = (int)mdlPtr->animAabbMinSamples.size() - 1;
				mn = mdlPtr->animAabbMinSamples[(size_t)idx];
				mx = mdlPtr->animAabbMaxSamples[(size_t)idx];
			}
			XMFLOAT3 c[8] = {
				{mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z},
				{mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z}
			};
			XMFLOAT4 col = XMFLOAT4(1, 1, 0, 1); // 노란색
			// 12 엣지 드로우
			auto Draw = [&](const int& i, const int& j) {
				m_->m_LineRenderer->DrawLine(m_->m_pDeviceContext, c[i], c[j], col, m_->m_pLineInputLayout, m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);
				};
			// 박스 면 4개 선 그리기 (밑면)
			Draw(0, 1); Draw(1, 2); Draw(2, 3); Draw(3, 0);
			// 박스 면 4개 선 그리기 (윗면)
			Draw(4, 5); Draw(5, 6); Draw(6, 7); Draw(7, 4);
			// 박스 세로 4개 선 그리기 (밑면과 윗면 연결)
			Draw(0, 4); Draw(1, 5); Draw(2, 6); Draw(3, 7);

			// VS/IL/b1 복원
			if (prevVS) { m_->m_pDeviceContext->VSSetShader(prevVS, nullptr, 0); prevVS->Release(); }
			if (prevVSb1) { m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &prevVSb1); prevVSb1->Release(); }
			if (prevIL) { m_->m_pDeviceContext->IASetInputLayout(prevIL); prevIL->Release(); }
		}
	}
}

void App::PassShadow() {
	// 그림자 옵션이 꺼져있거나 모델이 없으면 패스
	if (!m_->m_ShadowEnabled || m_->m_Models.empty() || !m_->m_pShadowDSV)
		return;

	// 4번 슬롯에 묶인 섀도우 맵 SRV를 제거해주고 시작함.
	// 이전 프레임의 PassMainScene 단계에서 섀도우 맵 텍스처가
	// Pixel Shader의 4번 슬롯(Input)에 바인딩 되어 있음.
	// 다음 프레임의 PassShadow 단계가 시작될 때, 이 텍스처를 Depth Stencil로 output으로 설정하려고 하니
	// DirectX가 읽고 있는 중인 자원에 쓸 수 없다며 경고를 띄움. 이를 해결하기 위해 여기서 4번 슬롯을 해제하는걸로 함.
	ID3D11ShaderResourceView* const nullSRV[1] = { nullptr };
	m_->m_pDeviceContext->PSSetShaderResources(4, 1, nullSRV);

	// 1. 섀도우 맵 깊이 타겟 설정 (Color Target은 불필요하므로 nullptr)
	ID3D11RenderTargetView* nullRTV = nullptr;
	m_->m_pDeviceContext->OMSetRenderTargets(0, &nullRTV, m_->m_pShadowDSV);
	m_->m_pDeviceContext->ClearDepthStencilView(m_->m_pShadowDSV,
		D3D11_CLEAR_DEPTH, 1.0f, 0);

	// 2. 뷰포트 및 라스터라이저 설정
	m_->m_pDeviceContext->RSSetViewports(1, &m_->m_ShadowViewport);
	if (m_->RSShadowBias)
		m_->m_pDeviceContext->RSSetState(m_->RSShadowBias);

	// 3. 셰이더 설정 (PS는 필요 없음)
	m_->m_pDeviceContext->PSSetShader(nullptr, nullptr, 0);

	// 4. 모델 렌더링
	for (const auto& mdl : m_->m_Models) {
		if (!mdl->shared || !mdl->shared->vb)
			continue;

		// 월드 행렬 계산
		XMMATRIX S = XMMatrixScaling(mdl->scale.x, mdl->scale.y, mdl->scale.z);
		XMMATRIX R = XMMatrixRotationQuaternion(XMQuaternionRotationRollPitchYaw(
			XMConvertToRadians(mdl->rotDeg.x), XMConvertToRadians(mdl->rotDeg.y),
			XMConvertToRadians(mdl->rotDeg.z)));
		XMMATRIX T = XMMatrixTranslation(mdl->pos.x, mdl->pos.y, mdl->pos.z);
		XMMATRIX W = S * R * T;

		// 상수 버퍼 업데이트 (Light ViewProj 사용)
		ConstantBuffer cb = m_->m_ConstantBuffer;
		cb.world = XMMatrixTranspose(W);
		cb.lightViewProj = m_->m_baseProjection.lightViewProj;
		UpdateCB(m_->m_pConstantBuffer, cb);
		m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

		// 스키닝 여부 확인 및 셰이더 선택
		bool hasSkeleton = false;
		if (mdl->source == ModelSource::FBX && mdl->shared->fbx)
			hasSkeleton = mdl->shared->fbx->HasSkeleton();
		else if (mdl->source == ModelSource::PMX && mdl->shared->pmx)
			hasSkeleton = mdl->shared->pmx->HasSkeleton();

		bool useSkin =
			hasSkeleton && m_->m_pVSSkinnedShadow && mdl->fbxBaseAnimator.GetBoneCB();

		if (useSkin) {
			m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayoutSkinned);
			m_->m_pDeviceContext->VSSetShader(m_->m_pVSSkinnedShadow, nullptr, 0);
			ID3D11Buffer* boneCB = (mdl->source == ModelSource::PMX)
				? mdl->shared->pmx->GetBoneConstantBuffer()
				: mdl->fbxBaseAnimator.GetBoneCB();
			m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &boneCB);
		}
		else {
			m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayout);
			m_->m_pDeviceContext->VSSetShader(m_->m_pVSShadow, nullptr, 0);
		}

		// 그리기
		UINT stride = mdl->shared->stride;
		UINT offset = 0;
		m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &mdl->shared->vb, &stride,
			&offset);
		m_->m_pDeviceContext->IASetIndexBuffer(mdl->shared->ib,
			DXGI_FORMAT_R32_UINT, 0);

		for (const auto& sub : mdl->shared->subsets)
			m_->m_pDeviceContext->DrawIndexed(sub.count, sub.start, 0);
	}

	// 상태 복구 (RSState만 nullptr로, 뷰포트는 다음 패스에서 재설정)
	m_->m_pDeviceContext->RSSetState(nullptr);
}

void App::PassMainScene() {
	// 1. 뷰포트를 클라이언트 윈도우 크기로 복구
	D3D11_VIEWPORT vp{};
	vp.Width = static_cast<float>(m_ClientWidth);
	vp.Height = static_cast<float>(m_ClientHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_->m_pDeviceContext->RSSetViewports(1, &vp);

	// 디퍼드 렌더링 모드 확인
	if (m_->m_UseDeferredRendering) {
		// ========== G-Buffer 패스 ==========
		PassGBuffer();

		// ========== 라이트 패스 ==========
		PassDeferredLight();

		m_->m_pDeviceContext->OMSetDepthStencilState(m_->m_pDepthStencilState, 0);
		PassDebugDraw();

		// ========== 스카이박스 렌더링 (포워드) ==========
		// Light 패스가 끝난 후 깊이 버퍼는 G-Buffer의 깊이 정보를 유지하고 있음
		// 스카이박스는 빈 공간(Depth=1.0)에만 그려짐
		if (m_->m_SkyBoxChoice != App::Impl::SkyBoxChoice::Off && m_->m_Skybox) {
			// 혹시 모를 타겟 해제 방지를 위해 렌더 타겟 재설정
			// m_->m_pDeviceContext->OMSetRenderTargets(1,
			// &m_->m_pHdrRenderTargetView, m_->m_pDepthStencilView);
			m_->m_Skybox->Render(
				m_->m_pDeviceContext, m_->m_pVertexBuffer, m_->m_pIndexBuffer,
				m_->m_nIndices, m_->m_VertextBufferStride, m_->m_VertextBufferOffset,
				m_->m_baseProjection.view, m_->m_baseProjection.proj);
		}

		// ========== Axis Overlay ==========
		{
			ConstantBuffer overlayCB = m_->m_ConstantBuffer;
			overlayCB.world = XMMatrixTranspose(XMMatrixIdentity());
			overlayCB.view = XMMatrixTranspose(XMMatrixIdentity());
			overlayCB.proj = XMMatrixTranspose(XMMatrixIdentity());
			overlayCB.worldInvTranspose = XMMatrixIdentity();
			overlayCB.pad = 3.0f;
			overlayCB.shadingMode = (int)m_->m_ShadingMode;

			UpdateCB(m_->m_pConstantBuffer, overlayCB);
			m_->m_LineRenderer->DrawAxesOverlay(
				m_->m_pDeviceContext, XMMatrixTranspose(m_->m_baseProjection.view),
				DirectX::XMFLOAT2(-0.9f, 0.85f), 0.08f, m_->m_pLineInputLayout,
				m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);
		}
	}

	m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pHdrRenderTargetView,
		m_->m_pDepthStencilView);
	m_->m_pDeviceContext->ClearDepthStencilView(
		m_->m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f,
		0);
	// 포워드 렌더링
	m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShader, nullptr, 0);
	m_->m_pDeviceContext->PSSetShader(m_->m_pPixelShader, nullptr, 0);

	// 1. 전역 상수 버퍼 설정
	{
		ConstantBuffer& cb = m_->m_ConstantBuffer;
		cb.world = m_->m_baseProjection.world;
		cb.view = m_->m_baseProjection.view;
		cb.proj = m_->m_baseProjection.proj;
		cb.lightViewProj = m_->m_baseProjection.lightViewProj;
		cb.shadowBias = m_->m_ShadowBias;
		cb.shadowMapSize = (float)m_->m_ShadowSize;
		cb.shadowPCFRadius = m_->m_ShadowPCFRadius;
		cb.shadowEnabled = m_->m_ShadowEnabled;
		cb.worldInvTranspose =
			XMMatrixTranspose(XMMatrixInverse(nullptr, m_->m_baseProjection.world));

		XMFLOAT3 lightDir = m_->m_DirLight.direction;
		XMStoreFloat3(&lightDir, XMVector3Normalize(XMLoadFloat3(&lightDir)));
		cb.dirLight = m_->m_DirLight;
		cb.dirLight.direction = lightDir;

		cb.eyePos = m_Camera.GetPosition();
		cb.pad = 0.0f;
		cb.useTextureColor =
			m_->m_UseTextureColor; // 리팩토링 시 누락되었던 부분 복구

		const PBRMaterialCPU& defPbr = m_->m_DefaultPbrMaterial;
		cb.pbrBaseColor = defPbr.baseColor;
		cb.pbrMetalness = defPbr.metalness;
		cb.pbrRoughness = defPbr.roughness;
		cb.pbrAO = defPbr.ambientOcclusion;
		cb.shadingMode = (int)m_->m_ShadingMode;
		cb.enableNormalMap = 0;
		cb.useSpecularMap = 0;
		cb.useDiffuseMap = 1;

		cb.outlineThickness = m_->m_OutlineThickness;
		cb.outlineColor = m_->m_OutlineColor;
		cb.outlineStrength = m_->m_OutlineStrength;
		cb.material = m_->m_Material;

		UpdateCB(m_->m_pConstantBuffer, cb);
	}

	// 공통 리소스 바인딩
	m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetSamplers(0, 1, &m_->m_pSamplerState);
	if (m_->m_pShadowSampler) m_->m_pDeviceContext->PSSetSamplers(1, 1, &m_->m_pShadowSampler);
	if (m_->m_pShadowSRV) m_->m_pDeviceContext->PSSetShaderResources(4, 1, &m_->m_pShadowSRV);

	// 큐브맵(t1)과 IBL(t5~7) 바인딩.
	// 이후 루프에서 t1은 건드리면 안 됨.
	m_->m_pDeviceContext->PSSetShaderResources(1, 1, &m_->m_pTextureSRV);
	if (m_->m_pIblDiffuseSRV) m_->m_pDeviceContext->PSSetShaderResources(5, 1, &m_->m_pIblDiffuseSRV);
	if (m_->m_pIblSpecularSRV) m_->m_pDeviceContext->PSSetShaderResources(6, 1, &m_->m_pIblSpecularSRV);
	if (m_->m_pIblBrdfLutSRV) m_->m_pDeviceContext->PSSetShaderResources(7, 1, &m_->m_pIblBrdfLutSRV);

	// 2. 큐브 렌더링
	UINT stride = m_->m_VertextBufferStride;
	UINT offset = m_->m_VertextBufferOffset;
	for (auto& objPtr : m_->m_Objects) {
		if (!objPtr || objPtr->kind != ObjectKind::Cube)
			continue;
		auto* cubeObj = static_cast<CubeObject*>(objPtr.get());
		const Transform& mc = cubeObj->cubeTransform;

		m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pVertexBuffer,
			&stride, &offset);
		m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayout);
		m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pIndexBuffer,
			DXGI_FORMAT_R32_UINT, 0);

		ConstantBuffer cb = m_->m_ConstantBuffer;
		XMMATRIX Sm = XMMatrixScaling(mc.scale.x, mc.scale.y, mc.scale.z);
		XMMATRIX Rm = XMMatrixRotationQuaternion(
			XMQuaternionRotationRollPitchYaw(XMConvertToRadians(mc.rotationDeg.x),
				XMConvertToRadians(mc.rotationDeg.y),
				XMConvertToRadians(mc.rotationDeg.z)));
		XMMATRIX Tm = XMMatrixTranslation(mc.position.x, mc.position.y, mc.position.z);
		XMMATRIX W = Sm * Rm * Tm;

		cb.world = XMMatrixTranspose(W);
		cb.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
		cb.material.ambient = cubeObj->matAmbient;
		cb.material.diffuse = cubeObj->matDiffuse;
		cb.material.specular = cubeObj->matSpecular;
		cb.material.reflect = cubeObj->matReflect;
		cb.pad = 0.0f;
		cb.shadingMode = (int)m_->m_ShadingMode;
		cb.enableNormalMap = (cubeObj->useNormalMap != 0) ? 1 : 0;
		cb.useSpecularMap = (cubeObj->useSpecularMap != 0) ? 1 : 0;
		const PBRMaterialCPU& cubePbr = m_->m_DefaultPbrMaterial;
		cb.pbrBaseColor = cubePbr.baseColor;
		cb.pbrMetalness = cubePbr.metalness;
		cb.pbrRoughness = cubePbr.roughness;
		cb.pbrAO = cubePbr.ambientOcclusion;

		for (int face = 0; face < 6; ++face) {
			bool isTexCube = (cubeObj->cubeType == ECubeType::Texture);
			ID3D11ShaderResourceView* srvDiffuse = cubeObj->faceSRV[face]
				? cubeObj->faceSRV[face] : m_->m_pFallbackWhite;
			ID3D11ShaderResourceView* srvNormal =
				(isTexCube && cubeObj->useNormalMap != 0)
				? (cubeObj->normalSRV[face] ? cubeObj->normalSRV[face] : m_->m_pFallbackNormal)
				: nullptr;
			ID3D11ShaderResourceView* srvSpec =
				(isTexCube && cubeObj->useSpecularMap != 0)
				? (cubeObj->specSRV[face] ? cubeObj->specSRV[face] : m_->m_pFallbackWhite) : nullptr;

			cb.useDiffuseMap = (cubeObj->faceSRV[face] != nullptr) ? 1 : 0;
			cb.enableNormalMap = (isTexCube && cubeObj->useNormalMap != 0) ? 1 : 0;
			cb.useSpecularMap = (isTexCube && cubeObj->useSpecularMap != 0) ? 1 : 0;

			UpdateCB(m_->m_pConstantBuffer, cb);

			// 여기서 0번부터 4개를 한번에 바인딩하면 t1(큐브맵)이 nullptr로
			// 덮어씌워져 반사가 깨짐. 반드시 t0, t2, t3를 개별(혹은 t1을 피해) 바인딩
			// 해야 함.
			m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
			m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
			m_->m_pDeviceContext->PSSetShaderResources(3, 1, &srvSpec);

			m_->m_pDeviceContext->DrawIndexed(6, face * 6, 0);
		}
	}

	// 3. 3D 모델 렌더링
	if (!m_->m_Models.empty()) {
		for (auto& mdlPtr : m_->m_Models) {
			if (!mdlPtr->shared || !mdlPtr->shared->vb || !mdlPtr->shared->ib)
				continue;
			UINT s = mdlPtr->shared->stride;
			UINT o = 0;
			m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &mdlPtr->shared->vb, &s, &o);

			// 스켈레톤/쉐이더 선택
			ID3D11Buffer* cbBones = nullptr;
			bool hasSkeleton = false;
			if (mdlPtr->source == ModelSource::FBX && mdlPtr->shared->fbx) {
				cbBones = mdlPtr->fbxBaseAnimator.GetBoneCB();
				hasSkeleton = mdlPtr->shared->fbx->HasSkeleton();
			}
			else if (mdlPtr->source == ModelSource::PMX && mdlPtr->shared->pmx) {
				cbBones = mdlPtr->shared->pmx->GetBoneConstantBuffer();
				hasSkeleton = mdlPtr->shared->pmx->HasSkeleton();
			}
			bool useSkinned = hasSkeleton && (s == sizeof(VertexSkinnedTBN)) && m_->m_pInputLayoutSkinned && m_->m_pVertexShaderSkinned && cbBones;

			if (useSkinned) { 
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayoutSkinned);
				m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShaderSkinned, nullptr, 0);
				if (cbBones) {
					if (mdlPtr->source == ModelSource::PMX && mdlPtr->shared->pmx && !mdlPtr->shared->pmx->HasAnimations())
						mdlPtr->shared->pmx->UploadIdentityPalette(m_->m_pDeviceContext);
					m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &cbBones);
				}
			}
			else {
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayout);
				m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShader, nullptr, 0);
				ID3D11Buffer* nullCB = nullptr;
				m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &nullCB);
			}
			m_->m_pDeviceContext->IASetIndexBuffer(mdlPtr->shared->ib,
				DXGI_FORMAT_R32_UINT, 0);

			XMMATRIX W = mdlPtr->GetWorldMatrix();

			ConstantBuffer cb = m_->m_ConstantBuffer;
			cb.world = XMMatrixTranspose(W);
			cb.worldInvTranspose =
				XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
			cb.material = (mdlPtr->useInstanceMaterial ? mdlPtr->instanceMaterial : m_->m_Material);
			cb.shadingMode = (int)mdlPtr->modelShading;
			cb.enableNormalMap = m_->m_EnableNormalMapForCube;
			cb.useSpecularMap = m_->m_UseSpecularMapForCube;

			const PBRMaterialCPU& activePbr = mdlPtr->useInstancePbrMaterial? mdlPtr->instancePbrMaterial : m_->m_DefaultPbrMaterial;
			cb.pbrBaseColor = activePbr.baseColor;
			cb.pbrMetalness = activePbr.metalness;
			cb.pbrRoughness = activePbr.roughness;
			cb.pbrAO = activePbr.ambientOcclusion;

			UpdateCB(m_->m_pConstantBuffer, cb);

			for (const auto& sub : mdlPtr->shared->subsets) {
				ID3D11ShaderResourceView* srvDiffuse = nullptr;
				if (mdlPtr->shared && sub.materialIndex < mdlPtr->shared->materialSRVs.size())
					srvDiffuse = mdlPtr->shared->materialSRVs[sub.materialIndex];
				if (!srvDiffuse) 
					srvDiffuse = m_->m_pFallbackWhite;

				ID3D11ShaderResourceView* srvNormal = nullptr;
				if (m_->m_EnableNormalMapForCube != 0) {
					if (mdlPtr->shared && sub.materialIndex < mdlPtr->shared->normalSRVs.size())
						srvNormal = mdlPtr->shared->normalSRVs[sub.materialIndex];
					if (!srvNormal)
						srvNormal = m_->m_pFallbackNormal;
				}
				ID3D11ShaderResourceView* srvSpec = (m_->m_UseSpecularMapForCube != 0) ? m_->m_pFallbackWhite : nullptr;

				// 개별로 바인딩함
				m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
				m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
				m_->m_pDeviceContext->PSSetShaderResources(3, 1, &srvSpec);
				m_->m_pDeviceContext->DrawIndexed(sub.count, sub.start, 0);
			}
		}
	}

	// VS 복구
	m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShader, nullptr, 0);
	ID3D11Buffer* nullCB = nullptr;
	m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &nullCB);

	// 4. SkyBox 렌더링
	if (m_->m_SkyBoxChoice != App::Impl::SkyBoxChoice::Off && m_->m_Skybox) {
		m_->m_Skybox->Render(m_->m_pDeviceContext, m_->m_pVertexBuffer,
			m_->m_pIndexBuffer, m_->m_nIndices,
			m_->m_VertextBufferStride, m_->m_VertextBufferOffset,
			m_->m_baseProjection.view, m_->m_baseProjection.proj);
	}

	// 5. Axis Overlay
	{
		ConstantBuffer overlayCB = m_->m_ConstantBuffer;
		overlayCB.world = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.view = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.proj = XMMatrixTranspose(XMMatrixIdentity());
		overlayCB.worldInvTranspose = XMMatrixIdentity();
		overlayCB.pad = 3.0f;
		overlayCB.shadingMode = (int)m_->m_ShadingMode;

		UpdateCB(m_->m_pConstantBuffer, overlayCB);
		m_->m_LineRenderer->DrawAxesOverlay(
			m_->m_pDeviceContext, XMMatrixTranspose(m_->m_baseProjection.view),
			DirectX::XMFLOAT2(-0.9f, 0.85f), 0.08f, m_->m_pLineInputLayout,
			m_->m_pLineVS, m_->m_pPixelShader, m_->m_pConstantBuffer);
	}

	PassDebugDraw();
}

// G-Buffer 패스: 지오메트리 정보를 G-Buffer에 렌더링
void App::PassGBuffer() {
	// 2. G-Buffer 클리어 및 타겟 설정
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };  // BaseColor Alpha=0
	float clearNormal[4] = { 0.5f, 0.5f, 0.5f, 1.0f }; // Normal 0 vector

	// 루프 대신 memset이나 fill을 사용하지 않고 처리
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pGBufferRTVs[0].Get(), clearColor); // Pos
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pGBufferRTVs[1].Get(), clearNormal); // Normal
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pGBufferRTVs[2].Get(), clearColor); // Metal
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pGBufferRTVs[3].Get(), clearColor); // Rough
	m_->m_pDeviceContext->ClearRenderTargetView(m_->m_pGBufferRTVs[4].Get(), clearColor); // Color

	m_->m_pDeviceContext->ClearDepthStencilView(m_->m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	ID3D11RenderTargetView* rtvs[Impl::GBufferCount] = {
		m_->m_pGBufferRTVs[0].Get(), m_->m_pGBufferRTVs[1].Get(),
		m_->m_pGBufferRTVs[2].Get(), m_->m_pGBufferRTVs[3].Get(),
		m_->m_pGBufferRTVs[4].Get() };
	m_->m_pDeviceContext->OMSetRenderTargets(Impl::GBufferCount, rtvs,
		m_->m_pDepthStencilView);
	m_->m_pDeviceContext->OMSetDepthStencilState(m_->m_pDepthStencilState, 0);

	// 3. 파이프라인 설정
	m_->m_pDeviceContext->VSSetShader(m_->m_pGBufferVS, nullptr, 0);
	m_->m_pDeviceContext->PSSetShader(m_->m_pGBufferPS, nullptr, 0);
	m_->m_pDeviceContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 4. 전역 상수 버퍼 업데이트
	ConstantBuffer& cb = m_->m_ConstantBuffer;
	cb.world = m_->m_baseProjection.world;
	cb.view = m_->m_baseProjection.view;
	cb.proj = m_->m_baseProjection.proj;
	cb.worldInvTranspose = XMMatrixTranspose(XMMatrixInverse(nullptr, m_->m_baseProjection.world));
	cb.eyePos = m_Camera.GetPosition();
	cb.useTextureColor = m_->m_UseTextureColor;

	// 기본 재질 설정
	const PBRMaterialCPU& defPbr = m_->m_DefaultPbrMaterial;
	cb.pbrBaseColor = defPbr.baseColor;
	cb.pbrMetalness = defPbr.metalness;
	cb.pbrRoughness = defPbr.roughness;
	cb.pbrAO = defPbr.ambientOcclusion;
	cb.enableNormalMap = 0;
	cb.useDiffuseMap = 1;

	UpdateCB(m_->m_pConstantBuffer, cb);

	m_->m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);
	m_->m_pDeviceContext->PSSetSamplers(0, 1, &m_->m_pSamplerState);

	// 큐브 렌더링
	UINT stride = m_->m_VertextBufferStride;
	UINT offset = m_->m_VertextBufferOffset;
	for (auto& objPtr : m_->m_Objects) {
		if (!objPtr || objPtr->kind != ObjectKind::Cube)
			continue;
		auto* cubeObj = static_cast<CubeObject*>(objPtr.get());
		const Transform& mc = cubeObj->cubeTransform;

		m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pVertexBuffer, &stride, &offset);
		m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

		ConstantBuffer cb = m_->m_ConstantBuffer;
		XMMATRIX Sm = XMMatrixScaling(mc.scale.x, mc.scale.y, mc.scale.z);
		XMMATRIX Rm = XMMatrixRotationQuaternion(
			XMQuaternionRotationRollPitchYaw(XMConvertToRadians(mc.rotationDeg.x),
				XMConvertToRadians(mc.rotationDeg.y),
				XMConvertToRadians(mc.rotationDeg.z)));
		XMMATRIX Tm = XMMatrixTranslation(mc.position.x, mc.position.y, mc.position.z);
		XMMATRIX W = Sm * Rm * Tm;

		cb.world = XMMatrixTranspose(W);
		cb.worldInvTranspose =
			XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));
		const PBRMaterialCPU& cubePbr = m_->m_DefaultPbrMaterial;
		cb.pbrBaseColor = cubePbr.baseColor;
		cb.pbrMetalness = cubePbr.metalness;
		cb.pbrRoughness = cubePbr.roughness;
		cb.enableNormalMap = (cubeObj->useNormalMap != 0) ? 1 : 0;

		for (int face = 0; face < 6; ++face) {
			bool isTexCube = (cubeObj->cubeType == ECubeType::Texture);
			ID3D11ShaderResourceView* srvDiffuse = cubeObj->faceSRV[face]
				? cubeObj->faceSRV[face] : m_->m_pFallbackWhite;
			ID3D11ShaderResourceView* srvNormal =
				(isTexCube && cubeObj->useNormalMap != 0)
				? (cubeObj->normalSRV[face] ? cubeObj->normalSRV[face] : m_->m_pFallbackNormal) : nullptr;

			cb.useDiffuseMap = (cubeObj->faceSRV[face] != nullptr) ? 1 : 0;
			cb.enableNormalMap = (isTexCube && cubeObj->useNormalMap != 0) ? 1 : 0;

			UpdateCB(m_->m_pConstantBuffer, cb);
			m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvDiffuse);
			m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvNormal);
			m_->m_pDeviceContext->DrawIndexed(6, face * 6, 0);
		}
	}

	// 3D 모델 렌더링
	if (!m_->m_Models.empty()) {
		for (auto& mdlPtr : m_->m_Models) {
			if (!mdlPtr->shared || !mdlPtr->shared->vb)
				continue;

			// VS, InputLayout 설정 (스키닝 여부 체크)
			bool hasSkeleton = false;
			if (mdlPtr->source == ModelSource::FBX && mdlPtr->shared->fbx)
				hasSkeleton = mdlPtr->shared->fbx->HasSkeleton();
			else if (mdlPtr->source == ModelSource::PMX && mdlPtr->shared->pmx)
				hasSkeleton = mdlPtr->shared->pmx->HasSkeleton();

			ID3D11Buffer* cbBones = mdlPtr->fbxBaseAnimator.GetBoneCB();
			bool useSkinned = hasSkeleton && m_->m_pVertexShaderSkinned && cbBones;

			if (useSkinned) {
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pInputLayoutSkinned);
				// G-Buffer용 스키닝 VS가 별도로 없다면 기존 스키닝 VS 사용하되
				// G-Buffer PS와 호환되는지 확인 필요. 보통 G-Buffer 출력을 위한 별도의
				// Skinned VS를 만듭니다. 만약 없다면 기본 Skinned VS가 VertexOut
				// 구조체를 공유하는지 확인하세요.
				m_->m_pDeviceContext->VSSetShader(m_->m_pVertexShaderSkinned, nullptr,
					0);
				m_->m_pDeviceContext->VSSetConstantBuffers(1, 1, &cbBones);
			}
			else {
				m_->m_pDeviceContext->IASetInputLayout(m_->m_pGBufferInputLayout);
				m_->m_pDeviceContext->VSSetShader(m_->m_pGBufferVS, nullptr, 0);
			}

			// 버퍼 바인딩
			UINT s = mdlPtr->shared->stride, o = 0;
			m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &mdlPtr->shared->vb, &s,
				&o);
			m_->m_pDeviceContext->IASetIndexBuffer(mdlPtr->shared->ib,
				DXGI_FORMAT_R32_UINT, 0);

			// 월드 행렬
			XMMATRIX W = mdlPtr->GetWorldMatrix(); // (Scale * Rot * Trans)
			ConstantBuffer cbM = m_->m_ConstantBuffer;
			cbM.world = XMMatrixTranspose(W);
			cbM.worldInvTranspose =
				XMMatrixTranspose(XMMatrixInverse(nullptr, XMMatrixTranspose(W)));

			const PBRMaterialCPU& activePbr = mdlPtr->useInstancePbrMaterial
				? mdlPtr->instancePbrMaterial
				: m_->m_DefaultPbrMaterial;
			cbM.pbrBaseColor = activePbr.baseColor;
			cbM.pbrMetalness = activePbr.metalness;
			cbM.pbrRoughness = activePbr.roughness;
			cbM.enableNormalMap =
				m_->m_EnableNormalMapForCube; // 모델별 설정 확인 필요함

			UpdateCB(m_->m_pConstantBuffer, cbM);

			// 서브셋 그리기
			for (const auto& sub : mdlPtr->shared->subsets) {
				// 재질 바인딩 (Diffuse, Normal)
				ID3D11ShaderResourceView* srvD =
					(sub.materialIndex < mdlPtr->shared->materialSRVs.size())
					? mdlPtr->shared->materialSRVs[sub.materialIndex]
					: m_->m_pFallbackWhite;
				ID3D11ShaderResourceView* srvN =
					(sub.materialIndex < mdlPtr->shared->normalSRVs.size())
					? mdlPtr->shared->normalSRVs[sub.materialIndex]
					: m_->m_pFallbackNormal;

				m_->m_pDeviceContext->PSSetShaderResources(0, 1, &srvD);
				m_->m_pDeviceContext->PSSetShaderResources(2, 1, &srvN);
				m_->m_pDeviceContext->DrawIndexed(sub.count, sub.start, 0);
			}
		}

		// RTV 해제
		ID3D11RenderTargetView* nullRTVs[Impl::GBufferCount] = { nullptr };
		m_->m_pDeviceContext->OMSetRenderTargets(Impl::GBufferCount, nullRTVs,
			nullptr);
	}
}

// 디퍼드 라이트 패스: G-Buffer를 읽어서 조명 계산
void App::PassDeferredLight() {
	// 1. HDR 타겟 설정 (Depth Stencil View는 바인딩하되 쓰기는 금지해야 함)
	m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pHdrRenderTargetView, m_->m_pDepthStencilView);

	// Depth Write 끄기: Quad가 깊이 버퍼(z=0)를 덮어쓰면 스카이박스(z=1)가 가려짐
	// Impl에 명시적인 DepthDisable 상태가 없다면 nullptr로 테스트 자체를 끄는
	// 것이 안전함
	m_->m_pDeviceContext->OMSetDepthStencilState(m_->m_pDepthStencilStateReadOnly, 0);

	// 2. 텍스처 바인딩 (G-Buffer 5개 + IBL 3개 + ShadowMap 1개)
	std::vector<ID3D11ShaderResourceView*> srvs = {
		m_->m_pGBufferSRVs[0].Get(),
		m_->m_pGBufferSRVs[1].Get(),
		m_->m_pGBufferSRVs[2].Get(),
		m_->m_pGBufferSRVs[3].Get(),
		m_->m_pGBufferSRVs[4].Get(),
		m_->m_pIblDiffuseSRV,
		m_->m_pIblSpecularSRV,
		m_->m_pIblBrdfLutSRV,
		m_->m_pShadowSRV // t8: 섀도우 맵 바인딩 필수
	};
	m_->m_pDeviceContext->PSSetShaderResources(0, static_cast<UINT>(srvs.size()), srvs.data());

	// 3. 샘플러 설정 (Shadow Sampler 포함)
	ID3D11SamplerState* samplers[] = { m_->m_pSamplerState, m_->m_pShadowSampler, m_->m_pSamplerLinear };
	m_->m_pDeviceContext->PSSetSamplers(0, 3, samplers);

	// 4. 상수 버퍼 업데이트 (CameraPos, ShadowInfo, LightMatrix)
	ConstantBuffer cb = m_->m_ConstantBuffer;
	cb.eyePos = m_Camera.GetPosition();
	cb.lightViewProj = m_->m_baseProjection.lightViewProj; // 그림자 판별용 행렬
	cb.shadowMapSize = (float)m_->m_ShadowSize;
	cb.shadowEnabled = m_->m_ShadowEnabled;
	cb.shadowPCFRadius = m_->m_ShadowPCFRadius;
	cb.shadowBias = m_->m_ShadowBias;
	UpdateCB(m_->m_pConstantBuffer, cb);
	m_->m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_->m_pConstantBuffer);

	// Directional Light 정보 업데이트
	{
		XMFLOAT3 L = m_->m_DirLight.direction;
		XMStoreFloat3(&L, XMVector3Normalize(XMLoadFloat3(&L)));
		struct DirLightCB {
			XMFLOAT4 dir;
			XMFLOAT4 color;
			float intensity;
			float padding[3];
		} lcb;
		lcb.dir = { L.x, L.y, L.z, 1.0f };
		lcb.color = { m_->m_DirLight.diffuse.x, m_->m_DirLight.diffuse.y, m_->m_DirLight.diffuse.z, 1.0f };
		lcb.intensity = m_->m_DirLight.intensity;
		UpdateCB(m_->m_pDirectionalLightBuffer, lcb);
		m_->m_pDeviceContext->PSSetConstantBuffers(3, 1, &m_->m_pDirectionalLightBuffer);
	}

	// 5. FullScreen Quad 그리기
	m_->m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_->m_pDeviceContext->IASetInputLayout(m_->m_pQuadInputLayout);
	m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pQuadVertexBuffer,
		&m_->m_QuadVertexBufferStride,
		&m_->m_QuadVertexBufferOffset);
	m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pQuadIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	m_->m_pDeviceContext->VSSetShader(m_->m_pQuadVertexShader, nullptr, 0);
	m_->m_pDeviceContext->PSSetShader(m_->m_pDeferredLightPS, nullptr, 0);
	m_->m_pDeviceContext->DrawIndexed(m_->m_nQuadIndices, 0, 0);

	// 6. 리소스 해제 (다음 패스 쓰기 충돌 방지)
	std::vector<ID3D11ShaderResourceView*> nullSRVs(9, nullptr);
	m_->m_pDeviceContext->PSSetShaderResources(0, 9, nullSRVs.data());

	// Depth Stencil 복구 (Skybox 렌더링 등을 위해)
	m_->m_pDeviceContext->OMSetDepthStencilState(m_->m_pDepthStencilState, 0);
}

void App::PassPostProcess() {
	// 1. 렌더 타겟 설정 (BackBuffer)
	ID3D11RenderTargetView* nullRTV = nullptr;
	m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pRenderTargetView,
		nullptr);

	// 2. 뷰포트 재설정 혹시 다른 패스에서 뷰포트가 변경되었을 경우
	D3D11_VIEWPORT vp{};
	vp.Width = static_cast<float>(m_ClientWidth);
	vp.Height = static_cast<float>(m_ClientHeight);
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	m_->m_pDeviceContext->RSSetViewports(1, &vp);

	// 3. 톤매핑 상수 버퍼 업데이트
	m_->m_PostProcessConstantBuffer.g_Exposure = m_->m_Exposure;
	m_->m_PostProcessConstantBuffer.g_MaxHDRNits = m_->m_MonitorMaxNits;
	UpdateCB(m_->m_pPostProcessConstantBuffer, m_->m_PostProcessConstantBuffer);
	m_->m_pDeviceContext->PSSetConstantBuffers(2, 1,
		&m_->m_pPostProcessConstantBuffer);

	// 4. Quad 렌더링 준비
	m_->m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_->m_pDeviceContext->IASetVertexBuffers(0, 1, &m_->m_pQuadVertexBuffer,
		&m_->m_QuadVertexBufferStride,
		&m_->m_QuadVertexBufferOffset);
	m_->m_pDeviceContext->IASetInputLayout(m_->m_pQuadInputLayout);
	m_->m_pDeviceContext->IASetIndexBuffer(m_->m_pQuadIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// 5. 셰이더 선택 HDR 지원 모니터 여부 확인
	m_->m_pDeviceContext->VSSetShader(m_->m_pQuadVertexShader, nullptr, 0);
	if (m_->m_format == DXGI_FORMAT_R16G16B16A16_FLOAT || m_->m_format == DXGI_FORMAT_R10G10B10A2_UNORM)
		m_->m_pDeviceContext->PSSetShader(m_->m_pPS_ToneMappingHDR, nullptr, 0);
	else
		m_->m_pDeviceContext->PSSetShader(m_->m_pPS_ToneMappingLDR, nullptr, 0);

	// 6. HDR 텍스처 바인딩 및 그리기
	m_->m_pDeviceContext->PSSetShaderResources(8, 1, &m_->m_pHdrShaderResourceView);
	m_->m_pDeviceContext->PSSetSamplers(2, 1, &m_->m_pSamplerLinear);

	m_->m_pDeviceContext->DrawIndexed(m_->m_nQuadIndices, 0, 0);

	// 7. 리소스 해제
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	m_->m_pDeviceContext->PSSetShaderResources(8, 1, nullSRV);
}

void App::PassUI() {
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	RenderControlPannel();
	RenderSceneCollection();
	RenderModelPannel();
	RenderAdvancedRigUI();
	RenderConsolPannel();
	m_->m_SystemInfo.RenderUI();
	RenderSceneImageWindow();
	RenderDeferredUI();

	// Sniper UI Overlay
	if (m_->m_SniperEnabled && m_->m_SniperCharging)
	{
		ImDrawList* dl = ImGui::GetForegroundDrawList();
		ImVec2 p = m_->m_SniperAimPos;

		const float r = m_->m_SniperAimRadius;
		dl->AddCircle(p, r, IM_COL32(255,255,255,220), 32, 2.0f);

		// 게이지 바
		ImVec2 barSize(80.0f, 7.0f);
		ImVec2 barPos(p.x - barSize.x * 0.5f, p.y + r + 10.0f);

		dl->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y),
		                  IM_COL32(0,0,0,160), 2.0f);

		dl->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x * m_->m_SniperCharge01, barPos.y + barSize.y),
		                  IM_COL32(255,255,255,220), 2.0f);

		dl->AddRect(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y),
		            IM_COL32(255,255,255,220), 2.0f);
	}

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool App::InitD3D() {
	// HDR 지원 여부를 확인함
	HRESULT hr = S_OK;
	DXGI_FORMAT result;
	m_->m_isHDRSupported =
		CheckHDRSupportAndGetMaxNits(m_->m_MonitorMaxNits, result);

	if (m_->m_isHDRSupported)
		CreateSwapChainAndBackBuffer(DXGI_FORMAT_R10G10B10A2_UNORM); // HDR
	else
		CreateSwapChainAndBackBuffer(DXGI_FORMAT_R8G8B8A8_UNORM); // LDR

	// 깊이 스텐실 텍스처/뷰 생성
	D3D11_TEXTURE2D_DESC dsDesc = {};
	dsDesc.Width = m_ClientWidth;
	dsDesc.Height = m_ClientHeight;
	dsDesc.MipLevels = 1;
	dsDesc.ArraySize = 1;
	dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsDesc.SampleDesc.Count = 1;
	dsDesc.SampleDesc.Quality = 0;
	dsDesc.Usage = D3D11_USAGE_DEFAULT;
	dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	dsDesc.CPUAccessFlags = 0;
	dsDesc.MiscFlags = 0;

	ID3D11Texture2D* pDepthStencil = nullptr;
	HR_T(m_->m_pDevice->CreateTexture2D(&dsDesc, nullptr, &pDepthStencil));
	HR_T(m_->m_pDevice->CreateDepthStencilView(pDepthStencil, nullptr,
		&m_->m_pDepthStencilView));
	SAFE_RELEASE(pDepthStencil);

	// DepthStencilState 생성 및 설정
	D3D11_DEPTH_STENCIL_DESC dssDesc = {};
	dssDesc.DepthEnable = TRUE;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS;
	dssDesc.StencilEnable = FALSE;
	HR_T(m_->m_pDevice->CreateDepthStencilState(&dssDesc,
		&m_->m_pDepthStencilState));
	m_->m_pDeviceContext->OMSetDepthStencilState(m_->m_pDepthStencilState, 0);

	// Outline용: 깊이 읽기 전용(LESS_EQUAL), 깊이 쓰기 금지
	dssDesc = {};
	dssDesc.DepthEnable = TRUE;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS;
	dssDesc.StencilEnable = FALSE;
	HR_T(m_->m_pDevice->CreateDepthStencilState(
		&dssDesc, &m_->m_pDepthStencilStateReadOnly));

	// 렌더 타겟/DSV 바인딩
	m_->m_pDeviceContext->OMSetRenderTargets(1, &m_->m_pRenderTargetView,
		m_->m_pDepthStencilView);

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
	HR_T(m_->m_pDevice->CreateRasterizerState(&rasterizerDesc,
		&m_->RSCullClockWise));
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

	// ====================================== HDR Render Target 생성
	// ====================================== HDR 씬 렌더링을 위한
	// R16G16B16A16_FLOAT 포맷의 렌더 타겟 생성 이 텍스처는 1.0을 넘는 HDR 값을
	// 저장할 수 있으며, 이후 톤매핑 패스에서 샘플링됨
	D3D11_TEXTURE2D_DESC hdrRTDesc = {};
	hdrRTDesc.Width = static_cast<UINT>(m_ClientWidth);
	hdrRTDesc.Height = static_cast<UINT>(m_ClientHeight);
	hdrRTDesc.MipLevels = 1;
	hdrRTDesc.ArraySize = 1;
	hdrRTDesc.Format =
		DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR 값을 저장할 수 있는 포맷
	hdrRTDesc.SampleDesc.Count = 1;     // MSAA 없음
	hdrRTDesc.SampleDesc.Quality = 0;
	hdrRTDesc.Usage = D3D11_USAGE_DEFAULT;
	hdrRTDesc.BindFlags = D3D11_BIND_RENDER_TARGET |
		D3D11_BIND_SHADER_RESOURCE; // RTV와 SRV 모두 가능

	HR_T(m_->m_pDevice->CreateTexture2D(&hdrRTDesc, nullptr,
		&m_->m_pHdrRenderTarget));
	HR_T(m_->m_pDevice->CreateRenderTargetView(m_->m_pHdrRenderTarget, nullptr,
		&m_->m_pHdrRenderTargetView));
	HR_T(m_->m_pDevice->CreateShaderResourceView(m_->m_pHdrRenderTarget, nullptr,
		&m_->m_pHdrShaderResourceView));

	// ====================================== 톤매핑용 선형 샘플러 생성
	// ====================================== 톤매핑 패스에서 HDR 텍스처를
	// 샘플링할 때 사용할 선형 필터링 샘플러
	D3D11_SAMPLER_DESC samplerLinearDesc = {};
	samplerLinearDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerLinearDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; // 가장자리 클램프
	samplerLinearDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerLinearDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerLinearDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerLinearDesc.MinLOD = 0;
	samplerLinearDesc.MaxLOD = D3D11_FLOAT32_MAX;
	HR_T(m_->m_pDevice->CreateSamplerState(&samplerLinearDesc,
		&m_->m_pSamplerLinear));

	return true;
}

void App::UninitD3D() {
	// 파이프라인 바인딩 참조 제거(잔존 참조로 인한 라이브 오브젝트 감소)
	if (m_->m_pDeviceContext) {
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
bool App::InitScene() {
	// HRESULT hr = 0;
	ID3D10Blob* errorMessage = nullptr; // 에러 메시지를 저장할 버퍼.

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
	StaticMesh::AssignIndexMemory(m_->m_pDevice, m_->m_pIndexBuffer, cube,
		m_->m_nIndices);

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

	// 디퍼드 라이트 패스용 상수 버퍼 생성 (b3)
	cbd.ByteWidth = sizeof(XMFLOAT4) * 3; // DirectionalLightBuffer: float4 * 2
	HR_T(m_->m_pDevice->CreateBuffer(&cbd, nullptr,
		&m_->m_pDirectionalLightBuffer));

	// ***********************************************************************************************
	// 스카이 박스 큐브 설정
	HR_T(CreateDDSTextureFromFile(m_->m_pDevice,
		L"..\\Resource\\Skybox\\Hanako.dds", nullptr,
		&m_->m_pSkyHanakoSRV));
	HR_T(CreateDDSTextureFromFile(m_->m_pDevice,
		L"..\\Resource\\Skybox\\cubemap.dds", nullptr,
		&m_->m_pSkyCubeMapSRV));
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
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R32_TYPELESS;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		HR_T(m_->m_pDevice->CreateTexture2D(&td, nullptr, &m_->m_pShadowTex));

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
		dsvd.Format = DXGI_FORMAT_D32_FLOAT;
		dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		HR_T(m_->m_pDevice->CreateDepthStencilView(m_->m_pShadowTex, &dsvd,
			&m_->m_pShadowDSV));

		D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
		srvd.Format = DXGI_FORMAT_R32_FLOAT;
		srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvd.Texture2D.MipLevels = 1;
		HR_T(m_->m_pDevice->CreateShaderResourceView(m_->m_pShadowTex, &srvd,
			&m_->m_pShadowSRV));

		// Shadow viewport
		m_->m_ShadowViewport = {
			0.0f, 0.0f, (float)m_->m_ShadowSize, (float)m_->m_ShadowSize,
			0.0f, 1.0f };

		// Depth-bias rasterizer
		D3D11_RASTERIZER_DESC rd{};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_BACK;
		rd.FrontCounterClockwise = true;
		rd.DepthBias = 1000;
		rd.SlopeScaledDepthBias = 1.0f;
		rd.DepthBiasClamp = 0.0f;
		rd.DepthClipEnable = TRUE;
		rd.MultisampleEnable = FALSE;
		rd.ScissorEnable = FALSE;
		rd.AntialiasedLineEnable = FALSE;
		HR_T(m_->m_pDevice->CreateRasterizerState(&rd, &m_->RSShadowBias));

		// Shadow sampler (linear, clamp)
		D3D11_SAMPLER_DESC ssd{};
		ssd.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		ssd.AddressU = ssd.AddressV = ssd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		ssd.MaxLOD = D3D11_FLOAT32_MAX;
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
	m_->m_baseProjection.worldInvTranspose =
		XMMatrixInverse(nullptr, XMMatrixTranspose(m_->m_baseProjection.world));
	// DirectionalLight 초기값 필드 대입
	m_->m_baseProjection.dirLight.ambient = DirectX::XMFLOAT4(0, 0, 0, 1);
	m_->m_baseProjection.dirLight.diffuse = DirectX::XMFLOAT4(1, 1, 1, 1);
	m_->m_baseProjection.dirLight.specular = DirectX::XMFLOAT4(1, 1, 1, 1);
	m_->m_baseProjection.dirLight.direction = DirectX::XMFLOAT3(0, -1, 1);
	m_->m_baseProjection.dirLight.intensity = 1.0f;
	m_->m_baseProjection.eyePos = m_Camera.GetPosition();
	m_->m_baseProjection.pad = 0.0f;

	// ***********************************************************************************************
	// 유틸 초기화. 라인 렌더러, 스카이박스, 디버그 박스
	if (!m_->m_LineRenderer)
		m_->m_LineRenderer = new LineRenderer();
	m_->m_LineRenderer->Initialize(m_->m_pDevice);

	// Skybox는 기존 Hanako를 기본으로 초기화
	if (!m_->m_Skybox)
		m_->m_Skybox = new Skybox();
	// Skybox는 이미 CreateDDSTextureFromFile로 SRV가 생성되어 있으므로, 여기선
	// 현재 선택된 SRV를 사용하도록 Initialize는 경로 기반 대신 스킵할 수
	// 있습니다. 간편화를 위해 cubemap.dds로 초기화
	m_->m_Skybox->Initialize(m_->m_pDevice, m_->m_CurrentSkyboxPath,
		m_->m_pSkyBoxVertexShader, m_->m_pSkyBoxPixelShader,
		m_->m_pSkyBoxInputLayout, m_->m_pConstantBuffer);

	// Debug box buffers for light position marker
	StaticMesh::CreateDebugBoxBuffersLightTex(
		m_->m_pDevice, XMFLOAT4(1, 1, 1, 1), 0.2f, &m_->m_pDebugBoxVB,
		&m_->m_pDebugBoxIB, &m_->m_DebugBoxIndexCount);

	return true;
}

void App::UninitScene() {
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
	for (auto& obj : m_->m_Objects) {
		if (!obj || obj->kind != ObjectKind::Cube)
			continue;
		auto* co = static_cast<CubeObject*>(obj.get());
		for (int i = 0; i < 6; ++i) {
			if (co->faceSRV[i] && co->faceSRV[i] != m_->m_pFallbackWhite &&
				co->faceSRV[i] != m_->m_pFallbackNormal &&
				co->faceSRV[i] != m_->m_pFallbackBlack) {
				SAFE_RELEASE(co->faceSRV[i]);
			}
			if (co->normalSRV[i] && co->normalSRV[i] != m_->m_pFallbackWhite &&
				co->normalSRV[i] != m_->m_pFallbackNormal &&
				co->normalSRV[i] != m_->m_pFallbackBlack) {
				SAFE_RELEASE(co->normalSRV[i]);
			}
			if (co->specSRV[i] && co->specSRV[i] != m_->m_pFallbackWhite &&
				co->specSRV[i] != m_->m_pFallbackNormal &&
				co->specSRV[i] != m_->m_pFallbackBlack) {
				SAFE_RELEASE(co->specSRV[i]);
			}
		}
	}
	for (int i = 0; i < 6; ++i)
		SAFE_RELEASE(m_->m_pSkyFaceSRV[i]);

	SAFE_RELEASE(m_->m_pFallbackWhite);
	SAFE_RELEASE(m_->m_pFallbackNormal);
	SAFE_RELEASE(m_->m_pFallbackBlack);

	SAFE_RELEASE(m_->m_pDebugBoxVB);
	SAFE_RELEASE(m_->m_pDebugBoxIB);
	if (m_->m_LineRenderer) {
		m_->m_LineRenderer->Release();
		delete m_->m_LineRenderer;
		m_->m_LineRenderer = nullptr;
	}
	if (m_->m_Skybox) {
		m_->m_Skybox->Release();
		delete m_->m_Skybox;
		m_->m_Skybox = nullptr;
	}

	SAFE_RELEASE(m_->m_pPS_ToneMappingLDR);
	SAFE_RELEASE(m_->m_pPS_ToneMappingHDR);

	// 모델 리소스 해제
	UnloadModel();
}

bool App::InitTexture() {
	PrepareSkyFaceSRVs();

	// CubeObject 내부 경로 기반으로 텍스처 로드(있을 때만)
	for (auto& obj : m_->m_Objects) {
		if (!obj || obj->kind != ObjectKind::Cube)
			continue;
		auto* co = static_cast<CubeObject*>(obj.get());
		for (int i = 0; i < 6; ++i) {
			Microsoft::WRL::ComPtr<ID3D11Resource> res;
			if (co->facePaths[i]) {
				HR_T(CreateWICTextureFromFile(m_->m_pDevice, co->facePaths[i],
					res.GetAddressOf(), &co->faceSRV[i]));
				res.Reset();
			}
			if (co->normalPaths[i]) {
				HR_T(CreateWICTextureFromFile(m_->m_pDevice, co->normalPaths[i],
					res.GetAddressOf(), &co->normalSRV[i]));
				res.Reset();
			}
			if (co->specularPaths[i]) {
				HR_T(CreateWICTextureFromFile(m_->m_pDevice, co->specularPaths[i],
					res.GetAddressOf(), &co->specSRV[i]));
				res.Reset();
			}
		}
	}
	return true;
}

bool App::InitImGui() {
	// ImGui 초기화
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// 한글/일본어 표시를 위한 방법
	{
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->AddFontDefault();
		ImFontConfig cfg{};
		cfg.MergeMode = true;
		cfg.PixelSnapH = true;
		cfg.OversampleH = 2;
		cfg.OversampleV = 2;
		// 한글: 맑은 고딕
		const ImWchar* rangeKR = io.Fonts->GetGlyphRangesKorean();
		io.Fonts->AddFontFromFileTTF("..\\Resource\\Font\\NotoSansKR-Regular.ttf",
			17.0f, &cfg, rangeKR);
		// 일본어: Meiryo
		const ImWchar* rangeJP = io.Fonts->GetGlyphRangesJapanese();
		io.Fonts->AddFontFromFileTTF("..\\Resource\\Font\\meiryo.ttc", 17.0f, &cfg,
			rangeJP);
	}

	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_->m_pDevice, m_->m_pDeviceContext);
	return true;
}

bool App::InitBasicEffect() {
	// Vertex Shader -------------------------------------
	D3D11_INPUT_ELEMENT_DESC layout[] = // 입력 레이아웃.
	{ {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
	  D3D11_INPUT_PER_VERTEX_DATA, 0},
	 {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
	  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	 {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
	  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	 {"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
	  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	 {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
	  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	 {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
	  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0} };

	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"36_BasicVS.hlsl", "main", "vs_5_0",
		&vertexShaderBuffer));
	HR_T(m_->m_pDevice->CreateInputLayout(
		layout, ARRAYSIZE(layout), vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &m_->m_pInputLayout));

	HR_T(m_->m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(),
		NULL, &m_->m_pVertexShader));
	SAFE_RELEASE(vertexShaderBuffer); // 컴파일 버퍼 해제

	// PMX 전용: NoTBN 입력용 VS/IL 생성
	D3D11_INPUT_ELEMENT_DESC layoutNoTBN[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		 D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
		 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
		 D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
		 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	ID3D10Blob* vsNoTBN = nullptr;
	HR_T(
		CompileShaderFromFile(L"36_BasicVS.hlsl", "VSNoTBN", "vs_5_0", &vsNoTBN));
	HR_T(m_->m_pDevice->CreateInputLayout(
		layoutNoTBN, ARRAYSIZE(layoutNoTBN), vsNoTBN->GetBufferPointer(),
		vsNoTBN->GetBufferSize(), &m_->m_pInputLayoutNoTBN));
	HR_T(m_->m_pDevice->CreateVertexShader(vsNoTBN->GetBufferPointer(),
		vsNoTBN->GetBufferSize(), nullptr,
		&m_->m_pVertexShaderNoTBN));
	SAFE_RELEASE(vsNoTBN);

	// Line VS -------------------------------------------
	D3D11_INPUT_ELEMENT_DESC lineLayout[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		 D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
		 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
		 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	ID3D10Blob* vsLine = nullptr;
	HR_T(CompileShaderFromFile(L"36_BasicVS.hlsl", "VSLine", "vs_5_0", &vsLine));

	// FBX GPU 스키닝용 VS/IL 생성
	// (POSITION,NORMAL,TANGENT,BINORMAL,COLOR,TEXCOORD,BLENDINDICES,BLENDWEIGHT)
	{
		D3D11_INPUT_ELEMENT_DESC layoutSkinned[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
			 D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
			 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
			 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 0,
			 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
			 D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};
		ID3D10Blob* vsSkinned = nullptr;
		HR_T(CompileShaderFromFile(L"36_BasicVS.hlsl", "VSSkinned", "vs_5_0",
			&vsSkinned));
		HR_T(m_->m_pDevice->CreateInputLayout(
			layoutSkinned, ARRAYSIZE(layoutSkinned), vsSkinned->GetBufferPointer(),
			vsSkinned->GetBufferSize(), &m_->m_pInputLayoutSkinned));
		HR_T(m_->m_pDevice->CreateVertexShader(vsSkinned->GetBufferPointer(),
			vsSkinned->GetBufferSize(), nullptr,
			&m_->m_pVertexShaderSkinned));
		SAFE_RELEASE(vsSkinned);
	}
	HR_T(m_->m_pDevice->CreateInputLayout(
		lineLayout, ARRAYSIZE(lineLayout), vsLine->GetBufferPointer(),
		vsLine->GetBufferSize(), &m_->m_pLineInputLayout));
	HR_T(m_->m_pDevice->CreateVertexShader(vsLine->GetBufferPointer(),
		vsLine->GetBufferSize(), nullptr,
		&m_->m_pLineVS));
	SAFE_RELEASE(vsLine);

	// Outline VS
	{
		ID3D10Blob* vsOutline = nullptr;
		HR_T(CompileShaderFromFile(L"36_BasicVS.hlsl", "VSOutline", "vs_5_0",
			&vsOutline));
		HR_T(m_->m_pDevice->CreateVertexShader(vsOutline->GetBufferPointer(),
			vsOutline->GetBufferSize(), nullptr,
			&m_->m_pVertexShaderOutline));
		SAFE_RELEASE(vsOutline);

		ID3D10Blob* vsSkinnedOutline = nullptr;
		HR_T(CompileShaderFromFile(L"36_BasicVS.hlsl", "VSSkinnedOutline", "vs_5_0",
			&vsSkinnedOutline));
		HR_T(m_->m_pDevice->CreateVertexShader(
			vsSkinnedOutline->GetBufferPointer(), vsSkinnedOutline->GetBufferSize(),
			nullptr, &m_->m_pVertexShaderSkinnedOutline));
		SAFE_RELEASE(vsSkinnedOutline);
	}

	// Shadow VS (depth-only)
	{
		ID3D10Blob* vsShadow = nullptr;
		HR_T(CompileShaderFromFile(L"36_BasicVS.hlsl", "VSShadow", "vs_5_0",
			&vsShadow));
		HR_T(m_->m_pDevice->CreateVertexShader(vsShadow->GetBufferPointer(),
			vsShadow->GetBufferSize(), nullptr,
			&m_->m_pVSShadow));
		SAFE_RELEASE(vsShadow);

		ID3D10Blob* vsSkinnedShadow = nullptr;
		HR_T(CompileShaderFromFile(L"36_BasicVS.hlsl", "VSSkinnedShadow", "vs_5_0",
			&vsSkinnedShadow));
		HR_T(m_->m_pDevice->CreateVertexShader(vsSkinnedShadow->GetBufferPointer(),
			vsSkinnedShadow->GetBufferSize(),
			nullptr, &m_->m_pVSSkinnedShadow));
		SAFE_RELEASE(vsSkinnedShadow);
	}

	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"36_BasicPS.hlsl", "main", "ps_4_0",
		&pixelShaderBuffer));
	HR_T(m_->m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(),
		NULL, &m_->m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer); // 픽셀 셰이더 버퍼 더이상 필요없음

	// Outline 전용 Pixel Shader (PSOutline)
	{
		ID3D10Blob* psOutline = nullptr;
		HR_T(CompileShaderFromFile(L"36_BasicPS.hlsl", "PSOutline", "ps_4_0",
			&psOutline));
		HR_T(m_->m_pDevice->CreatePixelShader(psOutline->GetBufferPointer(),
			psOutline->GetBufferSize(), nullptr,
			&m_->m_pPixelShaderOutline));
		SAFE_RELEASE(psOutline);
	}

	return true;
}

bool App::InitSkyBoxEffect() {
	// Vertex Shader -------------------------------------
	D3D11_INPUT_ELEMENT_DESC layout[] = // 입력 레이아웃.
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		 D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"36_SkyBoxVS.hlsl", "VS", "vs_4_0",
		&vertexShaderBuffer));
	HR_T(m_->m_pDevice->CreateInputLayout(
		layout, ARRAYSIZE(layout), vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &m_->m_pSkyBoxInputLayout));

	HR_T(m_->m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(),
		NULL, &m_->m_pSkyBoxVertexShader));
	SAFE_RELEASE(vertexShaderBuffer); // 컴파일 버퍼 해제

	// Pixel Shader -------------------------------------
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"36_SkyBoxPS.hlsl", "PS", "ps_4_0",
		&pixelShaderBuffer));
	HR_T(m_->m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(),
		NULL, &m_->m_pSkyBoxPixelShader));
	SAFE_RELEASE(pixelShaderBuffer); // 픽셀 셰이더 버퍼 더이상 필요없음
	return true;
}

bool App::CreateQuad() {
	HRESULT hr = 0;                     // 결과값.
	ID3D10Blob* errorMessage = nullptr; // 에러 메시지를 저장할 버퍼.
	// 정점 선언.
	struct QuadVertex {
		Vector3 position; // Normalized Device coordinate position
		Vector2 uv;       // Texture coordinate position

		QuadVertex(float x, float y, float z, float u, float v)
			: position(x, y, z), uv(u, v) {
		}
		QuadVertex(Vector3 p, Vector2 u) : position(p), uv(u) {}
	};

	QuadVertex QuadVertices[] = {
		QuadVertex(Vector3(-1.0f, 1.0f, 1.0f), Vector2(0.0f, 0.0f)), // Left Top
		QuadVertex(Vector3(1.0f, 1.0f, 1.0f), Vector2(1.0f, 0.0f)),  // Right Top
		QuadVertex(Vector3(-1.0f, -1.0f, 1.0f),
				   Vector2(0.0f, 1.0f)), // Left Bottom
		QuadVertex(Vector3(1.0f, -1.0f, 1.0f),
				   Vector2(1.0f, 1.0f)) // Right Bottom
	};

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = sizeof(QuadVertex) * ARRAYSIZE(QuadVertices);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = QuadVertices; // 배열 데이터 할당.
	HR_T(m_->m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_->m_pQuadVertexBuffer));
	m_->m_QuadVertexBufferStride = sizeof(QuadVertex); // 버텍스 버퍼 정보
	m_->m_QuadVertexBufferOffset = 0;

	// InputLayout 생성
	D3D11_INPUT_ELEMENT_DESC layout[] = // 입력 레이아웃.
	{// SemanticName , SemanticIndex , Format , InputSlot , AlignedByteOffset
	 // , InputSlotClass , InstanceDataStepRate
	 {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
	  D3D11_INPUT_PER_VERTEX_DATA, 0},
	 {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
	  D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0} };
	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"36_QuadVS.hlsl", "main", "vs_5_0",
		&vertexShaderBuffer));
	HR_T(m_->m_pDevice->CreateInputLayout(
		layout, ARRAYSIZE(layout), vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), &m_->m_pQuadInputLayout));

	// 버텍스 셰이더 생성
	HR_T(m_->m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(),
		NULL, &m_->m_pQuadVertexShader));
	SAFE_RELEASE(vertexShaderBuffer); // 버퍼 해제.

	// 인덱스 버퍼 생성
	WORD indices[] = { 0, 1, 2, 2, 1, 3 };
	m_->m_nQuadIndices = ARRAYSIZE(indices); // 인덱스 개수 저장.
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;
	HR_T(m_->m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_->m_pQuadIndexBuffer));

	// 픽셀 셰이더 생성
	ID3D10Blob* pixelShaderBuffer = nullptr;

	HR_T(CompileShaderFromFile(L"36_ToneMappingPS_LDR.hlsl", "main", "ps_5_0",
		&pixelShaderBuffer));
	HR_T(m_->m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(),
		NULL, &m_->m_pPS_ToneMappingLDR));
	SAFE_RELEASE(pixelShaderBuffer); // 픽셀 셰이더 버퍼 더이상 필요없음.

	HR_T(CompileShaderFromFile(L"36_ToneMappingPS_HDR.hlsl", "main", "ps_5_0",
		&pixelShaderBuffer));
	HR_T(m_->m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(),
		NULL, &m_->m_pPS_ToneMappingHDR));
	SAFE_RELEASE(pixelShaderBuffer); // 픽셀 셰이더 버퍼 더이상 필요없음.

	D3D11_BUFFER_DESC pdbDesc = {};
	pdbDesc.Usage =
		D3D11_USAGE_DYNAMIC; // CPU가 매 프레임 쓸(Write) 것이므로 Dynamic
	pdbDesc.ByteWidth = sizeof(PostProcessConstantBuffer);
	pdbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	pdbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // CPU에서 접근 가능하게 설정
	pdbDesc.MiscFlags = 0;
	pdbDesc.StructureByteStride = 0;

	// m_->m_pPostProcessBuffer에 생성
	HR_T(m_->m_pDevice->CreateBuffer(&pdbDesc, nullptr,
		&m_->m_pPostProcessConstantBuffer));

	return true;
}

// G-Buffer 생성 함수 (월드좌표, 월드Normal, 금속성, 거칠기, BaseColor)
bool App::CreateGBuffer() {
	// 기존 G-Buffer 해제
	for (int i = 0; i < Impl::GBufferCount; ++i) {
		m_->m_pGBufferSRVs[i].Reset();
		m_->m_pGBufferRTVs[i].Reset();
		m_->m_pGBufferTextures[i].Reset();
	}

	// G-Buffer 포맷 정의
	// 0: 월드좌표 (PositionWS)
	// 1: 월드Normal (NormalWS)
	// 2: 금속성 (Metalness)
	// 3: 거칠기 (Roughness)
	// 4: BaseColor
	struct RTDesc {
		DXGI_FORMAT format;
	};

	RTDesc formats[Impl::GBufferCount] = {
		{DXGI_FORMAT_R16G16B16A16_FLOAT}, // 0: PositionWS (월드 좌표)
		{DXGI_FORMAT_R16G16B16A16_FLOAT}, // 1: NormalWS (월드 노말, -1~1을 0~1로
		// 변환)
		{DXGI_FORMAT_R8_UNORM},            // 2: Metalness (금속성)
		{DXGI_FORMAT_R8_UNORM},            // 3: Roughness (거칠기)
		{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB}, // 4: BaseColor (베이스 컬러)
	};

	// 각 G-Buffer 텍스처 생성
	for (int i = 0; i < Impl::GBufferCount; ++i) {
		D3D11_TEXTURE2D_DESC td = {};
		td.Width = m_ClientWidth;
		td.Height = m_ClientHeight;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = formats[i].format;
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		td.CPUAccessFlags = 0;
		td.MiscFlags = 0;

		HR_T(m_->m_pDevice->CreateTexture2D(
			&td, nullptr, m_->m_pGBufferTextures[i].GetAddressOf()));
		HR_T(m_->m_pDevice->CreateRenderTargetView(
			m_->m_pGBufferTextures[i].Get(), nullptr,
			m_->m_pGBufferRTVs[i].GetAddressOf()));
		HR_T(m_->m_pDevice->CreateShaderResourceView(
			m_->m_pGBufferTextures[i].Get(), nullptr,
			m_->m_pGBufferSRVs[i].GetAddressOf()));
	}

	// 디퍼드 렌더링용 셰이더 생성
	{
		// G-Buffer 패스 Vertex Shader
		ID3D10Blob* vsBlob = nullptr;
		HR_T(CompileShaderFromFile(L"36_DeferredGBufferVS.hlsl", "main", "vs_5_0",
			&vsBlob));

		// G-Buffer 패스 Input Layout (TBN 구조체 사용)
		D3D11_INPUT_ELEMENT_DESC gbufferLayout[] = {
			/*{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
			D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "NORMAL", 0,
			DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
			D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "TANGENT", 0,
			DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
			D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "BINORMAL", 0,
			DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
			D3D11_INPUT_PER_VERTEX_DATA, 0 }, { "TEXCOORD", 0,
			DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
			D3D11_INPUT_PER_VERTEX_DATA, 0 },*/
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0} };
		HR_T(m_->m_pDevice->CreateInputLayout( gbufferLayout, ARRAYSIZE(gbufferLayout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_->m_pGBufferInputLayout));
		HR_T(m_->m_pDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_->m_pGBufferVS));
		SAFE_RELEASE(vsBlob);

		// G-Buffer 패스 Pixel Shader
		ID3D10Blob* psBlob = nullptr;
		HR_T(CompileShaderFromFile(L"36_DeferredGBufferPS.hlsl", "main", "ps_5_0", &psBlob));
		HR_T(m_->m_pDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_->m_pGBufferPS));
		SAFE_RELEASE(psBlob);

		// 디퍼드 라이트 패스 Pixel Shader
		ID3D10Blob* lightPSBlob = nullptr;
		HR_T(CompileShaderFromFile(L"36_DeferredLightPS.hlsl", "main", "ps_5_0", &lightPSBlob));
		HR_T(m_->m_pDevice->CreatePixelShader(lightPSBlob->GetBufferPointer(), lightPSBlob->GetBufferSize(), nullptr, &m_->m_pDeferredLightPS));
		SAFE_RELEASE(lightPSBlob);
	}

	// 가산 블렌딩 상태 생성 (라이트 패스용)
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask =
			D3D11_COLOR_WRITE_ENABLE_ALL;
		HR_T(m_->m_pDevice->CreateBlendState(&blendDesc,
			&m_->m_pBlendStateAdditive));
	}

	return true;
}

// ------------------------- Model Loader (FBX/OBJ/PMX via Assimp) -------------------------
bool App::LoadModelFromFile(const std::wstring& pathW) {
	// 새 모델델 추가

	// 폴백 텍스처(화이트/블랙/노멀) 생성: 각각 최초 1회만 생성
	auto createFallbackIfNull = [&](ID3D11ShaderResourceView** targetSRV,
		UINT rgba) {
			if (*targetSRV)
				return;
			D3D11_TEXTURE2D_DESC td{};
			td.Width = 1;
			td.Height = 1;
			td.MipLevels = 1;
			td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_IMMUTABLE;
			td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			D3D11_SUBRESOURCE_DATA sd{};
			sd.pSysMem = &rgba;
			sd.SysMemPitch = sizeof(UINT);
			Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
			HR_T(m_->m_pDevice->CreateTexture2D(&td, &sd, tex.GetAddressOf()));
			D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
			srvd.Format = td.Format;
			srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvd.Texture2D.MipLevels = 1;
			srvd.Texture2D.MostDetailedMip = 0;
			HR_T(m_->m_pDevice->CreateShaderResourceView(tex.Get(), &srvd, targetSRV));
		};
	createFallbackIfNull(&m_->m_pFallbackWhite, 0xFFFFFFFF);
	createFallbackIfNull(&m_->m_pFallbackBlack, 0x000000FF); // a=1
	createFallbackIfNull(&m_->m_pFallbackNormal,
		0x8080FFFF); // (0.5,0.5,1,1) in RGBA8

	// 받은 경로에서 이름, 확장자 추출
	std::wstring ext{ L"" }, fileName{ L"" };
	if (!pathW.empty()) {
		size_t dot = pathW.find_last_of(L'.');
		if (dot != std::wstring::npos) {
			ext = pathW.substr(dot);
			std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
			size_t sep = pathW.find_last_of(L"\\/");
			if (sep != std::wstring::npos)
				fileName = pathW.substr(sep + 1, dot - sep - 1);
			else
				fileName = pathW.substr(0, dot);
		}
	}

	bool ok = false;
	auto entry = std::make_unique<ModelEntry>();
	entry->modelName = fileName;

	// 캐시 확인
	std::shared_ptr<SharedModelData> shared;
	if (auto it = m_->m_ModelCache.find(pathW); it != m_->m_ModelCache.end())
		shared = it->second.lock();

	if (!shared) {
		shared = std::make_shared<SharedModelData>();
		shared->pathW = pathW;
		// 로드 경로에 따라 매니저 준비
		if (ext == L".fbx") {
			shared->source = ModelSource::FBX;
			// 공용 AssetManager를 통해 FBX 모델 공유/캐시
			shared->fbx = AssetManager::GetInstance().GetFbxModel(m_->m_pDevice, pathW);
			if (ok = (shared->fbx != nullptr)) {
				m_->PushLog("[OK] Loaded FBX(shared): " + Utf8FromWString(fileName));
				shared->stride = shared->fbx->GetVertexStride();
				shared->vb = shared->fbx->GetVertexBuffer();
				shared->ib = shared->fbx->GetIndexBuffer();
				shared->indexCount = shared->fbx->GetIndexCount();
				shared->subsets.clear();
				for (auto& s : shared->fbx->GetSubsets())
					shared->subsets.push_back(
						{ s.startIndex, s.indexCount, s.materialIndex });
				shared->materialSRVs = shared->fbx->GetMaterialSRVs();
				shared->normalSRVs = shared->fbx->GetNormalSRVs();
			}
			else {
				m_->PushLog("[ERR] Failed FBX: " + Utf8FromWString(fileName));
			}
		}
		else if (ext == L".obj") {
			shared->source = ModelSource::OBJ;
			shared->obj = std::make_shared<ObjManager>();
			if (ok = shared->obj->Load(m_->m_pDevice, pathW)) {
				m_->PushLog("[OK] Loaded OBJ(shared): " + Utf8FromWString(fileName));
				shared->stride = shared->obj->GetVertexStride();
				shared->vb = shared->obj->GetVertexBuffer();
				shared->ib = shared->obj->GetIndexBuffer();
				shared->indexCount = shared->obj->GetIndexCount();
				shared->subsets.clear();
				const auto& subs = shared->obj->GetSubsets();
				for (auto& s : subs)
					shared->subsets.push_back(
						{ s.startIndex, s.indexCount, s.materialIndex });
				shared->materialSRVs = shared->obj->GetMaterialSRVs();
			}
			else {
				m_->PushLog("[ERR] Failed OBJ: " + Utf8FromWString(fileName));
			}
		}
		else if (ext == L".pmx") {
			shared->source = ModelSource::PMX;
			shared->pmx = std::make_shared<PmxManager>();
			if (ok = shared->pmx->Load(m_->m_pDevice, pathW)) {
				m_->PushLog("[OK] Loaded PMX(shared): " + Utf8FromWString(fileName));
				shared->stride = shared->pmx->GetVertexStride();
				shared->vb = shared->pmx->GetVertexBuffer();
				shared->ib = shared->pmx->GetIndexBuffer();
				shared->indexCount = shared->pmx->GetIndexCount();
				shared->subsets.clear();
				const auto& subs = shared->pmx->GetSubsets();
				for (auto& s : subs)
					shared->subsets.push_back(
						{ s.startIndex, s.indexCount, s.materialIndex });
				shared->materialSRVs = shared->pmx->GetMaterialSRVs();
			}
			else {
				m_->PushLog("[ERR] Failed PMX: " + Utf8FromWString(fileName));
			}
		}

		if (ok) {
			m_->m_ModelCache[pathW] = shared; // 캐시 등록
		}
	}
	else {
		ok = true;
		m_->PushLog("[OK] Reused cached model: " + Utf8FromWString(fileName));
	}

	if (ok && shared) {
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
		if (entry->source == ModelSource::FBX && entry->shared &&
			entry->shared->fbx) {
			entry->fbxBaseAnimator.InitMetadata(entry->shared->fbx->GetScenePtr());
			entry->fbxBaseAnimator.SetSharedContext(entry->shared->fbx->GetScenePtr(),
				entry->shared->fbx->GetNodeIndexOfName(),
				&entry->shared->fbx->GetBoneNames(),
				&entry->shared->fbx->GetBoneOffsets(),
				&entry->shared->fbx->GetGlobalInverse());
			auto t = entry->shared->fbx->GetCurrentAnimationType();
			entry->fbxBaseAnimator.SetType(t == FbxModel::AnimationType::Rigid
				? FbxAnimation::AnimType::Rigid
				: (t == FbxModel::AnimationType::Skinned
					? FbxAnimation::AnimType::Skinned
					: FbxAnimation::AnimType::None));
			entry->animatorInited = true;
		}

		m_->m_Models.push_back(std::move(entry));
	}

	return ok;
}

void App::UnloadModel() {
	m_->m_Models.clear();
	m_->m_SelectedModelIdx = -1;
	m_->m_SelectedBoneIdx = -1;
}

void App::RenderControlPannel() {
	// Control 패널
	ImGuiIO& ioUI = ImGui::GetIO();
	const float W = ioUI.DisplaySize.x;
	const float H = ioUI.DisplaySize.y;

	ImGui::SetNextWindowPos(ImVec2(10, 20), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(300, 360), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Controls")) {
		ImGui::SeparatorText("Tone Mapping Parameter");
		ImGui::SliderFloat("Exposure", &m_->m_Exposure, -2.0f, 2.0f, "%.2f");
		ImGui::SliderFloat("Monitor Max Nits", &m_->m_MonitorMaxNits, 0.0f,
			50000.0f, "%.2f");

		ImGui::SeparatorText("Rendering Mode");
		ImGui::Checkbox("Use Deferred Rendering", &m_->m_UseDeferredRendering);

		ImGui::SeparatorText("PBR Parameter");
		// 텍스처 색 사용 여부 (PBR 전용)
		ImGui::Checkbox("Use Texture Color (PBR)", (bool*)&m_->m_UseTextureColor);
		// 노말맵 사용 여부 (PBR / 모델 공통)
		{
			bool useNormalMap = (m_->m_EnableNormalMapForCube != 0);
			if (ImGui::Checkbox("Use Normal Map (PBR)", &useNormalMap)) {
				m_->m_EnableNormalMapForCube = useNormalMap ? 1 : 0;
			}
		}

		ImGui::ColorEdit3("Base Color##PBR", &m_->m_DefaultPbrMaterial.baseColor.x);
		ImGui::SliderFloat("Metalness##PBR", &m_->m_DefaultPbrMaterial.metalness,
			0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Roughness##PBR", &m_->m_DefaultPbrMaterial.roughness,
			0.0f, 1.0f, "%.2f");
		ImGui::SliderFloat("Ambient Occlusion##PBR",
			&m_->m_DefaultPbrMaterial.ambientOcclusion, 0.0f, 1.0f,
			"%.2f");
		if (ImGui::Button("Reset PBR Material")) {
			m_->m_DefaultPbrMaterial = PBRMaterialCPU{};
		}

		ImGui::Separator();

		// SkyBox 선택
		{
			int cur = static_cast<int>(m_->m_SkyBoxChoice);

			const char* items[] = { "Off", "bridge", "indoor", "baker" };

			// 2. 콤보 선택 UI
			if (ImGui::Combo("SkyBox Choice", &cur, items, IM_ARRAYSIZE(items))) {
				// 3. int → enum 캐스팅
				m_->m_SkyBoxChoice = static_cast<App::Impl::SkyBoxChoice>(cur);

				if (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Off) {
					// Off: 배경 단색 사용
				}
				else {
					switch (m_->m_SkyBoxChoice) {
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
			if (m_->m_SkyBoxChoice == App::Impl::SkyBoxChoice::Off) {
				ImGui::ColorEdit4("Background Color", &m_->m_ClearColor.x);
			}
		}
		ImGui::Separator();
		ImGui::Text("Camera");
		{
			if (ImGui::Button("Reset")) {
				m_Camera.Reset();
			}
			ImGui::SliderFloat("Camera Speed", &m_Camera.m_MoveSpeed, 1.0f, 500.0f,
				"%.1f");
			DirectX::XMFLOAT3 pos = m_Camera.GetPosition();
			if (ImGui::DragFloat3("Camera Pos (x,y,z)", &pos.x, 0.1f)) {
				m_Camera.SetPosition(pos);
			}
			float fovDeg = XMConvertToDegrees(m_Camera.GetFovYRad());
			if (ImGui::SliderFloat("Camera FOV (deg)", &fovDeg, 30.0f, 120.0f)) {
				m_Camera.SetFrustum(XMConvertToRadians(fovDeg), AspectRatio(),
					m_Camera.GetNearZ(), m_Camera.GetFarZ());
			}
			float nearZ = m_Camera.GetNearZ();
			float farZ = m_Camera.GetFarZ();
			if (ImGui::DragFloatRange2("Near/Far", &nearZ, &farZ, 0.1f, 0.01f,
				10000.0f, "Near: %.2f", "Far: %.2f")) {
				m_Camera.SetFrustum(m_Camera.GetFovYRad(), AspectRatio(), nearZ, farZ);
			}
			// 카메라 회전(도) 표시 및 편집: pitch, yaw, roll (API로 일원화)
			{
				XMFLOAT3 rotDeg = m_Camera.GetRotation();
				if (ImGui::DragFloat3("Camera Rot (deg)", &rotDeg.x, 1.0f, -180.0f,
					180.0f, "%.1f")) {
					m_Camera.SetRotation(rotDeg);
				}
			}
		}
		ImGui::Separator();
		ImGui::Text("VMD Camera");
		if (ImGui::Button("Load VMD Camera and Play")) {
			std::wstring vmdPath;
			if (OpenFileDialogVMD(vmdPath)) {
				if (mmd::LoadVmdCameraFromFile(vmdPath, m_->m_VmdCamera)) {
					m_->PushLog("[OK] Loaded VMD Camera");
				}
				else {
					m_->PushLog("[ERR] Failed to load VMD Camera");
				}
			}
		}
		if (!m_->m_VmdCamera.frames.empty()) {
			ImGui::Checkbox("Use VMD Camera", &m_->m_VmdCamera.use);
			ImGui::SameLine();
			if (ImGui::Button(m_->m_VmdCamera.playing ? "Pause##VMD" : "Play##VMD")) {
				m_->m_VmdCamera.playing = !m_->m_VmdCamera.playing;
			}
			ImGui::SameLine();
			if (ImGui::Button("Stop##VMD")) {
				m_->m_VmdCamera.playing = false;
				if (!m_->m_VmdCamera.frames.empty())
					m_->m_VmdCamera.currentFrame =
					(float)m_->m_VmdCamera.frames.front().frame;
			}

			int firstFrame = m_->m_VmdCamera.frames.front().frame;
			int lastFrame = m_->m_VmdCamera.frames.back().frame;
			float curFrame = m_->m_VmdCamera.currentFrame;
			ImGui::Text("Frame: %.1f (%d ~ %d)", curFrame, firstFrame, lastFrame);
			ImGui::SliderFloat("VMD Camera Scale", &m_->m_VmdCamera.scale, 0.1f, 5.0f,
				"%.2f");
		}
		ImGui::Separator();
		ImGui::Text("Shading");
		{
			int mode = (int)m_->m_ShadingMode;
			const char* modes[] = { "Phong",       "Blinn-Phong", "Lambert", "Unlit",
								   "TextureOnly", "ToonShading", "PBR" };
			if (ImGui::Combo("Shading Mode", &mode, modes, IM_ARRAYSIZE(modes))) {
				m_->m_ShadingMode = (ShadingMode)mode;
			}

			{
				ImGui::Separator();
				ImGui::Text("Outline (Toon + Multipass)");
				// Multipass 아웃라인 파라미터
				ImGui::DragFloat("Thickness", &m_->m_OutlineThickness, 0.0f, 2.0f);
				ImGui::ColorEdit3("Color", &m_->m_OutlineColor.x);
				ImGui::SliderFloat("Strength", &m_->m_OutlineStrength, 0.0f, 4.0f,
					"%.2f");
			}
		}
		ImGui::Separator();
		ImGui::Text("Light");
		ImGui::DragFloat3("Light Direction", &m_->m_DirLight.direction.x, 0.05f);
		ImGui::SliderFloat("Intensity", &m_->m_DirLight.intensity, 0.1f, 30.0f,
			"%.1f");
		ImGui::ColorEdit4("Ambient", &m_->m_DirLight.ambient.x);
		ImGui::ColorEdit4("Diffuse", &m_->m_DirLight.diffuse.x);
		ImGui::ColorEdit4("Specular", &m_->m_DirLight.specular.x);
		if (ImGui::Button("Reset Light")) {
			m_->m_DirLight = { XMFLOAT4(0, 0, 0, 1), XMFLOAT4(1, 1, 1, 1),
							  XMFLOAT4(0.7f, 0.7f, 0.7f, 1), XMFLOAT3(0, 0, 1), 1.0f };
		}
		ImGui::Separator();

		ImGui::Text("Material");
		ImGui::ColorEdit4("Ambient (ka)", &m_->m_Material.ambient.x);
		ImGui::ColorEdit4("Diffuse (kd)", &m_->m_Material.diffuse.x);
		ImGui::ColorEdit4("Specular (ks)", &m_->m_Material.specular.x);
		ImGui::DragFloat("Shininess (alpha)", &m_->m_Material.specular.w, 0.05f,
			1.0f, 256.0f);
		ImGui::ColorEdit4("Reflect (R=metal, A=roughness)",
			&m_->m_Material.reflect.x);
		if (ImGui::Button("Reset Material")) {
			m_->m_Material = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1),
							  XMFLOAT4(1, 1, 1, 32), XMFLOAT4(0, 0, 0, 0) };
		}
	}
	ImGui::End();

	auto& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(10, 390), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(370, 360), ImGuiCond_FirstUseEver);
	// Shadow controls and debug view
	if (ImGui::Begin("ShadowMap (Directional)")) {
		ImGui::Checkbox("Enable Shadow", (bool*)&m_->m_ShadowEnabled);
		ImGui::DragFloat("Bias", &m_->m_ShadowBias, 0.0001f, 0.0f, .1f, "%.5f");
		ImGui::SliderFloat("PCF Radius(Texel)", &m_->m_ShadowPCFRadius, 0.0f, 3.0f,
			"%.2f");
		ImGui::SliderFloat("Ortho Radius(m)", &m_->m_ShadowOrthoRadius, 1.0f,
			3000.0f, "%.1f");
		if (m_->m_pShadowSRV) {
			ImGui::Separator();
			ImGui::Text("ShadowMap Debug");
			ImVec2 avail = ImGui::GetContentRegionAvail();
			float sz = std::min(avail.x, 256.0f);
			ImGui::Image((ImTextureID)m_->m_pShadowSRV, ImVec2(sz, sz));
		}
	}
	ImGui::End();
}

void App::RenderModelPannel() {
	// Models 독립 창
	auto& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 330, 380),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320, 360), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Details")) {
		if (m_->m_SelectedItem >= 0 &&
			m_->m_SelectedItem < (int)m_->m_Objects.size()) {
			auto* obj = m_->m_Objects[m_->m_SelectedItem].get();
			if (obj->kind == ObjectKind::Cube) {
				auto* co = static_cast<CubeObject*>(obj);
				ImGui::Text("Cube : %s", Utf8FromWString(co->name).c_str());
				ImGui::Separator();
				auto& t = co->cubeTransform;
				// Maps (per cube, Texture type only)
				if (co->cubeType == ECubeType::Texture) {
					bool nm = (co->useNormalMap != 0);
					if (ImGui::Checkbox("Enable Normal Map", &nm)) {
						co->useNormalMap = nm ? 1 : 0;
					}
					bool sm = (co->useSpecularMap != 0);
					if (ImGui::Checkbox("Use Specular Map", &sm)) {
						co->useSpecularMap = sm ? 1 : 0;
					}
				}
				ImGui::DragFloat3("Position", &t.position.x, 0.1f);
				ImGui::DragFloat3("Rotation (deg)", &t.rotationDeg.x, 1.0f, -360.0f,
					360.0f, "%.1f");
				ImGui::DragFloat3("Scale", &t.scale.x, 0.01f, 0.001f, 100.0f, "%.3f");

				ImGui::Separator();
				ImGui::TextUnformatted("Material");
				ImGui::ColorEdit4("Ambient (ka)", &co->matAmbient.x);
				ImGui::ColorEdit4("Diffuse (kd)", &co->matDiffuse.x);
				ImGui::ColorEdit4("Specular (ks)", &co->matSpecular.x);
				ImGui::DragFloat("Shininess (alpha)", &co->matSpecular.w, 0.05f, 1.0f,
					256.0f);
				ImGui::ColorEdit4("Reflect (kr,a)", &co->matReflect.x);
			}
			else {
				auto* mo = static_cast<ModelObject*>(obj);
				int i = mo->modelIndex;
				if (i >= 0 && i < (int)m_->m_Models.size()) {
					auto& mdl = *m_->m_Models[i];
					ImGui::Text("Model #%d : %s", i,
						Utf8FromWString(m_->m_Models[i]->modelName).c_str());
					ImGui::Separator();
					ImGui::PushID(i);
					ImGui::DragFloat3("Position", &mdl.pos.x, 0.1f);
					ImGui::DragFloat3("Rotation (deg)", &mdl.rotDeg.x, 1.0f, -360.0f,
						360.0f, "%.1f");
					ImGui::DragFloat3("Scale", &mdl.scale.x, 0.01f, 0.001f, 100.0f,
						"%.3f");
					ImGui::Checkbox("Auto Rotate (Yaw)", &mdl.autoRotate);
					if (mdl.source == ModelSource::FBX && mdl.shared && mdl.shared->fbx) {
						if (mdl.shared->fbx->HasAnimations()) {
							const auto& names = mdl.shared->fbx->GetAnimationNames();
							if (mdl.uiSelectedAnim < 0 ||
								mdl.uiSelectedAnim >= (int)names.size())
								mdl.uiSelectedAnim = mdl.fbxBaseAnimator.GetCurrentIndex();

							ImGui::Text("FBX Animations");
							if (ImGui::BeginListBox(
								"##AnimList",
								ImVec2(-FLT_MIN,
									4 * ImGui::GetTextLineHeightWithSpacing()))) {
								for (int a = 0; a < (int)names.size(); ++a) {
									bool sel = (a == mdl.uiSelectedAnim);
									if (ImGui::Selectable(names[a].c_str(), sel)) {
										mdl.uiSelectedAnim = a;
										mdl.fbxBaseAnimator.SetCurrentIndex(a);
										m_->PushLog(std::string("[OK] FBX Anim -> ") + names[a]);
									}
									if (sel)
										ImGui::SetItemDefaultFocus();
								}
								ImGui::EndListBox();
							}

							// FBX용 오디오 로드 (FMOD)
							if (ImGui::Button("Load Audio (FMOD)...")) {
								wchar_t file[MAX_PATH] = { 0 };
								OPENFILENAMEW ofn{};
								ofn.lStructSize = sizeof(ofn);
								ofn.hwndOwner = GameApp::m_hWnd;
								ofn.lpstrFilter =
									L"Audio Files (*.wav;*.mp3;*.ogg)\0*.wav;*.mp3;*.ogg\0All "
									L"Files\0*.*\0\0";
								ofn.lpstrFile = file;
								ofn.nMaxFile = MAX_PATH;
								ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
								if (GetOpenFileNameW(&ofn)) {
									mdl.audioPath = file;
									mdl.audioLoaded = Sound::LoadMusic(mdl.audioPath);
									if (mdl.audioLoaded) {
										m_->PushLog("[OK] Audio loaded (FMOD)");
									}
									else {
										m_->PushLog("[ERR] Audio load failed (FMOD)");
									}
								}
							}

							bool playFBX = mdl.fbxBaseAnimator.IsPlaying();
							if (ImGui::Checkbox("Play", &playFBX)) {
								mdl.fbxBaseAnimator.SetPlaying(playFBX);
								mdl.uiAnimPlaying = playFBX;

								// 애니메이션 재생 상태에 맞춰 사운드도 같이 제어
								if (mdl.audioLoaded) {
									if (playFBX) {
										Sound::Play();
									}
									else {
										Sound::Pause(true);
									}
								}
							}

							double cur = mdl.fbxBaseAnimator.GetTimeSec();
							double dur = mdl.fbxBaseAnimator.GetClipDurationSec(
								mdl.fbxBaseAnimator.GetCurrentIndex());
							float curF = (float)cur, durF = (float)dur;
							if (durF > 0.0f) {
								// 애니메이션 타임라인과 사운드 재생 위치를 동일한 초 단위로
								// 맞춘다.
								if (ImGui::SliderFloat("Time (s)", &curF, 0.0f, durF)) {
									mdl.fbxBaseAnimator.SetTimeSec((double)curF);
									if (mdl.audioLoaded) {
										Sound::SetTimeSeconds(curF);
									}
								}
							}

							// 정지 / 현재 시간 출력 (애니+오디오 동시 제어)
							if (mdl.audioLoaded) {
								ImGui::SeparatorText("Audio Sync (FMOD)");
								if (ImGui::Button("Stop (Anim + Audio)##FBX")) {
									mdl.fbxBaseAnimator.SetPlaying(false);
									mdl.fbxBaseAnimator.SetTimeSec(0.0);
									Sound::Stop();
									Sound::SetTimeSeconds(0.0f);
								}
								float curAudio = Sound::GetTimeSeconds();
								float lenAudio = Sound::GetLengthSeconds();
								ImGui::Text("Audio Time: %.2f / %.2f sec", curAudio, lenAudio);
							}
						}
					}
					else if (mdl.source == ModelSource::PMX && mdl.shared &&
						mdl.shared->pmx) {
						if (ImGui::Button("Load VMD...")) {
							std::wstring vmdPath;
							wchar_t file[MAX_PATH] = { 0 };
							OPENFILENAMEW ofn{};
							ofn.lStructSize = sizeof(ofn);
							ofn.hwndOwner = GameApp::m_hWnd;
							ofn.lpstrFilter = L"VMD Files (*.vmd)\0*.vmd\0All Files\0*.*\0\0";
							ofn.lpstrFile = file;
							ofn.nMaxFile = MAX_PATH;
							ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
							if (GetOpenFileNameW(&ofn)) {
								if (mdl.shared->pmx->LoadVMD(m_->m_pDevice, file)) {
									m_->PushLog("[OK] VMD loaded");
									mdl.uiAnimPlaying = true;
								}
								else {
									m_->PushLog("[ERR] VMD load failed");
								}
							}
						}

						bool playPMX = mdl.shared->pmx->IsAnimationPlaying();
						if (ImGui::Checkbox("Play##PMX", &playPMX)) {
							mdl.shared->pmx->SetAnimationPlaying(playPMX);
						}

						double cur = mdl.shared->pmx->GetAnimationTimeSeconds();
						double dur = mdl.shared->pmx->GetClipDurationSec();
						float curF = (float)cur, durF = (float)dur;
						if (durF > 0.0f) {
							if (ImGui::SliderFloat("Time (s)##PMX", &curF, 0.0f, durF)) {
								mdl.shared->pmx->SetAnimationTimeSeconds((double)curF);
							}
						}
					}

					// 셰이딩 모드 선택 UI
					{
						int modePer = (int)mdl.modelShading;
						const char* modes[] = { "Phong", "Blinn-Phong", "Lambert",
											   "Unlit", "TextureOnly", "ToonShading",
											   "PBR" };
						if (ImGui::Combo("Shading Mode", &modePer, modes,
							IM_ARRAYSIZE(modes))) {
							mdl.modelShading = (ShadingMode)modePer;
						}
					}
					// Outline 적용 토글
					ImGui::Checkbox("Outline", &mdl.outlineEnabled);

					// 인스턴스 머티리얼
					ImGui::Checkbox("Use Instance Material", &mdl.useInstanceMaterial);
					if (mdl.useInstanceMaterial) {
						ImGui::ColorEdit4("Ambient (ka)##inst",
							&mdl.instanceMaterial.ambient.x);
						ImGui::ColorEdit4("Diffuse (kd)##inst",
							&mdl.instanceMaterial.diffuse.x);
						ImGui::ColorEdit4("Specular (ks)##inst",
							&mdl.instanceMaterial.specular.x);
						ImGui::DragFloat("Shininess (alpha)##inst",
							&mdl.instanceMaterial.specular.w, 0.05f, 1.0f,
							256.0f);
						ImGui::ColorEdit4("Reflect (kr,a)##inst",
							&mdl.instanceMaterial.reflect.x);
					}

					ImGui::SeparatorText("PBR Material");
					ImGui::Checkbox("Use Instance PBR Material",
						&mdl.useInstancePbrMaterial);
					if (mdl.useInstancePbrMaterial) {
						ImGui::ColorEdit3("Base Color##instPBR",
							&mdl.instancePbrMaterial.baseColor.x);
						ImGui::SliderFloat("Metalness##instPBR",
							&mdl.instancePbrMaterial.metalness, 0.0f, 1.0f,
							"%.2f");
						ImGui::SliderFloat("Roughness##instPBR",
							&mdl.instancePbrMaterial.roughness, 0.04f, 1.0f,
							"%.2f");
						ImGui::SliderFloat("Ambient Occlusion##instPBR",
							&mdl.instancePbrMaterial.ambientOcclusion, 0.0f,
							1.0f, "%.2f");
					}
					else {
						const auto& defPbr = m_->m_DefaultPbrMaterial;
						ImGui::Text("Base Color: (%.2f, %.2f, %.2f)", defPbr.baseColor.x,
							defPbr.baseColor.y, defPbr.baseColor.z);
						ImGui::Text("Metalness: %.2f", defPbr.metalness);
						ImGui::Text("Roughness: %.2f", defPbr.roughness);
						ImGui::Text("AO: %.2f", defPbr.ambientOcclusion);
					}

					ImGui::Separator();
					// 디버그 AABB 기준 본 인덱스 설정 (-1: Auto)
					ImGui::TextUnformatted("Debug AABB");
					ImGui::DragInt("Bounds Bone Index (-1:auto)", &mdl.boundsBoneIndex,
						1.0f, -1, 1023);
					// 선택된 인덱스의 본 이름 표시 (FBX만)
					if (mdl.source == ModelSource::FBX && mdl.shared && mdl.shared->fbx) {
						const auto& boneNames = mdl.shared->fbx->GetBoneNames();
						const char* name = "<auto>";
						if (mdl.boundsBoneIndex >= 0 &&
							(size_t)mdl.boundsBoneIndex < boneNames.size()) {
							name = boneNames[(size_t)mdl.boundsBoneIndex].c_str();
						}
						ImGui::Text("Bone: %s", name);
					}

					ImGui::Separator();
					// 메시 통계는 최초에 한 번만 계산
					if (!mdl.meshStatsValid && mdl.shared && mdl.shared->vb &&
						mdl.shared->ib && mdl.shared->indexCount > 0) {
						mdl.meshStatsValid = ComputeMeshStats(
							m_->m_pDevice, m_->m_pDeviceContext, mdl.shared->vb,
							mdl.shared->stride, mdl.shared->ib, mdl.shared->indexCount,
							mdl.meshStats);
					}
					if (mdl.meshStatsValid) {
						ImGui::Text("Vertex: %u   Edge: %u   Face: %u   Tri: %u",
							mdl.meshStats.vertices, mdl.meshStats.edges,
							mdl.meshStats.faces, mdl.meshStats.triangles);
					}

					// 본 구조 카드
					const auto& cache = mdl.boneCache;
					int rootIdx = mdl.boneRoot;
					bool hasSkeleton =
						mdl.boneCacheValid && rootIdx >= 0 && rootIdx < (int)cache.size();
					if (hasSkeleton && rootIdx >= 0) {
						ImGui::Checkbox("Show Bone Details", &mdl.showBoneDetails);
						if (mdl.showBoneDetails) {
							ImGui::BeginChild("BoneCard", ImVec2(0, 240), true,
								ImGuiWindowFlags_HorizontalScrollbar);
							ImGui::TextUnformatted(mdl.boneDisplayText.c_str());
							ImGui::EndChild();
						}
					}

					ImGui::PopID();
				}
			}
		}
		else {
			ImGui::TextUnformatted("Select an object in Scene Collection.");
		}
	}
	ImGui::End();
}

void App::RenderSceneCollection() {
	auto& io = ImGui::GetIO();
	// Scene Collection 블렌더의 Hierarchy창
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 330, 20),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(320, 360), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Scene Collection")) {
		if (ImGui::Button("Browse Model...")) {
			std::wstring pathW;
			if (OpenFileDialogModel(pathW)) {
				if (LoadModelFromFile(pathW)) {
					m_->PushLog("[OK] Model Selected " + Utf8FromWString(pathW));
					m_->m_SelectedModelIdx = (int)m_->m_Models.size() - 1;
					int newIndex = (int)m_->m_Models.size() - 1;
					auto mo = std::make_unique<ModelObject>(
						m_->m_Models[(size_t)newIndex]->modelName, newIndex);
					m_->m_Objects.push_back(std::move(mo));
				}
				else {
					m_->PushLog("[ERR] Load failed (Browse) | file extension must be "
						"fbx, obj, pmx");
				}
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Unload Model All")) {
			UnloadModel();
			m_->PushLog("[OK] Unloaded models All");
		}

		// ======================== Audio (FMOD) 전역 제어 ========================
		ImGui::Separator();
		ImGui::TextUnformatted("Audio (FMOD)");

		// 노래(오디오) 로드
		if (ImGui::Button("Load Audio...")) {
			wchar_t file[MAX_PATH] = { 0 };
			OPENFILENAMEW ofn{};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = GameApp::m_hWnd;
			ofn.lpstrFilter =
				L"Audio Files (*.wav;*.mp3;*.ogg)\0*.wav;*.mp3;*.ogg\0All "
				L"Files\0*.*\0\0";
			ofn.lpstrFile = file;
			ofn.nMaxFile = MAX_PATH;
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
			if (GetOpenFileNameW(&ofn)) {
				m_->m_AudioPath = file;
				m_->m_AudioLoaded = Sound::LoadMusic(m_->m_AudioPath);
				if (m_->m_AudioLoaded) {
					m_->PushLog("[OK] Audio loaded (FMOD) : " +
						Utf8FromWString(m_->m_AudioPath));
				}
				else {
					m_->PushLog("[ERR] Audio load failed (FMOD)");
				}
			}
		}

		ImGui::SameLine();
		bool audioLoaded = m_->m_AudioLoaded;
		ImGui::BeginDisabled(!audioLoaded);
		if (ImGui::Button("Play##GlobalAudio")) {
			Sound::Play();
		}
		ImGui::SameLine();
		if (ImGui::Button("Pause##GlobalAudio")) {
			Sound::Pause(true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop##GlobalAudio")) {
			Sound::Stop();
			Sound::SetTimeSeconds(0.0f);
		}
		ImGui::EndDisabled();

		if (audioLoaded) {
			float cur = Sound::GetTimeSeconds();
			float len = Sound::GetLengthSeconds();
			ImGui::Text("Time: %.2f / %.2f sec", cur, len);
			if (len > 0.0f) {
				if (ImGui::SliderFloat("Time (s)##GlobalAudio", &cur, 0.0f, len)) {
					Sound::SetTimeSeconds(cur);
				}
			}
		}

		ImGui::Separator();
		ImGui::Text("Scene Collection");
		// 통합 목록: m_Objects 자체가 소스이므로 매 프레임 재구성하지 않음

		ImGui::BeginChild("##SceneList", ImVec2(0, 0), true,
			ImGuiWindowFlags_HorizontalScrollbar);
		for (int i = 0; i < (int)m_->m_Objects.size(); ++i) {
			ImGui::PushID(i);
			std::wstring wlabel = std::to_wstring(i) + L"  " + m_->m_Objects[i]->name;
			std::string label = Utf8FromWString(wlabel);
			bool sel = (i == m_->m_SelectedItem);
			if (ImGui::Selectable(label.c_str(), sel,
				ImGuiSelectableFlags_AllowItemOverlap))
				m_->m_SelectedItem = i;
			ImGui::SameLine();
			ImGui::SetItemAllowOverlap();
			if (ImGui::SmallButton("Unload")) {
				if (m_->m_Objects[i]->kind == ObjectKind::Cube) {
					// 큐브는 통합 컨테이너에서만 제거
					m_->m_Objects.erase(m_->m_Objects.begin() + i);
				}
				else {
					// 모델은 렌더 소스인 m_Models도 함께 제거
					auto* mo = static_cast<ModelObject*>(m_->m_Objects[i].get());
					int removed = mo->modelIndex;
					if (removed >= 0 && removed < (int)m_->m_Models.size())
						m_->m_Models.erase(m_->m_Models.begin() + removed);
					// 인덱스 재정렬
					for (auto& up : m_->m_Objects) {
						if (up && up->kind == ObjectKind::Model) {
							auto* om = static_cast<ModelObject*>(up.get());
							if (om->modelIndex > removed)
								om->modelIndex -= 1;
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

void App::RenderConsolPannel() {
	// Console
	{
		auto& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(
			ImVec2(io.DisplaySize.x / 2 - 415, io.DisplaySize.y - 210),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(830, 200), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("Console")) {
			ImGui::Checkbox("Auto Scroll", &m_->m_LogAutoScroll);
			ImGui::SameLine();
			ImGui::InputTextWithHint("##LogFilter", "filter...", m_->m_LogFilter, 70);
			ImGui::SameLine();
			if (ImGui::Button("Clear"))
				m_->m_LogLines.clear();
			ImGui::Separator();
			ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false,
				ImGuiWindowFlags_HorizontalScrollbar);
			for (const auto& ln : m_->m_LogLines) {
				if (m_->m_LogFilter[0] != '\0') {
					if (ln.find(m_->m_LogFilter) == std::string::npos)
						continue;
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
	if (m_->m_SkyBoxChoice != App::Impl::SkyBoxChoice::Off) {
		int face = 0;
		using namespace DirectX;
		XMFLOAT3 fwd = m_Camera.GetForward();
		XMVECTOR f = XMLoadFloat3(&fwd);
		XMVECTOR fn = XMVector3Normalize(f);
		XMFLOAT3 v;
		XMStoreFloat3(&v, fn);
		float ax = fabsf(v.x), ay = fabsf(v.y), az = fabsf(v.z);
		if (ax >= ay && ax >= az)
			face = (v.x >= 0.0f) ? 0 : 1; // +X / -X
		else if (ay >= ax && ay >= az)
			face = (v.y >= 0.0f) ? 2 : 3; // +Y / -Y
		else
			face = (v.z >= 0.0f) ? 4 : 5; // +Z / -Z

		ID3D11ShaderResourceView* faceSRV =
			(face >= 0 && face < 6) ? m_->m_pSkyFaceSRV[face] : nullptr;
		if (faceSRV) {
			ImGuiIO& io = ImGui::GetIO();
			ImVec2 pos(io.DisplaySize.x - m_->m_HanakoDrawSize.x - 330.0f, 20.0f);
			ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(
				ImVec2(m_->m_HanakoDrawSize.x + 50, m_->m_HanakoDrawSize.y + 80),
				ImGuiCond_Once);
			if (ImGui::Begin("Skybox Face")) {
				ImGui::BeginChild("SkyFaceView", ImVec2(0, 0), true,
					ImGuiWindowFlags_NoScrollbar |
					ImGuiWindowFlags_NoScrollWithMouse);
				const ImVec2 tex =
					(m_->m_HanakoDrawSize.x > 0 && m_->m_HanakoDrawSize.y > 0)
					? m_->m_HanakoDrawSize
					: m_->m_SkyFaceSize;
				ImVec2 avail = ImGui::GetContentRegionAvail();
				float sx = (tex.x > 0.f) ? (avail.x / tex.x) : 1.f;
				float sy = (tex.y > 0.f) ? (avail.y / tex.y) : 1.f;
				float scale = (sx > 0.f && sy > 0.f) ? std::min(sx, sy) : 1.f;
				ImVec2 draw = ImVec2(tex.x * scale, tex.y * scale);
				ImVec2 start = ImGui::GetCursorPos();
				ImVec2 offset =
					ImVec2((avail.x - draw.x) * 0.5f, (avail.y - draw.y) * 0.5f);
				ImGui::SetCursorPos(start + offset);
				ImGui::Image((ImTextureID)faceSRV, draw);
				ImVec2 r0 = ImGui::GetItemRectMin();
				ImVec2 r1 = ImGui::GetItemRectMax();
				ImGui::GetWindowDrawList()->AddRect(
					r0 - ImVec2(2, 2), r1 + ImVec2(2, 2), IM_COL32(255, 255, 255, 160),
					8.0f, 0, 2.0f);
				ImGui::EndChild();
			}
			ImGui::End();
		}
	}
}

// ============================================================
// UI 헬퍼 함수들
// ============================================================
static void DrawCrossFadeEditor(const char* label, CrossFadeParams& f)
{
    ImGui::PushID(&f);
    ImGui::SeparatorText(label);

    ImGui::DragFloat("Duration (sec)", &f.durationSec, 0.01f, 0.0f, 2.0f, "%.3f");
    ImGui::Checkbox("Use Exit Time", &f.useExitTime);
    if (f.useExitTime)
        ImGui::SliderFloat("Exit Norm", &f.exitNorm, 0.0f, 1.0f, "%.3f");

    ImGui::SliderFloat("Entry Norm", &f.entryNorm, 0.0f, 1.0f, "%.3f");
    ImGui::Checkbox("SmoothStep", &f.smoothStep);

    ImGui::PopID();
}

static void DrawBlendCurve(bool smoothStep, float blend01, ImVec2 size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);

    // 배경
    dl->AddRectFilled(p0, p1, IM_COL32(30, 30, 30, 255), 4.0f);
    dl->AddRect(p0, p1, IM_COL32(90, 90, 90, 255), 4.0f);

    auto Eval = [&](float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        if (!smoothStep) return t;
        return t * t * (3.0f - 2.0f * t);
    };

    // 곡선
    const int N = 64;
    ImVec2 prev{};
    for (int i = 0; i <= N; ++i)
    {
        float x = (float)i / (float)N;
        float y = Eval(x);

        ImVec2 p = ImVec2(
            p0.x + x * size.x,
            p1.y - y * size.y
        );

        if (i > 0)
            dl->AddLine(prev, p, IM_COL32(220, 220, 220, 255), 2.0f);
        prev = p;
    }

    // 현재 점(blend01)
    float yb = Eval(blend01);
    ImVec2 dot = ImVec2(p0.x + blend01 * size.x, p1.y - yb * size.y);
    dl->AddCircleFilled(dot, 5.0f, IM_COL32(255, 200, 0, 255));

    ImGui::Dummy(size);
}

static void DrawStateMachineGraph(const char* id, AnimStateMachine& sm)
{
    const auto& states = sm.GetStates();
    const auto& trans  = sm.GetTransitions();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 size = ImVec2(avail.x, 180.0f);
    ImGui::BeginChild(id, size, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();

    const float nodeW = 120.0f;
    const float nodeH = 36.0f;
    const float gapX  = 40.0f;
    const float gapY  = 28.0f;

    // 간단 배치(가로로 쭉)
    std::vector<ImVec2> nodePos(states.size());
    for (int i = 0; i < (int)states.size(); ++i)
    {
        nodePos[(size_t)i] = ImVec2(
            origin.x + 20.0f + i * (nodeW + gapX),
            origin.y + 40.0f
        );
    }

    int cur = sm.GetCurrentStateIndex();
    bool inTr = sm.IsInTransition();
    int from = sm.GetFromStateIndex();
    int to   = sm.GetToStateIndex();
    float b01 = sm.GetBlend01();

    // 링크 그리기
    for (int ti = 0; ti < (int)trans.size(); ++ti)
    {
        const auto& t = trans[(size_t)ti];

        int a = t.from;
        int b = t.to;
        if (b < 0 || b >= (int)states.size()) continue;

        ImVec2 pA;
        if (a >= 0 && a < (int)states.size())
            pA = ImVec2(nodePos[(size_t)a].x + nodeW, nodePos[(size_t)a].y + nodeH * 0.5f);
        else
            pA = ImVec2(origin.x + 10.0f, origin.y + 40.0f + nodeH * 0.5f); // AnyState

        ImVec2 pB = ImVec2(nodePos[(size_t)b].x, nodePos[(size_t)b].y + nodeH * 0.5f);

        bool activeEdge = inTr && (a == from) && (b == to);
        ImU32 col = activeEdge ? IM_COL32(255, 200, 0, 255) : IM_COL32(120, 120, 120, 255);
        float thickness = activeEdge ? (2.0f + 4.0f * b01) : 2.0f;

        dl->AddLine(pA, pB, col, thickness);

        if (activeEdge)
        {
            ImVec2 mid = ImVec2((pA.x + pB.x) * 0.5f, (pA.y + pB.y) * 0.5f - 14.0f);
            char buf[64];
            snprintf(buf, 64, "blend=%.2f", b01);
            dl->AddText(mid, IM_COL32(255, 220, 120, 255), buf);
        }
    }

    // 노드 그리기 (+ 클릭 강제전환)
    for (int i = 0; i < (int)states.size(); ++i)
    {
        ImVec2 p = nodePos[(size_t)i];
        ImVec2 p2 = ImVec2(p.x + nodeW, p.y + nodeH);

        bool activeNode = (i == cur);
        bool fromNode = inTr && (i == from);
        bool toNode   = inTr && (i == to);

        ImU32 fill = IM_COL32(60, 60, 60, 255);
        ImU32 border = IM_COL32(120, 120, 120, 255);

        if (activeNode) border = IM_COL32(120, 255, 120, 255);
        if (fromNode)   border = IM_COL32(255, 200, 0, 255);
        if (toNode)     border = IM_COL32(255, 200, 0, 255);

        dl->AddRectFilled(p, p2, fill, 6.0f);
        dl->AddRect(p, p2, border, 6.0f, 0, activeNode ? 3.0f : 2.0f);

        dl->AddText(ImVec2(p.x + 8, p.y + 8), IM_COL32(220, 220, 220, 255), states[(size_t)i].name.c_str());

        // 클릭 히트박스
        ImGui::SetCursorScreenPos(p);
        ImGui::InvisibleButton((std::string("node##") + id + std::to_string(i)).c_str(), ImVec2(nodeW, nodeH));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            sm.ForceState(i);
    }

    // 상단 텍스트
    ImGui::SetCursorScreenPos(ImVec2(origin.x + 10, origin.y + 8));
    ImGui::Text("Current: %s  |  InTransition: %s", sm.CurrentStateName().c_str(), inTr ? "true" : "false");

    ImGui::EndChild();
}

static void DrawTransitionsEditor(const char* layerName, AnimStateMachine& sm)
{
    ImGui::PushID(layerName); // ★ 충돌 제거 핵심

    auto& trans = sm.GetTransitions();
    const auto& states = sm.GetStates();

    ImGui::SeparatorText(layerName);

    for (int i = 0; i < (int)trans.size(); ++i)
    {
        ImGui::PushID(i);

        auto& t = trans[(size_t)i];
        auto NameOf = [&](int idx)->const char*
        {
            if (idx < 0) return "*";
            if (idx >= 0 && idx < (int)states.size()) return states[(size_t)idx].name.c_str();
            return "?";
        };

        std::string header = std::string(NameOf(t.from)) + " -> " + NameOf(t.to) +
            "  (pri=" + std::to_string(t.priority) + ")";

        if (ImGui::TreeNode("##trnode", "%s", header.c_str()))
        {
            ImGui::DragInt("Priority", &t.priority, 1, -1000, 1000);

            DrawCrossFadeEditor("Fade", t.fade);

            ImGui::SeparatorText("Conditions");
            for (int ci = 0; ci < (int)t.conditions.size(); ++ci)
            {
                const auto& c = t.conditions[(size_t)ci];
                ImGui::BulletText("cond[%d] type=%d param=%s (f=%.2f i=%d)",
                    ci, (int)c.type, c.param.c_str(), c.f, c.i);
            }

            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::PopID();
}

void App::RenderAdvancedRigUI()
{
    if (!m_->m_CharRigInited) return;

    auto& ctrl = m_->m_CharCtrl;

    ImGui::Begin("AnimGraph / Blend Debug##AdvancedRig");

    ImGui::Checkbox("Enable AdvancedRig", &m_->m_UseAdvancedRig);
    ImGui::Text("Rig Inited: %s", m_->m_CharRigInited ? "true" : "false");
    ImGui::Separator();

	// ----- Weapon Socket Transform -----
	ImGui::SeparatorText("Weapon Socket Transform");
	{
		auto& sock = m_->m_CharCtrl.config.weaponSocket;
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 1.0f, 1.0f), "[%s] attached to [%s]", sock.socketName.c_str(), sock.parentBone.c_str());
		bool changed = false;

		// |= 연산자를 사용해 하나라도 변경되면 changed가 true가 됨
		changed |= ImGui::DragFloat3("Socket Pos", &sock.pos.x, 1.00f, -500.0f,500.0f, "%.4f");
		changed |= ImGui::DragFloat3("Socket Rot", &sock.rotDeg.x, 1.0f, -360.0f, 360.0f);
		changed |= ImGui::DragFloat3("Socket Scale", &sock.scale.x, 0.01f, 0.1f, 10.0f);

		// 변경 사항이 있을 때만 업데이트 함수 호출
		if (changed)
		{
			m_->m_CharCtrl.AdjustSocket();
		}
	}

    // ----- Base/Upper/Add Runtime -----
    ImGui::SeparatorText("Runtime States");
    ImGui::Text("Base : %s", ctrl.DebugBaseState().c_str());
    ImGui::Text("Upper: %s", ctrl.DebugUpperState().c_str());
    ImGui::Text("Add  : %s", ctrl.DebugAddState().c_str());

    // 전환 진행률 + 커브
    {
        auto& b = ctrl.BaseSM();
        ImGui::SeparatorText("Base Transition");
        ImGui::Text("InTransition=%s  blend=%.3f  from=%d to=%d",
            b.IsInTransition() ? "true" : "false",
            b.GetBlend01(), b.GetFromStateIndex(), b.GetToStateIndex());
        ImGui::ProgressBar(b.IsInTransition() ? b.GetBlend01() : 0.0f, ImVec2(-1, 0));
        DrawBlendCurve(b.GetActiveFade().smoothStep, b.IsInTransition() ? b.GetBlend01() : 0.0f, ImVec2(ImGui::GetContentRegionAvail().x, 70));
    }

    {
        auto& u = ctrl.UpperSM();
        ImGui::SeparatorText("Upper Transition");
        ImGui::Text("InTransition=%s  blend=%.3f", u.IsInTransition() ? "true" : "false", u.GetBlend01());
        ImGui::ProgressBar(u.IsInTransition() ? u.GetBlend01() : 0.0f, ImVec2(-1, 0));
        DrawBlendCurve(u.GetActiveFade().smoothStep, u.IsInTransition() ? u.GetBlend01() : 0.0f, ImVec2(ImGui::GetContentRegionAvail().x, 70));
    }

    {
        auto& a = ctrl.AddSM();
        ImGui::SeparatorText("Additive Transition");
        ImGui::Text("InTransition=%s  blend=%.3f", a.IsInTransition() ? "true" : "false", a.GetBlend01());
        ImGui::ProgressBar(a.IsInTransition() ? a.GetBlend01() : 0.0f, ImVec2(-1, 0));
        DrawBlendCurve(a.GetActiveFade().smoothStep, a.IsInTransition() ? a.GetBlend01() : 0.0f, ImVec2(ImGui::GetContentRegionAvail().x, 70));
    }

    // ----- Graph View -----
    ImGui::SeparatorText("Graph View (click node = Force State)");
    DrawStateMachineGraph("BaseGraph",  ctrl.BaseSM());
    DrawStateMachineGraph("UpperGraph", ctrl.UpperSM());
    DrawStateMachineGraph("AddGraph",   ctrl.AddSM());

    // ----- Transition Parameter Edit -----
    ImGui::SeparatorText("Edit Transitions (live)");
    DrawTransitionsEditor("Base Transitions",  ctrl.BaseSM());
    DrawTransitionsEditor("Upper Transitions", ctrl.UpperSM());
    DrawTransitionsEditor("Add Transitions",   ctrl.AddSM());

    // ----- Slot(Key) -> Animation Index Mapping -----
    ImGui::SeparatorText("Slot Mapping (key -> animation index)");
    {
        auto& map = ctrl.GetAnimIndexMap();
        const auto& animNames = ctrl.GetAnimNames();

        // 키 정렬해서 보여주기
        std::vector<std::string> keys;
        keys.reserve(map.size());
        for (auto& kv : map) keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());

        for (auto& k : keys)
        {
            int& idx = map[k];
            ImGui::PushID(k.c_str());

            ImGui::Text("%s", k.c_str());
            ImGui::SameLine(140);

            // 콤보로 애니 선택
            int cur = idx;
            const char* preview = (cur >= 0 && cur < (int)animNames.size()) ? animNames[(size_t)cur].c_str() : "<none>";
            if (ImGui::BeginCombo("##AnimCombo", preview))
            {
                // none
                if (ImGui::Selectable("<none>", cur < 0)) idx = -1;

                for (int i = 0; i < (int)animNames.size(); ++i)
                {
                    bool sel = (i == cur);
                    if (ImGui::Selectable(animNames[(size_t)i].c_str(), sel))
                        idx = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::PopID();
        }

        if (ImGui::Button("AutoBind (contains)"))
            ctrl.AutoBindCommonSlotsByContains();

        ImGui::SameLine();
        if (ImGui::Button("Rebuild Default Graph"))
        {
            // 그래프를 다시 만들고 싶으면:
            // - config나 slot mapping 바꾼 뒤 적용하려면 rebuild가 필요할 때가 있음
            ctrl.BuildDefaultGraph();
            ctrl.ResetRuntime();
        }
    }

    // ----- Recoil (Shake) -----
    ImGui::SeparatorText("Recoil (Shake)");
    ImGui::SliderFloat("Recoil Kick",  &m_->m_RecoilKickUi,  0.0f, 1.5f);
    ImGui::SliderFloat("Recoil Decay", &m_->m_RecoilDecayUi, 0.0f, 30.0f);

    // ----- Character Move (TPS) -----
    ImGui::SeparatorText("Character Move (TPS)");
    ImGui::Checkbox("Rotate to Move Dir", &m_->m_CharRotateToMove);
    ImGui::SliderFloat("Walk Speed", &m_->m_CharWalkSpeed, 10.0f, 800.0f);
    ImGui::SliderFloat("Run Mul",    &m_->m_CharRunMul,    1.0f,  3.0f);
    ImGui::SliderFloat("Turn Speed", &m_->m_CharTurnSpeed, 1.0f, 25.0f);

    // ----- Sniper -----
    ImGui::SeparatorText("Sniper");
    ImGui::Checkbox("Sniper Enabled", &m_->m_SniperEnabled);
    ImGui::SliderFloat("Charge Time (sec)", &m_->m_SniperChargeTimeSec, 0.1f, 3.0f);
    ImGui::SliderFloat("Aim Radius", &m_->m_SniperAimRadius, 4.0f, 30.0f);



    ImGui::End();
}

void App::RenderSceneImageWindow() {
	if (!m_->m_ShowSceneImageWindow)
		return;

	// 씬 이미지 창 위치/크기 설정 (08_ImguiSystemInfo 참고)
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 size(130.0f, 260.0f);
	ImVec2 pos(io.DisplaySize.x - size.x - 330.0f, 20.0f);
	ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Scene Image", &m_->m_ShowSceneImageWindow)) {
		// 빛나는 글씨 효과로 "눌러보세요!" 텍스트 표시
		// 펄스 효과를 위한 시간 기반 색상 계산 (더 빠르고 눈에 띄게)
		float time = (float)ImGui::GetTime();
		float pulse =
			(sinf(time * 5.0f) + 1.0f) * 0.5f; // 0.0 ~ 1.0 사이 값 (속도 증가)

		// 밝은 노란색에서 흰색으로 강렬한 펄스 효과
		// 색상 범위를 넓혀서 더 눈에 띄게 만듦
		float r = 0.9f + pulse * 0.1f; // 0.9 ~ 1.0 (약간 변동)
		float g = 0.5f + pulse * 0.5f; // 0.5 ~ 1.0 (넓은 범위)
		float b = 0.0f + pulse * 0.8f; // 0.0 ~ 0.8 (더 넓은 범위)
		float a = 0.5f + pulse * 0.5f; // 0.5 ~ 1.0 (더 넓은 범위)

		// 글씨 크기 키우기
		ImGui::SetWindowFontScale(1.5f); // 기본 크기의 1.5배
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(r, g, b, a));
		ImGui::Text("%s", Utf8FromWString(L"눌러보세요!").c_str());
		ImGui::PopStyleColor();
		ImGui::SetWindowFontScale(1.0f); // 원래 크기로 복원
		ImGui::Separator();

		// 이미지 표시
		if (m_->m_pSceneImageSRV) {
			ImVec2 avail = ImGui::GetContentRegionAvail();
			float aspect = (m_->m_SceneImageSize.y > 0)
				? (m_->m_SceneImageSize.x / m_->m_SceneImageSize.y)
				: 1.0f;
			float displayWidth = avail.x;
			float displayHeight = displayWidth / aspect;
			if (displayHeight > avail.y) {
				displayHeight = avail.y;
				displayWidth = displayHeight * aspect;
			}

			// 이미지 위치 계산
			ImVec2 imagePos = ImGui::GetCursorScreenPos();
			ImVec2 imageSize(displayWidth, displayHeight);

			// 이미지를 클릭 가능한 버튼처럼 표시 (투명 버튼 위에 이미지)
			ImGui::PushID("SceneImageButton");
			if (ImGui::InvisibleButton("##SceneImage", imageSize)) {
				// 이미지 클릭 시 임시 이미지로 변경
				if (!m_->m_IsUsingTempImage && !m_->m_ShowScenePopup) {
					// 원본 이미지 경로 저장
					m_->m_OriginalSceneImagePath = m_->m_CurrentSceneImagePath;

					// 현재 씬에 따라 다른 임시 이미지와 메시지 설정
					if (m_SceneIndex == 0) {
						// SceneA일 때 SceneB 이미지로 변경
						m_->m_TempSceneImagePath = L"..\\Resource\\Image\\SceneB.png";
						m_->m_ScenePopupMessage = Utf8FromWString(L"안녕하세요 토끼씨!");
					}
					else {
						// SceneB일 때 SceneA 이미지로 변경
						m_->m_TempSceneImagePath = L"..\\Resource\\Image\\SceneA.png";
						m_->m_ScenePopupMessage = Utf8FromWString(L"기뻐요 토끼씨!");
					}

					// 임시 이미지 로드
					LoadSceneImage(m_->m_TempSceneImagePath);
					m_->m_CurrentSceneImagePath = m_->m_TempSceneImagePath;
					m_->m_IsUsingTempImage = true;

					// 팝업 표시 및 타이머 시작 (2초 후 원본으로 복원)
					m_->m_ShowScenePopup = true;
					m_->m_ScenePopupTimer = 2.0f;
				}
			}
			ImGui::PopID();

			// 이미지 표시 (버튼 위에)
			ImGui::SetItemAllowOverlap();
			ImGui::SetCursorScreenPos(imagePos);
			ImGui::Image((ImTextureID)m_->m_pSceneImageSRV, imageSize);

			// 팝업 표시 (이미지 위에 오버레이)
			if (m_->m_ShowScenePopup) {
				ImDrawList* drawList = ImGui::GetWindowDrawList();

				// 팝업 배경 (반투명)
				ImVec2 popupSize(300.0f, 80.0f);
				ImVec2 popupPos(imagePos.x + (imageSize.x - popupSize.x) * 0.5f,
					imagePos.y + (imageSize.y - popupSize.y) * 0.5f);
				ImVec2 popupEnd =
					ImVec2(popupPos.x + popupSize.x, popupPos.y + popupSize.y);

				// 배경 그리기
				drawList->AddRectFilled(popupPos, popupEnd, IM_COL32(0, 0, 0, 200),
					10.0f);
				drawList->AddRect(popupPos, popupEnd, IM_COL32(255, 255, 255, 255),
					10.0f, 0, 2.0f);

				// 텍스트 그리기 (중앙 정렬)
				ImVec2 textSize = ImGui::CalcTextSize(m_->m_ScenePopupMessage.c_str());
				ImVec2 textPos(popupPos.x + (popupSize.x - textSize.x) * 0.5f,
					popupPos.y + (popupSize.y - textSize.y) * 0.5f);
				drawList->AddText(textPos, IM_COL32(255, 255, 255, 255),
					m_->m_ScenePopupMessage.c_str());
			}
		}
		else {
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "Image not loaded");
		}
	}
	ImGui::End();
}

// G-Buffer 디버그 뷰
void App::RenderGBufferDebug() {
	if (!m_->m_UseDeferredRendering)
		return;

	ImGuiIO& io = ImGui::GetIO();
	ImVec2 pos(io.DisplaySize.x - 350.0f, 20.0f);
	ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(330.0f, 400.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("G-Buffer Debug View")) {
		ImGui::Text("G-Buffer Contents");
		ImGui::Separator();

		// 각 G-Buffer 텍스처 표시
		const char* gbufferNames[] = { "PositionWS", "NormalWS", "Metalness", "Roughness", "BaseColor" };

		for (int i = 0; i < Impl::GBufferCount; ++i) {
			ImGui::Text("%s", gbufferNames[i]);
			ImGui::Image((ImTextureID)m_->m_pGBufferSRVs[i].Get(), ImVec2(128, 128));
			ImGui::Separator();
		}
	}
	ImGui::End();
}

// Deferred Rendering UI - 설정 및 G-Buffer 디버그 뷰
void App::RenderDeferredUI() {
	// Deferred Rendering이 활성화되지 않았으면 표시하지 않음
	if (!m_->m_UseDeferredRendering)
		return;

	// Deferred Rendering Settings 윈도우
	ImGuiIO& io = ImGui::GetIO();
	ImVec2 settingsPos(20.0f, 20.0f);
	ImGui::SetNextWindowPos(settingsPos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(350.0f, 500.0f), ImGuiCond_FirstUseEver);

	// G-Buffer Debug View 윈도우
	ImVec2 gbufferPos(io.DisplaySize.x - 350.0f, 20.0f);
	ImGui::SetNextWindowPos(gbufferPos, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(330.0f, 600.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("G-Buffer Debug View")) {
		ImGui::Text("G-Buffer Contents");
		ImGui::Separator();

		// 각 G-Buffer 텍스처 이름
		const char* gbufferNames[] = { "PositionWS", "NormalWS", "Metalness",
									  "Roughness", "BaseColor" };

		// G-Buffer 텍스처들을 2x3 그리드로 표시
		for (int i = 0; i < Impl::GBufferCount; ++i) {
			ImGui::BeginGroup();
			ImGui::Text("%s", gbufferNames[i]);
			ImGui::Image((ImTextureID)m_->m_pGBufferSRVs[i].Get(), ImVec2(128, 128));
			ImGui::EndGroup();

			// 2개씩 한 줄에 배치
			if ((i + 1) % 2 != 0 && i < Impl::GBufferCount - 1) {
				ImGui::SameLine();
			}
			else {
				ImGui::Separator();
			}
		}

		// Depth Buffer 표시 (있는 경우)
		if (m_->m_pDepthStencilView) {
			ImGui::Separator();
			ImGui::Text("Depth Buffer");
			// Depth SRV가 별도로 있는지 확인 필요
			// 일반적으로 Depth는 별도 SRV로 접근하므로, 여기서는 주석 처리
			// ImGui::Image((ImTextureID)m_->m_pDepthSRV.Get(), ImVec2(128, 128));
		}
	}
	ImGui::End();
}

void App::LoadSceneImage(const std::wstring& path) {
	// 기존 이미지 해제
	SAFE_RELEASE(m_->m_pSceneImageSRV);
	m_->m_SceneImageSize = ImVec2(0, 0);

	// 새 이미지 로드
	if (LoadTextureSRVAndSize(m_->m_pDevice, path, &m_->m_pSceneImageSRV,
		&m_->m_SceneImageSize)) {
		m_->PushLog("[OK] Scene image loaded: " + Utf8FromWString(path));
	}
	else {
		m_->PushLog("[ERR] Failed to load scene image: " + Utf8FromWString(path));
	}
}

void App::ChangeIBLSkyBox(const std::wstring& path) {
	m_->m_pIblDiffuseSRV = nullptr;
	m_->m_pIblSpecularSRV = nullptr;
	m_->m_pIblBrdfLutSRV = nullptr;
	{
		HR_T(CreateDDSTextureFromFile(m_->m_pDevice,
			(path + L"DiffuseHDR.dds").c_str(), nullptr,
			&m_->m_pIblDiffuseSRV));

		HR_T(CreateDDSTextureFromFile(m_->m_pDevice,
			(path + L"SpecularHDR.dds").c_str(), nullptr,
			&m_->m_pIblSpecularSRV));

		HR_T(CreateDDSTextureFromFile(m_->m_pDevice, (path + L"Brdf.dds").c_str(),
			nullptr, &m_->m_pIblBrdfLutSRV));

		m_->PushLog("[OK] Loaded IBL(Sample): Diffuse / Specular / BRDF LUT");
	}

	// IBL과 동일한 환경맵을 스카이박스에도 사용 (배경과 반사가 일치하도록)
	ChangeSkyboxDDS((path + L"EnvHDR.dds").c_str());
}

void App::ChangeScene(std::unique_ptr<Scene> next) {
	if (!next)
		return;
	if (m_CurrentScene)
		m_CurrentScene->OnExit();
	m_CurrentScene = std::move(next);
	m_CurrentScene->OnEnter();
}

void App::TrimVideoMemory() {
	Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
	if (m_->m_pDevice && SUCCEEDED(m_->m_pDevice->QueryInterface(
		IID_PPV_ARGS(dxgiDevice.GetAddressOf())))) {
		Microsoft::WRL::ComPtr<IDXGIDevice3> dxgiDevice3;
		if (SUCCEEDED(dxgiDevice.As(&dxgiDevice3)) && dxgiDevice3) {
			dxgiDevice3->Trim();
		}
	}
}

bool App::CheckHDRSupportAndGetMaxNits(float& outMaxNits,
	DXGI_FORMAT& outFormat) {
	using namespace Microsoft::WRL;
	ComPtr<IDXGIFactory4> pFactory;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory));
	if (FAILED(hr)) {
		LOG_ERRORA("ERROR: DXGI Factory 생성 실패.\n");
		return false;
	}
	// 2. 주 그래픽 어댑터 (0번) 열거
	ComPtr<IDXGIAdapter1> pAdapter;
	UINT adapterIndex = 0;
	while (pFactory->EnumAdapters1(adapterIndex, &pAdapter) !=
		DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC1 desc;
		pAdapter->GetDesc1(&desc);

		// WARP 어댑터(소프트웨어)를 건너뛰고 주 어댑터만 사용하도록 선택할 수
		// 있습니다.
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			adapterIndex++;
			pAdapter.Reset();
			continue;
		}
		break;
	}

	if (!pAdapter) {
		LOG_ERRORA("ERROR: 유효한 하드웨어 어댑터를 찾을 수 없습니다.\n");
		return false;
	}

	// 3. 주 모니터 출력 (0번) 열거
	ComPtr<IDXGIOutput> pOutput;
	hr = pAdapter->EnumOutputs(0, &pOutput); // 0번 출력
	if (FAILED(hr)) {
		LOG_ERRORA("ERROR: 주 모니터 출력(Output 0)을 찾을 수 없습니다.\n");
		return false;
	}

	// 4. HDR 정보를 얻기 위해 IDXGIOutput6으로 쿼리
	ComPtr<IDXGIOutput6> pOutput6;
	hr = pOutput.As(&pOutput6);
	if (FAILED(hr)) {
		printf("INFO: IDXGIOutput6 인터페이스를 얻을 수 없습니다. HDR 정보를 얻을 "
			"수 없습니다.\n");
		outMaxNits = 100.0f;
		outFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		return false;
	}

	// 5. DXGI_OUTPUT_DESC1에서 HDR 정보 확인
	DXGI_OUTPUT_DESC1 desc1 = {};
	hr = pOutput6->GetDesc1(&desc1);
	if (FAILED(hr)) {
		printf("ERROR: GetDesc1 호출 실패.\n");
		return false;
	}

	// 6. HDR 활성화 조건 분석
	bool isHDRColorSpace =
		(desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
	outMaxNits = (float)desc1.MaxLuminance;

	// OS가 HDR을 켰을 때 MaxLuminance는 100 Nits(SDR 기준)를 초과합니다.
	bool isHDRActive = outMaxNits > 100.0f;

	if (isHDRColorSpace && isHDRActive) {
		// 최종 판단: HDR 지원 및 OS 활성화
		outFormat = DXGI_FORMAT_R10G10B10A2_UNORM; // HDR 포맷 설정
		printf("SUCCESS: HDR 활성화됨. MaxNits: %.1f, Format: R10G10B10A2_UNORM\n",
			outMaxNits);
		return true;
	}
	else {
		// HDR 지원 안함 또는 OS에서 비활성화
		outMaxNits = 100.0f;                    // SDR 기본값
		outFormat = DXGI_FORMAT_R8G8B8A8_UNORM; // SDR 포맷 설정
		printf("INFO: HDR 비활성화. MaxNits: 100.0, Format: R8G8B8A8_UNORM\n");
		return false;
	}
	return true;
}

//  DXGI_FORMAT_R8G8B8A8_UNORM : LDR
//  DXGI_FORMAT_R10G10B10A2_UNORM   : HDR
void App::CreateSwapChainAndBackBuffer(DXGI_FORMAT format) {
	m_->m_format = format;
	if (!(format == DXGI_FORMAT_R8G8B8A8_UNORM ||
		format == DXGI_FORMAT_R10G10B10A2_UNORM)) {
		m_->m_format = DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	HRESULT hr = 0; // 결과값.

	// 스왑체인 속성 설정 구조체 생성.
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 2;
	swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd; // 스왑체인 출력할 창 핸들 값.
	swapDesc.Windowed = true;       // 창 모드 여부 설정.
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
	HR_T(D3D11CreateDeviceAndSwapChain(
		NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_->m_pSwapChain, &m_->m_pDevice, NULL,
		&m_->m_pDeviceContext));

	// 4. 렌더타겟뷰 생성.  (백버퍼를 이용하는 렌더타겟뷰)
	ID3D11Texture2D* pBackBufferTexture = nullptr;
	HR_T(m_->m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
		(void**)&pBackBufferTexture));
	HR_T(m_->m_pDevice->CreateRenderTargetView(
		pBackBufferTexture, NULL,
		&m_->m_pRenderTargetView));   // 텍스처는 내부 참조 증가
	SAFE_RELEASE(pBackBufferTexture); // 외부 참조 카운트를 감소시킨다.

	Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
	HR_T(m_->m_pSwapChain->QueryInterface(__uuidof(IDXGISwapChain3),
		(void**)&swapChain3));
	if (m_->m_format == DXGI_FORMAT_R10G10B10A2_UNORM) {
		// EOTF = PQ (ST.2084 / G2084)  , 색역(Primaries) = Rec.2020 , RGB Full
		// Range 이 스왑체인의 0.0~1.0 값은 선형 RGB나 감마 값이 아니라 PQ로
		// 인코딩된 HDR10 신호로 해석하라”
		HR_T(
			swapChain3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020));
	}
}