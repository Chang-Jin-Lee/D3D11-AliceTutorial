#pragma once

// °£´ÜÇÑ Scene

#include <memory>
#include <vector>
#include <string>
#include <random>
#include <imgui.h>

class Scene
{
public:
	virtual ~Scene() = default;

	virtual void OnEnter() {}
	virtual void OnExit() {}

	virtual void Update(float dt) {}
	virtual void RenderUI() {}
};