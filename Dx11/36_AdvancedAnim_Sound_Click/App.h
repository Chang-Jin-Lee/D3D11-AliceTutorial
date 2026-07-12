#pragma once
#include "../Common/GameApp.h"
#include <memory>
#include <string>
#include "../Common/Scene.h"
#include <dxgiformat.h>
#include <thread>

class ID3D11Buffer;

class App : public GameApp
{
public:
	App();
	~App() override;

	// 수명주기
	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate(const float& dt) override;
	void OnRender() override;
	void OnInputProcess(const Keyboard::State& KeyState,
	                    const Keyboard::KeyboardStateTracker& KeyTracker,
	                    const Mouse::State& MouseState,
	                    const Mouse::ButtonStateTracker& MouseTracker) override;

	template<typename T>
	void UpdateCB(ID3D11Buffer* buffer, const T& data);
	void PassClear();
	void PassDebugDraw();
	void PassShadow();
	void PassMainScene(); // 큐브, 모델, 스카이박스 포함
	void PassPostProcess();
	void PassUI();

	// D3D 장치 생성/파괴
	bool InitD3D();
	void UninitD3D();

	// 리소스 생성/파괴
	bool InitScene();
	void UninitScene();

	// 텍스쳐 관련
	bool InitTexture();

	// ImGui 관련
	bool InitImGui();
	void RenderControlPannel();
	void RenderModelPannel();
	void RenderSceneCollection();
	void RenderConsolPannel();
	void RenderSceneImageWindow();
	void RenderGBufferDebug();  // G-Buffer 디버그 뷰
	void RenderDeferredUI();    // Deferred Rendering UI
	void RenderAdvancedRigUI(); // Advanced Animation(Socket/Blend/Layer/IK) UI
	void RenderSoundDebugUI();  // Sound Debug UI (3D 사운드, Pan 테스트)
	void RenderQuickGuideUI();  // Short player/camera control guide

	// 로더 API
	bool LoadModelFromFile(const std::wstring& pathW);
	void UnloadModel();

private:
	// 내부 헬퍼
	bool InitBasicEffect();
	bool InitSkyBoxEffect();
	bool CreateQuad();
	void PrepareSkyFaceSRVs();
	void ChangeSkyboxDDS(const wchar_t* ddsPath);
	void InitializeEnemyIdleRuntime(int modelIndex);
	void UpdateEnemyIdleAnimations(float dt);
	void StartPublicDemoAudioOnce();

private:
	struct Impl;
	std::unique_ptr<Impl> m_;

	// @brief : 장면 전환 및 관리
	std::unique_ptr<Scene> m_CurrentScene;
	int m_SceneIndex = 0; // 0: A, 1: B
	void ChangeScene(std::unique_ptr<Scene> next);

	// @brief : VRAM/자원 Trim 호출
	void TrimVideoMemory();

	bool CheckHDRSupportAndGetMaxNits(float& outMaxNits, DXGI_FORMAT& outFormat);
	void CreateSwapChainAndBackBuffer(DXGI_FORMAT format);

	// @brief : 씬 이미지 로드
	void LoadSceneImage(const std::wstring& path);
	void ChangeIBLSkyBox(const std::wstring& path);
private:
    void RenderForward();
    void RenderDeferred();
    void RenderScene(bool isDeferred);
    void RenderShadowMap();
    void RenderSkyBox();
    void RenderToneMapping();
    bool CreateGBuffer();
    void PassGBuffer();        // G-Buffer 패스 (지오메트리 정보를 G-Buffer에 렌더링)
    void PassDeferredLight();  // 디퍼드 라이트 패스 (G-Buffer를 읽어서 조명 계산)

private:

	// @brief : 멀티스레드로 로딩 화면 보여주기
	// 스레드 객체가 파괴될 때 자동으로 실행 종료를 대기함
	std::jthread m_loaderThread;
	// 락(Lock) 없이 스레드 간 안전하게 bool 값 공유
	std::atomic<const wchar_t*> m_sLoadingStr{L""};
	std::atomic<float> m_fLoadingProgress{ 0.0f };
	std::atomic<bool> m_bIsLoaded{ false };
	bool m_bIsGameStarted = false;
	// 별도 스레드에서 로딩함 fbx등 엄청 오래걸리니까 여기서 함
	void LoadDataAsync(std::stop_token stoken); // 토큰으로 메인 스레드가 중지 됐는지 확인함.
	void RenderWaitingUI(); // 별도 스레드에서 데이터 로드하는 동안 보여줄 UI
	ImFont* m_pFontLarge = nullptr; // 큰 글씨용 폰트

};


