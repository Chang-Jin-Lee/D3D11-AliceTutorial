#include "pch.h"
#include "InputSystem.h"

constexpr float ROTATION_GAIN = 0.004f;
constexpr float MOVEMENT_GAIN = 0.07f;

using namespace DirectX;
using namespace DirectX::SimpleMath;

InputSystem::InputSystem()
	:m_MouseState(), m_KeyboardState()
{
	assert(Instance == nullptr);
	Instance = this;
}

InputSystem* InputSystem::Instance = nullptr;

void InputSystem::Update(float DeltaTime)
{
	m_MouseState = m_Mouse->GetState();
	m_MouseStateTracker.Update(m_MouseState);

	m_KeyboardState = m_Keyboard->GetState();
	m_KeyboardStateTracker.Update(m_KeyboardState);


	if(m_pInputProcessers!=nullptr)
	{
		m_pInputProcessers->OnInputProcess(m_KeyboardState, m_KeyboardStateTracker, m_MouseState, m_MouseStateTracker);
	}
}

bool InputSystem::Initialize(HWND hWnd, InputProcesser* processer)
{
	m_Keyboard = std::make_unique<Keyboard>();
	m_Mouse = std::make_unique<Mouse>();
	m_Mouse->SetWindow(hWnd);
	m_pInputProcessers = processer;
	return true;
}
