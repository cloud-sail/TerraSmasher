#include "Game/Voxel.hpp"
#include "Engine/Math/Quantization.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "GameMaterialDefinition.hpp"
#include "Game/VoxelBreakSystem.hpp"

const Voxel Voxel::AIR{ 0, 0, 0, 0 };


float Voxel::GetUnormDensityFromSignedDistance(float distance, float voxelSpaceScale /*= 1.f*/)
{
	return RangeMapClamped(distance, -SDF_NORM_THRESHOLD * voxelSpaceScale, SDF_NORM_THRESHOLD * voxelSpaceScale, Voxel::FULLY_SOLID_FLOAT, Voxel::FULLY_EMPTY_FLOAT);
}

uint8_t Voxel::GetUint8DensityFromSignedDistance(float distance, float voxelSpaceScale /*= 1.f*/)
{
	return Quantization::ToUint8FromUNorm(GetUnormDensityFromSignedDistance(distance, voxelSpaceScale));
}


Voxel Voxel::MergeFromEightChildren(std::array<Voxel, 8> const& childVoxels)
{
	Voxel merged;

	float totalDensity = 0.f;
	for (Voxel const& voxel : childVoxels)
	{
		totalDensity += Quantization::ToUNormFromUint8(voxel.m_density);
	}

	float avgDensity = totalDensity / 8.0f;
	merged.m_density = Quantization::ToUint8FromUNorm(avgDensity);

	if (avgDensity < ISO_FLOAT_VALUE)
	{
		// if empty only calculate avgDensity, meaningless to consider materials
		merged.m_materialID1 = static_cast<uint8_t>(-1); // invalid ID for debug
		merged.m_materialID2 = static_cast<uint8_t>(-1); // invalid ID for debug
		merged.m_blendValue = 0;
	}
	else
	{
		std::array<float, 256> matWeights = {};
		for (Voxel const& voxel : childVoxels)
		{
			float voxelDensity = Quantization::ToUNormFromUint8(voxel.m_density);

			if (voxelDensity >= ISO_FLOAT_VALUE)
			{
				// Calculate weight based on distance from iso surface
				float distanceWeight = voxelDensity - ISO_FLOAT_VALUE; // [0, 0.5]
				// Apply non-linear weight to emphasize voxels closer to the surface
				distanceWeight = distanceWeight * distanceWeight * 4.0f; // [0, 1]

				float blend = Quantization::ToUNormFromUint8(voxel.m_blendValue);

				// Accumulate material weights
				matWeights[voxel.m_materialID1] += distanceWeight * (1.0f - blend);

				if (voxel.m_materialID2 != voxel.m_materialID1)
				{
					matWeights[voxel.m_materialID2] += distanceWeight * blend;
				}
			}
		}

		// Find the two materials with highest weights
		uint8_t topMat1 = 0;
		uint8_t topMat2 = 0;
		float topWeight1 = 0.0f;
		float topWeight2 = 0.0f;

		for (int i = 0; i < 256; ++i)
		{
			if (matWeights[i] > topWeight1)
			{
				// New highest weight
				topMat2 = topMat1;
				topWeight2 = topWeight1;
				topMat1 = static_cast<uint8_t>(i);
				topWeight1 = matWeights[i];
			}
			else if (matWeights[i] > topWeight2)
			{
				// New second highest weight
				topMat2 = static_cast<uint8_t>(i);
				topWeight2 = matWeights[i];
			}
		}

		// Set merged materials, only one material means 2 matIDs are same
		merged.m_materialID1 = topMat1;
		merged.m_materialID2 = (topWeight2 > 0.0f) ? topMat2 : topMat1;

		// Calculate blend value
		if (topWeight2 > 0.0f && topMat1 != topMat2)
		{
			float blendRatio = topWeight2 / (topWeight1 + topWeight2);
			merged.m_blendValue = Quantization::ToUint8FromUNorm(blendRatio);
		}
		else
		{
			merged.m_blendValue = 0;
		}
	}

	return merged;
}

ToughnessProfile Voxel::GetToughnessProfile() const
{
	constexpr uint8_t MAT2_NEGLIGIBLE_BELOW = 30;
	constexpr uint8_t MAT1_NEGLIGIBLE_ABOVE = 225;

	if (m_blendValue < MAT2_NEGLIGIBLE_BELOW)
	{
		GameMaterialDefinition const* materialDef = GameMaterialDefinition::GetByMatID(m_materialID1);
		return { materialDef->m_tier, materialDef->m_strength };
	}

	if (m_blendValue > MAT1_NEGLIGIBLE_ABOVE)
	{
		GameMaterialDefinition const* materialDef = GameMaterialDefinition::GetByMatID(m_materialID2);
		return { materialDef->m_tier, materialDef->m_strength };
	}

	GameMaterialDefinition const* m1 = GameMaterialDefinition::GetByMatID(m_materialID1);
	GameMaterialDefinition const* m2 = GameMaterialDefinition::GetByMatID(m_materialID2);

	ToughnessProfile toughness1 = { m1->m_tier, m1->m_strength };
	ToughnessProfile toughness2 = { m2->m_tier, m2->m_strength };

	return toughness1.IsLessDurableThan(toughness2) ? toughness1 : toughness2;
}

