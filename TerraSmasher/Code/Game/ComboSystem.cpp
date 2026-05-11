#include "Game/ComboSystem.hpp"

#include <algorithm>
#include <cmath>

namespace
{
	// Per-rank configuration. Index 0 is the bottom rank, NUM_RANKS-1 is the top.
	// abilityLevel:    value passed to PlayerShip::SetPlayerStrength when this rank becomes active (0..4).
	// scoreMultiplier: total-score multiplier applied to incoming base score while in this rank.
	// comboGainRatio:  raw -> progress conversion ratio. Lower at higher ranks so they're harder to fill.
	// decayPerSecond:  how much progress drains per second. Higher ranks decay faster.
	// entryProgress:   the progress value the bar lands on when this rank is entered (from above or below).
	struct ComboRankConfig
	{
		int   abilityLevel;
		float scoreMultiplier;
		float comboGainRatio;
		float decayPerSecond;
		float entryProgress;
	};

	// Decay design target: each rank alone empties in <= 10 seconds (rank 0 worst case ~8.3s),
	// while higher ranks empty noticeably faster. Full descent from rank 4 progress=1.0 down to
	// rank 0 progress=0 is roughly 8.5s, since each step lands at entryProgress=0.3 instead of 1.0.
	static const ComboRankConfig s_rankConfigs[ComboSystem::NUM_RANKS] = {
		// abilityLv, scoreMult, gainRatio, decay, entryProgress
		{ 0, 1.0f, 1.00f, 0.12f, 0.0f },
		{ 1, 1.1f, 0.80f, 0.15f, 0.34f },
		{ 2, 1.3f, 0.60f, 0.18f, 0.34f },
		{ 3, 1.6f, 0.40f, 0.22f, 0.34f },
		{ 4, 2.0f, 0.25f, 0.30f, 0.34f },
	};

	// Trauma tuning.
	//   STRESS_COEFF + sqrt mapping: small raw still registers visibly, large raw saturates near 1.
	//   Examples:
	//     raw 0.025 (one Grass hit)        -> stress ~0.087
	//     raw 0.20  (one Gold cube hit)    -> stress ~0.245
	//     raw 2.0   (a standard collectible)-> stress ~0.775
	//     raw 3.5   (gold_bar)              -> stress 1.0 (clamped)
	//   TRAUMA_DECAY_PER_SECOND: 1.0 trauma drains in 1.0 / 1.5 ~ 0.67s.
	//   MAX_SCALE_OFFSET: at trauma==1, peak amplitude of scale oscillation (so meter pumps ~+/-12%).
	static constexpr float STRESS_COEFF = 0.30f;
	static constexpr float TRAUMA_DECAY_PER_SECOND = 1.5f;
	static constexpr float MAX_SCALE_OFFSET = 0.12f;
	static constexpr float PI = 3.1415926535897932384626433832795f;

	float MapRawToStress(float raw)
	{
		if (raw <= 0.f) return 0.f;
		float s = sqrtf(raw * STRESS_COEFF);
		if (s > 1.f) s = 1.f;
		return s;
	}

	float TimeBasedOscillator(float seconds)
	{
		float x = 20.f * seconds;
		return (sinf(2.f * x) + sinf(PI * x)) * 0.5f;
	}
}

//-----------------------------------------------------------------------------------------------
ComboSystem::ComboSystem()
	: m_currentRank(0)
	, m_currentProgress(s_rankConfigs[0].entryProgress)
{
}

//-----------------------------------------------------------------------------------------------
void ComboSystem::ResetToBottom()
{
	int oldRank = m_currentRank;
	m_currentRank = 0;
	m_currentProgress = s_rankConfigs[0].entryProgress;
	m_trauma = 0.f;

	if (oldRank != 0 && m_onRankChanged)
	{
		m_onRankChanged(oldRank, 0);
	}
}

//-----------------------------------------------------------------------------------------------
float ComboSystem::GetCurrentScoreMultiplier() const
{
	return s_rankConfigs[m_currentRank].scoreMultiplier;
}

//-----------------------------------------------------------------------------------------------
int ComboSystem::GetAbilityLevelForRank(int rank) const
{
	if (rank < 0) rank = 0;
	if (rank >= NUM_RANKS) rank = NUM_RANKS - 1;
	return s_rankConfigs[rank].abilityLevel;
}

//-----------------------------------------------------------------------------------------------
void ComboSystem::StepUpRank()
{
	if (m_currentRank >= NUM_RANKS - 1) return;

	int oldRank = m_currentRank;
	m_currentRank += 1;
	m_currentProgress = s_rankConfigs[m_currentRank].entryProgress;

	if (m_onRankChanged)
	{
		m_onRankChanged(oldRank, m_currentRank);
	}
}

//-----------------------------------------------------------------------------------------------
void ComboSystem::StepDownRank()
{
	if (m_currentRank <= 0) return;

	int oldRank = m_currentRank;
	m_currentRank -= 1;
	m_currentProgress = s_rankConfigs[m_currentRank].entryProgress;

	if (m_onRankChanged)
	{
		m_onRankChanged(oldRank, m_currentRank);
	}
}

//-----------------------------------------------------------------------------------------------
// Cross-rank credit: treat rawAmount as a budget. Each rank consumes part of it at its own ratio
// to fill its remaining room; if the rank fills, we step up and continue with the new ratio.
// The mental model: raw is currency, each rank's ratio is the local exchange rate, the progress
// bar is the current rank's balance, and we make change between ranks.
void ComboSystem::AddTrauma(float stress)
{
	if (stress <= 0.f) return;
	m_trauma += stress;
	if (m_trauma > 1.f) m_trauma = 1.f;
}

//-----------------------------------------------------------------------------------------------
float ComboSystem::GetCurrentScaleShake(float totalSeconds) const
{
	if (m_trauma <= 0.f) return 0.f;
	float weight = m_trauma * m_trauma; // squared so small trauma barely moves
	return MAX_SCALE_OFFSET * weight * TimeBasedOscillator(totalSeconds);
}

//-----------------------------------------------------------------------------------------------
void ComboSystem::AddRawCombo(float rawAmount)
{
	if (rawAmount <= 0.f) return;

	// Push trauma based on the input size before we consume it across ranks.
	AddTrauma(MapRawToStress(rawAmount));

	float remainingRaw = rawAmount;

	// Hard guard against infinite loops in case of pathological config (e.g. ratio == 0 below top).
	int safetyIterations = NUM_RANKS + 1;

	while (remainingRaw > 0.f && safetyIterations-- > 0)
	{
		ComboRankConfig const& rank = s_rankConfigs[m_currentRank];
		float ratio = rank.comboGainRatio;

		// Top rank: clamp and stop. Excess raw is intentionally discarded (no higher rank exists).
		if (m_currentRank == NUM_RANKS - 1)
		{
			float gain = remainingRaw * ratio;
			m_currentProgress = std::min(1.f, m_currentProgress + gain);
			return;
		}

		// Defensive: a rank with ratio <= 0 below the top can't accept raw. Just stop.
		if (ratio <= 0.f) return;

		float room = 1.f - m_currentProgress;
		float gain = remainingRaw * ratio;

		if (gain < room)
		{
			// All remaining raw fits in this rank.
			m_currentProgress += gain;
			return;
		}

		// This rank is about to fill. Spend exactly the raw it costs to top off, then step up.
		float rawUsed = room / ratio;
		remainingRaw -= rawUsed;
		StepUpRank();
	}
}

//-----------------------------------------------------------------------------------------------
// Cross-rank decay: treat deltaSeconds as a time budget. Drain the current rank; if it empties
// and we're not at the bottom, step down and keep draining at the new rank's decay rate.
void ComboSystem::Tick(float deltaSeconds)
{
	if (deltaSeconds <= 0.f) return;

	// Trauma decay first; runs every frame regardless of progress drain logic below.
	if (m_trauma > 0.f)
	{
		m_trauma -= TRAUMA_DECAY_PER_SECOND * deltaSeconds;
		if (m_trauma < 0.f) m_trauma = 0.f;
	}

	float remainingTime = deltaSeconds;

	int safetyIterations = NUM_RANKS + 1;

	while (remainingTime > 0.f && safetyIterations-- > 0)
	{
		ComboRankConfig const& rank = s_rankConfigs[m_currentRank];

		// Rank configured not to decay (e.g. for a future grace-period feature). Stop here.
		if (rank.decayPerSecond <= 0.f) return;

		float timeToEmpty = m_currentProgress / rank.decayPerSecond;

		if (remainingTime < timeToEmpty)
		{
			m_currentProgress -= rank.decayPerSecond * remainingTime;
			if (m_currentProgress < 0.f) m_currentProgress = 0.f;
			return;
		}

		// This rank bleeds out completely.
		remainingTime -= timeToEmpty;

		if (m_currentRank == 0)
		{
			// Bottom rank: floor at zero and stop.
			m_currentProgress = 0.f;
			return;
		}

		StepDownRank();
	}
}
