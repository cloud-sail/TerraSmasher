#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"

struct AABB3;
struct Mat44;

struct GameTransform
{
	Vec3 m_position;
	EulerAngles m_orientation;
	float m_uniformScale = 1.f;

	GameTransform() = default;

	Vec3 TransformWorldToLocal(Vec3 const& worldPos) const;

	AABB3 TransformLocalToWorldAABB(AABB3 const& localAABB) const;

	Mat44 GetAsMatrix() const;
};

