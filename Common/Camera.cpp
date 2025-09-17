#include "pch.h"
#include "Camera.h"

using namespace DirectX;

static inline XMMATRIX LoadRowMajor(const XMFLOAT4X4& m)
{
	// XMMATRIX is row-major in memory when loaded via XMLoadFloat4x4
	return XMLoadFloat4x4(&m);
}

static inline void StoreRowMajor(XMFLOAT4X4& dst, FXMMATRIX M)
{
	XMStoreFloat4x4(&dst, M);
}

Camera::Camera()
{
	Reset();
	SetFrustum(XMConvertToRadians(90.0f), 16.0f / 9.0f, 1.0f, 1000.0f);
}

DirectX::XMFLOAT3 Camera::GetPosition() const
{
	return m_Position;
}

XMVECTOR Camera::GetForwardXM() const
{
	// World forward: +Z axis in LH. For identity world, returns (0,0,1).
	XMMATRIX W = LoadRowMajor(m_World);
	XMVECTOR forward = XMVector3Normalize(W.r[2]);
	return forward;
}

XMVECTOR Camera::GetRightXM() const
{
	XMMATRIX W = LoadRowMajor(m_World);
	XMVECTOR right = XMVector3Normalize(W.r[0]);
	return right;
}

XMFLOAT3 Camera::GetForward() const
{
	XMFLOAT3 f{};
	XMStoreFloat3(&f, GetForwardXM());
	return f;
}

XMFLOAT3 Camera::GetRight() const
{
	XMFLOAT3 r{};
	XMStoreFloat3(&r, GetRightXM());
	return r;
}

void Camera::Reset()
{
	StoreRowMajor(m_World, XMMatrixIdentity());
	m_Rotation = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_Position = XMFLOAT3(0.0f, 0.0f, -30.0f);
	m_InputVector = XMFLOAT3(0.0f, 0.0f, 0.0f);
}

void Camera::Update(float elapsedTime)
{
	// Apply movement
	XMVECTOR input = XMLoadFloat3(&m_InputVector);
	if (!XMVector3Equal(input, XMVectorZero()))
	{
		XMVECTOR pos = XMLoadFloat3(&m_Position);
		pos = XMVectorMultiplyAdd(input, XMVectorReplicate(m_MoveSpeed * elapsedTime), pos);
		XMStoreFloat3(&m_Position, pos);
		m_InputVector = XMFLOAT3(0.0f, 0.0f, 0.0f);
	}

	// World = R(yaw,pitch,roll) * T
	XMMATRIX R = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	XMMATRIX T = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	StoreRowMajor(m_World, R * T);
}

XMMATRIX Camera::GetViewMatrixXM() const
{
	// eye from world translation, look = eye + forward, up = row1
	XMMATRIX W = LoadRowMajor(m_World);
	XMVECTOR eye = W.r[3];
	XMVECTOR forward = GetForwardXM();
	XMVECTOR target = XMVectorAdd(eye, forward);
	XMVECTOR up = XMVector3Normalize(W.r[1]);
	return XMMatrixLookAtLH(eye, target, up);
}

void Camera::AddInputVector(const XMFLOAT3& input)
{
	XMVECTOR acc = XMLoadFloat3(&m_InputVector);
	XMVECTOR v = XMLoadFloat3(&input);
	acc = XMVectorAdd(acc, v);
	// normalize if not zero
	if (!XMVector3Equal(acc, XMVectorZero()))
	{
		acc = XMVector3Normalize(acc);
	}
	XMStoreFloat3(&m_InputVector, acc);
}

void Camera::AddPitch(float valueRad)
{
	m_Rotation.x += valueRad;
	if (m_Rotation.x > XM_PI)
		m_Rotation.x -= XM_2PI;
	else if (m_Rotation.x < -XM_PI)
		m_Rotation.x += XM_2PI;
}

void Camera::AddYaw(float valueRad)
{
	m_Rotation.y += valueRad;
	if (m_Rotation.y > XM_PI)
		m_Rotation.y -= XM_2PI;
	else if (m_Rotation.y < -XM_PI)
		m_Rotation.y += XM_2PI;
}

void Camera::OnInputProcess(const Keyboard::State& KeyState, const Keyboard::KeyboardStateTracker& KeyTracker, const Mouse::State& MouseState, const Mouse::ButtonStateTracker& MouseTracker)
{
	XMFLOAT3 fwd = GetForward();
	XMFLOAT3 right = GetRight();

	if (KeyTracker.IsKeyPressed(Keyboard::Keys::R))
	{
		Reset();
	}

	if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::W))
	{
		AddInputVector(fwd);
	}
	else if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::S))
	{
		XMFLOAT3 nf{-fwd.x, -fwd.y, -fwd.z};
		AddInputVector(nf);
	}

	if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::A))
	{
		XMFLOAT3 nr{-right.x, -right.y, -right.z};
		AddInputVector(nr);
	}
	else if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::D))
	{
		AddInputVector(right);
	}

	if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::E))
	{
		XMMATRIX W = LoadRowMajor(m_World);
		XMFLOAT3 down; XMStoreFloat3(&down, XMVectorNegate(W.r[1]));
		AddInputVector(down);
	}
	else if (KeyState.IsKeyDown(DirectX::Keyboard::Keys::Q))
	{
		XMMATRIX W = LoadRowMajor(m_World);
		XMFLOAT3 up; XMStoreFloat3(&up, W.r[1]);
		AddInputVector(up);
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
		XMFLOAT3 delta{ float(MouseState.x), float(MouseState.y), 0.f };
		XMFLOAT3 rad{ delta.x * m_RotationSpeed, delta.y * m_RotationSpeed, 0.f };
		AddPitch(-rad.y); // invert Y like FPS cams
		AddYaw(rad.x);
	}
}

void Camera::SetFrustum(float fovYRad, float aspect, float nearZ, float farZ)
{
	m_FovYRad = fovYRad;
	m_Aspect = aspect;
	m_NearZ = nearZ;
	m_FarZ = farZ;
}

XMMATRIX Camera::GetProjMatrixXM() const
{
	return XMMatrixPerspectiveFovLH(m_FovYRad, m_Aspect, m_NearZ, m_FarZ);
}

