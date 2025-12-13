#include "Game/DataChunk.hpp"
#include "Game/Voxel.hpp"
#include "Game/GameCommon.hpp"

DataChunk::DataChunk()
{

}

Voxel const& DataChunk::GetVoxel(int x, int y, int z) const
{
	if (!IsAllocated())
	{
		return Voxel::AIR;
	}

	GUARANTEE_OR_DIE(VoxelBlockUtils::IsLocalCoordsValid(x, y, z), "Invalid local coords");

	return (*m_block)[VoxelBlockUtils::GetLocalIndexFromLocalCoords(x, y, z)];
}

Voxel& DataChunk::GetVoxel(int x, int y, int z)
{
	EnsureAllocated();

	GUARANTEE_OR_DIE(VoxelBlockUtils::IsLocalCoordsValid(x, y, z), "Invalid local coords");

	return (*m_block)[VoxelBlockUtils::GetLocalIndexFromLocalCoords(x, y, z)];
}

Voxel const& DataChunk::GetVoxel(IntVec3 localCoords) const
{
	return GetVoxel(localCoords.x, localCoords.y, localCoords.z);
}

Voxel& DataChunk::GetVoxel(IntVec3 localCoords)
{
	return GetVoxel(localCoords.x, localCoords.y, localCoords.z);
}

void DataChunk::SetVoxel(int x, int y, int z, Voxel const& voxel)
{
	EnsureAllocated();

	GUARANTEE_OR_DIE(VoxelBlockUtils::IsLocalCoordsValid(x, y, z), "Invalid local coords");

	(*m_block)[VoxelBlockUtils::GetLocalIndexFromLocalCoords(x, y, z)] = voxel;
}

void DataChunk::Fill(Voxel const& voxel)
{
	EnsureAllocated();

	VoxelBlockUtils::Fill(*m_block, voxel);
}

bool DataChunk::TryCleanup()
{
	if (!m_block) return false; // 11/11 ??

	if (VoxelBlockUtils::IsAir(*m_block))
	{
		Clear();
		return true;
	}
	return false;
}

DataChunk::~DataChunk()
{
	Clear();
}

void DataChunk::SetPool(VoxelBlockPool* pool)
{
	m_pool = pool;
}

void DataChunk::Clear()
{
	if (m_block)
	{
		m_pool->Deallocate(m_block);
		m_block = nullptr;
	}
}

void DataChunk::EnsureAllocated()
{
	if (!m_block)
	{
		m_block = m_pool->Allocate();
		// Reset to all air
		VoxelBlockUtils::Fill(*m_block, Voxel::AIR);
	}
}
