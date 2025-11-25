#pragma once
#include "DirectXMath.h"

using namespace DirectX;
class Camera;

class PickingRay
{
public:
    XMFLOAT3 origin;     // 레이 시작점 (월드 공간)
    XMFLOAT3 direction;  // 정규화된 방향 (월드 공간)

    PickingRay();
    PickingRay(const XMFLOAT3& originPos, const XMFLOAT3& rayDir);

    // 화면 좌표와 카메라로부터 월드 공간 레이를 만들어 반환함.
    static PickingRay ScreenPointToRay(const Camera& camera, float x, float y, float width, float height);

    // 레이가 구 바운딩과 교차하는지 검사합니다.
    bool HitSphere(const XMFLOAT3& center, float radius, float& outT) const;

    // 레이랑 AABB가 겹쳤는지 검사합니다.
    bool HitAABB(const XMFLOAT3& boxMin, const XMFLOAT3& boxMax, float& outT) const;
};

