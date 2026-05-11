#include "Game/GameMaterialDefinition.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Renderer/Renderer.hpp"

std::vector<GameMaterialDefinition*> GameMaterialDefinition::s_definitions;

Buffer* GameMaterialDefinition::s_materialBuffer = nullptr;

DescriptorHandle GameMaterialDefinition::s_materialBufferSRV;

bool GameMaterialDefinition::LoadFromXmlElement(XmlElement const& element)
{
	m_name = ParseXmlAttribute(element, "name", m_name);

	XmlElement const* visualsElement = element.FirstChildElement("Visuals");

	GUARANTEE_OR_DIE(visualsElement != nullptr, "No Visuals under GameMaterialDefinition!");

	std::string albedoFilePath = ParseXmlAttribute(*visualsElement, "albedo", "");
	std::string normalFilePath = ParseXmlAttribute(*visualsElement, "normal", "");
	std::string ormhFilePath = ParseXmlAttribute(*visualsElement, "ormh", "");
	std::string emissiveFilePath = ParseXmlAttribute(*visualsElement, "emissive", "");

	Texture* albedoTexture = nullptr;
	Texture* normalTexture = nullptr;
	Texture* ormhTexture = nullptr;
	Texture* emissiveTexture = nullptr;

	if (!albedoFilePath.empty())
	{
		albedoTexture = g_theRenderer->CreateOrGetTextureFromFile(albedoFilePath.c_str());
	}
	if (!normalFilePath.empty())
	{
		normalTexture = g_theRenderer->CreateOrGetTextureFromFile(normalFilePath.c_str());
	}
	if (!ormhFilePath.empty())
	{
		ormhTexture = g_theRenderer->CreateOrGetTextureFromFile(ormhFilePath.c_str());
	}
	if (!emissiveFilePath.empty())
	{
		emissiveTexture = g_theRenderer->CreateOrGetTextureFromFile(emissiveFilePath.c_str());
	}

	m_albedoSrvIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(albedoTexture, DefaultTexture::CheckerboardMagentaBlack2D);
	m_normalSrvIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(normalTexture, DefaultTexture::DefaultNormalMap);
	m_ormhSrvIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(ormhTexture, DefaultTexture::DefaultORMHMap);
	m_emissiveSrvIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(emissiveTexture, DefaultTexture::BlackOpaque2D);
	
	m_uvScale = ParseXmlAttribute(*visualsElement, "uvScale", m_uvScale);
	m_lowPolyLevel = ParseXmlAttribute(*visualsElement, "lowPolyLevel", m_lowPolyLevel);

	XmlElement const* toughnessElement = element.FirstChildElement("ToughnessProfile");

	GUARANTEE_OR_DIE(toughnessElement != nullptr, "No ToughnessProfile under GameMaterialDefinition!");

	m_tier = ParseXmlAttribute(*toughnessElement, "tier", m_tier);
	m_strength = ParseXmlAttribute(*toughnessElement, "strength", m_strength);

	// Scoring is optional. Missing element keeps the defaults (0/0 -> no score, no combo).
	XmlElement const* scoringElement = element.FirstChildElement("Scoring");
	if (scoringElement != nullptr)
	{
		m_scorePerCubicMeter = ParseXmlAttribute(*scoringElement, "scorePerCubicMeter", m_scorePerCubicMeter);
		m_comboPerCubicMeter = ParseXmlAttribute(*scoringElement, "comboPerCubicMeter", m_comboPerCubicMeter);
	}

	return true;
}

void GameMaterialDefinition::InitializeDefinitions(const char* path)
{
	ClearDefinitions();

	XmlDocument document;
	XmlResult result = document.LoadFile(path);
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, Stringf("Failed to open xml file: \"%s\"", path));

	XmlElement* rootElement = document.RootElement();
	GUARANTEE_OR_DIE(rootElement, Stringf("No elements in xml file: \"%s\"", path));

	XmlElement* defElement = rootElement->FirstChildElement();

	while (defElement != nullptr)
	{
		std::string elementName = defElement->Name();
		GUARANTEE_OR_DIE(elementName == "GameMaterialDefinition", Stringf("Root child element in %s was <%s>, must be <GameMaterialDefinition>!", path, elementName.c_str()));
		
		GameMaterialDefinition* newDef = new GameMaterialDefinition();
		newDef->LoadFromXmlElement(*defElement);
		s_definitions.push_back(newDef);

		defElement = defElement->NextSiblingElement();
	}


	CreateMaterialBuffer();
}

GameMaterialDefinition const* GameMaterialDefinition::GetByMatID(uint8_t matID)
{
	GUARANTEE_OR_DIE(matID < s_definitions.size(), "Block type is out of range.");
	return s_definitions[matID];
}

GameMaterialDefinition const* GameMaterialDefinition::GetByName(std::string const& defName)
{
	for (int i = 0; i < (int)s_definitions.size(); ++i)
	{
		if (s_definitions[i]->m_name == defName)
		{
			return s_definitions[i];
		}
	}

	ERROR_AND_DIE(Stringf("Game Material is not in Definitions: \"%s\"", defName.c_str()));
}

uint8_t GameMaterialDefinition::GetMatIDByName(std::string const& defName)
{
	for (int i = 0; i < (int)s_definitions.size(); ++i)
	{
		if (s_definitions[i]->m_name == defName)
		{
			return static_cast<uint8_t>(i);
		}
	}

	ERROR_AND_DIE(Stringf("Game Material is not in Definitions: \"%s\"", defName.c_str()));
}

void GameMaterialDefinition::ClearDefinitions()
{
	for (int i = 0; i < (int)s_definitions.size(); ++i)
	{
		delete s_definitions[i];
	}
	s_definitions.clear();

	DestroyMaterialBuffer();
}

void GameMaterialDefinition::CreateMaterialBuffer()
{
	const int materialNum = (int)s_definitions.size();

	BufferInit initData;
	initData.m_size = materialNum * sizeof(GMaterial);
	s_materialBuffer = g_theRenderer->CreateBuffer(initData);


	std::vector<GMaterial> matData;
	matData.reserve(materialNum);

	for (GameMaterialDefinition const* def : s_definitions)
	{
		GMaterial mat;
		mat.albedoTextureIndex		= def->m_albedoSrvIndex;
		mat.normalTextureIndex		= def->m_normalSrvIndex;
		mat.ormhTextureIndex		= def->m_ormhSrvIndex;
		mat.emissiveTextureIndex	= def->m_emissiveSrvIndex;

		mat.uvScale = def->m_uvScale;
		mat.lowPolyLevel = def->m_lowPolyLevel;

		matData.push_back(mat);
	}


	s_materialBufferSRV = g_theRenderer->AllocateStructuredBufferSRV(*s_materialBuffer, sizeof(GMaterial), materialNum);

	g_theRenderer->UpdateBuffer(*s_materialBuffer, sizeof(GMaterial) * matData.size(), matData.data());
}

void GameMaterialDefinition::DestroyMaterialBuffer()
{
	g_theRenderer->DestroyBuffer(s_materialBuffer);
	g_theRenderer->EnqueueDeferredRelease(s_materialBufferSRV);
}

