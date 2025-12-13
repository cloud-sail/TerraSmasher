#pragma once
/*
Lookup Tables

# SD Coordinate System:

	z (Skyward)
	|     y (North)
	|   /
	| /
	+----- x (West)


i = cube index 0~7
x = (i >> 0) & 1;
y = (i >> 1) & 1;
z = (i >> 2) & 1;

index -> IntVec3(x, y, z)

000, 001, 010, 011, 100, 101, 110, 111

# Vertex and edge layout:

			6             7
			+-------------+               +-----6-------+
		  / |           / |             / |            /|
		/   |         /   |           7   11         5  10
	4 +-----+-------+  5  |         +-----+4------+     |
	  |   2 +-------+-----+ 3       |     +-----2-+-----+
	  |   /         |   /           8   3         9   1
	  | /           | /             | /           | /
	0 +-------------+ 1             +------0------+



	struct Point {
		float density;
		Vec3 v;
	};

constexpr uint8_t ISOSURFACE_VALUE = 128;

int index = 0;
if (densities[i] < ISOSURFACE_VALUE)  it is empty;
if (densities[i] >= ISOSURFACE_VALUE) index |= (1 << i);


Density		Empty			Solid
float		[0.f, 128.f)	[128.f, 255.f]
uint8_t		[0, 128)		[128, 255]

#include <cstdint>
#include <vector>
#include <algorithm>

// uint8_t -> float
inline float DensityToFloat(uint8_t density) 
{
	return static_cast<float>(density);
}

// float -> uint8_t
inline uint8_t FloatToDensity(float density) 
{
	density = std::clamp(density, 0.0f, 255.0f);
	return static_cast<uint8_t>(density);
}

float AverageDensity(const uint8_t densities[8]) 
{
	float sum = 0.0f;
	for(int i = 0; i < 8; ++i) 
	{
		sum += DensityToFloat(densities[i]);
	}
	return sum / 8.0f;
}

uint8_t AverageDensityAsUint8(const uint8_t densities[8]) 
{
	float avg = AverageDensity(densities);
	return FloatToDensity(avg);
}
*/


#include "Engine/Math/IntVec3.hpp"
#include <array>

namespace Isosurface 
{

	constexpr int kCubeEdgeCount = 12;
	constexpr int kCubeCornerCount = 8;

	extern const std::array<IntVec3, kCubeCornerCount> kCubeCornerOffsets; // input: cornerIndex, output: IntVec3 offset to minCorner
	extern const std::array<std::array<int, 2>, kCubeEdgeCount> kEdgeVertexIndices; // input: edgeIndex, output: 
	extern const std::array<uint16_t, 256> kEdgeIntersectionFlags; // input: cubeIndex, output: edgeFlags

} // namespace Isosurface


