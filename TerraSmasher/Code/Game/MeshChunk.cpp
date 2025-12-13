#include "Game/MeshChunk.hpp"
#include "Game/GameCommon.hpp"
#include "Game/ChunkRenderManager.hpp"
#include "Game/GameStructuredBuffer.hpp"
#include "Game/GameMaterialDefinition.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Renderer/Renderer.hpp"

MeshChunk::MeshChunk(ChunkRenderManager* manager, ChunkKey const& key)
	: m_manager(manager)
	, m_key(key)
{
	m_structuredVertexBuffer = new TerrainStructuredBuffer();
}

MeshChunk::~MeshChunk()
{
	delete m_structuredVertexBuffer;
	m_structuredVertexBuffer = nullptr;
}

void MeshChunk::Render() const
{
	if (m_structuredVertexBuffer->GetVertexNum() <= 0)
	{
		return; // no verts to draw
	}

	g_theRenderer->SetModelConstants(GetModelToWorldTransform());

	ExperimentalTerrainRenderResource res;

	res.engineConstantsIndex = g_theRenderer->GetCurrentEngineConstantsIndex();
	res.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	res.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();
	res.lightConstantsIndex = g_theRenderer->GetCurrentLightConstantsIndex();
	res.perFrameConstantsIndex = g_theRenderer->GetCurrentPerFrameConstantsIndex();

	res.materialBufferIndex = GameMaterialDefinition::GetMaterialBufferIndex();
	res.sharedVertexBufferIndex = m_structuredVertexBuffer->GetSharedVertexBufferSRVIndex();
	res.perVertexBufferIndex = m_structuredVertexBuffer->GetPerVertexBufferSRVIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(ExperimentalTerrainRenderResource), &res);

	g_theRenderer->BindShader(m_manager->GetShader());
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(m_manager->IsWireFrameMode() ? RasterizerMode::WIREFRAME_CULL_NONE : RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

	g_theRenderer->DrawProcedural(m_structuredVertexBuffer->GetVertexNum());
}

Mat44 MeshChunk::GetModelToWorldTransform() const
{
	int chunkSize = CoordsUtils::GetChunkSizeForLOD(m_key.m_lodLevel);
	Vec3 position(m_key.m_chunkCoords * chunkSize);
	float uniformScale = static_cast<float>(1 << m_key.m_lodLevel);

	Mat44 result;
	result.AppendScaleUniform3D(uniformScale);
	result.SetTranslation3D(position);
	return result;
}
