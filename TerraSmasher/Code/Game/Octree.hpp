#pragma once
/*
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Core/HashUtils.hpp"
#include <vector>
#include <array>
#include <unordered_map>
#include <optional>

// Notes: do not use std::optional in Array of Structures
// will have alignment padding for the flag in optional


using Level = uint8_t;        
using ChildIndex = uint8_t; 
using AllocPtr = uint32_t;

const AllocPtr EMPTY_ALLOC_PTR = UINT32_MAX;
const int OCTREE_CHILDREN = 8;

struct IntBox3 {
	IntVec3 m_mins;
	IntVec3 m_dimensions; // [mins, mins + dims)

	IntBox3() = default;
	IntBox3(IntVec3 const& mins, IntVec3 const& dimensions)
		: m_mins(mins)
		, m_dimensions(dimensions)
	{ }

	bool IsPointInside(IntVec3 point) const 
	{
		IntVec3 maxs = m_mins + m_dimensions;

		return point.x >= m_mins.x && point.x < maxs.x &&
			point.y >= m_mins.y && point.y < maxs.y &&
			point.z >= m_mins.z && point.z < maxs.z;
	}

	bool IsOverlap(const IntBox3& other) const 
	{
		IntVec3 maxs = m_mins + m_dimensions;
		IntVec3 otherMaxs = other.m_mins + other.m_dimensions;

		if (maxs.x <= other.m_mins.x || m_mins.x >= otherMaxs.x) return false;
		if (maxs.y <= other.m_mins.y || m_mins.y >= otherMaxs.y) return false;
		if (maxs.z <= other.m_mins.z || m_mins.z >= otherMaxs.z) return false;

		return true;
	}
};

struct NodeKey 
{
	Level m_level;
	IntVec3 m_coords;

	NodeKey() : m_level(0), m_coords() {}
	NodeKey(Level level, IntVec3 coords) : m_level(level), m_coords(coords) {}

	bool operator==(const NodeKey& other) const 
	{
		return m_level == other.m_level && m_coords == other.m_coords;
	}

	bool operator!=(const NodeKey& other) const 
	{
		return !(*this == other);
	}
};


struct NodeKeyHash 
{
	std::size_t operator()(const NodeKey& key) const 
	{
		size_t seed = 0;
		hash_combine(seed, key.m_level);
		hash_combine(seed, key.m_coords.x);
		hash_combine(seed, key.m_coords.y);
		hash_combine(seed, key.m_coords.z);
		return seed;

		//std::size_t h1 = std::hash<uint8_t>{}(key.level);
		//std::size_t h2 = std::hash<int>{}(key.coordinates.x);
		//std::size_t h3 = std::hash<int>{}(key.coordinates.y);
		//std::size_t h4 = std::hash<int>{}(key.coordinates.z);
		//return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
	}
};

struct NodePtr 
{
	Level m_level;
	AllocPtr m_allocPtr; // not initialized?

	NodePtr() : m_level(0), m_allocPtr(EMPTY_ALLOC_PTR) {}
	NodePtr(Level level, AllocPtr ptr) : m_level(level), m_allocPtr(ptr) {}

	bool IsNull() const { return m_allocPtr == EMPTY_ALLOC_PTR; }

	bool operator==(const NodePtr& other) const
	{
		return m_level == other.m_level && m_allocPtr == other.m_allocPtr;
	}
};

class OctreeShape
{
public:
	static ChildIndex LinearizeChild(IntVec3 offset) // [0, 1]
	{
		//return static_cast<ChildIndex>(offset.x + (offset.y << 1) + (offset.z << 2));
		return (offset.x & 1) |
			((offset.y & 1) << 1) |
			((offset.z & 1) << 2);
	}

	static IntVec3 DelinearizeChild(ChildIndex index)
	{
		return IntVec3(index & 1, (index >> 1) & 1, (index >> 2) & 1);
	}

	static IntVec3 GetParentKey(IntVec3 key)
	{
		return key >> 1;
	}

	static IntVec3 GetAncestorKey(IntVec3 key, uint32_t levelsUp)
	{
		return key >> static_cast<int>(levelsUp);
	}

	static IntVec3 GetMinChildKey(IntVec3 key)
	{
		return key << 1;
	}

	// Node size of this LOD
	static int GetNodeSizeAtLevel(Level level)
	{
		return 1 << level;
	}

	static IntBox3 GetNodeBounds(IntVec3 coords, Level level)
	{
		int size = GetNodeSizeAtLevel(level);
		IntVec3 mins = coords * size;
		IntVec3 dimensions = IntVec3(size, size, size);
		return IntBox3(mins, dimensions);
	}

};




template<typename T>
class NodeAllocator
{
private:
	std::vector<T> m_values;
	std::vector<std::array<AllocPtr, OCTREE_CHILDREN>> m_pointers; // EMPTY_ALLOC_PTR default value
	std::vector<bool> m_allocated; // allocated, occupied

	std::vector<AllocPtr> m_freeList;

	bool m_isLeafLevel = false;

public:
	explicit NodeAllocator(bool isLeaf) : m_isLeafLevel(isLeaf) {}
	// Does this class know what level they are in ?


	// insert leaf or branch

	// Insert a leaf node (level = 0, only has value no child pointers)
	// Insert a branch node (level > 0, has value and child pointers)

	AllocPtr Insert(const T& value)
	{
		AllocPtr ptr; // not initialized

		if (!m_freeList.empty())
		{
			// Reuse a slot in the free list
			ptr = m_freeList.back();
			m_freeList.pop_back();
			m_values[ptr] = value;
			m_allocated[ptr] = true;

			if (!m_isLeafLevel) //  && ptr < pointers.size()
			{
				m_pointers[ptr].fill(EMPTY_ALLOC_PTR);
			}

		}
		else
		{
			// assign a new slot
			ptr = static_cast<AllocPtr>(m_values.size());
			m_values.push_back(value);
			m_allocated.push_back(true);

			if (!m_isLeafLevel)
			{
				std::array<AllocPtr, 8> children;
				children.fill(EMPTY_ALLOC_PTR);
				m_pointers.push_back(children);
			}
		}

		return ptr;
	}

	//void Remove(AllocPtr ptr)
	//{
	//	if (ptr < m_allocated.size() && m_allocated[ptr])
	//	{
	//		m_allocated[ptr] = false;
	//		m_freeList.push_back(ptr);
	//	}
	//}

	std::pair<std::optional<T>, std::optional<std::array<AllocPtr, 8>>> Remove(AllocPtr ptr) 
	{
		if (ptr >= m_allocated.size() || !m_allocated[ptr])
		{
			return { std::nullopt, std::nullopt };
		}

		m_allocated[ptr] = false;
		m_freeList.push_back(ptr);

		std::optional<T> value = m_values[ptr];
		std::optional<std::array<AllocPtr, 8>> children;

		if (!m_isLeafLevel && ptr < m_pointers.size()) 
		{
			children = m_pointers[ptr];
		}

		return { value, children };
	}

	bool ContainsNode(AllocPtr ptr) const 
	{
		return ptr < m_allocated.size() && m_allocated[ptr];
	}

	T const* GetValue(AllocPtr ptr) const 
	{
		if (ContainsNode(ptr))
		{
			return &m_values[ptr];
		}
		return nullptr;
	}

	T * GetMutableValue(AllocPtr ptr) const
	{
		if (ContainsNode(ptr))
		{
			return &m_values[ptr];
		}
		return nullptr;
	}

	const std::array<AllocPtr, 8>* GetChildren(AllocPtr ptr) const {
		if (m_isLeafLevel || !ContainsNode(ptr) || ptr >= m_pointers.size()) 
		{
			return nullptr;
		}
		return &m_pointers[ptr];
	}

	std::array<AllocPtr, 8>* GetMutableChildren(AllocPtr ptr) 
	{
		if (m_isLeafLevel || !ContainsNode(ptr) || ptr >= m_pointers.size())
		{
			return nullptr;
		}
		return &m_pointers[ptr];
	}

	void SetChildPointer(AllocPtr parentPtr, ChildIndex childIndex, AllocPtr childPtr) 
	{
		auto* children = GetMutableChildren(parentPtr);
		if (children) 
		{
			(*children)[childIndex] = childPtr;
		}
	}

	size_t GetAllocatedCount() const 
	{
		return m_values.size() - m_freeList.size();
	}

	size_t GetCapacity() const 
	{
		return m_values.size();
	}

};

struct RootNode // it can just be an AllocPtr
{
	AllocPtr m_selfPtr;
	std::optional<AllocPtr> m_parentPtr; // Optional if have tree allocator

	RootNode() : m_selfPtr(EMPTY_ALLOC_PTR), m_parentPtr(std::nullopt) {}
	RootNode(AllocPtr selfPtr) : m_selfPtr(selfPtr), m_parentPtr(std::nullopt) {}
	RootNode(AllocPtr selfPtr, AllocPtr parentPtr) : m_selfPtr(selfPtr), m_parentPtr(parentPtr) {}
};


// Avoid twice search
template<typename T>
class NodeEntry
{
public:
	enum class Type { Occupied, Vacant };

private:


};


enum class VisitCommand {
	Continue,       
	SkipDescendants  
};



template<typename T>
class VoxelOctree
{
private:
	std::unordered_map<NodeKey, RootNode, NodeKeyHash> m_rootNodes;
	std::vector<NodeAllocator<T>> m_allocators;
	Level m_treeHeight; // LOD 0~2 height = 3?

public:
	explicit VoxelOctree(Level height) : m_treeHeight(height)
	{
		m_allocators.reserve(height);

		for (Level i = 0; i < height; ++i)
		{
			m_allocators.emplace_back(i == 0);
		}

	}
	//-----------------------------------------------------------------------------------------------
	// Basics
	//-----------------------------------------------------------------------------------------------
	Level GetHeight() const { return m_treeHeight; }
	Level GetRootLevel() const { return m_treeHeight - 1; }

	IntBox3 GetNodeBounds(NodeKey const& key) const 
	{
		return OctreeShape::GetNodeBounds(key.m_coords, key.m_level);
	}

	// Given any NodeKey calculate its root node key
	NodeKey GetAncestorRootKey(NodeKey const& descendantKey) const
	{
		uint32_t levelsUp = GetRootLevel() - descendantKey.m_level;
		IntVec3 coordinates = OctreeShape::GetAncestorKey(descendantKey.m_coords, levelsUp);
		return NodeKey(GetRootLevel(), coordinates);
	}

	//-----------------------------------------------------------------------------------------------
	// Root node operations
	//-----------------------------------------------------------------------------------------------
	std::pair<RootNode, std::optional<T>> InsertRoot(const NodeKey& key, const T& value) 
	{
		GUARANTEE_OR_DIE(key.m_level == GetRootLevel(), "The NodeKey is not root level.")

		auto& alloc = m_allocators[key.m_level];
		auto it = m_rootNodes.find(key);

		if (it != m_rootNodes.end()) 
		{
			// root node exists, update value
			RootNode rootNode = it->second;
			T* existingValue = alloc.GetMutableValue(rootNode.m_selfPtr);
			std::optional<T> oldValue = *existingValue;
			*existingValue = value;
			return { rootNode, oldValue };
		}
		else 
		{
			// create a new root node
			AllocPtr rootPtr = alloc.Insert(value);
			RootNode rootNode(rootPtr);
			m_rootNodes[key] = rootNode;
			return { rootNode, std::nullopt };
		}
	}

	std::optional<>
};

*/