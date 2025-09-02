#include "pch.h"
#include "Camera.h"


Camera::Camera()
{
	Reset();
}

Vector3 Camera::GetForward()
{
	return -m_World.Forward();
}

Vector3 Camera::GetRight()
{
	return m_World.Right();
}

void Camera::Reset()
{
	m_World = Matrix::Identity;
	m_Rotation = Vector3(0.0f, 0.0f, 0.0f);
	m_Position = Vector3(0.0f, 0.0f, -30.0f);
}

void Camera::Update(float elapsedTime)
{
	if (m_InputVector.Length() > 0.0f)
	{
		m_Position += m_InputVector * m_MoveSpeed * elapsedTime;
		m_InputVector = Vector3(0.0f, 0.0f, 0.0f);
	}

	m_World = Matrix::CreateFromYawPitchRoll(m_Rotation)*
		Matrix::CreateTranslation(m_Position);
}

void Camera::GetViewMatrix(Matrix& out)
{
	Vector3 eye = m_World.Translation();
	Vector3 target = m_World.Translation() + GetForward();
	Vector3 up = m_World.Up();
	out = XMMatrixLookAtLH(eye, target, up);
}

void Camera::AddInputVector(const Math::Vector3& input)
{
	m_InputVector += input;
	m_InputVector.Normalize();
}

void Camera::AddPitch(float value)
{
	m_Rotation.x += value;
	if (m_Rotation.x > XM_PI)
	{
		m_Rotation.x -= XM_2PI;
	}
	else if (m_Rotation.x < -XM_PI)
	{
		m_Rotation.x += XM_2PI;
	}
}

void Camera::AddYaw(float value)
{
	m_Rotation.y += value;
	if (m_Rotation.y > XM_PI)
	{
		m_Rotation.y -= XM_2PI;
	}
	else if (m_Rotation.y < -XM_PI)
	{
		m_Rotation.y += XM_2PI;
	}
}


void Camera::OnInputProcess(const Keyboard::State& KeyState, const Keyboard::KeyboardStateTracker& KeyTracker, const Mouse::State& MouseState, const Mouse::ButtonStateTracker& MouseTracker)
{
	Vector3 forward = GetForward();
	Vector3 right = GetRight();

	if (KeyTracker.IsKeyPressed(Keyboard::Keys::R))
	{
		Reset();
	}

	if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::W))
	{
		AddInputVector(forward);
	}
	else if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::S))
	{
		AddInputVector(-forward);
	}

	if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::A))
	{
		AddInputVector(-right);
	}
	else if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::D))
	{
		AddInputVector(right);
	}

	if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::E))
	{
		
		AddInputVector(-m_World.Up());
	}
	else if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::Q))
	{
		AddInputVector(m_World.Up());
	}

	if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::F1))
	{
		SetSpeed(200);
	}
	else if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::F2))
	{
		SetSpeed(5000);
	}
	else if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::F3))
	{
		SetSpeed(10000);
	}


	InputSystem::Instance->m_Mouse->SetMode(MouseState.rightButton ? Mouse::MODE_RELATIVE : Mouse::MODE_ABSOLUTE);
	if (MouseState.positionMode == Mouse::MODE_RELATIVE)
	{
		Vector3 delta = Vector3(float(MouseState.x), float(MouseState.y), 0.f) * m_RotationSpeed;
		AddPitch(delta.y);
		AddYaw(delta.x);
	}
}

