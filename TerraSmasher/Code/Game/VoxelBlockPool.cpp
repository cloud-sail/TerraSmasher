#include "Game/VoxelBlockPool.hpp"
#include "Game/Voxel.hpp"

VoxelBlockPool::VoxelBlockPool(size_t initialCapacity /*= POOL_INITIAL_CAPACITY*/)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_pool.reserve(initialCapacity);
}

VoxelBlockPool::~VoxelBlockPool()
{
	Clear();
}

VoxelBlock* VoxelBlockPool::Allocate()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_pool.empty())
	{
		m_totalAllocated.fetch_add(1, std::memory_order_relaxed);
		return new VoxelBlock();
	}
	else
	{
		VoxelBlock* block = m_pool.back();
		m_pool.pop_back();
		return block;
	}
}

void VoxelBlockPool::Deallocate(VoxelBlock* block)
{
	if (block)
	{
		// The block is not cleaned
		std::lock_guard<std::mutex> lock(m_mutex);
		m_pool.push_back(block);
	}
}

size_t VoxelBlockPool::GetAvailableCount() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_pool.size();
}

size_t VoxelBlockPool::GetTotalAllocated() const
{
	return m_totalAllocated.load(std::memory_order_relaxed);
}

void VoxelBlockPool::Clear()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	for (auto* block : m_pool)
	{
		delete block;
	}
	m_pool.clear();
	m_totalAllocated.store(0, std::memory_order_relaxed);
}


