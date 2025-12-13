#include "Game/CubeTables.hpp"


namespace Isosurface {

	const std::array<IntVec3, kCubeCornerCount> kCubeCornerOffsets =
	{
		IntVec3(0, 0, 0),
		IntVec3(1, 0, 0),
		IntVec3(0, 1, 0),
		IntVec3(1, 1, 0),
		IntVec3(0, 0, 1),
		IntVec3(1, 0, 1),
		IntVec3(0, 1, 1),
		IntVec3(1, 1, 1),
	};

	const std::array<std::array<int, 2>, kCubeEdgeCount> kEdgeVertexIndices = 
	{ {
		{0, 1},
		{1, 3},
		{3, 2},
		{2, 0},
		{4, 5},
		{5, 7},
		{7, 6},
		{6, 4},
		{0, 4},
		{1, 5},
		{3, 7},
		{2, 6}
	} };

	const std::array<uint16_t, 256> kEdgeIntersectionFlags = []
		{
			std::array<uint16_t, 256> table{};
			for (int i = 0; i < 256; ++i) 
			{
				uint16_t mask = 0;
				for (int edge = 0; edge < kCubeEdgeCount; ++edge) 
				{
					// if the signs of two vertices of the edge are different, record the edge
					int v0 = (i >> kEdgeVertexIndices[edge][0]) & 1;
					int v1 = (i >> kEdgeVertexIndices[edge][1]) & 1;
					if (v0 ^ v1)
					{
						mask |= (1 << edge);
					}

					//bool a = (i & (1 << kEdgeVertexIndices[edge][0])) != 0;
					//bool b = (i & (1 << kEdgeVertexIndices[edge][1])) != 0;
					//if (a != b) 
					//{
					//	mask |= (1 << edge);
					//}
				}
				table[i] = mask;
			}
			return table;
		}();

} // namespace Isosurface