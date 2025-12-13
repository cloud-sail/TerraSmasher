//#include "Game/VoxelOctree.hpp"
//#include "Engine/Core/EngineCommon.hpp"
//
//
//VoxelOctree::VoxelOctree(Level height)
//	: m_treeHeight(height)
//{
//	GUARANTEE_OR_DIE(height > 1, "Tree height must be at least 2");
//
//	m_allocators.reserve(height);
//	for (Level i = 0; i < height; ++i) 
//	{
//		m_allocators.emplace_back(i == 0);
//	}
//}
//
//NodeKey VoxelOctree::GetAncestorRootKey(NodeKey const& descendantKey) const
//{
//	GUARANTEE_OR_DIE(descendantKey.m_level <= GetRootLevel(), "Invalid Level");
//
//	uint32_t levelsUp = GetRootLevel() - descendantKey.m_level;
//	IntVec3 coordinates = OctreeShape::GetAncestorKey(descendantKey.m_coords, levelsUp);
//	return NodeKey(GetRootLevel(), coordinates);
//}
//
//std::pair<RootNode, std::optional<Voxel>> VoxelOctree::InsertRoot(NodeKey const& key, Voxel const& value)
//{
//	GUARANTEE_OR_DIE(key.m_level == GetRootLevel(), "NodeKey is not root level.");
//
//	auto& alloc = m_allocators[key.m_level];
//	auto it = m_rootNodes.find(key);
//
//	// Notes: it queries twice, can we use NodeEntry here?
//	if (it != m_rootNodes.end())
//	{
//		// Root exists: update value
//		RootNode rootNode = it->second;
//		Voxel* existingValue = alloc.GetValueMut(rootNode);
//		std::optional<Voxel> oldValue = *existingValue;
//		*existingValue = value;
//		return { rootNode, oldValue };
//	}
//	else
//	{
//		// Create a new root node
//		AllocPtr rootPtr = alloc.Insert(value);
//		m_rootNodes[key] = rootPtr;
//		return { rootPtr, std::nullopt }; // not understand, insert successful?
//	}
//}
//
//std::optional<RootNode> VoxelOctree::FindRoot(NodeKey const& key) const
//{
//	auto it = m_rootNodes.find(key);
//	if (it != m_rootNodes.end()) 
//	{
//		return it->second;
//	}
//	return std::nullopt;
//}
//
//RootNode VoxelOctree::GetOrCreateRoot(NodeKey const& key, std::function<Voxel()> filler)
//{
//	GUARANTEE_OR_DIE(key.m_level == GetRootLevel(), "NodeKey is not root level.");
//
//	auto& alloc = m_allocators[key.m_level];
//	auto it = m_rootNodes.find(key);
//
//	if (it != m_rootNodes.end()) 
//	{
//		return it->second;
//	}
//
//	AllocPtr rootPtr = alloc.Insert(filler());
//	m_rootNodes[key] = rootPtr;
//	return rootPtr;
//}
//
//bool VoxelOctree::ContainsNode(NodePtr const& ptr) const
//{
//	if (ptr.m_level >= m_treeHeight) return false;
//	return m_allocators[ptr.m_level].ContainsNode(ptr.m_allocPtr);
//}
//
//bool VoxelOctree::ContainsRoot(NodeKey const& key) const
//{
//	return m_rootNodes.find(key) != m_rootNodes.end();
//}
//
//Voxel const* VoxelOctree::GetValue(NodePtr const& ptr) const
//{
//	if (ptr.m_level >= m_treeHeight) return nullptr;
//	return m_allocators[ptr.m_level].GetValue(ptr.m_allocPtr);
//}
//
//Voxel* VoxelOctree::GetValueMut(NodePtr const& ptr)
//{
//	if (ptr.m_level >= m_treeHeight) return nullptr;
//	return m_allocators[ptr.m_level].GetValueMut(ptr.m_allocPtr);
//}
//
//std::optional<NodePtr> VoxelOctree::FindNode(NodeKey const& key) const
//{
//	if (key.m_level == GetRootLevel()) 
//	{
//		auto root = FindRoot(key);
//		if (root) 
//		{
//			return NodePtr(key.m_level, *root);
//		}
//		return std::nullopt;
//	}
//
//	NodeKey rootKey = GetAncestorRootKey(key);
//	auto root = FindRoot(rootKey);
//	if (!root) 
//	{
//		return std::nullopt;
//	}
//
//	NodePtr rootPtr(rootKey.m_level, *root);
//	return FindDescendant(rootPtr, rootKey.m_coords, key);
//}
//
//std::optional<NodePtr> VoxelOctree::FindDescendant(NodePtr const& ancestorPtr, IntVec3 ancestorCoords, NodeKey const& targetKey) const
//{
//	// Must ensure target is the descendant of ancestor
//	GUARANTEE_OR_DIE(ancestorPtr.m_level > targetKey.m_level, "TargetKey is not a descendant");
//
//	NodePtr currentPtr = ancestorPtr;
//	IntVec3 currentCoords = ancestorCoords;
//
//	for (Level level = ancestorPtr.m_level; level > targetKey.m_level; --level) 
//	{
//		auto& alloc = m_allocators[level];
//		auto* children = alloc.GetChildren(currentPtr.m_allocPtr);
//		if (!children) 
//		{
//			return std::nullopt;
//		}
//
//		uint32_t levelDiff = level - 1 - targetKey.m_level;
//		IntVec3 targetAtChildLevel = OctreeShape::GetAncestorKey(targetKey.m_coords, levelDiff);
//
//		IntVec3 minChild = OctreeShape::GetMinChildKey(currentCoords);
//		IntVec3 offset = targetAtChildLevel - minChild;
//
//		ChildIndex childIndex = OctreeShape::LinearizeChild(offset);
//		AllocPtr childPtr = (*children)[childIndex];
//
//		if (childPtr == EMPTY_ALLOC_PTR) 
//		{
//			return std::nullopt;
//		}
//
//		currentPtr = NodePtr(level - 1, childPtr);
//		currentCoords = targetAtChildLevel;
//	}
//
//	return currentPtr;
//}
//
//std::pair<NodePtr, std::optional<Voxel>> VoxelOctree::InsertChild(NodePtr const& parentPtr, ChildIndex childIndex, Voxel const& value)
//{
//	GUARANTEE_OR_DIE(parentPtr.m_level > 0, "Parent ptr should not at level 0");
//	GUARANTEE_OR_DIE(childIndex < 8, "Invalid ChildIndex");
//
//	Level childLevel = parentPtr.m_level - 1;
//	auto& parentAlloc = m_allocators[parentPtr.m_level];
//	auto& childAlloc = m_allocators[childLevel];
//
//	auto* children = parentAlloc.GetChildrenMut(parentPtr.m_allocPtr);
//	GUARANTEE_OR_DIE(children, "Parent node not found or is leaf");
//
//	AllocPtr childPtr = (*children)[childIndex];
//
//	if (childPtr == EMPTY_ALLOC_PTR) 
//	{
//		childPtr = childAlloc.Insert(value);
//		(*children)[childIndex] = childPtr;
//		return { NodePtr(childLevel, childPtr), std::nullopt };
//	}
//	else 
//	{
//		Voxel* existingValue = childAlloc.GetValueMut(childPtr);
//		std::optional<Voxel> oldValue = *existingValue;
//		*existingValue = value;
//		return { NodePtr(childLevel, childPtr), oldValue };
//	}
//}
//
//std::pair<NodePtr, std::optional<Voxel>> VoxelOctree::InsertChildAtOffset(NodePtr const& parentPtr, IntVec3 childOffset, Voxel const& value)
//{
//	ChildIndex childIndex = OctreeShape::LinearizeChild(childOffset);
//	return InsertChild(parentPtr, childIndex, value);
//}
//
//NodeEntry VoxelOctree::GetChildEntry(NodePtr const& parentPtr, ChildIndex childIndex)
//{
//	GUARANTEE_OR_DIE(parentPtr.m_level > 0, "Parent ptr should not at level 0");
//	GUARANTEE_OR_DIE(childIndex < 8, "Invalid ChildIndex");
//
//	Level childLevel = parentPtr.m_level - 1;
//	auto& parentAlloc = m_allocators[parentPtr.m_level];
//	auto& childAlloc = m_allocators[childLevel];
//
//	auto* children = parentAlloc.GetChildrenMut(parentPtr.m_allocPtr);
//	GUARANTEE_OR_DIE(children, "Parent node not found or is leaf");
//
//	AllocPtr* childPtrSlot = &(*children)[childIndex];
//
//	if (*childPtrSlot == EMPTY_ALLOC_PTR) 
//	{
//		return NodeEntry(NodeEntry::Type::Vacant, &childAlloc, childPtrSlot, childLevel, nullptr);
//	}
//	else 
//	{
//		Voxel* value = childAlloc.GetValueMut(*childPtrSlot);
//		return NodeEntry(NodeEntry::Type::Occupied, &childAlloc, childPtrSlot, childLevel, value);
//	}
//}
//
//std::pair<std::optional<RootNode>, VisitCommand> VoxelOctree::FillRoot(NodeKey const& key, std::function<VisitCommand(NodeEntry&)> filler)
//{
//	GUARANTEE_OR_DIE(key.m_level == GetRootLevel(), "Key is not at root level");
//
//	auto& alloc = m_allocators[key.m_level];
//	auto it = m_rootNodes.find(key);
//
//	if (it != m_rootNodes.end()) 
//	{
//		RootNode rootNode = it->second;
//		AllocPtr ptr = rootNode;
//		Voxel* value = alloc.GetValueMut(ptr);
//		NodeEntry entry(NodeEntry::Type::Occupied, &alloc, &ptr, key.m_level, value);
//		return { rootNode, filler(entry) };
//	}
//	else 
//	{
//		AllocPtr newPtr = EMPTY_ALLOC_PTR;
//		NodeEntry entry(NodeEntry::Type::Vacant, &alloc, &newPtr, key.m_level, nullptr);
//		VisitCommand cmd = filler(entry);
//
//		if (newPtr == EMPTY_ALLOC_PTR) 
//		{
//			return { std::nullopt, cmd };
//		}
//
//		m_rootNodes[key] = newPtr;
//		return { newPtr, cmd };
//	}
//}
//
//void VoxelOctree::FillDescendants(NodePtr const& ancestorPtr, IntVec3 ancestorCoords, Level minLevel, std::function<VisitCommand(NodeKey const&, NodeEntry&)> filler)
//{
//	GUARANTEE_OR_DIE(minLevel < ancestorPtr.m_level, "minLevel is not smaller than ancestor level");
//
//	struct StackItem 
//	{
//		NodePtr ptr;
//		IntVec3 coords;
//	};
//
//	std::stack<StackItem> stack;
//	stack.push({ ancestorPtr, ancestorCoords });
//
//	while (!stack.empty()) 
//	{
//		auto [parentPtr, parentCoords] = stack.top();
//		stack.pop();
//
//		if (parentPtr.m_level > minLevel) 
//		{
//			Level childLevel = parentPtr.m_level - 1;
//			bool hasGrandchildren = childLevel > minLevel;
//
//			for (ChildIndex i = 0; i < 8; ++i) 
//			{
//				auto entry = GetChildEntry(parentPtr, i);
//				IntVec3 childOffset = OctreeShape::DelinearizeChild(i);
//				IntVec3 childCoords = OctreeShape::GetMinChildKey(parentCoords) + childOffset;
//				NodeKey childKey(childLevel, childCoords);
//
//				VisitCommand cmd = filler(childKey, entry);
//
//				if (cmd == VisitCommand::Continue && hasGrandchildren) 
//				{
//					AllocPtr childPtr = entry.GetPointer();
//					if (childPtr != EMPTY_ALLOC_PTR) 
//					{
//						stack.push({ NodePtr(childLevel, childPtr), childCoords });
//					}
//				}
//			}
//		}
//	}
//}
//
//void VoxelOctree::FillTreeFromRoot(NodeKey const& rootKey, Level minLevel, std::function<VisitCommand(NodeKey const&, NodeEntry&)> filler)
//{
//	auto [rootNode, cmd] = FillRoot(rootKey, [&](NodeEntry& entry) { // #ToDo [&rootKey, &filler]
//		return filler(rootKey, entry);
//		});
//
//	if (rootNode && cmd == VisitCommand::Continue) 
//	{
//		NodePtr rootPtr(rootKey.m_level, *rootNode);
//		FillDescendants(rootPtr, rootKey.m_coords, minLevel, filler);
//	}
//}
//
//void VoxelOctree::FillPathToNode(NodeKey const& targetKey, std::function<VisitCommand(NodeKey const&, NodeEntry&)> filler)
//{
//	NodeKey rootKey = GetAncestorRootKey(targetKey);
//	auto [rootNode, cmd] = FillRoot(rootKey, [&](NodeEntry& entry) {
//		return filler(rootKey, entry);
//		});
//
//	if (!rootNode || cmd != VisitCommand::Continue) {
//		return;
//	}
//
//	NodePtr parentPtr(rootKey.m_level, *rootNode);
//	IntVec3 parentCoords = rootKey.m_coords;
//
//	for (Level childLevel = rootKey.m_level - 1; childLevel >= targetKey.m_level; --childLevel) {
//
//		uint32_t levelDiff = childLevel - targetKey.m_level;
//		IntVec3 ancestorCoords = OctreeShape::GetAncestorKey(targetKey.m_coords, levelDiff);
//		IntVec3 childOffset = ancestorCoords - OctreeShape::GetMinChildKey(parentCoords);
//		ChildIndex childIndex = OctreeShape::LinearizeChild(childOffset);
//
//		IntVec3 childCoords = OctreeShape::GetMinChildKey(parentCoords) + childOffset;
//		NodeKey childKey(childLevel, childCoords);
//
//		auto entry = GetChildEntry(parentPtr, childIndex);
//		cmd = filler(childKey, entry);
//
//		if (cmd != VisitCommand::Continue) 
//		{
//			break;
//		}
//
//		AllocPtr childPtr = entry.GetPointer();
//		if (childPtr == EMPTY_ALLOC_PTR) 
//		{
//			break;
//		}
//
//		parentPtr = NodePtr(childLevel, childPtr);
//		parentCoords = ancestorCoords;
//	}
//}
//
//void VoxelOctree::VisitChildren(NodePtr const& parentPtr, std::function<void(NodePtr, ChildIndex)> visitor) const
//{
//	if (parentPtr.m_level == 0) return;
//
//	auto& alloc = m_allocators[parentPtr.m_level];
//	auto* children = alloc.GetChildren(parentPtr.m_allocPtr);
//	if (!children) return;
//
//	Level childLevel = parentPtr.m_level - 1;
//	for (ChildIndex i = 0; i < 8; ++i) 
//	{
//		if ((*children)[i] != EMPTY_ALLOC_PTR) 
//		{
//			visitor(NodePtr(childLevel, (*children)[i]), i);
//		}
//	}
//}
//
//void VoxelOctree::VisitChildrenWithCoords(NodePtr const& parentPtr, IntVec3 parentCoords, std::function<void(NodePtr, IntVec3)> visitor) const
//{
//	VisitChildren(parentPtr, [&](NodePtr childPtr, ChildIndex index) {
//		IntVec3 childOffset = OctreeShape::DelinearizeChild(index);
//		IntVec3 childCoords = OctreeShape::GetMinChildKey(parentCoords) + childOffset;
//		visitor(childPtr, childCoords);
//		});
//}
//
//void VoxelOctree::VisitTreeDepthFirst(NodePtr const& ancestorPtr, IntVec3 ancestorCoords, Level minLevel, std::function<VisitCommand(NodePtr, IntVec3)> visitor) const
//{
//	// Wrong Implementation, not visit child first then parent
//
//	struct StackItem 
//	{
//		NodePtr ptr;
//		IntVec3 coords;
//	};
//
//	std::stack<StackItem> stack;
//	stack.push({ ancestorPtr, ancestorCoords });
//
//	while (!stack.empty()) {
//		auto [ptr, coords] = stack.top();
//		stack.pop();
//
//		VisitCommand cmd = visitor(ptr, coords);
//
//		if (cmd == VisitCommand::Continue && ptr.m_level > minLevel) 
//		{
//			VisitChildrenWithCoords(ptr, coords, [&](NodePtr childPtr, IntVec3 childCoords) {
//				stack.push({ childPtr, childCoords });
//				});
//		}
//	}
//}
//
//void VoxelOctree::VisitTreeBreadthFirst(NodePtr const& ancestorPtr, IntVec3 ancestorCoords, Level minLevel, std::function<VisitCommand(NodePtr, IntVec3)> visitor) const
//{
//	struct QueueItem 
//	{
//		NodePtr ptr;
//		IntVec3 coords;
//	};
//
//	std::queue<QueueItem> queue;
//	queue.push({ ancestorPtr, ancestorCoords });
//
//	while (!queue.empty()) 
//	{
//		auto [ptr, coords] = queue.front();
//		queue.pop();
//
//		VisitCommand cmd = visitor(ptr, coords);
//
//		if (cmd == VisitCommand::Continue && ptr.m_level > minLevel) 
//		{
//			VisitChildrenWithCoords(ptr, coords, [&](NodePtr childPtr, IntVec3 childCoords) {
//				queue.push({ childPtr, childCoords });
//				});
//		}
//	}
//}
//
//void VoxelOctree::VisitAllRoots(std::function<void(NodeKey const&, RootNode)> visitor) const
//{
//	for (auto const& [key, root] : m_rootNodes) 
//	{
//		visitor(key, root);
//	}
//}
//
//void VoxelOctree::QueryBox(IntBox3 const& queryBox, std::function<void(NodeKey const&, Voxel const&)> visitor) const
//{
//	for (auto const& [rootKey, rootNode] : m_rootNodes) 
//	{
//		IntBox3 rootBounds = GetNodeBounds(rootKey);
//
//		if (!rootBounds.IsOverlap(queryBox)) 
//		{
//			continue;
//		}
//
//		NodePtr rootPtr(rootKey.m_level, rootNode);
//		QueryBoxRecursive(rootPtr, rootKey.m_coords, queryBox, visitor);
//	}
//}
//
//bool VoxelOctree::TryMergeChildren(NodePtr const& parentPtr, IntVec3 parentCoords)
//{
//	UNUSED(parentCoords);
//	// has eight children, does not have any grandchildren, eight children has same voxel value
//	
//	if (parentPtr.m_level == 0) 
//	{
//		return false;
//	}
//
//	// check has eight valid children
//	auto& parentAlloc = m_allocators[parentPtr.m_level];
//	auto* children = parentAlloc.GetChildrenMut(parentPtr.m_allocPtr);
//	if (!children) 
//	{
//		return false;
//	}
//
//	Level childLevel = parentPtr.m_level - 1;
//	auto& childAlloc = m_allocators[childLevel];
//
//	std::array<AllocPtr, 8> childPtrs = *children;
//	bool allExist = true;
//	for (AllocPtr ptr : childPtrs) 
//	{
//		if (ptr == EMPTY_ALLOC_PTR) 
//		{
//			allExist = false;
//			break;
//		}
//	}
//
//	if (!allExist) 
//	{
//		return false;
//	}
//
//	// if has grandchildren, check if they are all EMPTY
//	if (childLevel > 0) 
//	{
//		for (AllocPtr ptr : childPtrs) 
//		{
//			auto* grandchildren = childAlloc.GetChildren(ptr);
//			if (grandchildren) 
//			{
//				for (AllocPtr gcPtr : *grandchildren) 
//				{
//					if (gcPtr != EMPTY_ALLOC_PTR) 
//					{
//						return false;
//					}
//				}
//			}
//		}
//	}
//
//	// Check if all children has the same value
//	Voxel const* firstValue = childAlloc.GetValue(childPtrs[0]);
//	if (!firstValue) 
//	{
//		return false;
//	}
//
//	for (size_t i = 1; i < 8; ++i) 
//	{
//		Voxel const* childValue = childAlloc.GetValue(childPtrs[i]);
//		if (!childValue || !Voxel::AreEqual(*childValue, *firstValue)) 
//		{
//			return false;
//		}
//	}
//
//	// Update parent value, clean child data
//	Voxel* parentValue = parentAlloc.GetValueMut(parentPtr.m_allocPtr);
//	*parentValue = *firstValue;
//
//	for (AllocPtr ptr : childPtrs) 
//	{
//		childAlloc.Remove(ptr);
//	}
//
//	children->fill(EMPTY_ALLOC_PTR);
//
//	return true;
//}
//
//void VoxelOctree::MergeUpwards(IntVec3 leafCoords)
//{
//	// leaf coords is level 0
//	for (Level level = 1; level < m_treeHeight; ++level) 
//	{
//		IntVec3 coordsAtLevel = OctreeShape::GetAncestorKey(leafCoords, level);
//		NodeKey key(level, coordsAtLevel);
//
//		auto ptr = FindNode(key);
//		if (!ptr) 
//		{
//			break;
//		}
//
//		if (!TryMergeChildren(*ptr, coordsAtLevel)) 
//		{
//			break;
//		}
//	}
//}
//
//VoxelOctree::MergeStats VoxelOctree::MergeAll()
//{
//	MergeStats stats;
//
//	for (Level level = 1; level < m_treeHeight; ++level) 
//	{
//		// Not sure, is logic right? if Visit Tree Depth is correct, why need for loop to check every level.
//		VisitAllRoots([&](NodeKey const& rootKey, RootNode rootNode) {
//			NodePtr rootPtr(rootKey.m_level, rootNode);
//
//			VisitTreeDepthFirst(
//				rootPtr,
//				rootKey.m_coords,
//				level,
//				[&](NodePtr ptr, IntVec3 coords) {
//					if (ptr.m_level == level) {
//						if (TryMergeChildren(ptr, coords)) {
//							stats.nodesRemoved += 8;
//						}
//					}
//					return VisitCommand::Continue;
//				}
//			);
//			});
//	}
//
//	return stats;
//}
//
//bool VoxelOctree::IsNodeEmpty(NodePtr const& ptr) const
//{
//	Voxel const* value = GetValue(ptr);
//	if (!value) 
//	{
//		return true;
//	}
//
//	return value->IsEmpty();
//}
//
//bool VoxelOctree::AreAllChildrenEmpty(NodePtr const& parentPtr) const
//{
//	if (parentPtr.m_level == 0) 
//	{
//		return IsNodeEmpty(parentPtr);
//	}
//
//	auto& alloc = m_allocators[parentPtr.m_level];
//	auto* children = alloc.GetChildren(parentPtr.m_allocPtr);
//	if (!children) 
//	{
//		return IsNodeEmpty(parentPtr);
//	}
//
//	bool hasChildren = false;
//	for (AllocPtr ptr : *children) 
//	{
//		if (ptr != EMPTY_ALLOC_PTR) 
//		{
//			hasChildren = true;
//
//			if (!AreAllChildrenEmpty(NodePtr(parentPtr.m_level - 1, ptr))) 
//			{
//				return false;
//			}
//		}
//	}
//
//	if (!hasChildren) 
//	{
//		return IsNodeEmpty(parentPtr);
//	}
//
//	return true;
//
//	// Not sure? only check children not itself?
//}
//
//int VoxelOctree::CleanupEmptyRoots()
//{
//	int removedCount = 0;
//
//	std::vector<NodeKey> toRemove;
//
//	for (auto const& [key, root] : m_rootNodes) 
//	{
//		NodePtr rootPtr(key.m_level, root);
//
//		if (AreAllChildrenEmpty(rootPtr)) 
//		{
//			toRemove.push_back(key);
//		}
//	}
//
//	for (auto const& key : toRemove) 
//	{
//		auto root = FindRoot(key);
//		if (root) 
//		{
//			NodePtr rootPtr(key.m_level, *root);
//			RemoveTree(rootPtr, key.m_coords);
//			m_rootNodes.erase(key);
//			removedCount++;
//		}
//	}
//
//	return removedCount;
//}
//
//VoxelOctree::CleanupStats VoxelOctree::SmartCleanup()
//{
//	CleanupStats stats;
//
//	auto mergeStats = MergeAll();
//	stats.mergedNodes = mergeStats.nodesRemoved;
//
//	stats.removedRoots = CleanupEmptyRoots();
//
//	return stats;
//}
//
//std::optional<Voxel> VoxelOctree::GetVoxelVirtual(NodeKey const& key) const
//{
//	auto ptr = FindNode(key);
//	if (ptr) 
//	{
//		Voxel const* value = GetValue(*ptr);
//		if (value) 
//		{
//			return *value;
//		}
//	}
//
//	// Can it be faster? visit from root to the key if no children return 
//	for (Level level = key.m_level + 1; level <= GetRootLevel(); ++level) 
//	{
//		uint32_t levelsUp = level - key.m_level;
//		IntVec3 ancestorCoords = OctreeShape::GetAncestorKey(key.m_coords, levelsUp);
//		NodeKey ancestorKey(level, ancestorCoords);
//
//		auto ancestorPtr = FindNode(ancestorKey);
//		if (ancestorPtr) {
//			Voxel const* value = GetValue(*ancestorPtr);
//			if (value) {
//				auto& alloc = m_allocators[ancestorPtr->m_level];
//				auto* children = alloc.GetChildren(ancestorPtr->m_allocPtr);
//
//				if (!children) {
//					return *value;
//				}
//
//				bool pathHasChildren = false;
//				for (AllocPtr childPtr : *children) {
//					if (childPtr != EMPTY_ALLOC_PTR) {
//						pathHasChildren = true;
//						break;
//					}
//				}
//
//				if (!pathHasChildren) {
//					return *value;
//				}
//			}
//		}
//	}
//
//	return std::nullopt;
//}
//
//std::vector<std::pair<IntVec3, Voxel>> VoxelOctree::GetVoxelsVirtual(IntBox3 const& box, Level targetLevel /*= 0*/) const
//{
//	std::vector<std::pair<IntVec3, Voxel>> result;
//
//	IntVec3 maxs = box.m_mins + box.m_dimensions;
//
//	for (int z = box.m_mins.z; z < maxs.z; ++z) {
//		for (int y = box.m_mins.y; y < maxs.y; ++y) {
//			for (int x = box.m_mins.x; x < maxs.x; ++x) {
//				IntVec3 pos(x, y, z);
//				NodeKey key(targetLevel, pos);
//
//				auto value = GetVoxelVirtual(key);
//				if (value) {
//					result.push_back({ pos, *value });
//				}
//			}
//		}
//	}
//
//	return result;
//}
//
//void VoxelOctree::RemoveTree(NodePtr const& rootPtr, IntVec3 rootCoords, std::function<void(NodeKey const&, Voxel)> consumer /*= nullptr*/)
//{
//	struct StackItem {
//		NodePtr ptr;
//		IntVec3 coords;
//	};
//
//	std::stack<StackItem> stack;
//	stack.push({ rootPtr, rootCoords });
//
//	while (!stack.empty()) {
//		auto [ptr, coords] = stack.top();
//		stack.pop();
//
//		auto [value, children] = m_allocators[ptr.m_level].Remove(ptr.m_allocPtr);
//
//		if (value && consumer) {
//			consumer(NodeKey(ptr.m_level, coords), *value);
//		}
//
//		if (children) {
//			Level childLevel = ptr.m_level - 1;
//			IntVec3 minChild = OctreeShape::GetMinChildKey(coords);
//
//			for (ChildIndex i = 0; i < 8; ++i) {
//				if ((*children)[i] != EMPTY_ALLOC_PTR) {
//					IntVec3 childOffset = OctreeShape::DelinearizeChild(i);
//					IntVec3 childCoords = minChild + childOffset;
//					stack.push({ NodePtr(childLevel, (*children)[i]), childCoords });
//				}
//			}
//		}
//	}
//}
//
//void VoxelOctree::PrintStatistics() const
//{
//	//std::cout << "=== Octree Statistics ===" << std::endl;
//	//std::cout << "Height: " << static_cast<int>(m_treeHeight) << std::endl;
//	//std::cout << "Root nodes: " << m_rootNodes.size() << std::endl;
//
//	//for (Level i = 0; i < m_treeHeight; ++i) {
//	//	std::cout << "Level " << static_cast<int>(i) << ": "
//	//		<< m_allocators[i].GetAllocatedCount() << " / "
//	//		<< m_allocators[i].GetCapacity() << " nodes" << std::endl;
//	//}
//}
//
//void VoxelOctree::QueryBoxRecursive(NodePtr const& ptr, IntVec3 coords, IntBox3 const& queryBox, std::function<void(NodeKey const&, Voxel const&)>& visitor) const
//{
//	IntBox3 nodeBox = OctreeShape::GetNodeBounds(coords, ptr.m_level);
//
//	if (!nodeBox.IsOverlap(queryBox)) 
//	{
//		return;
//	}
//
//	if (ptr.m_level == 0) 
//	{
//		Voxel const* value = GetValue(ptr);
//		if (value) 
//		{
//			visitor(NodeKey(0, coords), *value);
//		}
//	}
//	else 
//	{
//		VisitChildrenWithCoords(ptr, coords, [&](NodePtr childPtr, IntVec3 childCoords) {
//			QueryBoxRecursive(childPtr, childCoords, queryBox, visitor);
//			});
//	}
//}
//
