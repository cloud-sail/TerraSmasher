#pragma once
#include "Engine/Math/Quantization.hpp"
#include <array>
#include <stdint.h>



struct ToughnessProfile;

class Voxel
{
public:
	uint8_t m_density = 0; // [0, 128) is empty [128, 255] is solid, float 0.f~1.f
	uint8_t m_materialID1 = 0;
	uint8_t m_materialID2 = 0;
	uint8_t m_blendValue = 0; // 0~255 float 0.f~1.f
	uint8_t m_damage = 0;
	uint8_t m_padding0 = 0;
	uint8_t m_padding1 = 0;
	uint8_t m_padding2 = 0;


	// State Data (Gameplay)

public:
	static const Voxel AIR;

	static constexpr uint8_t INVALID_MATERIAL_ID = (uint8_t)(-1);

	// Density >= ISO_VALUE is solid, is meaningful
	static constexpr float	ISO_FLOAT_VALUE = 0.5f;
	static constexpr int	ISO_INT_VALUE = 128;

	static constexpr float	FULLY_SOLID_FLOAT = 1.f;
	static constexpr float	FULLY_EMPTY_FLOAT = 0.f;

	// Tweak This Value to have a good result
	static constexpr float	SDF_NORM_THRESHOLD = 0.8661f; // half of sqrt(3)

	// SDF Settings:  #ToDo: Test which one is better
	// Notes: SDF_NORM_THRESHOLD 1.f is good for Surface nets, 2.f is good for dual contouring
	// 
	// inward:	sdf value <= -SDF_NORM_THRESHOLD : 255; sdf value >= 0 : 0
	// outward:	sdf value <= 0 : 255; sdf value >= SDF_NORM_THRESHOLD : 0
	// average:	sdf value <= -SDF_NORM_THRESHOL : 255; sdf value >= SDF_NORM_THRESHOLD : 0 // is half
	// Distance is world space distance: need voxel Space Scale

	// 1. 255
	// 2. 255~0
	// 3. 0
	static float GetUnormDensityFromSignedDistance(float distance, float voxelSpaceScale = 1.f); 
	static uint8_t GetUint8DensityFromSignedDistance(float distance, float voxelSpaceScale = 1.f);

	static Voxel MergeFromEightChildren(std::array<Voxel, 8> const& childVoxels);

	static bool IsSolid(float d) { return d >= ISO_FLOAT_VALUE; }
	static bool IsEmpty(float d) { return d < ISO_FLOAT_VALUE; }

	inline bool IsSolid() const { return m_density >= ISO_INT_VALUE; }
	inline bool IsEmpty() const { return m_density < ISO_INT_VALUE; }

	inline float GetDensityUNorm() const { return Quantization::ToUNormFromUint8(m_density); }
	inline float GetBlendValueUNorm() const { return Quantization::ToUNormFromUint8(m_blendValue); }

	static bool AreEqual(Voxel const& a, Voxel const& b) 
	{
		return a.m_density == b.m_density &&
			a.m_materialID1 == b.m_materialID1 &&
			a.m_materialID2 == b.m_materialID2 &&
			a.m_blendValue == b.m_blendValue;
	}

	ToughnessProfile GetToughnessProfile() const;

	static bool IsInDamageRegion(float signedDistance) 
	{
		return signedDistance < -SDF_NORM_THRESHOLD;
	}

	static bool IsInAffectedRegion(float signedDistance) 
	{
		return signedDistance < SDF_NORM_THRESHOLD;
	}
};


/*
Voxel Voxel::MergeFromEightChildren(std::array<Voxel, 8> const& childVoxels)
{
	Voxel merged;

	int totalDensity = 0;
	for (const auto& child : childVoxels)
	{
		totalDensity += child.m_density;
	}
	merged.m_density = static_cast<uint8_t>(totalDensity / 8);

	// Use the first none empty material?
	merged.m_materialID1 = childVoxels[0].m_materialID1;
	merged.m_materialID2 = childVoxels[0].m_materialID2;

	int totalBlend = 0;
	for (const auto& child : childVoxels)
	{
		totalBlend += child.m_blendValue;
	}
	merged.m_blendValue = static_cast<uint8_t>(totalBlend / 8);

	return merged;
}

*/



// Voxel generator: generate a sdf voxel inside a bounding box overwritten and generate
// overwritten the place just overwritten the place where the density is not zero
// Voxel carve: 

// the data will be changed immediately, 
// hold brush will trigger brush in a fixed rate (0.1s do a single operation) 
// Flow: is 100%, set the value to dest value immediately or 50% to half


// add density: surface rise, raycast voxel changed, add what type of density
// subtract, reduce density until 0
// overwritten(paint): change matID1 matID2 blendvalue directly
// raycast only solid >= 128

// Add and change existing
// add and not change existing just increase density
// Add: will that change the existing voxel? keep original mat but increase density (increase with same speed delta)
