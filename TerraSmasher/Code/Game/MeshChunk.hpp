#pragma once
#include "Game/WorldCommon.hpp"


class TerrainStructuredBuffer;
class ChunkRenderManager;
struct Mat44;

class MeshChunk
{
public:
	MeshChunk(ChunkRenderManager* manager, ChunkKey const& key);
	~MeshChunk();

	void Render() const;

public:
	// GPU Resource pointers
	TerrainStructuredBuffer* m_structuredVertexBuffer = nullptr;
	// 1. On Worker Thread
	// VoxelMesher mesher(MeshingMode::DUAL_CONTOURING);
	// mesher.GenerateMesh(voxels, chunk, m_structuredVertexBuffer, m_blockyBlendFactor);
	// 2. On Main Thread
	// m_structuredVertexBuffer->UploadToGPU();

private:
	Mat44 GetModelToWorldTransform() const;
private:
	ChunkRenderManager* m_manager = nullptr;
	ChunkKey m_key;
};

