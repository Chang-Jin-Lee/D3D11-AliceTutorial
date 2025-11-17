#pragma once
#include "../Common/GameApp.h"
#include <memory>
#include <string>
#include "../Common/Scene.h"

class App :
	public GameApp
{
public:
	App();
	~App() override;

	// 수명주기
	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate(const float& dt) override;
	void OnRender() override;

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
	void RenderWidgetUI();

	// 로더 API
	bool LoadModelFromFile(const std::wstring& pathW);
	void UnloadModel();

private:
	// 내부 헬퍼
	bool InitBasicEffect();
	bool InitSkyBoxEffect();
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
};

