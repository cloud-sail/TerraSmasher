#pragma once
#include "Game/GameCommon.hpp"
#include "Game/WorldCommon.hpp"
#include "Game/VoxelWorld.hpp"
#include "Game/ImGuiUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Game/GameTransform.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Math/Plane3.hpp"
#include "Engine/Math/OBB3.hpp"
#include "Engine/Math/AABB3.hpp"
#include <vector>
#include <unordered_set>

class SDFShape;
class SDF;
class ChunkRenderManager;
class VoxelWorld;
class PlayerShip;
class BloomEffect;
class ProjectileSystem;
class ExplosionSystem;
struct ExplosionDefinition;
class DebrisSystem;
struct DebrisDefinition;
class CollectibleDefinition;
class ComboSystem;
class ScoreSystem;
struct StrikeContext;
struct StrikeResult;
//-----------------------------------------------------------------------------------------------
struct CollectibleInstance
{
	CollectibleDefinition const* m_definition = nullptr;
	GameTransform m_transform;

	// Space Mode runtime state
	bool m_isCollected = false;
	AABB3 m_worldAABB;
	OBB3 m_worldOBB;

	// Sonar highlight
	float m_sonarHighlightTimer = 0.f;
	static constexpr float SONAR_HIGHLIGHT_DURATION = 3.f;

	float GetSonarHighlightAlpha() const;
	Vec3 GetWorldCenter() const;

	void ReadFromXml(XmlElement const& element);
	void WriteToXml(XmlElement& element) const;
	void Render(Shader* blinnPhongShader) const;
	void RenderWithSonar(Shader* collectibleShader, uint32_t sceneDepthSRVIndex, float sonarAlpha) const;
};
//-----------------------------------------------------------------------------------------------
// EDITOR <-> BRUSH <-> SPACE

enum class WorldMode
{
	EDITOR,
	BRUSH,
	SPACE
};


struct SonarParams
{
	Vec3 m_sonarCenter;
	float m_sonarInnerRadius = 0.f;
	float m_sonarThickness = 1.f;
	float m_sonarColor[4] = {};
};


//-----------------------------------------------------------------------------------------------
class World
{
public:
	~World();
	World();

	void Update();
	void Render() const;
	void RenderUI() const;

	CursorMode GetCursorMode() const;

	void OnWindowResized();

private:
	void UpdateSpectatorCamera();


#pragma region ModeMangement
private:
	WorldMode GetCurrentMode() const { return m_currentMode; }

	void SetNextMode(WorldMode newMode);

	void SwitchToNextMode();
	void SwitchToBrushFromEditor();
	void SwitchToEditorFromBrush();
	void SwitchToSpaceFromBrush();
	void SwitchToBrushFromSpace();

	void UpdateCurrentMode();
	void RenderCurrentMode() const;
	void RenderCurrentModeUI() const;

	void UpdateEditorMode();
	void RenderEditorMode() const;

	void UpdateBrushMode();
	void RenderBrushMode() const;

	void UpdateSpaceMode();
	void RenderSpaceMode() const;
	void RenderSpaceModeUI() const;

	// Mode transition input handling
	void HandleModeTransitionInput();

private:
	WorldMode m_currentMode = WorldMode::EDITOR;
	WorldMode m_nextMode = WorldMode::EDITOR;

#pragma endregion

#pragma region Editor
private:
	void UpdateEditorInput();
	void UpdateEditorImGui();
	void RenderEditorShapes() const;

	void AddShape(SDFShape* shape);
	void RemoveShape(int index);
	void ClearAllShapes();
	int GetShapeCount() const { return static_cast<int>(m_sdfShapes.size()); }
	SDFShape* GetShape(int index);
	SDFShape const* GetShape(int index) const;

	void SelectShape(int index);
	void DeselectAll();
	int GetSelectedShapeIndex() const { return m_selectedShapeIndex; }
	SDFShape* GetSelectedShape();
	SDFShape const* GetSelectedShape() const;

	void ShowEditorWindow();
	void ShowShapeListWindow();
	void ShowShapePropertiesWindow();

	// Collectible editor
	void ShowCollectibleListWindow();
	void ShowCollectiblePropertiesWindow();
	void RenderCollectibles() const;			// Editor mode: plain BlinnPhong
	void RenderCollectiblesSpaceMode() const;	// Space mode: with sonar + depth test

	// Scene Save/Load
	void SaveSceneToXml(const char* filepath);
	void LoadSceneFromXml(const char* filepath);

	void RenderWorldGrid() const;

	void ClearAllCollectibles();

private:
	std::vector<SDFShape*> m_sdfShapes;
	int m_selectedShapeIndex = -1;

	// Collectibles
	std::vector<CollectibleInstance*> m_collectibles;
	int m_selectedCollectibleIndex = -1;

	// Editor UI State
	bool m_showEditorWindow = true;
	bool m_showShapeListWindow = true;
	bool m_showPropertiesWindow = true;
	bool m_showCollectibleListWindow = true;
	bool m_showCollectiblePropertiesWindow = true;

	// Scene file selector
	GameFileSelector m_sceneFileSelector;
	GameFileSelector m_densityCloudFileSelector;

	Shader* m_blinnPhongShader = nullptr;
	Shader* m_collectibleShader = nullptr;

#pragma endregion

#pragma region Brush
private:
	void UpdateRay();
	void UpdateBrushInput();
	void UpdateBrushImGui();


	void HandleBrushOperation();

	void ApplyAddBrush(Vec3 const& worldPos, Vec3 const& normal);
	void ApplyPaintBrush(Vec3 const& worldPos);
	void ApplyCarveBrush(Vec3 const& worldPos, Vec3 const& normal);
	void ApplyFlattenBrush(Vec3 const& worldPos, Vec3 const& normal);
	void ApplySmoothBrush(Vec3 const& worldPos, Vec3 const& normal);
	void PickMaterial(); // Color picker

	void ProcessCompletedMeshJobs();
	void SubmitMeshGenerationJobs();

	void ShowBrushControlWindow();

	void RenderRaycastImpact(VoxelRaycastResult3D const& raycastResult, float innerRadius, float thickness, float arrowLength) const;
	void RenderGridSea() const;

private:
	VoxelWorld* m_voxelWorld = nullptr;
	ChunkRenderManager* m_chunkRenderManager = nullptr;

	//std::vector<ChunkKey> m_pendingMeshJobs; // limit the maximum running jobs
	std::unordered_set<ChunkKey, ChunkKeyHash> m_pendingMeshJobs; // Track chunks currently generating mesh to avoid multi-threading conflicts

	VoxelRaycastResult3D m_raycastResult; // Add, Paint, Carve mode

	// Brush settings
	enum class BrushMode
	{
		ADD,
		PAINT,
		CARVE,
		FLATTEN,
		SMOOTH
	};

	BrushMode m_currentBrushMode = BrushMode::ADD;
	float m_brushRadius = 3.0f;
	uint8_t m_brushStrength = 5; // Currently, it is related to the frame rate, 60 * strength = 1s deltaDensity
	uint8_t m_brushMaterialID1 = 0;
	uint8_t m_brushMaterialID2 = 1;
	uint8_t m_brushBlendValue = 128;

	// Flatten brush state (planar constraint)
	bool m_isFlattenPlaneActive = false;
	Plane3 m_flattenPlane;

	// Brush mode UI
	bool m_showBrushControlWindow = true;
	bool m_enableBrush = true;

#pragma endregion

#pragma region EditorToBrush
private:
	void BakeSDFShapesToVoxelWorld();
#pragma endregion

private:
	void FlushJobSystemAndRetrieveJobs();



public:
	int m_totalNumVertices = 0;
	int m_totalNumIndices = 0;


private:
	// lod color six face different 
	void DebugDrawLODChunks();


#pragma region MesherTest
private:
	void GenerateMesh();
	void RenderMesh() const;

private:
	//NaiveSurfaceNets* m_surfaceNets = nullptr;
	//DualContouring1* m_surfaceNets = nullptr;
	//DualContouring2* m_surfaceNets = nullptr;
	//Shader* m_debugNormalShader = nullptr;

	Shader* m_shader = nullptr;
	Shader* m_experimentalShader = nullptr;

private:
	void ShowWorldImGuiWindow();

	void Regenerate();
private:
	int m_chunkSize[3] = { 17, 17, 17 };
	float m_blockyBlendFactor = 0.f;
	float m_sdSphereRadius = 5.f;
	bool m_isWireframeMode = false;

	uint8_t m_westMatID1 = 4;
	uint8_t m_westMatID2 = 5;
	uint8_t m_westBlend = 200;

	uint8_t m_eastMatID1 = 5;
	uint8_t m_eastMatID2 = 4;
	uint8_t m_eastBlend = 200;

	//bool m_isDebugNormal = false;

	//int m_copySize[3] = { 1, 1, 1 };

	//int m_currentSdfType = 0;

private:
	float m_debugBoxHalfSize = 0.07f;
	std::vector<Vertex_PCU> m_debugVertices;
	std::vector<unsigned int> m_debugIndices;
	VertexBuffer* m_debugVertexBuffer = nullptr;
	IndexBuffer* m_debugIndexBuffer = nullptr;

	// Common
	VertexBuffer* m_vertexBuffer = nullptr;
	IndexBuffer* m_indexBuffer = nullptr;

	// Experimental
	TerrainStructuredBuffer* m_structuredVertexBuffer = nullptr;
#pragma endregion

#pragma region Space
public:
	void ResetPlayerShipMouseRotationInput();
	VoxelRaycastResult3D DoProjectileTrace(Vec3 rayStart, Vec3 rayForwardNormal, float rayLength) const;
	VoxelRaycastResult3D DoShipTrace(Vec3 rayStart, Vec3 rayForwardNormal, float rayLength) const;

	StrikeResult ApplyStrike(Vec3 const& worldPos, float strikeRadius, StrikeContext const& ctx);
	void SpawnExplosionVFX(ExplosionDefinition const& def);
	void SpawnDebrisVFX(DebrisDefinition const& def);

	void ResetMaterialDestroyedVolumes();

	// Collectible collection system
	void RegisterCollectibles();
	void CheckCollectibleCollection(AABB3 const& dirtyAABB);
	void OnCollectibleCollected(CollectibleInstance* item);
	void UpdateCollectibleSonarHighlights(float deltaSeconds);
	void RenderCollectibleUI() const;

public:
	ProjectileSystem* m_projectileSystem = nullptr;
	ExplosionSystem* m_explosionSystem = nullptr;
	DebrisSystem* m_debrisSystem = nullptr;
private:
	PlayerShip* m_player = nullptr;
	std::vector<float> m_materialDestroyedVolumes;

	// Combo & scoring (owned). ScoreSystem holds a non-owning pointer to ComboSystem.
	ComboSystem* m_comboSystem = nullptr;
	ScoreSystem* m_scoreSystem = nullptr;

	void RenderScoreAndComboUI() const;

	// Collectible collection UI
	CollectibleInstance* m_lastCollectedItem = nullptr;
	float m_collectUITimer = 0.f;
	mutable float m_collectUIRotation = 0.f;
	std::vector<CollectibleInstance*> m_collectedItems;
	static constexpr float COLLECT_UI_DURATION = 4.0f;
	static constexpr float COLLECT_UI_ROTATION_SPEED = 60.f;

#pragma endregion

#pragma region Skybox
private:
	void InitializeSkybox();
	void RenderSkybox() const;

private:
	Texture* m_skyTexture = nullptr;
	Shader* m_skyShader = nullptr;
#pragma endregion

#pragma region RenderPass
private:
	void RenderPassStartup();
	void RenderPassShutdown();

	void RenderOpaquePass() const;

	void RenderAdditivePass() const;

	void RenderSonarScanPass() const;

	void RenderBloomPass() const;

	void CopyToBackBuffer() const;

private:
	void CreateHDRRenderTargets();
	void ReleaseHDRRenderTargets();

public: // For Debugging Feature
	void ShowBloomSettings();

private:
	BloomEffect* m_bloomEffect = nullptr;

	DXGI_FORMAT const m_hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	Texture* m_sceneTexture = nullptr;			// World RenderTarget (HDR)
	Texture* m_postProcessTexture = nullptr;	// PostProcess Result (HDR) Ping Pong: If has multiple postprocessing, sceneRT<->finalRT

	mutable bool m_useSceneAsSource = true;	// true: read from scene, write to postProcess
											// false: read from postProcess, write to scene

	DescriptorHandle m_sceneRTV;
	DescriptorHandle m_sceneSRV;
	DescriptorHandle m_sceneUAV;

	DescriptorHandle m_postProcessSRV;
	DescriptorHandle m_postProcessUAV;

	// Collectible-only depth buffer (used when scene depth is bound as SRV)
	Texture* m_collectibleDepthBuffer = nullptr;
	DescriptorHandle m_collectibleDSV;

	// Still Using the default depth buffer, because we do not have shadow map to draw

	Shader* m_copyToBackBufferShader = nullptr; 
	// #ToDo Failed: tone mapping and gamma correction, do not do these in other shader (PBR)

public:
	Shader* m_unlitEmissiveShader = nullptr;
	Shader* m_comboMeterShader    = nullptr;

private:
	Shader* m_diffuseShader = nullptr;

//-----------------------------------------------------------------------------------------------
// DFS2:
public:
	void UpdateSonar(SonarParams const& params);

private:
	Shader* m_sonarScanShader = nullptr;

	SonarParams m_sonarParams;

	//void UpdateTestSonarScanImGui();

	DescriptorHandle m_defaultDepthBufferSRV;
};

