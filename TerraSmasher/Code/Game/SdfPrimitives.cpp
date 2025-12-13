#include "Game/SdfPrimitives.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include <algorithm>
#include <cmath>


float SdfHelper::Sphere(Vec3 p, float r)
{
	return p.GetLength() - r;
}

float SdfHelper::Box(Vec3 p, Vec3 h)
{
	Vec3 q = Vec3(fabsf(p.x), fabsf(p.y), fabsf(p.z)) - h;
	float outside = Vec3(std::max(q.x, 0.f), std::max(q.y, 0.f), std::max(q.z, 0.f)).GetLength();
	float inside = std::min(std::max(q.x, std::max(q.y, q.z)), 0.f);

	return outside + inside;
}

float SdfHelper::Ellipsoid(Vec3 p, Vec3 r)
{
	// approximated value
	float k0 = (p / r).GetLength();
	float k1 = (p / (r * r)).GetLength();
	return k0 * (k0 - 1.0f) / k1;
}

float SdfHelper::Torus(Vec3 p, float majorRadius, float minorRadius)
{
	// Torus lies in XY plane, hole faces Z direction
	float distXY = sqrtf(p.x * p.x + p.y * p.y);
	Vec2 q(distXY - majorRadius, p.z);

	return q.GetLength() - minorRadius;
}

float SdfHelper::Capsule(Vec3 p, Vec3 a, Vec3 b, float r)
{
	Vec3 aToP = p - a;
	Vec3 aToB = b - a;
	float h = GetClampedZeroToOne(DotProduct3D(aToP, aToB) / aToB.GetLengthSquared());

	return (aToP - aToB * h).GetLength() - r;
}

float SdfHelper::CutSphere(Vec3 p, float r, float h)
{
	// +z direction, SDF is the upper part, |h| < r, h<0 is bigger part of the sphere
	float w = sqrtf(r * r - h * h);

	float qx = sqrtf(p.x * p.x + p.y * p.y);  // horizontal distance
	float qy = p.z;  // vertical coordinate

	float s = std::max((h - r) * qx * qx + w * w * (h + r - 2.0f * qy), h * qx - w * qy);

	if (s < 0.0f)
	{
		return sqrtf(qx * qx + qy * qy) - r;
	}
	else if (qx < w)
	{
		return h - qy;
	}
	else
	{
		Vec2 diff = Vec2(qx, qy) - Vec2(w, h);
		return diff.GetLength();
	}
}

float SdfHelper::CutHollowSphere(Vec3 p, float r, float h, float t)
{
	// +z direction, SDF is the bottom part
	float w = sqrtf(r * r - h * h);

	Vec2 q = Vec2(sqrtf(p.x * p.x + p.y * p.y), p.z);

	return ((h * q.x < w * q.y) ? GetDistance2D(q, Vec2(w, h)) :
		fabsf(q.GetLength() - r)) - t;
}
