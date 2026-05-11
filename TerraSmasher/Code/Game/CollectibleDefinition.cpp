#include "Game/CollectibleDefinition.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/StaticMeshUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"

void CollectibleDefinition::InitializeDefinitions(const char* path /*= "Data/Definitions/CollectibleDefinitions.xml"*/)
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
		GUARANTEE_OR_DIE(elementName == "CollectibleDefinition", Stringf("Root child element in %s was <%s>, must be <CollectibleDefinition>!", path, elementName.c_str()));
		CollectibleDefinition* newDef = new CollectibleDefinition();

		newDef->LoadFromXmlElement(*defElement);
		s_definitions.push_back(newDef);

		defElement = defElement->NextSiblingElement();
	}
}

void CollectibleDefinition::ClearDefinitions()
{
	for (int i = 0; i < static_cast<int>(s_definitions.size()); ++i)
	{
		delete s_definitions[i];
		s_definitions[i] = nullptr;
	}

	s_definitions.clear();
}

CollectibleDefinition const* CollectibleDefinition::GetByName(std::string const& defName)
{
	for (int i = 0; i < (int)s_definitions.size(); ++i)
	{
		if (s_definitions[i]->m_name == defName)
		{
			return s_definitions[i];
		}
	}

	return nullptr;
}

CollectibleDefinition::~CollectibleDefinition()
{
	delete m_vb;
	m_vb = nullptr;

	delete m_ib;
	m_ib = nullptr;
}

bool CollectibleDefinition::LoadFromXmlElement(XmlElement const& element)
{
	m_name = ParseXmlAttribute(element, "name", m_name);
	m_displayName = ParseXmlAttribute(element, "displayName", m_name);

	m_vb = g_theRenderer->CreateVertexBuffer(1 * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	m_ib = g_theRenderer->CreateIndexBuffer(1 * sizeof(unsigned int));

	if (!TryLoadStaticMesh(ParseXmlAttribute(element, "model", "")))
	{
		ERROR_AND_DIE(Stringf("Failed to load collectible model for: \"%s\"", m_name.c_str()));
	}

	// Local AABB
	XmlElement const* aabbElement = element.FirstChildElement("LocalAABB");
	if (aabbElement)
	{
		Vec3 mins = ParseXmlAttribute(*aabbElement, "mins", Vec3(-0.5f, -0.5f, 0.0f));
		Vec3 maxs = ParseXmlAttribute(*aabbElement, "maxs", Vec3(0.5f, 0.5f, 1.0f));
		m_localAABB = AABB3(mins, maxs);
	}

	// Scoring is optional. Missing element keeps the defaults (0/0 -> no score, no combo).
	XmlElement const* scoringElement = element.FirstChildElement("Scoring");
	if (scoringElement != nullptr)
	{
		m_scoreValue = ParseXmlAttribute(*scoringElement, "scoreValue", m_scoreValue);
		m_comboValue = ParseXmlAttribute(*scoringElement, "comboValue", m_comboValue);
	}

	return true;
}

bool CollectibleDefinition::TryLoadStaticMesh(std::string const& filePath)
{
	if (filePath.empty())
	{
		return false;
	}

	std::vector<Vertex_PCUTBN> verts;
	std::vector<unsigned int> indexes;

	StaticModelInfo modelInfo;
	bool isSuccess = LoadOBJFromXML(verts, modelInfo, filePath.c_str());
	if (!isSuccess)
	{
		return false;
	}

	if (modelInfo.m_shaderName.empty()) modelInfo.m_shaderName = "Data/Shaders/BlinnPhong";
	m_shader = g_theRenderer->CreateOrGetShader(ShaderConfig(modelInfo.m_shaderName.c_str()), VertexType::VERTEX_PCUTBN);

	if (!modelInfo.m_diffuseMapFilePath.empty())
	{
		m_diffuseTexture = g_theRenderer->CreateOrGetTextureFromFile(modelInfo.m_diffuseMapFilePath.c_str());
	}
	else
	{
		m_diffuseTexture = nullptr;
	}

	if (!modelInfo.m_normalMapFilePath.empty())
	{
		m_normalTexture = g_theRenderer->CreateOrGetTextureFromFile(modelInfo.m_normalMapFilePath.c_str());
	}
	else
	{
		m_normalTexture = nullptr;
	}

	if (!modelInfo.m_specGlossEmitMapFilePath.empty())
	{
		m_sgeTexture = g_theRenderer->CreateOrGetTextureFromFile(modelInfo.m_specGlossEmitMapFilePath.c_str());
	}
	else
	{
		m_sgeTexture = nullptr;
	}

	indexes.reserve(verts.size());
	for (int i = 0; i < (int)verts.size(); ++i)
	{
		indexes.push_back(i);
	}

	g_theRenderer->CopyCPUToGPU(verts.data(), static_cast<unsigned int>(verts.size()) * m_vb->GetStride(), m_vb);
	g_theRenderer->CopyCPUToGPU(indexes.data(), static_cast<unsigned int>(indexes.size()) * m_ib->GetStride(), m_ib);

	return true;
}
