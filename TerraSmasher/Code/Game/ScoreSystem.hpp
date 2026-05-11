#pragma once
#include <cstdint>
#include <vector>

class ComboSystem;
class CollectibleDefinition;

//-----------------------------------------------------------------------------------------------
// ScoreSystem: thin glue layer between gameplay events and the ComboSystem / total score.
//
// On terrain destruction it looks up the per-material score and combo rates, multiplies score
// by the current ComboSystem multiplier, and feeds the raw combo amount into the ComboSystem
// (which applies its own tier ratio internally).
//
// On collectible pickup it does the same with the collectible's own score / combo values.
//
// IMPORTANT: the score multiplier is applied to the score only; combo progress is never multiplied
// by the score multiplier (otherwise the same factor would scale both axes, double-counting).
//-----------------------------------------------------------------------------------------------

class ScoreSystem
{
public:
	explicit ScoreSystem(ComboSystem* combo);

	// matID indexes GameMaterialDefinition::s_definitions. volume is in cubic meters.
	void OnTerrainBroken(uint8_t matID, float volume);

	// Convenience: feed a whole vector indexed by matID (matches StrikeResult::materialVolumesRemoved).
	void OnTerrainBrokenBulk(std::vector<float> const& volumesByMatID);

	// def may be null; in that case this is a no-op.
	void OnCollectiblePicked(CollectibleDefinition const* def);

	void   Reset() { m_totalScore = 0.0; }
	double GetTotalScore() const { return m_totalScore; }

private:
	ComboSystem* m_combo = nullptr; // not owned
	double       m_totalScore = 0.0;
};
