#include "Game/DualContouring1.hpp"
#include "Game/CubeTables.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"


DualContouring1::DualContouring1()
{
	m_vertexBuffer = g_theRenderer->CreateVertexBuffer(1 * sizeof(Vertex_PNMD), sizeof(Vertex_PNMD));
	m_indexBuffer = g_theRenderer->CreateIndexBuffer(1 * sizeof(unsigned int));
}

DualContouring1::~DualContouring1()
{
	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;

	delete m_indexBuffer;
	m_indexBuffer = nullptr;
}

void DualContouring1::RegenerateMesh(IntGrid3 const& grid, std::vector<float> const& sdfValues, float blockyBlendFactor /*= 0.f*/)
{
	m_isReady = false;

	m_grid = grid;
	m_sdfValues = sdfValues;
	m_blockyBlendValue = blockyBlendFactor;

	ResetData();
	EstimateIsosurfaceVertexAndNormal();
	BuildMeshQuads();

	m_isReady = true;
}

void DualContouring1::ResetData()
{
	m_index1DToVertexIndex.assign(m_grid.GetCellCount(), INVALID_VERTEX_INDEX);

	int estimatedQuadNum = (m_grid.GetSizeX() * m_grid.GetSizeY() + m_grid.GetSizeX() * m_grid.GetSizeZ() + m_grid.GetSizeY() * m_grid.GetSizeZ()) * 2;
	m_vertices.clear();
	m_vertices.reserve(estimatedQuadNum * 4);
	m_indices.clear();
	m_indices.reserve(estimatedQuadNum * 6);
	m_isosurfaceVertexIndexs.clear();
	m_isosurfaceVertexIndexs.reserve(estimatedQuadNum * 4);
}

void DualContouring1::EstimateIsosurfaceVertexAndNormal()
{
	// linear transversal of lattices
	for (int z = 0; z < m_grid.GetSizeZ() - 1; ++z)
	{
		for (int y = 0; y < m_grid.GetSizeY() - 1; ++y)
		{
			for (int x = 0; x < m_grid.GetSizeX() - 1; ++x)
			{

				IntVec3 minCornerIndex3D(x, y, z);
				int minCornerIndex1D = m_grid.Linearize(minCornerIndex3D);
				Vec3 minCornerPos = Vec3(minCornerIndex3D) + Vec3(0.5f, 0.5f, 0.5f); // Get Voxel Center Position

				float cornerDensities[Isosurface::kCubeCornerCount] = {};

				int cubeIndex = 0;
				for (int i = 0; i < Isosurface::kCubeCornerCount; ++i)
				{
					int cornerIndex1D = m_grid.Linearize(minCornerIndex3D + Isosurface::kCubeCornerOffsets[i]);
					float d = GetDensity(cornerIndex1D);

					cornerDensities[i] = d;
					// check if the corner is solid
					if (d <= m_isosurfaceValue)
					{
						cubeIndex |= (1 << i);
					}
				}

				// if 8 corner are all solid or empty, there is no isosurface vertex in the cube 
				bool isIsosurfaceVertexInCube = cubeIndex != 0 && cubeIndex != 255;

				if (isIsosurfaceVertexInCube)
				{
					//Vec3 centroid = GetEdgeIntersectionsCentroid(cubeIndex, cornerDensities, m_isosurfaceValue);
					Vec3 isoPoint = GetVertexFromHermiteData(cubeIndex, cornerDensities, m_isosurfaceValue);
					Vec3 interpResult = Interpolate(isoPoint, Vec3(0.5f, 0.5f, 0.5f), m_blockyBlendValue); // blend to blocky
					Vec3 position = interpResult + minCornerPos;

					Vec3 sdfGradient = GetSdfGradient(interpResult, cornerDensities);
					// Normal is solid -> empty
					Vec3 normal = sdfGradient; // negative points to positive #Todo: inverse the normal (high density -> low density)

					// #ToDo: Blend Material
					// Just take the top corner by density, and set matID and density(0~255)
					// in Vertex Shader, a triangle with 3 different matIDs and different densities

					m_index1DToVertexIndex[minCornerIndex1D] = (uint32_t)m_vertices.size();
					m_vertices.push_back(Vertex_PNMD(position, normal, 0, 0));
					m_isosurfaceVertexIndexs.push_back(std::make_pair(minCornerIndex1D, minCornerIndex3D));
				}

			}
		}
	}
}

void DualContouring1::BuildMeshQuads()
{
	int strideAxisX = m_grid.Linearize(IntVec3(1, 0, 0));
	int strideAxisY = m_grid.Linearize(IntVec3(0, 1, 0));
	int strideAxisZ = m_grid.Linearize(IntVec3(0, 0, 1));

	for (auto const& item : m_isosurfaceVertexIndexs)
	{
		uint32_t const& index1D = item.first; // linear Index
		IntVec3 const& index3D = item.second; // localCoords

		// Question? x = 0, y = 0, z = 0 plane meshes will be generate by previous chunk max 
		if (index3D.x == 0 || index3D.y == 0 || index3D.z == 0)
		{
			continue;
		}

		// x axis
		TryBuildQuad(index1D, index1D + strideAxisX, strideAxisY, strideAxisZ);
		// y axis
		TryBuildQuad(index1D, index1D + strideAxisY, strideAxisZ, strideAxisX);
		// z axis
		TryBuildQuad(index1D, index1D + strideAxisZ, strideAxisX, strideAxisY);
	}

	g_theRenderer->CopyCPUToGPU(m_vertices.data(), static_cast<unsigned int>(m_vertices.size()) * m_vertexBuffer->GetStride(), m_vertexBuffer);
	g_theRenderer->CopyCPUToGPU(m_indices.data(), static_cast<unsigned int>(m_indices.size()) * m_indexBuffer->GetStride(), m_indexBuffer);

}

void DualContouring1::TryBuildQuad(int p1, int p2, int strideAxisB, int strideAxisC)
{
	//int p1 = index1D;
	//int p2 = index1D + strideAxisX; // x axis

	//int strideAxisB = strideAxisY;
	//int strideAxisC = strideAxisZ;

	float d1 = GetDensity(p1); // axis start
	float d2 = GetDensity(p2); // axis end

	bool isSolid1 = IsDensitySolid(d1);
	bool isSolid2 = IsDensitySolid(d2);

	if (isSolid1 == isSolid2) return;

	bool isNegativeFace = !isSolid1; // face normal points from solid to empty

	// Vertex Index
	uint32_t v1 = m_index1DToVertexIndex[p1];
	uint32_t v2 = m_index1DToVertexIndex[p1 - strideAxisB];
	uint32_t v3 = m_index1DToVertexIndex[p1 - strideAxisC];
	uint32_t v4 = m_index1DToVertexIndex[p1 - strideAxisB - strideAxisC];

	// Vertex Position
	Vec3 vertexPos1 = m_vertices[v1].m_position;
	Vec3 vertexPos2 = m_vertices[v2].m_position;
	Vec3 vertexPos3 = m_vertices[v3].m_position;
	Vec3 vertexPos4 = m_vertices[v4].m_position;

	// Split the quad along shorter axis
	if (GetDistanceSquared3D(vertexPos1, vertexPos4) < GetDistanceSquared3D(vertexPos2, vertexPos3))
	{
		if (isNegativeFace)
		{
			m_indices.push_back(v1);
			m_indices.push_back(v4);
			m_indices.push_back(v2);
			m_indices.push_back(v1);
			m_indices.push_back(v3);
			m_indices.push_back(v4);
		}
		else
		{
			m_indices.push_back(v1);
			m_indices.push_back(v2);
			m_indices.push_back(v4);
			m_indices.push_back(v1);
			m_indices.push_back(v4);
			m_indices.push_back(v3);
		}
	}
	else
	{
		if (isNegativeFace)
		{
			m_indices.push_back(v2);
			m_indices.push_back(v3);
			m_indices.push_back(v4);
			m_indices.push_back(v2);
			m_indices.push_back(v1);
			m_indices.push_back(v3);
		}
		else
		{
			m_indices.push_back(v2);
			m_indices.push_back(v4);
			m_indices.push_back(v3);
			m_indices.push_back(v2);
			m_indices.push_back(v3);
			m_indices.push_back(v1);
		}
	}
}

Vec3 DualContouring1::GetVertexFromHermiteData(int cubeIndex, float* cornerDensities, float isosurfaceValue)
{
	if (cubeIndex == 0 || cubeIndex == 255)
	{
		return Vec3(0.5f, 0.5f, 0.5f); // center of the cube
	}

	uint16_t edgeFlags = Isosurface::kEdgeIntersectionFlags[cubeIndex];

	std::vector<Vec3> intersectionPointPositions;
	intersectionPointPositions.reserve(Isosurface::kCubeEdgeCount);


	for (int edge = 0; edge < Isosurface::kCubeEdgeCount; ++edge)
	{
		if (edgeFlags & (1 << edge))
		{
			int v0 = Isosurface::kEdgeVertexIndices[edge][0];
			int v1 = Isosurface::kEdgeVertexIndices[edge][1];

			Vec3 p0 = Vec3(Isosurface::kCubeCornerOffsets[v0]);
			Vec3 p1 = Vec3(Isosurface::kCubeCornerOffsets[v1]);

			float d0 = cornerDensities[v0];
			float d1 = cornerDensities[v1];

			// d0 - isosurface value - d1
			float t = 0.5f;
			if (fabsf(d1 - d0) > 0.00001f)
			{
				t = (isosurfaceValue - d0) / (d1 - d0);
			}
			// no need to clamp or keep calculation order to avoid cracks (in marching cubes)

			Vec3 p = Interpolate(p0, p1, t);
			intersectionPointPositions.push_back(p);
		}
	}

	GUARANTEE_OR_DIE(intersectionPointPositions.size() > 0, "");

	std::vector<Vec3> intersectionPointNormals;
	intersectionPointNormals.reserve(intersectionPointPositions.size());

	// mixing forward / backward difference #ToDo maybe central difference is better but need more data
	for (Vec3 const& p : intersectionPointPositions)
	{
		intersectionPointNormals.push_back(GetSdfGradient(p, cornerDensities).GetNormalized());
	}


	// Analysis and Acceleration of High Quality Isosurface Contouring

	const int activeEdgeCount = (int)intersectionPointPositions.size();
	Vec3 massPoint;
	for (Vec3 const& p : intersectionPointPositions)
	{
		massPoint += p;
	}
	massPoint /= (float)activeEdgeCount;


	Vec3 particlePosition = massPoint;

	constexpr int MAX_ITERATION = 50;
	constexpr float THRESHOLD = 0.0000001f;

	for (int iterationIndex = 0; iterationIndex < MAX_ITERATION; ++iterationIndex)
	{
		Vec3 force;
		
		for (int activeEdgeIndex = 0; activeEdgeIndex < activeEdgeCount; ++activeEdgeIndex)
		{
			Vec3 const& edgePos = intersectionPointPositions[activeEdgeIndex];
			Vec3 const& edgeNormal = intersectionPointNormals[activeEdgeIndex];

			force += (-1.f) * edgeNormal * DotProduct3D(edgeNormal, particlePosition - edgePos);
		}

		float damping = 1.f - static_cast<float>(iterationIndex) / static_cast<float>(MAX_ITERATION);
		particlePosition += force * damping / static_cast<float>(activeEdgeCount);

		if (force.GetLengthSquared() < THRESHOLD)
		{
			if (iterationIndex > 15)
			{
				DebuggerPrintf("Iteration: %d\n", iterationIndex);
			}

			break;
		}

	}
	AABB3 box = AABB3(Vec3(0.f, 0.f, 0.f), Vec3(1.f, 1.f, 1.f));
	particlePosition = box.GetNearestPoint(particlePosition);

	return particlePosition;
}

STATIC Vec3 DualContouring1::GetEdgeIntersectionsCentroid(int cubeIndex, float* cornerDensities, float isosurfaceValue)
{
	if (cubeIndex == 0 || cubeIndex == 255)
	{
		return Vec3(0.5f, 0.5f, 0.5f); // center of the cube
	}

	uint16_t edgeFlags = Isosurface::kEdgeIntersectionFlags[cubeIndex];

	std::vector<Vec3> intersectionPoints;
	intersectionPoints.reserve(Isosurface::kCubeEdgeCount);


	for (int edge = 0; edge < Isosurface::kCubeEdgeCount; ++edge)
	{
		if (edgeFlags & (1 << edge))
		{
			int v0 = Isosurface::kEdgeVertexIndices[edge][0];
			int v1 = Isosurface::kEdgeVertexIndices[edge][1];

			Vec3 p0 = Vec3(Isosurface::kCubeCornerOffsets[v0]);
			Vec3 p1 = Vec3(Isosurface::kCubeCornerOffsets[v1]);

			float d0 = cornerDensities[v0];
			float d1 = cornerDensities[v1];

			// d0 - isosurface value - d1
			float t = 0.5f;
			if (fabsf(d1 - d0) > 0.00001f)
			{
				t = (isosurfaceValue - d0) / (d1 - d0);
			}
			// no need to clamp or keep calculation order to avoid cracks (in marching cubes)

			Vec3 p = Interpolate(p0, p1, t);
			intersectionPoints.push_back(p);
		}
	}

	GUARANTEE_OR_DIE(intersectionPoints.size() > 0, "");

	Vec3 result;
	for (Vec3 const& p : intersectionPoints)
	{
		result += p;
	}
	return result / (float)intersectionPoints.size();
}

STATIC Vec3 DualContouring1::GetSdfGradient(Vec3 const& s, float* d)
{
	// Bilinear Interpolation, and not normalized
	Vec3 p00(d[0b001], d[0b010], d[0b100]);
	Vec3 n00(d[0b000], d[0b000], d[0b000]);

	Vec3 p10(d[0b101], d[0b011], d[0b110]);
	Vec3 n10(d[0b100], d[0b001], d[0b010]);

	Vec3 p01(d[0b011], d[0b110], d[0b101]);
	Vec3 n01(d[0b010], d[0b100], d[0b001]);

	Vec3 p11(d[0b111], d[0b111], d[0b111]);
	Vec3 n11(d[0b110], d[0b101], d[0b011]);

	Vec3 d00 = p00 - n00;
	Vec3 d10 = p10 - n10;
	Vec3 d01 = p01 - n01;
	Vec3 d11 = p11 - n11;

	Vec3 neg(1.0f - s.x, 1.0f - s.y, 1.0f - s.z);

	return
		neg.YZX() * neg.ZXY() * d00 +
		neg.YZX() * s.ZXY() * d10 +
		s.YZX() * neg.ZXY() * d01 +
		s.YZX() * s.ZXY() * d11;
}


