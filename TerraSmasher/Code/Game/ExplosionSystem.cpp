#include "Game/ExplosionSystem.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

//-----------------------------------------------------------------------------------------------
// Explosion Effect
//-----------------------------------------------------------------------------------------------
ExplosionEffect::ExplosionEffect(ExplosionDefinition const& def)
	: m_center(def.m_center)
	, m_maxRadius(def.m_maxRadius)
	, m_lifetime(def.m_lifetime)
	, m_color(def.m_color)
	, m_intensity(def.m_intensity)
	, m_fresnelPower(def.m_fresnelPower)
{
}

void ExplosionEffect::Update(float deltaSeconds)
{
	if (m_isDead)
		return;

	m_elapsed += deltaSeconds;
	if (m_elapsed >= m_lifetime)
	{
		m_isDead = true;
	}
}

float ExplosionEffect::GetAlpha() const
{
	if (m_lifetime <= 0.0f)
		return 1.0f;

	float alpha = m_elapsed / m_lifetime;
	if (alpha > 1.0f) alpha = 1.0f;
	return alpha;
}

float ExplosionEffect::GetCurrentRadius() const
{
	float alpha = GetAlpha();
	return m_maxRadius * alpha * alpha; // easing function
}

//-----------------------------------------------------------------------------------------------
// Explosion System
//-----------------------------------------------------------------------------------------------

ExplosionSystem::ExplosionSystem()
{
}

ExplosionSystem::~ExplosionSystem()
{
	Shutdown();
}

void ExplosionSystem::Startup()
{
	if (m_isInitialized)
	{
		Shutdown();
	}

	// Create shader (uses Vertex_PCU input layout for the sphere VB)
	ShaderConfig renderConfig;
	renderConfig.m_name = "Data/Shaders/ExplosionRender";
	m_renderShader = g_theRenderer->CreateOrGetShader(renderConfig, VertexType::VERTEX_PCU);

	CreateResources();

	m_isInitialized = true;
}

void ExplosionSystem::Shutdown()
{
	if (!m_isInitialized)
		return;

	DestroyResources();
	m_explosions.clear();
	m_isInitialized = false;
}

void ExplosionSystem::SpawnExplosion(ExplosionDefinition const& def)
{
	if (m_explosions.size() >= MAX_EXPLOSIONS)
		return;

	m_explosions.emplace_back(def);
}

void ExplosionSystem::Update(float deltaSeconds)
{
	if (!m_isInitialized)
		return;

	for (ExplosionEffect& explosion : m_explosions)
	{
		explosion.Update(deltaSeconds);
	}

	// Remove dead explosions (erase-remove idiom)
	m_explosions.erase(
		std::remove_if(m_explosions.begin(), m_explosions.end(),
			[](ExplosionEffect const& e) { return e.IsDead(); }),
		m_explosions.end()
	);
}

void ExplosionSystem::Render() const
{
	if (!m_isInitialized || m_explosions.empty())
		return;

	// Build instance data
	std::vector<ExplosionInstanceData> instances;
	instances.reserve(m_explosions.size());

	for (ExplosionEffect const& explosion : m_explosions)
	{
		if (explosion.IsDead())
			continue;

		float currentRadius = explosion.GetCurrentRadius();
		if (currentRadius < 0.001f)
			continue; // too small to see

		ExplosionInstanceData data = {};
		data.m_center = explosion.GetCenter();
		data.m_radius = currentRadius;

		Rgba8 color = explosion.GetColor();
		color.GetAsFloats(data.m_color);

		data.m_intensity = explosion.GetIntensity();
		data.m_fresnelPower = explosion.GetFresnelPower();
		data.m_alpha = explosion.GetAlpha();
		data.m_padding = 0.0f;

		instances.push_back(data);
	}

	if (instances.empty())
		return;

	GUARANTEE_OR_DIE(instances.size() <= MAX_EXPLOSIONS, "Explosion Overflow");

	// Upload instance data to GPU
	size_t dataSize = instances.size() * sizeof(ExplosionInstanceData);
	g_theRenderer->UpdateBuffer(*m_instanceBuffer, dataSize, instances.data());

	// Transition buffer for reading
	g_theRenderer->TransitionToGenericRead(*m_instanceBuffer);

	// Setup bindless resources
	ExplosionRenderResources resources;
	resources.explosionBufferIndex = m_instanceBufferSRV.m_index;
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(ExplosionRenderResources), &resources);

	// Set render states
	g_theRenderer->BindShader(m_renderShader);
	g_theRenderer->SetBlendMode(BlendMode::ADDITIVE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::READ_ONLY_LESS_EQUAL);

	// Draw instanced: shared sphere VB, one instance per explosion
	uint32_t numInstances = static_cast<uint32_t>(instances.size());
	g_theRenderer->DrawVertexBufferInstanced(m_sphereVB, m_sphereVertexCount, numInstances);
}

int ExplosionSystem::GetActiveCount() const
{
	return static_cast<int>(m_explosions.size());
}

void ExplosionSystem::Clear()
{
	m_explosions.clear();
}

void ExplosionSystem::CreateResources()
{
	// Generate unit sphere geometry (centered at origin, radius 1.0)
	std::vector<Vertex_PCU> sphereVerts;
	AddVertsForSphere3D(sphereVerts, Vec3::ZERO, 1.0f, Rgba8::OPAQUE_WHITE, AABB2::ZERO_TO_ONE, SPHERE_SLICES, SPHERE_STACKS);

	m_sphereVertexCount = static_cast<unsigned int>(sphereVerts.size());

	// Create and upload vertex buffer
	unsigned int vbSize = m_sphereVertexCount * sizeof(Vertex_PCU);
	m_sphereVB = g_theRenderer->CreateVertexBuffer(vbSize, sizeof(Vertex_PCU));
	g_theRenderer->CopyCPUToGPU(sphereVerts.data(), vbSize, m_sphereVB);

	// Create instance structured buffer
	BufferInit bufferInit;
	bufferInit.m_size = MAX_EXPLOSIONS * sizeof(ExplosionInstanceData);
	bufferInit.m_name = L"Explosion Instance Structured Buffer";
	m_instanceBuffer = g_theRenderer->CreateBuffer(bufferInit);

	// Allocate SRV descriptor
	m_instanceBufferSRV = g_theRenderer->AllocateStructuredBufferSRV(
		*m_instanceBuffer,
		sizeof(ExplosionInstanceData),
		MAX_EXPLOSIONS
	);
}

void ExplosionSystem::DestroyResources()
{
	delete m_sphereVB;
	m_sphereVB = nullptr;

	g_theRenderer->DestroyBuffer(m_instanceBuffer);
	g_theRenderer->EnqueueDeferredRelease(m_instanceBufferSRV);
}
