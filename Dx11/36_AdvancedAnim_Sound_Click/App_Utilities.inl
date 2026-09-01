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
	m_->m_CurrentIBLPath = path;

	auto releaseSRV = [](ID3D11ShaderResourceView*& srv) {
		if (srv) {
			srv->Release();
			srv = nullptr;
		}
	};

	ID3D11ShaderResourceView* diffuseSRV = nullptr;
	ID3D11ShaderResourceView* specularSRV = nullptr;
	ID3D11ShaderResourceView* brdfSRV = nullptr;

	auto loadDDS = [&](const std::wstring& file,
		ID3D11ShaderResourceView** outSRV) -> bool {
			if (!std::filesystem::exists(file)) {
				m_->PushLog("[WARN] Missing IBL asset: " + Utf8FromWString(file));
				return false;
			}
			const HRESULT hr = CreateDDSTextureFromFile(
				m_->m_pDevice, file.c_str(), nullptr, outSRV);
			if (FAILED(hr)) {
				m_->PushLog("[WARN] Failed IBL asset: " + Utf8FromWString(file));
				return false;
			}
			return true;
		};

	const std::wstring diffusePath = path + L"DiffuseHDR.dds";
	const std::wstring specularPath = path + L"SpecularHDR.dds";
	const std::wstring brdfPath = path + L"Brdf.dds";
	if (!SkyboxAssetManager::HasIBLAssetSet(path)) {
		SkyboxAssetManager::EnsureSkyboxAssetsAsync();
		m_->PushLog("[INFO] Skybox IBL validation is pending; using the neutral fallback.");
		releaseSRV(m_->m_pIblDiffuseSRV);
		releaseSRV(m_->m_pIblSpecularSRV);
		releaseSRV(m_->m_pIblBrdfLutSRV);
		ChangeSkyboxDDS(L"..\\Resource\\Skybox\\cubemap.dds");
		return;
	}
	const bool loaded = loadDDS(diffusePath, &diffuseSRV) &&
		loadDDS(specularPath, &specularSRV) && loadDDS(brdfPath, &brdfSRV);

	releaseSRV(m_->m_pIblDiffuseSRV);
	releaseSRV(m_->m_pIblSpecularSRV);
	releaseSRV(m_->m_pIblBrdfLutSRV);

	if (loaded) {
		m_->m_pIblDiffuseSRV = diffuseSRV;
		diffuseSRV = nullptr;
		m_->m_pIblSpecularSRV = specularSRV;
		specularSRV = nullptr;
		m_->m_pIblBrdfLutSRV = brdfSRV;
		brdfSRV = nullptr;
		m_->PushLog("[OK] Loaded IBL: " + Utf8FromWString(path));
	}
	else {
		m_->PushLog("[WARN] IBL disabled; keeping app alive without this asset set.");
	}

	releaseSRV(diffuseSRV);
	releaseSRV(specularSRV);
	releaseSRV(brdfSRV);

	const std::wstring envPath = path + L"EnvHDR.dds";
	ChangeSkyboxDDS(std::filesystem::exists(envPath) ? envPath.c_str()
		: L"..\\Resource\\Skybox\\cubemap.dds");
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

namespace {
// The format the swap chain is actually created with.
//
// Outside README capture mode this only clamps the caller's request to the two
// formats this application knows how to present.
//
// Inside capture mode it pins the back buffer to plain 8-bit sRGB. The capture
// publisher (App_PortfolioShowcase.inl) copies back-buffer pixels straight into
// a PNG, and a PNG is an sRGB container: an HDR back buffer carries a PQ
// (ST.2084) encoded Rec.2020 signal, which WIC would only range-convert from 10
// to 8 bits - no transfer function, no gamut mapping - publishing a washed-out,
// wrong-hue image that every automated check still accepts. Pinning the format
// also routes PassPostProcess through 36_ToneMappingPS_LDR.hlsl, whose
// gamma-encoded sRGB output is exactly what a PNG wants, and keeps
// SetColorSpace1 below off the HDR10 colour space.
DXGI_FORMAT ResolveSwapChainFormat(DXGI_FORMAT requested,
	bool readmeCaptureMode) noexcept {
	if (readmeCaptureMode) {
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
	if (requested == DXGI_FORMAT_R8G8B8A8_UNORM ||
		requested == DXGI_FORMAT_R10G10B10A2_UNORM) {
		return requested;
	}
	return DXGI_FORMAT_R8G8B8A8_UNORM;
}
} // namespace

//  DXGI_FORMAT_R8G8B8A8_UNORM : LDR
//  DXGI_FORMAT_R10G10B10A2_UNORM   : HDR
void App::CreateSwapChainAndBackBuffer(DXGI_FORMAT format) {
	m_->m_format = ResolveSwapChainFormat(format, IsReadmeCaptureMode());

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
