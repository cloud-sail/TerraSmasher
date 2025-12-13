#pragma once
#include "Game/Projectile.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/RendererCommon.hpp"

#include <vector>


struct ProjectileInstanceData
{
	Vec3	m_startPos;
	float	m_intensity;
	Vec3	m_endPos;
	float	m_thickness;

	float	m_color[4]; // it may be packed as Rgba8(uint), and decode in HLSL later, but for now float4 is ok
};


struct ProjectileRenderResources
{
	uint32_t projectileBufferIndex = INVALID_INDEX_U32;  // StructuredBuffer<ProjectileInstanceData>
	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
};


class ProjectileSystem
{
public:
	ProjectileSystem();
	~ProjectileSystem();

	void Startup();
	void Shutdown();

	// Spawn a new projectile
	void SpawnProjectile(ProjectileDefinition const& def);

	// Update all active projectiles
	void Update(float deltaSeconds, World* world);

	// Render all projectiles
	void Render() const;

	// Get count for debugging
	int GetActiveCount() const;

	// Clear all projectiles
	void Clear();

public:
	static constexpr uint32_t MAX_PROJECTILES = 512; // Notes: If more projectile, not spawn

private:
	void CreateBuffers();
	void DestroyBuffers();

	bool m_isInitialized = false;

	std::vector<Projectile> m_projectiles;

	// GPU Buffer for instance data
	Buffer* m_instanceBuffer = nullptr;
	DescriptorHandle m_instanceBufferSRV;

	// Shader
	Shader* m_renderShader = nullptr;

	// Visual parameters
	float m_baseThickness = 0.15f;
	float m_baseIntensity = 1.5f;
	float m_baseTrailLength = 1.0f;
};


/*
// OLD Example Code:
void Ship::FireMachineGun()
{
	Vec3 spawnPos = m_position + m_forward * 5.0f;
	Vec3 velocity = m_forward * 400.0f;  // 400 unit/s

	g_theGame->m_projectileSystem.SpawnProjectile(
		spawnPos,
		velocity,
		0.5f,                              // 0.5s lifetime
		5.0f,                              // 5 unit explosion radius
		Rgba8(255, 180, 80, 255)          // Hot orange tracer
	);
}

void Ship::FireLaser()
{
	Vec3 spawnPos = m_position + m_forward * 5.0f;
	Vec3 velocity = m_forward * 1000.0f;  // High Speed

	g_theGame->m_projectileSystem.SpawnProjectile(
		spawnPos,
		velocity,
		2.0f,
		10.0f,
		Rgba8(100, 200, 255, 255)         // Cyan laser
	);
}

void Ship::FireMissile()
{
	Vec3 spawnPos = m_position + m_forward * 3.0f;
	Vec3 velocity = m_forward * 300.0f;

	g_theGame->m_projectileSystem.SpawnProjectile(
		spawnPos,
		velocity,
		5.0f,
		20.0f,
		Rgba8(255, 50, 50, 255)           // Red exhaust trail
	);
}

void Ship::FirePlasmaBolt()
{
	Vec3 spawnPos = m_position + m_forward * 5.0f;
	Vec3 velocity = m_forward * 500.0f;

	g_theGame->m_projectileSystem.SpawnProjectile(
		spawnPos,
		velocity,
		1.0f,
		8.0f,
		Rgba8(50, 255, 100, 255)          // Green plasma
	);
}
*/
