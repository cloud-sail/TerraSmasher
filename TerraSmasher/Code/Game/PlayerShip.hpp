#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Quat.hpp"
#include "Engine/Math/Mat44.hpp"
#include <string>

class World;
class Camera;

class ShipDefinition;
struct ShipSpawnInfo;
struct InputOutputParams;


class PlayerShip
{
public:
	PlayerShip(World* world, ShipSpawnInfo const& spawnInfo);

	void Update();
	void Render() const;
	void RenderUI() const;
	void RenderAdditive() const;

	void UpdateCamera(Camera& camera);

	void ResetMouseRotationInput() { m_mouseRotationInput = Vec2::ZERO; } // #ToDo event system

private:
	void InitializeParams(InputOutputParams& out_params) const;
	void UpdateInput(InputOutputParams& io_params);

	float GetDeltaRollDegrees_Uniform(float deltaSeconds); // Will reset m_isRollingToHorizon, if reaches the horizon
	float GetDeltaRollDegrees_Interp(float deltaSeconds);

private:
	Mat44 GetShipModelToWorldTransform() const;

private:
	// Control Frame Transform (World Space)
	Vec3 m_controlFramePosition; // in world coords
	Quat m_controlFrameRotation; // in world coords


	// Body Frame Transform (Control-Relative Space)
	Vec3 m_bodyLocalPosition; // in control coords
	Quat m_bodyLocalRotation; // in control coords

	Vec3 m_velocity; // in world coords


private:
	ShipDefinition const* m_definition = nullptr;
	World* m_world = nullptr;

private:
	//Mat44 m_cameraLocalTransform; // Final Result
	//Vec3 m_prevCameraOffset;

	// Position is lerping, Rotation is precise
	// first do hit detection, set target position and then lerping to target
	// In Control Frame Transform
	//Vec3 m_cameraLookAtOffset; // Or float, along forward?
	//Vec3 m_cameraOffset; // When Accelerating the offset will change

	// Or ALL lerping to target value
	//float cameraDistance;        
	//float cameraHeight;          
	//float lookAheadDistance;    

	Vec3 GetAimedWorldPosition() const;

private:
	// Input State
	bool m_isRollingToHorizon = false; // set false when reach horizontal, or on hit

	float m_throttleInput = 0.f; // 0 ~ 1
	// Only used in keyboard & mouse control
	Vec2 m_mouseRotationInput; // Clamp to Unit cycle, has a dead zone area, +x right +y up


private:
	// Camera Rig Parameters
	float m_cameraDistance = 2.545f; // camera Position Vec3(0.f, 0.f, -m_cameraDistance)
	float m_cameraFOV = 60.f;

	Vec3 m_cameraShakeOrbitTarget;
	float m_trauma = 0.f;
	float m_traumaReduceRate = 1.f;

private:
	// Fire Logic
	void UpdateFiring(InputOutputParams& io_params);

private:
	//static const int GUN_COUNT = 4; // #ToDo set weapon in xml and ship definition class, GUARANTEE > 0
	//Vec3 m_gunOffset[GUN_COUNT]; // #ToDo set them in xml and ship definition class
	//float m_fireInterval = 0.1f;	// #ToDo set in xml
	float m_timeSinceLastFire = 0.0f;
	int m_currentCannonIndex = 0;
};


