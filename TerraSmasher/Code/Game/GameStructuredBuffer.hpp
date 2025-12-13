#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/RendererCommon.hpp"
#include <vector>
#include <cstdint>

struct SharedVertex
{
	Vec3		m_position;			// 12 bytes
	uint8_t		m_smoothNormal[2] = { 0, 0 };
	uint8_t		m_padding[2] = { 0, 0 };

	SharedVertex(const Vec3& position, const uint8_t smoothNormal[2])
		: m_position(position)
	{
		m_smoothNormal[0] = smoothNormal[0];
		m_smoothNormal[1] = smoothNormal[1];
	}
};

struct PerVertex
{
	uint32_t m_sharedVertexIndex = 0;
	uint8_t m_materialID1 = 0;			// 1 byte
	uint8_t m_materialID2 = 0;			// 1 byte
	uint8_t m_blendValue = 0;			// 1 byte (0.f ~ 1.f)
	uint8_t m_padding = 0;

	PerVertex(uint32_t sharedVertexIndex, uint8_t matID1, uint8_t matID2, uint8_t blendValue)
		: m_sharedVertexIndex(sharedVertexIndex)
		, m_materialID1(matID1)
		, m_materialID2(matID2)
		, m_blendValue(blendValue)
	{

	}
};


class TerrainStructuredBuffer
{
public:
	~TerrainStructuredBuffer();

	void AddSharedVertex(SharedVertex const& vertex);
	void AddPerVertex(PerVertex const& vertex);


	void Reserve(int sharedNum, int perNum);
	void ClearCPUMesh();
	void ClearGPUMesh(); // Choose a good timing to release GPU resource, maybe 
	void UploadToGPU(bool needClearCPUMesh = true); // will clear cpu mesh by default


	uint32_t GetSharedVertexBufferSRVIndex() const { return m_sharedVertexBufferSRV.m_index; }
	uint32_t GetPerVertexBufferSRVIndex() const { return m_perVertexBufferSRV.m_index; }

	unsigned int GetVertexNum() const { return m_numVerts; } // For Draw Procedural

private:
	void DestroySharedVertexBuffer();
	void DestroyPerVertexBuffer();

	//-----------------------------------------------------------------------------------------------
	// Smart Scaling
private:
	static constexpr float GROWTH_FACTOR = 1.25f;
	static constexpr float SHRINK_THRESHOLD = 0.25f;
	static constexpr float SHRINK_FACTOR = 1.5f;
	static constexpr size_t MIN_CAPACITY = 3; // #ToDo: Find a better number 16? 32? 64?
	static constexpr int SHRINK_HYSTERESIS = 120;

	//int m_sharedVBFramesSinceShrinkEligible = 0;
	//int m_perVBFramesSinceShrinkEligible = 0;

	int m_sharedVBFrameWhenShrinkEligible = -1;
	int m_perVBFrameWhenShrinkEligible = -1;

private:
	std::vector<SharedVertex> m_sharedVertices;
	std::vector<PerVertex> m_perVertices;

	Buffer* m_sharedVertexBuffer = nullptr;
	DescriptorHandle m_sharedVertexBufferSRV;

	Buffer* m_perVertexBuffer = nullptr;
	DescriptorHandle m_perVertexBufferSRV;

	unsigned int m_numVerts = 0;
};

/*
	struct ColorData {
		uint8_t r, g, b, a;
	};


	float4 UnpackUint32ToFloat4(uint packedValue)
	{
		float4 result;
		result.r = ((packedValue      ) & 0xFF) / 255.0f;
		result.g = ((packedValue >> 8 ) & 0xFF) / 255.0f;
		result.b = ((packedValue >> 16) & 0xFF) / 255.0f;
		result.a = ((packedValue >> 24) & 0xFF) / 255.0f;
		return result;
	}

	uint packedColor = colorBuffer[index];
	float4 color = UnpackUint32ToFloat4(packedColor);

	float4 color;
	color.r = r / 255.0f;
	color.g = g / 255.0f;
	color.b = b / 255.0f;
	color.a = a / 255.0f;


	struct SharedVertex
	{
		float3 position;        // 12 bytes
		uint packedNormal;      // 4 bytes - Octahedral encoding (2x uint8)
	};

	struct PerVertexData
	{
		uint sharedVertexIndex; // 4 bytes
		uint packedMaterial;    // 4 bytes - materialID1(8) | materialID2(8) | blendValue(8) | padding(8)
	};

	struct SharedVertex
	{
		Vec3 position;    // 12 bytes
		uint32_t packedNormal;         // 4 bytes
	};

	struct PerVertexData
	{
		uint32_t sharedVertexIndex;    // 4 bytes
		uint32_t packedMaterial;       // 4 bytes
	};

	void AddVertex(const DirectX::XMFLOAT3& position,
				   const uint8_t normal[2],
				   uint8_t materialID1,
				   uint8_t materialID2,
				   uint8_t blendValue)
	{
		uint64_t key = CreateVertexKey(position, normal);

		uint32_t sharedIndex;
		auto it = m_vertexDeduplicationMap.find(key);

		if (it != m_vertexDeduplicationMap.end()) {

			sharedIndex = it->second;
		} else {
			sharedIndex = static_cast<uint32_t>(m_sharedVertices.size());

			SharedVertex sharedVertex;
			sharedVertex.position = position;
			sharedVertex.packedNormal = PackOctahedralNormal(normal);

			m_sharedVertices.push_back(sharedVertex);
			m_vertexDeduplicationMap[key] = sharedIndex;
		}

		PerVertexData perVertex;
		perVertex.sharedVertexIndex = sharedIndex;
		perVertex.packedMaterial = (uint32_t(materialID1) << 0) |
								  (uint32_t(materialID2) << 8) |
								  (uint32_t(blendValue) << 16);

		m_perVertexData.push_back(perVertex);
	}

	VSOutput VSMain(uint vertexID : SV_VertexID)
	{
		VSOutput output;

		PerVertexData perVertexData = g_PerVertexData[vertexID];

		SharedVertex sharedVertex = g_SharedVertices[perVertexData.sharedVertexIndex];

		float3 worldPosition = mul(float4(sharedVertex.position, 1.0f), g_WorldMatrix).xyz;
		float3 worldNormal = normalize(mul(DecodeOctahedralNormal(sharedVertex.packedNormal), (float3x3)g_WorldMatrix));

		uint materialID1 = (perVertexData.packedMaterial >> 0) & 0xFF;
		uint materialID2 = (perVertexData.packedMaterial >> 8) & 0xFF;
		uint blendValue = (perVertexData.packedMaterial >> 16) & 0xFF;

		output.position = mul(float4(worldPosition, 1.0f), g_ViewProjectionMatrix);
		output.worldPos = worldPosition;
		output.normal = worldNormal;
		output.materialIDs = float2(materialID1, materialID2);
		output.blendValue = float(blendValue) / 255.0f;
		output.vertexID = vertexID;

		return output;
	}

*/
