#pragma once
#include "Game/WorldCommon.hpp"
#include "Game/Voxel.hpp"
#include <vector>
#include <stack>
#include <atomic>
#include <mutex>

using VoxelBlock = std::array<Voxel, CHUNK_VOLUME>;

// Some Voxel Block visit clear reset change method, first use linear one, then change to morton encoding
namespace VoxelBlockUtils
{
	// Linear indexing #ToDo Test Morton Encoding performance
	inline int GetLocalIndexFromLocalCoords(int x, int y, int z)
	{
		return x + (y << CHUNK_BITS) + (z << (CHUNK_BITS + CHUNK_BITS));
	}

	//inline int GetIndexFromLocalCoords(IntVec3 const& coords)
	//{
	//	return GetIndexFromLocalCoords(coords.x, coords.y, coords.z);
	//}

	inline void Fill(VoxelBlock& block, Voxel const& voxel)
	{
		block.fill(voxel);
	}

	inline bool IsEmpty(VoxelBlock const& block)
	{
		return std::all_of(block.begin(), block.end(),
			[](const Voxel& v) { return v.IsEmpty(); });
	}

	inline bool IsSolid(VoxelBlock const& block)
	{
		return std::all_of(block.begin(), block.end(),
			[](const Voxel& v) { return v.IsSolid(); });
	}

	inline bool IsAir(VoxelBlock const& block)
	{
		return std::all_of(block.begin(), block.end(),
			[](const Voxel& v) { return v.m_density == 0; });
	}

	inline bool IsLocalCoordsValid(int x, int y, int z)
	{
		return x >= 0 && x < CHUNK_SIZE &&
			y >= 0 && y < CHUNK_SIZE &&
			z >= 0 && z < CHUNK_SIZE;
	}


}



class VoxelBlockPool
{
public:
	VoxelBlockPool(size_t initialCapacity = POOL_INITIAL_CAPACITY);
	~VoxelBlockPool();;

	VoxelBlock* Allocate();
	void Deallocate(VoxelBlock* block); // Remember to Deallocate the allocated block before destroy the pool

	// Debug
	size_t GetAvailableCount() const;
	size_t GetTotalAllocated() const;


private:
	void Clear();

private:
	std::vector<VoxelBlock*> m_pool;

	mutable std::mutex m_mutex;
	std::atomic<size_t> m_totalAllocated{ 0 };


};

