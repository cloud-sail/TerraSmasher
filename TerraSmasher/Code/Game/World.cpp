#include "Game/World.hpp"
#include "Game/WorldCommon.hpp"
#include "Game/App.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Voxel.hpp"
#include "Game/IntGrid3.hpp"
#include "Game/SdfPrimitives.hpp"
#include "Game/VoxelMesher.hpp"
#include "Game/GameMaterialDefinition.hpp"
#include "Game/ShipDefinition.hpp"
#include "Game/GameStructuredBuffer.hpp"
#include "Game/ImGuiUtils.hpp"
#include "Game/SDF.hpp"
#include "Game/PlayerShip.hpp"
#include "Game/ShipTypes.hpp"
#include "Game/BloomEffect.hpp"
#include "Game/ProjectileSystem.hpp"

#include "Game/VoxelWorld.hpp"
#include "Game/ChunkRenderManager.hpp"
#include "Game/MeshChunk.hpp"
#include "Game/GenerateMeshJob.hpp"

#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/RaycastUtils.hpp"
#include "Engine/Math/IntBox3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Frustum.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Window/Window.hpp"

#include <vector>

#include "ThirdParty/imgui/imgui.h"
#include "ThirdParty/Noise/SmoothNoise.hpp"
#include <filesystem>

World::~World()
{
	delete m_projectileSystem;
	m_projectileSystem = nullptr;

	RenderPassShutdown();
	
	// Clean up SDF shapes
	ClearAllShapes();
	FlushJobSystemAndRetrieveJobs();

	ShipDefinition::ClearDefinitions();
	GameMaterialDefinition::ClearDefinitions();

	//delete m_surfaceNets;
	//m_surfaceNets = nullptr;

	//delete m_debugVertexBuffer;
	//m_debugVertexBuffer = nullptr;

	//delete m_debugIndexBuffer;
	//m_debugIndexBuffer = nullptr;

	//delete m_vertexBuffer;
	//m_vertexBuffer = nullptr;

	//delete m_indexBuffer;
	//m_indexBuffer = nullptr;

	//delete m_structuredVertexBuffer;
	//m_structuredVertexBuffer = nullptr;

	// Destroy voxel world and render manager (If it is Brush / Space Mode)
	delete m_voxelWorld;
	m_voxelWorld = nullptr;

	delete m_chunkRenderManager;
	m_chunkRenderManager = nullptr;

	// Destroy Player (If it is Space Mode)
	delete m_player;
	m_player = nullptr;
}

World::World()
	: m_sceneFileSelector("Data\\Scenes", ".xml")
	, m_densityCloudFileSelector("Data\\Models", ".dcloud")
{
	//m_shader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/Terrain"), VertexType::VERTEX_TERRAIN);
	//m_experimentalShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/ExperimentalTerrain"), VertexType::VERTEX_NONE);

	//ShaderConfig tbnShaderConfig;
	//tbnShaderConfig.m_name = "Data/Shaders/DebugNormal";
	//tbnShaderConfig.m_stages = (ShaderStage)(SHADER_STAGE_VS | SHADER_STAGE_PS | SHADER_STAGE_GS);
	//m_debugNormalShader = g_theRenderer->CreateOrGetShader(tbnShaderConfig, VertexType::VERTEX_PNMD);

	//m_debugVertexBuffer = g_theRenderer->CreateVertexBuffer(1 * sizeof(Vertex_PCU), sizeof(Vertex_PCU));
	//m_debugIndexBuffer = g_theRenderer->CreateIndexBuffer(1 * sizeof(unsigned int));

	//m_vertexBuffer = g_theRenderer->CreateVertexBuffer(1 * sizeof(Vertex_Terrain), sizeof(Vertex_Terrain));
	//m_indexBuffer = g_theRenderer->CreateIndexBuffer(1 * sizeof(unsigned int));

	//m_structuredVertexBuffer = new TerrainStructuredBuffer();
	m_unlitEmissiveShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/UnlitEmissive"), VertexType::VERTEX_PCU);
	m_diffuseShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/Diffuse"), VertexType::VERTEX_PCU);
	GameMaterialDefinition::InitializeDefinitions(g_gameConfigBlackboard.GetValue("gameMaterialDefinitionsPath", "Data/Definitions/GameMaterialDefinitions.xml").c_str());
	ShipDefinition::InitializeDefinitions();

	g_theJobSystem->StartAcceptingJobs();

	InitializeSkybox();
	RenderPassStartup();

	m_projectileSystem = new ProjectileSystem();
	m_projectileSystem->Startup();
}

void World::FlushJobSystemAndRetrieveJobs()
{
	g_theJobSystem->Flush(JobPriority::CRITICAL);
	std::vector<Job*> completedJobs = g_theJobSystem->RetrieveCompletedJobs();
	for (Job* job : completedJobs)
	{
		delete job;
	}

	UNUSED(completedJobs);
}

void World::ApplyAddBrush(Vec3 const& worldPos, Vec3 const& normal)
{
	if (!m_voxelWorld || !m_chunkRenderManager) return;

	SDFCutSphere* brushSDF = new SDFCutSphere(m_brushRadius, m_brushRadius * -0.3f);
	Mat44 brushTransform = Mat44::MakeFromZ(-normal);
	brushSDF->GetMutTransform().m_position = worldPos;
	brushSDF->GetMutTransform().m_orientation = brushTransform.GetEulerAngles();


	IntBox3 dirtyRegion;
	bool wasModified = m_voxelWorld->AddWithSDF(
		brushSDF,
		m_brushStrength,
		m_brushMaterialID1,
		m_brushMaterialID2,
		m_brushBlendValue,
		dirtyRegion
	);

	delete brushSDF;

	if (wasModified)
	{
		m_chunkRenderManager->MarkDirtyRegion(dirtyRegion);
	}
}

void World::ApplyPaintBrush(Vec3 const& worldPos)
{
	if (!m_voxelWorld || !m_chunkRenderManager) return;

	// Create brush SDF
	SDFSphere* brushSDF = new SDFSphere(m_brushRadius);
	brushSDF->GetMutTransform().m_position = worldPos;

	IntBox3 dirtyRegion;
	bool wasModified = m_voxelWorld->PaintWithSDF(
		brushSDF,
		m_brushMaterialID1,
		m_brushMaterialID2,
		m_brushBlendValue,
		dirtyRegion
	);

	delete brushSDF;

	if (wasModified)
	{
		m_chunkRenderManager->MarkDirtyRegion(dirtyRegion);
	}
}

void World::ApplyCarveBrush(Vec3 const& worldPos, Vec3 const& normal)
{
	if (!m_voxelWorld || !m_chunkRenderManager) return;

	UNUSED(normal);

	// Create brush SDF
	SDFSphere* brushSDF = new SDFSphere(m_brushRadius);
	brushSDF->GetMutTransform().m_position = worldPos; // Slight offset to carve inward

	IntBox3 dirtyRegion;
	bool wasModified = m_voxelWorld->CarveWithSDF(
		brushSDF,
		m_brushStrength,
		dirtyRegion
	);

	delete brushSDF;

	if (wasModified)
	{
		m_chunkRenderManager->MarkDirtyRegion(dirtyRegion);
	}
}

void World::ApplyFlattenBrush(Vec3 const& worldPos, Vec3 const& normal)
{
	// ToDo still use raycast result?

	// Flatten brush uses planar constraint (like Astroneer)
	// When mouse is first pressed, lock the plane
	// Then raycast against that plane while mouse is held

	// 2. Outer hemisphere (toward +normal) - Carve logic to flatten bumps
	if (!m_voxelWorld || !m_chunkRenderManager) return;

	//Voxel const& currentVoxel = m_raycastResult.m_voxel;
	//uint8_t matID1 = currentVoxel.m_materialID1;
	//uint8_t matID2 = currentVoxel.m_materialID2;
	//uint8_t blendValue = currentVoxel.m_blendValue;

	// New improved Flatten brush:
	// - Uses a spherical SDF to define the affected region
	// - Gradually modifies each voxel's density to match its signed distance to the plane
	// - Voxels above the plane (positive distance) -> air
	// - Voxels below the plane (negative distance) -> solid
	// - More precise and controllable than dual-hemisphere approach

	// Create spherical brush SDF to define the affected region
	SDFSphere* brushSDF = new SDFSphere(m_brushRadius);
	brushSDF->GetMutTransform().m_position = worldPos;

	// Create plane from the impact point and normal
	Plane3 targetPlane(normal, worldPos);

	// Apply the new FlattenToPlane operation
	IntBox3 dirtyRegion;
	bool wasModified = m_voxelWorld->FlattenToPlane(
		brushSDF,
		targetPlane,
		m_brushStrength,
		m_brushMaterialID1,
		m_brushMaterialID2,
		m_brushBlendValue,
		dirtyRegion
	);

	delete brushSDF;

	// Mark dirty region
	if (wasModified)
	{
		m_chunkRenderManager->MarkDirtyRegion(dirtyRegion);
	}
}

void World::ApplySmoothBrush(Vec3 const& worldPos, Vec3 const& normal)
{
	UNUSED(worldPos);
	UNUSED(normal);
}

void World::PickMaterial()
{
	if (!m_raycastResult.m_didImpact) return;

	Voxel const& pickedVoxel = m_raycastResult.m_voxel;
	m_brushMaterialID1 = pickedVoxel.m_materialID1;
	m_brushMaterialID2 = pickedVoxel.m_materialID2;
	m_brushBlendValue = pickedVoxel.m_blendValue;

	DebugAddMessage(Stringf("Picked material: %d/%d (blend: %d)", m_brushMaterialID1, m_brushMaterialID2, m_brushBlendValue), 2.f, Rgba8::CYAN);
}

void World::ProcessCompletedMeshJobs()
{
	if (!m_chunkRenderManager) return;

	// Retrieve completed jobs
	std::vector<Job*> completedJobs = g_theJobSystem->RetrieveCompletedJobs();

	for (Job* job : completedJobs)
	{
		GenerateMeshJob* meshJob = dynamic_cast<GenerateMeshJob*>(job);
		if (meshJob)
		{
			// Remove from pending jobs
			ChunkKey completedKey = meshJob->GetChunkKey();
			m_pendingMeshJobs.erase(completedKey);

			// Upload to GPU on main thread
			TerrainStructuredBuffer* buffer = meshJob->GetStructuredBuffer();
			if (buffer)
			{
				buffer->UploadToGPU();
			}

			// Clean up job
			delete job;
		}
	}
}

// #ToDo Racing condition, throw away new request?
void World::SubmitMeshGenerationJobs()
{
	if (!m_voxelWorld || !m_chunkRenderManager) return;

	// Check if we can submit more jobs
	int availableSlots = MAX_GENERATING_MESH_CHUNKS - (int)m_pendingMeshJobs.size();
	if (availableSlots <= 0) return;

	// Get dirty chunks from render manager
	std::vector<ChunkKey> dirtyChunks = m_chunkRenderManager->PopDirtyBatch(availableSlots);

	for (ChunkKey const& key : dirtyChunks)
	{
		// CRITICAL: Check if this chunk is already generating mesh
		// Avoid multi-threading conflict where two jobs write to the same buffer
		if (m_pendingMeshJobs.find(key) != m_pendingMeshJobs.end())
		{
			// This chunk is currently being processed by a worker thread
			// Re-mark it as dirty so it will be regenerated after current job completes
			m_chunkRenderManager->MarkDirty(key);
			continue;
		}

		// Extract voxels for this chunk
		std::vector<Voxel> voxels;
		voxels.reserve(TOTAL_VOXEL_BUFFER_SIZE);
		m_voxelWorld->ExtractVoxelsForMesh(key, voxels);

		// Get MeshChunk and its structured buffer
		MeshChunk* meshChunk = m_chunkRenderManager->GetOrCreateMeshChunk(key);
		if (!meshChunk || !meshChunk->m_structuredVertexBuffer)
		{
			continue; // Skip if mesh chunk or buffer doesn't exist
		}

		// Create and submit job
		GenerateMeshJob* meshJob = new GenerateMeshJob(
			key,
			std::move(voxels), // Move voxels to avoid copy
			meshChunk->m_structuredVertexBuffer, // Pass actual buffer owned by MeshChunk
			m_blockyBlendFactor,
			MeshingMode::DUAL_CONTOURING
		);

		g_theJobSystem->AddJob(meshJob);
		m_pendingMeshJobs.insert(key); // Add to set (not push_back)
	}
}

void World::ShowBrushControlWindow()
{
	ImGui::SetNextWindowSize(ImVec2(350, 450), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Brush Mode Controls", &m_showBrushControlWindow))
	{
		//ImGui::Text("Brush Mode Active");
		//ImGui::Separator();

		// Mode switching
		if (ImGui::Button("Switch to Editor Mode", ImVec2(-1, 30)))
		{
			SetNextMode(WorldMode::EDITOR);
		}
		if (ImGui::Button("Switch to Space Mode", ImVec2(-1, 30)))
		{
			SetNextMode(WorldMode::SPACE);
		}

		ImGui::Separator();
		ImGui::Text("Brush Controls");

		// Enable brush checkbox
		ImGui::Checkbox("Enable Brush", &m_enableBrush);

		if (m_enableBrush)
		{
			// Brush mode
			const char* brushModes[] = { "Add (1)", "Paint (2)", "Carve (3)", "Flatten (4)" };
			int currentMode = (int)m_currentBrushMode;
			if (ImGui::Combo("Brush Mode", &currentMode, brushModes, IM_ARRAYSIZE(brushModes)))
			{
				m_currentBrushMode = (BrushMode)currentMode;
			}

			// Brush parameters
			ImGui::SliderFloat("Brush Radius", &m_brushRadius, 1.0f, 20.0f);

			int strength = m_brushStrength;
			if (ImGui::SliderInt("Brush Strength", &strength, 1, 255))
			{
				m_brushStrength = (uint8_t)strength;
			}

			ImGui::Separator();
			ImGui::Text("Material Settings");

			// Material IDs
			int maxMatID = (int)GameMaterialDefinition::s_definitions.size() - 1;
			int matID1 = m_brushMaterialID1;
			int matID2 = m_brushMaterialID2;
			int blendValue = m_brushBlendValue;

			std::string mat1Name = GameMaterialDefinition::s_definitions[matID1]->m_name;
			std::string mat2Name = GameMaterialDefinition::s_definitions[matID2]->m_name;

			// Material ID 1
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Material 1:");
			ImGui::SameLine(100);
			ImGui::TextColored(ImVec4(0.5f, 0.9f, 1.0f, 1.0f), "%s", mat1Name.c_str());
			ImGui::SetNextItemWidth(-1);
			if (ImGui::SliderInt("##MaterialID1", &matID1, 0, maxMatID, "ID: %d"))
			{
				m_brushMaterialID1 = (uint8_t)matID1;
			}

			ImGui::Spacing();

			// Material ID 2
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Material 2:");
			ImGui::SameLine(100);
			ImGui::TextColored(ImVec4(0.5f, 0.9f, 1.0f, 1.0f), "%s", mat2Name.c_str());
			ImGui::SetNextItemWidth(-1);
			if (ImGui::SliderInt("##MaterialID2", &matID2, 0, maxMatID, "ID: %d"))
			{
				m_brushMaterialID2 = (uint8_t)matID2;
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Blend Value
			ImGui::AlignTextToFramePadding();
			ImGui::Text("Blend:");
			ImGui::SameLine(100);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::SliderInt("##BlendValue", &blendValue, 0, 255, "%d"))
			{
				m_brushBlendValue = (uint8_t)blendValue;
			}

			ImGui::Separator();
			ImGui::Text("Instructions:");
			ImGui::BulletText("LMB: Apply brush");
			ImGui::BulletText("MMB: Pick material");
			ImGui::BulletText("1/2/3/4: Switch brush mode");
		}

		ImGui::Separator();
		ImGui::Text("Raycast Info");
		if (m_raycastResult.m_didImpact)
		{
			ImGui::Text("Hit: Yes");
			ImGui::Text("Pos: (%.1f, %.1f, %.1f)",
				m_raycastResult.m_impactPos.x,
				m_raycastResult.m_impactPos.y,
				m_raycastResult.m_impactPos.z);
			ImGui::Text("Normal: (%.2f, %.2f, %.2f)",
				m_raycastResult.m_impactNormal.x,
				m_raycastResult.m_impactNormal.y,
				m_raycastResult.m_impactNormal.z);
			ImGui::Text("Voxel Density: %d", m_raycastResult.m_voxel.m_density);
		}
		else
		{
			ImGui::TextDisabled("Hit: No");
		}

		ImGui::Separator();
		ImGui::Text("Job System");
		ImGui::Text("Pending Jobs: %d / %d", (int)m_pendingMeshJobs.size(), MAX_GENERATING_MESH_CHUNKS);

		if (m_chunkRenderManager)
		{
			ImGui::Separator();
			bool wireframeMode = m_chunkRenderManager->IsWireFrameMode();
			if (ImGui::Checkbox("Wireframe Mode", &wireframeMode)) 
			{
				m_chunkRenderManager->SetWireFrameMode(wireframeMode);
			}
		}
	}
	ImGui::End();
}

void World::RenderRaycastImpact(VoxelRaycastResult3D const& raycastResult, float innerRadius, float thickness, float arrowLength) const
{
	if (!raycastResult.m_didImpact) return;

	//Mat44 transform = Mat44::MakeFromX(raycastResult.m_impactNormal);

	std::vector<Vertex_PCU> verts;
	AddVertsForRing3D(verts, raycastResult.m_impactPos, raycastResult.m_impactPos + raycastResult.m_impactNormal * thickness, innerRadius, thickness, Rgba8::OPAQUE_WHITE, AABB2::ZERO_TO_ONE, 16);

	AddVertsForArrow3D(verts, raycastResult.m_impactPos, raycastResult.m_impactPos + raycastResult.m_impactNormal * arrowLength, 0.5f * thickness, Rgba8::OPAQUE_WHITE, 8);

	DebugAddWorldTriangleList(verts, 0.f, Rgba8::CYAN, Rgba8::CYAN, DebugRenderMode::X_RAY);
}

void World::RenderGridSea() const
{
	g_theRenderer->SetModelConstants();

	std::vector<Vertex_PCU> verts;

	float minX = 0.f;
	float minY = 0.f;
	float minZ = 0.f;
	float maxX = (float)WORLD_SIZE;
	float maxY = (float)WORLD_SIZE;
	//float maxZ = (float)WORLD_SIZE;

	// p-max n-min
	Vec3 nnn(minX, minY, minZ);
	Vec3 npn(minX, maxY, minZ);
	Vec3 pnn(maxX, minY, minZ);
	Vec3 ppn(maxX, maxY, minZ);


	AddVertsForQuad3D(verts, ppn, npn, nnn, pnn); // -z

	Texture* gridTex = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/grid_sea.png");

	// resource settings
	UnlitRenderResources resources;
	resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(gridTex, DefaultTexture::WhiteOpaque2D);
	resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(UnlitRenderResources), &resources);

	g_theRenderer->BindShader(nullptr);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

	g_theRenderer->DrawVertexArray(verts);
}

void World::Update()
{
	SwitchToNextMode();
	HandleModeTransitionInput();

	UpdateCurrentMode();

	DebugDrawLODChunks();
}

void World::Render() const
{
	Camera const& camera = g_theGame->GetCamera();
	g_theRenderer->BeginCamera(camera);
	//RenderCurrentMode();
	RenderOpaquePass();
	RenderAdditivePass();
	g_theRenderer->EndCamera(camera);

	m_useSceneAsSource = true; // Reset this bool before post processing
	RenderBloomPass();


	CopyToBackBuffer();


}

void World::RenderUI() const
{
	RenderCurrentModeUI();
}

CursorMode World::GetCursorMode() const
{
	if (m_currentMode == WorldMode::BRUSH || m_currentMode == WorldMode::EDITOR)
	{
		bool isRMBPressed = g_theInput->IsKeyDown(KEYCODE_RIGHT_MOUSE);
		CursorMode cursorMode = isRMBPressed ? CursorMode::FPS : CursorMode::POINTER;
		return cursorMode;
	}
	else if (m_currentMode == WorldMode::SPACE)
	{
		return CursorMode::FPS;
	}

	return CursorMode::POINTER;
}

void World::OnWindowResized()
{
	CreateHDRRenderTargets();

	m_bloomEffect->OnResize(g_windowWidth, g_windowHeight);
}

void World::UpdateSpectatorCamera()
{
	float deltaSeconds = static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds()); // Unscaled DeltaSeconds

	Camera& camera = g_theGame->GetCamera();

	Vec3 targetPosition = camera.GetPosition();
	EulerAngles targetOrientation = camera.GetOrientation();

	XboxController const& controller = g_theInput->GetController(0);

	// Update Orientation
	{
		Vec2 cursorPositionDelta = g_theInput->GetCursorClientDelta(); // Only in FPS cursor mode
		float deltaYaw = -cursorPositionDelta.x * 0.125f;
		float deltaPitch = cursorPositionDelta.y * 0.125f;

		Vec2 rightStick = controller.GetRightStick().GetPosition();
		deltaYaw -= deltaSeconds * EDITOR_CAMERA_YAW_TURN_RATE * rightStick.x;
		deltaPitch -= deltaSeconds * EDITOR_CAMERA_PITCH_TURN_RATE * rightStick.y;

		targetOrientation.m_yawDegrees += deltaYaw;
		targetOrientation.m_pitchDegrees += deltaPitch;
		targetOrientation.m_pitchDegrees = GetClamped(targetOrientation.m_pitchDegrees, -EDITOR_CAMERA_MAX_PITCH, EDITOR_CAMERA_MAX_PITCH);
	}

	// Update Position
	{
		float const speedMultiplier = (g_theInput->IsKeyDown(KEYCODE_SHIFT) || controller.GetLeftTrigger() > 0.f || controller.GetRightTrigger() > 0.f) ? 
			EDITOR_CAMERA_SPEED_FACTOR : 1.f;

		Vec3 moveIntention = Vec3(controller.GetLeftStick().GetPosition().GetRotatedMinus90Degrees());

		if (g_theInput->IsKeyDown(KEYCODE_W))
		{
			moveIntention += Vec3(1.f, 0.f, 0.f);
		}
		if (g_theInput->IsKeyDown(KEYCODE_S))
		{
			moveIntention += Vec3(-1.f, 0.f, 0.f);
		}
		if (g_theInput->IsKeyDown(KEYCODE_A))
		{
			moveIntention += Vec3(0.f, 1.f, 0.f);
		}
		if (g_theInput->IsKeyDown(KEYCODE_D))
		{
			moveIntention += Vec3(0.f, -1.f, 0.f);
		}

		moveIntention.ClampLength(1.f);

		Vec3 forwardIBasis, leftJBasis, upKBasis;
		targetOrientation.GetAsVectors_IFwd_JLeft_KUp(forwardIBasis, leftJBasis, upKBasis);
		targetPosition += (forwardIBasis * moveIntention.x + leftJBasis * moveIntention.y + upKBasis * moveIntention.z) *
			EDITOR_CAMERA_MOVE_SPEED * deltaSeconds * speedMultiplier;

		Vec3 elevateIntention;
		if (g_theInput->IsKeyDown(KEYCODE_Q))
		{
			elevateIntention += Vec3(0.f, 0.f, -1.f);
		}
		if (g_theInput->IsKeyDown(KEYCODE_E))
		{
			elevateIntention += Vec3(0.f, 0.f, 1.f);
		}
		if (controller.IsButtonDown(XboxButtonId::XBOX_BUTTON_LEFT_SHOULDER))
		{
			elevateIntention += Vec3(0.f, 0.f, -1.f);
		}
		if (controller.IsButtonDown(XboxButtonId::XBOX_BUTTON_RIGHT_SHOULDER))
		{
			elevateIntention += Vec3(0.f, 0.f, 1.f);
		}
		elevateIntention.ClampLength(1.f);

		targetPosition += elevateIntention * EDITOR_CAMERA_MOVE_SPEED * deltaSeconds * speedMultiplier;
	}

	// Update Camera
	camera.SetPositionAndOrientation(targetPosition, targetOrientation);
}

void World::SetNextMode(WorldMode newMode)
{
	m_nextMode = newMode;
}

void World::SwitchToNextMode()
{
	if (m_currentMode != m_nextMode)
	{
		if (m_currentMode == WorldMode::EDITOR && m_nextMode == WorldMode::BRUSH)
		{
			SwitchToBrushFromEditor();
		}
		else if (m_currentMode == WorldMode::BRUSH && m_nextMode == WorldMode::SPACE)
		{
			SwitchToSpaceFromBrush();
		}
		else if (m_currentMode == WorldMode::SPACE && m_nextMode == WorldMode::BRUSH)
		{
			SwitchToBrushFromSpace();
		}
		else if (m_currentMode == WorldMode::BRUSH && m_nextMode == WorldMode::EDITOR)
		{
			SwitchToEditorFromBrush();
		}
		else
		{
			ERROR_AND_DIE("Mode Transition Not Defined!");
		}
		m_currentMode = m_nextMode;
	}
}

void World::SwitchToBrushFromEditor()
{
	// Create voxel world and render manager
	m_voxelWorld = new VoxelWorld();
	m_chunkRenderManager = new ChunkRenderManager();

	// Bake all SDF shapes to voxel world
	BakeSDFShapesToVoxelWorld();

	g_theJobSystem->StartAcceptingJobs();

	DebugAddMessage(Stringf("Brush Mode ready. %d shapes baked.", (int)m_sdfShapes.size()), 3.f, Rgba8::GREEN);
}

void World::SwitchToEditorFromBrush()
{
	// Flush all pending jobs
	FlushJobSystemAndRetrieveJobs();
	m_pendingMeshJobs.clear();

	// Destroy voxel world and render manager
	delete m_voxelWorld;
	m_voxelWorld = nullptr;

	delete m_chunkRenderManager;
	m_chunkRenderManager = nullptr;

	DebugAddMessage("Editor Mode ready.", 3.f, Rgba8::GREEN);
}

void World::SwitchToSpaceFromBrush()
{
	ShipSpawnInfo spawnInfo;
	spawnInfo.m_shipName = "X-Wing";
	spawnInfo.m_worldPosition = Vec3(256.f, 256.f, 256.f);
	m_player = new PlayerShip(this, spawnInfo);

	DebugAddMessage("Space Mode ready.", 3.f, Rgba8::GREEN);
}

void World::SwitchToBrushFromSpace()
{
	delete m_player;
	m_player = nullptr;

	// Reset Camera Roll
	Camera& camera = g_theGame->GetCamera();
	EulerAngles cameraRotation = camera.GetOrientation();
	cameraRotation.m_rollDegrees = 0.f;
	camera.SetOrientation(cameraRotation);
	camera.SetPerspectiveView(Window::s_mainWindow->GetAspectRatio(), GAME_CAMERA_FOV, GAME_CAMERA_NEAR, GAME_CAMERA_FAR);

	DebugAddMessage("Brush Mode ready.", 3.f, Rgba8::GREEN);
}

void World::BakeSDFShapesToVoxelWorld()
{
	if (!m_voxelWorld || !m_chunkRenderManager) return;

	for (SDFShape const* shape : m_sdfShapes)
	{
		if (!shape) continue;

		IntBox3 dirtyRegion;
		bool wasModified = m_voxelWorld->BakeSDFShape(*shape, dirtyRegion);

		if (wasModified)
		{
			m_chunkRenderManager->MarkDirtyRegion(dirtyRegion);
		}
	}
}

void World::UpdateCurrentMode()
{
	switch (m_currentMode)
	{
	case WorldMode::EDITOR:
		UpdateEditorMode();
		break;
	case WorldMode::BRUSH:
		UpdateBrushMode();
		break;
	case WorldMode::SPACE:
		UpdateSpaceMode();
		break;
	}
}

void World::RenderCurrentMode() const
{
	switch (m_currentMode)
	{
	case WorldMode::EDITOR:
		RenderEditorMode();
		break;
	case WorldMode::BRUSH:
		RenderBrushMode();
		break;
	case WorldMode::SPACE:
		RenderSpaceMode();
		break;
	}
}

void World::RenderCurrentModeUI() const
{
	switch (m_currentMode)
	{
	case WorldMode::EDITOR:
		break;
	case WorldMode::BRUSH:
		break;
	case WorldMode::SPACE:
		RenderSpaceModeUI();
		break;
	}
}

void World::UpdateEditorMode()
{
	UpdateSpectatorCamera();
	UpdateEditorInput();
	UpdateEditorImGui();
}

void World::RenderEditorMode() const
{
	RenderEditorShapes();
	RenderWorldGrid();
}

void World::UpdateBrushMode()
{
	UpdateSpectatorCamera();

	// 1. Update raycast for brush visualization
	UpdateRay();

	// 2. Handle user input and brush operations
	UpdateBrushInput();

	// 3. Update ImGui controls
	UpdateBrushImGui();

	// 4. Process completed mesh generation jobs
	ProcessCompletedMeshJobs();

	// 5. Submit new mesh generation jobs
	SubmitMeshGenerationJobs();
}

void World::RenderBrushMode() const
{
	if (!m_voxelWorld || !m_chunkRenderManager) return;

	RenderSkybox();

	// Get camera information for frustum culling and LOD selection
	Vec3 refPosition = g_theGame->GetLODReferencePosition();
	Frustum frustum = g_theGame->GetCullingFrustum();

	// Get visible chunks with LOD selection
	std::vector<ChunkKey> visibleKeys = GetVisibleChunksWithLOD(refPosition, frustum);

	// Render all visible chunks
	m_chunkRenderManager->Render(visibleKeys);

	RenderGridSea();
	RenderRaycastImpact(m_raycastResult, m_brushRadius, 0.2f, 1.f);

}

void World::UpdateSpaceMode()
{
	m_player->Update();

	m_player->UpdateCamera(g_theGame->GetCamera());

	m_projectileSystem->Update(g_theGame->GetDeltaSeconds(), this);

	// 4. Process completed mesh generation jobs
	ProcessCompletedMeshJobs();

	// 5. Submit new mesh generation jobs
	SubmitMeshGenerationJobs();
}

void World::RenderSpaceMode() const
{
	if (!m_voxelWorld || !m_chunkRenderManager) return;

	RenderSkybox();

	// Get camera information for frustum culling and LOD selection
	Vec3 refPosition = g_theGame->GetLODReferencePosition();
	Frustum frustum = g_theGame->GetCullingFrustum();

	// Get visible chunks with LOD selection
	std::vector<ChunkKey> visibleKeys = GetVisibleChunksWithLOD(refPosition, frustum);

	// Render all visible chunks
	m_chunkRenderManager->Render(visibleKeys);

	m_player->Render();
}

void World::RenderSpaceModeUI() const
{
	if (m_player)
	{
		m_player->RenderUI();
	}
}

void World::HandleModeTransitionInput()
{
	XboxController const& controller = g_theInput->GetController(0);

	// N key or START button: Move forward (right) through modes
	// Editor -> Brush -> Space
	if (g_theInput->WasKeyJustPressed(KEYCODE_N) ||
		controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_START))
	{
		switch (m_currentMode)
		{
		case WorldMode::EDITOR:
			SetNextMode(WorldMode::BRUSH);
			DebugAddMessage("Switching to Brush Mode", 2.f, Rgba8::CYAN);
			break;
		case WorldMode::BRUSH:
			SetNextMode(WorldMode::SPACE);
			DebugAddMessage("Switching to Space Mode", 2.f, Rgba8::CYAN);
			break;
		case WorldMode::SPACE:
			// Do nothing
			DebugAddMessage("Already in Space Mode", 1.f, Rgba8::YELLOW);
			break;
		}
	}
	// ESC key or BACK button: Move backward (left) through modes
	// Space -> Brush -> Editor (then quit)
	else if (g_theInput->WasKeyJustPressed(KEYCODE_ESCAPE) ||
		controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_BACK))
	{
		switch (m_currentMode)
		{
		case WorldMode::EDITOR:
			// At the leftmost mode, ESC/BACK quits the application
			g_theApp->HandleQuitRequested();
			break;
		case WorldMode::BRUSH:
			SetNextMode(WorldMode::EDITOR);
			DebugAddMessage("Returning to Editor Mode", 2.f, Rgba8::CYAN);
			break;
		case WorldMode::SPACE:
			SetNextMode(WorldMode::BRUSH);
			DebugAddMessage("Returning to Brush Mode", 2.f, Rgba8::CYAN);
			break;
		}
	}
}

void World::UpdateEditorInput()
{
	// Some Hot Keys
	if (g_theInput->WasKeyJustPressed(KEYCODE_DELETE) && m_selectedShapeIndex >= 0)
	{
		RemoveShape(m_selectedShapeIndex);
	}

}

void World::UpdateEditorImGui()
{
	if (m_showEditorWindow)
	{
		ShowEditorWindow();
	}

	if (m_showShapeListWindow)
	{
		ShowShapeListWindow();
	}

	if (m_showPropertiesWindow)
	{
		ShowShapePropertiesWindow();
	}
}

void World::RenderEditorShapes() const
{
	// Debug draw all SDF shapes
	//for (SDFShape const* shape : m_sdfShapes)
	//{
	//	if (shape)
	//	{
	//		shape->DebugDrawShape();
	//	}
	//}

	std::vector<Vertex_PCU> worldVerts;
	worldVerts.reserve(100);

	for (SDFShape const* shape : m_sdfShapes)
	{
		if (shape)
		{
			shape->AddWorldVerts(worldVerts);
		}
	}

	g_theRenderer->SetModelConstants();

	DiffuseRenderResources resources; // 
	resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(nullptr);
	resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_WARP);
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();
	resources.lightConstantsIndex = g_theRenderer->GetCurrentLightConstantsIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(DiffuseRenderResources), &resources);

	// pipeline settings 
	g_theRenderer->BindShader(m_diffuseShader);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

	g_theRenderer->DrawVertexArray(worldVerts);

}

void World::AddShape(SDFShape* shape)
{
	if (shape)
	{
		m_sdfShapes.push_back(shape);
		SelectShape((int)m_sdfShapes.size() - 1);
	}
}

void World::RemoveShape(int index)
{
	if (index >= 0 && index < (int)m_sdfShapes.size())
	{
		delete m_sdfShapes[index];
		m_sdfShapes.erase(m_sdfShapes.begin() + index);

		// Adjust selection
		if (m_selectedShapeIndex >= (int)m_sdfShapes.size())
		{
			m_selectedShapeIndex = (int)m_sdfShapes.size() - 1;
		}
	}
}

void World::ClearAllShapes()
{
	for (SDFShape* shape : m_sdfShapes)
	{
		delete shape;
	}
	m_sdfShapes.clear();
	m_selectedShapeIndex = -1;
}

SDFShape* World::GetShape(int index)
{
	if (index >= 0 && index < (int)m_sdfShapes.size())
	{
		return m_sdfShapes[index];
	}
	return nullptr;
}

SDFShape const* World::GetShape(int index) const
{
	if (index >= 0 && index < (int)m_sdfShapes.size())
	{
		return m_sdfShapes[index];
	}
	return nullptr;
}

void World::SelectShape(int index)
{
	// Deselect old shape
	if (m_selectedShapeIndex >= 0 && m_selectedShapeIndex < (int)m_sdfShapes.size())
	{
		m_sdfShapes[m_selectedShapeIndex]->m_isSelected = false;
	}

	// Select new shape
	m_selectedShapeIndex = index;
	if (m_selectedShapeIndex >= 0 && m_selectedShapeIndex < (int)m_sdfShapes.size())
	{
		m_sdfShapes[m_selectedShapeIndex]->m_isSelected = true;
	}
}

void World::DeselectAll()
{
	if (m_selectedShapeIndex >= 0 && m_selectedShapeIndex < (int)m_sdfShapes.size())
	{
		m_sdfShapes[m_selectedShapeIndex]->m_isSelected = false;
	}
	m_selectedShapeIndex = -1;
}

SDFShape* World::GetSelectedShape()
{
	if (m_selectedShapeIndex >= 0 && m_selectedShapeIndex < (int)m_sdfShapes.size())
	{
		return m_sdfShapes[m_selectedShapeIndex];
	}
	return nullptr;
}

SDFShape const* World::GetSelectedShape() const
{
	if (m_selectedShapeIndex >= 0 && m_selectedShapeIndex < (int)m_sdfShapes.size())
	{
		return m_sdfShapes[m_selectedShapeIndex];
	}
	return nullptr;
}

void World::ShowEditorWindow()
{
	ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
	//if (ImGui::Begin("SDF Shape Editor", &m_showEditorWindow))
	if (ImGui::Begin("SDF Shape Editor"))
	{
		// Window toggles
		ImGui::Checkbox("Show Shape List", &m_showShapeListWindow);
		ImGui::Checkbox("Show Properties", &m_showPropertiesWindow);
		ImGui::Separator();


		ImGui::Text("Editor Mode");
		ImGui::Separator();

		// Mode switching
		if (ImGui::Button("Switch to Brush Mode", ImVec2(-1, 30)))
		{
			SetNextMode(WorldMode::BRUSH);
		}

		ImGui::Separator();
		ImGui::Text("Add New Shape");

		// Add shape buttons
		if (ImGui::Button("Add Sphere", ImVec2(-1, 0)))
		{
			SDFSphere* sphere = new SDFSphere(5.0f);
			sphere->GetMutTransform().m_position = Vec3(256.f, 256.f, 256.f);
			SDFShape* shape = new SDFShape(sphere);
			//shape->m_materialID1 = 0;
			//shape->m_materialID2 = 1;
			AddShape(shape);
		}

		if (ImGui::Button("Add Box", ImVec2(-1, 0)))
		{
			SDFBox* box = new SDFBox(Vec3(5.f, 5.f, 5.f));
			box->GetMutTransform().m_position = Vec3(256.f, 256.f, 256.f);
			SDFShape* shape = new SDFShape(box);

			AddShape(shape);
		}

		if (ImGui::Button("Add Torus", ImVec2(-1, 0)))
		{
			SDFTorus* torus = new SDFTorus(10.f, 3.f);
			torus->GetMutTransform().m_position = Vec3(256.f, 256.f, 256.f);
			SDFShape* shape = new SDFShape(torus);

			AddShape(shape);
		}

		if (ImGui::Button("Add Capsule", ImVec2(-1, 0)))
		{
			SDFCapsule* capsule = new SDFCapsule(Vec3(0.f, 0.f, -4.f), Vec3(0.f, 0.f, 4.f), 2.f);
			capsule->GetMutTransform().m_position = Vec3(256.f, 256.f, 256.f);
			SDFShape* shape = new SDFShape(capsule);

			AddShape(shape);
		}

		if (ImGui::Button("Add CutSphere", ImVec2(-1, 0)))
		{
			SDFCutSphere* cutSphere = new SDFCutSphere(5.f, -2.f);
			cutSphere->GetMutTransform().m_position = Vec3(256.f, 256.f, 256.f);
			SDFShape* shape = new SDFShape(cutSphere);

			AddShape(shape);
		}

		if (ImGui::Button("Add CutHollowSphere", ImVec2(-1, 0)))
		{
			SDFCutHollowSphere* cutHollowSphere = new SDFCutHollowSphere(5.f, 2.f, 1.f);
			cutHollowSphere->GetMutTransform().m_position = Vec3(256.f, 256.f, 256.f);
			SDFShape* shape = new SDFShape(cutHollowSphere);

			AddShape(shape);
		}

		if (ImGui::Button("Add Density Cloud", ImVec2(-1, 0)))
		{
			m_densityCloudFileSelector.Refresh(); // Refresh before open popup
			ImGui::OpenPopup("Select Density Cloud File");
		}

		// Density Cloud file selector popup
		if (ImGui::BeginPopupModal("Select Density Cloud File", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Select a .dcloud file to add:");
			ImGui::Separator();

			m_densityCloudFileSelector.Render();

			ImGui::Separator();

			// Add button (only enabled if a file is selected)
			ImGui::BeginDisabled(!m_densityCloudFileSelector.HasSelection());
			if (ImGui::Button("Add", ImVec2(120, 0)))
			{
				auto selectedPath = m_densityCloudFileSelector.GetSelectedFilePath();
				if (selectedPath.has_value())
				{
					SDFDensityCloud* densityCloud = new SDFDensityCloud();
					densityCloud->LoadFromFile(selectedPath.value());
					densityCloud->GetMutTransform().m_position = Vec3(256.f, 256.f, 256.f);
					SDFShape* shape = new SDFShape(densityCloud);
					//shape->m_materialID1 = 0;
					//shape->m_materialID2 = 1;
					AddShape(shape);

					DebugAddMessage(Stringf("Density Cloud added from: %s", selectedPath.value().c_str()), 3.f, Rgba8::GREEN);
					//m_showDensityCloudSelector = false;
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndDisabled();

			ImGui::SameLine();

			// Cancel button
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				//m_showDensityCloudSelector = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		ImGui::Separator();

		// Scene Save/Load
		ImGui::Text("Scene Management");


		// Save button
		//char saveFileName[128] = "NewScene.xml";
		static char s_saveFileName[128] = "NewScene.xml";
		ImGui::PushItemWidth(-80);
		ImGui::InputText("##SaveFileName", s_saveFileName, IM_ARRAYSIZE(s_saveFileName));
		ImGui::PopItemWidth();

		ImGui::SameLine();
		if (ImGui::Button("Save", ImVec2(70, 0)))
		{
			std::string savePath = std::string("Data/Scenes/") + s_saveFileName;
			SaveSceneToXml(savePath.c_str());
			m_sceneFileSelector.Refresh(); // Refresh after save
			DebugAddMessage(Stringf("Scene saved to: %s", savePath.c_str()), 3.f);
		}

		// Load button

		if (ImGui::Button("Load Scene", ImVec2(-1, 0)))
		{
			m_sceneFileSelector.Refresh();  // Refresh before open popup
			ImGui::OpenPopup("Load Scene File");
		}

		// Load Scene popup
		if (ImGui::BeginPopupModal("Load Scene File", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Select a scene file to load:");
			ImGui::Separator();

			m_sceneFileSelector.Render();

			ImGui::Separator();

			// Load button (only enabled if a file is selected)
			ImGui::BeginDisabled(!m_sceneFileSelector.HasSelection());
			if (ImGui::Button("Load", ImVec2(120, 0)))
			{
				auto selectedPath = m_sceneFileSelector.GetSelectedFilePath();
				if (selectedPath.has_value())
				{
					LoadSceneFromXml(selectedPath.value().c_str());

					std::filesystem::path filePath(selectedPath.value());
					std::string fileName = filePath.filename().string();

					strcpy_s(s_saveFileName, IM_ARRAYSIZE(s_saveFileName), fileName.c_str());

					DebugAddMessage(Stringf("Scene loaded from: %s", selectedPath.value().c_str()), 3.f, Rgba8::GREEN);
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndDisabled();

			ImGui::SameLine();

			// Cancel button
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}



		/*m_sceneFileSelector.Render();
		ImGui::BeginDisabled(!m_sceneFileSelector.HasSelection());
		if (ImGui::Button("Load Selected", ImVec2(-1, 0)))
		{
			auto selectedPath = m_sceneFileSelector.GetSelectedFilePath();
			if (selectedPath.has_value())
			{
				LoadSceneFromXml(selectedPath.value().c_str());
				DebugAddMessage(Stringf("Scene loaded from: %s", selectedPath.value().c_str()), 3.f);
			}
		}
		ImGui::EndDisabled();*/

		ImGui::Separator();

		// Clear all shapes button
		if (ImGui::Button("Clear All Shapes", ImVec2(-1, 0)))
		{
			ImGui::OpenPopup("Confirm Clear");
		}

		// Confirm dialog (always check, will only render when open)
		if (ImGui::BeginPopupModal("Confirm Clear", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Are you sure you want to clear all shapes?");
			ImGui::Text("This action cannot be undone.");
			ImGui::Separator();

			// Yes button with default focus
			if (ImGui::Button("Yes", ImVec2(120, 0)))
			{
				ClearAllShapes();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();

			ImGui::SameLine();

			// No button
			if (ImGui::Button("No", ImVec2(120, 0)))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}


	}
	ImGui::End();
}

void World::ShowShapeListWindow()
{
	ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Shape List", &m_showShapeListWindow))
	{
		ImGui::Text("Total Shapes: %d", GetShapeCount());
		ImGui::Separator();

		for (int i = 0; i < GetShapeCount(); ++i)
		{
			SDFShape* shape = GetShape(i);
			if (!shape || !shape->m_sdf) continue;

			ImGui::PushID(i);

			// Selection highlight
			bool isSelected = (i == m_selectedShapeIndex);
			if (isSelected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
			}

			// Shape button // Why Button?
			std::string label = Stringf("%d: %s", i, shape->m_sdf->GetShapeName());
			if (ImGui::Button(label.c_str(), ImVec2(-50, 0)))
			{
				SelectShape(i);
			}

			if (isSelected)
			{
				ImGui::PopStyleColor();
			}

			// Delete button
			ImGui::SameLine();
			if (ImGui::Button("X", ImVec2(40, 0)))
			{
				RemoveShape(i);
				ImGui::PopID();
				break; // Exit loop since we modified the vector
			}

			ImGui::PopID();
		}
	}
	ImGui::End();
}

void World::ShowShapePropertiesWindow()
{
	ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Shape Properties", &m_showPropertiesWindow))
	{
		SDFShape* selectedShape = GetSelectedShape();

		if (selectedShape && selectedShape->m_sdf)
		{
			ImGui::Text("Editing Shape: %s", selectedShape->m_sdf->GetShapeName());
			ImGui::Separator();

			// SDF Transform and Parameters
			DrawSDFDetail(selectedShape->m_sdf);

			ImGui::Separator();

			// Fill Method
			if (ImGui::CollapsingHeader("Material Fill Method", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const char* fillMethods[] = { "Uniform Blend", "Z-Axis Gradient", "Distance Gradient" };
				int currentMethod = (int)selectedShape->m_fillMethod;
				if (ImGui::Combo("Fill Method", &currentMethod, fillMethods, IM_ARRAYSIZE(fillMethods)))
				{
					selectedShape->m_fillMethod = (SDFFillMethod)currentMethod;
				}

				// Material IDs
				int maxMatID = (int)GameMaterialDefinition::s_definitions.size() - 1;
				int matID1 = selectedShape->m_materialID1;
				int matID2 = selectedShape->m_materialID2;

				std::string mat1Name = GameMaterialDefinition::s_definitions[matID1]->m_name;
				std::string mat2Name = GameMaterialDefinition::s_definitions[matID2]->m_name;

				// Material ID 1
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Material 1:");
				ImGui::SameLine(100);
				ImGui::TextColored(ImVec4(0.5f, 0.9f, 1.0f, 1.0f), "%s", mat1Name.c_str());
				ImGui::SetNextItemWidth(-1);
				if (ImGui::SliderInt("##MaterialID1", &matID1, 0, maxMatID, "ID: %d"))
				{
					selectedShape->m_materialID1 = (uint8_t)matID1;
				}

				ImGui::Spacing();

				// Material ID 2
				ImGui::AlignTextToFramePadding();
				ImGui::Text("Material 2:");
				ImGui::SameLine(100);
				ImGui::TextColored(ImVec4(0.5f, 0.9f, 1.0f, 1.0f), "%s", mat2Name.c_str());
				ImGui::SetNextItemWidth(-1);
				if (ImGui::SliderInt("##MaterialID2", &matID2, 0, maxMatID, "ID: %d"))
				{
					selectedShape->m_materialID2 = (uint8_t)matID2;
				}

				ImGui::Spacing();

				//if (ImGui::SliderInt("Material ID 1", &matID1, 0, maxMatID))
				//{
				//	selectedShape->m_materialID1 = (uint8_t)matID1;
				//}

				//if (ImGui::SliderInt("Material ID 2", &matID2, 0, maxMatID))
				//{
				//	selectedShape->m_materialID2 = (uint8_t)matID2;
				//}

				// Fill method specific parameters
				switch (selectedShape->m_fillMethod)
				{
				case SDFFillMethod::UNIFORM_BLEND:
					ImGui::SliderFloat("Blend Value", &selectedShape->m_uniformBlendValue, 0.0f, 1.0f);
					break;

				case SDFFillMethod::Z_AXIS_GRADIENT:
					ImGui::SliderFloat("Min Z Factor", &selectedShape->m_gradientMinZFactor, 0.0f, 1.0f);
					ImGui::SliderFloat("Max Z Factor", &selectedShape->m_gradientMaxZFactor, 0.0f, 1.0f);
					break;

				case SDFFillMethod::DISTANCE_GRADIENT:
					ImGui::DragFloat("Min Distance", &selectedShape->m_gradientMinDist, 0.1f, -10.0f, 0.0f);
					ImGui::DragFloat("Max Distance", &selectedShape->m_gradientMaxDist, 0.1f, -10.0f, 0.0f);
					break;
				}
			}
		}
		else
		{
			ImGui::TextDisabled("No shape selected");
			ImGui::Text("Select a shape from the Shape List to edit its properties.");
		}
	}
	ImGui::End();
}

void World::SaveSceneToXml(const char* filepath)
{
	XmlDocument doc;

	XmlElement* root = doc.NewElement("SDFScene");
	doc.InsertFirstChild(root);

	// Save each shape
	for (SDFShape const* shape : m_sdfShapes)
	{
		if (shape)
		{
			XmlElement* shapeElem = doc.NewElement("SDFShape");
			shape->WriteToXml(*shapeElem);
			root->InsertEndChild(shapeElem);
		}
	}

	// Save to file
	XmlResult result = doc.SaveFile(filepath);
	if (result != tinyxml2::XML_SUCCESS)
	{
		DebugAddMessage(Stringf("Failed to save scene to: %s", filepath), 5.f, Rgba8::RED);
	}
}

void World::LoadSceneFromXml(const char* filepath)
{
	XmlDocument doc;
	XmlResult result = doc.LoadFile(filepath);

	if (result != tinyxml2::XML_SUCCESS)
	{
		DebugAddMessage(Stringf("Failed to load scene from: %s", filepath), 5.f, Rgba8::RED);
		return;
	}

	XmlElement* root = doc.RootElement();
	if (!root)
	{
		DebugAddMessage("Invalid scene file: No root element", 5.f, Rgba8::RED);
		return;
	}

	// Clear existing shapes
	ClearAllShapes();

	// Load shapes
	XmlElement const* shapeElem = root->FirstChildElement("SDFShape");
	while (shapeElem)
	{
		SDFShape* shape = SDFShape::CreateFromXml(*shapeElem);
		if (shape)
		{
			m_sdfShapes.push_back(shape);
		}
		shapeElem = shapeElem->NextSiblingElement("SDFShape");
	}

	DebugAddMessage(Stringf("Loaded %d shapes from scene", (int)m_sdfShapes.size()), 3.f, Rgba8::GREEN);

}

void World::RenderWorldGrid() const
{
	g_theRenderer->SetModelConstants();

	std::vector<Vertex_PCU> verts;

	float minX = 0.f;
	float minY = 0.f;
	float minZ = 0.f;
	float maxX = (float)WORLD_SIZE;
	float maxY = (float)WORLD_SIZE;
	float maxZ = (float)WORLD_SIZE;

	// p-max n-min
	Vec3 nnn(minX, minY, minZ);
	Vec3 nnp(minX, minY, maxZ);
	Vec3 npn(minX, maxY, minZ);
	Vec3 npp(minX, maxY, maxZ);
	Vec3 pnn(maxX, minY, minZ);
	Vec3 pnp(maxX, minY, maxZ);
	Vec3 ppn(maxX, maxY, minZ);
	Vec3 ppp(maxX, maxY, maxZ);

	constexpr float TILE_SIZE = static_cast<float>(WORLD_SIZE) / 32.f;
	maxX /= TILE_SIZE;
	maxY /= TILE_SIZE;
	maxZ /= TILE_SIZE;


	// Make them face inside => a special effect
	// use world pos as uv
	AddVertsForQuad3D(verts, ppn, pnn, pnp, ppp, Rgba8(255, 128, 128), AABB2(minY, minZ, maxY, maxZ)); // +x
	AddVertsForQuad3D(verts, nnn, npn, npp, nnp, Rgba8(0,	128, 128), AABB2(-maxY, minZ, -minY, maxZ)); // -x or maxY, minZ, minY, maxZ
	AddVertsForQuad3D(verts, npn, ppn, ppp, npp, Rgba8(128, 255, 128), AABB2(-maxX, minZ, -minX, maxZ)); // +y
	AddVertsForQuad3D(verts, pnn, nnn, nnp, pnp, Rgba8(128, 0,   128), AABB2(minX, minZ, maxX, maxZ)); // -y
	AddVertsForQuad3D(verts, pnp, nnp, npp, ppp, Rgba8(128, 128, 255), AABB2(minX, minY, maxX, maxY)); // +z
	AddVertsForQuad3D(verts, ppn, npn, nnn, pnn, Rgba8(128, 128, 0  ), AABB2(minX, -maxY, maxX, -minY)); // -z

	Texture* gridTex = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/32x32.dds");

	// resource settings
	UnlitRenderResources resources;
	resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(gridTex, DefaultTexture::WhiteOpaque2D);
	resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_WARP);
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(UnlitRenderResources), &resources);

	Shader* gridShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/UnlitGrid"));
	g_theRenderer->BindShader(gridShader);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

	g_theRenderer->DrawVertexArray(verts);
}

void World::UpdateRay()
{
	m_raycastResult = {};

	Vec3 rayStart;
	Vec3 rayFwdNormal;
	float rayLength;

	bool hasRay = g_theGame->GetScreenRay(rayStart, rayFwdNormal, rayLength);
	if (hasRay && m_voxelWorld)
	{
		m_raycastResult = m_voxelWorld->FastVoxelRaycast(rayStart, rayFwdNormal, rayLength);
	}
}

void World::UpdateBrushInput()
{
	if (!m_enableBrush || !m_voxelWorld) return;

	// Left mouse button - apply brush
	if (g_theInput->IsKeyDown(KEYCODE_LEFT_MOUSE))
	{
		HandleBrushOperation();
	}

	// Middle mouse button - pick material
	if (g_theInput->WasKeyJustPressed(KEYCODE_MIDDLE_MOUSE))
	{
		PickMaterial();
	}

	// Hot keys for brush mode switching
	if (g_theInput->WasKeyJustPressed('1'))
	{
		m_currentBrushMode = BrushMode::ADD;
		DebugAddMessage("Brush Mode: ADD", 1.f, Rgba8::GREEN);
	}
	if (g_theInput->WasKeyJustPressed('2'))
	{
		m_currentBrushMode = BrushMode::PAINT;
		DebugAddMessage("Brush Mode: PAINT", 1.f, Rgba8::YELLOW);
	}
	if (g_theInput->WasKeyJustPressed('3'))
	{
		m_currentBrushMode = BrushMode::CARVE;
		DebugAddMessage("Brush Mode: CARVE", 1.f, Rgba8::RED);
	}
	if (g_theInput->WasKeyJustPressed('4'))
	{
		m_currentBrushMode = BrushMode::FLATTEN;
		DebugAddMessage("Brush Mode: FLATTEN", 1.f, Rgba8::CYAN);
	}
}

void World::UpdateBrushImGui()
{
	if (m_showBrushControlWindow)
	{
		ShowBrushControlWindow();
	}
}

void World::HandleBrushOperation()
{
	// Special handling for FLATTEN mode with planar constraint
	if (m_currentBrushMode == BrushMode::FLATTEN)
	{
		// On first press, lock the plane at impact point
		if (g_theInput->WasKeyJustPressed(KEYCODE_LEFT_MOUSE))
		{
			if (m_raycastResult.m_didImpact)
			{
				// Lock the plane using impact point and normal
				m_isFlattenPlaneActive = true;
				m_flattenPlane = Plane3(m_raycastResult.m_impactNormal, m_raycastResult.m_impactPos);
			}
		}

		// While mouse is held, raycast against locked plane
		if (g_theInput->IsKeyDown(KEYCODE_LEFT_MOUSE) && m_isFlattenPlaneActive)
		{
			RaycastResult3D planeRaycast = {};

			// Raycast against the locked plane
			Vec3 rayStart;
			Vec3 rayFwdNormal;
			float rayLength;

			bool hasRay = g_theGame->GetScreenRay(rayStart, rayFwdNormal, rayLength);

			if (hasRay)
			{
				planeRaycast = RaycastVsPlane3D(
					rayStart,
					rayFwdNormal,
					rayLength,
					m_flattenPlane
				);
			}

			if (planeRaycast.m_didImpact)
			{
				// Apply flatten brush at plane intersection point
				// Use the locked plane normal (not the surface normal)
				ApplyFlattenBrush(planeRaycast.m_impactPos, m_flattenPlane.m_normal);
			}
		}

		// When mouse is released, unlock the plane
		if (g_theInput->WasKeyJustReleased(KEYCODE_LEFT_MOUSE))
		{
			m_isFlattenPlaneActive = false;
		}

		return; // Skip normal brush handling for FLATTEN mode
	}

	// Normal brush handling for other modes






	//-----------------------------------------------------------------------------------------------
	if (!m_raycastResult.m_didImpact) return;

	Vec3 hitPos = m_raycastResult.m_impactPos;
	Vec3 hitNormal = m_raycastResult.m_impactNormal;

	switch (m_currentBrushMode)
	{
	case BrushMode::ADD:
		ApplyAddBrush(hitPos, hitNormal);
		break;
	case BrushMode::PAINT:
		ApplyPaintBrush(hitPos);
		break;
	case BrushMode::CARVE:
		ApplyCarveBrush(hitPos, hitNormal);
		break;
	case BrushMode::FLATTEN:
		// Handled above
		break;
	}
}

void World::DebugDrawLODChunks()
{
	if (!g_isDebugDraw)
	{
		return;
	}

	Frustum frustum = g_theGame->GetCullingFrustum();

	Vec3 refPosition = g_theGame->GetLODReferencePosition();
	//std::vector<ChunkKey> keys;
	//keys.reserve(20);
	//TraverseOctreeForLOD(IntBox3(IntVec3(), IntVec3(WORLD_SIZE, WORLD_SIZE, WORLD_SIZE)), playerCamPosition, frustum, keys);

	std::vector<ChunkKey> keys = GetVisibleChunksWithLOD(refPosition, frustum);

	Rgba8 lodColors[NUM_LOD_LEVELS] = { Rgba8(102, 194, 165), Rgba8(252, 141, 98), Rgba8(231, 138, 195) };
	
	for (ChunkKey const& key : keys)
	{
		IntBox3 nodeBox = CoordsUtils::GetChunkBounds(key.m_chunkCoords, key.m_lodLevel);

		AABB3 box(
			Vec3((float)nodeBox.m_mins.x, (float)nodeBox.m_mins.y, (float)nodeBox.m_mins.z),
			Vec3((float)(nodeBox.m_mins.x + nodeBox.m_dimensions.x),
				(float)(nodeBox.m_mins.y + nodeBox.m_dimensions.y),
				(float)(nodeBox.m_mins.z + nodeBox.m_dimensions.z))
		);

		std::vector<Vertex_PCU> verts;
		verts.reserve(36);
		float minX = box.m_mins.x;
		float minY = box.m_mins.y;
		float minZ = box.m_mins.z;
		float maxX = box.m_maxs.x;
		float maxY = box.m_maxs.y;
		float maxZ = box.m_maxs.z;

		// p-max n-min
		Vec3 nnn(minX, minY, minZ);
		Vec3 nnp(minX, minY, maxZ);
		Vec3 npn(minX, maxY, minZ);
		Vec3 npp(minX, maxY, maxZ);
		Vec3 pnn(maxX, minY, minZ);
		Vec3 pnp(maxX, minY, maxZ);
		Vec3 ppn(maxX, maxY, minZ);
		Vec3 ppp(maxX, maxY, maxZ);


		bool isEven = (key.m_chunkCoords.x + key.m_chunkCoords.y + key.m_chunkCoords.z) % 2 == 0;

		Rgba8 xColor = lodColors[key.m_lodLevel];
		xColor.ScaleRGB(isEven ? 0.9f : 0.85f);
		Rgba8 yColor = lodColors[key.m_lodLevel];
		yColor.ScaleRGB(isEven ? 0.8f : 0.75f);
		Rgba8 zColor = lodColors[key.m_lodLevel];
		zColor.ScaleRGB(isEven ? 1.f : 0.9f);


		AddVertsForQuad3D(verts, pnn, ppn, ppp, pnp, xColor); // +x
		AddVertsForQuad3D(verts, npn, nnn, nnp, npp, xColor); // -x
		AddVertsForQuad3D(verts, ppn, npn, npp, ppp, yColor); // +y
		AddVertsForQuad3D(verts, nnn, pnn, pnp, nnp, yColor); // -y
		AddVertsForQuad3D(verts, nnp, pnp, ppp, npp, zColor); // +z
		AddVertsForQuad3D(verts, npn, ppn, pnn, nnn, zColor); // -z

		//DebugAddWorldTriangleList(verts, 0.f);
		DebugAddWorldWireTriangleListNoneCull(verts, 0.f);
	}

}

void World::GenerateMesh()
{
	IntGrid3 chunk(m_chunkSize[0], m_chunkSize[1], m_chunkSize[2]);

	Vec3 localChunkCenter(chunk.GetSizeX() * 0.5f, chunk.GetSizeY() * 0.5f, chunk.GetSizeZ() * 0.5f);

	int cellCount = chunk.GetCellCount();
	std::vector<Voxel> voxels(cellCount);


	m_debugVertices.clear();
	m_debugIndices.clear();
	m_debugVertices.reserve(cellCount * 24);
	m_debugIndices.reserve(cellCount * 36);


	for (int i = 0; i < cellCount; ++i)
	{
		Vec3 localVoxelCenterPos = Vec3(chunk.Delinearize(i)) + Vec3(0.5f, 0.5f, 0.5f);

		Vec3 p = localVoxelCenterPos - localChunkCenter;
		float distance = 0.f;

		distance = SdfHelper::Sphere(p, m_sdSphereRadius);
		
		voxels[i].m_density = Voxel::GetUint8DensityFromSignedDistance(distance);
		if (localVoxelCenterPos.x < localChunkCenter.x)
		{
			voxels[i].m_materialID1 = m_westMatID1;
			voxels[i].m_materialID2 = m_westMatID2;
			voxels[i].m_blendValue = m_westBlend;
		}
		else
		{
			voxels[i].m_materialID1 = m_eastMatID1;
			voxels[i].m_materialID2 = m_eastMatID2;
			voxels[i].m_blendValue = m_eastBlend;
		}


		AABB3 debugBox = AABB3(localVoxelCenterPos - Vec3(m_debugBoxHalfSize, m_debugBoxHalfSize, m_debugBoxHalfSize),
			localVoxelCenterPos + Vec3(m_debugBoxHalfSize, m_debugBoxHalfSize, m_debugBoxHalfSize));

		if (voxels[i].IsEmpty()) // empty
		{
			AddVertsForAABB3D(m_debugVertices, m_debugIndices, debugBox, Rgba8::RED);
		}
		else
		{
			AddVertsForAABB3D(m_debugVertices, m_debugIndices, debugBox, Rgba8::GREEN);
		}


	}

	// On worker thread
	VoxelMesher mesher(MeshingMode::DUAL_CONTOURING);

	// 1. Common
	//std::vector<Vertex_Terrain> vertices;
	//std::vector<unsigned int> indices;
	//mesher.GenerateMesh(voxels, chunk, vertices, indices, m_blockyBlendFactor);
	//// On main thread
	//g_theRenderer->CopyCPUToGPU(vertices.data(), static_cast<unsigned int>(vertices.size()) * m_vertexBuffer->GetStride(), m_vertexBuffer);
	//g_theRenderer->CopyCPUToGPU(indices.data(), static_cast<unsigned int>(indices.size()) * m_indexBuffer->GetStride(), m_indexBuffer);

	// 2. Experimental
	mesher.GenerateMesh(voxels, chunk, m_structuredVertexBuffer, m_blockyBlendFactor);
	// On main thread
	m_structuredVertexBuffer->UploadToGPU();

}

void World::RenderMesh() const
{
	Mat44 transform;
	g_theRenderer->SetModelConstants(transform);



	//TerrainRenderResource res;

	//res.engineConstantsIndex = g_theRenderer->GetCurrentEngineConstantsIndex();
	//res.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	//res.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();
	//res.lightConstantsIndex = g_theRenderer->GetCurrentLightConstantsIndex();
	//res.perFrameConstantsIndex = g_theRenderer->GetCurrentPerFrameConstantsIndex();

	//res.materialBufferIndex = GameMaterialDefinition::GetMaterialBufferIndex();

	//g_theRenderer->SetGraphicsBindlessResources(sizeof(TerrainRenderResource), &res);

	//g_theRenderer->BindShader(m_shader);
	//g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	//g_theRenderer->SetRasterizerMode(m_isWireframeMode ? RasterizerMode::WIREFRAME_CULL_NONE : RasterizerMode::SOLID_CULL_BACK);
	//g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	//g_theRenderer->SetRenderTargetFormats();

	//g_theRenderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, m_indexBuffer->GetCount());

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

	g_theRenderer->BindShader(m_experimentalShader);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(m_isWireframeMode ? RasterizerMode::WIREFRAME_CULL_NONE : RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

	g_theRenderer->DrawProcedural(m_structuredVertexBuffer->GetVertexNum());




	//if (m_isDebugNormal)
	//{
	//	UnlitRenderResources resources;
	//	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	//	resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();
	//	g_theRenderer->SetGraphicsBindlessResources(sizeof(UnlitRenderResources), &resources);

	//	g_theRenderer->BindShader(m_debugNormalShader);
	//	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	//	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	//	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	//	g_theRenderer->SetRenderTargetFormats();
	//	g_theRenderer->DrawIndexedVertexBuffer(m_surfaceNets->m_vertexBuffer, m_surfaceNets->m_indexBuffer, m_surfaceNets->m_indexBuffer->GetCount());
	//}


	if (g_isDebugDraw)
	{
		g_theRenderer->SetModelConstants(Mat44::MakeTranslation3D(Vec3(-1.f, -1.f, -1.f)));

		// resource settings
		UnlitRenderResources resources;
		resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(nullptr, DefaultTexture::WhiteOpaque2D);
		resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);
		resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
		resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();

		g_theRenderer->SetGraphicsBindlessResources(sizeof(UnlitRenderResources), &resources);

		g_theRenderer->BindShader(nullptr);
		g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
		g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

		g_theRenderer->DrawIndexedVertexArray(m_debugVertices, m_debugIndices);
	}
}

void World::ShowWorldImGuiWindow()
{
	static SDFSphere testing;
	if (ImGui::Begin("Dual Contouring")) 
	{
		DrawSDFDetail(&testing);

		ImGui::Text("Grid Size:");

		float itemWidth = 80.0f;

		ImGui::SetNextItemWidth(itemWidth);
		ImGui::InputInt("X", &m_chunkSize[0]);
		if (m_chunkSize[0] < 1) m_chunkSize[0] = 1;

		ImGui::SameLine();

		ImGui::SetNextItemWidth(itemWidth);
		ImGui::InputInt("Y", &m_chunkSize[1]);
		if (m_chunkSize[1] < 1) m_chunkSize[1] = 1;

		ImGui::SameLine();

		ImGui::SetNextItemWidth(itemWidth);
		ImGui::InputInt("Z", &m_chunkSize[2]);
		if (m_chunkSize[2] < 1) m_chunkSize[2] = 1;

		ImGui::SliderFloat("blockyBlendFactor", &m_blockyBlendFactor, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat("SdSphere Radius", &m_sdSphereRadius, 0.05f);

		//const char* items[] = { "Sphere", "Box", "Perlin Noise"};
		//ImGui::Combo("Sdf Type", &m_currentSdfType, items, IM_ARRAYSIZE(items));

		if (ImGui::Button("Regenerate")) 
		{
			Regenerate();
		}

		ImGui::Checkbox("Wireframe Mode", &m_isWireframeMode);


		int westMatID1 = m_westMatID1;
		int westMatID2 = m_westMatID2;
		int westBlend = m_westBlend;

		int eastMatID1 = m_eastMatID1;
		int eastMatID2 = m_eastMatID2;
		int eastBlend = m_eastBlend;

		int const maxMatID = (int)GameMaterialDefinition::s_definitions.size() - 1;

		if (ImGui::SliderInt("westMatID1", &westMatID1, 0, maxMatID))
		{
			m_westMatID1 = static_cast<uint8_t>(westMatID1);
		}
		if (ImGui::SliderInt("westMatID2", &westMatID2, 0, maxMatID))
		{
			m_westMatID2 = static_cast<uint8_t>(westMatID2);
		}
		if (ImGui::SliderInt("westBlend", &westBlend, 0, 255))
		{
			m_westBlend = static_cast<uint8_t>(westBlend);
		}

		if (ImGui::SliderInt("eastMatID1", &eastMatID1, 0, maxMatID))
		{
			m_eastMatID1 = static_cast<uint8_t>(eastMatID1);
		}
		if (ImGui::SliderInt("eastMatID2", &eastMatID2, 0, maxMatID))
		{
			m_eastMatID2 = static_cast<uint8_t>(eastMatID2);
		}
		if (ImGui::SliderInt("eastBlend", &eastBlend, 0, 255))
		{
			m_eastBlend = static_cast<uint8_t>(eastBlend);
		}



	}

	ImGui::End();
}

void World::Regenerate()
{
	GenerateMesh();

}

void World::ResetPlayerShipMouseRotationInput()
{
	if (m_player)
	{
		m_player->ResetMouseRotationInput();
	}
}

VoxelRaycastResult3D World::DoProjectileTrace(Vec3 rayStart, Vec3 rayForwardNormal, float rayLength) const
{
	if (m_voxelWorld)
	{
		return m_voxelWorld->FastVoxelRaycast(rayStart, rayForwardNormal, rayLength);
	}

	return { };
}

void World::ApplyExplosion(Vec3 const& worldPos, float explosionRadius, uint8_t deltaDensity)
{
	if (!m_voxelWorld || !m_chunkRenderManager) return;

	// Create brush SDF
	SDFSphere* brushSDF = new SDFSphere(explosionRadius);
	brushSDF->GetMutTransform().m_position = worldPos;

	IntBox3 dirtyRegion;
	bool wasModified = m_voxelWorld->CarveWithSDF(
		brushSDF,
		deltaDensity,
		dirtyRegion
	);

	delete brushSDF;

	if (wasModified)
	{
		m_chunkRenderManager->MarkDirtyRegion(dirtyRegion);
	}
}

void World::InitializeSkybox()
{
	//-----------------------------------------------------------------------------------------------
	TextureCubeSixFacesConfig config;
	config.m_name = "Skybox";
	config.m_rightImageFilePath = "Data/Images/Skybox/px.png";
	config.m_leftImageFilePath = "Data/Images/Skybox/nx.png";
	config.m_upImageFilePath = "Data/Images/Skybox/py.png";
	config.m_downImageFilePath = "Data/Images/Skybox/ny.png";
	config.m_forwardImageFilePath = "Data/Images/Skybox/pz.png";
	config.m_backwardImageFilePath = "Data/Images/Skybox/nz.png";

	m_skyTexture = g_theRenderer->CreateOrGetTextureCubeFromSixFaces(config);
	m_skyShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/Sky"), VertexType::VERTEX_PCU);
}

void World::RenderSkybox() const
{
	std::vector<Vertex_PCU> verts;
	AddVertsForAABB3D(verts, AABB3(Vec3(-1.f, -1.f, -1.f), Vec3(1.f, 1.f, 1.f)));

	g_theRenderer->SetModelConstants();


	SkyboxRenderResources resources;
	resources.cubeMapTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(m_skyTexture);
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(SkyboxRenderResources), &resources);

	g_theRenderer->BindShader(m_skyShader);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->DrawVertexArray(verts);
}

void World::RenderPassStartup()
{
	m_bloomEffect = new BloomEffect();
	m_bloomEffect->Initialize();

	m_bloomEffect->SetBloomIntensity(2.0f);
	m_bloomEffect->SetBloomThreshold(1.0f);

	CreateHDRRenderTargets();

	m_copyToBackBufferShader = g_theRenderer->CreateOrGetShader(ShaderConfig("Data/Shaders/FullScreenQuad"), VertexType::VERTEX_NONE);
}

void World::RenderPassShutdown()
{
	ReleaseHDRRenderTargets();

	delete m_bloomEffect;
	m_bloomEffect = nullptr;
}

void World::RenderOpaquePass() const
{
	g_theRenderer->TransitionToRenderTarget(*m_sceneTexture);
	g_theRenderer->ClearRenderTargetByIndex(m_sceneRTV.m_index, Rgba8(128, 128, 128));

	g_theRenderer->SetRenderTargetFormats({m_hdrFormat}); // Use Default Depth Format
	g_theRenderer->SetRenderTargetsByIndex({ m_sceneRTV.m_index }, g_theRenderer->GetDefaultDepthBufferIndex());

	RenderCurrentMode();
}

void World::RenderAdditivePass() const
{
	if (m_currentMode == WorldMode::SPACE)
	{
		m_projectileSystem->Render();
		m_player->RenderAdditive();
	}
}

void World::RenderBloomPass() const
{
	g_theRenderer->TransitionToAllShaderResource(*(m_useSceneAsSource ? m_sceneTexture : m_postProcessTexture));
	g_theRenderer->TransitionToUnorderedAccess(*(m_useSceneAsSource ? m_postProcessTexture : m_sceneTexture));

	DescriptorHandle emissiveTextureSRV = m_useSceneAsSource ? m_sceneSRV : m_postProcessSRV;
	DescriptorHandle sceneTextureSRV = m_useSceneAsSource ? m_sceneSRV : m_postProcessSRV;
	DescriptorHandle outputTextureUAV = m_useSceneAsSource ? m_postProcessUAV : m_sceneUAV;

	m_bloomEffect->Execute(emissiveTextureSRV, sceneTextureSRV, outputTextureUAV);

	m_useSceneAsSource = !m_useSceneAsSource; // Flip
}

void World::CopyToBackBuffer() const
{
	// Remember to reset to default rt and ds
	// if has custom ds, remember to render full screen quad with depth (overwrite), or just keep use the new one
	//g_theRenderer->SetRenderTargetsByIndex({g_theRenderer->GetCurrentBackBufferIndex()}, m_sceneDepthDSV.m_index);
	//g_theRenderer->SetRenderTargetFormats({ DXGI_FORMAT_R8G8B8A8_UNORM }, m_sceneDepthFormat);


	g_theRenderer->SetDefaultRenderTargets();
	g_theRenderer->SetRenderTargetFormats();

	g_theRenderer->BindShader(m_copyToBackBufferShader); // if want to overwrite depth, use FullScreenQuadWithDepth.hlsl
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
	g_theRenderer->SetDepthMode(DepthMode::DISABLED); // if want to overwrite depth, change this

	g_theRenderer->TransitionToPixelShaderResource(*(m_useSceneAsSource ? m_sceneTexture : m_postProcessTexture));
	DescriptorHandle inputSRV = m_useSceneAsSource ? m_sceneSRV : m_postProcessSRV;

	FullScreenQuadResources resources;
	resources.textureIndex = inputSRV.m_index;
	resources.samplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);

	g_theRenderer->SetGraphicsBindlessResources(sizeof(FullScreenQuadResources), &resources);

	g_theRenderer->DrawProcedural(6);
}

void World::CreateHDRRenderTargets()
{
	ReleaseHDRRenderTargets();

	//-----------------------------------------------------------------------------------------------
	TextureInit sceneInit;
	sceneInit.m_width = g_windowWidth;
	sceneInit.m_height = g_windowHeight;
	sceneInit.m_format = m_hdrFormat;
	sceneInit.m_allowSRV = true;
	sceneInit.m_allowRTV = true;
	sceneInit.m_allowUAV = true;
	sceneInit.m_debugName = L"Game_SceneRT";
	sceneInit.m_rtvClearFormat = m_hdrFormat;
	Rgba8(128, 128, 128).GetAsFloats(sceneInit.m_rtvClearColor);

	m_sceneTexture = g_theRenderer->CreateTexture(sceneInit);
	m_sceneRTV = g_theRenderer->AllocateRTV(*m_sceneTexture);
	m_sceneSRV = g_theRenderer->AllocateSRV(*m_sceneTexture);
	m_sceneUAV = g_theRenderer->AllocateUAV(*m_sceneTexture);

	//-----------------------------------------------------------------------------------------------
	TextureInit finalInit;
	finalInit.m_width = g_windowWidth;
	finalInit.m_height = g_windowHeight;
	finalInit.m_format = m_hdrFormat;
	finalInit.m_allowSRV = true;
	finalInit.m_allowUAV = true;
	finalInit.m_debugName = L"Game_FinalRT";

	m_postProcessTexture = g_theRenderer->CreateTexture(finalInit);
	m_postProcessSRV = g_theRenderer->AllocateSRV(*m_postProcessTexture);
	m_postProcessUAV = g_theRenderer->AllocateUAV(*m_postProcessTexture);
}

void World::ReleaseHDRRenderTargets()
{
	g_theRenderer->DestroyTexture(m_sceneTexture);
	g_theRenderer->EnqueueDeferredRelease(m_sceneRTV);
	g_theRenderer->EnqueueDeferredRelease(m_sceneSRV);
	g_theRenderer->EnqueueDeferredRelease(m_sceneUAV);

	g_theRenderer->DestroyTexture(m_postProcessTexture);
	g_theRenderer->EnqueueDeferredRelease(m_postProcessSRV);
	g_theRenderer->EnqueueDeferredRelease(m_postProcessUAV);
}

void World::ShowBloomSettings()
{
	if (!m_bloomEffect) return;

	// Bloom Intensity
	float intensity = m_bloomEffect->GetBloomIntensity();
	if (ImGui::SliderFloat("Bloom Intensity", &intensity, 0.0f, 5.0f, "%.2f"))
	{
		m_bloomEffect->SetBloomIntensity(intensity);
	}

	// Bloom Threshold
	float threshold = m_bloomEffect->GetBloomThreshold();
	if (ImGui::SliderFloat("Bloom Threshold", &threshold, 0.0f, 5.0f, "%.2f"))
	{
		m_bloomEffect->SetBloomThreshold(threshold);
	}
}
