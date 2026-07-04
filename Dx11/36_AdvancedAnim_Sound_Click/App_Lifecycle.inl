App::App() : m_(new Impl) {}
App::~App() {}


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

	// ====================================== 씬 이미지 초기 로드  ====================================== 
	// 만화 뷰어 초기화: 처음에는 AliceDagwa.png 표시
	m_->m_MangaIndex = 0;
	m_->m_CurrentSceneImagePath = L"..\\Resource\\Image\\AliceDagwa.png";
	m_->m_OriginalSceneImagePath = m_->m_CurrentSceneImagePath;
	LoadSceneImage(m_->m_CurrentSceneImagePath);

	// 로비에서 나올 음성
	Sound::Load(L"Waitforsecond3", L"..\\Resource\\Sound\\Dagwa.mp3", Sound::Type::SFX);
	Sound::Load(L"Waitforsecond2", L"..\\Resource\\Sound\\Waitforsecond2.mp3", Sound::Type::SFX);
	Sound::Load(L"Waitforsecond", L"..\\Resource\\Sound\\Waitforsecond.mp3", Sound::Type::SFX);
	Sound::Load(L"LoadingDone", L"..\\Resource\\Sound\\LoadingDone.mp3", Sound::Type::SFX);

	// 데이터 로딩은 별도 스레드에서 시작 
	m_loaderThread = std::jthread([this](std::stop_token st)
		{
			LoadDataAsync(st);
		});

	return true;
}

void App::LoadDataAsync(std::stop_token stoken)
{
	m_fLoadingProgress = 0.05f;
	m_sLoadingStr = L"FMOD 사운드를 초기화 합니다.";
	if (stoken.stop_requested()) return;
	// ====================================== FMOD 사운드 초기화 ======================================
	if (!Sound::Initialize()) {
		m_->PushLog("[ERR] FMOD initialize failed");
	}
	else {
		m_->PushLog("[OK] FMOD initialized");
		// SFX 로드 
		Sound::Load(L"Walk", L"..\\Resource\\Sound\\Walk.mp3", Sound::Type::SFX);
		Sound::Load(L"RunVoice", L"..\\Resource\\Sound\\Run_voice.mp3", Sound::Type::SFX);
		Sound::Load(L"Shoot", L"..\\Resource\\Sound\\Shoot.mp3", Sound::Type::SFX);
		Sound::Load(L"ShootCharged", L"..\\Resource\\Sound\\ShootCharged.mp3", Sound::Type::SFX);
		Sound::Load(L"Reload", L"..\\Resource\\Sound\\Reload.mp3", Sound::Type::SFX);
		Sound::Load(L"LetItHappen", L"..\\Resource\\Sound\\LetItHappen.mp3", Sound::Type::BGM);
		Sound::Load(L"test", L"..\\Resource\\Sound\\test.wav", Sound::Type::SFX);

		Sound::Load(L"CaliforniaGirls", L"..\\Resource\\Sound\\CaliforniaGirls.wav", Sound::Type::BGM);
		Sound::Load(L"CaramellDansen", L"..\\Resource\\Sound\\CaramellDansen.wav", Sound::Type::BGM);
		Sound::Load(L"menisyuki", L"..\\Resource\\Sound\\MeniShukiRushshu.mp3", Sound::Type::BGM);
		Sound::Load(L"RabbitHole", L"..\\Resource\\Sound\\RabbitHole.mp3", Sound::Type::BGM);
		Sound::Load(L"Specialist", L"..\\Resource\\Sound\\Specialist.mp3", Sound::Type::BGM);

		// ====================================== SoundBox 예제 추가 (테스트용) ======================================
		// - 옛날에는 BGM을 전역으로 PlayBGM/StopBGM으로 제어했지만,
		//   이제는 SoundBox마다 3D 인스턴스를 별도로 만들어서 onEnter/onExit에서
		//   "들어갈 때 0초부터 재생, 나가면 완전히 정지" 하도록 한다.
		//Sound::SetBGMVolume(0.6f);
		//Sound::PlayBGM(L"LetItHappen");

		// 예제 SoundBox 생성 (원점 주변 10x10x10 영역)
		SoundBox box1;
		box1.bgmKey = L"LetItHappen";
		box1.instanceId = L"SB_Origin_LetItHappen"; // ✅ 이 박스만의 고유 인스턴스 ID
		box1.position = { 0.0f, 50.0f, 0.0f };
		box1.scale = { 10.0f, 10.0f, 10.0f };
		box1.boundsMin = { -10.0f, -10.0f, -10.0f };
		box1.boundsMax = { 10.0f, 10.0f, 10.0f };
		// 중심에 가까울수록 커지게 기본 감쇄 설정
		box1.edgeVolume = 0.0f;
		box1.centerVolume = 1.0f;
		box1.curve = 1.0f;
		box1.minDist = 50.0f;

		// 박스 진입 시: 항상 0초부터 재생되도록 Stop 후 Play
		box1.onEnter = [id = box1.instanceId, key = box1.bgmKey]()
		{
			Sound::Stop3DInstance(id);
			Sound::Play3DInstance(id, key, true);
		};
		// 박스 이탈 시: 정지(시간도 멈춤)
		box1.onExit = [id = box1.instanceId]()
		{
			Sound::Stop3DInstance(id);
		};
		m_->m_SoundBoxSystem.AddBox(box1);

	}
	m_fLoadingProgress = 0.1f;

	m_fLoadingProgress = 0.2f;
	m_sLoadingStr = L"IBL을 초기화 합니다.";
	if (stoken.stop_requested()) return;
	// ====================================== IBL 텍스처 로드 (Sample 세트) ======================================
	//  - BakerSampleDiffuseHDR.dds  : Irradiance(난반사) 맵   → Diffuse IBL
	//  - BakerSampleSpecularHDR.dds : Prefiltered Env 맵      → Specular IBL
	//  - BakerSampleBrdf.dds        : BRDF LUT (NdotV,Roughness → A,B)
	ChangeIBLSkyBox(L"..\\Resource\\Skybox\\Sample\\BakerSample");
	m_->m_SkyBoxChoice = App::Impl::SkyBoxChoice::Baker;

	m_fLoadingProgress = 0.3f;
	m_sLoadingStr = L"3D 모델을 로드합니다. 시간이 오래 걸릴 수 있습니다";
	if (stoken.stop_requested()) return;
	// ====================================== 3D 모델 ======================================
	//LoadModelFromFile(L"..\\Resource\\fbx\\Study\\char\\char.fbx"); // 0
	// LoadModelFromFile(L"..\\Resource\\fbx\\Alice_UmaUma.fbx"); // 0

	//LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Alice3DGame\\Alice.fbx"); // 0
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Alice3DGame\\Alice_.fbx"); // 0
	//LoadModelFromFile(L"..\\Resource\\fbx\\Study\\alice_normal_mapping_idle_walk_run.fbx"); // 0
	m_fLoadingProgress = 0.8f;
	if (stoken.stop_requested()) return;
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Alice3DGame\\AmazingWonderland.fbx"); // 1
	m_fLoadingProgress = 0.9f;
	if (stoken.stop_requested()) return;
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\sphere.fbx"); // 2
	// LoadModelFromFile(L"..\\Resource\\fbx\\Neon.fbx"); // 3
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\sphere.fbx"); // 3
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Ground.fbx"); // 4
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Alice3DGame\\Alice_.fbx"); // 5
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Alice3DGame\\Alice_.fbx"); // 6
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Alice3DGame\\Alice_.fbx"); // 7
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Alice3DGame\\Alice_.fbx"); // 8
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Alice3DGame\\Alice_.fbx"); // 9
	LoadModelFromFile(L"..\\Resource\\fbx\\Study\\Alice3DGame\\Alice_.fbx"); // 10

	m_fLoadingProgress = 0.95f;
	if (stoken.stop_requested()) return;
	m_->m_Objects.clear();
	for (int mi = 0; mi < (int)m_->m_Models.size(); ++mi) {
		auto mo = std::make_unique<ModelObject>(m_->m_Models[mi]->modelName, mi);
		m_->m_Objects.push_back(std::move(mo));
	}
	m_->m_Models[0]->boundsBoneIndex = 2;
	m_->m_Models[5]->boundsBoneIndex = 2;
	m_->m_Models[6]->boundsBoneIndex = 2;
	m_->m_Models[7]->boundsBoneIndex = 2;
	m_->m_Models[8]->boundsBoneIndex = 2;
	m_->m_Models[9]->boundsBoneIndex = 2;
	m_->m_Models[10]->boundsBoneIndex = 2;

	m_->m_Models[0]->modelShading = ShadingMode::PBR;
	m_->m_Models[1]->modelShading = ShadingMode::PBR;
	m_->m_Models[2]->modelShading = ShadingMode::PBR;
	m_->m_Models[3]->modelShading = ShadingMode::PBR;
	m_->m_Models[5]->modelShading = ShadingMode::PBR;
	m_->m_Models[6]->modelShading = ShadingMode::PBR;
	m_->m_Models[7]->modelShading = ShadingMode::PBR;
	m_->m_Models[8]->modelShading = ShadingMode::PBR;
	m_->m_Models[9]->modelShading = ShadingMode::PBR;
	m_->m_Models[10]->modelShading = ShadingMode::PBR;

	m_->m_Models[0]->pos = XMFLOAT3(0, 0.0f, 0.0f);
	m_->m_Models[1]->pos = XMFLOAT3(-26, 66.5f, -29.3f);
	m_->m_Models[2]->pos = XMFLOAT3(-220, 90.0f, -80.0f);
	m_->m_Models[3]->pos = XMFLOAT3(170, 90.0f, 70.0f);
	m_->m_Models[4]->pos = XMFLOAT3(0.0f, -2.0f, 0.0f);
	m_->m_Models[5]->pos = XMFLOAT3(-350.0f, 0.0f, 230.0f);
	m_->m_Models[6]->pos = XMFLOAT3(0.0f, 0.0f, 300.0f);
	m_->m_Models[7]->pos = XMFLOAT3(350.0f, 0.0f, 230.0f);
	m_->m_Models[8]->pos = XMFLOAT3(-350.0f, 0.0f, -260.0f);
	m_->m_Models[9]->pos = XMFLOAT3(350.0f, 0.0f, -260.0f);
	m_->m_Models[10]->pos = XMFLOAT3(0.0f, 0.0f, -300.0f);

	m_->m_Models[0]->rotDeg = XMFLOAT3(0, 0.0f, 0.0f);
	m_->m_Models[1]->rotDeg = XMFLOAT3(13, 90.0f, 2.0f);
	m_->m_Models[5]->rotDeg = XMFLOAT3(0.0f, -60.0f, 0.0f);
	m_->m_Models[7]->rotDeg = XMFLOAT3(0.0f, 60.0f, 0.0f);
	m_->m_Models[8]->rotDeg = XMFLOAT3(0.0f, -120.0f, 0.0f);
	m_->m_Models[9]->rotDeg = XMFLOAT3(0.0f, -240.0f, 0.0f);
	m_->m_Models[10]->rotDeg = XMFLOAT3(0.0f, 180.0f, 0.0f);

	m_->m_Models[3]->scale = XMFLOAT3(0.5f, 0.5f, 0.5f);
	m_->m_Models[4]->scale = XMFLOAT3(2.0f, 1.0f, 8.0f);

	m_->m_Models[2]->useInstancePbrMaterial = true;
	m_->m_Models[3]->useInstancePbrMaterial = true;
	m_->m_Models[4]->useInstancePbrMaterial = true;
	m_->m_Models[5]->useInstancePbrMaterial = true;
	m_->m_Models[6]->useInstancePbrMaterial = true;
	m_->m_Models[7]->useInstancePbrMaterial = true;
	m_->m_Models[8]->useInstancePbrMaterial = true;
	m_->m_Models[9]->useInstancePbrMaterial = true;
	m_->m_Models[10]->useInstancePbrMaterial = true;

	m_->m_Models[2]->instancePbrMaterial.metalness = 1.0f;
	m_->m_Models[2]->instancePbrMaterial.roughness = 0.01f;
	m_->m_Models[2]->instancePbrMaterial.ambientOcclusion = 1.0f;

	m_->m_Models[3]->instancePbrMaterial.metalness = 1.0f;
	m_->m_Models[3]->instancePbrMaterial.roughness = 0.01f;
	m_->m_Models[3]->instancePbrMaterial.ambientOcclusion = 1.0f;

	m_->m_Models[4]->instancePbrMaterial.baseColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	m_->m_Models[4]->instancePbrMaterial.metalness = 0.01f;
	m_->m_Models[4]->instancePbrMaterial.roughness = 1.0f;
	m_->m_Models[4]->instancePbrMaterial.ambientOcclusion = 1.0f;



	m_->m_Models[5]->instancePbrMaterial.baseColor = XMFLOAT4(0.96f, 0.96f, 0.96f, 1.0f);
	m_->m_Models[5]->instancePbrMaterial.metalness = 0.1f;
	m_->m_Models[5]->instancePbrMaterial.roughness = 0.43f;
	m_->m_Models[5]->instancePbrMaterial.ambientOcclusion = 1.0f;

	m_->m_Models[6]->instancePbrMaterial.baseColor = XMFLOAT4(0.96f, 0.96f, 0.96f, 1.0f);
	m_->m_Models[6]->instancePbrMaterial.metalness = 0.1f;
	m_->m_Models[6]->instancePbrMaterial.roughness = 0.43f;
	m_->m_Models[6]->instancePbrMaterial.ambientOcclusion = 1.0f;

	m_->m_Models[7]->instancePbrMaterial.baseColor = XMFLOAT4(0.96f, 0.96f, 0.96f, 1.0f);
	m_->m_Models[7]->instancePbrMaterial.metalness = 0.1f;
	m_->m_Models[7]->instancePbrMaterial.roughness = 0.43f;
	m_->m_Models[7]->instancePbrMaterial.ambientOcclusion = 1.0f;

	m_->m_Models[8]->instancePbrMaterial.baseColor = XMFLOAT4(0.96f, 0.96f, 0.96f, 1.0f);
	m_->m_Models[8]->instancePbrMaterial.metalness = 0.1f;
	m_->m_Models[8]->instancePbrMaterial.roughness = 0.43f;
	m_->m_Models[8]->instancePbrMaterial.ambientOcclusion = 1.0f;

	m_->m_Models[9]->instancePbrMaterial.baseColor = XMFLOAT4(0.96f, 0.96f, 0.96f, 1.0f);
	m_->m_Models[9]->instancePbrMaterial.metalness = 0.1f;
	m_->m_Models[9]->instancePbrMaterial.roughness = 0.43f;
	m_->m_Models[9]->instancePbrMaterial.ambientOcclusion = 1.0f;

	m_->m_Models[10]->instancePbrMaterial.baseColor = XMFLOAT4(0.96f, 0.96f, 0.96f, 1.0f);
	m_->m_Models[10]->instancePbrMaterial.metalness = 0.1f;
	m_->m_Models[10]->instancePbrMaterial.roughness = 0.43f;
	m_->m_Models[10]->instancePbrMaterial.ambientOcclusion = 1.0f;

	m_->m_Models[5]->uiAnimPlaying = false;
	m_->m_Models[6]->uiAnimPlaying = false;
	m_->m_Models[7]->uiAnimPlaying = false;
	m_->m_Models[8]->uiAnimPlaying = false;
	m_->m_Models[9]->uiAnimPlaying = false;
	m_->m_Models[10]->uiAnimPlaying = false;
	m_->m_Models[5]->fbxBaseAnimator.SetCurrentIndex(1);    // Idle
	m_->m_Models[6]->fbxBaseAnimator.SetCurrentIndex(1);    // Idle
	m_->m_Models[7]->fbxBaseAnimator.SetCurrentIndex(1);    // Idle
	m_->m_Models[8]->fbxBaseAnimator.SetCurrentIndex(1);    // Idle
	m_->m_Models[9]->fbxBaseAnimator.SetCurrentIndex(1);    // Idle
	m_->m_Models[10]->fbxBaseAnimator.SetCurrentIndex(1);   // Idle

	// ====================================== SoundBox 생성 및 애니메이션 바인딩 ======================================
	// 각 SoundBox에 해당하는 모델의 애니메이션을 설정하는 콜백 함수를 바인딩합니다.
	// 박스에 들어가면 BGM 재생 + 해당 모델의 애니메이션이 자동으로 실행됩니다.

	// Model 5: CaramellDansen
	SoundBox boxCaramellDansen;
	boxCaramellDansen.bgmKey = L"CaramellDansen";
	boxCaramellDansen.instanceId = L"SB_Caramell"; // ✅ 박스별 고유 인스턴스 ID
	boxCaramellDansen.position = m_->m_Models[5]->pos;
	boxCaramellDansen.minDist = 50.0f;
	boxCaramellDansen.scale = { 15.0f, 15.0f, 15.0f };
	boxCaramellDansen.boundsMin = { -10.0f, -10.0f, -10.0f };
	boxCaramellDansen.boundsMax = { 10.0f, 10.0f, 10.0f };
	boxCaramellDansen.onEnter = [this,
		id = boxCaramellDansen.instanceId,
		key = boxCaramellDansen.bgmKey]()
	{
		// ✅ 항상 0초부터 재생되도록, 들어올 때마다 Stop → Play 순서로 호출
		Sound::Stop3DInstance(id);
		Sound::Play3DInstance(id, key, true);

		m_->m_Models[5]->uiAnimPlaying = true;
		m_->m_Models[5]->fbxBaseAnimator.SetCurrentIndex(1);  // CaramellaDansen
	};
	boxCaramellDansen.onExit = [this, id = boxCaramellDansen.instanceId]()
	{
		// ✅ 박스에서 나가면 정지해서 시간이 더 이상 흐르지 않게 한다.
		Sound::Stop3DInstance(id);

		m_->m_Models[5]->uiAnimPlaying = false;
	};
	m_->m_SoundBoxSystem.AddBox(boxCaramellDansen);

	// Model 6: menisyukiLeft
	SoundBox boxMenisyukiLeft;
	boxMenisyukiLeft.bgmKey = L"menisyuki";
	boxMenisyukiLeft.instanceId = L"SB_Menisyuki_Left";
	boxMenisyukiLeft.position = m_->m_Models[6]->pos;
	boxMenisyukiLeft.minDist = 50.0f;
	boxMenisyukiLeft.scale = { 15.0f, 15.0f, 15.0f };
	boxMenisyukiLeft.boundsMin = { -10.0f, -10.0f, -10.0f };
	boxMenisyukiLeft.boundsMax = { 10.0f, 10.0f, 10.0f };
	boxMenisyukiLeft.onEnter = [this,
		id = boxMenisyukiLeft.instanceId,
		key = boxMenisyukiLeft.bgmKey]()
	{
		Sound::Stop3DInstance(id);
		Sound::Play3DInstance(id, key, true);

		m_->m_Models[6]->uiAnimPlaying = true;
		m_->m_Models[6]->fbxBaseAnimator.SetCurrentIndex(3);  // menisyuikiLeft
	};
	boxMenisyukiLeft.onExit = [this, id = boxMenisyukiLeft.instanceId]()
	{
		Sound::Stop3DInstance(id);

		m_->m_Models[6]->uiAnimPlaying = false;
	};
	m_->m_SoundBoxSystem.AddBox(boxMenisyukiLeft);

	// Model 7: menisyukiRight
	SoundBox boxMenisyukiRight;
	boxMenisyukiRight.bgmKey = L"menisyuki";
	boxMenisyukiRight.instanceId = L"SB_Menisyuki_Right";
	boxMenisyukiRight.position = m_->m_Models[7]->pos;
	boxMenisyukiRight.minDist = 50.0f;
	boxMenisyukiRight.scale = { 15.0f, 15.0f, 15.0f };
	boxMenisyukiRight.boundsMin = { -10.0f, -10.0f, -10.0f };
	boxMenisyukiRight.boundsMax = { 10.0f, 10.0f, 10.0f };
	boxMenisyukiRight.onEnter = [this,
		id = boxMenisyukiRight.instanceId,
		key = boxMenisyukiRight.bgmKey]()
	{
		Sound::Stop3DInstance(id);
		Sound::Play3DInstance(id, key, true);

		m_->m_Models[7]->uiAnimPlaying = true;
		m_->m_Models[7]->fbxBaseAnimator.SetCurrentIndex(4);  // menisyukiRight
	};
	boxMenisyukiRight.onExit = [this, id = boxMenisyukiRight.instanceId]()
	{
		Sound::Stop3DInstance(id);

		m_->m_Models[7]->uiAnimPlaying = false;
	};
	m_->m_SoundBoxSystem.AddBox(boxMenisyukiRight);

	// Model 8: RabbitHole
	SoundBox boxRabbitHole;
	boxRabbitHole.bgmKey = L"RabbitHole";
	boxRabbitHole.instanceId = L"SB_RabbitHole";
	boxRabbitHole.position = m_->m_Models[8]->pos;
	boxRabbitHole.minDist = 50.0f;
	boxRabbitHole.scale = { 15.0f, 15.0f, 15.0f };
	boxRabbitHole.boundsMin = { -10.0f, -10.0f, -10.0f };
	boxRabbitHole.boundsMax = { 10.0f, 10.0f, 10.0f };
	boxRabbitHole.onEnter = [this,
		id = boxRabbitHole.instanceId,
		key = boxRabbitHole.bgmKey]()
	{
		Sound::Stop3DInstance(id);
		Sound::Play3DInstance(id, key, true);

		m_->m_Models[8]->uiAnimPlaying = true;
		m_->m_Models[8]->fbxBaseAnimator.SetCurrentIndex(5);  // RabbitHole
	};
	boxRabbitHole.onExit = [this, id = boxRabbitHole.instanceId]()
	{
		Sound::Stop3DInstance(id);

		m_->m_Models[8]->uiAnimPlaying = false;
	};
	m_->m_SoundBoxSystem.AddBox(boxRabbitHole);

	// Model 9: Specialist
	SoundBox boxSpecialist;
	boxSpecialist.bgmKey = L"Specialist";
	boxSpecialist.instanceId = L"SB_Specialist";
	boxSpecialist.position = m_->m_Models[9]->pos;
	boxSpecialist.minDist = 50.0f;
	boxSpecialist.scale = { 15.0f, 15.0f, 15.0f };
	boxSpecialist.boundsMin = { -10.0f, -10.0f, -10.0f };
	boxSpecialist.boundsMax = { 10.0f, 10.0f, 10.0f };
	boxSpecialist.onEnter = [this,
		id = boxSpecialist.instanceId,
		key = boxSpecialist.bgmKey]()
	{
		Sound::Stop3DInstance(id);
		Sound::Play3DInstance(id, key, true);

		m_->m_Models[9]->uiAnimPlaying = true;
		m_->m_Models[9]->fbxBaseAnimator.SetCurrentIndex(10);  // Specialist
	};
	boxSpecialist.onExit = [this, id = boxSpecialist.instanceId]()
	{
		Sound::Stop3DInstance(id);

		m_->m_Models[9]->uiAnimPlaying = false;
	};
	m_->m_SoundBoxSystem.AddBox(boxSpecialist);

	// Model 10: CaliforniaGirls
	SoundBox boxCaliforniaGirls;
	boxCaliforniaGirls.bgmKey = L"CaliforniaGirls";
	boxCaliforniaGirls.instanceId = L"SB_CaliforniaGirls";
	boxCaliforniaGirls.position = m_->m_Models[10]->pos;
	boxCaliforniaGirls.minDist = 50.0f;
	boxCaliforniaGirls.scale = { 15.0f, 15.0f, 15.0f };
	boxCaliforniaGirls.boundsMin = { -10.0f, -10.0f, -10.0f };
	boxCaliforniaGirls.boundsMax = { 10.0f, 10.0f, 10.0f };
	boxCaliforniaGirls.onEnter = [this,
		id = boxCaliforniaGirls.instanceId,
		key = boxCaliforniaGirls.bgmKey]()
	{
		Sound::Stop3DInstance(id);
		Sound::Play3DInstance(id, key, true);

		m_->m_Models[10]->uiAnimPlaying = true;
		m_->m_Models[10]->fbxBaseAnimator.SetCurrentIndex(0);  // CaliforniaGirls
	};
	boxCaliforniaGirls.onExit = [this, id = boxCaliforniaGirls.instanceId]()
	{
		Sound::Stop3DInstance(id);

		m_->m_Models[10]->uiAnimPlaying = false;
	};
	m_->m_SoundBoxSystem.AddBox(boxCaliforniaGirls);


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
				//m_->m_CharCtrl.config.weaponSocket.parentBone = "Hand_R";
				m_->m_CharCtrl.config.weaponSocket.parentBone = "手首.R";
				m_->m_CharCtrl.config.weaponSocket.pos = { 0.0f, 0.0f, 0.0f };
				m_->m_CharCtrl.config.weaponSocket.rotDeg = { 0.0f, 0.0f, 0.0f };
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
	m_fLoadingProgress = 1.0f;
	// 스레드를 종료시킴 
	m_bIsLoaded = true;
}

void App::OnUninitialize() {

	// 1. 스레드에 멈춤 신호 보내기
	m_loaderThread.request_stop();

	// 2. 스레드가 하던 작업(예: 텍스처 로딩 함수)이 리턴할 때까지 대기
	// 이걸 안 하면 B스레드가 살아있는데 아래에서 Device를 Release 해버려서 터짐
	if (m_loaderThread.joinable())
	{
		m_loaderThread.join();
	}
	// 3. 안전하게 리소스 해제 (이제 B 스레드는 완전히 죽었으므로 안전함)
	if (m_->m_pDeviceContext) m_->m_pDeviceContext->ClearState();

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
		ImFontConfig cfg{};
		cfg.PixelSnapH = true;
		cfg.OversampleH = 2;
		cfg.OversampleV = 2;
		const ImWchar* rangeKR = io.Fonts->GetGlyphRangesKorean();
		const ImWchar* rangeENG = io.Fonts->GetGlyphRangesDefault();
		const ImWchar* rangeJP = io.Fonts->GetGlyphRangesJapanese();
		// 한글: NotoSansKR-Regular
		cfg.MergeMode = false;
		io.Fonts->AddFontFromFileTTF("..\\Resource\\Font\\NotoSansKR-Regular.ttf", 20.0f, &cfg, rangeKR);
		// 일본어: Meiryo
		io.Fonts->AddFontFromFileTTF("..\\Resource\\Font\\meiryo.ttc", 22.0f, &cfg, rangeJP);
		// 영어
		cfg.MergeMode = true;
		io.Fonts->AddFontFromFileTTF("..\\Resource\\Font\\NotoSansKR-Regular.ttf", 20.0f, &cfg, rangeENG);
		m_pFontLarge = io.Fonts->AddFontFromFileTTF("..\\Resource\\Font\\NotoSansKR-Regular.ttf", 30.0f, NULL, rangeKR);
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
		HR_T(m_->m_pDevice->CreateInputLayout(gbufferLayout, ARRAYSIZE(gbufferLayout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_->m_pGBufferInputLayout));
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
