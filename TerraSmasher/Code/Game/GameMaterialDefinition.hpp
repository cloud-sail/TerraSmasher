#pragma once
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Renderer/RendererCommon.hpp"
#include <vector>
#include <string>



struct GMaterial
{
	uint32_t albedoTextureIndex = INVALID_INDEX_U32;
	uint32_t normalTextureIndex = INVALID_INDEX_U32;
	uint32_t ormhTextureIndex = INVALID_INDEX_U32;
	uint32_t emissiveTextureIndex = INVALID_INDEX_U32;

	float uvScale = 1.f;
	float lowPolyLevel = 0.f;
};



class GameMaterialDefinition
{
public:
	static void InitializeDefinitions(const char* path);
	static GameMaterialDefinition const* GetByMatID(uint8_t matID);
	static GameMaterialDefinition const* GetByName(std::string const& defName);
	static uint8_t GetMatIDByName(std::string const& defName);
	static void ClearDefinitions();

	static void CreateMaterialBuffer();
	static void DestroyMaterialBuffer();

	static uint32_t GetMaterialBufferIndex() { return s_materialBufferSRV.m_index; }

	static std::vector<GameMaterialDefinition*> s_definitions;
	static Buffer* s_materialBuffer; // Structured Buffer
	static DescriptorHandle s_materialBufferSRV;

public:
	~GameMaterialDefinition() = default;
	bool LoadFromXmlElement(XmlElement const& element);

public:
	std::string m_name = "";

	// Visuals
	uint32_t m_albedoSrvIndex = INVALID_INDEX_U32;
	uint32_t m_normalSrvIndex = INVALID_INDEX_U32;
	uint32_t m_ormhSrvIndex = INVALID_INDEX_U32; // o-AO r-roughness m-metallic h-height
	uint32_t m_emissiveSrvIndex = INVALID_INDEX_U32;
	// SamplerState is fixed in shader (BilinearWarp)
	
	float m_uvScale = 1.f;
	float m_lowPolyLevel = 0.f;




};

