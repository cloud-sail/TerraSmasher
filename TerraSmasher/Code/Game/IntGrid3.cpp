#include "Game/IntGrid3.hpp"
#include "Engine/Math/IntVec3.hpp"

IntGrid3::IntGrid3(int sizeX, int sizeY, int sizeZ)
	: m_sizeX(sizeX)
	, m_sizeY(sizeY)
	, m_sizeZ(sizeZ)
{
}



int IntGrid3::Linearize(const IntVec3& coords) const
{
	return coords.x + coords.y * m_sizeX + coords.z * (m_sizeY * m_sizeX);
}

IntVec3 IntGrid3::Delinearize(int index) const
{
	int xy = m_sizeY * m_sizeX;
	int z = index / xy;
	int rest = index % xy;
	int y = rest / m_sizeX;
	int x = rest % m_sizeX;
	return IntVec3(x, y, z);
}

bool IntGrid3::IsInBounds(const IntVec3& coords) const
{
	return (coords.x >= 0 && coords.x < m_sizeX) &&
		(coords.y >= 0 && coords.y < m_sizeY) &&
		(coords.z >= 0 && coords.z < m_sizeZ);
}

