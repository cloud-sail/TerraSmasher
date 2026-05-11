#include "Game/Projectile.hpp"
#include "Game/World.hpp"
#include "Game/ExplosionSystem.hpp"
#include "Game/VoxelBreakSystem.hpp"
#include "Game/DebrisSystem.hpp"

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
	, m_tier(def.m_tier)
	, m_strength(def.m_strength)
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
		StrikeContext ctx;
		ctx.m_tier = m_tier;
		ctx.m_strength = m_strength;
		StrikeResult strikeResult = world->ApplyStrike(rayResult.m_impactPos, m_explosionRadius, ctx);

		ExplosionDefinition expDef;
		expDef.m_center = rayResult.m_impactPos;
		expDef.m_maxRadius = m_explosionRadius;
		expDef.m_color = m_color;

		world->SpawnExplosionVFX(expDef);




		if (strikeResult.breakTriggered)
		{
			// Break something
			DebrisDefinition debrisDef;
			debrisDef.m_emitterPos = rayResult.m_impactPos;
			debrisDef.m_emitterDir = rayResult.m_impactNormal;
			debrisDef.m_materialID = strikeResult.GetMaxVolumeRemovedIndex();

			debrisDef.m_coneHalfAngleRadians	= 1.57f;
			debrisDef.m_particleCount			= 30;
			debrisDef.m_lifetime		= FloatRange(1.5f, 3.0f);
			debrisDef.m_initialSpeed	= FloatRange(2.0f, 6.0f);
			debrisDef.m_initialScale	= FloatRange(0.2f, 0.6f);
			debrisDef.m_eulerSpeed		= FloatRange(-10.0f, 10.0f);
			
			world->SpawnDebrisVFX(debrisDef);
		}
		else if (!strikeResult.anyModified)
		{
			// Hit immune object
			// #ToDo play immune sound, give controller/visual feedback
		}
		else
		{
			// Do damage to some voxels, but not break anything

			DebrisDefinition debrisDef;
			debrisDef.m_emitterPos = rayResult.m_impactPos;
			debrisDef.m_emitterDir = rayResult.m_impactNormal;
			debrisDef.m_materialID = strikeResult.GetMaxVolumeDamageAccumulatedIndex();

			debrisDef.m_coneHalfAngleRadians	= 2.0f;
			debrisDef.m_particleCount			= 60;
			debrisDef.m_lifetime		= FloatRange(0.8f, 1.8f);
			debrisDef.m_initialSpeed	= FloatRange(1.0f, 3.0f);
			debrisDef.m_initialScale	= FloatRange(0.06f, 0.3f);
			debrisDef.m_eulerSpeed		= FloatRange(-3.0f, 3.0f);

			world->SpawnDebrisVFX(debrisDef);
		}
		// Sometimes you see Debris Material seems wrong (because hard blend with soft material(>30) will also be destroyed)












		m_isDead = true;
	}
	else
	{
		// No collision, continue flying
		m_position += m_velocity * deltaSeconds;
	}

}
