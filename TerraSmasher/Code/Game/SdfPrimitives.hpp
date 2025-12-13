#pragma once

struct Vec3;

// https://iquilezles.org/articles/distfunctions/

namespace SdfHelper
{
	float Sphere(Vec3 p, float r);
	float Box(Vec3 p, Vec3 h);
	float Ellipsoid(Vec3 p, Vec3 r);

	float Torus(Vec3 p, float majorRadius, float minorRadius);
	float Capsule(Vec3 p, Vec3 a, Vec3 b, float r);

	float CutSphere(Vec3 p, float r, float h);
	float CutHollowSphere(Vec3 p, float r, float h, float t);
	// #ToDo Onion
}