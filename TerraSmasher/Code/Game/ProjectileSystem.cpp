#include "Game/ProjectileSystem.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/DebugRender.hpp"

ProjectileSystem::ProjectileSystem()
{

}

ProjectileSystem::~ProjectileSystem()
{
	Shutdown();
}

void ProjectileSystem::Startup()
{
	if (m_isInitialized)
	{
		Shutdown();
	}

	// Create shader
	ShaderConfig renderConfig;
	renderConfig.m_name = "Data/Shaders/ProjectileRender";
	m_renderShader = g_theRenderer->CreateOrGetShader(renderConfig, VertexType::VERTEX_NONE);

	// Create GPU buffers
	CreateBuffers();

	m_isInitialized = true;
}

void ProjectileSystem::Shutdown()
{
	if (!m_isInitialized)
		return;

	DestroyBuffers();
	m_projectiles.clear();
	m_isInitialized = false;
}

void ProjectileSystem::SpawnProjectile(ProjectileDefinition const& def)
{
	if (m_projectiles.size() >= MAX_PROJECTILES)
		return;

	m_projectiles.emplace_back(def);
}

void ProjectileSystem::Update(float deltaSeconds, World* world)
{
	if (!m_isInitialized)
		return;

	for (Projectile& projectile : m_projectiles)
	{
		projectile.Update(deltaSeconds, world);
	}

	// Remove dead projectiles (erase-remove idiom)
	m_projectiles.erase(
		std::remove_if(m_projectiles.begin(), m_projectiles.end(),
			[](Projectile const& p) { return p.IsDead(); }),
		m_projectiles.end()
	);

	//DebugAddMessage(Stringf("%4d active bullet", m_projectiles.size()), 0.f);
}

void ProjectileSystem::Render() const
{
	if (!m_isInitialized || m_projectiles.empty())
		return;

	// Build instance data from projectiles
	std::vector<ProjectileInstanceData> instances;
	instances.reserve(m_projectiles.size());

	for (Projectile const& projectile : m_projectiles)
	{
		if (projectile.IsDead())
			continue;

		ProjectileInstanceData data = {};

		// Get current position and velocity
		Vec3 currentPos = projectile.GetPosition();
		Vec3 velocity = projectile.GetVelocity();
		float speed = velocity.GetLength();

		// Calculate trail length: base length * per-projectile multiplier
		float trailLength = m_baseTrailLength * projectile.GetLengthMultiplier();

		if (speed > 0.001f)
		{
			// Calculate start position by going backwards along velocity direction
			Vec3 velocityNormalized = velocity / speed;
			data.m_startPos = currentPos - velocityNormalized * trailLength;
			data.m_endPos = currentPos;
		}
		else
		{
			// If velocity is near zero, just use current position for both
			data.m_startPos = currentPos;
			data.m_endPos = currentPos;
		}

		// Calculate intensity based on speed
		// float speed = projectile.GetVelocity().GetLength();

		// Apply multipliers to base values
		data.m_intensity = m_baseIntensity * projectile.GetIntensityMultiplier();
		data.m_thickness = m_baseThickness * projectile.GetThicknessMultiplier();

		// Set color
		Rgba8 color = projectile.GetColor();
		color.GetAsFloats(data.m_color);

		instances.push_back(data);
	}

	if (instances.empty())
		return;

	GUARANTEE_OR_DIE(instances.size() <= MAX_PROJECTILES, "Projectile Overflow");

	// Upload instance data to GPU
	size_t dataSize = instances.size() * sizeof(ProjectileInstanceData);
	g_theRenderer->UpdateBuffer(*m_instanceBuffer, dataSize, instances.data());

	// Transition buffer for reading
	g_theRenderer->TransitionToGenericRead(*m_instanceBuffer);

	// Setup resources
	ProjectileRenderResources resources;
	resources.projectileBufferIndex = m_instanceBufferSRV.m_index;
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(ProjectileRenderResources), &resources);

	// Set render states
	g_theRenderer->BindShader(m_renderShader);
	g_theRenderer->SetBlendMode(BlendMode::ADDITIVE);  // Additive for glow
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::READ_ONLY_LESS_EQUAL);

	// Draw instances (6 vertices per quad, one instance per projectile)
	uint32_t numInstances = static_cast<uint32_t>(instances.size());
	g_theRenderer->DrawProceduralInstanced(6, numInstances);

}

int ProjectileSystem::GetActiveCount() const
{
	return static_cast<int>(m_projectiles.size());
}

void ProjectileSystem::Clear()
{
	m_projectiles.clear();
}

void ProjectileSystem::CreateBuffers()
{
	// Create instance buffer
	BufferInit bufferInit;
	bufferInit.m_size = MAX_PROJECTILES * sizeof(ProjectileInstanceData);
	bufferInit.m_name = L"Projectile Instance Structured Buffer";
	m_instanceBuffer = g_theRenderer->CreateBuffer(bufferInit);

	// Allocate SRV descriptor
	m_instanceBufferSRV = g_theRenderer->AllocateStructuredBufferSRV(
		*m_instanceBuffer,
		sizeof(ProjectileInstanceData),
		MAX_PROJECTILES
	);
}

void ProjectileSystem::DestroyBuffers()
{
	g_theRenderer->DestroyBuffer(m_instanceBuffer);
	g_theRenderer->EnqueueDeferredRelease(m_instanceBufferSRV);
}
