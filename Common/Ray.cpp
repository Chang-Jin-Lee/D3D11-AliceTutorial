#include "pch.h"
#include "Ray.h"
#include "Camera.h"

using namespace DirectX;

PickingRay::PickingRay() : origin(0.0f, 0.0f, 0.0f), direction(0.0f, 0.0f, 1.0f) {}

PickingRay::PickingRay(const XMFLOAT3& originPos, const XMFLOAT3& rayDir) : origin(originPos)
{
	XMVECTOR d = XMLoadFloat3(&rayDir);
	d = XMVector3Normalize(d);
	XMStoreFloat3(&direction, d);
}

PickingRay PickingRay::ScreenPointToRay(const Camera& camera, float x, float y, float width, float height)
{
	// 스크린 좌표 → NDC(-1~1)
	float nx = 2.0f * x / width - 1.0f;
	float ny = 1.0f - 2.0f * y / height;

	XMMATRIX view = camera.GetViewMatrixXM();
	XMMATRIX proj = camera.GetProjMatrixXM();
	XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);

	XMVECTOR nearPoint = XMVectorSet(nx, ny, 0.0f, 1.0f);
	XMVECTOR farPoint  = XMVectorSet(nx, ny, 1.0f, 1.0f);

	nearPoint = XMVector3TransformCoord(nearPoint, invViewProj);
	farPoint  = XMVector3TransformCoord(farPoint, invViewProj);

	XMVECTOR dir = XMVector3Normalize(farPoint - nearPoint);

	XMFLOAT3 o, d;
	XMStoreFloat3(&o, nearPoint);
	XMStoreFloat3(&d, dir);
	return PickingRay(o, d);
}

bool PickingRay::HitSphere(const XMFLOAT3& center, float radius, float& outT) const
{
	XMVECTOR o = XMLoadFloat3(&origin);
	XMVECTOR d = XMLoadFloat3(&direction);
	XMVECTOR c = XMLoadFloat3(&center);
	XMVECTOR oc = XMVectorSubtract(o, c);

	float a = XMVectorGetX(XMVector3Dot(d, d));
	float b = 2.0f * XMVectorGetX(XMVector3Dot(oc, d));
	float cc = XMVectorGetX(XMVector3Dot(oc, oc)) - radius * radius;
	float disc = b * b - 4.0f * a * cc;
	if (disc < 0.0f) return false;

	float s = sqrtf(disc);
	float t0 = (-b - s) / (2.0f * a);
	float t1 = (-b + s) / (2.0f * a);
	float t = (t0 > 0.0f) ? t0 : t1;
	if (t < 0.0f) return false;
	outT = t;
	return true;
}

bool PickingRay::HitAABB(const XMFLOAT3& boxMin, const XMFLOAT3& boxMax, float& outT) const
{
	// AABB 교차 테스트
	const float ox = origin.x, oy = origin.y, oz = origin.z;
	const float dx = direction.x, dy = direction.y, dz = direction.z;

	float tMin = 0.0f;
	float tMax = FLT_MAX;

	auto updateAxis = [&](float o, float d, float minA, float maxA) -> bool {
		if (fabsf(d) < 1e-6f)
		{
			// 레이가 축과 평행인데, 원점이 박스 범위 밖이면 교차 없음
			return (o >= minA && o <= maxA);
		}
		float invD = 1.0f / d;
		float t0 = (minA - o) * invD;
		float t1 = (maxA - o) * invD;
		if (t0 > t1) { float tmp = t0; t0 = t1; t1 = tmp; }
		if (t0 > tMin) tMin = t0;
		if (t1 < tMax) tMax = t1;
		return tMax >= tMin;
	};

	if (!updateAxis(ox, dx, boxMin.x, boxMax.x)) return false;
	if (!updateAxis(oy, dy, boxMin.y, boxMax.y)) return false;
	if (!updateAxis(oz, dz, boxMin.z, boxMax.z)) return false;

	if (tMax < 0.0f) return false;
	outT = (tMin > 0.0f) ? tMin : tMax;
	return true;
}
