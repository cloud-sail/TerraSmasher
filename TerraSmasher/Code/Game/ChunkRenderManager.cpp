#include "Game/ChunkRenderManager.hpp"
#include "Game/MeshChunk.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/Renderer.hpp"

ChunkRenderManager::ChunkRenderManager()
{
	m_shader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/ExperimentalTerrain"), VertexType::VERTEX_NONE);



	for (int lod = 0; lod < NUM_LOD_LEVELS; ++lod)
	{
		int totalChunks = CoordsUtils::GetTotalChunksForLOD(lod);
		auto& lodLayer = m_lodLayers[lod];
		lodLayer.reserve(totalChunks);

		int perAxis = CoordsUtils::GetChunksPerAxis(lod);

		for (int z = 0; z < perAxis; ++z)
		{
			for (int y = 0; y < perAxis; ++y)
			{
				for (int x = 0; x < perAxis; ++x)
				{
					ChunkKey key(IntVec3(x, y, z), lod);
					//int idx = GetIndexFromChunkKey(key); // if not reserve but resize use this
					//lodLayer[idx] = new MeshChunk(key);
					lodLayer.push_back(new MeshChunk(this, key));
				}

			}
		}
	}
}

ChunkRenderManager::~ChunkRenderManager()
{
	for (int lod = 0; lod < NUM_LOD_LEVELS; ++lod)
	{
		for (MeshChunk* chunk : m_lodLayers[lod])
		{
			delete chunk;
		}
	}
}

int ChunkRenderManager::GetIndexFromChunkKey(ChunkKey const& key)
{
	// Not Check
	int perAxis = CoordsUtils::GetChunksPerAxis(key.m_lodLevel);
	return key.m_chunkCoords.x + key.m_chunkCoords.y * perAxis + key.m_chunkCoords.z * perAxis * perAxis;
}

MeshChunk* ChunkRenderManager::GetChunk(ChunkKey const& key)
{
	//std::vector<MeshChunk*>& layer = m_lodLayers[key.m_lodLevel];
	int chunkIndex = GetIndexFromChunkKey(key);

	return m_lodLayers[key.m_lodLevel][chunkIndex];
}

void ChunkRenderManager::MarkDirtyRegion(IntBox3 const& dirtyRegion)
{
	std::vector<ChunkKey> keys = GetDirtyChunksInRegion(dirtyRegion);

	for (ChunkKey const& key : keys)
	{
		MarkDirty(key);
	}
}

void ChunkRenderManager::MarkDirty(ChunkKey const& key)
{
	// Already in dirty queue
	if (m_dirtySet.find(key) != m_dirtySet.end())
	{
		return;
	}

	m_dirtyQueue.push(key);
	m_dirtySet.insert(key);
}

MeshChunk* ChunkRenderManager::GetOrCreateMeshChunk(ChunkKey const& key)
{
	// All mesh chunks are already created in constructor
	// Just return the existing one
	return GetChunk(key);
}

void ChunkRenderManager::Render(std::vector<ChunkKey> const& visibleKeys)
{
	if (visibleKeys.empty())
	{
		return;
	}

	// Render each visible chunk
	// Note: Each MeshChunk::Render() sets its own render state
	for (ChunkKey const& key : visibleKeys)
	{
		MeshChunk* chunk = GetChunk(key);
		if (chunk)
		{
			chunk->Render();
		}
	}
}

std::optional<ChunkKey> ChunkRenderManager::PopDirty()
{
	if (m_dirtyQueue.empty()) 
	{
		return std::nullopt;
	}

	ChunkKey key = m_dirtyQueue.front();
	m_dirtyQueue.pop();
	m_dirtySet.erase(key);

	return key;
}

std::vector<ChunkKey> ChunkRenderManager::PopDirtyBatch(size_t maxCount)
{
	std::vector<ChunkKey> result;
	result.reserve(std::min(maxCount, m_dirtyQueue.size()));

	while (!m_dirtyQueue.empty() && result.size() < maxCount) 
	{
		ChunkKey key = m_dirtyQueue.front();
		m_dirtyQueue.pop();
		m_dirtySet.erase(key);
		result.push_back(key);
	}

	return result;
}

bool ChunkRenderManager::IsDirty(ChunkKey const& key) const
{
	return m_dirtySet.find(key) != m_dirtySet.end();
}

size_t ChunkRenderManager::GetDirtyCount() const
{
	return m_dirtyQueue.size();
}
