#include "Game/GameTransform.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/AABB3.hpp"
#include <float.h>
#include <algorithm>

Vec3 GameTransform::TransformWorldToLocal(Vec3 const& worldPos) const
{
	Mat44 tr = m_orientation.GetAsMatrix_IFwd_JLeft_KUp();
	tr.SetTranslation3D(m_position);
	Mat44 inverseTR = tr.GetOrthonormalInverse();

	Vec3 result = inverseTR.TransformPosition3D(worldPos);

	return result / m_uniformScale;

}

AABB3 GameTransform::TransformLocalToWorldAABB(AABB3 const& localAABB) const
{
	Vec3 corners[8];
	localAABB.GetCornerPoints(corners);

	Mat44 localToWorld = m_orientation.GetAsMatrix_IFwd_JLeft_KUp();
	localToWorld.AppendScaleUniform3D(m_uniformScale);
	localToWorld.SetTranslation3D(m_position);

	Vec3 worldMin = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
	Vec3 worldMax = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for (int i = 0; i < 8; i++) 
	{
		Vec3 worldCorner = localToWorld.TransformPosition3D(corners[i]);

		worldMin.x = std::min(worldMin.x, worldCorner.x);
		worldMin.y = std::min(worldMin.y, worldCorner.y);
		worldMin.z = std::min(worldMin.z, worldCorner.z);

		worldMax.x = std::max(worldMax.x, worldCorner.x);
		worldMax.y = std::max(worldMax.y, worldCorner.y);
		worldMax.z = std::max(worldMax.z, worldCorner.z);
	}

	return AABB3(worldMin, worldMax);
}

Mat44 GameTransform::GetAsMatrix() const
{
	Mat44 localToWorld = m_orientation.GetAsMatrix_IFwd_JLeft_KUp();
	localToWorld.AppendScaleUniform3D(m_uniformScale);
	localToWorld.SetTranslation3D(m_position);
	return localToWorld;
}
