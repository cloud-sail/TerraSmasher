#pragma once
#include "Engine/Core/HashUtils.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/IntBox3.hpp"
#include "Engine/Math/AABB3.hpp"

#include <array>
#include <vector>
#include <functional>
#include <stack>

struct Vec3;
struct Frustum;


class Voxel;
//-----------------------------------------------------------------------------------------------
// Configs
//-----------------------------------------------------------------------------------------------
// Notes: #ToDo,
// - Change MESH_CHUNK_EXPANSION to 2 will solve seams between lod meshes by overlapping
// - Change the merging algorithm(8 child voxel to 1 parent)
// - Chunk Bits 3 -> 4



constexpr int CHUNK_BITS = 3; // Also the lod 0 chunk size
constexpr int CHUNK_SIZE = 1 << CHUNK_BITS;
constexpr int CHUNK_MAX = CHUNK_SIZE - 1;


constexpr int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

constexpr size_t POOL_INITIAL_CAPACITY = 1024;
constexpr size_t POOL_GROW_SIZE = 512;

constexpr int CHUNKS_PER_AXIS = 1 << (9 - CHUNK_BITS); // 512^3 World
constexpr int TOTAL_CHUNKS = CHUNKS_PER_AXIS * CHUNKS_PER_AXIS * CHUNKS_PER_AXIS;
constexpr int WORLD_SIZE = CHUNK_SIZE * CHUNKS_PER_AXIS;

constexpr int NUM_LOD_LEVELS = 3; // Remember to change ChunkShape::SelectLODByDistanceSq
// Why separate? LOD is for visual chunk, original is for data chunk
constexpr int LOD0_CHUNK_BITS = CHUNK_BITS;
constexpr int LOD0_CHUNK_SIZE = CHUNK_SIZE;
constexpr int LOD0_CHUNKS_PER_AXIS = CHUNKS_PER_AXIS;
constexpr int LOD0_TOTAL_CHUNKS = LOD0_CHUNKS_PER_AXIS * LOD0_CHUNKS_PER_AXIS * LOD0_CHUNKS_PER_AXIS;
//constexpr float LOD3_DISTANCE_THRESHOLD = 512.f; // >= Threshold, use lod3
//constexpr float LOD2_DISTANCE_THRESHOLD = 256.f; // >= Threshold, use lod2
//constexpr float LOD1_DISTANCE_THRESHOLD = 128.f; // >= Threshold, use lod1

constexpr float LOD_DISTANCE_THRESHOLD[NUM_LOD_LEVELS - 1] = {
	128.f,	// LOD1
	256.f	// LOD2
};

constexpr int MESH_CHUNK_EXPANSION = 2; // 8^3 -> 10^3
constexpr int VOXEL_BUFFER_SIZE_PER_AXIS = (CHUNK_SIZE + 2 * MESH_CHUNK_EXPANSION); // 8 + 2
constexpr int TOTAL_VOXEL_BUFFER_SIZE = VOXEL_BUFFER_SIZE_PER_AXIS * VOXEL_BUFFER_SIZE_PER_AXIS * VOXEL_BUFFER_SIZE_PER_AXIS; // 10^3

// Job Config
// Max queued job
// 
constexpr int MAX_GENERATING_MESH_CHUNKS = 64;


// Same Lod, ChunkKey.m_chunkCoords * LOD0_CHUNK_SIZE
//struct VoxelKey
//{
//	IntVec3 m_coords;
//	int m_lodLevel;
//};
//enum class LODSamplingMode
//{
//	MIN_CORNER,
//	AVERAGE,
//	MAX_DENSITY,
//	MIN_DENSITY, 
//	MAJORITY   
//};




//-----------------------------------------------------------------------------------------------
// Lod Level can be uint8_t
// Notes: 
// Lod0 
// mins (LOD0_CHUNK_SIZE, LOD0_CHUNK_SIZE, LOD0_CHUNK_SIZE) * m_chunkCoords, 
// dims (LOD0_CHUNK_SIZE, LOD0_CHUNK_SIZE, LOD0_CHUNK_SIZE)
struct ChunkKey
{
	IntVec3 m_chunkCoords;
	int		m_lodLevel = 0;

	ChunkKey() : m_chunkCoords(), m_lodLevel(0) {}
	ChunkKey(IntVec3 index, int lod) : m_chunkCoords(index), m_lodLevel(lod) {}


	bool operator==(ChunkKey const& other) const 
	{
		return m_chunkCoords == other.m_chunkCoords && m_lodLevel == other.m_lodLevel;
	}

	bool operator!=(ChunkKey const& other) const 
	{
		return !(*this == other);
	}



};

struct ChunkKeyHash
{
	std::size_t operator()(const ChunkKey& key) const
	{
		size_t seed = 0;
		hash_combine(seed, key.m_lodLevel);
		hash_combine(seed, key.m_chunkCoords.x);
		hash_combine(seed, key.m_chunkCoords.y);
		hash_combine(seed, key.m_chunkCoords.z);
		return seed;

		//std::size_t h1 = std::hash<uint8_t>{}(key.level);
		//std::size_t h2 = std::hash<int>{}(key.coordinates.x);
		//std::size_t h3 = std::hash<int>{}(key.coordinates.y);
		//std::size_t h4 = std::hash<int>{}(key.coordinates.z);
		//return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
	}
};


class CoordsUtils
{
public:
	static const std::array<int, NUM_LOD_LEVELS> LOD_SIZES;

	static int LinearizeChild(IntVec3 offset);
	static IntVec3 DelinearizeChild(int index);
	static IntVec3 GetParentKey(IntVec3 key);
	static IntVec3 GetAncestorKey(IntVec3 key, uint32_t levelsUp);
	static IntVec3 GetMinChildKey(IntVec3 key);
	static int GetChunkSizeForLOD(int lodLevel);
	static int GetChunksPerAxis(int lodLevel);
	static int GetTotalChunksForLOD(int lodLevel);
	static int GetLODShift(int lodLevel);
	static int SelectLODByDistanceSq(float distanceSq);
	static IntBox3 GetChunkBounds(IntVec3 coords, int level);
	static std::vector<ChunkKey> GetChildChunkKeys(IntVec3 parentCoords, int level);
	static int GetLODFromChunkSize(int chunkSize);


};

// World Coordinates
void TraverseOctreeForLOD(IntBox3 const& nodeBox, Vec3 const& cameraPos, Frustum const& frustum, std::vector<ChunkKey>& out_chunks);


//-----------------------------------------------------------------------------------------------
// An experimental way to traversal Octree
// TraverseOctreeWithVisitor can finish many tasks without changing the core traversal logic
// Separate detailed business logic from traversal logic

enum class TraversalCommand
{
	Continue,          // Continue to traverse the 8 child nodes
	SkipDescendants,   // Skip all descendants of the current node (pruning the subtree)
	UseThisNode,       // Use the current node and stop further subdivision
	StopTraversal      // Stop the entire traversal immediately (early termination)
};

// Assume nodeBox is valid
struct OctreeNodeInfo
{
	IntBox3 box;              // Node Grid Bounding Box
	int nodeSize;             // Node Size (length of Cube Size)
	int depth;                // Current Depth (0 is the root)
	Vec3 centerWorld;         // Center in World Space
	AABB3 aabbWorld;          // AABB in World Space

	OctreeNodeInfo() = default;
	OctreeNodeInfo(IntBox3 const& nodeBox, int nodeDepth);
};


struct TraversalConfig
{
	int minNodeSize = 0;      // 0 is not limiting, or stop subdividing when smaller than this
	int maxDepth = -1;        // -1 is not limiting depth

	TraversalConfig() = default;
	TraversalConfig(int minSize) : minNodeSize(minSize) {}
};

void TraverseOctreeWithVisitor(
	IntBox3 const& rootBox,
	std::function<TraversalCommand(OctreeNodeInfo const&)> visitor
);

void TraverseOctreeWithVisitor(
	IntBox3 const& rootBox,
	std::function<TraversalCommand(OctreeNodeInfo const&)> visitor,
	TraversalConfig const& config
);

//-----------------------------------------------------------------------------------------------

std::vector<ChunkKey> GetVisibleChunksWithLOD(Vec3 const& cameraPos, Frustum const& frustum);

std::vector<ChunkKey> GetVisibleChunksWithLOD_Safe(Vec3 const& cameraPos, Frustum const& frustum);

std::vector<ChunkKey> GetDirtyChunksInRegion(IntBox3 const& dirtyRegion);

//int CountNodesInFrustum(Frustum const& frustum);
//
//// Find All lod nodes contains point
//std::vector<IntBox3> FindNodesContainingPoint(IntVec3 const& point);
//
//std::vector<ChunkKey> GetAllChunksAtLOD(int targetLOD);
//
//std::vector<ChunkKey> FindChunksIntersectingBox(IntBox3 const& queryBox, int targetLOD);
//
//std::vector<ChunkKey> FindNearestChunks(Vec3 const& cameraPos, int count, int targetLOD);
