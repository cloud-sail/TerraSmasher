#include "Game/GameVertexUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/MathUtils.hpp"

void AddGameVertsForIsoscelesTriangle2D(std::vector<Vertex_PCU>& verts, Vec2 const& base, Vec2 const& tip, float halfWidth, Rgba8 const& color)
{
	if (base == tip)
	{
		return;
	}

	Vec2 tipDisplacement = tip - base;
	Vec2 leftDisplacement = tipDisplacement.GetRotated90Degrees();
	leftDisplacement.SetLength(halfWidth);
	Vec2 rightDisplacement = -leftDisplacement;

	Vec2 left = base + leftDisplacement;
	Vec2 right = base + rightDisplacement;

	verts.emplace_back(Vertex_PCU(Vec3(left.x, left.y, 0.f), color, Vec2::ZERO));
	verts.emplace_back(Vertex_PCU(Vec3(right.x, right.y, 0.f), color, Vec2::ZERO));
	verts.emplace_back(Vertex_PCU(Vec3(tip.x, tip.y, 0.f), color, Vec2::ZERO));
}
