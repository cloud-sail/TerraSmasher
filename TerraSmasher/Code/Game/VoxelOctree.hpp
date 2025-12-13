//#pragma once
//#include "Game/Voxel.hpp"
//#include "Engine/Core/HashUtils.hpp"
//#include "Engine/Math/IntVec3.hpp"
//#include "Engine/Math/IntBox3.hpp"
//
//#include <vector>
//#include <unordered_map>
//#include <array>
//#include <optional>
//#include <functional>
//#include <stack>
//#include <queue>
//
////-----------------------------------------------------------------------------------------------
//// Base Types
////-----------------------------------------------------------------------------------------------
//using Level = uint8_t;
//using ChildIndex = uint8_t;
//using AllocPtr = uint32_t;
//using RootNode = AllocPtr;
//
//constexpr AllocPtr EMPTY_ALLOC_PTR = UINT32_MAX;
//
////-----------------------------------------------------------------------------------------------
//struct NodeKey
//{
//	Level m_level;
//	IntVec3 m_coords;
//
//	NodeKey() : m_level(0), m_coords() {}
//	NodeKey(Level level, IntVec3 coords) : m_level(level), m_coords(coords) {}
//
//	bool operator==(const NodeKey& other) const
//	{
//		return m_level == other.m_level && m_coords == other.m_coords;
//	}
//
//	bool operator!=(const NodeKey& other) const
//	{
//		return !(*this == other);
//	}
//};
//
//struct NodeKeyHash
//{
//	std::size_t operator()(const NodeKey& key) const
//	{
//		size_t seed = 0;
//		hash_combine(seed, key.m_level);
//		hash_combine(seed, key.m_coords.x);
//		hash_combine(seed, key.m_coords.y);
//		hash_combine(seed, key.m_coords.z);
//		return seed;
//
//		//std::size_t h1 = std::hash<uint8_t>{}(key.level);
//		//std::size_t h2 = std::hash<int>{}(key.coordinates.x);
//		//std::size_t h3 = std::hash<int>{}(key.coordinates.y);
//		//std::size_t h4 = std::hash<int>{}(key.coordinates.z);
//		//return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
//	}
//};
//
////-----------------------------------------------------------------------------------------------
//struct NodePtr 
//{
//	Level m_level;
//	AllocPtr m_allocPtr;
//
//	NodePtr() : m_level(0), m_allocPtr(EMPTY_ALLOC_PTR) {}
//	NodePtr(Level level, AllocPtr ptr) : m_level(level), m_allocPtr(ptr) {}
//
//	bool IsNull() const { return m_allocPtr == EMPTY_ALLOC_PTR; }
//
//	bool operator==(NodePtr const& other) const 
//	{
//		return m_level == other.m_level && m_allocPtr == other.m_allocPtr;
//	}
//};
//
////-----------------------------------------------------------------------------------------------
//// Octree Helper Class
////-----------------------------------------------------------------------------------------------
//class OctreeShape
//{
//public:
//	static ChildIndex LinearizeChild(IntVec3 offset) // [0, 1]
//	{
//		//return static_cast<ChildIndex>(offset.x + offset.y * 2 + offset.z * 4);
//		//return static_cast<ChildIndex>(offset.x + (offset.y << 1) + (offset.z << 2));
//		return (offset.x & 1) |
//			((offset.y & 1) << 1) |
//			((offset.z & 1) << 2);
//	}
//
//	static IntVec3 DelinearizeChild(ChildIndex index)
//	{
//		return IntVec3(index & 1, (index >> 1) & 1, (index >> 2) & 1);
//	}
//
//	static IntVec3 GetParentKey(IntVec3 key)
//	{
//		return key >> 1;
//	}
//
//	static IntVec3 GetAncestorKey(IntVec3 key, uint32_t levelsUp)
//	{
//		return key >> static_cast<int>(levelsUp);
//	}
//
//	static IntVec3 GetMinChildKey(IntVec3 key)
//	{
//		return key << 1;
//	}
//
//	// Node size of this LOD
//	static int GetNodeSizeAtLevel(Level level)
//	{
//		return 1 << level;
//	}
//
//	static IntBox3 GetNodeBounds(IntVec3 coords, Level level)
//	{
//		int size = GetNodeSizeAtLevel(level);
//		IntVec3 mins = coords * size;
//		IntVec3 dimensions = IntVec3(size, size, size);
//		return IntBox3(mins, dimensions);
//	}
//
//};
//
////-----------------------------------------------------------------------------------------------
//class NodeAllocator
//{
//private:
//	std::vector<Voxel> m_values;
//	std::vector<std::array<AllocPtr, 8>> m_pointers;
//	std::vector<bool> m_allocated;
//
//	std::vector<AllocPtr> m_freeList;
//	bool m_isLeafLevel;
//
//public:
//	explicit NodeAllocator(bool isLeaf) : m_isLeafLevel(isLeaf) {}
//
//	AllocPtr Insert(Voxel const& value)
//	{
//		AllocPtr ptr;
//
//		if (!m_freeList.empty())
//		{
//			// Reuse a slot in the free list
//			ptr = m_freeList.back();
//			m_freeList.pop_back();
//			m_values[ptr] = value;
//			m_allocated[ptr] = true;
//
//			if (!m_isLeafLevel) //  && ptr < pointers.size()
//			{
//				m_pointers[ptr].fill(EMPTY_ALLOC_PTR);
//			}
//
//		}
//		else
//		{
//			// assign a new slot
//			ptr = static_cast<AllocPtr>(m_values.size());
//			m_values.push_back(value);
//			m_allocated.push_back(true);
//
//			if (!m_isLeafLevel)
//			{
//				std::array<AllocPtr, 8> children;
//				children.fill(EMPTY_ALLOC_PTR);
//				m_pointers.push_back(children);
//			}
//		}
//
//		return ptr;
//	}
//
//	//void Remove(AllocPtr ptr)
//	//{
//	//	if (ptr < m_allocated.size() && m_allocated[ptr])
//	//	{
//	//		m_allocated[ptr] = false;
//	//		m_freeList.push_back(ptr);
//	//	}
//	//}
//
//	std::pair<std::optional<Voxel>, std::optional<std::array<AllocPtr, 8>>> Remove(AllocPtr ptr) 
//	{
//		if (ptr >= m_allocated.size() || !m_allocated[ptr]) 
//		{
//			return { std::nullopt, std::nullopt };
//		}
//
//		m_allocated[ptr] = false;
//		m_freeList.push_back(ptr);
//
//		std::optional<Voxel> value = m_values[ptr];
//		std::optional<std::array<AllocPtr, 8>> children;
//
//		if (!m_isLeafLevel && ptr < m_pointers.size()) 
//		{
//			children = m_pointers[ptr];
//		}
//
//		return { value, children };
//	}
//
//	bool ContainsNode(AllocPtr ptr) const 
//	{
//		return ptr < m_allocated.size() && m_allocated[ptr];
//	}
//
//	Voxel const* GetValue(AllocPtr ptr) const 
//	{
//		if (ContainsNode(ptr)) 
//		{
//			return &m_values[ptr];
//		}
//		return nullptr;
//	}
//
//	Voxel* GetValueMut(AllocPtr ptr) 
//	{
//		if (ContainsNode(ptr)) 
//		{
//			return &m_values[ptr];
//		}
//		return nullptr;
//	}
//
//	std::array<AllocPtr, 8> const* GetChildren(AllocPtr ptr) const 
//	{
//		if (m_isLeafLevel || !ContainsNode(ptr) || ptr >= m_pointers.size()) 
//		{
//			return nullptr;
//		}
//		return &m_pointers[ptr];
//	}
//
//	std::array<AllocPtr, 8>* GetChildrenMut(AllocPtr ptr) 
//	{
//		if (m_isLeafLevel || !ContainsNode(ptr) || ptr >= m_pointers.size()) 
//		{
//			return nullptr;
//		}
//		return &m_pointers[ptr];
//	}
//
//	void SetChildPointer(AllocPtr parentPtr, ChildIndex childIndex, AllocPtr childPtr) 
//	{
//		auto* children = GetChildrenMut(parentPtr);
//		if (children) 
//		{
//			(*children)[childIndex] = childPtr;
//		}
//	}
//
//	size_t GetAllocatedCount() const 
//	{
//		return m_values.size() - m_freeList.size();
//	}
//
//	size_t GetCapacity() const 
//	{
//		return m_values.size();
//	}
//};
//
////-----------------------------------------------------------------------------------------------
//// Entry
////-----------------------------------------------------------------------------------------------
//class NodeEntry 
//{
//public:
//	enum class Type { Occupied, Vacant };
//
//private:
//	Type m_type;
//	NodeAllocator* m_allocator;
//	AllocPtr* m_ptrSlot;
//	Level m_level;
//	Voxel* m_valuePtr;
//
//public:
//	NodeEntry(Type type, NodeAllocator* alloc, AllocPtr* slot, Level level, Voxel* value)
//		: m_type(type), m_allocator(alloc), m_ptrSlot(slot), m_level(level), m_valuePtr(value) 
//	{}
//
//	Type GetType() const { return m_type; }
//	bool IsOccupied() const { return m_type == Type::Occupied; }
//	bool IsVacant() const { return m_type == Type::Vacant; }
//
//	AllocPtr GetPointer() const { return *m_ptrSlot; }
//
//	Voxel& GetMut() 
//	{
//		return *m_valuePtr;
//	}
//
//	Voxel const& Get() const 
//	{
//		return *m_valuePtr;
//	}
//
//	// Insert a new value unconditionally
//	AllocPtr Insert(Voxel const& value) 
//	{
//		AllocPtr newPtr = m_allocator->Insert(value);
//		*m_ptrSlot = newPtr;
//		m_type = Type::Occupied;
//		m_valuePtr = m_allocator->GetValueMut(newPtr);
//		return newPtr;
//	}
//
//
//	// Deferred Calculation, one search and keep the "handle"
//	// if occupied return original value, not update
//	std::pair<AllocPtr, Voxel&> OrInsertWith(std::function<Voxel()> filler) 
//	{
//		if (IsOccupied()) 
//		{
//			return { GetPointer(), *m_valuePtr };
//		}
//		else 
//		{
//			AllocPtr ptr = Insert(filler());
//			return { ptr, *m_valuePtr };
//		}
//	}
//
//
//	AllocPtr InsertOrUpdate(Voxel const& value) 
//	{
//		if (IsOccupied()) 
//		{
//			*m_valuePtr = value;
//			return GetPointer();
//		}
//		else 
//		{
//			return Insert(value);
//		}
//	}
//};
//
////-----------------------------------------------------------------------------------------------
//
//enum class VisitCommand 
//{
//	Continue,
//	SkipDescendants
//};
//
////-----------------------------------------------------------------------------------------------
//class VoxelOctree
//{
//private:
//	std::unordered_map<NodeKey, RootNode, NodeKeyHash> m_rootNodes;
//	std::vector<NodeAllocator> m_allocators;
//	Level m_treeHeight; // LOD 0~2 height = 3
//
//public:
//	explicit VoxelOctree(Level height);
//
//	//-----------------------------------------------------------------------------------------------
//	// Basic Properties
//	//----------------------------------------------------------------------------------------------- 
//
//	Level GetHeight() const { return m_treeHeight; }
//	Level GetRootLevel() const { return m_treeHeight - 1; }
//
//	IntBox3 GetNodeBounds(NodeKey const& key) const 
//	{
//		return OctreeShape::GetNodeBounds(key.m_coords, key.m_level);
//	}
//
//	NodeKey GetAncestorRootKey(NodeKey const& descendantKey) const;
//
//	//-----------------------------------------------------------------------------------------------
//	// Root node operation
//	//-----------------------------------------------------------------------------------------------
//
//	std::pair<RootNode, std::optional<Voxel>> InsertRoot(NodeKey const& key, Voxel const& value);
//	std::optional<RootNode> FindRoot(NodeKey const& key) const;
//	RootNode GetOrCreateRoot(NodeKey const& key, std::function<Voxel()> filler);
//
//	//-----------------------------------------------------------------------------------------------
//	// Node Query
//	//-----------------------------------------------------------------------------------------------
//
//	bool ContainsNode(NodePtr const& ptr) const;
//	bool ContainsRoot(NodeKey const& key) const;
//
//	Voxel const* GetValue(NodePtr const& ptr) const;
//	Voxel* GetValueMut(NodePtr const& ptr);
//
//	std::optional<NodePtr> FindNode(NodeKey const& key) const;
//	std::optional<NodePtr> FindDescendant(NodePtr const& ancestorPtr, IntVec3 ancestorCoords, NodeKey const& targetKey) const;
//
//	//-----------------------------------------------------------------------------------------------
//	// Node Insertion
//	//-----------------------------------------------------------------------------------------------
//
//	std::pair<NodePtr, std::optional<Voxel>> InsertChild(NodePtr const& parentPtr, ChildIndex childIndex, Voxel const& value);
//	std::pair<NodePtr, std::optional<Voxel>> InsertChildAtOffset(NodePtr const& parentPtr, IntVec3 childOffset, Voxel const& value);
//
//	//-----------------------------------------------------------------------------------------------
//	// Entry API
//	//-----------------------------------------------------------------------------------------------
//	// NOT UNDERSTAND
//	NodeEntry GetChildEntry(NodePtr const& parentPtr, ChildIndex childIndex);
//
//	//-----------------------------------------------------------------------------------------------
//	// Batch filling API
//	//-----------------------------------------------------------------------------------------------
//	// NOT UNDERSTAND, minLevel, VisitCommand
//	std::pair<std::optional<RootNode>, VisitCommand> FillRoot(
//		NodeKey const& key,
//		std::function<VisitCommand(NodeEntry&)> filler);
//
//	void FillDescendants(
//		NodePtr const& ancestorPtr,
//		IntVec3 ancestorCoords,
//		Level minLevel,
//		std::function<VisitCommand(NodeKey const&, NodeEntry&)> filler);
//
//	// = FillRoot + FillDescendants
//	void FillTreeFromRoot(
//		NodeKey const& rootKey,
//		Level minLevel,
//		std::function<VisitCommand(NodeKey const&, NodeEntry&)> filler);
//
//	void FillPathToNode(
//		NodeKey const& targetKey,
//		std::function<VisitCommand(NodeKey const&, NodeEntry&)> filler);
//
//	//-----------------------------------------------------------------------------------------------
//	// Iteration API
//	//-----------------------------------------------------------------------------------------------
//
//	void VisitChildren(NodePtr const& parentPtr, std::function<void(NodePtr, ChildIndex)> visitor) const;
//	void VisitChildrenWithCoords(NodePtr const& parentPtr, IntVec3 parentCoords, std::function<void(NodePtr, IntVec3)> visitor) const;
//
//	void VisitTreeDepthFirst(
//		NodePtr const& ancestorPtr,
//		IntVec3 ancestorCoords,
//		Level minLevel,
//		std::function<VisitCommand(NodePtr, IntVec3)> visitor) const;
//
//	void VisitTreeBreadthFirst(
//		NodePtr const& ancestorPtr,
//		IntVec3 ancestorCoords,
//		Level minLevel,
//		std::function<VisitCommand(NodePtr, IntVec3)> visitor) const;
//
//	void VisitAllRoots(std::function<void(NodeKey const&, RootNode)> visitor) const;
//
//	//-----------------------------------------------------------------------------------------------
//	// Spatial Query
//	//-----------------------------------------------------------------------------------------------
//
//	void QueryBox(IntBox3 const& queryBox, std::function<void(NodeKey const&, Voxel const&)> visitor) const;
//
//	//-----------------------------------------------------------------------------------------------
//	// Merge and Cleanup
//	//-----------------------------------------------------------------------------------------------
//
//	struct MergeStats 
//	{
//		int nodesRemoved = 0;
//		int memorySaved = 0;
//	};
//
//	bool TryMergeChildren(NodePtr const& parentPtr, IntVec3 parentCoords);
//	void MergeUpwards(IntVec3 leafCoords);
//	MergeStats MergeAll();
//
//	bool IsNodeEmpty(NodePtr const& ptr) const;
//	bool AreAllChildrenEmpty(NodePtr const& parentPtr) const;
//	int CleanupEmptyRoots();
//
//	struct CleanupStats 
//	{
//		int mergedNodes = 0;
//		int removedRoots = 0;
//	};
//
//	CleanupStats SmartCleanup();
//
//	//-----------------------------------------------------------------------------------------------
//	// Virtual Query (query merged node)
//	//-----------------------------------------------------------------------------------------------
//
//	std::optional<Voxel> GetVoxelVirtual(NodeKey const& key) const;
//	std::vector<std::pair<IntVec3, Voxel>> GetVoxelsVirtual(IntBox3 const& box, Level targetLevel = 0) const;
//
//	//-----------------------------------------------------------------------------------------------
//	// Delete
//	//-----------------------------------------------------------------------------------------------
//
//	void RemoveTree(NodePtr const& rootPtr, IntVec3 rootCoords, std::function<void(NodeKey const&, Voxel)> consumer = nullptr);
//
//	//-----------------------------------------------------------------------------------------------
//	// Debugging
//	//-----------------------------------------------------------------------------------------------
//
//	void PrintStatistics() const;
//
//private:
//	void QueryBoxRecursive(
//		NodePtr const& ptr,
//		IntVec3 coords,
//		IntBox3 const& queryBox,
//		std::function<void(NodeKey const&, Voxel const&)>& visitor) const;
//
//
//
//};
//
//// NodePtr: There must have voxel here
//// NodeKey: It may query a big empty/ same material box
//
