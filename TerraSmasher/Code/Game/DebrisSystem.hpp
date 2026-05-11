#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/RendererCommon.hpp"

#include <cstdint>

class Shader;
class Buffer;

//-----------------------------------------------------------------------------------------------
// Spawn-time definition. Each spawn picks N particles randomly inside the cone with random
// per-particle initial speed/scale/rotation/lifetime sampled from these ranges.
struct DebrisDefinition
{
	Vec3       m_emitterPos          = Vec3::ZERO;
	Vec3       m_emitterDir          = Vec3(0.f, 0.f, 1.f); // cone axis (forward)
	float      m_coneHalfAngleRadians = 0.5f;               // ~28.6 degrees
	int        m_particleCount       = 30;                  // clamped to MAX_PARTICLES_PER_EMITTER

	uint8_t    m_materialID          = 0;                   // GameMaterial index for triplanar PBR

	FloatRange m_lifetime            = FloatRange(1.5f, 3.0f);    // seconds
	FloatRange m_initialSpeed        = FloatRange(2.0f, 6.0f);    // units / sec, constant in space
	FloatRange m_initialScale        = FloatRange(0.15f, 0.45f);  // local-space half-extent at birth
	FloatRange m_eulerSpeed          = FloatRange(-3.0f, 3.0f);   // rad/sec, sampled per axis
};

//-----------------------------------------------------------------------------------------------
// GPU-side particle. Mirrored exactly by HLSL DebrisParticle. 64 bytes, 16-byte aligned.
struct DebrisParticleGPU
{
	Vec3   m_initPos;     float m_initialScale = 0.f;
	Vec3   m_velocity;    float m_lifetime     = 0.f;
	Vec3   m_initEuler;   float m_pad0         = 0.f;
	Vec3   m_angularVel;  float m_pad1         = 0.f;
};
static_assert(sizeof(DebrisParticleGPU) == 64, "DebrisParticleGPU must be 64 bytes");

//-----------------------------------------------------------------------------------------------
// One pre-allocated GPU buffer + CPU-side runtime state. Pool entries.
struct DebrisEmitterSlot
{
	Buffer*           m_particleBuffer = nullptr;          // MAX_PARTICLES_PER_EMITTER * sizeof(DebrisParticleGPU)
	DescriptorHandle  m_particleSRV;                       // pre-allocated, never re-bound

	bool              m_isActive       = false;
	float             m_age            = 0.f;
	float             m_maxLifetime    = 0.f;              // longest lifetime among this slot's particles
	uint32_t          m_particleCount  = 0;                // <= MAX_PARTICLES_PER_EMITTER
	uint8_t           m_materialID     = 0;
};

//-----------------------------------------------------------------------------------------------
// Mirrors HLSL DebrisRenderResources cbuffer. Per-draw bindless indices + scalars.
struct DebrisRenderResources
{
	uint32_t debrisVertexBufferIndex = INVALID_INDEX_U32;  // StructuredBuffer<float4>
	uint32_t particleBufferIndex     = INVALID_INDEX_U32;  // StructuredBuffer<DebrisParticle>

	uint32_t cameraConstantsIndex    = INVALID_INDEX_U32;
	uint32_t lightConstantsIndex     = INVALID_INDEX_U32;
	uint32_t materialBufferIndex     = INVALID_INDEX_U32;  // StructuredBuffer<GMaterial>

	uint32_t materialID              = 0;
	float    age                     = 0.f;
	float    pad0                    = 0.f;
};

//-----------------------------------------------------------------------------------------------
class DebrisSystem
{
public:
	DebrisSystem();
	~DebrisSystem();

	void Startup();
	void Shutdown();

	void SpawnDebris(DebrisDefinition const& def);
	void Update(float deltaSeconds);
	void Render() const;
	void Clear();

	int  GetActiveEmitterCount() const;

public:
	static constexpr uint32_t MAX_EMITTERS              = 15;
	static constexpr uint32_t MAX_PARTICLES_PER_EMITTER = 100;

private:
	void CreateSharedDebrisGeometry();
	void DestroyResources();
	int  AcquireSlot(); // free slot if any, otherwise the oldest active slot

private:
	bool m_isInitialized = false;

	DebrisEmitterSlot m_emitters[MAX_EMITTERS];

	// Shared debris model: positions packed as float4 (w=0) for 16-byte StructuredBuffer stride
	Buffer*           m_debrisVertexBuffer = nullptr;
	DescriptorHandle  m_debrisVertexSRV;
	uint32_t          m_debrisVertexCount  = 0;

	Shader*           m_renderShader       = nullptr;

	RandomNumberGenerator m_rng;
};
