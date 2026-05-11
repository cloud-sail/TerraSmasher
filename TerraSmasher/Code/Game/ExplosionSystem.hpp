#pragma once
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/RendererCommon.hpp"

#include <vector>

struct ExplosionDefinition
{
	Vec3	m_center;
	float	m_maxRadius = 5.0f;
	float	m_lifetime = 0.5f;
	Rgba8	m_color = Rgba8(255, 200, 100, 255);
	float	m_intensity = 2.0f;
	float	m_fresnelPower = 3.0f;
};


class ExplosionEffect
{
public:
	ExplosionEffect(ExplosionDefinition const& def);

	void Update(float deltaSeconds);

	bool  IsDead() const { return m_isDead; }
	Vec3  GetCenter() const { return m_center; }
	Rgba8 GetColor() const { return m_color; }
	float GetMaxRadius() const { return m_maxRadius; }
	float GetIntensity() const { return m_intensity; }
	float GetFresnelPower() const { return m_fresnelPower; }

	// alpha = elapsed / lifetime, in [0, 1]
	float GetAlpha() const;

	// r = maxRadius * alpha * alpha
	float GetCurrentRadius() const;

private:
	Vec3	m_center;
	float	m_maxRadius;
	float	m_lifetime;
	float	m_elapsed = 0.0f;
	Rgba8	m_color;
	float	m_intensity;
	float	m_fresnelPower;
	bool	m_isDead = false;
};

//-----------------------------------------------------------------------------------------------
struct ExplosionInstanceData
{
	Vec3	m_center;
	float	m_radius;
	float	m_color[4];
	float	m_intensity;
	float	m_fresnelPower;
	float	m_alpha;
	float	m_padding;
};


struct ExplosionRenderResources
{
	uint32_t explosionBufferIndex = INVALID_INDEX_U32;
	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
};


class ExplosionSystem
{
public:
	ExplosionSystem();
	~ExplosionSystem();

	void Startup();
	void Shutdown();

	void SpawnExplosion(ExplosionDefinition const& def);

	void Update(float deltaSeconds);

	void Render() const;

	int GetActiveCount() const;

	void Clear();

public:
	static constexpr uint32_t MAX_EXPLOSIONS = 256;
	static constexpr int SPHERE_SLICES = 32;
	static constexpr int SPHERE_STACKS = 16;

private:
	void CreateResources();
	void DestroyResources();

	bool m_isInitialized = false;

	std::vector<ExplosionEffect> m_explosions;

	// Shared unit sphere vertex buffer
	VertexBuffer* m_sphereVB = nullptr;
	unsigned int m_sphereVertexCount = 0;

	// GPU buffer for per-instance data
	Buffer* m_instanceBuffer = nullptr;
	DescriptorHandle m_instanceBufferSRV;

	// Shader
	Shader* m_renderShader = nullptr;
};

