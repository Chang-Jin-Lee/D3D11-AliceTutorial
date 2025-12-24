#pragma once

#include "../Common/Scene.h"

// 데모 씬 A. 인스턴스 추가/삭제를 위한 캔버스 제공
class SceneA final : public Scene
{
public:
	void OnEnter() override { m_Title = "Scene A - Instances"; }

	void Update(float dt) override
	{
		// 입력은 RenderUI에서 윈도우 포커스 조건과 함께 처리
		(void)dt;
	}

	void RenderUI() override
	{
		ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_Once);
		if (ImGui::Begin(m_Title.c_str()))
		{
			ImGui::TextUnformatted("Insert: Add, Delete: Remove");
			ImGui::Separator();

			// 캔버스
			ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y - 4.0f);
			ImVec2 p0 = ImGui::GetCursorScreenPos();
			ImVec2 p1 = ImVec2(p0.x + canvasSize.x, p0.y + canvasSize.y);
			ImDrawList* dl = ImGui::GetWindowDrawList();
			dl->AddRectFilled(p0, p1, IM_COL32(20, 20, 20, 255), 6.0f);
			dl->AddRect(p0, p1, IM_COL32(64, 64, 64, 255), 6.0f, 0, 2.0f);

			// 포커스된 상태에서만 키 입력을 처리
			const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
			if (focused)
			{
				// 추가: Insert
				if (ImGui::IsKeyPressed(ImGuiKey_Insert, false))
				{
					EnsureRng();
					std::uniform_real_distribution<float> distX(p0.x + 6.0f, p1.x - 14.0f);
					std::uniform_real_distribution<float> distY(p0.y + 6.0f, p1.y - 14.0f);
					m_Positions.emplace_back(distX(m_Rng), distY(m_Rng));
				}
				// 삭제: Delete
				if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
				{
					if (!m_Positions.empty()) m_Positions.pop_back();
				}
			}

			// 인스턴스 그리기
			for (const ImVec2& pos : m_Positions)
			{
				ImVec2 q0 = ImVec2(pos.x - 6, pos.y - 6);
				ImVec2 q1 = ImVec2(pos.x + 6, pos.y + 6);
				dl->AddRectFilled(q0, q1, IM_COL32(120, 200, 255, 255), 2.0f);
				dl->AddRect(q0, q1, IM_COL32(0, 100, 180, 255), 2.0f);
			}

			// 캔버스 영역을 실제로 차지하도록 보조 InvisibleButton
			ImGui::InvisibleButton("instances_canvas", canvasSize);

			ImGui::Separator();
			ImGui::Text("Instances : %d", (int)m_Positions.size());
		}
		ImGui::End();
	}

private:
	void EnsureRng()
	{
		if (!m_RngInit)
		{
			std::random_device rd;
			m_Rng.seed(rd());
			m_RngInit = true;
		}
	}

	std::string m_Title;
	std::vector<ImVec2> m_Positions;
	std::mt19937 m_Rng{};
	bool m_RngInit = false;
};
