#pragma once
#include "../Common/GameApp.h"
#include <memory>
#include <string>
#include "../Common/Scene.h"
#include <dxgiformat.h>

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

	template<typename T>
	void UpdateCB(ID3D11Buffer* buffer, const T& data);
	void PassClear();
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

};


