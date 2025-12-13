#pragma once
#include "Game/VoxelBlockPool.hpp"

class Voxel;
class VoxelBlockPool;


class DataChunk
{
public:
	DataChunk();
	~DataChunk();

	// Must be set before using
	void SetPool(VoxelBlockPool* pool);

	// Prevent copy and move, because of fixed size world.
	DataChunk(const DataChunk&) = delete;
	DataChunk& operator=(const DataChunk&) = delete;
	DataChunk(DataChunk&&) = delete;
	DataChunk& operator=(DataChunk&&) = delete;

	bool IsAllocated() const { return m_block != nullptr; }

	// Access Voxels
	Voxel const& GetVoxel(int x, int y, int z) const;
	Voxel const& GetVoxel(IntVec3 localCoords) const;

	Voxel& GetVoxel(int x, int y, int z);
	Voxel& GetVoxel(IntVec3 localCoords);

	void SetVoxel(int x, int y, int z, Voxel const& voxel);

	// Fast access through index
	inline const Voxel& GetVoxelUnsafe(int index) const
	{
		return (*m_block)[index];
	}

	inline Voxel& GetVoxelUnsafe(int index)
	{
		EnsureAllocated();
		return (*m_block)[index];
	}


	// Batch operation
	void Fill(Voxel const& voxel);


	// Try Clean-up: After subtracting voxel, you can deallocate the chunk if the density of every voxels is 0
	// return: bool - the chunk has been deallocated
	bool TryCleanup();

	// Some boolean for getting value quickly, or just write it in Get Voxel?
	//bool IsAllAir() const; // If not allocated, the voxels are all air
	// Think a good way later, maybe it is a very limited improvement
	//bool IsAllEmpty() const; // If all chunks for generating meshes are all empty, no need to generate 
	//bool IsAllSolid() const; // If all chunks for generating meshes are all solid, no need to generate

private:
	void Clear();

	// Delay Allocation
	void EnsureAllocated();

private:
	VoxelBlockPool* m_pool = nullptr;
	VoxelBlock* m_block = nullptr;
};

// VoxelWorld:   std::array<VoxelDataChunk, TOTAL_CHUNKS> m_chunks;