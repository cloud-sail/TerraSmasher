#include "Game/ShipDefinition.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/StaticMeshUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Math/Quantization.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"

void ShipDefinition::InitializeDefinitions(const char* path /*= "Data/Definitions/ShipDefinitions.xml"*/)
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
		GUARANTEE_OR_DIE(elementName == "ShipDefinition", Stringf("Root child element in %s was <%s>, must be <ShipDefinition>!", path, elementName.c_str()));
		ShipDefinition* newDef = new ShipDefinition();

		newDef->LoadFromXmlElement(*defElement);
		s_definitions.push_back(newDef);

		defElement = defElement->NextSiblingElement();
	}

}

void ShipDefinition::ClearDefinitions()
{
	for (int i = 0; i < static_cast<int>(s_definitions.size()); ++i)
	{
		delete s_definitions[i];
		s_definitions[i] = nullptr;
	}

	s_definitions.clear();
}

ShipDefinition const* ShipDefinition::GetByName(std::string const& defName)
{
	for (int i = 0; i < (int)s_definitions.size(); ++i)
	{
		if (s_definitions[i]->m_name == defName)
		{
			return s_definitions[i];
		}
	}

	ERROR_AND_DIE(Stringf("Ship is not in ShipDefinitions: \"%s\"", defName.c_str()));
}

ShipDefinition::~ShipDefinition()
{
	delete m_shipVB;
	m_shipVB = nullptr;

	delete m_shipIB;
	m_shipIB = nullptr;
}

bool ShipDefinition::LoadFromXmlElement(XmlElement const& element)
{
	m_name = ParseXmlAttribute(element, "name", m_name);
	
	m_shipVB = g_theRenderer->CreateVertexBuffer(1 * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	m_shipIB = g_theRenderer->CreateIndexBuffer(1 * sizeof(unsigned int));

	if (!TryLoadStaticMesh(ParseXmlAttribute(element, "model", "INVALID_PATH")))
	{
		LoadDebugStaticMesh();
	}


	//-----------------------------------------------------------------------------------------------
	// Child Elements
	// Physics
	{
		XmlElement const* physicsElement = element.FirstChildElement("Physics");

		if (physicsElement)
		{
			m_topSpeed			= ParseXmlAttribute(*physicsElement, "topSpeed", m_topSpeed);
			m_secondsToTopSpeed = ParseXmlAttribute(*physicsElement, "secondsToTopSpeed", m_secondsToTopSpeed);
			m_secondsToStop		= ParseXmlAttribute(*physicsElement, "secondsToStop", m_secondsToStop);
			m_lateralDragRate	= ParseXmlAttribute(*physicsElement, "lateralDragRate", m_lateralDragRate);
			m_turnRate			= ParseXmlAttribute(*physicsElement, "turnRate", m_turnRate);

			m_collisionRadius = ParseXmlAttribute(*physicsElement, "collisionRadius", m_collisionRadius);
			m_collisionBounceCoefficient = ParseXmlAttribute(*physicsElement, "bounceCoefficient", m_collisionBounceCoefficient);
			m_collisionFrictionCoefficient = ParseXmlAttribute(*physicsElement, "frictionCoefficient", m_collisionFrictionCoefficient);

			m_barrelRollDuration = ParseXmlAttribute(*physicsElement, "barrelRollDuration", m_barrelRollDuration);
			m_barrelRollCooldown = ParseXmlAttribute(*physicsElement, "barrelRollCooldown", m_barrelRollCooldown);
			m_barrelRollBodyOffset = ParseXmlAttribute(*physicsElement, "barrelRollBodyOffset", m_barrelRollBodyOffset);
			m_barrelRollLateralSpeed = ParseXmlAttribute(*physicsElement, "barrelRollLateralSpeed", m_barrelRollLateralSpeed);
		}
	}

	// Acceleration Curve
	{
		XmlElement const* accelerationCurveElement = element.FirstChildElement("AccelerationCurve");
		if (accelerationCurveElement)
		{
			m_accelerationCurve.Clear();
			XmlElement const* pointElement = accelerationCurveElement->FirstChildElement("Point");
			while (pointElement)
			{
				float x = ParseXmlAttribute(*pointElement, "x", 0.f);
				float y = ParseXmlAttribute(*pointElement, "y", 0.f);

				m_accelerationCurve.AddPoint(x, y);

				pointElement = pointElement->NextSiblingElement("Point");
			}
		}
		else
		{
			m_accelerationCurve.Clear();
			m_accelerationCurve.AddPoint(0.0f, 0.0f);
			m_accelerationCurve.AddPoint(1.0f, 1.0f);
		}	
	}

	// Deceleration Curve
	{
		XmlElement const* decelerationCurveElement = element.FirstChildElement("DecelerationCurve");
		if (decelerationCurveElement)
		{
			m_decelerationCurve.Clear();
			XmlElement const* pointElement = decelerationCurveElement->FirstChildElement("Point");
			while (pointElement)
			{
				float x = ParseXmlAttribute(*pointElement, "x", 0.f);
				float y = ParseXmlAttribute(*pointElement, "y", 0.f);

				m_decelerationCurve.AddPoint(x, y);

				pointElement = pointElement->NextSiblingElement("Point");
			}
		}
		else
		{
			m_decelerationCurve.Clear();
			m_decelerationCurve.AddPoint(0.0f, 1.0f);
			m_decelerationCurve.AddPoint(1.0f, 0.0f);
		}
	}

	// Visuals
	{
		XmlElement const* visualsElement = element.FirstChildElement("Visuals");

		if (visualsElement)
		{
			m_bodyHorizontalOffsetMultiplier = ParseXmlAttribute(*visualsElement, "bodyHorizontalOffsetMultiplier", m_bodyHorizontalOffsetMultiplier);


			XmlElement const* currElement = visualsElement->FirstChildElement("MidTransform");
			if (currElement)
			{
				Vec3 position = ParseXmlAttribute(*currElement, "position", Vec3::ZERO);
				EulerAngles rotation = ParseXmlAttribute(*currElement, "rotation", EulerAngles());
				Mat44 rotMat = rotation.GetAsMatrix_IFwd_JLeft_KUp();

				m_visualPositionMiddle = position;
				m_visualRotationMiddle = rotMat.GetQuat().GetNormalized();
			}

			currElement = visualsElement->FirstChildElement("LeftTransform");
			if (currElement)
			{
				Vec3 position = ParseXmlAttribute(*currElement, "position", Vec3::ZERO);
				EulerAngles rotation = ParseXmlAttribute(*currElement, "rotation", EulerAngles());
				Mat44 rotMat = rotation.GetAsMatrix_IFwd_JLeft_KUp();

				m_visualPositionLeft = position;
				m_visualRotationLeft = rotMat.GetQuat().GetNormalized();
			}

			currElement = visualsElement->FirstChildElement("RightTransform");
			if (currElement)
			{
				Vec3 position = ParseXmlAttribute(*currElement, "position", Vec3::ZERO);
				EulerAngles rotation = ParseXmlAttribute(*currElement, "rotation", EulerAngles());
				Mat44 rotMat = rotation.GetAsMatrix_IFwd_JLeft_KUp();

				m_visualPositionRight = position;
				m_visualRotationRight = rotMat.GetQuat().GetNormalized();
			}

			currElement = visualsElement->FirstChildElement("UpTransform");
			if (currElement)
			{
				Vec3 position = ParseXmlAttribute(*currElement, "position", Vec3::ZERO);
				EulerAngles rotation = ParseXmlAttribute(*currElement, "rotation", EulerAngles());
				Mat44 rotMat = rotation.GetAsMatrix_IFwd_JLeft_KUp();

				m_visualPositionUp = position;
				m_visualRotationUp = rotMat.GetQuat().GetNormalized();
			}

			currElement = visualsElement->FirstChildElement("DownTransform");
			if (currElement)
			{
				Vec3 position = ParseXmlAttribute(*currElement, "position", Vec3::ZERO);
				EulerAngles rotation = ParseXmlAttribute(*currElement, "rotation", EulerAngles());
				Mat44 rotMat = rotation.GetAsMatrix_IFwd_JLeft_KUp();

				m_visualPositionDown = position;
				m_visualRotationDown = rotMat.GetQuat().GetNormalized();
			}

		}
	
	}

	// Camera
	{
		XmlElement const* cameraElement = element.FirstChildElement("Camera");

		if (cameraElement)
		{
			m_minCameraFOV			= ParseXmlAttribute(*cameraElement, "minFOV", m_minCameraFOV);
			m_maxCameraFOV			= ParseXmlAttribute(*cameraElement, "maxFOV", m_maxCameraFOV);
			m_minCameraDistance		= ParseXmlAttribute(*cameraElement, "minCameraDist", m_minCameraDistance);
			m_maxCameraDistance		= ParseXmlAttribute(*cameraElement, "maxCameraDist", m_maxCameraDistance);
		}
	}

	// Cannons
	{
		XmlElement const* cannonsElement = element.FirstChildElement("Cannons");

		if (cannonsElement)
		{
			m_cannonOffsets.clear();
			m_cannonColors.clear();
			m_cannonIntensities.clear();

			float damage = ParseXmlAttribute(*cannonsElement, "damage", 1.f);
			m_cannonDeltaDensity = Quantization::ToUint8FromUNorm(damage);
			m_cannonFireInterval		= ParseXmlAttribute(*cannonsElement, "fireInterval", m_cannonFireInterval);
			m_cannonLifetime			= ParseXmlAttribute(*cannonsElement, "lifetime", m_cannonLifetime);
			m_cannonSpeed				= ParseXmlAttribute(*cannonsElement, "speed", m_cannonSpeed);
			m_cannonExplosionRadius		= ParseXmlAttribute(*cannonsElement, "explosionRadius", m_cannonExplosionRadius);



			XmlElement const* cannonElement = cannonsElement->FirstChildElement("Cannon");
			while (cannonElement)
			{
				Vec3 offset = ParseXmlAttribute(*cannonElement, "offset", Vec3::ZERO);
				Rgba8 color = ParseXmlAttribute(*cannonElement, "color", Rgba8::RED);
				float intensity = ParseXmlAttribute(*cannonElement, "intensity", 2.0f);

				m_cannonOffsets.push_back(offset);
				m_cannonColors.push_back(color);
				m_cannonIntensities.push_back(intensity);

				cannonElement = cannonElement->NextSiblingElement("Cannon");
			}

			m_numCannons = (int)m_cannonOffsets.size();
		}
	}

	// Sonar
	{
		XmlElement const* sonarElement = element.FirstChildElement("Sonar");

		if (sonarElement)
		{
			m_sonarColor		= ParseXmlAttribute(*sonarElement, "color", m_sonarColor);
			m_sonarMaxRadius	= ParseXmlAttribute(*sonarElement, "maxRadius", m_sonarMaxRadius);
			m_sonarScanDuration = ParseXmlAttribute(*sonarElement, "scanDuration", m_sonarScanDuration);
			m_sonarScanInterval = ParseXmlAttribute(*sonarElement, "scanInterval", m_sonarScanInterval);
		}
	}

	return true;
}

bool ShipDefinition::TryLoadStaticMesh(std::string const& filePath)
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
	m_shipShader = g_theRenderer->CreateOrGetShader(ShaderConfig(modelInfo.m_shaderName.c_str()), VertexType::VERTEX_PCUTBN);

	
	if (!modelInfo.m_diffuseMapFilePath.empty())
	{
		m_shipDiffuseTexture = g_theRenderer->CreateOrGetTextureFromFile(modelInfo.m_diffuseMapFilePath.c_str());
	}
	else
	{
		m_shipDiffuseTexture = nullptr;
	}
	
	if (!modelInfo.m_normalMapFilePath.empty())
	{
		m_shipNormalTexture = g_theRenderer->CreateOrGetTextureFromFile(modelInfo.m_normalMapFilePath.c_str());
	}
	else
	{
		m_shipNormalTexture = nullptr;
	}

	if (!modelInfo.m_specGlossEmitMapFilePath.empty())
	{
		m_shipSGETexture = g_theRenderer->CreateOrGetTextureFromFile(modelInfo.m_specGlossEmitMapFilePath.c_str());
	}
	else
	{
		m_shipSGETexture = nullptr;
	}

	indexes.reserve(verts.size());
	for (int i = 0; i < (int)verts.size(); ++i)
	{
		indexes.push_back(i);
	}

	g_theRenderer->CopyCPUToGPU(verts.data(), static_cast<unsigned int>(verts.size()) * m_shipVB->GetStride(), m_shipVB);
	g_theRenderer->CopyCPUToGPU(indexes.data(), static_cast<unsigned int>(indexes.size()) * m_shipIB->GetStride(), m_shipIB);

	return true;
}

void ShipDefinition::LoadDebugStaticMesh()
{
	std::vector<Vertex_PCUTBN> verts;
	std::vector<unsigned int> indexes;

	AddVertsForCylinderZ3D(verts, indexes, Vec2(0.f, 0.f), FloatRange(-0.1f, 0.1f), 1.f, 16, Rgba8::GREEN);
	AddVertsForSphere3D(verts, indexes, Vec3(0.f, 0.f, 0.f), 0.4f, Rgba8::YELLOW, AABB2::ZERO_TO_ONE, 16, 8);
	AddVertsForCylinder3D(verts, indexes, Vec3(0.f, 0.9f, 0.f), Vec3(1.f, 0.9f, 0.f), 0.1f, Rgba8::RED, AABB2::ZERO_TO_ONE, 8);
	AddVertsForCylinder3D(verts, indexes, Vec3(0.f, -0.9f, 0.f), Vec3(1.f, -0.9f, 0.f), 0.1f, Rgba8::RED, AABB2::ZERO_TO_ONE, 8);


	g_theRenderer->CopyCPUToGPU(verts.data(), static_cast<unsigned int>(verts.size()) * m_shipVB->GetStride(), m_shipVB);
	g_theRenderer->CopyCPUToGPU(indexes.data(), static_cast<unsigned int>(indexes.size()) * m_shipIB->GetStride(), m_shipIB);

	m_shipShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/BlinnPhong"), VertexType::VERTEX_PCUTBN);
}
