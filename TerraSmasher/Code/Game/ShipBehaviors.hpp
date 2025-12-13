#pragma once

struct InputOutputParams;
class ShipDefinition;
struct Vec2;
struct Vec3;
struct Quat;

namespace ShipBehavior
{
	void UpdateThrottle(InputOutputParams& io_params, ShipDefinition const& shipDef);
	void UpdateLateralDrag(InputOutputParams& io_params, ShipDefinition const& shipDef);
	void UpdateCameraDistance(InputOutputParams& io_params, ShipDefinition const& shipDef);
	void UpdateCameraFOV(InputOutputParams& io_params, ShipDefinition const& shipDef);

	void UpdateTargetBodyLocalPosition(InputOutputParams& io_params, ShipDefinition const& shipDef);
	void UpdateTargetBodyLocalRotation(InputOutputParams& io_params, ShipDefinition const& shipDef);

	namespace detail
	{
		Vec3 CalculateBarycentricCoordinates(Vec2 const& p, Vec2 const& a, Vec2 const& b, Vec2 const& c);

		Quat InterpolateQuatWithBarycentric(Quat const& q0, Quat const& q1, Quat const& q2, Vec3 const& bary);

	}
}

	// #ToDo Collision Response
