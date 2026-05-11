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

	// Last-frame unified rotation input (mouse or gamepad), [-1, 1] in each axis. Persisted from
	// UpdateInput so external systems (HUD tilt, etc.) can read it during render.
	Vec2 GetRotationInput() const { return m_rotationInput; }

private:
	void InitializeParams(InputOutputParams& out_params) const;
	void UpdateInput(InputOutputParams& io_params);
	void UpdateTerrainCollision(InputOutputParams& io_params);
	void ApplyCollisionAdditiveRotation(InputOutputParams& io_params, Vec3 const& shipWorldPos, Vec3 const& impactPos, Vec3 const& impactNormal);
	void BlendOutCollisionAdditiveRotation(InputOutputParams& io_params);
	void UpdateBarrelRoll(InputOutputParams& io_params);

	float GetDeltaRollDegrees_Uniform(float deltaSeconds); // Will reset m_isRollingToHorizon, if reaches the horizon
	float GetDeltaRollDegrees_Interp(float deltaSeconds);

	void UpdateDebugInput();
	void UpdateDebugPlayerAttributes();

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

	// Unified rotation input (mouse or gamepad), persisted from io_params at end of UpdateInput.
	Vec2 m_rotationInput;


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
	// Sonar Scan
	void UpdateSonarScanning(InputOutputParams& io_params);

private:
	//static const int GUN_COUNT = 4; // #ToDo set weapon in xml and ship definition class, GUARANTEE > 0
	//Vec3 m_gunOffset[GUN_COUNT]; // #ToDo set them in xml and ship definition class
	//float m_fireInterval = 0.1f;	// #ToDo set in xml
	float m_timeSinceLastFire = 0.0f;
	int m_currentCannonIndex = 0;

	float m_timeSinceLastScan = 0.0f;

private:
	// Collision Additive Rotation (visual flinch on hard impact)
	Quat m_collisionAdditiveRotation = Quat::IDENTITY; // full target rotation from impact
	float m_collisionAdditiveAlpha = 0.f;              // 1.0 at trigger, decays to 0

private:
	// Barrel Roll
	bool  m_isBarrelRolling = false;
	float m_barrelRollTotalAngle = 0.f;      // +360 (right) or -360 (left)
	float m_barrelRollElapsedTime = 0.f;
	float m_barrelRollCooldownTimer = 0.f;   // counts down to 0


	
public:
	// PlayerShip Attribute
	int GetPlayerTier() const;
	int GetPlayerStrength() const;

	void SetPlayerTier(int tier);
	void SetPlayerStrength(int strength);

private:
	int m_playerTier = 0;
	int m_playerStrength = 0;
};


