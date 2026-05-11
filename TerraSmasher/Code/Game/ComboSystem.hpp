#pragma once
#include <functional>

//-----------------------------------------------------------------------------------------------
// ComboSystem: a pure-logic state machine that owns
//   - a 0..1 progress bar inside the current rank
//   - a rank index in [0, NUM_RANKS-1]
//
// We use the term "rank" for the combo level so it doesn't collide with the toughness "tier"
// that lives on materials / projectiles (ToughnessProfile::m_tier).
//
// External callers feed it raw combo amounts (already scaled by material/collectible config but
// not yet by rank ratio). The system:
//   - Applies the current rank's gain ratio to convert raw -> progress.
//   - Handles cross-rank credit on add (carrying remaining raw to higher ranks using the new ratio).
//   - Handles cross-rank decay on tick (draining time across ranks as upper ranks empty out).
//   - Emits OnRankChanged whenever the rank index changes (up or down).
//
// The system has no knowledge of materials, score, or the player ship. World subscribes to the
// rank-change event and translates it into PlayerShip::SetPlayerStrength + UI feedback.
//-----------------------------------------------------------------------------------------------

class ComboSystem
{
public:
	using RankChangedCallback = std::function<void(int oldRank, int newRank)>;

	static constexpr int NUM_RANKS = 5;

	ComboSystem();

	// Per-frame decay tick. Drains progress at the current rank's decay rate, dropping down through
	// ranks if a single dt is large enough to bleed multiple ranks. Floors at rank 0 / progress 0.
	void Tick(float deltaSeconds);

	// Add a raw combo amount (e.g. volume * material ComboPerCubicMeter, or collectible ComboValue).
	// Rank ratio is applied internally; if the current rank fills up and we're not at the top,
	// remaining raw is carried into the next rank and converted at that rank's ratio.
	void AddRawCombo(float rawAmount);

	// Reset to bottom rank with progress 0 (e.g. on entering Space mode).
	void ResetToBottom();

	void SetOnRankChanged(RankChangedCallback cb) { m_onRankChanged = std::move(cb); }

	int   GetCurrentRank() const     { return m_currentRank; }
	float GetCurrentProgress() const { return m_currentProgress; }
	float GetCurrentScoreMultiplier() const;

	// Returns the player-strength value (0..NUM_RANKS-1) that the given rank maps to. The mapping
	// matches PlayerShip::m_playerStrength's [0..4] convention.
	int   GetAbilityLevelForRank(int rank) const;

	// Trauma / shake. AddRawCombo automatically pushes trauma based on the raw amount it received,
	// so callers normally don't need to call AddTrauma directly. Trauma decays in Tick.
	void  AddTrauma(float stress);
	float GetTrauma() const { return m_trauma; }

	// Returns the current scale-shake multiplier offset. Add this to a base scale of 1.0 to get the
	// pulsing scale. Sampled from a deterministic time-based oscillator weighted by trauma^2.
	// Output range is roughly [-MAX_SCALE_OFFSET, +MAX_SCALE_OFFSET]; near zero when trauma == 0.
	float GetCurrentScaleShake(float totalSeconds) const;

private:
	void StepUpRank();   // emits event
	void StepDownRank(); // emits event

private:
	int   m_currentRank = 0;
	float m_currentProgress = 0.f;
	RankChangedCallback m_onRankChanged;

	// Juice: trauma from each AddRawCombo, decayed every Tick. Drives scale shake on the meter UI.
	float m_trauma = 0.f;
};
