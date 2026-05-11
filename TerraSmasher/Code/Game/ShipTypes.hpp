#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Quat.hpp"
#include "Engine/Math/Mat44.hpp"
#include <string>

struct ShipSpawnInfo
{
	std::string m_shipName;
	Vec3 m_worldPosition;
	Quat m_worldRotation;
};

// prev- should not be changed
struct InputOutputParams
{
	float m_deltaSeconds;

	// Player Control
	float	m_throttleInput = 0.f; // 0 ~ 1
	Vec2	m_rotationInput; // Unit Circle, it is processed right stick or mouse, the ship offset will read this
	
	// Previous Frame Data (Most of calculation is based on prev frame data)
	Vec3 m_prevVelocity;
	Quat m_prevRotation;

	float m_prevCameraDistance;
	float m_prevCameraFOV;

	Vec3 m_prevBodyLocalPosition; // may be changed by some behaviors (current pos)
	Quat m_prevBodyLocalRotation;

	Mat44 m_prevBodyWorldTransform; // Used for collision check now

	// Result
	Vec3 m_linearVelocity; // do not read this, m_linearVelocity = m_prevVelocity initially
	Vec2 m_angularVelocity; // x-yaw y-pitch
	float m_cameraDistance;
	float m_cameraFOV;
	Vec3 m_bodyTargetLocalPosition; // target pos, Lerp After Collision?
	Quat m_bodyTargetLocalRotation;

	// Input
	bool m_isFiring = false;

};
