#pragma once
#include "Engine/Core/JobSystem.hpp"
#include "Game/WorldCommon.hpp"
#include "Game/Voxel.hpp"
#include <vector>

class VoxelMesher;
class TerrainStructuredBuffer;
enum class MeshingMode;

//-----------------------------------------------------------------------------------------------
// GenerateMeshJob
// Worker thread job that generates mesh from voxel data using VoxelMesher
// CPU mesh generation happens on worker thread, GPU upload happens on main thread
//-----------------------------------------------------------------------------------------------
class GenerateMeshJob : public Job
{
public:
	GenerateMeshJob(ChunkKey const& key, std::vector<Voxel>&& voxels, TerrainStructuredBuffer* buffer, float blockyBlendFactor, MeshingMode meshingMode);
	~GenerateMeshJob();

	void Execute() override;

	ChunkKey GetChunkKey() const { return m_key; }
	TerrainStructuredBuffer* GetStructuredBuffer() const { return m_structuredBuffer; }

private:
	ChunkKey m_key;
	std::vector<Voxel> m_voxels; // Moved from main thread
	TerrainStructuredBuffer* m_structuredBuffer = nullptr; // CPU mesh data will be written here
	float m_blockyBlendFactor = 0.f; // Remove It Later?
	MeshingMode m_meshingMode;
};


