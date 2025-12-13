#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/App.hpp"
#include "Game/World.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Frustum.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Window/Window.hpp"
#include "ThirdParty/imgui/imgui.h"

//-----------------------------------------------------------------------------------------------
Game::Game()
{
	UpdateScreenDimensions();

	DebugDrawStartup();
	ResetLighting();
	InitializeCameras();
	m_clock = new Clock();

	m_world = new World();
}

Game::~Game()
{
	delete m_clock;
	m_clock = nullptr;

	delete m_world;
	m_world = nullptr;

	DebugRenderClear();

}

void Game::Update()
{
	m_screenCamera.SetOrthographicView(Vec2(0.f, 0.f), Vec2(g_screenWidth, g_screenHeight));
	DetectInputDevice();
	UpdateDeveloperCheats();
	UpdatePerFrameConstants();
	ShowCommonImGuiWindow();

	DebugDrawUpdate();

	m_world->Update();
}

void Game::Render() const
{
	// Render World

	m_world->Render();

	DebugRenderWorld(m_camera);

	// Render Screen
	g_theRenderer->BeginCamera(m_screenCamera);
	RenderUI();
	g_theRenderer->EndCamera(m_screenCamera);
	DebugRenderScreen(m_screenCamera);
}

void Game::RenderUI() const
{
	if (m_world)
	{
		m_world->RenderUI();
	}
}

void Game::OnWindowResized()
{
	UpdateScreenDimensions();

	// May have some bugs here, it is set in many places
	m_camera.SetPerspectiveView(Window::s_mainWindow->GetAspectRatio(), GAME_CAMERA_FOV, GAME_CAMERA_NEAR, GAME_CAMERA_FAR);

	m_world->OnWindowResized();
}

void Game::UpdateDeveloperCheats()
{
	if (g_theInput->WasKeyJustPressed(KEYCODE_R))
	{
		m_isPlayerCameraLocked = !m_isPlayerCameraLocked;
		if (m_isPlayerCameraLocked)
		{
			// Copy camera to locked one
			m_lockedPlayerCamera = m_camera;
		}
	}

	if (g_theInput->WasKeyJustPressed(KEYCODE_F1))
	{
		g_isDebugDraw = !g_isDebugDraw;
	}

	bool isSlowMo = g_theInput->IsKeyDown(KEYCODE_T);
	m_clock->SetTimeScale(isSlowMo ? 0.1 : 1.0);

	if (g_theInput->WasKeyJustPressed(KEYCODE_P))
	{
		m_clock->TogglePause();
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_O))
	{
		m_clock->StepSingleFrame();
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F11))
	{
		Window::s_mainWindow->ToggleFullscreen();
	}
}

void Game::UpdatePerFrameConstants()
{
	// Update PerFrameData
	PerFrameConstants perFrameCB;
	IntVec2 clientDimensions = Window::s_mainWindow->GetClientDimensions();
	perFrameCB.m_resolution = Vec3((float)clientDimensions.x, (float)clientDimensions.y, 1.f);

	perFrameCB.m_timeSeconds = (float)m_clock->GetTotalSeconds();
	g_theRenderer->SetPerFrameConstants(perFrameCB);
}

CursorMode Game::GetCursorMode() const
{
	if (m_world)
	{
		return m_world->GetCursorMode();
	}

	return CursorMode::POINTER;
}

float Game::GetDeltaSeconds() const
{
	return (float)m_clock->GetDeltaSeconds();
}

int Game::GetFrameCount() const
{
	return m_clock->GetFrameCount();
}

Vec3 Game::GetLODReferencePosition() const
{
	if (m_isPlayerCameraLocked)
	{
		return m_lockedPlayerCamera.GetPosition();
	}
	return m_camera.GetPosition();
}

Frustum Game::GetCullingFrustum() const
{
	if (m_isPlayerCameraLocked)
	{
		return m_lockedPlayerCamera.GetFrustum();
	}
	return m_camera.GetFrustum();
}

bool Game::GetScreenRay(Vec3& out_rayStart, Vec3& out_rayFwdNormal, float& out_rayLength) const
{
	// return false means no ray
	constexpr float RAY_LENGTH = 80.f; // #ToDo find a better ray length

	if (m_isPlayerCameraLocked)
	{
		return false;
	}

	Vec2 clientUV = g_theInput->GetCursorNormalizedPosition();
	bool ok = m_camera.ScreenPointToRay(out_rayStart, out_rayFwdNormal, clientUV);
	if (ok)
	{
		out_rayLength = RAY_LENGTH;
		return true;
	}
	else
	{
		return false;
	}
}

const Camera& Game::GetCamera() const
{
	return m_camera;
}

Camera& Game::GetCamera()
{
	return m_camera;
}

void Game::InitializeCameras()
{
	m_camera.SetPerspectiveView(Window::s_mainWindow->GetAspectRatio(), GAME_CAMERA_FOV, GAME_CAMERA_NEAR, GAME_CAMERA_FAR);
	m_camera.SetPositionAndOrientation(Vec3(350.f, 256.f, 300.f), EulerAngles(180.f, 30.f, 0.f));
	m_camera.SetCameraToRenderTransform(Mat44::DIRECTX_C2R);
}

void Game::DebugDrawStartup()
{
	constexpr float CELL_ASPECT = 0.9f;
	constexpr float TEXT_HEIGHT = 0.2f;
	constexpr float ORIGIN_OFFSET = 0.15f;
	DebugAddWorldBasis(Mat44(), -1.f);
	DebugAddWorldText("x - forward", Mat44(Vec3(0.f, -1.f, 0.f), Vec3(1.f, 0.f, 0.f), Vec3(0.f, 0.f, 1.f), Vec3(ORIGIN_OFFSET, 0.f, ORIGIN_OFFSET)), TEXT_HEIGHT, -1.f, CELL_ASPECT, Vec2::ZERO, Rgba8::RED);
	DebugAddWorldText("y - left", Mat44(Vec3(-1.f, 0.f, 0.f), Vec3(0.f, -1.f, 0.f), Vec3(0.f, 0.f, 1.f), Vec3(0.f, ORIGIN_OFFSET, ORIGIN_OFFSET)), TEXT_HEIGHT, -1.f, CELL_ASPECT, Vec2(1.f, 0.f), Rgba8::GREEN);
	DebugAddWorldText("z - up", Mat44(Vec3(-1.f, 0.f, 0.f), Vec3(0.f, 0.f, 1.f), Vec3(0.f, 1.f, 0.f), Vec3(0.f, -ORIGIN_OFFSET, ORIGIN_OFFSET)), TEXT_HEIGHT, -1.f, CELL_ASPECT, Vec2(0.f, 1.f), Rgba8::BLUE);
}

void Game::DebugDrawUpdate()
{
	// Camera Frustum
	if (m_isPlayerCameraLocked)
	{
		m_lockedPlayerCamera.DebugDrawFrustum();
	}
}

#pragma region ImGui
void Game::ShowCommonImGuiWindow()
{
	if (ImGui::Begin("Control Panel"))
	{
		float totalSeconds = (float)m_clock->GetTotalSeconds();
		float frameRate = (float)m_clock->GetFrameRate();
		float timeScale = (float)m_clock->GetTimeScale();
		ImGui::Text("Time: %.2f FPS: %8.02f Scale: %.2f", totalSeconds, frameRate, timeScale);
		ImGui::Text("%d vertices, %d indices", m_world->m_totalNumVertices, m_world->m_totalNumIndices);
		ImGui::Checkbox("Light Control Window", &m_showLightControlWindow);
		ImGui::InputInt("debug int", &m_debugInt, 1);
		ImGui::DragFloat("debug float", &m_debugFloat);

		if (m_world)
		{
			m_world->ShowBloomSettings();
		}

		ImGui::Text("RMB - Rotate Camera\nF1 - Debug\nR - Lock the camera \nF11 - Full Screen");
	}
	ImGui::End();
	g_theRenderer->SetEngineConstants(m_debugInt, m_debugFloat);


	if (m_showLightControlWindow)
	{
		ShowLightControlWindow(&m_showLightControlWindow);
	}



}

void Game::DebugDrawLights()
{
	for (int i = 0; i < m_lightConstants.m_numLights; ++i)
	{
		Light const& light = m_lightConstants.m_lights[i];

		Rgba8 color = Rgba8(DenormalizeByte(light.m_color[0]), DenormalizeByte(light.m_color[1]), DenormalizeByte(light.m_color[2]));
		DebugAddWorldSphere(light.m_worldPosition, 0.1f, 0.f, color);

		if (light.m_outerDotThreshold > -0.99f)
		{
			DebugAddWorldWirePenumbraNoneCull(light.m_worldPosition, light.m_spotForwardNormal, light.m_outerRadius, light.m_outerDotThreshold, 0.f, color);
		}

		if (light.m_innerDotThreshold > -0.99f)
		{
			DebugAddWorldWirePenumbraNoneCull(light.m_worldPosition, light.m_spotForwardNormal, light.m_innerRadius, light.m_innerDotThreshold, 0.f, color);
		}
		else
		{
			DebugAddWorldWireSphereNoneCull(light.m_worldPosition, light.m_innerRadius, 0.f, color);
			DebugAddWorldWireSphereNoneCull(light.m_worldPosition, light.m_outerRadius, 0.f, color);
		}
	}
}

void Game::ShowLightControlWindow(bool* pOpen)
{
	if (ImGui::Begin("Light Control Panel", pOpen))
	{
		if (ImGui::Button("Reset Lighting"))
		{
			ResetLighting();
		}
		ImGui::Checkbox("Debug Draw Lights", &m_showDebugLights);
		//-----------------------------------------------------------------------------------------------
		ImGui::SeparatorText("Sun");
		ImGui::ColorEdit4("Sun Color", m_lightConstants.m_sunColor);
		static float sunDir[3] = { 1.f, 2.f, -1.f };
		if (ImGui::DragFloat3("Sun Direction", sunDir, 0.02f, -10.f, 10.f, "%.2f"))
		{
			Vec3 newSunDir = Vec3(sunDir[0], sunDir[1], sunDir[2]);
			m_lightConstants.m_sunNormal = newSunDir.GetNormalized();
		}

		//-----------------------------------------------------------------------------------------------
		ImGui::SeparatorText("Lights");
		ImGui::SliderInt("Number of Lights", &m_lightConstants.m_numLights, 0, MAX_LIGHTS);

		for (int i = 0; i < m_lightConstants.m_numLights; ++i)
		{
			ImGui::PushID(i);
			Light& light = m_lightConstants.m_lights[i];

			if (ImGui::CollapsingHeader(Stringf("Light %d", i).c_str()))
			{
				ImGui::ColorEdit4("Color", light.m_color);
				
				//ImGui::DragFloat3("World Position", (float*)&(light.m_worldPosition), 0.1f, -10.f, 18.f, "%.2f");
				ImGui::DragFloat3("World Position", (float*)&(light.m_worldPosition), 0.1f, 0.0f, 512.0f, "%.2f");

				if (ImGui::DragFloat3("Forward Normal", m_lightDirBuffer[i], 0.01f))
				{
					Vec3 newDir = Vec3(m_lightDirBuffer[i][0], m_lightDirBuffer[i][1], m_lightDirBuffer[i][2]);
					light.m_spotForwardNormal = newDir.GetNormalized();
				}

				ImGui::SliderFloat("Ambience", &light.m_ambience, 0.0f, 1.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
				ImGui::DragFloat("Inner Radius", &light.m_innerRadius, 0.1f, 0.0f, 512.0f, "%.3f");
				ImGui::DragFloat("Outer Radius", &light.m_outerRadius, 0.1f, 0.0f, 512.0f, "%.3f");
				//ImGui::SliderFloat("Inner Radius", &light.m_innerRadius, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
				//ImGui::SliderFloat("Outer Radius", &light.m_outerRadius, light.m_innerRadius + 0.001f, 12.0f);

				ImGui::SliderFloat("Inner Penumbra Dot", &light.m_innerDotThreshold, -1.0f, 1.0f);
				ImGui::SliderFloat("Outer Penumbra Dot", &light.m_outerDotThreshold, -1.0f, 1.0f);
			}

			ImGui::PopID();
		}

	}
	ImGui::End();

	if (m_showDebugLights)
	{
		DebugDrawLights();
	}

	ApplyLighting();
}

void Game::ResetLighting()
{
	// ================== Sun Light (Main Light Source) ==================
// Warm white sunlight, simulating natural light in the afternoon
	m_lightConstants.m_sunColor[0] = 1.0f;
	m_lightConstants.m_sunColor[1] = 0.98f;
	m_lightConstants.m_sunColor[2] = 0.95f;
	m_lightConstants.m_sunColor[3] = 0.85f;  // Increased intensity as main light source

	// Shining from right-front-top at 45 degrees (simulating 3 PM sunlight)
	m_lightConstants.m_sunNormal = Vec3(1.2f, 0.8f, -1.5f).GetNormalized();

	m_lightConstants.m_numLights = 7;

	// ================== Sky Fill Light System (Simulating Sky Scattering) ==================
	// Light 0: Sky fill light - Central high altitude
	Light skyLight1;
	skyLight1.SetColor(0.6f, 0.75f, 1.0f, 0.3f);  // Light blue sky light
	skyLight1.m_worldPosition = Vec3(256.f, 256.f, 400.f);  // Above scene center
	skyLight1.m_innerRadius = 250.f;
	skyLight1.m_outerRadius = 400.f;
	skyLight1.m_ambience = 0.15f;  // Provides base ambient brightness
	m_lightConstants.m_lights[0] = skyLight1;

	// Light 1: Sky fill light - Front area
	Light skyLight2;
	skyLight2.SetColor(0.55f, 0.7f, 0.95f, 0.25f);
	skyLight2.m_worldPosition = Vec3(400.f, 256.f, 380.f);
	skyLight2.m_innerRadius = 200.f;
	skyLight2.m_outerRadius = 350.f;
	skyLight2.m_ambience = 0.1f;
	m_lightConstants.m_lights[1] = skyLight2;

	// Light 2: Sky fill light - Back area
	Light skyLight3;
	skyLight3.SetColor(0.5f, 0.65f, 0.9f, 0.2f);
	skyLight3.m_worldPosition = Vec3(100.f, 256.f, 380.f);
	skyLight3.m_innerRadius = 180.f;
	skyLight3.m_outerRadius = 320.f;
	skyLight3.m_ambience = 0.08f;
	m_lightConstants.m_lights[2] = skyLight3;

	// ================== Ground Bounce Light System ==================
	// Light 3: Ground warm light - Simulating ground reflection
	Light groundLight1;
	groundLight1.SetColor(1.0f, 0.9f, 0.75f, 0.25f);  // Warm tone
	groundLight1.m_worldPosition = Vec3(256.f, 256.f, 50.f);  // Close to ground
	groundLight1.m_innerRadius = 200.f;
	groundLight1.m_outerRadius = 300.f;
	groundLight1.m_ambience = 0.08f;
	m_lightConstants.m_lights[3] = groundLight1;

	// Light 4: Secondary ground light - Front area
	Light groundLight2;
	groundLight2.SetColor(0.95f, 0.85f, 0.7f, 0.2f);
	groundLight2.m_worldPosition = Vec3(380.f, 256.f, 40.f);
	groundLight2.m_innerRadius = 150.f;
	groundLight2.m_outerRadius = 250.f;
	groundLight2.m_ambience = 0.05f;
	m_lightConstants.m_lights[4] = groundLight2;

	// ================== Edge Fill/Rim Lights ==================
	// Light 5: Side rim light (Left side)
	Light rimLight1;
	rimLight1.SetColor(0.7f, 0.8f, 1.0f, 0.35f);  // Cool tone rim light
	rimLight1.m_worldPosition = Vec3(256.f, 450.f, 250.f);
	rimLight1.m_innerRadius = 180.f;
	rimLight1.m_outerRadius = 280.f;
	m_lightConstants.m_lights[5] = rimLight1;

	// Light 6: Side rim light (Right side)
	Light rimLight2;
	rimLight2.SetColor(1.0f, 0.95f, 0.85f, 0.3f);  // Warm tone rim light
	rimLight2.m_worldPosition = Vec3(256.f, 60.f, 250.f);
	rimLight2.m_innerRadius = 150.f;
	rimLight2.m_outerRadius = 250.f;
	m_lightConstants.m_lights[6] = rimLight2;

	// ================== Key Focus Light ==================
	// Light 7: Central spotlight (Can be used to highlight key areas)
	//Light focusLight;
	//focusLight.SetColor(1.0f, 0.95f, 0.9f, 0.5f);  // Strong warm light
	//focusLight.m_worldPosition = Vec3(256.f, 256.f, 300.f);
	//focusLight.m_spotForwardNormal = Vec3(0.f, 0.f, -1.f);  // Pointing downward
	//focusLight.m_innerRadius = 80.f;
	//focusLight.m_outerRadius = 150.f;
	//focusLight.m_innerDotThreshold = 0.85f;  // Spotlight effect
	//focusLight.m_outerDotThreshold = 0.6f;
	//m_lightConstants.m_lights[7] = focusLight;

	// Update light direction buffer
	for (int i = 0; i < m_lightConstants.m_numLights; ++i) {
		m_lightDirBuffer[i][0] = m_lightConstants.m_lights[i].m_spotForwardNormal.x;
		m_lightDirBuffer[i][1] = m_lightConstants.m_lights[i].m_spotForwardNormal.y;
		m_lightDirBuffer[i][2] = m_lightConstants.m_lights[i].m_spotForwardNormal.z;
	}

	ApplyLighting();
}

void Game::ApplyLighting()
{
	g_theRenderer->SetLightConstants(m_lightConstants);
}


#pragma endregion

void Game::DetectInputDevice()
{
	bool hasKeyboardMouseInput = g_theInput->HasAnyKeyboardMouseInput() || g_theInput->HasMouseMoved();

	bool hasGamepadInput = g_theInput->HasAnyControllerInput(0); // Only Check Controller 0

	if (hasKeyboardMouseInput && m_currentInputDevice != InputDevice::KEYBOARD_MOUSE)
	{
		m_currentInputDevice = InputDevice::KEYBOARD_MOUSE;
		OnInputDeviceChanged();
	}
	else if (hasGamepadInput && m_currentInputDevice != InputDevice::GAMEPAD)
	{
		m_currentInputDevice = InputDevice::GAMEPAD;
		OnInputDeviceChanged();
	}
}

void Game::OnInputDeviceChanged()
{
	if (m_currentInputDevice == InputDevice::KEYBOARD_MOUSE)
	{
		if (m_world)
		{
			m_world->ResetPlayerShipMouseRotationInput();
		}
		DebugAddMessage("Switched to Keyboard & Mouse", 1.f, Rgba8::YELLOW);
	}
	else
	{
		DebugAddMessage("Switched to Gamepad", 1.f, Rgba8::YELLOW);
	}
}
