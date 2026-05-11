#pragma once
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/RendererCommon.hpp"
#include "Engine/Input/InputSystem.hpp"

// ----------------------------------------------------------------------------------------------
class Game
{
public:
	Game();
	~Game();
	void Update();
	void Render() const;
	void RenderUI() const;

	void OnWindowResized(); // Event WINDOW_RESIZE_EVENT, refresh the setting of the camera

protected:
	void UpdateDeveloperCheats();
	void UpdatePerFrameConstants();

public:
	CursorMode GetCursorMode() const;
	float GetDeltaSeconds() const;
	float GetTotalSeconds() const;
	int GetFrameCount() const;

protected:
	Clock* m_clock = nullptr;

public:
	Vec3 GetLODReferencePosition() const;
	Frustum GetCullingFrustum() const;
	bool GetScreenRay(Vec3& out_rayStart, Vec3& out_rayFwdNormal, float& out_rayLength) const;

public:
	const Camera& GetCamera() const;
	Camera& GetCamera();
	const Camera& GetScreenCamera() const { return m_screenCamera; }

protected:
	void InitializeCameras();

protected:
	Camera m_camera;
	Camera m_screenCamera;

	Camera m_lockedPlayerCamera;
	bool m_isPlayerCameraLocked = false;

protected:
	World* m_world = nullptr;

protected:
	void DebugDrawStartup();
	void DebugDrawUpdate();


#pragma region ImGui
protected:
	void ShowCommonImGuiWindow();
	//void ToggleCursorMode();
	void DebugDrawLights();

	void ShowLightControlWindow(bool* pOpen);

	void ResetLighting();
	void ApplyLighting();
	
protected:
	bool m_showLightControlWindow = false;
	LightConstants m_lightConstants;
	float m_lightDirBuffer[MAX_LIGHTS][3]; // for ImGui
	bool m_showDebugLights = false;

	// Engine Constants
	int m_debugInt = 0;
	float m_debugFloat = 0.f;
#pragma endregion

public:
	bool IsUsingGamepad() const { return m_currentInputDevice == InputDevice::GAMEPAD; }

private:
	void DetectInputDevice();

	void OnInputDeviceChanged();
private:
	enum class InputDevice
	{
		KEYBOARD_MOUSE,
		GAMEPAD
	};

	InputDevice m_currentInputDevice = InputDevice::KEYBOARD_MOUSE;
};
