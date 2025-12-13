#include "Game/DensityCloud.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Quantization.hpp"
#include <algorithm>

bool DensityCloud::LoadFromFile(const std::string& filePath)
{
	// Clear existing data
	Clear();

	// Check if file exists
	if (!FileExists(filePath))
	{
		return false;
	}

	// Read file to buffer
	std::vector<uint8_t> buffer;
	int bytesRead = FileReadToBuffer(buffer, filePath);

	if (bytesRead < static_cast<int>(sizeof(DensityCloudHeader)))
	{
		return false;
	}

	// Validate header
	const DensityCloudHeader* header = reinterpret_cast<const DensityCloudHeader*>(buffer.data());

	// Check FourCC
	if (header->m_fourCC[0] != 'D' ||
		header->m_fourCC[1] != 'E' ||
		header->m_fourCC[2] != 'N' ||
		header->m_fourCC[3] != 'S')
	{
		return false;
	}

	// Check version
	if (header->m_version != DENSITY_CLOUD_VERSION)
	{
		return false;
	}

	// Read dimensions
	m_sizeX = header->m_sizeX;  // X (forward)
	m_sizeY = header->m_sizeY;  // Y (left)
	m_sizeZ = header->m_sizeZ;  // Z (up)

	int totalVoxels = m_sizeX * m_sizeY * m_sizeZ;

	// Validate RLE data size
	size_t rleBytes = buffer.size() - sizeof(DensityCloudHeader);
	if ((rleBytes % sizeof(DensityRun)) != 0)
	{
		Clear();
		return false;
	}

	size_t runCount = rleBytes / sizeof(DensityRun);

	// Allocate density data
	m_densityData.resize(totalVoxels, 0);

	// Decode RLE data directly into engine order
	// No coordinate conversion needed!
	size_t outIndex = 0;
	const uint8_t* runsPtr = buffer.data() + sizeof(DensityCloudHeader);

	for (size_t i = 0; i < runCount; ++i)
	{
		const DensityRun* run = reinterpret_cast<const DensityRun*>(runsPtr + i * sizeof(DensityRun));
		uint8_t density = run->m_density;
		uint8_t length = run->m_length;

		// Validate run length
		if (length == 0)
		{
			Clear();
			return false;
		}

		// Check if we exceed total voxels
		if (outIndex + length > static_cast<size_t>(totalVoxels))
		{
			Clear();
			return false;
		}

		// Fill density values
		for (uint8_t j = 0; j < length; ++j)
		{
			m_densityData[outIndex] = density;
			++outIndex;
		}
	}

	// Verify we decoded exactly the right number of voxels
	if (outIndex != static_cast<size_t>(totalVoxels))
	{
		Clear();
		return false;
	}

	return true;

}

uint8_t DensityCloud::GetDensity(int x, int y, int z) const
{
	if (!AreCoordinatesValid(x, y, z))
	{
		return 0;
	}

	int index = GetVoxelIndex(x, y, z);
	return m_densityData[index];
}

float DensityCloud::GetDensityNormalized(int x, int y, int z) const
{
	uint8_t density = GetDensity(x, y, z);
	return static_cast<float>(density) / 255.0f;
}

void DensityCloud::SetDensity(int x, int y, int z, uint8_t density)
{
	if (!AreCoordinatesValid(x, y, z))
	{
		return;
	}

	int index = GetVoxelIndex(x, y, z);
	m_densityData[index] = density;
}

void DensityCloud::SetDensityNormalized(int x, int y, int z, float density)
{
	// Clamp to [0, 1] range
	float clamped = GetClampedZeroToOne(density);
	uint8_t densityUint8 = Quantization::ToUint8FromUNorm(clamped);
	SetDensity(x, y, z, densityUint8);
}

void DensityCloud::Clear()
{
	m_densityData.clear();
	m_sizeX = 0;
	m_sizeY = 0;
	m_sizeZ = 0;
}

void DensityCloud::Initialize(int sizeX, int sizeY, int sizeZ, uint8_t defaultDensity /*= 0*/)
{
	m_sizeX = sizeX;
	m_sizeY = sizeY;
	m_sizeZ = sizeZ;

	int totalVoxels = sizeX * sizeY * sizeZ;
	m_densityData.resize(totalVoxels, defaultDensity);
}

int DensityCloud::GetVoxelIndex(int x, int y, int z) const
{
	// Engine coordinate system: X forward, Y left, Z up
	// Traversal order: X -> Y -> Z (X changes fastest, Z changes slowest)
	return x + (y * m_sizeX) + (z * m_sizeX * m_sizeY);
}

bool DensityCloud::AreCoordinatesValid(int x, int y, int z) const
{
	return (x >= 0 && x < m_sizeX &&
		y >= 0 && y < m_sizeY &&
		z >= 0 && z < m_sizeZ);
}

