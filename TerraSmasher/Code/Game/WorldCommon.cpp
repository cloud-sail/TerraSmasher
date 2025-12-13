#include "Game/WorldCommon.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/MathUtils.hpp"

//-----------------------------------------------------------------------------------------------
const std::array<int, NUM_LOD_LEVELS> CoordsUtils::LOD_SIZES = []() {
	std::array<int, NUM_LOD_LEVELS> sizes;
	for (int lod = 0; lod < NUM_LOD_LEVELS; ++lod) {
		sizes[lod] = LOD0_CHUNK_SIZE << lod;
	}
	return sizes;
	}();

//-----------------------------------------------------------------------------------------------
// ChunkShape Utils
//-----------------------------------------------------------------------------------------------

STATIC int CoordsUtils::LinearizeChild(IntVec3 offset) // [0, 1]
{
	return (offset.x & 1) |
		((offset.y & 1) << 1) |
		((offset.z & 1) << 2);
}

STATIC IntVec3 CoordsUtils::DelinearizeChild(int index)
{
	return IntVec3(index & 1, (index >> 1) & 1, (index >> 2) & 1);
}

STATIC IntVec3 CoordsUtils::GetParentKey(IntVec3 key)
{
	return key >> 1;
}

STATIC IntVec3 CoordsUtils::GetAncestorKey(IntVec3 key, uint32_t levelsUp)
{
	return key >> static_cast<int>(levelsUp);
}

STATIC IntVec3 CoordsUtils::GetMinChildKey(IntVec3 key)
{
	return key << 1;
}

STATIC int CoordsUtils::GetChunkSizeForLOD(int lodLevel)
{
	return LOD0_CHUNK_SIZE << lodLevel; // LOD0: 8, LOD1: 16, LOD2: 32
}

int CoordsUtils::GetChunksPerAxis(int lodLevel)
{
	return LOD0_CHUNKS_PER_AXIS >> lodLevel;
}

STATIC int CoordsUtils::GetTotalChunksForLOD(int lodLevel)
{
	return LOD0_TOTAL_CHUNKS >> (3 * lodLevel);
}

STATIC int CoordsUtils::GetLODShift(int lodLevel)
{
	return LOD0_CHUNK_BITS + lodLevel; // LOD0: 3 (8=2^3), LOD1: 4 (16=2^4), LOD2: 5 (32=2^5)
}

STATIC int CoordsUtils::SelectLODByDistanceSq(float distanceSq)
{
	for (int i = NUM_LOD_LEVELS - 2; i >= 0; --i)
	{
		if (distanceSq >= LOD_DISTANCE_THRESHOLD[i] * LOD_DISTANCE_THRESHOLD[i])
		{
			return i + 1;
		}
	}

	return 0;
}

STATIC IntBox3 CoordsUtils::GetChunkBounds(IntVec3 coords, int level)
{
	int size = GetChunkSizeForLOD(level);
	IntVec3 mins = coords * size;
	IntVec3 dimensions = IntVec3(size, size, size);
	return IntBox3(mins, dimensions);
}

STATIC std::vector<ChunkKey> CoordsUtils::GetChildChunkKeys(IntVec3 parentCoords, int level)
{
	if (level == 0)
	{
		return {};
	}

	std::vector<ChunkKey> children;
	children.reserve(8);

	int childLOD = level - 1;
	IntVec3 childBase = parentCoords << 1;

	for (int z = 0; z < 2; ++z)
	{
		for (int y = 0; y < 2; ++y)
		{
			for (int x = 0; x < 2; ++x)
			{
				IntVec3 childIndex = childBase + IntVec3(x, y, z);
				children.push_back(ChunkKey(childIndex, childLOD));
			}
		}
	}

	return children;
}

STATIC int CoordsUtils::GetLODFromChunkSize(int chunkSize)
{
	for (int lod = 0; lod < NUM_LOD_LEVELS; ++lod)
	{
		if (LOD_SIZES[lod] == chunkSize)
		{
			return lod;
		}
	}

	ERROR_AND_DIE(Stringf("Invalid chunk size %d: must be one of the valid LOD sizes", chunkSize));
}

//-----------------------------------------------------------------------------------------------

void TraverseOctreeForLOD(IntBox3 const& nodeBox, Vec3 const& cameraPos, Frustum const& frustum, std::vector<ChunkKey>& out_chunks)
{
	GUARANTEE_OR_DIE(nodeBox.m_dimensions.x == nodeBox.m_dimensions.y && nodeBox.m_dimensions.y == nodeBox.m_dimensions.z, "Box is not cubic");
	
	int nodeSize = nodeBox.m_dimensions.x;
	if (nodeSize < LOD0_CHUNK_SIZE) return;

	GUARANTEE_OR_DIE(nodeSize > 0 && (nodeSize & (nodeSize - 1)) == 0, "Node size is not power of 2.");

	// grid coords -> world coords aabb
	AABB3 box(
		Vec3((float)nodeBox.m_mins.x, (float)nodeBox.m_mins.y, (float)nodeBox.m_mins.z),
		Vec3((float)(nodeBox.m_mins.x + nodeBox.m_dimensions.x),
			(float)(nodeBox.m_mins.y + nodeBox.m_dimensions.y),
			(float)(nodeBox.m_mins.z + nodeBox.m_dimensions.z))
	);

	if (!IsAABBOnFrustum(box, frustum)) 
	{
		return;
	}

	Vec3 boxCenter = box.GetCenter();
	float distanceSq = (boxCenter - cameraPos).GetLengthSquared();

	int desiredLOD = CoordsUtils::SelectLODByDistanceSq(distanceSq);
	int desiredSize = CoordsUtils::GetChunkSizeForLOD(desiredLOD);

	if (nodeSize == desiredSize)
	{
		ChunkKey key;
		key.m_lodLevel = desiredLOD;
		key.m_chunkCoords = nodeBox.m_mins >> CoordsUtils::GetLODShift(desiredLOD);
		out_chunks.push_back(key);
		return;
	}

	// Need subdivision
	if (nodeSize > desiredSize)
	{
		int halfSize = nodeSize >> 1;

		for (int i = 0; i < 8; ++i)
		{
			IntVec3 childOffset = CoordsUtils::DelinearizeChild(i);
			IntVec3 childMins = nodeBox.m_mins + childOffset * halfSize;
			IntBox3 childBox(childMins, IntVec3(halfSize, halfSize, halfSize));

			TraverseOctreeForLOD(childBox, cameraPos, frustum, out_chunks);
		}
		return;
	}

	int lodLevel = CoordsUtils::GetLODFromChunkSize(nodeSize);
	int shift = CoordsUtils::GetLODShift(lodLevel);
	IntVec3 chunkCoords = nodeBox.m_mins >> shift;
	out_chunks.push_back(ChunkKey(chunkCoords, lodLevel));
}


OctreeNodeInfo::OctreeNodeInfo(IntBox3 const& nodeBox, int nodeDepth)
	: box(nodeBox)
	, nodeSize(nodeBox.m_dimensions.x)
	, depth(nodeDepth)
{
	aabbWorld = AABB3(
		Vec3((float)box.m_mins.x, (float)box.m_mins.y, (float)box.m_mins.z),
		Vec3((float)(box.m_mins.x + box.m_dimensions.x),
			(float)(box.m_mins.y + box.m_dimensions.y),
			(float)(box.m_mins.z + box.m_dimensions.z))
	);
	centerWorld = aabbWorld.GetCenter();
}
//-----------------------------------------------------------------------------------------------
// Visitor Pattern Traversal
//-----------------------------------------------------------------------------------------------

void TraverseOctreeWithVisitor(
	IntBox3 const& rootBox,
	std::function<TraversalCommand(OctreeNodeInfo const&)> visitor)
{
	struct StackItem
	{
		IntBox3 box;
		int depth;
	};

	std::stack<StackItem> stack;
	stack.push({ rootBox, 0 });
	bool shouldStop = false;

	while (!stack.empty() && !shouldStop)
	{
		StackItem item = stack.top();
		stack.pop();

		OctreeNodeInfo nodeInfo(item.box, item.depth);

		TraversalCommand cmd = visitor(nodeInfo);

		switch (cmd)
		{
		case TraversalCommand::SkipDescendants:
		case TraversalCommand::UseThisNode:
			continue;

		case TraversalCommand::StopTraversal:
			shouldStop = true;
			continue;

		case TraversalCommand::Continue:
			break;
		}

		int halfSize = nodeInfo.nodeSize >> 1;

		// Not checking min size
		// let visitor decide when to stop

		// Push to stack reversely
		for (int z = 1; z >= 0; z--)
		{
			for (int y = 1; y >= 0; y--)
			{
				for (int x = 1; x >= 0; x--)
				{
					IntVec3 childMins = item.box.m_mins + IntVec3(
						x * halfSize,
						y * halfSize,
						z * halfSize
					);
					IntBox3 childBox(childMins, IntVec3(halfSize, halfSize, halfSize));
					stack.push({ childBox, item.depth + 1 });
				}
			}
		}
	}
}


void TraverseOctreeWithVisitor(
	IntBox3 const& rootBox,
	std::function<TraversalCommand(OctreeNodeInfo const&)> visitor,
	TraversalConfig const& config)
{
	struct StackItem
	{
		IntBox3 box;
		int depth;
	};

	std::stack<StackItem> stack;
	stack.push({ rootBox, 0 });
	bool shouldStop = false;

	while (!stack.empty() && !shouldStop)
	{
		StackItem item = stack.top();
		stack.pop();

		OctreeNodeInfo nodeInfo(item.box, item.depth);

		// Limiting Depth (totally ignore over depth node)
		//if (config.maxDepth >= 0 && item.depth > config.maxDepth)
		//{
		//	continue; // skip this node
		//}

		TraversalCommand cmd = visitor(nodeInfo);

		switch (cmd)
		{
		case TraversalCommand::SkipDescendants:
		case TraversalCommand::UseThisNode:
			continue;

		case TraversalCommand::StopTraversal:
			shouldStop = true;
			continue;

		case TraversalCommand::Continue:
			break;
		}

		if (config.maxDepth >= 0 && item.depth >= config.maxDepth)
		{
			continue;
		}

		int halfSize = nodeInfo.nodeSize >> 1;

		if (config.minNodeSize > 0 && halfSize < config.minNodeSize)
		{
			continue;
		}

		for (int z = 1; z >= 0; z--)
		{
			for (int y = 1; y >= 0; y--)
			{
				for (int x = 1; x >= 0; x--)
				{
					IntVec3 childMins = item.box.m_mins + IntVec3(
						x * halfSize,
						y * halfSize,
						z * halfSize
					);
					IntBox3 childBox(childMins, IntVec3(halfSize, halfSize, halfSize));
					stack.push({ childBox, item.depth + 1 });
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------------------
std::vector<ChunkKey> GetVisibleChunksWithLOD(Vec3 const& cameraPos, Frustum const& frustum)
{
	std::vector<ChunkKey> visibleChunks;

	auto lodVisitor = [&](OctreeNodeInfo const& node) -> TraversalCommand
		{
			// 1. Frustum culling
			if (!IsAABBOnFrustum(node.aabbWorld, frustum))
			{
				return TraversalCommand::SkipDescendants;
			}

			// 2. Check node size (finish condition)
			if (node.nodeSize < LOD0_CHUNK_SIZE)
			{
				return TraversalCommand::SkipDescendants;
			}

			// 3. Get Desire LOD
			float distanceSq = (node.centerWorld - cameraPos).GetLengthSquared();
			int desiredLOD = CoordsUtils::SelectLODByDistanceSq(distanceSq);
			int desiredSize = CoordsUtils::GetChunkSizeForLOD(desiredLOD);

			// 4. 
			if (node.nodeSize == desiredSize)
			{
				int lod = CoordsUtils::GetLODFromChunkSize(node.nodeSize);
				ChunkKey key(node.box.m_mins >> CoordsUtils::GetLODShift(lod), lod);
				visibleChunks.push_back(key);
				return TraversalCommand::UseThisNode;
			}

			// 5. Continue Subdivision
			if (node.nodeSize > desiredSize)
			{
				return TraversalCommand::Continue;
			}

			// 
			int lod = CoordsUtils::GetLODFromChunkSize(node.nodeSize);
			ChunkKey key(node.box.m_mins >> CoordsUtils::GetLODShift(lod), lod);
			visibleChunks.push_back(key);
			return TraversalCommand::UseThisNode;
		};

	IntBox3 worldBox(IntVec3(0, 0, 0), IntVec3(WORLD_SIZE, WORLD_SIZE, WORLD_SIZE));
	TraverseOctreeWithVisitor(worldBox, lodVisitor);

	return visibleChunks;
}

std::vector<ChunkKey> GetVisibleChunksWithLOD_Safe(Vec3 const& cameraPos, Frustum const& frustum)
{
	std::vector<ChunkKey> visibleChunks;

	auto lodVisitor = [&](OctreeNodeInfo const& node) -> TraversalCommand
		{
			if (!IsAABBOnFrustum(node.aabbWorld, frustum))
			{
				return TraversalCommand::SkipDescendants;
			}

			float distanceSq = (node.centerWorld - cameraPos).GetLengthSquared();
			int desiredLOD = CoordsUtils::SelectLODByDistanceSq(distanceSq);
			int desiredSize = CoordsUtils::GetChunkSizeForLOD(desiredLOD);

			if (node.nodeSize == desiredSize)
			{
				int lod = CoordsUtils::GetLODFromChunkSize(node.nodeSize);
				ChunkKey key(node.box.m_mins >> CoordsUtils::GetLODShift(lod), lod);
				visibleChunks.push_back(key);
				return TraversalCommand::UseThisNode;
			}

			if (node.nodeSize > desiredSize)
			{
				return TraversalCommand::Continue;
			}

			int lod = CoordsUtils::GetLODFromChunkSize(node.nodeSize);
			ChunkKey key(node.box.m_mins >> CoordsUtils::GetLODShift(lod), lod);
			visibleChunks.push_back(key);
			return TraversalCommand::UseThisNode;
		};

	IntBox3 worldBox(IntVec3(0, 0, 0), IntVec3(WORLD_SIZE, WORLD_SIZE, WORLD_SIZE));

	TraversalConfig config(LOD0_CHUNK_SIZE); // Prevent subdividing to smaller size
	TraverseOctreeWithVisitor(worldBox, lodVisitor, config);

	return visibleChunks;
}

std::vector<ChunkKey> GetDirtyChunksInRegion(IntBox3 const& dirtyRegion)
{
	std::vector<ChunkKey> dirtyChunks;

	auto getExpandedBox = [](IntBox3 const& chunkBox, int lodLevel) -> IntBox3
		{
			// LOD0: 8^3	-> 10^3
			// LOD1: 16^3	-> 20^3
			// LOD2: 32^3	-> 40^3
			int expansion = MESH_CHUNK_EXPANSION << lodLevel;

			IntVec3 expandedMins = chunkBox.m_mins - IntVec3(expansion, expansion, expansion);
			IntVec3 expandedDims = chunkBox.m_dimensions + IntVec3(expansion * 2, expansion * 2, expansion * 2);

			return IntBox3(expandedMins, expandedDims);
		};

	auto dirtyVisitor = [&](OctreeNodeInfo const& node)->TraversalCommand
		{
			// May not need to check
			if (node.nodeSize < LOD0_CHUNK_SIZE)
			{
				return TraversalCommand::SkipDescendants;
			}

			//int currentLOD = ChunkShape::GetLODFromChunkSize(node.nodeSize);
			int lodForExpansion = -1;
			bool isExactLODLevel = false;

			for (int lod = 0; lod < NUM_LOD_LEVELS; ++lod)
			{
				if (node.nodeSize == CoordsUtils::GetChunkSizeForLOD(lod))
				{
					lodForExpansion = lod;
					isExactLODLevel = true;
					break;
				}
			}

			if (!isExactLODLevel)
			{
				lodForExpansion = NUM_LOD_LEVELS - 1; 
			}

			IntBox3 expandedBox = getExpandedBox(node.box, lodForExpansion);

			if (!expandedBox.IsOverlap(dirtyRegion))
			{
				return TraversalCommand::SkipDescendants;
			}

			if (isExactLODLevel)
			{
				int shift = CoordsUtils::GetLODShift(lodForExpansion);
				IntVec3 chunkCoords = node.box.m_mins >> shift;
				dirtyChunks.push_back(ChunkKey(chunkCoords, lodForExpansion));
			}

			if (node.nodeSize > LOD0_CHUNK_SIZE)
			{
				return TraversalCommand::Continue;
			}

			return TraversalCommand::UseThisNode;
		};

	IntBox3 worldBox(IntVec3(0, 0, 0), IntVec3(WORLD_SIZE, WORLD_SIZE, WORLD_SIZE));
	TraversalConfig config(LOD0_CHUNK_SIZE); // Prevent subdividing to smaller size
	TraverseOctreeWithVisitor(worldBox, dirtyVisitor, config);

	return dirtyChunks;
}


