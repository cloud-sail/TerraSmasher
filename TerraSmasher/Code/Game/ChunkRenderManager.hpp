#pragma once
#include "Game/WorldCommon.hpp"
#include <array>
#include <vector>
#include <queue>
#include <unordered_set>
#include <optional>

class MeshChunk;
class Shader;

class ChunkRenderManager
{
public:
	ChunkRenderManager();
	~ChunkRenderManager();

public:
	Shader* GetShader() const { return m_shader; };
	bool IsWireFrameMode() const { return m_isWireframeMode; }
	void SetWireFrameMode(bool isWireFrame) { m_isWireframeMode = isWireFrame; }

	MeshChunk* GetOrCreateMeshChunk(ChunkKey const& key);

	// Rendering
	void Render(std::vector<ChunkKey> const& visibleKeys);

	std::optional<ChunkKey> PopDirty();
	std::vector<ChunkKey> PopDirtyBatch(size_t maxCount);

private:
	Shader* m_shader = nullptr;
	bool m_isWireframeMode = false;


private:
	static int GetIndexFromChunkKey(ChunkKey const& key);

private:
	MeshChunk* GetChunk(ChunkKey const& key);


private:
	std::array<std::vector<MeshChunk*>, NUM_LOD_LEVELS> m_lodLayers;

//-----------------------------------------------------------------------------------------------
// Dirty Chunk Management
public:
	// Notes: When editing realtime, or initialize from sdf shape (shape has bounds), no need to mark all chunk dirty when initializing
	void MarkDirtyRegion(IntBox3 const& dirtyRegion);
	void MarkDirty(ChunkKey const& key);

private:


	bool IsDirty(ChunkKey const& key) const;

	size_t GetDirtyCount() const;

private:
	std::queue<ChunkKey> m_dirtyQueue;
	std::unordered_set<ChunkKey, ChunkKeyHash> m_dirtySet;

};

