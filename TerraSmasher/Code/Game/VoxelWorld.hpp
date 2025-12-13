#pragma once
#include "Game/VoxelBlockPool.hpp"
#include "Game/DataChunk.hpp"
#include "Game/WorldCommon.hpp"
#include "Game/NodeKey.hpp"
#include <array>
#include <unordered_map>
#include <unordered_set>

class DataChunk;
class SDFShape;
class SDF;
struct Plane3;

namespace VoxelWorldUtils
{
	inline Vec3 GetWorldVoxelCenterFromWorldVoxelCoords(IntVec3 const& worldCoords)
	{
		return Vec3(worldCoords) + Vec3(0.5f, 0.5f, 0.5f); // no offset, no scale
	}

	IntBox3 GetSafeWorldBoxRegion(IntBox3 const& oldRegion);

	inline bool IsValidWorldCoords(IntVec3 const& worldCoords)
	{
		return worldCoords.x >= 0 && worldCoords.x < WORLD_SIZE &&
			worldCoords.y >= 0 && worldCoords.y < WORLD_SIZE &&
			worldCoords.z >= 0 && worldCoords.z < WORLD_SIZE;
	}

	inline bool IsValidChunkCoords(IntVec3 const& chunkCoords)
	{
		return chunkCoords.x >= 0 && chunkCoords.x < CHUNKS_PER_AXIS &&
			chunkCoords.y >= 0 && chunkCoords.y < CHUNKS_PER_AXIS &&
			chunkCoords.z >= 0 && chunkCoords.z < CHUNKS_PER_AXIS;
	}

	inline int GetChunkIndexFromChunkCoords(IntVec3 const& chunkCoords)
	{
		return chunkCoords.x + chunkCoords.y * CHUNKS_PER_AXIS + chunkCoords.z * CHUNKS_PER_AXIS * CHUNKS_PER_AXIS;
	}

	inline IntVec3 GetLocalCoordsFromWorldCoords(IntVec3 const& worldCoords)
	{
		return IntVec3(worldCoords.x & CHUNK_MAX, worldCoords.y & CHUNK_MAX, worldCoords.z & CHUNK_MAX);
	}


	// WORLD_SIZE - 512, CHUNKS_PER_AXIS - 64
	// World Coords, Chunk Coords, local Coords
	// Chunk Index, localIndex
}

struct VoxelRaycastResult3D
{
	Vec3	m_rayStartPos;
	Vec3	m_rayFwdNormal;
	float	m_rayLength = 1.f;

	bool	m_didImpact = false;
	float	m_impactDist = 0.0f;
	Vec3	m_impactPos;
	Vec3	m_impactNormal;

	Voxel	m_voxel;
};




class VoxelWorld
{
public:
	VoxelWorld();
	~VoxelWorld();

	// No copy and move
	VoxelWorld(const VoxelWorld&) = delete;
	VoxelWorld& operator=(const VoxelWorld&) = delete;
	VoxelWorld(VoxelWorld&&) = delete;
	VoxelWorld& operator=(VoxelWorld&&) = delete;

	VoxelRaycastResult3D FastVoxelRaycast(Vec3 rayStart, Vec3 rayForwardNormal, float rayLength) const;

	// invalid coords, and not allocated chunk will return Voxel::AIR
	Voxel GetVirtualVoxel(IntVec3 const& worldCoords) const;


	// Node Key: Check VoxelOctree.hpp
	Voxel GetVoxelByNodeKey(IntVec3 const& coords, int lodLevel) const;

	void ExtractVoxelsForMesh(ChunkKey const& key, std::vector<Voxel>& voxels); // Try Clean up the DataChunk for LOD0

public:
	// Voxel Picker? In World Class
	// SmoothenWithSDF ( a sphere sdf, upper part is carving, bottom part is adding)

	//-----------------------------------------------------------------------------------------------
	// Voxel modification interface - return bool indicating whether any voxels were actually modified
	// also output the region
	// If Modified, void Chunk::RenderManager::MarkDirtyRegion(IntBox3 const& dirtyRegion); 
	// Using AABB3 SDFShape::GetExpandedBoundingBox() const; or other ways to determine region 
	// (Later can debug draw the region for 1s)
	//-----------------------------------------------------------------------------------------------
	
	// Bake SDF Shape (World Generation)
	// Rule: new_density = max(old_density, sdf_density), material override 
	bool BakeSDFShape(SDFShape const& shape, IntBox3& out_region);

	// Add operation (increase density, like a brush)
	// Rule: gradually increase density up to sdf_density limit, set material on first fill, keep old material
	// deltaDensity: amount of density to increase per operation
	bool AddWithSDF(SDF const* sdf, uint8_t deltaDensity, uint8_t matID1, uint8_t matID2, uint8_t blendValue, IntBox3& out_region);

	// Paint operation (modify material only, no density change)
	// Rule: modify material only on voxels where density > 0
	bool PaintWithSDF(SDF const* sdf, uint8_t matID1, uint8_t matID2, uint8_t blendValue, IntBox3& out_region);

	// Carve operation (decrease density)
	// Rule: gradually decrease density down to (255 - sdf_density) limit
	// deltaDensity: amount of density to decrease per operation
	bool CarveWithSDF(SDF const* sdf, uint8_t deltaDensity, IntBox3& out_region);

	// Flatten operation (constrain voxels to a plane)
	// Rule: Gradually modify voxel densities to match the signed distance to the plane
	// sdf: defines the affected region (voxels with SDF < 0 are modified)
	// plane: the target plane to flatten towards
	// deltaDensity: rate of density change per operation
	// matID1, matID2, blendValue: material for newly filled voxels
	// Returns true if any voxels were modified
	bool FlattenToPlane(SDF const* sdf, Plane3 const& plane, uint8_t deltaDensity,
		uint8_t matID1, uint8_t matID2, uint8_t blendValue, IntBox3& out_region);


private:
	// Get Voxel Ref, If not allocated, will allocated
	Voxel& GetOrCreateVoxelRef(IntVec3 const& worldCoords);
	// Get Voxel Ptr, If not allocated, will return nullptr (Air)
	Voxel* GetVoxelPtrIfAllocated(IntVec3 const& worldCoords);


	// LOD Cache invalidation helpers
	// Invalidate cache for a single modified voxel
	void InvalidateCacheForWorldCoords(IntVec3 const& worldCoords);
	// Batch invalidate cache for multiple modified voxels (more efficient)
	void InvalidateCacheForWorldCoordsSet(std::unordered_set<IntVec3> const& modifiedCoords);


	// Get By IntVec3
	//DataChunk& GetChunk(int cx, int cy, int cz);
	//DataChunk const& GetChunk(int cx, int cy, int cz) const;

	DataChunk* GetChunk(IntVec3 chunkCoords);
	DataChunk const* GetChunk(IntVec3 chunkCoords) const;


	// QueryBoxRegionVoxel: Input Box region, and LOD level, Output Voxel Buffer (linear indexing)

	// ChangeBoxRegionVoxel: using std::function visitor

	// Cleanup logic

	// GetVirtualVoxel(if not inside the world, ...)


private:
	// Notes: the order is important, when destructing, first destroy all DataChunk, then destroy the pool
	// If not working, considering let the pool own all Blocks, DataChunks do not need to return the block.
	VoxelBlockPool m_pool; // Voxel World own the pool
	std::array<DataChunk, TOTAL_CHUNKS> m_chunks;

	// LOD Cache: Only cache LOD1 and LOD2 merged voxels (LOD0 can be queried directly)
	// Marked as mutable because caching is an implementation detail that doesn't affect logical state
	// Cache entries are automatically invalidated when voxels are modified
	mutable std::unordered_map<NodeKey, Voxel, NodeKeyHash> m_lodCache;
};

