#pragma once
#include "Game/IntGrid3.hpp"
#include "Game/Voxel.hpp"
#include "Engine/Core/Vertex_Terrain.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/IntVec3.hpp"
#include <array>
#include <vector>

class TerrainStructuredBuffer;


//-----------------------------------------------------------------------------------------------
enum class MeshingMode
{
	SURFACE_NETS,
	DUAL_CONTOURING,
	DUAL_CONTOURING_QEF, // Bad result
};

//-----------------------------------------------------------------------------------------------
class VoxelMesher
{
public:
	// Do not new this class, it is a tool class, it may be put in a job(multi threading)
	VoxelMesher(MeshingMode mode);
	~VoxelMesher() = default;

	// Input: Voxel data, Grid Size
	// Output: vertices, indices
	// ModelSpace: no rotation, cube side length is 1, center is offsetFromMinCorner
	void GenerateMesh(std::vector<Voxel> const& voxels, IntGrid3 const& grid, std::vector<Vertex_Terrain>& vertices, std::vector<unsigned int>& indices, float blockyBlendFactor = 0.f, Vec3 offsetFromMinCorner = Vec3(1.f, 1.f, 1.f));

	// 
	// void GenerateMesh() using Structured Buffer
	void GenerateMesh(std::vector<Voxel> const& voxels, IntGrid3 const& grid, TerrainStructuredBuffer* structuredVertexBuffer, float blockyBlendFactor = 0.f, Vec3 offsetFromMinCorner = Vec3(1.f, 1.f, 1.f));

private:
	//-----------------------------------------------------------------------------------------------
	// Generate IsoVertices
	//-----------------------------------------------------------------------------------------------
	// 1. Naive Surface Nets
	void GenerateIsoVertex_SurfaceNets();
	// 2. Dual Contouring (none qef)
	void GenerateIsoVertex_DualContouring();


	//-----------------------------------------------------------------------------------------------
	// Building Quads
	//-----------------------------------------------------------------------------------------------
	// 1. Vertex Buffer Index buffer Method:
	void BuildMeshQuads();
	// p1 - axis start index, p2 - axis end index
	void TryBuildQuad(int p1, int p2, int strideAxisB, int strideAxisC);

	// 2. Experimental
	void BuildMeshQuads2();
	void TryBuildQuad2(int p1, int p2, int strideAxisB, int strideAxisC);

private:
	static Vec3 GetVertexFromHermiteData(int cubeIndex, float* cornerDensities, float isosurfaceValue);
	static Vec3 GetEdgeIntersectionsCentroid(int cubeIndex, float* cornerDensities, float isosurfaceValue);

	inline float GetDensity(int index1D) const
	{
		return (*m_voxelsPtr)[index1D].GetDensityUNorm();
	}

public:
	static Vec3 GetSdfGradient(Vec3 const& p, float* cornerDensities); // the Direction of Steepest Ascent

private:
	MeshingMode m_mode = MeshingMode::SURFACE_NETS;

	// Temporary Data (Be careful)
	std::vector<Voxel> const* m_voxelsPtr = nullptr; 
	std::vector<Vertex_Terrain>* m_verticesPtr = nullptr;
	std::vector<unsigned int>* m_indicesPtr;
	TerrainStructuredBuffer* m_structuredVertexBuffer = nullptr;


	//VertexBuffer* m_vertexBuffer = nullptr;
	//IndexBuffer* m_indexBuffer = nullptr;


	IntGrid3 m_grid;
	Vec3 m_offsetFromMinCorner;
	float m_blockyBlendValue = 0.f; // 0 is smooth mesh 1 is blocky (minecraft)

	static const uint32_t INVALID_VERTEX_INDEX = static_cast<uint32_t>(-1);

	// Index1D is the min corner of a lattice. A lattice contains at most one isosurface vertex
	std::vector<uint32_t> m_index1DToVertexIndex;
	std::vector<std::pair<uint32_t, IntVec3>> m_validIsoVertexIndexes; // quicker to for loop this

	struct IsoVertex
	{
	public:
		std::array<Voxel, 8> m_corners;
		Vec3 m_position;
		uint8_t m_smoothNormal[2] = { 0, 0 };

	public:
		void SetNormal(const Vec3& normal);

	};

	// if the lattice contains an isosurface vertex, push one struct in the vector
	std::vector<IsoVertex> m_isoVerts;

	// reserve for 2 quads for one vertex is a good estimation? 3 is the maximum
	//std::vector<Vertex_Terrain> m_vertices;
	//std::vector<unsigned int> m_indices;
};




/*
# Learning Resource
https://bonsairobo.medium.com/smooth-voxel-mapping-a-technical-deep-dive-on-real-time-surface-nets-and-texturing-ef06d0f8ca14
https://github.com/cheind/sdftoolbox/blob/main/doc/SDF.md
Analysis and Acceleration of High Quality Isosurface Contouring - by LEONARDO AUGUSTO SCHMITZ
https://0fps.net/2012/07/12/smooth-voxel-terrain-part-2/

Given a SDF and grid defining the sampling locations, the basic dual isosurface algorithm works as follows:
- Active edges: For each edge in the sampling grid, determine if it intersects the boundary of the SDF. We call those edges with intersections active edges.
- Edge intersection: For each active edge find the intersection point with the boundary of the surface along the edge.
- Vertex placement: For each grid (active) voxel with at least one active edge, determine a single vertex location.
- Face generation: For each active edge create a quadliteral connecting the vertices of the four active voxels sharing this active edge.








*/



