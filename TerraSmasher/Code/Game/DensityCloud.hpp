#pragma once
#include <string>
#include <vector>


//-----------------------------------------------------------------------------------------------
// Density Cloud File Format Constants
//-----------------------------------------------------------------------------------------------
constexpr uint8_t DENSITY_CLOUD_VERSION = 1;

//-----------------------------------------------------------------------------------------------
// File format header structure
// Note: Dimensions are stored in ENGINE coordinate system (X forward, Y left, Z up)
//       Data is stored in ENGINE traversal order (X -> Y -> Z)
//       No coordinate conversion needed when loading!
//-----------------------------------------------------------------------------------------------
struct DensityCloudHeader
{
	char    m_fourCC[4];	// 'D','E','N','S'
	uint8_t m_version;		// 1
	uint8_t m_sizeX;		// Grid size X (forward)
	uint8_t m_sizeY;		// Grid size Y (left)
	uint8_t m_sizeZ;		// Grid size Z (up)
};
static_assert(sizeof(DensityCloudHeader) == 8, "DensityCloudHeader must be 8 bytes");

//-----------------------------------------------------------------------------------------------
// RLE run structure for density data
//-----------------------------------------------------------------------------------------------
struct DensityRun
{
	uint8_t m_density;   // density value [0, 255] (0.0 to 1.0)
	uint8_t m_length;    // run length [1, 255]
};
static_assert(sizeof(DensityRun) == 2, "DensityRun must be 2 bytes");


//-----------------------------------------------------------------------------------------------
// DensityCloud class to manage 3D density field data
//-----------------------------------------------------------------------------------------------
class DensityCloud
{
public:
	DensityCloud() = default;
	~DensityCloud() = default;

	// File I/O
	bool LoadFromFile(const std::string& filePath);

	// Data access
	uint8_t GetDensity(int x, int y, int z) const;
	float GetDensityNormalized(int x, int y, int z) const; // Returns [0.0, 1.0]
	void SetDensity(int x, int y, int z, uint8_t density);
	void SetDensityNormalized(int x, int y, int z, float density); // Input [0.0, 1.0]

	// Dimension queries
	int GetSizeX() const { return m_sizeX; }
	int GetSizeY() const { return m_sizeY; }
	int GetSizeZ() const { return m_sizeZ; }
	int GetTotalVoxels() const { return m_sizeX * m_sizeY * m_sizeZ; }

	// Utility
	bool IsValid() const { return !m_densityData.empty(); }
	void Clear();
	void Initialize(int sizeX, int sizeY, int sizeZ, uint8_t defaultDensity = 0);

private:
	// Convert 3D coordinates to 1D array index
	// Engine uses X forward, Y left, Z up with traversal order X->Y->Z
	int GetVoxelIndex(int x, int y, int z) const;

	// Validate coordinates
	bool AreCoordinatesValid(int x, int y, int z) const;

private:
	int m_sizeX = 0;  // Engine X (forward)
	int m_sizeY = 0;  // Engine Y (left)
	int m_sizeZ = 0;  // Engine Z (up)
	std::vector<uint8_t> m_densityData;
};
