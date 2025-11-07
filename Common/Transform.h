#pragma once
#include <directxtk/SimpleMath.h>
using namespace DirectX::SimpleMath;

class Transform
{
public:
    Transform() = default;
    Transform(XMFLOAT3 pos, XMFLOAT3 rot, XMFLOAT3 _scale) : position(pos), rotationDeg(rot), scale(_scale) {}
    ~Transform() = default;

    XMFLOAT3 position{ 0.0f, 0.0f, 0.0f };
    XMFLOAT3 rotationDeg{ 0.0f, 0.0f, 0.0f }; // yaw=Y, pitch=X, roll=Z (degrees)
    XMFLOAT3 scale{ 1.0f, 1.0f, 1.0f };
};


