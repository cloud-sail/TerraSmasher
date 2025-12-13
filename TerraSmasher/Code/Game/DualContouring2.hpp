#pragma once
#include "IntGrid3.hpp"
#include "Engine/Core/Vertex_Terrain.hpp"
#include <vector>

class VertexBuffer;
class IndexBuffer;

/***************************************************************************************************

* P is the voxel corner, d is the sdf distance/density value located at the center of the voxel
* eight d (four d in 2D) form eight corner of a lattice
	P---P---P
	| d | d |
	P---P---P
	| d | d |
	P---P---P
* v is the isosurface vertex in the lattice
* There is at most one isosurface vertex in one lattice. (no vertex when 8 corner are fully inside/outside the sdf shape, same sign)
	d---d
	| v |
	d---d

* Chunk is 2x2x2, but it need (n+2)x(n+2)x(n+2) 4x4x4 voxel data
	P---P---P---P---P
	| d | d | d | d |
	P---P---P---P---P
	| d | d | d | d |
	P---P---P---P---P
	| d | d | d | d |
	P---P---P---P---P
	| d | d | d | d |
	P---P---P---P---P
*
	d---d---d---d
	| v | v | v |
	d---d---d---d
	| v | v | v |
	d---d---d---d
	| v | v | v |
	d---d---d---d

Given a SDF and grid defining the sampling locations, the basic dual isosurface algorithm works as follows

Active edges: For each edge in the sampling grid, determine if it intersects the boundary of the SDF. We call those edges with intersections active edges.
Edge intersection: For each active edge find the intersection point with the boundary of the surface along the edge.
Vertex placement: For each grid (active) voxel with at least one active edge, determine a single vertex location.
Face generation: For each active edge create a quadliteral connecting the vertices of the four active voxels sharing this active edge.

*/

//-----------------------------------------------------------------------------------------------
class DualContouring2
{
public:
	DualContouring2();
	~DualContouring2();

public:
	void RegenerateMesh(IntGrid3 const& grid, std::vector<float> const& sdfValues, float blockyBlendFactor = 0.f);

	bool m_isReady = false;

	VertexBuffer* m_vertexBuffer = nullptr;
	IndexBuffer* m_indexBuffer = nullptr;

private:
	// local/Model Coordinate: voxelEdgeLength  = 1.f, start at (0, 0, 0), 
	// Density Point is at the center of the voxel
	// ModelToWorld: scale, orientation, position (Mat44)

	IntGrid3 m_grid;
	std::vector<float> m_sdfValues; // Now: >0 is empty, < 0 is solid. #ToDo a voxel list: in voxel 0-255

	std::vector<Vec3> m_cornerGradients; // normalized f(x+1) - f(x-1), ...

	float m_isosurfaceValue = 0.f;

	float m_blockyBlendValue = 0.f; // 0 is surface nets 1 is blocky

	// Intermediate Values
	static const uint32_t INVALID_VERTEX_INDEX = static_cast<uint32_t>(-1); // just for debug, maybe remove latter
	// initial value is invalid vertex index, it is a map from linear index to vertex index in m_vertices
	std::vector<uint32_t> m_index1DToVertexIndex;
	std::vector<std::pair<int, IntVec3>> m_isosurfaceVertexIndexs; // pushed with m_vertices

	// if the cube meets requirement push back a vertex in it
	// position; normal; matID(0~255); density (0~255)
	std::vector<Vertex_Terrain> m_vertices;
	std::vector<unsigned int> m_indices;


private:
	void ResetData();
	void RecalculateCornerGradient();

	void EstimateIsosurfaceVertexAndNormal();
	void BuildMeshQuads();

	// p1 - axis start index, p2 - axis end index
	void TryBuildQuad(int p1, int p2, int strideAxisB, int strideAxisC);

	static Vec3 GetVertexFromHermiteData(int cubeIndex, float* cornerDensities, Vec3* cornerGradients, float isosurfaceValue);


	static Vec3 GetEdgeIntersectionsCentroid(int cubeIndex, float* cornerDensities, float isosurfaceValue);
	static Vec3 GetSdfGradient(Vec3 const& p, float* cornerDensities); // the Direction of Steepest Ascent

	inline float GetDensity(int index1D) { return m_sdfValues[index1D]; } // #ToDo return (float)voxel.density
	inline bool IsDensitySolid(float d) { return d <= m_isosurfaceValue; } // d<=0.f -> solid // #ToDo: change it to den >= 128.f -> solid

};


// index3D localCoords
// index1D linearIndex
// strideX strideY strideZ


// Deprecated: A new Vertex Layout: position; normal; matID1(0~255); matID2(0~255); material weight (0.f~1.f);
// Blend Material, for each corner, if the corner is solid, take top 2 corners by density
// MatID1 = A MatID2 = B. if no top 2, materialWeight = 0.f, (1-weight)*A +weight*B
// top 2 densities of material ID from 8 corner and blend

