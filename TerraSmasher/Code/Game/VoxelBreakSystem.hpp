#pragma once
#include "Engine/Math/IntBox3.hpp"
#include <cstdint>
#include <vector>

struct ToughnessProfile
{
	int m_tier = 0;
	int m_strength = 0;

	// <
	inline bool IsLessDurableThan(ToughnessProfile const& other) const
	{
		if (m_tier != other.m_tier) return m_tier < other.m_tier;
		return m_strength < other.m_strength;
	}

	// <=
	inline bool IsNoMoreDurableThan(ToughnessProfile const& other) const
	{
		return !other.IsLessDurableThan(*this);
	}
};

// For player, sth interact with terrain
struct StrikeContext
{
	int m_tier = 0;
	int m_strength = 0;
};

enum class StrikeOutcome
{
	Immune,
	Cumulative,
	Instant,
};

struct StrikeResult
{
	bool          anyModified = false;		// any damage or any density change
	bool          breakTriggered = false;	// any density change
	IntBox3       affectedRegion;			// SDF possible affected region
	std::vector<float>	materialDamageAccumulated;	// Pass 1
	std::vector<float>	materialVolumesRemoved;		// Pass 2

	uint8_t GetMaxVolumeDamageAccumulatedIndex() const;
	uint8_t GetMaxVolumeRemovedIndex() const;
};

StrikeOutcome ComputeStrikeOutcome(ToughnessProfile const& profile, StrikeContext const& ctx, uint8_t& out_damagePerHit);
