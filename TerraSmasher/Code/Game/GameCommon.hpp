#pragma once
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/RendererCommon.hpp"

//-----------------------------------------------------------------------------------------------
class AudioSystem;
class JobSystem;
class InputSystem;
class Renderer;
class Window;
class App;
class Game;
class Clock;
class Shader;

struct Vertex_PCU;
struct IntVec2;

//-----------------------------------------------------------------------------------------------
class World;
class TerrainStructuredBuffer;

//-----------------------------------------------------------------------------------------------
extern AudioSystem*		g_theAudio;
extern JobSystem*		g_theJobSystem;
extern InputSystem*		g_theInput;
extern Renderer*		g_theRenderer;
extern Window*			g_theWindow;
extern App*				g_theApp;
extern Game*			g_theGame;

//-----------------------------------------------------------------------------------------------
// Gameplay Globals
extern bool g_isDebugDraw;

extern float g_screenWidth;
extern float g_screenHeight;
extern unsigned int	g_windowWidth;
extern unsigned int g_windowHeight;

//-----------------------------------------------------------------------------------------------
// Gameplay Constants
constexpr float EDITOR_CAMERA_MOVE_SPEED = 4.f;
constexpr float EDITOR_CAMERA_YAW_TURN_RATE = 180.f;
constexpr float EDITOR_CAMERA_PITCH_TURN_RATE = 180.f;
constexpr float EDITOR_CAMERA_SPEED_FACTOR = 20.f;
constexpr float EDITOR_CAMERA_MAX_PITCH = 85.f;

constexpr float GAME_CAMERA_FOV = 60.f; // V=0 60 V=Max 80?
constexpr float GAME_CAMERA_NEAR = 0.1f;
constexpr float GAME_CAMERA_FAR = 1500.f;

constexpr float SHIP_ROUGH_SIZE = 2.f; // #ToDo define it in XML

constexpr float MOUSE_ROTATION_INPUT_DEADZONE = 0.2f;
constexpr float MOUSE_ROTATION_SENSITIVITY = 3.0f;
constexpr float THROTTLE_LEVER_SPEED = 1.f;

constexpr float ROLL_TO_HORIZON_RATE = 110.f;
constexpr float ROLL_TO_HORIZON_INTERP_SPEED = 5.f;

constexpr float FOV_INTERP_SPEED = 3.f;
constexpr float CAMERA_DIST_INTERP_SPEED = 3.f;

constexpr float BODY_POSITION_INTERP_SPEED = 2.5f;
constexpr float BODY_ROTATION_INTERP_SPEED = 3.0f;

constexpr float SPACE_METERS_PER_UNIT = 10.f;

//-----------------------------------------------------------------------------------------------
void UpdateScreenDimensions();

//-----------------------------------------------------------------------------------------------
// Render Resources
struct DiffuseTerrainResources
{
	uint32_t engineConstantsIndex = INVALID_INDEX_U32;
	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
	uint32_t modelConstantsIndex = INVALID_INDEX_U32;
	uint32_t lightConstantsIndex = INVALID_INDEX_U32;
};

struct TerrainRenderResource
{
	uint32_t engineConstantsIndex = INVALID_INDEX_U32;
	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
	uint32_t modelConstantsIndex = INVALID_INDEX_U32;
	uint32_t lightConstantsIndex = INVALID_INDEX_U32;
	uint32_t perFrameConstantsIndex = INVALID_INDEX_U32;

	uint32_t materialBufferIndex = INVALID_INDEX_U32; // StructuredBuffer<GMaterial>
};

struct ExperimentalTerrainRenderResource
{
	uint32_t engineConstantsIndex = INVALID_INDEX_U32;
	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
	uint32_t modelConstantsIndex = INVALID_INDEX_U32;
	uint32_t lightConstantsIndex = INVALID_INDEX_U32;
	uint32_t perFrameConstantsIndex = INVALID_INDEX_U32;

	uint32_t materialBufferIndex = INVALID_INDEX_U32; // StructuredBuffer<GMaterial>
	uint32_t sharedVertexBufferIndex = INVALID_INDEX_U32;
	uint32_t perVertexBufferIndex = INVALID_INDEX_U32;
};

struct FullScreenQuadResources
{
	uint32_t textureIndex = INVALID_INDEX_U32;
	uint32_t samplerIndex = INVALID_INDEX_U32;
};

struct FullScreenQuadWithDepthResources
{
	uint32_t textureIndex = INVALID_INDEX_U32;
	uint32_t depthTexIndex = INVALID_INDEX_U32;
	uint32_t samplerIndex = INVALID_INDEX_U32;
};

struct UnlitEmissiveResources
{
	uint32_t diffuseTextureIndex = INVALID_INDEX_U32;
	uint32_t diffuseSamplerIndex = INVALID_INDEX_U32;

	uint32_t cameraConstantsIndex = INVALID_INDEX_U32;
	uint32_t modelConstantsIndex = INVALID_INDEX_U32;

	float emissiveStrength = 1.0f;
};

