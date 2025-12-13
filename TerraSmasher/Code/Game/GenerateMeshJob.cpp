#include "Game/GenerateMeshJob.hpp"
#include "Game/VoxelMesher.hpp"
#include "Game/GameStructuredBuffer.hpp"
#include "Game/IntGrid3.hpp"

GenerateMeshJob::GenerateMeshJob(ChunkKey const& key, std::vector<Voxel>&& voxels, TerrainStructuredBuffer* buffer, float blockyBlendFactor, MeshingMode meshingMode)
	: Job(JobType::GENERIC, JobPriority::NORMAL)
	, m_key(key)
	, m_voxels(std::move(voxels)) // Move voxels to avoid copy
	, m_structuredBuffer(buffer)
	, m_blockyBlendFactor(blockyBlendFactor)
	, m_meshingMode(meshingMode)
{
}

GenerateMeshJob::~GenerateMeshJob()
{
	// Note: Do NOT delete m_structuredBuffer here, it's owned by MeshChunk
}

void GenerateMeshJob::Execute()
{
	if (IsCancelled() || !m_structuredBuffer)
	{
		return;
	}

	// Clipmap approach: Grid size is FIXED for all LODs
	// All LODs work with the same 10^3 voxel buffer
	const int gridSize = VOXEL_BUFFER_SIZE_PER_AXIS; // Always 10

	// Create grid info
	IntGrid3 grid(gridSize, gridSize, gridSize);

	// Offset from min corner in voxel buffer space (always 1 for all LODs)
	// This represents the expansion in the voxel buffer coordinate system
	Vec3 offsetFromMinCorner(
		static_cast<float>(MESH_CHUNK_EXPANSION),
		static_cast<float>(MESH_CHUNK_EXPANSION),
		static_cast<float>(MESH_CHUNK_EXPANSION)
	);

	// Generate mesh using VoxelMesher
	// The mesher works in model space (0-8 range for chunk content)
	// LOD scaling is handled by the ModelToWorld transform in MeshChunk
	VoxelMesher mesher(m_meshingMode);
	mesher.GenerateMesh(m_voxels, grid, m_structuredBuffer, m_blockyBlendFactor, offsetFromMinCorner);
}

// VoxelMesher is local space and voxel size is 1 unit, need model constant to scale and translate
