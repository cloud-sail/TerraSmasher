#include "Game/VoxelBreakSystem.hpp"
#include "Game/Voxel.hpp"

StrikeOutcome ComputeStrikeOutcome(ToughnessProfile const& profile, StrikeContext const& ctx, uint8_t& out_damagePerHit)
{
	static constexpr uint8_t DAMAGE_OVERPOWER	= 128;  // D < 0	2 times
	static constexpr uint8_t DAMAGE_STANDARD	= 86;   // D == 0	3 times
	static constexpr uint8_t DAMAGE_STRUGGLE	= 43;   // D == 1	6 times
	static constexpr uint8_t DAMAGE_CAPPED		= 29;   // D >= 2	9 times

	out_damagePerHit = 0;
	int tierDiff = profile.m_tier - ctx.m_tier;

	if (tierDiff >= 2)
	{
		return StrikeOutcome::Immune;
	}

	if (tierDiff <= 0)
	{
		return StrikeOutcome::Instant;
	}

	int strengthDiff = profile.m_strength - ctx.m_strength;

	if (strengthDiff < 0)
	{
		out_damagePerHit = DAMAGE_OVERPOWER;
	}
	else if (strengthDiff == 0)
	{
		out_damagePerHit = DAMAGE_STANDARD;
	}
	else if (strengthDiff == 1)
	{
		out_damagePerHit = DAMAGE_STRUGGLE;
	}
	else if (strengthDiff >= 2)
	{
		out_damagePerHit = DAMAGE_CAPPED;
	}

	return StrikeOutcome::Cumulative;
}

uint8_t StrikeResult::GetMaxVolumeDamageAccumulatedIndex() const
{
	if (materialDamageAccumulated.empty())
	{
		return Voxel::INVALID_MATERIAL_ID;
	}

	size_t maxIdx = 0;
	for (size_t i = 1; i < materialDamageAccumulated.size(); ++i)
	{
		if (materialDamageAccumulated[i] > materialDamageAccumulated[maxIdx])
			maxIdx = i;
	}
	return static_cast<uint8_t>(maxIdx);
}

uint8_t StrikeResult::GetMaxVolumeRemovedIndex() const
{
	if (materialVolumesRemoved.empty())
	{
		return Voxel::INVALID_MATERIAL_ID;
	}

	size_t maxIdx = 0;
	for (size_t i = 1; i < materialVolumesRemoved.size(); ++i)
	{
		if (materialVolumesRemoved[i] > materialVolumesRemoved[maxIdx])
			maxIdx = i;
	}

	if (materialVolumesRemoved[maxIdx] < 5.f) // #ToFix Magic Number for not generate debris
	{
		return Voxel::INVALID_MATERIAL_ID;
	}
	return static_cast<uint8_t>(maxIdx);
}
