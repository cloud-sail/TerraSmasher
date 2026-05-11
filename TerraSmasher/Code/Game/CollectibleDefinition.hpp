#pragma once
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/AABB3.hpp"
#include <vector>
#include <string>

class IndexBuffer;
class VertexBuffer;
class Texture;
class Shader;

class CollectibleDefinition
{
public:
	static void InitializeDefinitions(const char* path = "Data/Definitions/CollectibleDefinitions.xml");
	static void ClearDefinitions();
	static CollectibleDefinition const* GetByName(std::string const& defName);
	inline static std::vector<CollectibleDefinition*> s_definitions;

private:
	~CollectibleDefinition();

private:
	bool LoadFromXmlElement(XmlElement const& element);
	bool TryLoadStaticMesh(std::string const& filePath);

public:
	std::string m_name = "UNKNOWN";
	std::string m_displayName = "Unknown Item";

	// Local AABB within the (-0.5,-0.5,0) to (0.5,0.5,1) space
	AABB3 m_localAABB = AABB3(Vec3(-0.5f, -0.5f, 0.0f), Vec3(0.5f, 0.5f, 1.0f));

	// Scoring (per pickup). 0 means this collectible does not contribute to score/combo.
	float m_scoreValue = 0.f;
	float m_comboValue = 0.f;

	// GPU resources
	VertexBuffer* m_vb = nullptr;
	IndexBuffer* m_ib = nullptr;

	Texture* m_diffuseTexture = nullptr;
	Texture* m_normalTexture = nullptr;
	Texture* m_sgeTexture = nullptr;

	Shader* m_shader = nullptr;
};
