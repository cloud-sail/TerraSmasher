#pragma once
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Math/MonotonicCurve.hpp"
#include "Engine/Math/Quat.hpp"
#include "Engine/Math/Vec3.hpp"
#include <vector>
#include <string>


class IndexBuffer;
class VertexBuffer;
class Texture;
class Shader;

class ShipDefinition
{
public:
	static void InitializeDefinitions(const char* path = "Data/Definitions/ShipDefinitions.xml");
	static void ClearDefinitions();
	static ShipDefinition const* GetByName(std::string const& defName);
	inline static std::vector<ShipDefinition*> s_definitions;

private:
	~ShipDefinition();

private:
	bool LoadFromXmlElement(XmlElement const& element);
	
	

	bool TryLoadStaticMesh(std::string const& filePath);
	void LoadDebugStaticMesh();


public:
	std::string m_name = "UNKNOWN";

	// Physics
	float m_topSpeed = 20.0f;
	float m_secondsToTopSpeed = 2.0f;
	float m_secondsToStop = 1.5f;
	float m_lateralDragRate = 5.f;
	float m_turnRate = 105.f; // Degrees per seconds

	MonotonicCurve m_accelerationCurve; // time ratio (0~1) vs. speed ratio(0~1)
	MonotonicCurve m_decelerationCurve; // time ratio (0~1) vs. speed ratio(0~1)

	// Camera
	float m_minCameraFOV = 60.f;
	float m_maxCameraFOV = 78.f;
	float m_minCameraDistance = 2.55f;
	float m_maxCameraDistance = 3.3f;


	// Visual
	float m_bodyHorizontalOffsetMultiplier = 2.5f;

	Vec3 m_visualPositionMiddle;
	Quat m_visualRotationMiddle;

	Vec3 m_visualPositionLeft;
	Quat m_visualRotationLeft;

	Vec3 m_visualPositionRight;
	Quat m_visualRotationRight;

	Vec3 m_visualPositionUp;
	Quat m_visualRotationUp;

	Vec3 m_visualPositionDown;
	Quat m_visualRotationDown;

	// Cannons
	int m_numCannons = 0;
	uint8_t m_cannonDeltaDensity = 255;
	float m_cannonFireInterval = 0.15f;
	float m_cannonLifetime = 0.5f;
	float m_cannonSpeed = 100.f;
	float m_cannonExplosionRadius = 5.f;

	std::vector<Vec3> m_cannonOffsets;
	std::vector<Rgba8> m_cannonColors;
	std::vector<float> m_cannonIntensities;

public:
	// model,  The expected size is 2^3, pivot is defined by obj file
	VertexBuffer* m_shipVB = nullptr; // VERTEX_PCUTBN
	IndexBuffer* m_shipIB = nullptr;

	Texture* m_shipDiffuseTexture = nullptr;
	Texture* m_shipNormalTexture = nullptr; // If empty, set as nullptr
	Texture* m_shipSGETexture = nullptr;

	Shader* m_shipShader = nullptr; // BlinnPhong #ToDo Add Normal Map, SGE Map
};

