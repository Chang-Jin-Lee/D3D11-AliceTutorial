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
	Material instanceMaterial{ {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 32}, {0, 0, 0, 0} };
	bool useInstanceMaterial = false;
	bool useNormalMap = false;
	bool useSpecularMap = false;
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
	std::wstring audioKey; // 사운드 매니저에서 사용할 key
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
	XMVECTOR b = XMVectorSet(XMVectorGetX(toDirWS), 0.0f, XMVectorGetZ(toDirWS), 0.0f);

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

	std::vector<int8_t> memo(cache.size(), -1);

	auto subtreeContainsFilter = [&](auto&& self, int idx) -> bool {
		if (!useFilter) return true;
		if (idx < 0 || idx >= (int)cache.size()) return false;

		int8_t& m = memo[(size_t)idx];
		if (m != -1) return m != 0;

		bool ok = (cache[idx].nameU8.find(f) != std::string::npos);
		if (!ok) {
			for (int ch : cache[idx].children) {
				if (self(self, ch)) { ok = true; break; }
			}
		}
		m = ok ? 1 : 0;
		return ok;
		};

	/*std::function<bool(int)> subtreeContainsFilter = [&](int idx) -> bool {
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
		};*/

	std::function<void(int, int)> dfs = [&](int idx, int depth) {
		if (idx < 0 || idx >= (int)cache.size())
			return;
		//if (useFilter && !subtreeContainsFilter(idx))
		if (useFilter && !subtreeContainsFilter(subtreeContainsFilter, idx))
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
	std::wstring m_CurrentIBLPath = L"..\\Resource\\Skybox\\Sample\\BakerSample";
	unsigned int m_SkyboxAssetGeneration = 0;

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
	std::wstring m_AudioKey; // 사운드 매니저에서 사용할 key. mmd 스타일로 실행할 그 버튼
	bool m_AudioLoaded = false;

	// SoundBox 시스템
	SoundBoxSystem m_SoundBoxSystem;

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
	std::wstring m_OriginalSceneImagePath = L"..\\Resource\\Image\\SceneA.png"; // 원본 이미지 경로
	std::wstring m_TempSceneImagePath;      // 임시 이미지 경로
	ID3D11ShaderResourceView* m_pSceneImageSRV = nullptr;
	ImVec2 m_SceneImageSize = ImVec2(0, 0);
	bool m_ShowSceneImageWindow = true;
	bool m_IsUsingTempImage = false; // 임시 이미지 사용 중인지 여부

	// 씬 변경 팝업 관련
	bool m_ShowScenePopup = false;
	float m_ScenePopupTimer = 0.0f;
	std::string m_ScenePopupMessage;

	// Public image viewer state
	int m_MangaIndex = 0;  // 0 = Public loading image, 1-3 = Public comic pages
	bool m_LoadingDoneSoundPlayed = false;  // public loading-complete UI sound state

	// VMD 카메라 상태 (공용)
	mmd::VmdCameraState m_VmdCamera;

	// ===================== Advanced Character Rig (Socket/Blend/Layer/IK) =====================
	// - CharacterAnimController가 모든 로직을 처리
	bool m_UseAdvancedRig = true;
	bool m_CharRigInited = false;
	int m_CharModelIndex = 0;
	int m_WeaponModelIndex = -1;

	ExternalAnimationClipLibrary m_ExternalAnimClips;
	CharacterAnimController m_CharCtrl;

	// ===================== TPS Camera Follow =====================
	bool  m_TpsCamAttached = false;     // V키 토글
	float m_TpsYawRad = 0.0f;
	float m_TpsPitchRad = XMConvertToRadians(15.0f);

	float m_TpsDist = 220.0f;
	float m_TpsDistMin = 80.0f;
	float m_TpsDistMax = 450.0f;
	float m_TpsZoomStep = 12.0f;        // 휠 1칸 당 거리 변화

	float m_TpsRotSpeed = 0.004f;       // 마우스 회전 민감도
	float m_TpsPitchMin = XMConvertToRadians(-35.0f);
	float m_TpsPitchMax = XMConvertToRadians(60.0f);

	float m_TpsTargetHeightStand = 95.0f;
	float m_TpsTargetHeightCrouch = 70.0f;

	int   m_TpsLastWheel = 0;

	// Aim
	float m_AimYawSmoothed = 0.0f;
	float m_AimSmoothing = 18.0f;         // 클수록 빨리 따라감
	float m_AimMaxYawDeg = 70.0f;         // 좌우 제한
	float m_AimFarDist = 2000.0f;       // 조준점 생성용 거리

	// ===================== TPS Character Control =====================
	float m_CharWalkSpeed = 180.0f;   // 걷기 속도 (units/sec) - UI에서 조절
	float m_CharRunMul = 1.7f;     // 달리기 배율 - UI에서 조절
	float m_CharTurnSpeed = 12.0f;    // 회전 따라가기(라디안/초) - UI에서 조절
	bool  m_CharRotateToMove = true; // 이동 방향으로 몸통 회전

	// ===================== Recoil(UI) : 앉은 상태 클릭 흔들림 줄이기 =====================
	float m_RecoilKickUi = 0.25f;    // 기본값을 낮춰서 "덜 흔들리게" (UI에서 조절)
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
	bool m_LastRun = false;

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
		L"Models (*.fbx;*.obj;*.pmx;*.gltf;*.glb)\0*.fbx;*.obj;*.pmx;*.gltf;*.glb\0All Files\0*.*\0\0";
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
