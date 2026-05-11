#include "Game/VoxelWorld.hpp"
#include "Game/SDF.hpp"
#include "Game/GameCommon.hpp"
#include "Game/VoxelMesher.hpp"
#include "Game/CubeTables.hpp"
#include "Game/VoxelBreakSystem.hpp"
#include "Game/GameMaterialDefinition.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Plane3.hpp"

VoxelWorld::VoxelWorld()
	: m_pool(POOL_INITIAL_CAPACITY)
{
	for (int i = 0; i < TOTAL_CHUNKS; ++i) 
	{
		m_chunks[i].SetPool(&m_pool);
	}
}

VoxelWorld::~VoxelWorld()
{

}

VoxelRaycastResult3D VoxelWorld::FastVoxelRaycast(Vec3 rayStart, Vec3 rayForwardNormal, float rayLength) const
{
	// Assume world voxel scale is 1.f, origin point is 0,0,0

	VoxelRaycastResult3D result;
	result.m_rayStartPos = rayStart;
	result.m_rayFwdNormal = rayForwardNormal;
	result.m_rayLength = rayLength;

	// ===== Initialize starting voxel =====
	IntVec3 currentVoxelCoords(
		static_cast<int>(floorf(rayStart.x)),
		static_cast<int>(floorf(rayStart.y)),
		static_cast<int>(floorf(rayStart.z))
	);

	Voxel previousVoxel = GetVirtualVoxel(currentVoxelCoords);
	IntVec3 previousVoxelCoords = currentVoxelCoords;

	// ===== Setup 3D voxel traversal parameters =====
	// X axis
	float fwdDistPerXCrossing = 1.0f / fabsf(rayForwardNormal.x);
	int stepDirectionX = (rayForwardNormal.x < 0.0f) ? -1 : 1;
	float xAtFirstCrossing = static_cast<float>(currentVoxelCoords.x) + static_cast<float>(stepDirectionX + 1) * 0.5f;
	float xDistToFirstCrossing = xAtFirstCrossing - rayStart.x;
	float fwdDistAtNextXCrossing = fabsf(xDistToFirstCrossing) * fwdDistPerXCrossing;

	// Y axis
	float fwdDistPerYCrossing = 1.0f / fabsf(rayForwardNormal.y);
	int stepDirectionY = (rayForwardNormal.y < 0.0f) ? -1 : 1;
	float yAtFirstCrossing = static_cast<float>(currentVoxelCoords.y) + static_cast<float>(stepDirectionY + 1) * 0.5f;
	float yDistToFirstCrossing = yAtFirstCrossing - rayStart.y;
	float fwdDistAtNextYCrossing = fabsf(yDistToFirstCrossing) * fwdDistPerYCrossing;

	// Z axis
	float fwdDistPerZCrossing = 1.0f / fabsf(rayForwardNormal.z);
	int stepDirectionZ = (rayForwardNormal.z < 0.0f) ? -1 : 1;
	float zAtFirstCrossing = static_cast<float>(currentVoxelCoords.z) + static_cast<float>(stepDirectionZ + 1) * 0.5f;
	float zDistToFirstCrossing = zAtFirstCrossing - rayStart.z;
	float fwdDistAtNextZCrossing = fabsf(zDistToFirstCrossing) * fwdDistPerZCrossing;

	// ===== Main traversal loop =====
	while (true)
	{
		// Step to the nearest voxel boundary
		if (fwdDistAtNextXCrossing <= fwdDistAtNextYCrossing && fwdDistAtNextXCrossing <= fwdDistAtNextZCrossing)
		{
			if (fwdDistAtNextXCrossing > rayLength)
			{
				result.m_impactDist = rayLength;
				return result;
			}
			currentVoxelCoords.x += stepDirectionX;
			fwdDistAtNextXCrossing += fwdDistPerXCrossing;
		}
		else if (fwdDistAtNextYCrossing <= fwdDistAtNextZCrossing)
		{
			if (fwdDistAtNextYCrossing > rayLength)
			{
				result.m_impactDist = rayLength;
				return result;
			}
			currentVoxelCoords.y += stepDirectionY;
			fwdDistAtNextYCrossing += fwdDistPerYCrossing;
		}
		else
		{
			if (fwdDistAtNextZCrossing > rayLength)
			{
				result.m_impactDist = rayLength;
				return result;
			}
			currentVoxelCoords.z += stepDirectionZ;
			fwdDistAtNextZCrossing += fwdDistPerZCrossing;
		}

		Voxel currentVoxel = GetVirtualVoxel(currentVoxelCoords);

		// ===== Check for empty-to-solid transition =====
		if (previousVoxel.IsEmpty() && currentVoxel.IsSolid())
		{
			// ===== Compute precise impact distance using linear interpolation =====
			Vec3 emptyVoxelCenter = VoxelWorldUtils::GetWorldVoxelCenterFromWorldVoxelCoords(previousVoxelCoords);
			Vec3 solidVoxelCenter = VoxelWorldUtils::GetWorldVoxelCenterFromWorldVoxelCoords(currentVoxelCoords);

			float distanceToEmptyCenter = DotProduct3D(emptyVoxelCenter - rayStart, rayForwardNormal);
			float distanceToSolidCenter = DotProduct3D(solidVoxelCenter - rayStart, rayForwardNormal);

			float emptyVoxelDensity = Quantization::ToUNormFromUint8(previousVoxel.m_density);
			float solidVoxelDensity = Quantization::ToUNormFromUint8(currentVoxel.m_density);

			// Linear interpolation to find ISO surface (0.5)
			float interpolationT = (Voxel::ISO_FLOAT_VALUE - emptyVoxelDensity) / (solidVoxelDensity - emptyVoxelDensity);
			float impactDistance = distanceToEmptyCenter + interpolationT * (distanceToSolidCenter - distanceToEmptyCenter);

			result.m_didImpact = true;
			result.m_impactDist = impactDistance;
			result.m_impactPos = rayStart + rayForwardNormal * impactDistance;
			result.m_voxel = currentVoxel;

			// ===== Compute impact normal using SDF gradient =====
			// Find the base voxel whose 8 neighbors' centers surround the impact point
			// The 8 voxel centers form a cube that contains impactPos
			IntVec3 baseVoxelCoords(
				static_cast<int>(floorf(result.m_impactPos.x - 0.5f)),
				static_cast<int>(floorf(result.m_impactPos.y - 0.5f)),
				static_cast<int>(floorf(result.m_impactPos.z - 0.5f))
			);

			// Gather density values from 8 surrounding voxels
			// These 8 voxels' centers form a unit cube surrounding the impact point
			float cornerDensities[8];
			for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
			{
				//IntVec3 cornerOffset(
				//	cornerIndex & 1,
				//	(cornerIndex >> 1) & 1,
				//	(cornerIndex >> 2) & 1
				//);
				IntVec3 cornerOffset = Isosurface::kCubeCornerOffsets[cornerIndex];

				IntVec3 voxelCoords = baseVoxelCoords + cornerOffset;
				Voxel voxel = GetVirtualVoxel(voxelCoords);
				cornerDensities[cornerIndex] = Quantization::ToUNormFromUint8(voxel.m_density);
			}

			// Calculate local position within the cube formed by 8 voxel centers (0-1 range)
			// The cube ranges from (base + 0.5) to (base + 1.5) in each dimension
			Vec3 baseVoxelCenter = VoxelWorldUtils::GetWorldVoxelCenterFromWorldVoxelCoords(baseVoxelCoords);
			Vec3 localPositionInVoxel(
				result.m_impactPos.x - baseVoxelCenter.x,
				result.m_impactPos.y - baseVoxelCenter.y,
				result.m_impactPos.z - baseVoxelCenter.z
			);

			// Compute gradient (points toward increasing density) and negate for surface normal
			Vec3 densityGradient = VoxelMesher::GetSdfGradient(localPositionInVoxel, cornerDensities);
			result.m_impactNormal = -densityGradient.GetNormalized();

			return result;
		}

		// Move to next voxel
		previousVoxel = currentVoxel;
		previousVoxelCoords = currentVoxelCoords;
	}

	return result;
}

Voxel VoxelWorld::GetVirtualVoxel(IntVec3 const& worldCoords) const
{
	//GUARANTEE_OR_DIE(VoxelWorldUtils::IsValidWorldCoords(worldCoords), "World Coords is not valid");

	if (!VoxelWorldUtils::IsValidWorldCoords(worldCoords))
	{
		return Voxel::AIR;
	}

	IntVec3 chunkCoords = worldCoords >> CHUNK_BITS;

	DataChunk const* chunk = GetChunk(chunkCoords);

	if (chunk == nullptr)
	{
		return Voxel::AIR;
	}

	IntVec3 localCoords = VoxelWorldUtils::GetLocalCoordsFromWorldCoords(worldCoords);
	return chunk->GetVoxel(localCoords);
}

//-----------------------------------------------------------------------------------------------
// GetVoxelByNodeKey - Get voxel at specified LOD level with caching
// LOD0: Direct access to actual voxel data (no cache)
// LOD1/LOD2: Cached merged voxels for performance
//-----------------------------------------------------------------------------------------------
Voxel VoxelWorld::GetVoxelByNodeKey(IntVec3 const& coords, int lodLevel) const
{
	// LOD0: Direct access to actual voxel data
	if (lodLevel == 0)
	{
		return GetVirtualVoxel(coords);
	}

	// LOD1 and LOD2: Try cache first
	NodeKey key(lodLevel, coords);

	auto it = m_lodCache.find(key);
	if (it != m_lodCache.end())
	{
		// Cache hit - return cached value
		return it->second;
	}

	// Cache miss - calculate by merging 8 children recursively


	std::array<Voxel, 8> childVoxels;
	IntVec3 minChild = coords << 1;
	for (int index = 0; index < 8; ++index)
	{
		IntVec3 childOffset = IntVec3(index & 1, (index >> 1) & 1, (index >> 2) & 1);
		childVoxels[index] = GetVoxelByNodeKey(minChild + childOffset, lodLevel - 1);
	}

	Voxel mergedVoxel = Voxel::MergeFromEightChildren(childVoxels);

	// Store in cache for future queries
	m_lodCache[key] = mergedVoxel;

	return mergedVoxel;

	/*
	 //Higher LOD levels: Sample from lower LOD
	 //LOD1: Each voxel represents 2^3 voxels from LOD0
	 //LOD2: Each voxel represents 4^3 voxels from LOD0

	int voxelSize = 1 << lodLevel; // LOD1=2, LOD2=4

	// Calculate the base world coordinates for this LOD voxel
	IntVec3 baseWorldCoords = coords * voxelSize;

	// Simple sampling: Take the min corner voxel
	// This is the simplest approach and works well for visualization
	// More advanced: Could merge 8 child voxels using Voxel::MergeFromEightChildren
	return GetVirtualVoxel(baseWorldCoords);
	 
	 Advanced implementation (commented out for now):
	 Sample multiple voxels and merge them
	std::array<Voxel, 8> childVoxels;
	int halfSize = voxelSize / 2;

	for (int i = 0; i < 8; ++i)
	{
		IntVec3 offset(
			(i & 1) * halfSize,
			((i >> 1) & 1) * halfSize,
			((i >> 2) & 1) * halfSize
		);
		childVoxels[i] = GetVoxelAtLOD(baseWorldCoords + offset, lodLevel - 1);
	}

	return Voxel::MergeFromEightChildren(childVoxels);
	*/
}

//-----------------------------------------------------------------------------------------------
// ExtractVoxelsForMesh - Extract voxel buffer for mesh generation
// Clipmap approach: All LODs extract fixed 10³ voxels, but each voxel represents different world space size
//-----------------------------------------------------------------------------------------------
void VoxelWorld::ExtractVoxelsForMesh(ChunkKey const& key, std::vector<Voxel>& voxels)
{
	// ChunkKey -> Node Key
	IntVec3 startNodeCoords = (key.m_chunkCoords << LOD0_CHUNK_BITS) - IntVec3(MESH_CHUNK_EXPANSION, MESH_CHUNK_EXPANSION, MESH_CHUNK_EXPANSION);

	voxels.clear();
	voxels.reserve(TOTAL_VOXEL_BUFFER_SIZE); // #ToDo A VoxelBufferPool

	for (int z = 0; z < VOXEL_BUFFER_SIZE_PER_AXIS; ++z)
	{
		for (int y = 0; y < VOXEL_BUFFER_SIZE_PER_AXIS; ++y)
		{
			for (int x = 0; x < VOXEL_BUFFER_SIZE_PER_AXIS; ++x)
			{

				// Calculate world coordinates with voxel stride
				IntVec3 currentNodeCoords = startNodeCoords + IntVec3(x, y, z);

				// Get voxel at appropriate LOD level
				Voxel voxel = GetVoxelByNodeKey(currentNodeCoords, key.m_lodLevel);
				voxels.push_back(voxel);
			}
		}
	}

	// Try to clean up LOD0 chunks after extraction
	// This is a good time to reclaim memory from empty chunks?
	if (key.m_lodLevel == 0)
	{
		DataChunk* chunk = GetChunk(key.m_chunkCoords);
		if (chunk)
		{
			chunk->TryCleanup();
		}
	}
}


bool VoxelWorld::BakeSDFShape(SDFShape const& shape, IntBox3& out_region)
{
	GUARANTEE_OR_DIE(shape.m_sdf != nullptr, "SDF is null");

	AABB3 worldAABB = shape.m_sdf->GetAffectedAABB();
	IntBox3 region = IntBox3::MakeFromAABB3(worldAABB);
	region = VoxelWorldUtils::GetSafeWorldBoxRegion(region);

	bool anyModified = false;

	// Collect all modified voxel coordinates for batch cache invalidation
	std::unordered_set<IntVec3> modifiedCoords;

	IntVec3 regionMins = region.m_mins;
	IntVec3 regionMaxs = region.m_mins + region.m_dimensions;

	for (int z = regionMins.z; z < regionMaxs.z; ++z)
	{
		for (int y = regionMins.y; y < regionMaxs.y; ++y)
		{
			for (int x = regionMins.x; x < regionMaxs.x; ++x)
			{
				IntVec3 worldCoords(x, y, z);
				Vec3 worldPos = VoxelWorldUtils::GetWorldVoxelCenterFromWorldVoxelCoords(worldCoords);

				uint8_t newDensity{}, newMatID1{}, newMatID2{}, newBlendValue{};
				shape.ComputeVoxelData(worldPos, newDensity, newMatID1, newMatID2, newBlendValue);

				if (newDensity == 0)
				{
					continue;
				}

				Voxel& oldVoxel = GetOrCreateVoxelRef(worldCoords);

				uint8_t finalDensity = (newDensity > oldVoxel.m_density) ? newDensity : oldVoxel.m_density;

				bool modified = (finalDensity != oldVoxel.m_density) ||
					(newMatID1 != oldVoxel.m_materialID1) ||
					(newMatID2 != oldVoxel.m_materialID2) ||
					(newBlendValue != oldVoxel.m_blendValue);

				if (modified)
				{
					oldVoxel.m_density = finalDensity;
					oldVoxel.m_materialID1 = newMatID1;
					oldVoxel.m_materialID2 = newMatID2;
					oldVoxel.m_blendValue = newBlendValue;
					anyModified = true;

					// Track modified coordinate for batch cache invalidation
					modifiedCoords.insert(worldCoords);
				}
			}
		}
	}

	// Batch invalidate LOD cache for all modified voxels
	if (!modifiedCoords.empty())
	{
		InvalidateCacheForWorldCoordsSet(modifiedCoords);
	}

	out_region = region;
	return anyModified;
}




bool VoxelWorld::AddWithSDF(SDF const* sdf, uint8_t deltaDensity, uint8_t matID1, uint8_t matID2, uint8_t blendValue, IntBox3& out_region)
{
	GUARANTEE_OR_DIE(sdf != nullptr, "SDF is null");

	AABB3 worldAABB = sdf->GetAffectedAABB();
	IntBox3 region = IntBox3::MakeFromAABB3(worldAABB);
	region = VoxelWorldUtils::GetSafeWorldBoxRegion(region);

	bool anyModified = false;

	// Collect all modified voxel coordinates for batch cache invalidation
	std::unordered_set<IntVec3> modifiedCoords;

	IntVec3 regionMins = region.m_mins;
	IntVec3 regionMaxs = region.m_mins + region.m_dimensions;

	for (int z = regionMins.z; z < regionMaxs.z; ++z)
	{
		for (int y = regionMins.y; y < regionMaxs.y; ++y)
		{
			for (int x = regionMins.x; x < regionMaxs.x; ++x)
			{
				IntVec3 worldCoords(x, y, z);
				Vec3 worldPos = VoxelWorldUtils::GetWorldVoxelCenterFromWorldVoxelCoords(worldCoords);

				float signedDistance = sdf->GetSignedDistance(worldPos);
				uint8_t targetDensity = Voxel::GetUint8DensityFromSignedDistance(signedDistance);

				if (targetDensity == 0)
				{
					continue;
				}

				Voxel& oldVoxel = GetOrCreateVoxelRef(worldCoords);

				uint8_t oldDensity = oldVoxel.m_density;

				if (oldDensity >= targetDensity)
				{
					continue; // no need to add density
				}

				uint8_t newDensity = (oldDensity > 255 - deltaDensity) ? 255 : oldDensity + deltaDensity;
				if (newDensity > targetDensity)
				{
					newDensity = targetDensity;
				}

				// if old voxel is air, set material
				if (oldDensity == 0)
				{
					oldVoxel.m_materialID1 = matID1;
					oldVoxel.m_materialID2 = matID2;
					oldVoxel.m_blendValue = blendValue;
				}

				oldVoxel.m_density = newDensity;
				anyModified = true;

				// Track modified coordinate for batch cache invalidation
				modifiedCoords.insert(worldCoords);
			}
		}
	}

	// Batch invalidate LOD cache for all modified voxels
	if (!modifiedCoords.empty())
	{
		InvalidateCacheForWorldCoordsSet(modifiedCoords);
	}

	out_region = region;
	return anyModified;
}

bool VoxelWorld::PaintWithSDF(SDF const* sdf, uint8_t matID1, uint8_t matID2, uint8_t blendValue, IntBox3& out_region)
{
	GUARANTEE_OR_DIE(sdf != nullptr, "SDF is null");

	AABB3 worldAABB = sdf->GetAffectedAABB();
	IntBox3 region = IntBox3::MakeFromAABB3(worldAABB);
	region = VoxelWorldUtils::GetSafeWorldBoxRegion(region);

	bool anyModified = false;

	// Collect all modified voxel coordinates for batch cache invalidation
	std::unordered_set<IntVec3> modifiedCoords;

	IntVec3 regionMins = region.m_mins;
	IntVec3 regionMaxs = region.m_mins + region.m_dimensions;

	for (int z = regionMins.z; z < regionMaxs.z; ++z)
	{
		for (int y = regionMins.y; y < regionMaxs.y; ++y)
		{
			for (int x = regionMins.x; x < regionMaxs.x; ++x)
			{
				IntVec3 worldCoords(x, y, z);
				Vec3 worldPos = VoxelWorldUtils::GetWorldVoxelCenterFromWorldVoxelCoords(worldCoords);

				float signedDistance = sdf->GetSignedDistance(worldPos);
				uint8_t brushDensity = Voxel::GetUint8DensityFromSignedDistance(signedDistance);

				if (brushDensity == 0)
				{
					continue;
				}

				Voxel* voxelPtr = GetVoxelPtrIfAllocated(worldCoords);

				if (!voxelPtr)
				{
					continue;
				}

				if (voxelPtr->m_density == 0)
				{
					continue;
				}

				bool modified = (voxelPtr->m_materialID1 != matID1) ||
					(voxelPtr->m_materialID2 != matID2) ||
					(voxelPtr->m_blendValue != blendValue);

				if (modified)
				{
					voxelPtr->m_materialID1 = matID1;
					voxelPtr->m_materialID2 = matID2;
					voxelPtr->m_blendValue = blendValue;
					anyModified = true;

					// Track modified coordinate for batch cache invalidation
					modifiedCoords.insert(worldCoords);
				}
			}
		}
	}

	// Batch invalidate LOD cache for all modified voxels
	if (!modifiedCoords.empty())
	{
		InvalidateCacheForWorldCoordsSet(modifiedCoords);
	}

	out_region = region;
	return anyModified;
}

bool VoxelWorld::CarveWithSDF(SDF const* sdf, uint8_t deltaDensity, IntBox3& out_region)
{
	GUARANTEE_OR_DIE(sdf != nullptr, "SDF is null");

	AABB3 worldAABB = sdf->GetAffectedAABB();
	IntBox3 region = IntBox3::MakeFromAABB3(worldAABB);
	region = VoxelWorldUtils::GetSafeWorldBoxRegion(region);

	bool anyModified = false;

	// Collect all modified voxel coordinates for batch cache invalidation
	std::unordered_set<IntVec3> modifiedCoords;

	IntVec3 regionMins = region.m_mins;
	IntVec3 regionMaxs = region.m_mins + region.m_dimensions;

	for (int z = regionMins.z; z < regionMaxs.z; ++z)
	{
		for (int y = regionMins.y; y < regionMaxs.y; ++y)
		{
			for (int x = regionMins.x; x < regionMaxs.x; ++x)
			{
				IntVec3 worldCoords(x, y, z);
				Vec3 worldPos = VoxelWorldUtils::GetWorldVoxelCenterFromWorldVoxelCoords(worldCoords);

				float signedDistance = sdf->GetSignedDistance(worldPos);
				uint8_t brushDensity = Voxel::GetUint8DensityFromSignedDistance(signedDistance);

				if (brushDensity == 0)
				{
					continue;
				}

				Voxel* voxelPtr = GetVoxelPtrIfAllocated(worldCoords);

				if (!voxelPtr)
				{
					continue;
				}

				uint8_t oldDensity = voxelPtr->m_density;
				uint8_t targetDensity = 255 - brushDensity;

				if (oldDensity <= targetDensity) // also skip oldDensity == 0
				{
					continue;
				}

				uint8_t newDensity = (oldDensity > deltaDensity) ? (oldDensity - deltaDensity) : 0;
				if (newDensity < targetDensity)
				{
					newDensity = targetDensity;
				}

				if (newDensity != oldDensity)
				{
					voxelPtr->m_density = newDensity;
					anyModified = true;

					// Track modified coordinate for batch cache invalidation
					modifiedCoords.insert(worldCoords);
				}
			}
		}
	}

	// Batch invalidate LOD cache for all modified voxels
	if (!modifiedCoords.empty())
	{
		InvalidateCacheForWorldCoordsSet(modifiedCoords);
	}

	out_region = region;
	return anyModified;
}

bool VoxelWorld::CarveWithSDFTracked(SDF const* sdf, uint8_t deltaDensity, IntBox3& out_region, std::vector<float>& out_materialVolumes)
{
	GUARANTEE_OR_DIE(sdf != nullptr, "SDF is null");

	AABB3 worldAABB = sdf->GetAffectedAABB();
	IntBox3 region = IntBox3::MakeFromAABB3(worldAABB);
	region = VoxelWorldUtils::GetSafeWorldBoxRegion(region);

	bool anyModified = false;

	// Collect all modified voxel coordinates for batch cache invalidation
	std::unordered_set<IntVec3> modifiedCoords;

	IntVec3 regionMins = region.m_mins;
	IntVec3 regionMaxs = region.m_mins + region.m_dimensions;

	for (int z = regionMins.z; z < regionMaxs.z; ++z)
	{
		for (int y = regionMins.y; y < regionMaxs.y; ++y)
		{
			for (int x = regionMins.x; x < regionMaxs.x; ++x)
			{
				IntVec3 worldCoords(x, y, z);
				Vec3 worldPos = VoxelWorldUtils::GetWorldVoxelCenterFromWorldVoxelCoords(worldCoords);

				float signedDistance = sdf->GetSignedDistance(worldPos);
				uint8_t brushDensity = Voxel::GetUint8DensityFromSignedDistance(signedDistance);

				if (brushDensity == 0)
				{
					continue;
				}

				Voxel* voxelPtr = GetVoxelPtrIfAllocated(worldCoords);

				if (!voxelPtr)
				{
					continue;
				}

				uint8_t oldDensity = voxelPtr->m_density;
				uint8_t targetDensity = 255 - brushDensity;

				if (oldDensity <= targetDensity) // also skip oldDensity == 0
				{
					continue;
				}

				uint8_t newDensity = (oldDensity > deltaDensity) ? (oldDensity - deltaDensity) : 0;
				if (newDensity < targetDensity)
				{
					newDensity = targetDensity;
				}

				if (newDensity != oldDensity)
				{
					// Track per-material volume before modifying density
					float densityReduction = Quantization::ToUNormFromUint8(oldDensity) - Quantization::ToUNormFromUint8(newDensity);
					float blend = Quantization::ToUNormFromUint8(voxelPtr->m_blendValue);

					uint8_t matID1 = voxelPtr->m_materialID1;
					uint8_t matID2 = voxelPtr->m_materialID2;

					if (matID1 < out_materialVolumes.size())
					{
						out_materialVolumes[matID1] += densityReduction * (1.0f - blend);
					}
					if (matID2 < out_materialVolumes.size())
					{
						out_materialVolumes[matID2] += densityReduction * blend;
					}

					voxelPtr->m_density = newDensity;
					anyModified = true;

					// Track modified coordinate for batch cache invalidation
					modifiedCoords.insert(worldCoords);
				}
			}
		}
	}

	// Batch invalidate LOD cache for all modified voxels
	if (!modifiedCoords.empty())
	{
		InvalidateCacheForWorldCoordsSet(modifiedCoords);
	}

	out_region = region;
	return anyModified;
}

StrikeResult VoxelWorld::StrikeWithSDFTracked(SDF const* sdf, StrikeContext const& ctx)
{
	GUARANTEE_OR_DIE(sdf != nullptr, "SDF is null");

	StrikeResult result;
	size_t matCount = GameMaterialDefinition::s_definitions.size();
	result.materialVolumesRemoved.resize(matCount, 0.0f);
	result.materialDamageAccumulated.resize(matCount, 0.0f);

	AABB3   worldAABB = sdf->GetAffectedAABB();
	IntBox3 region = IntBox3::MakeFromAABB3(worldAABB);
	region = VoxelWorldUtils::GetSafeWorldBoxRegion(region);
	result.affectedRegion = region;

	IntVec3 regionMins = region.m_mins;
	IntVec3 regionMaxs = region.m_mins + region.m_dimensions;

	//-----------------------------------------------------------------------------------------------
	// Pass 1
	bool				triggerBreak = false;
	ToughnessProfile	criterionProfile{ 0, 0 };

	for (int z = regionMins.z; z < regionMaxs.z; ++z)
	{
		for (int y = regionMins.y; y < regionMaxs.y; ++y)
		{
			for (int x = regionMins.x; x < regionMaxs.x; ++x)
			{
				IntVec3 worldCoords(x, y, z);
				Vec3    worldPos = VoxelWorldUtils::GetWorldVoxelCenterFromWorldVoxelCoords(worldCoords);

				float signedDistance = sdf->GetSignedDistance(worldPos);
				if (!Voxel::IsInAffectedRegion(signedDistance)) continue;

				Voxel* voxelPtr = GetVoxelPtrIfAllocated(worldCoords);
				if (!voxelPtr) continue;
				if (voxelPtr->m_density == 0) continue;

				ToughnessProfile profile = voxelPtr->GetToughnessProfile();

				uint8_t damagePerHit = 0;
				StrikeOutcome outcome = ComputeStrikeOutcome(profile, ctx, damagePerHit);

				if (outcome == StrikeOutcome::Instant) 
				{
					triggerBreak = true;
					if (criterionProfile.IsLessDurableThan(profile)) 
					{
						criterionProfile = profile;
					}
				}
				else if (outcome == StrikeOutcome::Cumulative) 
				{
					if (Voxel::IsInDamageRegion(signedDistance)) 
					{
						uint8_t oldDamage = voxelPtr->m_damage;
						int     newDamage = (int)oldDamage + (int)damagePerHit;
						uint8_t clampedNewDamage = (uint8_t)std::min(newDamage, 255);

						if (clampedNewDamage > oldDamage)
						{
							float damageDelta = Quantization::ToUNormFromUint8(clampedNewDamage - oldDamage);
							float blend = Quantization::ToUNormFromUint8(voxelPtr->m_blendValue);
							uint8_t matID1 = voxelPtr->m_materialID1;
							uint8_t matID2 = voxelPtr->m_materialID2;
							if (matID1 < matCount)
								result.materialDamageAccumulated[matID1] += damageDelta * (1.0f - blend);
							if (matID2 < matCount)
								result.materialDamageAccumulated[matID2] += damageDelta * blend;

							result.anyModified = true;
						}

						voxelPtr->m_damage = clampedNewDamage;

						if (newDamage >= 255)
						{
							triggerBreak = true;
							if (criterionProfile.IsLessDurableThan(profile))
							{
								criterionProfile = profile;
							}
						}
					}
					// Transition Region: do not apply damage
				}

				// Immune: do nothing
			}
		}
	}

	result.breakTriggered = triggerBreak;

	if (!triggerBreak) 
	{
		return result;
	}

	//-----------------------------------------------------------------------------------------------
	// Pass 2: decrease density
	// Collect all modified voxel coordinates for batch cache invalidation
	std::unordered_set<IntVec3> densityChangedCoords;

	for (int z = regionMins.z; z < regionMaxs.z; ++z)
	{
		for (int y = regionMins.y; y < regionMaxs.y; ++y)
		{
			for (int x = regionMins.x; x < regionMaxs.x; ++x)
			{
				IntVec3 worldCoords(x, y, z);
				Vec3 worldPos = VoxelWorldUtils::GetWorldVoxelCenterFromWorldVoxelCoords(worldCoords);

				float signedDistance = sdf->GetSignedDistance(worldPos);
				uint8_t brushDensity = Voxel::GetUint8DensityFromSignedDistance(signedDistance);
				if (brushDensity == 0) continue;

				Voxel* voxelPtr = GetVoxelPtrIfAllocated(worldCoords);
				if (!voxelPtr) continue;
				if (voxelPtr->m_density == 0) continue;

				ToughnessProfile profile = voxelPtr->GetToughnessProfile();

				uint8_t damagePerHit = 0;
				StrikeOutcome outcome = ComputeStrikeOutcome(profile, ctx, damagePerHit);

				if (outcome == StrikeOutcome::Immune) continue;
				if (criterionProfile.IsLessDurableThan(profile)) continue;

				uint8_t oldDensity = voxelPtr->m_density;
				uint8_t targetDensity = 255 - brushDensity;

				if (oldDensity <= targetDensity) // also skip oldDensity == 0
				{
					continue;
				}

				uint8_t newDensity = targetDensity;


				float densityReduction = Quantization::ToUNormFromUint8(oldDensity) - Quantization::ToUNormFromUint8(newDensity);
				float blend = Quantization::ToUNormFromUint8(voxelPtr->m_blendValue);
				uint8_t matID1 = voxelPtr->m_materialID1;
				uint8_t matID2 = voxelPtr->m_materialID2;

				if (matID1 < result.materialVolumesRemoved.size())
				{
					result.materialVolumesRemoved[matID1] += densityReduction * (1.0f - blend);
				}
				if (matID2 < result.materialVolumesRemoved.size())
				{
					result.materialVolumesRemoved[matID2] += densityReduction * blend;
				}

				voxelPtr->m_density = newDensity;
				result.anyModified = true;

				// Track modified coordinate for batch cache invalidation
				densityChangedCoords.insert(worldCoords);
			}
		}
	}

	// Batch invalidate LOD cache for all modified voxels
	if (!densityChangedCoords.empty())
	{
		InvalidateCacheForWorldCoordsSet(densityChangedCoords);
	}

	return result;
}

bool VoxelWorld::FlattenToPlane(SDF const* sdf, Plane3 const& plane, uint8_t deltaDensity, uint8_t matID1, uint8_t matID2, uint8_t blendValue, IntBox3& out_region)
{
	GUARANTEE_OR_DIE(sdf != nullptr, "SDF is null");

	AABB3 worldAABB = sdf->GetAffectedAABB();
	IntBox3 region = IntBox3::MakeFromAABB3(worldAABB);
	region = VoxelWorldUtils::GetSafeWorldBoxRegion(region);

	bool anyModified = false;

	// Collect all modified voxel coordinates for batch cache invalidation
	std::unordered_set<IntVec3> modifiedCoords;

	IntVec3 regionMins = region.m_mins;
	IntVec3 regionMaxs = region.m_mins + region.m_dimensions;

	for (int z = regionMins.z; z < regionMaxs.z; ++z)
	{
		for (int y = regionMins.y; y < regionMaxs.y; ++y)
		{
			for (int x = regionMins.x; x < regionMaxs.x; ++x)
			{
				IntVec3 worldCoords(x, y, z);
				Vec3 worldPos = VoxelWorldUtils::GetWorldVoxelCenterFromWorldVoxelCoords(worldCoords);

				// Only process voxels inside the SDF region (SDF < 0)
				float sdfDistance = sdf->GetSignedDistance(worldPos);
				if (sdfDistance >= 0.0f)
				{
					continue; // Outside SDF region, skip
				}

				// Calculate signed distance from voxel to plane
				float planeDistance = plane.GetSignedDistanceToPoint(worldPos);

				// Convert plane distance to target density
				// Voxels above the plane (positive distance) should be empty
				// Voxels below the plane (negative distance) should be solid
				uint8_t targetDensity = Voxel::GetUint8DensityFromSignedDistance(planeDistance);

				// Get current voxel (or create if we need to add material)
				Voxel* voxelPtr = GetVoxelPtrIfAllocated(worldCoords);
				uint8_t oldDensity = voxelPtr ? voxelPtr->m_density : 0;

				// Determine if we need to increase or decrease density
				if (oldDensity < targetDensity)
				{
					// Need to ADD density (move towards solid)
					uint8_t newDensity = (oldDensity > 255 - deltaDensity) ? 255 : oldDensity + deltaDensity;
					if (newDensity > targetDensity)
					{
						newDensity = targetDensity; // Don't overshoot
					}

					// Get or create voxel reference
					Voxel& voxel = GetOrCreateVoxelRef(worldCoords);

					// If previously air, set material
					if (oldDensity == 0)
					{
						voxel.m_materialID1 = matID1;
						voxel.m_materialID2 = matID2;
						voxel.m_blendValue = blendValue;
					}

					voxel.m_density = newDensity;
					anyModified = true;
					modifiedCoords.insert(worldCoords);
				}
				else if (oldDensity > targetDensity)
				{
					// Need to CARVE density (move towards air)
					if (!voxelPtr || oldDensity == 0)
					{
						continue; // Already air or not allocated, nothing to carve
					}

					uint8_t newDensity = (oldDensity > deltaDensity) ? (oldDensity - deltaDensity) : 0;
					if (newDensity < targetDensity)
					{
						newDensity = targetDensity; // Don't overshoot
					}

					if (newDensity != oldDensity)
					{
						voxelPtr->m_density = newDensity;
						anyModified = true;
						modifiedCoords.insert(worldCoords);
					}
				}
				// else: oldDensity == targetDensity, already at target, do nothing
			}
		}
	}

	// Batch invalidate LOD cache for all modified voxels
	if (!modifiedCoords.empty())
	{
		InvalidateCacheForWorldCoordsSet(modifiedCoords);
	}

	out_region = region;
	return anyModified;
}

Voxel& VoxelWorld::GetOrCreateVoxelRef(IntVec3 const& worldCoords)
{
	GUARANTEE_OR_DIE(VoxelWorldUtils::IsValidWorldCoords(worldCoords), "World Coords is not valid");

	IntVec3 chunkCoords = worldCoords >> CHUNK_BITS;

	DataChunk* chunk = GetChunk(chunkCoords);

	GUARANTEE_OR_DIE(chunk != nullptr, "Chunk is null");

	IntVec3 localCoords = VoxelWorldUtils::GetLocalCoordsFromWorldCoords(worldCoords);
	return chunk->GetVoxel(localCoords);

}

Voxel* VoxelWorld::GetVoxelPtrIfAllocated(IntVec3 const& worldCoords)
{
	GUARANTEE_OR_DIE(VoxelWorldUtils::IsValidWorldCoords(worldCoords), "World Coords is not valid");

	IntVec3 chunkCoords = worldCoords >> CHUNK_BITS;

	DataChunk* chunk = GetChunk(chunkCoords);

	if (!chunk || !chunk->IsAllocated())
	{
		return nullptr;
	}

	IntVec3 localCoords = VoxelWorldUtils::GetLocalCoordsFromWorldCoords(worldCoords);
	return &chunk->GetVoxel(localCoords);
}

DataChunk* VoxelWorld::GetChunk(IntVec3 chunkCoords)
{
	if (!VoxelWorldUtils::IsValidChunkCoords(chunkCoords))
	{
		return nullptr;
	}

	return &m_chunks[VoxelWorldUtils::GetChunkIndexFromChunkCoords(chunkCoords)];
}

DataChunk const* VoxelWorld::GetChunk(IntVec3 chunkCoords) const
{
	if (!VoxelWorldUtils::IsValidChunkCoords(chunkCoords))
	{
		return nullptr;
	}

	return &m_chunks[VoxelWorldUtils::GetChunkIndexFromChunkCoords(chunkCoords)];
}


IntBox3 VoxelWorldUtils::GetSafeWorldBoxRegion(IntBox3 const& oldRegion)
{
	IntBox3 worldBox(IntVec3(0, 0, 0), IntVec3(WORLD_SIZE, WORLD_SIZE, WORLD_SIZE));
	IntBox3 result = oldRegion;
	result.ClampWithIn(worldBox);
	return result;
}



//-----------------------------------------------------------------------------------------------
// Cache Invalidation Helper Functions
//-----------------------------------------------------------------------------------------------

// Invalidate cache for a single modified voxel
// When a LOD0 voxel is modified, its parent LOD1 and grandparent LOD2 cache entries become stale
void VoxelWorld::InvalidateCacheForWorldCoords(IntVec3 const& worldCoords)
{
	for (int lod = 1; lod < NUM_LOD_LEVELS; ++lod)
	{
		IntVec3 currentLodCoords = NodeKey::GetAncestorCoords(worldCoords, lod);
		NodeKey currentLodKey(lod, currentLodCoords);
		m_lodCache.erase(currentLodKey);
	}
}

// Batch invalidate cache for multiple modified voxels (more efficient than calling InvalidateCacheForWorldCoords repeatedly)
// Uses a set to avoid duplicate invalidations when multiple LOD0 voxels belong to the same LOD1/LOD2 node
void VoxelWorld::InvalidateCacheForWorldCoordsSet(std::unordered_set<IntVec3> const& modifiedCoords)
{
	// Collect unique LOD1 and LOD2 keys that need to be invalidated
	std::unordered_set<NodeKey, NodeKeyHash> keysToInvalidate;

	for (IntVec3 const& worldCoords : modifiedCoords)
	{
		for (int lod = 1; lod < NUM_LOD_LEVELS; ++lod)
		{
			IntVec3 currentLodCoords = NodeKey::GetAncestorCoords(worldCoords, lod);
			keysToInvalidate.insert(NodeKey(lod, currentLodCoords));
		}
	}

	// Batch erase all affected cache entries
	for (NodeKey const& key : keysToInvalidate)
	{
		m_lodCache.erase(key);
	}
}

