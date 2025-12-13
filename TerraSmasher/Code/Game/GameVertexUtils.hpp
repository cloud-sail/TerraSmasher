#pragma once
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/AABB2.hpp"
#include <vector>

void AddGameVertsForIsoscelesTriangle2D(std::vector<Vertex_PCU>& verts, Vec2 const& base, Vec2 const& tip, float halfWidth, Rgba8 const& color);
