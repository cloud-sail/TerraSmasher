#include "Game/Projectile.hpp"
#include "Game/World.hpp"

Projectile::Projectile(ProjectileDefinition const& def)
	: m_position(def.m_startPos)
	, m_velocity(def.m_velocity)
	, m_color(def.m_color)
	, m_remainingLifetime(def.m_lifetime)
	, m_explosionRadius(def.m_explosionRadius)
	, m_intensityMultiplier(def.m_intensityMultiplier)
	, m_thicknessMultiplier(def.m_thicknessMultiplier)
	, m_lengthMultiplier(def.m_lengthMultiplier)
	, m_deltaDensity(def.m_deltaDensity)
{
}

void Projectile::Update(float deltaSeconds, World* world)
{
	if (m_isDead)
		return;

	// Calculate actual time to simulate (clamped by remaining lifetime)
	float actualDeltaSeconds = deltaSeconds;
	if (m_remainingLifetime < deltaSeconds)
	{
		actualDeltaSeconds = m_remainingLifetime;
	}

	m_remainingLifetime -= deltaSeconds;
	if (m_remainingLifetime <= 0.0f)
	{
		m_isDead = true;
		// Don't return yet - still need to simulate the final movement
	}

	if (world == nullptr) return;

	float travelDistance = m_velocity.GetLength() * deltaSeconds;
	Vec3 travelDirection = m_velocity.GetNormalized();

	VoxelRaycastResult3D rayResult = world->DoProjectileTrace(
		m_position,
		travelDirection,
		travelDistance
	);



	if (rayResult.m_didImpact)
	{
		// Hit something! Trigger explosion
		//world->Explode(rayResult.m_impactPos, m_explosionRadius);

		//SDFSphere* brushSDF = new SDFSphere(m_explosionRadius);
		//brushSDF->GetMutTransform().m_position = rayResult.m_impactPos;

		world->ApplyExplosion(rayResult.m_impactPos, m_explosionRadius, m_deltaDensity);

		m_isDead = true;
	}
	else
	{
		// No collision, continue flying
		m_position += m_velocity * deltaSeconds;
	}

}
