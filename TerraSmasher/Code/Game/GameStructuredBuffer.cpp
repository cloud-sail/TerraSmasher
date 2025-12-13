#include "Game/GameStructuredBuffer.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Buffer.hpp"

TerrainStructuredBuffer::~TerrainStructuredBuffer()
{
	DestroySharedVertexBuffer();

	DestroyPerVertexBuffer();
}

void TerrainStructuredBuffer::AddSharedVertex(SharedVertex const& vertex)
{
	m_sharedVertices.push_back(vertex);
}

void TerrainStructuredBuffer::AddPerVertex(PerVertex const& vertex)
{
	m_perVertices.push_back(vertex);
}

void TerrainStructuredBuffer::Reserve(int sharedNum, int perNum)
{
	m_sharedVertices.reserve(sharedNum);
	m_perVertices.reserve(perNum);
}

void TerrainStructuredBuffer::ClearCPUMesh()
{
	m_sharedVertices.clear();
	m_perVertices.clear();
}

// #ToDo is that a problem that zero elements in structured buffer? Add a min size
void TerrainStructuredBuffer::UploadToGPU(bool needClearCPUMesh /*= true*/)
{
	int currentFrameCount = g_theGame->GetFrameCount();

	//-----------------------------------------------------------------------------------------------
	{
		size_t currentSharedVertexBufferSize = (m_sharedVertexBuffer == nullptr) ? 0 : m_sharedVertexBuffer->GetSize();
		size_t newSharedVertexBufferSize = m_sharedVertices.size() * sizeof(SharedVertex);

		// Check if need grow buffer
		if (m_sharedVertexBuffer == nullptr || currentSharedVertexBufferSize < newSharedVertexBufferSize)
		{
			DestroySharedVertexBuffer();
			unsigned int newElementNum = static_cast<unsigned int>(m_sharedVertices.size() * GROWTH_FACTOR);
			newElementNum = static_cast<unsigned int>(std::max(static_cast<size_t>(newElementNum), MIN_CAPACITY));

			uint64_t newSize = newElementNum * sizeof(SharedVertex);
			BufferInit initData;
			initData.m_size = newSize;
			m_sharedVertexBuffer = g_theRenderer->CreateBuffer(initData);

			m_sharedVertexBufferSRV = g_theRenderer->AllocateStructuredBufferSRV(*m_sharedVertexBuffer, sizeof(SharedVertex), newElementNum);
		}
		// Check if need shrink buffer
		else if (newSharedVertexBufferSize < currentSharedVertexBufferSize * SHRINK_THRESHOLD)
		{
			if (m_sharedVBFrameWhenShrinkEligible < 0)
			{
				m_sharedVBFrameWhenShrinkEligible = currentFrameCount;
			}
			else if ((currentFrameCount - m_sharedVBFrameWhenShrinkEligible) > SHRINK_HYSTERESIS)
			{
				DestroySharedVertexBuffer();

				unsigned int newElementNum = static_cast<unsigned int>(m_sharedVertices.size() * SHRINK_FACTOR);
				newElementNum = static_cast<unsigned int>(std::max(static_cast<size_t>(newElementNum), MIN_CAPACITY));

				uint64_t newSize = newElementNum * sizeof(SharedVertex);
				BufferInit initData;
				initData.m_size = newSize;
				m_sharedVertexBuffer = g_theRenderer->CreateBuffer(initData);

				m_sharedVertexBufferSRV = g_theRenderer->AllocateStructuredBufferSRV(*m_sharedVertexBuffer, sizeof(SharedVertex), newElementNum);

				m_sharedVBFrameWhenShrinkEligible = -1;
			}
		}
		else
		{
			m_sharedVBFrameWhenShrinkEligible = -1;
		}

		//if (m_sharedVertexBuffer == nullptr || m_sharedVertexBuffer->GetSize() < m_sharedVertices.size() * sizeof(SharedVertex))
		//{
		//	DestroySharedVertexBuffer();
		//	BufferInit initData;
		//	initData.m_size = m_sharedVertices.size() * sizeof(SharedVertex);
		//	m_sharedVertexBuffer = g_theRenderer->CreateBuffer(initData);

		//	m_sharedVertexBufferSRV = g_theRenderer->AllocateStructuredBufferSRV(*m_sharedVertexBuffer, sizeof(SharedVertex), (unsigned int)m_sharedVertices.size());
		//}

		g_theRenderer->UpdateBuffer(*m_sharedVertexBuffer, m_sharedVertices.size() * sizeof(SharedVertex), m_sharedVertices.data());
		g_theRenderer->TransitionToGenericRead(*m_sharedVertexBuffer);
	}



	//-----------------------------------------------------------------------------------------------
	{
		size_t currentPerVertexBufferSize = (m_perVertexBuffer == nullptr) ? 0 : m_perVertexBuffer->GetSize();
		size_t newPerVertexBufferSize = m_perVertices.size() * sizeof(PerVertex);

		// Check if need grow buffer
		if (m_perVertexBuffer == nullptr || currentPerVertexBufferSize < newPerVertexBufferSize)
		{
			DestroyPerVertexBuffer();
			unsigned int newElementNum = static_cast<unsigned int>(m_perVertices.size() * GROWTH_FACTOR);
			newElementNum = static_cast<unsigned int>(std::max(static_cast<size_t>(newElementNum), MIN_CAPACITY));

			uint64_t newSize = newElementNum * sizeof(PerVertex);
			BufferInit initData;
			initData.m_size = newSize;
			m_perVertexBuffer = g_theRenderer->CreateBuffer(initData);

			m_perVertexBufferSRV = g_theRenderer->AllocateStructuredBufferSRV(*m_perVertexBuffer, sizeof(PerVertex), newElementNum);
		}
		// Check if need shrink buffer
		else if (newPerVertexBufferSize < currentPerVertexBufferSize * SHRINK_THRESHOLD)
		{
			if (m_perVBFrameWhenShrinkEligible < 0)
			{
				m_perVBFrameWhenShrinkEligible = currentFrameCount;
			}
			else if ((currentFrameCount - m_perVBFrameWhenShrinkEligible) > SHRINK_HYSTERESIS)
			{
				DestroyPerVertexBuffer();

				unsigned int newElementNum = static_cast<unsigned int>(m_perVertices.size() * SHRINK_FACTOR);
				newElementNum = static_cast<unsigned int>(std::max(static_cast<size_t>(newElementNum), MIN_CAPACITY));

				uint64_t newSize = newElementNum * sizeof(PerVertex);
				BufferInit initData;
				initData.m_size = newSize;
				m_perVertexBuffer = g_theRenderer->CreateBuffer(initData);

				m_perVertexBufferSRV = g_theRenderer->AllocateStructuredBufferSRV(*m_perVertexBuffer, sizeof(PerVertex), newElementNum);

				m_perVBFrameWhenShrinkEligible = -1;
			}
		}
		else
		{
			m_perVBFrameWhenShrinkEligible = -1;
		}

		//if (m_perVertexBuffer == nullptr || m_perVertexBuffer->GetSize() < m_perVertices.size() * sizeof(PerVertex))
		//{
		//	DestroyPerVertexBuffer();
		//	BufferInit initData;
		//	initData.m_size = m_perVertices.size() * sizeof(PerVertex);
		//	m_perVertexBuffer = g_theRenderer->CreateBuffer(initData);

		//	m_perVertexBufferSRV = g_theRenderer->AllocateStructuredBufferSRV(*m_perVertexBuffer, sizeof(PerVertex), (unsigned int)m_perVertices.size());
		//}

		g_theRenderer->UpdateBuffer(*m_perVertexBuffer, m_perVertices.size() * sizeof(PerVertex), m_perVertices.data());
		g_theRenderer->TransitionToGenericRead(*m_perVertexBuffer);
	}



	//-----------------------------------------------------------------------------------------------
	m_numVerts = (unsigned int)m_perVertices.size();

	if (needClearCPUMesh)
	{
		ClearCPUMesh();
	}
}

void TerrainStructuredBuffer::ClearGPUMesh()
{
	DestroySharedVertexBuffer();

	DestroyPerVertexBuffer();
}

void TerrainStructuredBuffer::DestroySharedVertexBuffer()
{
	g_theRenderer->DestroyBuffer(m_sharedVertexBuffer);
	g_theRenderer->EnqueueDeferredRelease(m_sharedVertexBufferSRV);
}

void TerrainStructuredBuffer::DestroyPerVertexBuffer()
{
	g_theRenderer->DestroyBuffer(m_perVertexBuffer);
	g_theRenderer->EnqueueDeferredRelease(m_perVertexBufferSRV);
}
