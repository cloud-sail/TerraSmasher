#include "Game/ScoreSystem.hpp"

#include "Game/ComboSystem.hpp"
#include "Game/GameMaterialDefinition.hpp"
#include "Game/CollectibleDefinition.hpp"

//-----------------------------------------------------------------------------------------------
ScoreSystem::ScoreSystem(ComboSystem* combo)
	: m_combo(combo)
{
}

//-----------------------------------------------------------------------------------------------
void ScoreSystem::OnTerrainBroken(uint8_t matID, float volume)
{
	if (volume <= 0.f) return;
	if (!m_combo) return;

	if (matID >= GameMaterialDefinition::s_definitions.size()) return;
	GameMaterialDefinition const* def = GameMaterialDefinition::s_definitions[matID];
	if (!def) return;

	float baseScore = def->m_scorePerCubicMeter * volume;
	if (baseScore > 0.f)
	{
		m_totalScore += static_cast<double>(baseScore) * static_cast<double>(m_combo->GetCurrentScoreMultiplier());
	}

	float rawCombo = def->m_comboPerCubicMeter * volume;
	if (rawCombo > 0.f)
	{
		m_combo->AddRawCombo(rawCombo);
	}
}

//-----------------------------------------------------------------------------------------------
void ScoreSystem::OnTerrainBrokenBulk(std::vector<float> const& volumesByMatID)
{
	for (size_t i = 0; i < volumesByMatID.size(); ++i)
	{
		float v = volumesByMatID[i];
		if (v <= 0.f) continue;
		OnTerrainBroken(static_cast<uint8_t>(i), v);
	}
}

//-----------------------------------------------------------------------------------------------
void ScoreSystem::OnCollectiblePicked(CollectibleDefinition const* def)
{
	if (!def || !m_combo) return;

	if (def->m_scoreValue > 0.f)
	{
		m_totalScore += static_cast<double>(def->m_scoreValue) * static_cast<double>(m_combo->GetCurrentScoreMultiplier());
	}

	if (def->m_comboValue > 0.f)
	{
		m_combo->AddRawCombo(def->m_comboValue);
	}
}
