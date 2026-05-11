#include "Game/DebrisSystem.hpp"
#include "Game/GameCommon.hpp"
#include "Game/GameMaterialDefinition.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Math/MathUtils.hpp"

#include <vector>
#include <cmath>

//-----------------------------------------------------------------------------------------------
DebrisSystem::DebrisSystem()
{
}

DebrisSystem::~DebrisSystem()
{
	Shutdown();
}

//-----------------------------------------------------------------------------------------------
void DebrisSystem::Startup()
{
	if (m_isInitialized) Shutdown();

	ShaderConfig cfg;
	cfg.m_name = "Data/Shaders/DebrisRender";
	m_renderShader = g_theRenderer->CreateOrGetShader(cfg, VertexType::VERTEX_NONE);

	CreateSharedDebrisGeometry();

	for (uint32_t i = 0; i < MAX_EMITTERS; ++i)
	{
		DebrisEmitterSlot& slot = m_emitters[i];

		BufferInit bi;
		bi.m_size = MAX_PARTICLES_PER_EMITTER * sizeof(DebrisParticleGPU);
		bi.m_name = L"Debris Emitter Particle Buffer";
		slot.m_particleBuffer = g_theRenderer->CreateBuffer(bi);

		slot.m_particleSRV = g_theRenderer->AllocateStructuredBufferSRV(
			*slot.m_particleBuffer,
			sizeof(DebrisParticleGPU),
			MAX_PARTICLES_PER_EMITTER
		);
	}

	m_isInitialized = true;
}

//-----------------------------------------------------------------------------------------------
void DebrisSystem::Shutdown()
{
	if (!m_isInitialized) return;
	DestroyResources();
	m_isInitialized = false;
}

//-----------------------------------------------------------------------------------------------
void DebrisSystem::CreateSharedDebrisGeometry()
{
	// Octahedron with mild per-vertex perturbation rolled once at startup so the chunk
	// looks irregular instead of perfectly symmetric. 8 faces, each face is its own
	// 3 verts so face normals come from ddx/ddy in the pixel shader without any seams.
	Vec3 base[6] = {
		Vec3( 1.0f,  0.0f,  0.0f), // +X
		Vec3(-1.0f,  0.0f,  0.0f), // -X
		Vec3( 0.0f,  1.0f,  0.0f), // +Y
		Vec3( 0.0f, -1.0f,  0.0f), // -Y
		Vec3( 0.0f,  0.0f,  1.0f), // +Z
		Vec3( 0.0f,  0.0f, -1.0f), // -Z
	};

	for (int i = 0; i < 6; ++i)
	{
		base[i] += Vec3(
			m_rng.RollRandomFloatInRange(-0.18f, 0.18f),
			m_rng.RollRandomFloatInRange(-0.18f, 0.18f),
			m_rng.RollRandomFloatInRange(-0.18f, 0.18f));
	}

	// 8 octahedron faces, CCW from outside (so SOLID_CULL_BACK keeps front faces)
	int const faces[8][3] = {
		{0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4},  // +Z hemisphere
		{2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5},  // -Z hemisphere
	};

	std::vector<float> packedVerts;
	packedVerts.reserve(8 * 3 * 4);

	auto pushVec = [&](Vec3 const& v)
	{
		packedVerts.push_back(v.x);
		packedVerts.push_back(v.y);
		packedVerts.push_back(v.z);
		packedVerts.push_back(0.f);
	};

	for (int f = 0; f < 8; ++f)
	{
		pushVec(base[faces[f][0]]);
		pushVec(base[faces[f][1]]);
		pushVec(base[faces[f][2]]);
	}

	m_debrisVertexCount = 8 * 3; // 24 verts

	BufferInit bi;
	bi.m_size = packedVerts.size() * sizeof(float);
	bi.m_name = L"Debris Shared Vertex Buffer";
	m_debrisVertexBuffer = g_theRenderer->CreateBuffer(bi);

	g_theRenderer->UpdateBuffer(*m_debrisVertexBuffer,
		packedVerts.size() * sizeof(float), packedVerts.data());

	m_debrisVertexSRV = g_theRenderer->AllocateStructuredBufferSRV(
		*m_debrisVertexBuffer,
		sizeof(float) * 4,
		m_debrisVertexCount
	);

	g_theRenderer->TransitionToGenericRead(*m_debrisVertexBuffer);
}

//-----------------------------------------------------------------------------------------------
void DebrisSystem::DestroyResources()
{
	for (uint32_t i = 0; i < MAX_EMITTERS; ++i)
	{
		DebrisEmitterSlot& slot = m_emitters[i];
		if (slot.m_particleBuffer)
		{
			g_theRenderer->DestroyBuffer(slot.m_particleBuffer);
		}
		g_theRenderer->EnqueueDeferredRelease(slot.m_particleSRV);
		slot = DebrisEmitterSlot{};
	}

	if (m_debrisVertexBuffer)
	{
		g_theRenderer->DestroyBuffer(m_debrisVertexBuffer);
	}
	g_theRenderer->EnqueueDeferredRelease(m_debrisVertexSRV);
	m_debrisVertexCount = 0;
}

//-----------------------------------------------------------------------------------------------
int DebrisSystem::AcquireSlot()
{
	for (uint32_t i = 0; i < MAX_EMITTERS; ++i)
	{
		if (!m_emitters[i].m_isActive) return (int)i;
	}
	int evictIdx = 0;
	float oldestAge = m_emitters[0].m_age;
	for (uint32_t i = 1; i < MAX_EMITTERS; ++i)
	{
		if (m_emitters[i].m_age > oldestAge)
		{
			oldestAge = m_emitters[i].m_age;
			evictIdx = (int)i;
		}
	}
	return evictIdx;
}

//-----------------------------------------------------------------------------------------------
// Uniform-area sample of a direction inside a cone around `axis` with given half-angle.
static Vec3 SampleConeDirection(RandomNumberGenerator& rng, Vec3 const& axis, float halfAngleRadians)
{
	float cosHalf = cosf(halfAngleRadians);
	float u = rng.RollRandomFloatInRange(cosHalf, 1.0f); // cos(theta)
	float sinTheta = sqrtf(fmaxf(0.f, 1.f - u * u));
	float phi = rng.RollRandomFloatInRange(0.f, 6.28318530718f);

	// Cone-local frame (cone axis = +Z)
	Vec3 localDir(sinTheta * cosf(phi), sinTheta * sinf(phi), u);

	// Build orthonormal basis with `axis` as the +Z and rotate localDir into world
	Vec3 z = axis.GetNormalized();
	Vec3 helper = (fabsf(z.z) < 0.99f) ? Vec3(0.f, 0.f, 1.f) : Vec3(1.f, 0.f, 0.f);
	Vec3 x = CrossProduct3D(helper, z).GetNormalized();
	Vec3 y = CrossProduct3D(z, x);
	return (x * localDir.x) + (y * localDir.y) + (z * localDir.z);
}

//-----------------------------------------------------------------------------------------------
void DebrisSystem::SpawnDebris(DebrisDefinition const& def)
{
	if (!m_isInitialized) return;
	if (def.m_particleCount <= 0) return;

	int n = def.m_particleCount;
	if (n > (int)MAX_PARTICLES_PER_EMITTER) n = (int)MAX_PARTICLES_PER_EMITTER;

	int slotIdx = AcquireSlot();
	DebrisEmitterSlot& slot = m_emitters[slotIdx];

	DebrisParticleGPU particles[MAX_PARTICLES_PER_EMITTER];
	float maxLifetime = 0.f;

	for (int i = 0; i < n; ++i)
	{
		Vec3 dir = SampleConeDirection(m_rng, def.m_emitterDir, def.m_coneHalfAngleRadians);
		float speed = m_rng.RollRandomFloatInRange(def.m_initialSpeed.m_min, def.m_initialSpeed.m_max);

		DebrisParticleGPU& p = particles[i];
		p.m_initPos      = def.m_emitterPos;
		p.m_velocity     = dir * speed;
		p.m_initialScale = m_rng.RollRandomFloatInRange(def.m_initialScale.m_min, def.m_initialScale.m_max);
		p.m_lifetime     = m_rng.RollRandomFloatInRange(def.m_lifetime.m_min, def.m_lifetime.m_max);

		p.m_initEuler    = Vec3(
			m_rng.RollRandomFloatInRange(0.f, 6.28318530718f),
			m_rng.RollRandomFloatInRange(0.f, 6.28318530718f),
			m_rng.RollRandomFloatInRange(0.f, 6.28318530718f));
		p.m_angularVel   = Vec3(
			m_rng.RollRandomFloatInRange(def.m_eulerSpeed.m_min, def.m_eulerSpeed.m_max),
			m_rng.RollRandomFloatInRange(def.m_eulerSpeed.m_min, def.m_eulerSpeed.m_max),
			m_rng.RollRandomFloatInRange(def.m_eulerSpeed.m_min, def.m_eulerSpeed.m_max));
		p.m_pad0 = 0.f;
		p.m_pad1 = 0.f;

		if (p.m_lifetime > maxLifetime) maxLifetime = p.m_lifetime;
	}

	size_t uploadBytes = (size_t)n * sizeof(DebrisParticleGPU);
	g_theRenderer->UpdateBuffer(*slot.m_particleBuffer, uploadBytes, particles);
	g_theRenderer->TransitionToGenericRead(*slot.m_particleBuffer);

	slot.m_isActive      = true;
	slot.m_age           = 0.f;
	slot.m_maxLifetime   = maxLifetime;
	slot.m_particleCount = (uint32_t)n;
	slot.m_materialID    = def.m_materialID;
}

//-----------------------------------------------------------------------------------------------
void DebrisSystem::Update(float deltaSeconds)
{
	if (!m_isInitialized) return;

	for (uint32_t i = 0; i < MAX_EMITTERS; ++i)
	{
		DebrisEmitterSlot& s = m_emitters[i];
		if (!s.m_isActive) continue;
		s.m_age += deltaSeconds;
		if (s.m_age >= s.m_maxLifetime)
		{
			s.m_isActive = false;
		}
	}
}

//-----------------------------------------------------------------------------------------------
void DebrisSystem::Render() const
{
	if (!m_isInitialized) return;

	g_theRenderer->BindShader(m_renderShader);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

	for (uint32_t i = 0; i < MAX_EMITTERS; ++i)
	{
		DebrisEmitterSlot const& s = m_emitters[i];
		if (!s.m_isActive || s.m_particleCount == 0) continue;

		DebrisRenderResources rr;
		rr.debrisVertexBufferIndex = m_debrisVertexSRV.m_index;
		rr.particleBufferIndex     = s.m_particleSRV.m_index;
		rr.cameraConstantsIndex    = g_theRenderer->GetCurrentCameraConstantsIndex();
		rr.lightConstantsIndex     = g_theRenderer->GetCurrentLightConstantsIndex();
		rr.materialBufferIndex     = GameMaterialDefinition::GetMaterialBufferIndex();
		rr.materialID              = (uint32_t)s.m_materialID;
		rr.age                     = s.m_age;

		g_theRenderer->SetGraphicsBindlessResources(sizeof(DebrisRenderResources), &rr);
		g_theRenderer->DrawProceduralInstanced(m_debrisVertexCount, s.m_particleCount);
	}
}

//-----------------------------------------------------------------------------------------------
void DebrisSystem::Clear()
{
	for (uint32_t i = 0; i < MAX_EMITTERS; ++i)
	{
		m_emitters[i].m_isActive = false;
		m_emitters[i].m_age = 0.f;
		m_emitters[i].m_particleCount = 0;
	}
}

//-----------------------------------------------------------------------------------------------
int DebrisSystem::GetActiveEmitterCount() const
{
	int count = 0;
	for (uint32_t i = 0; i < MAX_EMITTERS; ++i)
	{
		if (m_emitters[i].m_isActive) ++count;
	}
	return count;
}
