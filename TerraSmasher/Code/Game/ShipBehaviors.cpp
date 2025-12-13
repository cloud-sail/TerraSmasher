#include "Game/ShipBehaviors.hpp"
#include "Game/ShipTypes.hpp"
#include "Game/GameCommon.hpp"
#include "Game/ShipDefinition.hpp"
#include "Engine/Math/MathUtils.hpp"

void ShipBehavior::UpdateThrottle(InputOutputParams& io_params, ShipDefinition const& shipDef)
{
	Vec3 forward = io_params.m_prevRotation.RotateVector(Vec3::FORWARD).GetNormalized();
	
	float currentSpeed = DotProduct3D(io_params.m_prevVelocity, forward);
	float targetSpeed = io_params.m_throttleInput * shipDef.m_topSpeed; // >=0

	float speedDiff = targetSpeed - currentSpeed;
	
	// Very close
	if (fabsf(speedDiff) < 0.001f)
	{
		return;
	}

	float finalSpeed = currentSpeed;

	if (speedDiff > 0.f)
	{
		// Accelerate
		float speedRatio = GetClampedZeroToOne(currentSpeed / shipDef.m_topSpeed);
		float timeRatio = shipDef.m_accelerationCurve.EvaluateY(speedRatio);

		float timeStep = io_params.m_deltaSeconds / shipDef.m_secondsToTopSpeed;
		float nextTimeRatio = GetClampedZeroToOne(timeRatio + timeStep);
		float nextSpeedRatio = shipDef.m_accelerationCurve.EvaluateX(nextTimeRatio);

		float deltaSpeed = (nextSpeedRatio - speedRatio) * shipDef.m_topSpeed;

		finalSpeed = currentSpeed + deltaSpeed;
		if (finalSpeed > targetSpeed)
		{
			finalSpeed = targetSpeed;
		}
	}
	else
	{
		// Decelerate
		float speedRatio = GetClampedZeroToOne(currentSpeed / shipDef.m_topSpeed);
		float timeRatio = shipDef.m_decelerationCurve.EvaluateY(speedRatio);

		float timeStep = io_params.m_deltaSeconds / shipDef.m_secondsToStop;
		float nextTimeRatio = GetClampedZeroToOne(timeRatio + timeStep);
		float nextSpeedRatio = shipDef.m_decelerationCurve.EvaluateX(nextTimeRatio);

		float deltaSpeed = (nextSpeedRatio - speedRatio) * shipDef.m_topSpeed; // <= 0

		finalSpeed = currentSpeed + deltaSpeed;
		if (finalSpeed < targetSpeed)
		{
			finalSpeed = targetSpeed;
		}
	}


	io_params.m_linearVelocity += forward * (finalSpeed - currentSpeed);
}

void ShipBehavior::UpdateLateralDrag(InputOutputParams& io_params, ShipDefinition const& shipDef)
{
	Vec3 forward = io_params.m_prevRotation.RotateVector(Vec3::FORWARD).GetNormalized();
	float forwardSpeed = DotProduct3D(io_params.m_prevVelocity, forward);
	Vec3 forwardComponent = forward * forwardSpeed;
	Vec3 lateralComponent = io_params.m_prevVelocity - forwardComponent;

	float dragFactor = shipDef.m_lateralDragRate;
	Vec3 newLateralComponent = InterpTo(lateralComponent, Vec3::ZERO, io_params.m_deltaSeconds, dragFactor);

	Vec3 lateralDelta = newLateralComponent - lateralComponent;
	io_params.m_linearVelocity += lateralDelta;
}

void ShipBehavior::UpdateCameraDistance(InputOutputParams& io_params, ShipDefinition const& shipDef)
{
	Vec3 forward = io_params.m_prevRotation.RotateVector(Vec3::FORWARD).GetNormalized();
	float forwardSpeed = DotProduct3D(io_params.m_prevVelocity, forward);

	float speedRatio = GetClampedZeroToOne(forwardSpeed / shipDef.m_topSpeed);
	float t = SmoothStep3(speedRatio); // Not linear
	float targetCameraDistance = Interpolate(shipDef.m_minCameraDistance, shipDef.m_maxCameraDistance, t);

	io_params.m_cameraDistance = InterpTo(io_params.m_prevCameraDistance, targetCameraDistance, io_params.m_deltaSeconds, CAMERA_DIST_INTERP_SPEED);
}

void ShipBehavior::UpdateCameraFOV(InputOutputParams& io_params, ShipDefinition const& shipDef)
{
	Vec3 forward = io_params.m_prevRotation.RotateVector(Vec3::FORWARD).GetNormalized();
	float forwardSpeed = DotProduct3D(io_params.m_prevVelocity, forward);

	float speedRatio = GetClampedZeroToOne(forwardSpeed / shipDef.m_topSpeed);
	float t = SmoothStep3(speedRatio); // Not linear
	float targetCameraFOV = Interpolate(shipDef.m_minCameraFOV, shipDef.m_maxCameraFOV, t);

	io_params.m_cameraFOV = InterpTo(io_params.m_prevCameraFOV, targetCameraFOV, io_params.m_deltaSeconds, FOV_INTERP_SPEED);
}

void ShipBehavior::UpdateTargetBodyLocalPosition(InputOutputParams& io_params, ShipDefinition const& shipDef)
{
	Vec2 input = -io_params.m_rotationInput;

	//float inputLength = input.GetLength();
	//const float DEAD_ZONE = 0.01f;

	//if (inputLength < DEAD_ZONE)
	//{
	//	io_params.m_bodyTargetLocalPosition = shipDef.m_visualPositionMiddle;
	//	return;
	//}

	Vec3 pos0, pos1, pos2;
	Vec2 p0, p1, p2;

	p0 = Vec2(0.f, 0.f);
	pos0 = shipDef.m_visualPositionMiddle;

	if (input.x >= 0.f && input.y >= 0.f)
	{
		p1 = Vec2(1.f, 0.f);
		p2 = Vec2(0.f, 1.f);
		pos1 = shipDef.m_visualPositionRight;
		pos2 = shipDef.m_visualPositionUp;
	}
	else if (input.x < 0.f && input.y >= 0.f)
	{
		p1 = Vec2(0.f, 1.f);
		p2 = Vec2(-1.f, 0.f);
		pos1 = shipDef.m_visualPositionUp;
		pos2 = shipDef.m_visualPositionLeft;
	}
	else if (input.x < 0.f && input.y < 0.f)
	{
		p1 = Vec2(-1.f, 0.f);
		p2 = Vec2(0.f, -1.f);
		pos1 = shipDef.m_visualPositionLeft;
		pos2 = shipDef.m_visualPositionDown;
	}
	else
	{
		p1 = Vec2(0.f, -1.f);
		p2 = Vec2(1.f, 0.f);
		pos1 = shipDef.m_visualPositionDown;
		pos2 = shipDef.m_visualPositionRight;
	}

	Vec2 clampedInput = input; // not clamped
	//Vec2 clampedInput = GetNearestPointOnTriangle2D(input, p0, p1, p2);

	Vec3 bary = detail::CalculateBarycentricCoordinates(clampedInput, p0, p1, p2);

	// Speed Multiplier
	Vec3 forward = io_params.m_prevRotation.RotateVector(Vec3::FORWARD).GetNormalized();
	float forwardSpeed = DotProduct3D(io_params.m_prevVelocity, forward);
	float speedRatio = GetClampedZeroToOne(forwardSpeed / shipDef.m_topSpeed);
	float t = SmoothStart2(speedRatio);

	float multiplier = Interpolate(1.f, shipDef.m_bodyHorizontalOffsetMultiplier, t);

	io_params.m_bodyTargetLocalPosition = (pos0 * bary.x + pos1 * bary.y + pos2 * bary.z);
	io_params.m_bodyTargetLocalPosition.y *= multiplier;
}

void ShipBehavior::UpdateTargetBodyLocalRotation(InputOutputParams& io_params, ShipDefinition const& shipDef)
{
	Vec2 input = -io_params.m_rotationInput;

	//float inputLength = input.GetLength();
	//const float DEAD_ZONE = 0.0001f;

	//if (inputLength < DEAD_ZONE)
	//{
	//	io_params.m_bodyTargetLocalRotation = shipDef.m_visualRotationMiddle;
	//	return;
	//}

	Quat rot0, rot1, rot2;
	Vec2 p0, p1, p2;

	p0 = Vec2(0.f, 0.f);
	rot0 = shipDef.m_visualRotationMiddle;

	if (input.x >= 0.f && input.y >= 0.f)
	{
		p1 = Vec2(1.f, 0.f);
		p2 = Vec2(0.f, 1.f);
		rot1 = shipDef.m_visualRotationRight;
		rot2 = shipDef.m_visualRotationUp;
	}
	else if (input.x < 0.f && input.y >= 0.f)
	{
		p1 = Vec2(0.f, 1.f);
		p2 = Vec2(-1.f, 0.f);
		rot1 = shipDef.m_visualRotationUp;
		rot2 = shipDef.m_visualRotationLeft;
	}
	else if (input.x < 0.f && input.y < 0.f)
	{
		p1 = Vec2(-1.f, 0.f);
		p2 = Vec2(0.f, -1.f);
		rot1 = shipDef.m_visualRotationLeft;
		rot2 = shipDef.m_visualRotationDown;
	}
	else
	{
		p1 = Vec2(0.f, -1.f);
		p2 = Vec2(1.f, 0.f);
		rot1 = shipDef.m_visualRotationDown;
		rot2 = shipDef.m_visualRotationRight;
	}

	//Vec2 clampedInput = input; // not clamped
	Vec2 clampedInput = GetNearestPointOnTriangle2D(input, p0, p1, p2);

	Vec3 bary = detail::CalculateBarycentricCoordinates(clampedInput, p0, p1, p2);

	io_params.m_bodyTargetLocalRotation = detail::InterpolateQuatWithBarycentric(rot0, rot1, rot2, bary);


}

Vec3 ShipBehavior::detail::CalculateBarycentricCoordinates(Vec2 const& p, Vec2 const& a, Vec2 const& b, Vec2 const& c)
{
	Vec2 v0 = b - a;
	Vec2 v1 = c - a;
	Vec2 v2 = p - a;

	float dot00 = DotProduct2D(v0, v0);
	float dot01 = DotProduct2D(v0, v1);
	float dot11 = DotProduct2D(v1, v1);
	float dot20 = DotProduct2D(v2, v0);
	float dot21 = DotProduct2D(v2, v1);

	float denom = dot00 * dot11 - dot01 * dot01;

	if (fabsf(denom) < 1e-6f)
	{
		return Vec3(0.0f, 0.0f, 0.0f); // Degenerate
	}

	float invDenom = 1.0f / denom;
	float v = (dot11 * dot20 - dot01 * dot21) * invDenom;
	float w = (dot00 * dot21 - dot01 * dot20) * invDenom;
	float u = 1.0f - v - w;

	return Vec3(u, v, w); // (a, b, c)
}

Quat ShipBehavior::detail::InterpolateQuatWithBarycentric(Quat const& q0, Quat const& q1, Quat const& q2, Vec3 const& bary)
{
	float totalWeight = bary.x + bary.y + bary.z;
	if (fabsf(totalWeight) < 1e-6f)
	{
		return q0;
	}

	float u = bary.x / totalWeight; // not necessary?
	float v = bary.y / totalWeight;
	float w = bary.z / totalWeight;

	float t12 = w / (v + w + 1e-6f);
	Quat q12 = Quat::Slerp(q1, q2, t12);

	float t0_12 = (v + w) / (u + v + w + 1e-6f);
	Quat result = Quat::Slerp(q0, q12, t0_12);

	//float t12_0 = (u);
	//Quat result = Quat::Nlerp(q12, q0, t12_0);

	return result;

}

