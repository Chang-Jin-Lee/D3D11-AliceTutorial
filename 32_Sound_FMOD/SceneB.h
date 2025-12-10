#pragma once
#include "../Common/Scene.h"

// 데모 씬 B. 텍스트만
class SceneB final : public Scene
{
public:
	void OnEnter() override {}
	void Update(float dt) override { (void)dt; }
	void RenderUI() override
	{
		ImGui::SetNextWindowSize(ImVec2(340, 120), ImGuiCond_Once);
		if (ImGui::Begin("Scene B"))
		{
			ImGui::TextUnformatted("This is Scene B");
			ImGui::TextUnformatted("Press F6 to switch scenes");
		}
		ImGui::End();
	}
};

