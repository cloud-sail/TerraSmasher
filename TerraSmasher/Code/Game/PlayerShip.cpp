#include "Game/PlayerShip.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Game.hpp"
#include "Game/World.hpp"
#include "Game/ProjectileSystem.hpp"
#include "Game/ShipDefinition.hpp"
#include "Game/ShipTypes.hpp"
#include "Game/ShipBehaviors.hpp"
#include "Game/GameVertexUtils.hpp"

#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/DebugRender.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Gradient.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Window/Window.hpp"

//-----------------------------------------------------------------------------------------------
constexpr float MOUSE_OUTER_RADIUS_RATIO = 0.25f;
constexpr float MOUSE_INNER_RADIUS_RATIO = MOUSE_OUTER_RADIUS_RATIO * MOUSE_ROTATION_INPUT_DEADZONE;
constexpr float GAMEPAD_RADIUS_RATIO = 0.2f;
constexpr float CROSSHAIR_RADIUS_RATIO = 0.018f;
constexpr float INDICATOR_HALF_WIDTH_RATIO = 0.003f;
constexpr float INDICATOR_HEIGHT_RATIO = 0.024f;

constexpr float THROTTLE_WIDTH_RATIO = 0.02f;
constexpr float THROTTLE_HEIGHT_RATIO = 0.4f;

constexpr float COLLISION_FLINCH_DEGREES = 85.f;
constexpr float COLLISION_FLINCH_BLEND_OUT_SPEED = 3.f; // alpha units per second

//-----------------------------------------------------------------------------------------------
PlayerShip::PlayerShip(World* world, ShipSpawnInfo const& spawnInfo)
	: m_world(world)
{
	m_definition = ShipDefinition::GetByName(spawnInfo.m_shipName);
	m_controlFramePosition = spawnInfo.m_worldPosition;
	m_controlFrameRotation = spawnInfo.m_worldRotation;

	m_bodyLocalPosition = m_definition->m_visualPositionMiddle;
	m_bodyLocalRotation = m_definition->m_visualRotationMiddle;

	m_cameraDistance = m_definition->m_minCameraDistance;
	m_cameraFOV = m_definition->m_minCameraFOV;


	m_timeSinceLastScan = m_definition->m_sonarScanDuration;
}

void PlayerShip::Update()
{

	UpdateDebugInput();

	InputOutputParams params;
	InitializeParams(params);

	UpdateInput(params);

	ShipBehavior::UpdateThrottle(params, *m_definition);
	ShipBehavior::UpdateLateralDrag(params, *m_definition);
	
	// Visual Rotation & Position
	ShipBehavior::UpdateTargetBodyLocalPosition(params, *m_definition);
	ShipBehavior::UpdateTargetBodyLocalRotation(params, *m_definition);

	// Camera parameter updates, Update game camera later
	ShipBehavior::UpdateCameraDistance(params, *m_definition);
	ShipBehavior::UpdateCameraFOV(params, *m_definition);


	// Update Physics
	m_velocity = params.m_linearVelocity;
	m_controlFramePosition += m_velocity * params.m_deltaSeconds;

	float deltaRollDegrees = GetDeltaRollDegrees_Interp(params.m_deltaSeconds);
	m_controlFrameRotation = m_controlFrameRotation * 
		Quat::MakeFromEulerAngles(EulerAngles(params.m_angularVelocity.x * params.m_deltaSeconds, params.m_angularVelocity.y * params.m_deltaSeconds, deltaRollDegrees));
	m_controlFrameRotation.Normalize();
	//...

	m_cameraDistance = params.m_cameraDistance;
	m_cameraFOV = params.m_cameraFOV;

	// Terrain Collision (after physics, uses ship model world center)
	UpdateTerrainCollision(params);



	// Collision additive rotation: modify target before interpolation
	BlendOutCollisionAdditiveRotation(params);

	// Barrel roll: modify target after collision additive, rotates around body-local forward
	UpdateBarrelRoll(params);

	// Visual Interpolation
	//m_bodyLocalPosition = InterpTo(params.m_prevBodyLocalPosition, params.m_bodyTargetLocalPosition, params.m_deltaSeconds, BODY_POSITION_INTERP_SPEED);
	//m_bodyLocalRotation = InterpToNlerp(params.m_prevBodyLocalRotation, params.m_bodyTargetLocalRotation, params.m_deltaSeconds, BODY_ROTATION_INTERP_SPEED);


	// During barrel roll, directly assign rotation to avoid Nlerp shortest-path issue
	// (accumulated roll angle can exceed 180 degrees, causing Nlerp to reverse direction)
	if (m_isBarrelRolling)
	{
		//m_bodyLocalRotation = params.m_bodyTargetLocalRotation;
		m_bodyLocalPosition = InterpTo(params.m_prevBodyLocalPosition, params.m_bodyTargetLocalPosition, params.m_deltaSeconds, BODY_POSITION_INTERP_SPEED * 1.5f);
		m_bodyLocalRotation = InterpToNlerp(params.m_prevBodyLocalRotation, params.m_bodyTargetLocalRotation, params.m_deltaSeconds, BODY_ROTATION_INTERP_SPEED * 3.f);

	}
	else
	{
		m_bodyLocalPosition = InterpTo(params.m_prevBodyLocalPosition, params.m_bodyTargetLocalPosition, params.m_deltaSeconds, BODY_POSITION_INTERP_SPEED);
		m_bodyLocalRotation = InterpToNlerp(params.m_prevBodyLocalRotation, params.m_bodyTargetLocalRotation, params.m_deltaSeconds, BODY_ROTATION_INTERP_SPEED);
	}
	//-----------------------------------------------------------------------------------------------
	// After
	UpdateFiring(params);
	UpdateSonarScanning(params);
}

void PlayerShip::Render() const
{
	g_theRenderer->SetModelConstants(GetShipModelToWorldTransform());

	BlinnPhongRenderResources blinnPhongRes;
	blinnPhongRes.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(m_definition->m_shipDiffuseTexture, DefaultTexture::WhiteOpaque2D);
	blinnPhongRes.normalTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(m_definition->m_shipNormalTexture, DefaultTexture::DefaultNormalMap);
	blinnPhongRes.specGlossEmitTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(m_definition->m_shipSGETexture, DefaultTexture::DefaultSpecGlossEmitMap);

	blinnPhongRes.engineConstantsIndex = g_theRenderer->GetCurrentEngineConstantsIndex();
	blinnPhongRes.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	blinnPhongRes.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();
	blinnPhongRes.lightConstantsIndex = g_theRenderer->GetCurrentLightConstantsIndex();

	g_theRenderer->SetGraphicsBindlessResources(sizeof(BlinnPhongRenderResources), &blinnPhongRes);

	g_theRenderer->BindShader(m_definition->m_shipShader);
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
	g_theRenderer->DrawIndexedVertexBuffer(m_definition->m_shipVB, m_definition->m_shipIB, m_definition->m_shipIB->GetCount());
}

void PlayerShip::RenderUI() const
{


	auto makeSubRegion = [](const AABB2& box, const AABB2& uvRegion) -> AABB2 {
		return AABB2(
			box.GetPointAtUV(uvRegion.m_mins),
			box.GetPointAtUV(uvRegion.m_maxs)
		);
		};

	float const SCREEN_WIDTH = g_screenWidth;
	float const SCREEN_HEIGHT = g_screenHeight;
	Vec2 const SCREEN_CENTER = Vec2(SCREEN_WIDTH, SCREEN_HEIGHT) * 0.5f;
	AABB2 const SCREEN_BOUNDS = AABB2(Vec2::ZERO, Vec2(SCREEN_WIDTH, SCREEN_HEIGHT));

	EulerAngles controlFrameOrientation = Mat44::MakeFromUnitQuat(m_controlFrameRotation).GetEulerAngles();
	float attitudeIndicatorRollOrientation = controlFrameOrientation.m_rollDegrees;

	Vec3 forwardDirection = m_controlFrameRotation.RotateVector(Vec3::FORWARD).GetNormalized();

	bool useGamepad = g_theGame->IsUsingGamepad();

	{
		std::vector<Vertex_PCU> verts;

		// Crosshair
		{
			Vec2 crosshairCenter = SCREEN_CENTER;

			if (!useGamepad)
			{
				crosshairCenter += m_mouseRotationInput * MOUSE_OUTER_RADIUS_RATIO * SCREEN_HEIGHT;
			}

			AddVertsForRing2D(verts, crosshairCenter, CROSSHAIR_RADIUS_RATIO * SCREEN_HEIGHT, CROSSHAIR_RADIUS_RATIO * SCREEN_HEIGHT * 0.15f, Rgba8::OPAQUE_WHITE, 32);
			AddVertsForDisc2D(verts, crosshairCenter, CROSSHAIR_RADIUS_RATIO * SCREEN_HEIGHT * 0.1f, Rgba8::OPAQUE_WHITE, 32);
		}

		// Attitude Indicator
		{
			Vec2 attitudeIndicatorDirection = Vec2::MakeFromPolarDegrees(attitudeIndicatorRollOrientation);
			if (useGamepad)
			{
				float startRadius = SCREEN_HEIGHT * GAMEPAD_RADIUS_RATIO;
				float endRadius = SCREEN_HEIGHT * (GAMEPAD_RADIUS_RATIO + INDICATOR_HEIGHT_RATIO);
				float halfWidth = SCREEN_HEIGHT * INDICATOR_HALF_WIDTH_RATIO;
				AddGameVertsForIsoscelesTriangle2D(verts, SCREEN_CENTER + startRadius * attitudeIndicatorDirection, SCREEN_CENTER + endRadius * attitudeIndicatorDirection, halfWidth, Rgba8::YELLOW);
				AddGameVertsForIsoscelesTriangle2D(verts, SCREEN_CENTER - startRadius * attitudeIndicatorDirection, SCREEN_CENTER - endRadius * attitudeIndicatorDirection, halfWidth, Rgba8::YELLOW);
			}
			else
			{
				float startRadius = SCREEN_HEIGHT * MOUSE_OUTER_RADIUS_RATIO;
				float endRadius = SCREEN_HEIGHT * (MOUSE_OUTER_RADIUS_RATIO + INDICATOR_HEIGHT_RATIO);
				float halfWidth = SCREEN_HEIGHT * INDICATOR_HALF_WIDTH_RATIO;
				AddGameVertsForIsoscelesTriangle2D(verts, SCREEN_CENTER + startRadius * attitudeIndicatorDirection, SCREEN_CENTER + endRadius * attitudeIndicatorDirection, halfWidth, Rgba8::YELLOW);
				AddGameVertsForIsoscelesTriangle2D(verts, SCREEN_CENTER - startRadius * attitudeIndicatorDirection, SCREEN_CENTER - endRadius * attitudeIndicatorDirection, halfWidth, Rgba8::YELLOW);

				float innerStartRadius = SCREEN_HEIGHT * MOUSE_INNER_RADIUS_RATIO;
				float innerEndRadius = SCREEN_HEIGHT * (MOUSE_INNER_RADIUS_RATIO + INDICATOR_HEIGHT_RATIO * 0.5f);
				float innerHalfWidth = SCREEN_HEIGHT * INDICATOR_HALF_WIDTH_RATIO;
				AddGameVertsForIsoscelesTriangle2D(verts, SCREEN_CENTER + innerStartRadius * attitudeIndicatorDirection, SCREEN_CENTER + innerEndRadius * attitudeIndicatorDirection, innerHalfWidth, Rgba8::YELLOW);
				AddGameVertsForIsoscelesTriangle2D(verts, SCREEN_CENTER - innerStartRadius * attitudeIndicatorDirection, SCREEN_CENTER - innerEndRadius * attitudeIndicatorDirection, innerHalfWidth, Rgba8::YELLOW);
			}
		}

		// Throttle & Debug Speed
		{
			Vec2 throttleBackSize = Vec2(THROTTLE_WIDTH_RATIO * SCREEN_HEIGHT, THROTTLE_HEIGHT_RATIO * SCREEN_HEIGHT);
			AABB2 throttleBackBox = AABB2(Vec2::ZERO, throttleBackSize);
			Vec2 throttleBackPivot = Vec2(0.f, 0.5f);
			throttleBackBox.Translate(-throttleBackPivot * throttleBackSize); // Pivot at origin
			throttleBackBox.Translate(SCREEN_BOUNDS.GetPointAtUV(Vec2(0.f, 0.5f)));
			throttleBackBox.Translate(Vec2(SCREEN_HEIGHT * 0.05f, 0.f));


			AddVertsForAABB2D(verts, throttleBackBox, Rgba8(30, 30, 40, 204));
		
			AABB2 throttleFrontBox = throttleBackBox;
			throttleFrontBox.ChopOffTop(GetClampedZeroToOne(1.f - m_throttleInput));
			Gradient throttleGradient;
			std::vector<GradientRgba8Key> keys =
			{
				GradientRgba8Key(0.0f,	Rgba8(120, 50, 0)),
				GradientRgba8Key(0.5f,	Rgba8(200, 80, 0)),
				GradientRgba8Key(1.0f,	Rgba8(255, 140, 0)),
			};
			throttleGradient.SetKeys(keys);

			AddVertsForAABB2D(verts, throttleFrontBox, throttleGradient.Evaluate(m_throttleInput));



			float forwardSpeed = DotProduct3D(forwardDirection, m_velocity) * SPACE_METERS_PER_UNIT;

			DebugAddScreenText(Stringf("%3d m/s", (int)forwardSpeed),
				makeSubRegion(SCREEN_BOUNDS, AABB2(Vec2(0.02f, 0.02f), Vec2(0.1f, 0.1f))),
				40.f, Vec2(0.f, 0.5f), 0.f, 0.55f);

		}


		UnlitRenderResources resources;
		resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(nullptr);
		resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);
		resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
		resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();
		g_theRenderer->SetGraphicsBindlessResources(sizeof(UnlitRenderResources), &resources);

		g_theRenderer->BindShader(nullptr);
		g_theRenderer->SetBlendMode(BlendMode::ALPHA);
		g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
		g_theRenderer->SetDepthMode(DepthMode::DISABLED);

		g_theRenderer->DrawVertexArray(verts);
	}




}

void PlayerShip::RenderAdditive() const
{
	// Temp Code
	static Gradient s_thrusterGradient = []() {
		std::vector<GradientRgba8Key> keys = {
			GradientRgba8Key(0.0f,  Rgba8(10, 40, 80)),    
			GradientRgba8Key(0.25f, Rgba8(30, 120, 200)),  
			GradientRgba8Key(0.5f,  Rgba8(60, 180, 255)),  
			GradientRgba8Key(0.75f, Rgba8(100, 200, 255)), 
			GradientRgba8Key(1.0f,  Rgba8(120, 220, 255))  
		};
		Gradient gradient;
		gradient.SetKeys(keys);
		return gradient;
		}();

	g_theRenderer->SetModelConstants(GetShipModelToWorldTransform());

	float coneRadius = Interpolate(0.015f, 0.04f, m_throttleInput);        
	float coneLength = Interpolate(0.05f, 0.7f, m_throttleInput);          
	float emissiveStrength = Interpolate(3.f, 10.f, m_throttleInput);      
	Rgba8 thrusterColor = s_thrusterGradient.Evaluate(m_throttleInput);    

	Vec3 coneStart = Vec3::ZERO;
	constexpr int THRUSTER_COUNT = 4;
	constexpr float xOffset = -0.3f;
	constexpr float yOffset = 0.16f;
	constexpr float zOffset = 0.076f;
	Vec3 coneOffsets[THRUSTER_COUNT] = {
		Vec3(xOffset, yOffset, zOffset),
		Vec3(xOffset, -yOffset, zOffset),
		Vec3(xOffset, yOffset, -zOffset),
		Vec3(xOffset, -yOffset, -zOffset)
	};

	std::vector<Vertex_PCU> verts;
	for (int i = 0; i < THRUSTER_COUNT; ++i)
	{
		Vec3 thrusterStart = coneStart + coneOffsets[i];
		Vec3 thrusterEnd = thrusterStart - Vec3::FORWARD * coneLength;
		AddVertsForCone3D(verts, thrusterStart, thrusterEnd, coneRadius, thrusterColor);
	}

	UnlitEmissiveResources resources;
	resources.diffuseTextureIndex = g_theRenderer->GetSrvIndexFromLoadedTexture(nullptr, DefaultTexture::WhiteOpaque2D);
	resources.diffuseSamplerIndex = g_theRenderer->GetDefaultSamplerIndex(SamplerMode::POINT_CLAMP);
	resources.cameraConstantsIndex = g_theRenderer->GetCurrentCameraConstantsIndex();
	resources.modelConstantsIndex = g_theRenderer->GetCurrentModelConstantsIndex();
	resources.emissiveStrength = emissiveStrength;
	g_theRenderer->SetGraphicsBindlessResources(sizeof(UnlitEmissiveResources), &resources);
	g_theRenderer->BindShader(m_world->m_unlitEmissiveShader);
	g_theRenderer->SetBlendMode(BlendMode::ADDITIVE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetDepthMode(DepthMode::READ_ONLY_LESS_EQUAL);
	g_theRenderer->DrawVertexArray(verts);
}

void PlayerShip::UpdateCamera(Camera& camera)
{
	// Calculate Final Position Here and apply trauma


	Mat44 controlToWorldTransform;
	controlToWorldTransform = Mat44::MakeFromUnitQuat(m_controlFrameRotation);
	controlToWorldTransform.SetTranslation3D(m_controlFramePosition);

	Mat44 cameraToControlTransform = Mat44();
	cameraToControlTransform.SetTranslation3D(Vec3(-m_cameraDistance, 0.f, 0.f)); // IMPORTANT 

	Mat44 cameraToWorld = controlToWorldTransform;
	cameraToWorld.Append(cameraToControlTransform);
	
	camera.SetPosition(cameraToWorld.GetTranslation3D());
	camera.SetOrientation(cameraToWorld.GetEulerAngles());
	camera.SetPerspectiveView(Window::s_mainWindow->GetAspectRatio(), m_cameraFOV, GAME_CAMERA_NEAR, GAME_CAMERA_FAR);

}

void PlayerShip::InitializeParams(InputOutputParams& out_params) const
{
	out_params.m_deltaSeconds = g_theGame->GetDeltaSeconds();



	out_params.m_prevVelocity = m_velocity;
	out_params.m_prevRotation = m_controlFrameRotation;
	out_params.m_prevCameraDistance = m_cameraDistance;
	out_params.m_prevCameraFOV = m_cameraFOV;
	out_params.m_prevBodyLocalPosition = m_bodyLocalPosition;
	out_params.m_prevBodyLocalRotation = m_bodyLocalRotation;

	out_params.m_prevBodyWorldTransform = GetShipModelToWorldTransform();


	out_params.m_linearVelocity = out_params.m_prevVelocity;
	// Redundant Code
	out_params.m_angularVelocity = Vec2::ZERO;
	out_params.m_cameraDistance = out_params.m_prevCameraDistance;
	out_params.m_cameraFOV = out_params.m_prevCameraFOV;
	out_params.m_bodyTargetLocalPosition = out_params.m_prevBodyLocalPosition;
	out_params.m_bodyTargetLocalRotation = out_params.m_prevBodyLocalRotation;

	 

}

void PlayerShip::UpdateInput(InputOutputParams& io_params)
{
	Vec2 rotationInput; // Yaw
	bool isFireButtonDown = false;

	bool useGamepad = g_theGame->IsUsingGamepad();
	if (useGamepad)
	{
		// Throttle Input
		XboxController const& controller = g_theInput->GetController(0);
		Vec2 leftStick = controller.GetLeftStick().GetPosition();
		
		m_throttleInput += leftStick.y * THROTTLE_LEVER_SPEED * io_params.m_deltaSeconds;
		m_throttleInput = GetClampedZeroToOne(m_throttleInput);
		
		// Rotation Input
		Vec2 rightStick = controller.GetRightStick().GetPosition();
		rotationInput = Vec2(-rightStick.x, -rightStick.y);

		// Roll To Horizon
		if (controller.WasButtonJustPressed(XboxButtonId::XBOX_BUTTON_RIGHT_THUMB))
		{
			m_isRollingToHorizon = true;
		}

		// Fire Input
		isFireButtonDown = controller.GetRightTrigger() > 0.f;

	}
	else
	{
		// Throttle Input
		{
			float throttleIntention = 0.f;
			if (g_theInput->IsKeyDown(KEYCODE_W))
			{
				throttleIntention += 1.f;
			}
			if (g_theInput->IsKeyDown(KEYCODE_S))
			{
				throttleIntention -= 1.f;
			}

			m_throttleInput += throttleIntention * THROTTLE_LEVER_SPEED * io_params.m_deltaSeconds;
			m_throttleInput = GetClampedZeroToOne(m_throttleInput);
		}

		// Rotation Input
		{
			Vec2 cursorPositionDelta = g_theInput->GetCursorClientDelta();
			Vec2 normalizedCursorDelta = cursorPositionDelta * (MOUSE_ROTATION_SENSITIVITY / g_screenHeight);
			m_mouseRotationInput += Vec2(normalizedCursorDelta.x, -normalizedCursorDelta.y);
			m_mouseRotationInput.ClampLength(1.f);

			// Dead zone
			float rawRadius = m_mouseRotationInput.GetLength();
			float rawDegrees = m_mouseRotationInput.GetOrientationDegrees();

			float correctedRadius = RangeMapClamped(rawRadius, MOUSE_ROTATION_INPUT_DEADZONE, 1.f, 0.f, 1.f);
			float correctedDegrees = rawDegrees;

			Vec2 rightStick = Vec2::MakeFromPolarDegrees(correctedDegrees, correctedRadius);
			rotationInput = Vec2(-rightStick.x, -rightStick.y);
		}

		// Roll To Horizon
		if (g_theInput->WasKeyJustPressed(KEYCODE_MIDDLE_MOUSE))
		{
			m_isRollingToHorizon = true;
		}

		// Barrel Roll
		if (!m_isBarrelRolling && m_barrelRollCooldownTimer <= 0.f)
		{
			if (g_theInput->WasKeyJustPressed(KEYCODE_A))
			{
				m_isBarrelRolling = true;
				m_barrelRollTotalAngle = -360.f;
				m_barrelRollElapsedTime = 0.f;
			}
			else if (g_theInput->WasKeyJustPressed(KEYCODE_D))
			{
				m_isBarrelRolling = true;
				m_barrelRollTotalAngle = 360.f;
				m_barrelRollElapsedTime = 0.f;
			}
		}

		// Fire Input
		isFireButtonDown = g_theInput->IsKeyDown(KEYCODE_LEFT_MOUSE);
	}


	io_params.m_throttleInput = m_throttleInput;
	io_params.m_rotationInput = rotationInput;
	m_rotationInput = rotationInput; // persist for external readers (HUD tilt, etc.)

	io_params.m_angularVelocity = io_params.m_rotationInput * m_definition->m_turnRate;

	io_params.m_isFiring = isFireButtonDown;
}

void PlayerShip::UpdateTerrainCollision(InputOutputParams& io_params)
{
	constexpr float MIN_DISPLACEMENT_THRESHOLD = 0.001f;

	float collisionRadius = m_definition->m_collisionRadius;

	Mat44 shipToWorld = GetShipModelToWorldTransform();
	Vec3 currentShipWorldPos = shipToWorld.GetTranslation3D();

	Vec3 preShipWorldPos = io_params.m_prevBodyWorldTransform.GetTranslation3D();

	Vec3 displacement = currentShipWorldPos - preShipWorldPos;
	float displacementLength = displacement.GetLength();

	if (displacementLength < MIN_DISPLACEMENT_THRESHOLD)
	{
		return;
	}

	Vec3 moveDir = displacement / displacementLength;

	Mat44 moveBasis = Mat44::MakeFromX(moveDir);
	Vec3 basisI = moveBasis.GetIBasis3D(); // forward
	Vec3 basisJ = moveBasis.GetJBasis3D(); // left
	Vec3 basisK = moveBasis.GetKBasis3D(); // up

	// 5 sample points on the front hemisphere of the sphere (in movement-aligned coords)
	// Center, and 4 points at -30 deg from forward (cos30=0.866, sin30=0.5)
	constexpr int NUM_RAYS = 5;
	Vec3 localOffsets[NUM_RAYS] =
	{
		Vec3(0.f, 0.f, 0.f),                          // center
		Vec3(-0.5f,  0.866025f, 0.f),                 // left-back
		Vec3(-0.5f, -0.866025f, 0.f),                 // right-back
		Vec3(-0.5f, 0.f,  0.866025f),                 // up-back
		Vec3(-0.5f, 0.f, -0.866025f),                 // down-back
	};

	float rayLength = collisionRadius + displacementLength;

	// Find closest impact among the 5 rays
	bool anyHit = false;
	float closestImpactDist = rayLength;
	Vec3 closestImpactNormal;
	Vec3 closestImpactPos;

	for (int i = 0; i < NUM_RAYS; ++i)
	{
		// Convert local offset to world offset: offset is in (I, J, K) basis, scaled by radius
		Vec3 worldOffset = (basisI * localOffsets[i].x + basisJ * localOffsets[i].y + basisK * localOffsets[i].z) * collisionRadius;
		Vec3 rayStart = currentShipWorldPos + worldOffset;

		VoxelRaycastResult3D result = m_world->DoShipTrace(rayStart, moveDir, rayLength);

		if (!result.m_didImpact)
			continue;

		// If impact distance is 0, the ray origin is already inside terrain - skip
		if (result.m_impactDist <= 0.f)
			continue;

		if (result.m_impactDist < closestImpactDist)
		{
			closestImpactDist = result.m_impactDist;
			closestImpactNormal = result.m_impactNormal;
			closestImpactPos = result.m_impactPos;
			anyHit = true;
		}
	}

	if (!anyHit)
	{
		return;
	}

	// The ray length is R + displacementLength
	// If impactDist < rayLength, the ship has penetrated the surface
	// Penetration depth = rayLength - impactDist
	float penetration = rayLength - closestImpactDist;
	if (penetration <= 0.f)
	{
		return;
	}

	// Push the ship back along the movement direction by the penetration amount
	Vec3 correction = moveDir * (-penetration);
	m_controlFramePosition += correction;

	// Velocity reflection: decompose velocity into normal and tangential components
	Vec3 impactNormal = closestImpactNormal.GetNormalized();
	float velocityAlongNormal = DotProduct3D(m_velocity, impactNormal);

	// Only respond if moving into the surface (negative dot = moving towards)
	if (velocityAlongNormal < 0.f)
	{
		Vec3 normalComponent = impactNormal * velocityAlongNormal;
		Vec3 tangentialComponent = m_velocity - normalComponent;

		// Bounce the normal component (flip and scale by bounce coefficient)
		// Reduce the tangential component by friction
		m_velocity = tangentialComponent * m_definition->m_collisionFrictionCoefficient
			- normalComponent * m_definition->m_collisionBounceCoefficient;


		// Hard collision: if normal impact speed exceeds 25% of top speed
		float impactSeverity = fabsf(velocityAlongNormal) / m_definition->m_topSpeed;
		if (impactSeverity > 0.25f)
		{
			m_throttleInput *= 0.1f;

			Vec3 correctedShipWorldPos = GetShipModelToWorldTransform().GetTranslation3D();
			ApplyCollisionAdditiveRotation(io_params, correctedShipWorldPos, closestImpactPos, impactNormal);
			// #ToDo: trigger collision VFX / SFX / camera shake here
		}
	}
}

void PlayerShip::ApplyCollisionAdditiveRotation(InputOutputParams& io_params, Vec3 const& shipWorldPos, Vec3 const& impactPos, Vec3 const& impactNormal)
{
	UNUSED(io_params);
	// Axis = cross(centerToImpact, impactNormal)
	// This gives a rotation axis perpendicular to both, making the ship "flinch" away from the impact
	Vec3 centerToImpact = (impactPos - shipWorldPos).GetNormalized();
	Vec3 axis = CrossProduct3D(centerToImpact, impactNormal);

	float axisLength = axis.GetLength();
	if (axisLength < 0.001f)
	{
		// centerToImpact and impactNormal are nearly parallel, pick a fallback perpendicular axis
		Vec3 forward = m_controlFrameRotation.RotateVector(Vec3::FORWARD).GetNormalized();
		axis = CrossProduct3D(centerToImpact, forward);
		axisLength = axis.GetLength();
		if (axisLength < 0.001f)
		{
			return; // degenerate case, skip
		}
	}
	axis /= axisLength; // normalize

	// Build the world-space additive rotation
	Quat worldAdditiveRotation = Quat::MakeFromAxisAngleDegrees(axis, COLLISION_FLINCH_DEGREES);

	// Convert to control-frame-local rotation:
	// localAdditive = inverse(controlRot) * worldAdditive * controlRot
	Quat controlRotInv = m_controlFrameRotation.GetInverse();
	m_collisionAdditiveRotation = controlRotInv * worldAdditiveRotation * m_controlFrameRotation;
	m_collisionAdditiveRotation.Normalize();

	// Reset alpha to 1 (new collision overrides any in-progress blend-out)
	m_collisionAdditiveAlpha = 1.f;
}

void PlayerShip::BlendOutCollisionAdditiveRotation(InputOutputParams& io_params)
{
	if (m_collisionAdditiveAlpha > 0.f)
	{
		Quat additiveThisFrame = Quat::Slerp(Quat::IDENTITY, m_collisionAdditiveRotation, m_collisionAdditiveAlpha);
		io_params.m_bodyTargetLocalRotation = io_params.m_bodyTargetLocalRotation * additiveThisFrame;
		io_params.m_bodyTargetLocalRotation.Normalize();

		m_collisionAdditiveAlpha -= io_params.m_deltaSeconds * COLLISION_FLINCH_BLEND_OUT_SPEED;
		if (m_collisionAdditiveAlpha < 0.f)
		{
			m_collisionAdditiveAlpha = 0.f;
		}
	}
}

void PlayerShip::UpdateBarrelRoll(InputOutputParams& io_params)
{
	// Tick cooldown
	if (m_barrelRollCooldownTimer > 0.f)
	{
		m_barrelRollCooldownTimer -= io_params.m_deltaSeconds;
	}

	if (!m_isBarrelRolling)
	{
		return;
	}

	// dodgeSign: +360 (right/D) -> -1 (negative Y = right), -360 (left/A) -> +1 (positive Y = left)
	float dodgeSign = (m_barrelRollTotalAngle > 0.f) ? -1.f : 1.f;

	float prevElapsed = m_barrelRollElapsedTime;
	m_barrelRollElapsedTime += io_params.m_deltaSeconds;
	float fraction = GetClampedZeroToOne(m_barrelRollElapsedTime / m_definition->m_barrelRollDuration);

	// Rotation: cumulative angle around body-local forward
	float currentAngleDegrees = m_barrelRollTotalAngle * fraction;
	io_params.m_bodyTargetLocalRotation = io_params.m_bodyTargetLocalRotation * Quat::MakeFromAxisAngleDegrees(Vec3::FORWARD, currentAngleDegrees);
	io_params.m_bodyTargetLocalRotation.Normalize();

	// Visual body lateral offset: sine curve (0 -> peak -> 0)
	float offsetY = dodgeSign * m_definition->m_barrelRollBodyOffset * SinDegrees(fraction * 180.f);
	io_params.m_bodyTargetLocalPosition.y += offsetY;

	// Lateral velocity impulse (first frame only, UpdateLateralDrag will dissipate it) // or put it where activate the ability
	if (prevElapsed == 0.f)
	{
		Vec3 worldDodgeDir = m_controlFrameRotation.RotateVector(Vec3(0.f, dodgeSign, 0.f)).GetNormalized();
		m_velocity += worldDodgeDir * m_definition->m_barrelRollLateralSpeed;
	}
	
	if (fraction >= 1.f)
	{
		m_isBarrelRolling = false;
		m_barrelRollCooldownTimer = m_definition->m_barrelRollCooldown;
	}
}

float PlayerShip::GetDeltaRollDegrees_Uniform(float deltaSeconds)
{
	if (!m_isRollingToHorizon) return 0.f;

	EulerAngles controlFrameOrientation = Mat44::MakeFromUnitQuat(m_controlFrameRotation).GetEulerAngles();
	float rollDegrees = controlFrameOrientation.m_rollDegrees; // -180~180

	float maxDeltaDegrees = deltaSeconds * ROLL_TO_HORIZON_RATE;

	float shortestDisp = GetShortestAngularDispDegrees(rollDegrees, 0.f);

	float deltaRoll = GetClamped(shortestDisp, -maxDeltaDegrees, maxDeltaDegrees);

	if (fabsf(rollDegrees + deltaRoll) < 0.1f)
	{
		m_isRollingToHorizon = false;
	}

	return deltaRoll;
}

float PlayerShip::GetDeltaRollDegrees_Interp(float deltaSeconds)
{
	if (!m_isRollingToHorizon) return 0.f;

	EulerAngles controlFrameOrientation = Mat44::MakeFromUnitQuat(m_controlFrameRotation).GetEulerAngles();
	float rollDegrees = controlFrameOrientation.m_rollDegrees; // -180~180

	float shortestDisp = GetShortestAngularDispDegrees(rollDegrees, 0.f);

	if (fabsf(shortestDisp) < 0.1f)
	{
		m_isRollingToHorizon = false;
		return -rollDegrees;
	}

	float deltaMove = shortestDisp * GetClampedZeroToOne(deltaSeconds * ROLL_TO_HORIZON_INTERP_SPEED);
	return deltaMove;
}

void PlayerShip::UpdateDebugInput()
{
	UpdateDebugPlayerAttributes();
}

void PlayerShip::UpdateDebugPlayerAttributes()
{
	bool changed = false;

	if (g_theInput->WasKeyJustPressed('1'))
	{
		m_playerStrength = 0;
		changed = true;
	}
	else if (g_theInput->WasKeyJustPressed('2'))
	{
		m_playerStrength = 1;
		changed = true;
	}
	else if (g_theInput->WasKeyJustPressed('3'))
	{
		m_playerStrength = 2;
		changed = true;
	}
	else if (g_theInput->WasKeyJustPressed('4'))
	{
		m_playerStrength = 3;
		changed = true;
	}
	else if (g_theInput->WasKeyJustPressed('5'))
	{
		m_playerStrength = 4;
		changed = true;
	}

	if (g_theInput->WasKeyJustPressed('0'))
	{
		m_playerTier = (m_playerTier == 0) ? 1 : 0;
		changed = true;
	}

	if (changed)
	{
		DebugAddMessage(
			Stringf("Player Tier: %d  |  Player Strength: %d", m_playerTier, m_playerStrength), 3.f, Rgba8::BLUE);
	}
}

Mat44 PlayerShip::GetShipModelToWorldTransform() const
{
	// All Quat should be unit.

	Mat44 bodyToControlTransform;
	Mat44 controlToWorldTransform;

	bodyToControlTransform = Mat44::MakeFromUnitQuat(m_bodyLocalRotation);
	bodyToControlTransform.SetTranslation3D(m_bodyLocalPosition);

	controlToWorldTransform = Mat44::MakeFromUnitQuat(m_controlFrameRotation);
	controlToWorldTransform.SetTranslation3D(m_controlFramePosition);

	Mat44 result = controlToWorldTransform;
	result.Append(bodyToControlTransform);
	return result;
}

Vec3 PlayerShip::GetAimedWorldPosition() const
{
	float shootRange = m_definition->m_cannonSpeed * m_definition->m_cannonLifetime; // Hard coded, if has more weapon types, change it 

	Camera const& camera = g_theGame->GetCamera();

	Vec3 forward, left, up;
	camera.GetOrientation().GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

	Vec3 defaultResult = camera.GetPosition() + forward * shootRange;

	if (g_theGame->IsUsingGamepad())
	{
		return defaultResult;
	}


	float const SCREEN_WIDTH = g_screenWidth;
	float const SCREEN_HEIGHT = g_screenHeight;
	Vec2 const SCREEN_CENTER = Vec2(SCREEN_WIDTH, SCREEN_HEIGHT) * 0.5f;

	Vec2 crosshairCenter = SCREEN_CENTER + m_mouseRotationInput * MOUSE_OUTER_RADIUS_RATIO * SCREEN_HEIGHT;
	
	Vec2 clientUV = Vec2(crosshairCenter.x / SCREEN_WIDTH, crosshairCenter.y / SCREEN_HEIGHT);

	Vec3 rayStart;
	Vec3 rayFwdNormal;

	bool ok = camera.ScreenPointToRay(rayStart, rayFwdNormal, clientUV);
	if (ok)
	{
		return rayStart + rayFwdNormal * shootRange;
	}
	else
	{
		return defaultResult;
	}
}

void PlayerShip::UpdateFiring(InputOutputParams& io_params)
{
	m_timeSinceLastFire += io_params.m_deltaSeconds;

	if (!io_params.m_isFiring) return;
	if (m_definition->m_numCannons <= 0) return; // No cannons

	if (m_timeSinceLastFire >= m_definition->m_cannonFireInterval) 
	{
		Mat44 shipToWorldTransform = GetShipModelToWorldTransform();

		Vec3 currentCannonOffset = m_definition->m_cannonOffsets[m_currentCannonIndex];
		Rgba8 currentCannonColor = m_definition->m_cannonColors[m_currentCannonIndex];
		float currentCannonIntensity = m_definition->m_cannonIntensities[m_currentCannonIndex];

		Mat44 cannonToShipTransform = Mat44::MakeTranslation3D(currentCannonOffset);

		Mat44 cannonToWorldTransform = shipToWorldTransform;
		cannonToWorldTransform.Append(cannonToShipTransform);

		Vec3 cannonWorldPos = cannonToWorldTransform.GetTranslation3D();

		//Vec3 forward = m_controlFrameRotation.RotateVector(Vec3::FORWARD).GetNormalized();
		Vec3 aimedWorldPos = GetAimedWorldPosition();


		ProjectileDefinition bulletDef;
		bulletDef.m_startPos = cannonWorldPos;
		bulletDef.m_velocity = (aimedWorldPos - cannonWorldPos).GetNormalized() * m_definition->m_cannonSpeed;
		bulletDef.m_lifetime = m_definition->m_cannonLifetime;
		bulletDef.m_explosionRadius = m_definition->m_cannonExplosionRadius;
		bulletDef.m_color = currentCannonColor;
		bulletDef.m_intensityMultiplier = currentCannonIntensity;
		bulletDef.m_lengthMultiplier = 2.f;
		bulletDef.m_thicknessMultiplier = 1.f;
		bulletDef.m_deltaDensity = m_definition->m_cannonDeltaDensity;
		bulletDef.m_tier = GetPlayerTier();
		bulletDef.m_strength = GetPlayerStrength();

		m_world->m_projectileSystem->SpawnProjectile(bulletDef);


		m_timeSinceLastFire = 0.0f;
		m_currentCannonIndex = (m_currentCannonIndex + 1) % m_definition->m_numCannons;
	}
}

void PlayerShip::UpdateSonarScanning(InputOutputParams& io_params)
{
	m_timeSinceLastScan += io_params.m_deltaSeconds;

	if (g_theInput->WasKeyJustPressed(KEYCODE_E) && m_timeSinceLastScan >= m_definition->m_sonarScanDuration)
	{
		m_timeSinceLastScan = 0.0f;
	}

	float alpha = m_timeSinceLastScan / m_definition->m_sonarScanDuration;


	SonarParams params;
	params.m_sonarCenter = m_controlFramePosition;
	Rgba8::RED.GetAsFloats(params.m_sonarColor);
	params.m_sonarThickness = 3.f;
	if (alpha >= 1.f)
	{
		params.m_sonarInnerRadius = -1.f;
	}
	else
	{
		params.m_sonarInnerRadius = m_definition->m_sonarMaxRadius * SmoothStep3(alpha);
	}

	m_world->UpdateSonar(params);
}

int PlayerShip::GetPlayerTier() const
{
	return m_playerTier;
}

int PlayerShip::GetPlayerStrength() const
{
	return m_playerStrength;
}

void PlayerShip::SetPlayerTier(int tier)
{
	m_playerTier = tier;
}

void PlayerShip::SetPlayerStrength(int strength)
{
	m_playerStrength = strength;
}
