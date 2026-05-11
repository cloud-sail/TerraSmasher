#pragma once
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec3.hpp"
#include <stdint.h>


class World;

// 200 unit/s 10 times of ship speed
// 0.5s 100 unit total range

struct ProjectileDefinition
{
	Vec3	m_startPos;
	Vec3	m_velocity;
	float	m_lifetime = 0.5f;
	float	m_explosionRadius = 5.0f;
	Rgba8	m_color = Rgba8::OPAQUE_WHITE;
	float	m_intensityMultiplier = 1.0f;
	float	m_thicknessMultiplier = 1.0f;
	float	m_lengthMultiplier = 1.0f;

	uint8_t m_deltaDensity = 255; // If use hardness system, this parameter will not be used

	int m_tier = 0;
	int m_strength = 0;

};


class Projectile
{
public:
	Projectile(ProjectileDefinition const& def);

	void Update(float deltaSeconds, World* world);

	bool IsDead() const { return m_isDead; }
	Vec3 GetPosition() const { return m_position; }
	Vec3 GetVelocity() const { return m_velocity; }
	Rgba8 GetColor() const { return m_color; }
	float GetIntensityMultiplier() const { return m_intensityMultiplier; }
	float GetThicknessMultiplier() const { return m_thicknessMultiplier; }
	float GetLengthMultiplier() const { return m_lengthMultiplier; }

private:
	Vec3 m_position;
	Vec3 m_velocity;
	Rgba8 m_color;

	float m_remainingLifetime;
	float m_explosionRadius;

	float m_intensityMultiplier;
	float m_thicknessMultiplier;
	float m_lengthMultiplier;

	uint8_t m_deltaDensity;

	int m_tier = 0;
	int m_strength = 0;

	bool m_isDead = false;
};

