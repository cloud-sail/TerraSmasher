#include "Game/SDF.hpp"
#include "Game/Voxel.hpp"
#include "Game/DensityCloud.hpp"
#include "Game/SdfPrimitives.hpp"
#include "Game/ImGuiUtils.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Renderer/Renderer.hpp"

#include <algorithm>

#include "ThirdParty/imgui/imgui.h"

float SDF::GetSignedDistance(Vec3 const& worldPos) const
{
	Vec3 localPos = m_transform.TransformWorldToLocal(worldPos);
	float localDistance = ComputeLocalSDF(localPos);
	return localDistance * m_transform.m_uniformScale;
}

AABB3 SDF::GetAffectedAABB() const
{
	AABB3 localAABB = ComputeLocalAABB();

	AABB3 worldAABB = m_transform.TransformLocalToWorldAABB(localAABB);

	Vec3 expansion(Voxel::SDF_NORM_THRESHOLD, Voxel::SDF_NORM_THRESHOLD, Voxel::SDF_NORM_THRESHOLD);

	worldAABB.m_maxs += expansion;
	worldAABB.m_mins -= expansion;

	return worldAABB;
}

uint8_t SDF::GetPointCloudDensity(Vec3 const& worldPos) const
{
	UNUSED(worldPos);
	return 0;
}

//-----------------------------------------------------------------------------------------------
float SDFSphere::ComputeLocalSDF(Vec3 const& localPos) const
{
	return localPos.GetLength() - m_radius;
}

AABB3 SDFSphere::ComputeLocalAABB() const
{
	Vec3 halfDimensions(m_radius, m_radius, m_radius);
	return AABB3(-halfDimensions, halfDimensions);
}

void SDFSphere::AddWorldVerts(std::vector<Vertex_PCU>& worldVerts, Rgba8 const& color /*= Rgba8::OPAQUE_WHITE*/) const
{
	std::vector<Vertex_PCU> localVerts;

	AddVertsForSphere3D(localVerts, Vec3::ZERO, m_radius, color, AABB2::ZERO_TO_ONE, 16, 8);

	// Local To World
	TransformVertexArray3D(localVerts, m_transform.GetAsMatrix());

	worldVerts.insert(worldVerts.end(),
		localVerts.begin(),
		localVerts.end());
}

void SDFSphere::WritePropertiesToXml(XmlElement& element) const
{
	element.SetAttribute("radius", m_radius);
}

void SDFSphere::ReadPropertiesFromXml(XmlElement const& element)
{
	m_radius = ParseXmlAttribute(element, "radius", 1.0f);
}

//-----------------------------------------------------------------------------------------------
float SDFBox::ComputeLocalSDF(Vec3 const& localPos) const
{
	Vec3 const& p = localPos;
	Vec3 const& h = m_halfExtents;

	Vec3 q = p.GetAbs() - h;

	float outside = Vec3(std::max(q.x, 0.f), std::max(q.y, 0.f), std::max(q.z, 0.f)).GetLength();
	float inside = std::min(std::max(q.x, std::max(q.y, q.z)), 0.f);

	return outside + inside;
}

AABB3 SDFBox::ComputeLocalAABB() const
{
	return AABB3(-m_halfExtents, m_halfExtents);
}

void SDFBox::AddWorldVerts(std::vector<Vertex_PCU>& worldVerts, Rgba8 const& color /*= Rgba8::OPAQUE_WHITE*/) const
{
	std::vector<Vertex_PCU> localVerts;

	AddVertsForAABB3D(localVerts, AABB3(-m_halfExtents, m_halfExtents), color);

	// Local To World
	TransformVertexArray3D(localVerts, m_transform.GetAsMatrix());

	worldVerts.insert(worldVerts.end(),
		localVerts.begin(),
		localVerts.end());
}

void SDFBox::WritePropertiesToXml(XmlElement& element) const
{
	element.SetAttribute("halfExtents", Stringf("%.3f,%.3f,%.3f", m_halfExtents.x, m_halfExtents.y, m_halfExtents.z).c_str());
}

void SDFBox::ReadPropertiesFromXml(XmlElement const& element)
{
	m_halfExtents = ParseXmlAttribute(element, "halfExtents", Vec3(1.f, 1.f, 1.f));
}

//-----------------------------------------------------------------------------------------------
float SDFTorus::ComputeLocalSDF(Vec3 const& localPos) const
{
	return SdfHelper::Torus(localPos, m_majorRadius, m_minorRadius);
}

AABB3 SDFTorus::ComputeLocalAABB() const
{
	float outerRadius = m_majorRadius + m_minorRadius;
	Vec3 halfDimensions(outerRadius, outerRadius, m_minorRadius);
	return AABB3(-halfDimensions, halfDimensions);
}

void SDFTorus::AddWorldVerts(std::vector<Vertex_PCU>& worldVerts, Rgba8 const& color /*= Rgba8::OPAQUE_WHITE*/) const
{
	std::vector<Vertex_PCU> localVerts;

	// Generate torus vertices
	int numMajorSegments = 16;
	int numMinorSegments = 8;

	AddVertsForTorusZ3D(localVerts, m_majorRadius, m_minorRadius, numMajorSegments, numMinorSegments, color);

	// Local To World
	TransformVertexArray3D(localVerts, m_transform.GetAsMatrix());

	worldVerts.insert(worldVerts.end(),
		localVerts.begin(),
		localVerts.end());
}

void SDFTorus::WritePropertiesToXml(XmlElement& element) const
{
	element.SetAttribute("majorRadius", m_majorRadius);
	element.SetAttribute("minorRadius", m_minorRadius);
}

void SDFTorus::ReadPropertiesFromXml(XmlElement const& element)
{
	m_majorRadius = ParseXmlAttribute(element, "majorRadius", 2.0f);
	m_minorRadius = ParseXmlAttribute(element, "minorRadius", 0.5f);
}

//-----------------------------------------------------------------------------------------------
float SDFCapsule::ComputeLocalSDF(Vec3 const& localPos) const
{
	return SdfHelper::Capsule(localPos, m_pointA, m_pointB, m_radius);
}

AABB3 SDFCapsule::ComputeLocalAABB() const
{
	Vec3 mins(
		std::min(m_pointA.x, m_pointB.x) - m_radius,
		std::min(m_pointA.y, m_pointB.y) - m_radius,
		std::min(m_pointA.z, m_pointB.z) - m_radius
	);
	Vec3 maxs(
		std::max(m_pointA.x, m_pointB.x) + m_radius,
		std::max(m_pointA.y, m_pointB.y) + m_radius,
		std::max(m_pointA.z, m_pointB.z) + m_radius
	);
	return AABB3(mins, maxs);
}

void SDFCapsule::AddWorldVerts(std::vector<Vertex_PCU>& worldVerts, Rgba8 const& color /*= Rgba8::OPAQUE_WHITE*/) const
{
	std::vector<Vertex_PCU> localVerts;

	AddVertsForCapsule3D(localVerts, m_pointA, m_pointB, m_radius, 16, 8, color);

	// Local To World
	TransformVertexArray3D(localVerts, m_transform.GetAsMatrix());

	worldVerts.insert(worldVerts.end(),
		localVerts.begin(),
		localVerts.end());
}

void SDFCapsule::WritePropertiesToXml(XmlElement& element) const
{
	element.SetAttribute("pointA", Stringf("%.3f,%.3f,%.3f", m_pointA.x, m_pointA.y, m_pointA.z).c_str());
	element.SetAttribute("pointB", Stringf("%.3f,%.3f,%.3f", m_pointB.x, m_pointB.y, m_pointB.z).c_str());
	element.SetAttribute("radius", m_radius);
}

void SDFCapsule::ReadPropertiesFromXml(XmlElement const& element)
{
	m_pointA = ParseXmlAttribute(element, "pointA", Vec3(0.f, 0.f, -1.f));
	m_pointB = ParseXmlAttribute(element, "pointB", Vec3(0.f, 0.f, 1.f));
	m_radius = ParseXmlAttribute(element, "radius", 0.5f);
}

//-----------------------------------------------------------------------------------------------
float SDFCutSphere::ComputeLocalSDF(Vec3 const& localPos) const
{
	return SdfHelper::CutSphere(localPos, m_radius, m_cutHeight);
}

AABB3 SDFCutSphere::ComputeLocalAABB() const
{
	if (m_cutHeight <= 0.0f)
	{
		return AABB3(Vec3(-m_radius, -m_radius, m_cutHeight), Vec3(m_radius, m_radius, m_radius));
	}
	else
	{
		float w = sqrtf(m_radius * m_radius - m_cutHeight * m_cutHeight);
		return AABB3(Vec3(-w, -w, m_cutHeight), Vec3(w, w, m_radius));
	}
}

void SDFCutSphere::AddWorldVerts(std::vector<Vertex_PCU>& worldVerts, Rgba8 const& color /*= Rgba8::OPAQUE_WHITE*/) const
{
	std::vector<Vertex_PCU> localVerts;

	AddVertsForSphere3D(localVerts, Vec3::ZERO, m_radius, color, AABB2::ZERO_TO_ONE, 16, 8);

	// Local To World
	TransformVertexArray3D(localVerts, m_transform.GetAsMatrix());

	worldVerts.insert(worldVerts.end(),
		localVerts.begin(),
		localVerts.end());
}

void SDFCutSphere::SetRadius(float r)
{
	if (r > 0.f)
	{
		m_radius = r;
		m_cutHeight = GetClamped(m_cutHeight, -m_radius, m_radius);
	}
}

void SDFCutSphere::SetCutHeight(float h)
{
	m_cutHeight = h;
	m_cutHeight = GetClamped(m_cutHeight, -m_radius, m_radius);
}

void SDFCutSphere::WritePropertiesToXml(XmlElement& element) const
{
	element.SetAttribute("radius", m_radius);
	element.SetAttribute("cutHeight", m_cutHeight);
}

void SDFCutSphere::ReadPropertiesFromXml(XmlElement const& element)
{
	m_radius = ParseXmlAttribute(element, "radius", 1.0f);
	m_cutHeight = ParseXmlAttribute(element, "cutHeight", 0.5f);
}

//-----------------------------------------------------------------------------------------------
float SDFCutHollowSphere::ComputeLocalSDF(Vec3 const& localPos) const
{
	return SdfHelper::CutHollowSphere(localPos, m_radius, m_cutHeight, m_thickness);
}

AABB3 SDFCutHollowSphere::ComputeLocalAABB() const
{
	if (m_cutHeight <= 0.0f)
	{
		float w = sqrtf(m_radius * m_radius - m_cutHeight * m_cutHeight);

		return AABB3(Vec3(-(w + m_thickness), -(w + m_thickness), -(m_radius + m_thickness)),
			Vec3((w + m_thickness), (w + m_thickness), m_cutHeight + m_thickness));
	}
	else
	{
		return AABB3(Vec3(-(m_radius + m_thickness), -(m_radius + m_thickness), -(m_radius + m_thickness)),
			Vec3((m_radius + m_thickness), (m_radius + m_thickness), m_cutHeight + m_thickness));
	}


}

void SDFCutHollowSphere::AddWorldVerts(std::vector<Vertex_PCU>& worldVerts, Rgba8 const& color /*= Rgba8::OPAQUE_WHITE*/) const
{
	std::vector<Vertex_PCU> localVerts;

	AddVertsForSphere3D(localVerts, Vec3::ZERO, m_radius, color, AABB2::ZERO_TO_ONE, 16, 8);

	// Local To World
	TransformVertexArray3D(localVerts, m_transform.GetAsMatrix());

	worldVerts.insert(worldVerts.end(),
		localVerts.begin(),
		localVerts.end());
}

void SDFCutHollowSphere::SetRadius(float r)
{
	if (r > 0.f)
	{
		m_radius = r;
		m_cutHeight = GetClamped(m_cutHeight, -m_radius, m_radius);
		m_thickness = GetClamped(m_thickness, 0.01f, m_radius * 0.9f);
	}
}

void SDFCutHollowSphere::SetCutHeight(float h)
{
	m_cutHeight = GetClamped(h, -m_radius, m_radius);
}

void SDFCutHollowSphere::SetThickness(float t)
{
	m_thickness = GetClamped(t, 0.01f, m_radius * 0.9f);
}

void SDFCutHollowSphere::WritePropertiesToXml(XmlElement& element) const
{
	element.SetAttribute("radius", m_radius);
	element.SetAttribute("cutHeight", m_cutHeight);
	element.SetAttribute("thickness", m_thickness);
}

void SDFCutHollowSphere::ReadPropertiesFromXml(XmlElement const& element)
{
	m_radius = ParseXmlAttribute(element, "radius", 1.0f);
	m_cutHeight = ParseXmlAttribute(element, "cutHeight", 0.5f);
	m_thickness = ParseXmlAttribute(element, "thickness", 0.1f);
}

//-----------------------------------------------------------------------------------------------
SDFDensityCloud::SDFDensityCloud()
{
	m_isPointCloud = true;
}

SDFDensityCloud::~SDFDensityCloud()
{
	delete m_densityCloud;
	m_densityCloud = nullptr;
}

AABB3 SDFDensityCloud::GetAffectedAABB() const
{
	AABB3 result = ComputeLocalAABB();

	result.Translate(GetFlooredPosition()); // Point Cloud does not have rotation and scale

	return result;
}

uint8_t SDFDensityCloud::GetPointCloudDensity(Vec3 const& worldPos) const
{
	if (!m_densityCloud || !m_densityCloud->IsValid())
	{
		return 0;
	}

	Vec3 flooredPos = GetFlooredPosition();
	Vec3 localPos = worldPos - flooredPos;

	int x = static_cast<int>(std::floor(localPos.x));
	int y = static_cast<int>(std::floor(localPos.y));
	int z = static_cast<int>(std::floor(localPos.z));

	return m_densityCloud->GetDensity(x, y, z);
}

bool SDFDensityCloud::LoadFromFile(const std::string& filePath)
{
	m_filePath = filePath;
	if (!m_densityCloud)
	{
		m_densityCloud = new DensityCloud();
	}
	return m_densityCloud->LoadFromFile(filePath);
}

void SDFDensityCloud::WritePropertiesToXml(XmlElement& element) const
{
	element.SetAttribute("filePath", m_filePath.c_str());
}

void SDFDensityCloud::ReadPropertiesFromXml(XmlElement const& element)
{
	m_filePath = ParseXmlAttribute(element, "filePath", std::string(""));
	if (!m_filePath.empty())
	{
		LoadFromFile(m_filePath);
	}
}

Vec3 SDFDensityCloud::GetFlooredPosition() const
{
	Vec3 pos = m_transform.m_position;
	return Vec3(std::floor(pos.x), std::floor(pos.y), std::floor(pos.z));
}

float SDFDensityCloud::ComputeLocalSDF(Vec3 const& localPos) const
{
	UNUSED(localPos);
	ERROR_AND_DIE("SDFDensityCloud cannot compute sdf.");
}

AABB3 SDFDensityCloud::ComputeLocalAABB() const
{
	if (!m_densityCloud || !m_densityCloud->IsValid())
	{
		return AABB3(Vec3::ZERO, Vec3::ZERO);
	}

	Vec3 maxs(
		static_cast<float>(m_densityCloud->GetSizeX()),
		static_cast<float>(m_densityCloud->GetSizeY()),
		static_cast<float>(m_densityCloud->GetSizeZ())
	);

	return AABB3(Vec3::ZERO, maxs);
}

void SDFDensityCloud::AddWorldVerts(std::vector<Vertex_PCU>& worldVerts, Rgba8 const& color /*= Rgba8::OPAQUE_WHITE*/) const
{
	// Use AABB3 bounding box to visualize the density cloud
	AABB3 worldAABB = GetAffectedAABB();

	std::vector<Vertex_PCU> appendVerts;
	AddVertsForAABB3D(appendVerts, worldAABB, color);

	worldVerts.insert(worldVerts.end(), appendVerts.begin(), appendVerts.end());
}

//-----------------------------------------------------------------------------------------------
void DrawSDFDetail(SDF* sdf)
{
	if (!sdf) return;
	ImGui::PushID(sdf);


	ImGui::Text("Shape: %s", sdf->GetShapeName());

	// Transform
	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) 
	{
		GameTransform& transform = sdf->GetMutTransform();

		DrawVec3Control("Position", transform.m_position);

		DrawEulerAnglesControl("Rotation", transform.m_orientation);

		DrawFloatControl("Scale", transform.m_uniformScale, 1.f);
	}

	// #ToDo different types

	// Shape parameters
	if (ImGui::CollapsingHeader("Shape Parameters", ImGuiTreeNodeFlags_DefaultOpen)) 
	{

		if (SDFSphere* sphere = dynamic_cast<SDFSphere*>(sdf)) 
		{
			float radius = sphere->GetRadius();
			if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 25.0f)) 
			{
				sphere->SetRadius(radius);
			}
		}
		else if (SDFBox* box = dynamic_cast<SDFBox*>(sdf)) 
		{
			Vec3 extents = box->GetHalfExtents();
			if (ImGui::DragFloat3("Half Extents", &extents.x, 0.01f, 0.01f, 10.0f)) 
			{
				box->SetHalfExtents(extents);
			}
		}
		else if (SDFTorus* torus = dynamic_cast<SDFTorus*>(sdf))
		{
			float majorRadius = torus->GetMajorRadius();
			float minorRadius = torus->GetMinorRadius();
			if (ImGui::DragFloat("Major Radius", &majorRadius, 0.01f, 0.01f, 25.0f))
			{
				torus->SetMajorRadius(majorRadius);
			}
			if (ImGui::DragFloat("Minor Radius", &minorRadius, 0.01f, 0.01f, 10.0f))
			{
				torus->SetMinorRadius(minorRadius);
			}
		}
		else if (SDFCapsule* capsule = dynamic_cast<SDFCapsule*>(sdf))
		{
			Vec3 pointA = capsule->GetPointA();
			Vec3 pointB = capsule->GetPointB();
			float radius = capsule->GetRadius();
			if (ImGui::DragFloat3("Point A", &pointA.x, 0.01f, -10.0f, 10.0f))
			{
				capsule->SetPointA(pointA);
			}
			if (ImGui::DragFloat3("Point B", &pointB.x, 0.01f, -10.0f, 10.0f))
			{
				capsule->SetPointB(pointB);
			}
			if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 10.0f))
			{
				capsule->SetRadius(radius);
			}
		}
		else if (SDFCutSphere* cutSphere = dynamic_cast<SDFCutSphere*>(sdf))
		{
			float radius = cutSphere->GetRadius();
			float cutHeight = cutSphere->GetCutHeight();
			if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 25.0f))
			{
				cutSphere->SetRadius(radius);
			}
			if (ImGui::DragFloat("Cut Height", &cutHeight, 0.01f, -radius, radius))
			{
				cutSphere->SetCutHeight(cutHeight);
			}
		}
		else if (SDFCutHollowSphere* cutHollowSphere = dynamic_cast<SDFCutHollowSphere*>(sdf))
		{
			float radius = cutHollowSphere->GetRadius();
			float cutHeight = cutHollowSphere->GetCutHeight();
			float thickness = cutHollowSphere->GetThickness();
			if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.01f, 25.0f))
			{
				cutHollowSphere->SetRadius(radius);
			}
			if (ImGui::DragFloat("Cut Height", &cutHeight, 0.01f, -radius, radius))
			{
				cutHollowSphere->SetCutHeight(cutHeight);
			}
			if (ImGui::DragFloat("Thickness", &thickness, 0.01f, 0.01f, radius * 0.9f))
			{
				cutHollowSphere->SetThickness(thickness);
			}
		}
		else if (SDFDensityCloud* densityCloud = dynamic_cast<SDFDensityCloud*>(sdf))
		{
			ImGui::Text("File Path: %s", densityCloud->GetFilePath().c_str());
			if (densityCloud->GetDensityCloud() && densityCloud->GetDensityCloud()->IsValid())
			{
				ImGui::Text("Dimensions: %d x %d x %d",
					densityCloud->GetDensityCloud()->GetSizeX(),
					densityCloud->GetDensityCloud()->GetSizeY(),
					densityCloud->GetDensityCloud()->GetSizeZ());
				ImGui::Text("Total Voxels: %d", densityCloud->GetDensityCloud()->GetTotalVoxels());
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Failed to load density cloud file!");
			}
			ImGui::Separator();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Note: Position will be floored to integer");
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Rotation and Scale are ignored");
		}
	}


	if (ImGui::CollapsingHeader("Affected AABB Region")) 
	{
		AABB3 aabb = sdf->GetAffectedAABB();
		ImGui::Text("Min: (%.2f, %.2f, %.2f)", aabb.m_mins.x, aabb.m_mins.y, aabb.m_mins.z);
		ImGui::Text("Max: (%.2f, %.2f, %.2f)", aabb.m_maxs.x, aabb.m_maxs.y, aabb.m_maxs.z);
	}

	ImGui::Separator();
	ImGui::PopID();
}

//-----------------------------------------------------------------------------------------------
SDFShape::SDFShape(SDF* sdf)
	: m_sdf(sdf)
{

}

//SDFShape::SDFShape(SDFShape&& other) noexcept
//	: m_sdf(other.m_sdf)
//	, m_fillMethod(other.m_fillMethod)
//	, m_materialID1(other.m_materialID1)
//	, m_materialID2(other.m_materialID2)
//	, m_uniformBlendValue(other.m_uniformBlendValue)
//	, m_gradientMinZFactor(other.m_gradientMinZFactor)
//	, m_gradientMaxZFactor(other.m_gradientMaxZFactor)
//	, m_gradientMinDist(other.m_gradientMinDist)
//	, m_gradientMaxDist(other.m_gradientMaxDist)
//{
//	other.m_sdf = nullptr;
//	// #ToDo Move Preview Mesh
//
//}
//
//SDFShape& SDFShape::operator=(SDFShape&& other) noexcept
//{
//	if (this != &other)
//	{
//		DestroyPreviewMesh();
//
//		m_sdf = other.m_sdf;
//		m_fillMethod = other.m_fillMethod;
//		m_materialID1 = other.m_materialID1;
//		m_materialID2 = other.m_materialID2;
//		m_uniformBlendValue = other.m_uniformBlendValue;
//		m_gradientMinZFactor = other.m_gradientMinZFactor;
//		m_gradientMaxZFactor = other.m_gradientMaxZFactor;
//		m_gradientMinDist = other.m_gradientMinDist;
//		m_gradientMaxDist = other.m_gradientMaxDist;
//
//		m_isSelected = other.m_isSelected;
//
//		other.m_sdf = nullptr;
//		// #ToDo Move Preview Mesh
//	}
//	return *this;
//}

void SDFShape::ComputeVoxelData(Vec3 const& worldPos, uint8_t& out_density, uint8_t& out_matID1, uint8_t& out_matID2, uint8_t& out_blendValue) const
{
	if (!m_sdf)
	{
		out_density = 0;
		out_matID1 = 0;
		out_matID2 = 0;
		out_blendValue = 0;
		return;
	}
	
	float signedDist = 0.f;
	if (!m_sdf->m_isPointCloud)
	{
		signedDist = m_sdf->GetSignedDistance(worldPos);
		out_density = Voxel::GetUint8DensityFromSignedDistance(signedDist);
	}
	else
	{
		out_density = m_sdf->GetPointCloudDensity(worldPos);
	}

	if (out_density == 0)
	{
		return; // early out
	}

	out_matID1 = m_materialID1;
	out_matID2 = m_materialID2;


	switch (m_fillMethod)
	{
	case SDFFillMethod::UNIFORM_BLEND:
		out_blendValue = Quantization::ToUint8FromUNorm(m_uniformBlendValue);
		break;
	case SDFFillMethod::Z_AXIS_GRADIENT:
		out_blendValue = ComputeZGradientBlend(worldPos);
		break;
	case SDFFillMethod::DISTANCE_GRADIENT:
		out_blendValue = ComputeDistanceGradientBlend(signedDist);
		break;
	default:
		out_blendValue = 0;
		break;
	}
}

void SDFShape::WriteToXml(XmlElement& element) const
{
	if (!m_sdf) return;

	// SDF Type
	element.SetAttribute("type", m_sdf->GetTypeString());

	// Transform
	XmlElement* transformElem = element.InsertNewChildElement("Transform");
	GameTransform const& transform = m_sdf->GetTransform();
	transformElem->SetAttribute("position", Stringf("%.3f,%.3f,%.3f", transform.m_position.x, transform.m_position.y, transform.m_position.z).c_str());
	transformElem->SetAttribute("rotation", Stringf("%.3f,%.3f,%.3f", transform.m_orientation.m_yawDegrees, transform.m_orientation.m_pitchDegrees, transform.m_orientation.m_rollDegrees).c_str());
	transformElem->SetAttribute("scale", transform.m_uniformScale);

	// SDF-specific properties
	XmlElement* propertiesElem = element.InsertNewChildElement("Properties");
	m_sdf->WritePropertiesToXml(*propertiesElem);

	// Fill method
	const char* fillMethodStr = "UNIFORM_BLEND";
	switch (m_fillMethod)
	{
	case SDFFillMethod::UNIFORM_BLEND: fillMethodStr = "UNIFORM_BLEND"; break;
	case SDFFillMethod::Z_AXIS_GRADIENT: fillMethodStr = "Z_AXIS_GRADIENT"; break;
	case SDFFillMethod::DISTANCE_GRADIENT: fillMethodStr = "DISTANCE_GRADIENT"; break;
	}
	element.SetAttribute("fillMethod", fillMethodStr);

	// Materials
	element.SetAttribute("materialID1", m_materialID1);
	element.SetAttribute("materialID2", m_materialID2);

	// Fill parameters
	element.SetAttribute("uniformBlendValue", m_uniformBlendValue);
	element.SetAttribute("gradientMinZFactor", m_gradientMinZFactor);
	element.SetAttribute("gradientMaxZFactor", m_gradientMaxZFactor);
	element.SetAttribute("gradientMinDist", m_gradientMinDist);
	element.SetAttribute("gradientMaxDist", m_gradientMaxDist);
}

void SDFShape::ReadFromXml(XmlElement const& element)
{
	// Transform
	XmlElement const* transformElem = element.FirstChildElement("Transform");
	if (transformElem && m_sdf)
	{
		GameTransform& transform = m_sdf->GetMutTransform();
		transform.m_position = ParseXmlAttribute(*transformElem, "position", Vec3::ZERO);
		EulerAngles rotation = ParseXmlAttribute(*transformElem, "rotation", EulerAngles());
		transform.m_orientation = rotation;
		transform.m_uniformScale = ParseXmlAttribute(*transformElem, "scale", 1.0f);
	}

	// SDF-specific properties
	XmlElement const* propertiesElem = element.FirstChildElement("Properties");
	if (propertiesElem && m_sdf)
	{
		m_sdf->ReadPropertiesFromXml(*propertiesElem);
	}

	// Fill method
	std::string fillMethodStr = ParseXmlAttribute(element, "fillMethod", "UNIFORM_BLEND");
	if (fillMethodStr == "UNIFORM_BLEND")
		m_fillMethod = SDFFillMethod::UNIFORM_BLEND;
	else if (fillMethodStr == "Z_AXIS_GRADIENT")
		m_fillMethod = SDFFillMethod::Z_AXIS_GRADIENT;
	else if (fillMethodStr == "DISTANCE_GRADIENT")
		m_fillMethod = SDFFillMethod::DISTANCE_GRADIENT;

	// Materials
	m_materialID1 = (uint8_t)ParseXmlAttribute(element, "materialID1", 0);
	m_materialID2 = (uint8_t)ParseXmlAttribute(element, "materialID2", 0);

	// Fill parameters
	m_uniformBlendValue = ParseXmlAttribute(element, "uniformBlendValue", 0.0f);
	m_gradientMinZFactor = ParseXmlAttribute(element, "gradientMinZFactor", 0.9f);
	m_gradientMaxZFactor = ParseXmlAttribute(element, "gradientMaxZFactor", 0.95f);
	m_gradientMinDist = ParseXmlAttribute(element, "gradientMinDist", -2.0f);
	m_gradientMaxDist = ParseXmlAttribute(element, "gradientMaxDist", -1.0f);
}

SDFShape* SDFShape::CreateFromXml(XmlElement const& element)
{
	std::string typeStr = ParseXmlAttribute(element, "type", "Sphere");

	SDF* sdf = nullptr;

	if (typeStr == "Sphere")
	{
		sdf = new SDFSphere();
	}
	else if (typeStr == "Box")
	{
		sdf = new SDFBox();
	}
	else if (typeStr == "Torus")
	{
		sdf = new SDFTorus();
	}
	else if (typeStr == "Capsule")
	{
		sdf = new SDFCapsule();
	}
	else if (typeStr == "CutSphere")
	{
		sdf = new SDFCutSphere();
	}
	else if (typeStr == "CutHollowSphere")
	{
		sdf = new SDFCutHollowSphere();
	}
	else if (typeStr == "DensityCloud")
	{
		sdf = new SDFDensityCloud();
	}
	else
	{
		// Unknown type, default to sphere
		sdf = new SDFSphere();
	}

	SDFShape* shape = new SDFShape(sdf);
	shape->ReadFromXml(element);

	return shape;
}

uint8_t SDFShape::ComputeZGradientBlend(Vec3 const& worldPos) const
{
	AABB3 worldAABB = m_sdf->GetAffectedAABB(); // not validate m_sdf
	float height = worldAABB.m_maxs.z - worldAABB.m_mins.z;
	if (height <= 0.f)
	{
		return 0;
	}

	float relativeZ = (worldPos.z - worldAABB.m_mins.z) / height;

	float t = RangeMapClamped(relativeZ, m_gradientMinZFactor, m_gradientMaxZFactor, 0.f, 1.f);

	float smoothT = SmoothStep3(t);

	return Quantization::ToUint8FromUNorm(smoothT);
}

uint8_t SDFShape::ComputeDistanceGradientBlend(float signedDist) const
{
	float t = RangeMapClamped(signedDist, m_gradientMinDist, m_gradientMaxDist, 0.f, 1.f);
	float smoothT = SmoothStep3(t);
	return Quantization::ToUint8FromUNorm(smoothT);
}

void SDFShape::DebugDrawShape() const
{
	// #ToDo Not use DebugDraw (Unlit), Use Diffuse?

	if (!m_sdf) return;
	Rgba8 color = m_isSelected ? Rgba8(255, 215, 0, 255) : Rgba8(70, 130, 180, 255); // Yellow/Blue
	std::vector<Vertex_PCU> worldVerts;
	m_sdf->AddWorldVerts(worldVerts);
	DebugAddWorldTriangleList(worldVerts, 0.f, color, color, m_isSelected ? DebugRenderMode::X_RAY : DebugRenderMode::USE_DEPTH);
}

void SDFShape::AddWorldVerts(std::vector<Vertex_PCU>& worldVerts) const
{
	if (!m_sdf) return;

	Rgba8 color = m_isSelected ? Rgba8(255, 215, 0, 255) : Rgba8(70, 130, 180, 255); // Yellow/Blue
	m_sdf->AddWorldVerts(worldVerts, color);
}

void SDFShape::GeneratePreviewMesh()
{

}

void SDFShape::DestroyPreviewMesh()
{

}


SDFShape::~SDFShape()
{
	DestroyPreviewMesh();

	delete m_sdf;
	m_sdf = nullptr;
}

