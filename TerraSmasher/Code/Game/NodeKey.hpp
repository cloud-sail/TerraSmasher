#pragma once
#include "Engine/Core/HashUtils.hpp"
#include "Engine/Math/IntVec3.hpp"

struct NodeKey
{
	int m_level;			// LOD level: 0=finest detail, higher=coarser
	IntVec3 m_coords;		// Coordinates at this LOD level

	NodeKey() : m_level(0), m_coords() {}
	NodeKey(int level, IntVec3 coords) : m_level(level), m_coords(coords) {}

	bool operator==(const NodeKey& other) const
	{
		return m_level == other.m_level && m_coords == other.m_coords;
	}

	bool operator!=(const NodeKey& other) const
	{
		return !(*this == other);
	}

	static IntVec3 GetParentCoords(IntVec3 coords)
	{
		return coords >> 1;
	}

	static IntVec3 GetAncestorCoords(IntVec3 coords, uint32_t levelsUp)
	{
		return coords >> static_cast<int>(levelsUp);
	}

	static IntVec3 GetMinChildCoords(IntVec3 coords)
	{
		return coords << 1;
	}

	static int GetNodeSizeAtLevel(int level)
	{
		return 1 << level;
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
	}
};