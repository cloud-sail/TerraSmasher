#include "Game/VoxelMesher.hpp"
#include "Game/GameCommon.hpp"
#include "Game/CubeTables.hpp"
#include "Game/GameStructuredBuffer.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
//#include "Engine/Renderer/VertexBuffer.hpp"
//#include "Engine/Renderer/IndexBuffer.hpp"



//-----------------------------------------------------------------------------------------------
struct QuadMaterialInfo
{
	uint8_t matID1;
	uint8_t matID2;
	std::array<uint8_t, 4> blendValues;
};


class MaterialContribution 
{
private:
	float weights[256] = { 0.0f };

public:
	void AddVoxel(const Voxel& voxel) 
	{
		float voxelDensity = voxel.GetDensityUNorm();

		if (voxelDensity >= Voxel::ISO_FLOAT_VALUE)
		{
			// Calculate weight based on distance from iso surface
			float distanceWeight = voxelDensity - Voxel::ISO_FLOAT_VALUE; // [0, 0.5]
			// Apply non-linear weight to emphasize voxels closer to the surface
			distanceWeight = distanceWeight * distanceWeight * 4.0f; // [0, 1]

			float blend = Quantization::ToUNormFromUint8(voxel.m_blendValue);

			// Accumulate material weights
			weights[voxel.m_materialID1] += distanceWeight * (1.0f - blend);

			if (voxel.m_materialID2 != voxel.m_materialID1)
			{
				weights[voxel.m_materialID2] += distanceWeight * blend;
			}
		}
	}


	uint8_t CalculateBlendValue(uint8_t mat1, uint8_t mat2) const 
	{
		if (mat1 == mat2) return 0;

		float w1 = weights[mat1];
		float w2 = weights[mat2];

		constexpr float EPSILON = 0.0001f;
		if (w1 + w2 < EPSILON) return 0;

		float ratio = w2 / (w1 + w2);
		return Quantization::ToUint8FromUNorm(ratio);
	}

	float GetWeight(uint8_t matID) const 
	{
		return weights[matID];
	}
};

QuadMaterialInfo CalculateQuadMaterial(std::array<std::array<Voxel, 8>, 4> const& vertexVoxels)
{
	QuadMaterialInfo result;

	// 1. Collect mat contribution from each vertex
	std::array<MaterialContribution, 4> vertexContribs;
	for (int v = 0; v < 4; ++v) 
	{
		for (int i = 0; i < 8; ++i) {
			vertexContribs[v].AddVoxel(vertexVoxels[v][i]);
		}
	}

	// 2. Calculate overall quad mat weight
	float quadWeights[256] = {};
	for (const auto& contrib : vertexContribs) 
	{
		for (int i = 0; i < 256; ++i) 
		{
			quadWeights[i] += contrib.GetWeight((uint8_t)i);
		}
	}

	// 3. Find the two materials with highest weights
	uint8_t topMat1 = 0;
	uint8_t topMat2 = 0;
	float topWeight1 = 0.0f;
	float topWeight2 = 0.0f;

	for (int i = 0; i < 256; ++i) 
	{
		if (quadWeights[i] > topWeight1) 
		{
			// New highest weight
			topMat2 = topMat1;
			topWeight2 = topWeight1;
			topMat1 = static_cast<uint8_t>(i);
			topWeight1 = quadWeights[i];
		}
		else if (quadWeights[i] > topWeight2) 
		{
			// New second highest weight
			topMat2 = static_cast<uint8_t>(i);
			topWeight2 = quadWeights[i];
		}
	}

	// 4. Set Quad Mat
	constexpr float EPSILON = 0.00001f;
	if (topWeight1 < EPSILON)
	{
		// No valid Mat
		result.matID1 = 0;
		result.matID2 = 0;
		result.blendValues.fill(0);
		return result;
	}

	result.matID1 = topMat1;
	result.matID2 = (topWeight2 > EPSILON) ? topMat2 : topMat1;

	// 5. Calculate blend value of each vertex
	for (int v = 0; v < 4; ++v) 
	{
		result.blendValues[v] = vertexContribs[v].CalculateBlendValue(result.matID1, result.matID2);
	}

	return result;
}











//-----------------------------------------------------------------------------------------------
VoxelMesher::VoxelMesher(MeshingMode mode)
	: m_mode(mode)
{

}

void VoxelMesher::GenerateMesh(std::vector<Voxel> const& voxels, IntGrid3 const& grid, std::vector<Vertex_Terrain>& vertices, std::vector<unsigned int>& indices, float blockyBlendFactor /*= 0.f*/, Vec3 offsetFromMinCorner /*= Vec3(1.f, 1.f, 1.f)*/)
{
	// Initialization
	m_grid = grid;
	m_offsetFromMinCorner = offsetFromMinCorner;
	m_blockyBlendValue = blockyBlendFactor;
	m_voxelsPtr = &voxels;
	m_verticesPtr = &vertices;
	m_indicesPtr = &indices;

	//m_vertexBuffer = vertexBuffer;
	//m_indexBuffer = indexBuffer;

	// Clear Data and Reserve memory
	m_index1DToVertexIndex.assign(m_grid.GetCellCount(), INVALID_VERTEX_INDEX);
	int estimatedIsoVertexNum = (m_grid.GetSizeX() * m_grid.GetSizeY() + m_grid.GetSizeX() * m_grid.GetSizeZ() + m_grid.GetSizeY() * m_grid.GetSizeZ()) * 2;
	m_validIsoVertexIndexes.clear();
	m_validIsoVertexIndexes.reserve(estimatedIsoVertexNum);
	m_isoVerts.clear();
	m_isoVerts.reserve(estimatedIsoVertexNum);

	// Different types
	switch (m_mode)
	{
	case MeshingMode::SURFACE_NETS:
		GenerateIsoVertex_SurfaceNets();
		BuildMeshQuads();
		break;
	case MeshingMode::DUAL_CONTOURING:
		GenerateIsoVertex_DualContouring();
		BuildMeshQuads();
		break;
	case MeshingMode::DUAL_CONTOURING_QEF:
		// TODO: Implement dual contouring with QEF
		break;
	}
}

void VoxelMesher::GenerateMesh(std::vector<Voxel> const& voxels, IntGrid3 const& grid, TerrainStructuredBuffer* structuredVertexBuffer, float blockyBlendFactor /*= 0.f*/, Vec3 offsetFromMinCorner /*= Vec3(1.f, 1.f, 1.f)*/)
{
	// Initialization
	m_grid = grid;
	m_offsetFromMinCorner = offsetFromMinCorner;
	m_blockyBlendValue = blockyBlendFactor;
	m_voxelsPtr = &voxels;
	m_structuredVertexBuffer = structuredVertexBuffer;

	// Clear Data and Reserve memory
	m_index1DToVertexIndex.assign(m_grid.GetCellCount(), INVALID_VERTEX_INDEX);
	int estimatedIsoVertexNum = (m_grid.GetSizeX() * m_grid.GetSizeY() + m_grid.GetSizeX() * m_grid.GetSizeZ() + m_grid.GetSizeY() * m_grid.GetSizeZ()) * 2;
	m_validIsoVertexIndexes.clear();
	m_validIsoVertexIndexes.reserve(estimatedIsoVertexNum);
	m_isoVerts.clear();
	m_isoVerts.reserve(estimatedIsoVertexNum);

	// Different types
	switch (m_mode)
	{
	case MeshingMode::SURFACE_NETS:
		GenerateIsoVertex_SurfaceNets();
		BuildMeshQuads2();
		break;
	case MeshingMode::DUAL_CONTOURING:
		GenerateIsoVertex_DualContouring();
		BuildMeshQuads2();
		break;
	case MeshingMode::DUAL_CONTOURING_QEF:
		// TODO: Implement dual contouring with QEF
		break;
	}
}

void VoxelMesher::GenerateIsoVertex_SurfaceNets()
{
	// Estimate IsoVertex (Position and Normal)
	for (int z = 0; z < m_grid.GetSizeZ() - 1; ++z)
	{
		for (int y = 0; y < m_grid.GetSizeY() - 1; ++y)
		{
			for (int x = 0; x < m_grid.GetSizeX() - 1; ++x)
			{

				IntVec3 minCornerIndex3D(x, y, z);
				int minCornerIndex1D = m_grid.Linearize(minCornerIndex3D);
				Vec3 minCornerPos = Vec3(minCornerIndex3D) + Vec3(0.5f, 0.5f, 0.5f) - m_offsetFromMinCorner; // Get Voxel Center Position

				float cornerDensities[Isosurface::kCubeCornerCount] = {};

				int cubeIndex = 0;
				for (int i = 0; i < Isosurface::kCubeCornerCount; ++i)
				{
					int cornerIndex1D = m_grid.Linearize(minCornerIndex3D + Isosurface::kCubeCornerOffsets[i]);
					float d = GetDensity(cornerIndex1D);

					cornerDensities[i] = d;
					// check if the corner is solid
					if (Voxel::IsSolid(d))
					{
						cubeIndex |= (1 << i);
					}
				}

				// if 8 corner are all solid or empty, there is no isosurface vertex in the cube 
				bool isIsosurfaceVertexInCube = cubeIndex != 0 && cubeIndex != 255;

				if (isIsosurfaceVertexInCube)
				{
					Vec3 centroid = GetEdgeIntersectionsCentroid(cubeIndex, cornerDensities, Voxel::ISO_FLOAT_VALUE);
					Vec3 interpResult = Interpolate(centroid, Vec3(0.5f, 0.5f, 0.5f), m_blockyBlendValue); // blend to blocky
					Vec3 position = interpResult + minCornerPos;

					Vec3 sdfGradient = GetSdfGradient(interpResult, cornerDensities);
					// Normal is solid -> empty (high density -> low density)
					Vec3 normal = -sdfGradient;

					// Build IsoVertex
					IsoVertex vert;
					vert.m_position = position;
					vert.SetNormal(normal);
					for (int i = 0; i < Isosurface::kCubeCornerCount; ++i)
					{
						int cornerIndex1D = m_grid.Linearize(minCornerIndex3D + Isosurface::kCubeCornerOffsets[i]);
						vert.m_corners[i] = (*m_voxelsPtr)[cornerIndex1D];
					}

					m_index1DToVertexIndex[minCornerIndex1D] = (uint32_t)m_isoVerts.size();
					m_isoVerts.push_back(vert);
					m_validIsoVertexIndexes.push_back(std::make_pair(minCornerIndex1D, minCornerIndex3D));
				}
			}
		}
	}
}

void VoxelMesher::GenerateIsoVertex_DualContouring()
{
	// Estimate IsoVertex (Position and Normal)
	for (int z = 0; z < m_grid.GetSizeZ() - 1; ++z)
	{
		for (int y = 0; y < m_grid.GetSizeY() - 1; ++y)
		{
			for (int x = 0; x < m_grid.GetSizeX() - 1; ++x)
			{

				IntVec3 minCornerIndex3D(x, y, z);
				int minCornerIndex1D = m_grid.Linearize(minCornerIndex3D);
				Vec3 minCornerPos = Vec3(minCornerIndex3D) + Vec3(0.5f, 0.5f, 0.5f) - m_offsetFromMinCorner; // Get Voxel Center Position

				float cornerDensities[Isosurface::kCubeCornerCount] = {};

				int cubeIndex = 0;
				for (int i = 0; i < Isosurface::kCubeCornerCount; ++i)
				{
					int cornerIndex1D = m_grid.Linearize(minCornerIndex3D + Isosurface::kCubeCornerOffsets[i]);
					float d = GetDensity(cornerIndex1D);

					cornerDensities[i] = d;
					// check if the corner is solid
					if (Voxel::IsSolid(d))
					{
						cubeIndex |= (1 << i);
					}
				}

				// if 8 corner are all solid or empty, there is no isosurface vertex in the cube 
				bool isIsosurfaceVertexInCube = cubeIndex != 0 && cubeIndex != 255;

				if (isIsosurfaceVertexInCube)
				{
					Vec3 isoPoint = GetVertexFromHermiteData(cubeIndex, cornerDensities, Voxel::ISO_FLOAT_VALUE);
					Vec3 interpResult = Interpolate(isoPoint, Vec3(0.5f, 0.5f, 0.5f), m_blockyBlendValue); // blend to blocky
					Vec3 position = interpResult + minCornerPos;

					Vec3 sdfGradient = GetSdfGradient(interpResult, cornerDensities);
					// Normal is solid -> empty (high density -> low density)
					Vec3 normal = -sdfGradient;

					// Build IsoVertex
					IsoVertex vert;
					vert.m_position = position;
					vert.SetNormal(normal);
					for (int i = 0; i < Isosurface::kCubeCornerCount; ++i)
					{
						int cornerIndex1D = m_grid.Linearize(minCornerIndex3D + Isosurface::kCubeCornerOffsets[i]);
						vert.m_corners[i] = (*m_voxelsPtr)[cornerIndex1D];
					}

					m_index1DToVertexIndex[minCornerIndex1D] = (uint32_t)m_isoVerts.size();
					m_isoVerts.push_back(vert);
					m_validIsoVertexIndexes.push_back(std::make_pair(minCornerIndex1D, minCornerIndex3D));
				}
			}
		}
	}
}

void VoxelMesher::BuildMeshQuads()
{
	int strideAxisX = m_grid.Linearize(IntVec3(1, 0, 0));
	int strideAxisY = m_grid.Linearize(IntVec3(0, 1, 0));
	int strideAxisZ = m_grid.Linearize(IntVec3(0, 0, 1));

	int estimatedQuadNum = static_cast<int>(m_isoVerts.size() * 3);
	(*m_verticesPtr).clear();
	(*m_verticesPtr).reserve(estimatedQuadNum * 4);
	(*m_indicesPtr).clear();
	(*m_indicesPtr).reserve(estimatedQuadNum * 6);

	for (auto const& item : m_validIsoVertexIndexes)
	{
		uint32_t const& index1D = item.first; // linear Index
		IntVec3 const& index3D = item.second; // localCoords

		// x = 0, y = 0, z = 0 plane meshes will be generate by previous chunk max 
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
	// #ToDo copy from cpu to gpu is always on main thread, may be return m_vertices m_indices, let outer code update vb ib?
	//g_theRenderer->CopyCPUToGPU(m_vertices.data(), static_cast<unsigned int>(m_vertices.size()) * m_vertexBuffer->GetStride(), m_vertexBuffer);
	//g_theRenderer->CopyCPUToGPU(m_indices.data(), static_cast<unsigned int>(m_indices.size()) * m_indexBuffer->GetStride(), m_indexBuffer);

}

void VoxelMesher::TryBuildQuad(int p1, int p2, int strideAxisB, int strideAxisC)
{
	//int p1 = index1D;
	//int p2 = index1D + strideAxisX; // x axis

	//int strideAxisB = strideAxisY;
	//int strideAxisC = strideAxisZ;

	float d1 = GetDensity(p1); // axis start
	float d2 = GetDensity(p2); // axis end

	bool isSolid1 = Voxel::IsSolid(d1);
	bool isSolid2 = Voxel::IsSolid(d2);

	if (isSolid1 == isSolid2) return;

	bool isNegativeFace = !isSolid1; // face normal points from solid to empty

	// Vertex Index
	uint32_t isoVertIndex1 = m_index1DToVertexIndex[p1];
	uint32_t isoVertIndex2 = m_index1DToVertexIndex[p1 - strideAxisB];
	uint32_t isoVertIndex3 = m_index1DToVertexIndex[p1 - strideAxisC];
	uint32_t isoVertIndex4 = m_index1DToVertexIndex[p1 - strideAxisB - strideAxisC];

	IsoVertex const& isoVert1 = m_isoVerts[isoVertIndex1];
	IsoVertex const& isoVert2 = m_isoVerts[isoVertIndex2];
	IsoVertex const& isoVert3 = m_isoVerts[isoVertIndex3];
	IsoVertex const& isoVert4 = m_isoVerts[isoVertIndex4];

	std::array<std::array<Voxel, 8>, 4> vertexVoxels;
	vertexVoxels[0] = isoVert1.m_corners;
	vertexVoxels[1] = isoVert2.m_corners;
	vertexVoxels[2] = isoVert3.m_corners;
	vertexVoxels[3] = isoVert4.m_corners;
	QuadMaterialInfo material = CalculateQuadMaterial(vertexVoxels);


	unsigned int const startIndex = static_cast<unsigned int>((*m_verticesPtr).size());

	(*m_verticesPtr).push_back(Vertex_Terrain(isoVert1.m_position, isoVert1.m_smoothNormal, material.matID1, material.matID2, material.blendValues[0]));
	(*m_verticesPtr).push_back(Vertex_Terrain(isoVert2.m_position, isoVert2.m_smoothNormal, material.matID1, material.matID2, material.blendValues[1]));
	(*m_verticesPtr).push_back(Vertex_Terrain(isoVert3.m_position, isoVert3.m_smoothNormal, material.matID1, material.matID2, material.blendValues[2]));
	(*m_verticesPtr).push_back(Vertex_Terrain(isoVert4.m_position, isoVert4.m_smoothNormal, material.matID1, material.matID2, material.blendValues[3]));

	uint32_t v1 = startIndex;
	uint32_t v2 = startIndex + 1;
	uint32_t v3 = startIndex + 2;
	uint32_t v4 = startIndex + 3;

	// Split the quad along shorter axis
	if (GetDistanceSquared3D(isoVert1.m_position, isoVert4.m_position) < GetDistanceSquared3D(isoVert2.m_position, isoVert3.m_position))
	{
		if (isNegativeFace)
		{
			(*m_indicesPtr).push_back(v1);
			(*m_indicesPtr).push_back(v4);
			(*m_indicesPtr).push_back(v2);
			(*m_indicesPtr).push_back(v1);
			(*m_indicesPtr).push_back(v3);
			(*m_indicesPtr).push_back(v4);
		}
		else
		{
			(*m_indicesPtr).push_back(v1);
			(*m_indicesPtr).push_back(v2);
			(*m_indicesPtr).push_back(v4);
			(*m_indicesPtr).push_back(v1);
			(*m_indicesPtr).push_back(v4);
			(*m_indicesPtr).push_back(v3);
		}
	}
	else
	{
		if (isNegativeFace)
		{
			(*m_indicesPtr).push_back(v2);
			(*m_indicesPtr).push_back(v3);
			(*m_indicesPtr).push_back(v4);
			(*m_indicesPtr).push_back(v2);
			(*m_indicesPtr).push_back(v1);
			(*m_indicesPtr).push_back(v3);
		}
		else
		{
			(*m_indicesPtr).push_back(v2);
			(*m_indicesPtr).push_back(v4);
			(*m_indicesPtr).push_back(v3);
			(*m_indicesPtr).push_back(v2);
			(*m_indicesPtr).push_back(v3);
			(*m_indicesPtr).push_back(v1);
		}
	}
}

void VoxelMesher::BuildMeshQuads2()
{
	int strideAxisX = m_grid.Linearize(IntVec3(1, 0, 0));
	int strideAxisY = m_grid.Linearize(IntVec3(0, 1, 0));
	int strideAxisZ = m_grid.Linearize(IntVec3(0, 0, 1));

	int estimatedQuadNum = static_cast<int>(m_isoVerts.size() * 3);
	m_structuredVertexBuffer->ClearCPUMesh();
	m_structuredVertexBuffer->Reserve((int)m_isoVerts.size(), estimatedQuadNum * 6);

	for (IsoVertex const& v : m_isoVerts)
	{
		m_structuredVertexBuffer->AddSharedVertex(SharedVertex(v.m_position, v.m_smoothNormal));
	}

	for (auto const& item : m_validIsoVertexIndexes)
	{
		uint32_t const& index1D = item.first; // linear Index
		IntVec3 const& index3D = item.second; // localCoords

		// x = 0, y = 0, z = 0 plane meshes will be generate by previous chunk max 
		if (index3D.x == 0 || index3D.y == 0 || index3D.z == 0)
		{
			continue;
		}

		// x axis
		TryBuildQuad2(index1D, index1D + strideAxisX, strideAxisY, strideAxisZ);
		// y axis
		TryBuildQuad2(index1D, index1D + strideAxisY, strideAxisZ, strideAxisX);
		// z axis
		TryBuildQuad2(index1D, index1D + strideAxisZ, strideAxisX, strideAxisY);
	}
}

void VoxelMesher::TryBuildQuad2(int p1, int p2, int strideAxisB, int strideAxisC)
{
	float d1 = GetDensity(p1); // axis start
	float d2 = GetDensity(p2); // axis end

	bool isSolid1 = Voxel::IsSolid(d1);
	bool isSolid2 = Voxel::IsSolid(d2);

	if (isSolid1 == isSolid2) return;

	bool isNegativeFace = !isSolid1; // face normal points from solid to empty

	// Vertex Index
	uint32_t isoVertIndex1 = m_index1DToVertexIndex[p1];
	uint32_t isoVertIndex2 = m_index1DToVertexIndex[p1 - strideAxisB];
	uint32_t isoVertIndex3 = m_index1DToVertexIndex[p1 - strideAxisC];
	uint32_t isoVertIndex4 = m_index1DToVertexIndex[p1 - strideAxisB - strideAxisC];

	IsoVertex const& isoVert1 = m_isoVerts[isoVertIndex1];
	IsoVertex const& isoVert2 = m_isoVerts[isoVertIndex2];
	IsoVertex const& isoVert3 = m_isoVerts[isoVertIndex3];
	IsoVertex const& isoVert4 = m_isoVerts[isoVertIndex4];

	std::array<std::array<Voxel, 8>, 4> vertexVoxels;
	vertexVoxels[0] = isoVert1.m_corners;
	vertexVoxels[1] = isoVert2.m_corners;
	vertexVoxels[2] = isoVert3.m_corners;
	vertexVoxels[3] = isoVert4.m_corners;
	QuadMaterialInfo material = CalculateQuadMaterial(vertexVoxels);

	PerVertex v1(isoVertIndex1, material.matID1, material.matID2, material.blendValues[0]);
	PerVertex v2(isoVertIndex2, material.matID1, material.matID2, material.blendValues[1]);
	PerVertex v3(isoVertIndex3, material.matID1, material.matID2, material.blendValues[2]);
	PerVertex v4(isoVertIndex4, material.matID1, material.matID2, material.blendValues[3]);

	// Split the quad along shorter axis
	if (GetDistanceSquared3D(isoVert1.m_position, isoVert4.m_position) < GetDistanceSquared3D(isoVert2.m_position, isoVert3.m_position))
	{
		if (isNegativeFace)
		{
			m_structuredVertexBuffer->AddPerVertex(v1);
			m_structuredVertexBuffer->AddPerVertex(v4);
			m_structuredVertexBuffer->AddPerVertex(v2);
			m_structuredVertexBuffer->AddPerVertex(v1);
			m_structuredVertexBuffer->AddPerVertex(v3);
			m_structuredVertexBuffer->AddPerVertex(v4);
		}
		else
		{
			m_structuredVertexBuffer->AddPerVertex(v1);
			m_structuredVertexBuffer->AddPerVertex(v2);
			m_structuredVertexBuffer->AddPerVertex(v4);
			m_structuredVertexBuffer->AddPerVertex(v1);
			m_structuredVertexBuffer->AddPerVertex(v4);
			m_structuredVertexBuffer->AddPerVertex(v3);
		}
	}
	else
	{
		if (isNegativeFace)
		{
			m_structuredVertexBuffer->AddPerVertex(v2);
			m_structuredVertexBuffer->AddPerVertex(v3);
			m_structuredVertexBuffer->AddPerVertex(v4);
			m_structuredVertexBuffer->AddPerVertex(v2);
			m_structuredVertexBuffer->AddPerVertex(v1);
			m_structuredVertexBuffer->AddPerVertex(v3);
		}
		else
		{
			m_structuredVertexBuffer->AddPerVertex(v2);
			m_structuredVertexBuffer->AddPerVertex(v4);
			m_structuredVertexBuffer->AddPerVertex(v3);
			m_structuredVertexBuffer->AddPerVertex(v2);
			m_structuredVertexBuffer->AddPerVertex(v3);
			m_structuredVertexBuffer->AddPerVertex(v1);
		}
	}
}

Vec3 VoxelMesher::GetVertexFromHermiteData(int cubeIndex, float* cornerDensities, float isosurfaceValue)
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

	constexpr int MAX_ITERATION = 60;
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
			break;
		}

	}
	AABB3 box = AABB3(Vec3(0.f, 0.f, 0.f), Vec3(1.f, 1.f, 1.f));
	particlePosition = box.GetNearestPoint(particlePosition);

	return particlePosition;
}

Vec3 VoxelMesher::GetEdgeIntersectionsCentroid(int cubeIndex, float* cornerDensities, float isosurfaceValue)
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

Vec3 VoxelMesher::GetSdfGradient(Vec3 const& s, float* d)
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

void VoxelMesher::IsoVertex::SetNormal(const Vec3& normal)
{
	Quantization::OctEncodeNormal(normal, m_smoothNormal[0], m_smoothNormal[1]);
}
