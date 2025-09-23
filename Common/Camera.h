#pragma once

#include <DirectXMath.h>
#include "InputSystem.h"

using namespace DirectX;

class Camera : public InputProcesser
{
public:
	Camera();

	// Euler rotation in radians: (pitch=x, yaw=y, roll=z)
	XMFLOAT3 m_Rotation{0,0,0}; 						// z(Roll) is used for roll
	XMFLOAT3 m_Position{0,0,-10};
	XMFLOAT4X4 m_World; 						// Row-major world matrix storage
	XMFLOAT3 m_InputVector; 					// Accumulated move input (normalized per frame)

	float m_MoveSpeed = 20.0f;
	float m_RotationSpeed = 0.004f; 			// radians per mouse delta unit

	// Frustum
	void SetFrustum(float fovYRad, float aspect, float nearZ, float farZ);
	float GetFovYRad() const { return m_FovYRad; }
	float GetAspect() const { return m_Aspect; }
	float GetNearZ() const { return m_NearZ; }
	float GetFarZ() const { return m_FarZ; }
	XMMATRIX GetProjMatrixXM() const;

	// Direction getters
	XMVECTOR GetForwardXM() const;
	XMVECTOR GetRightXM() const;
	XMFLOAT3 GetForward() const;
	XMFLOAT3 GetRight() const;

	void Reset();
	void Update(float elapsedTime);
	XMMATRIX GetViewMatrixXM() const;
	XMFLOAT3 GetPosition() const;
	void SetPosition(const XMFLOAT3& pos) { m_Position = pos; }

	void AddInputVector(const XMFLOAT3& input);
	void SetSpeed(float speed) { m_MoveSpeed = speed; }
	void AddPitch(float valueRad);
	void AddYaw(float valueRad);

	void OnInputProcess(const Keyboard::State& KeyState, const Keyboard::KeyboardStateTracker& KeyTracker,
		const Mouse::State& MouseState, const Mouse::ButtonStateTracker& MouseTracker) override;

private:
	float m_FovYRad = 90.0f;
	float m_Aspect = 0.0f;
	float m_NearZ = 1.0f;
	float m_FarZ = 1000.0f;
};

