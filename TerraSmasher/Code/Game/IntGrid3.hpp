#pragma once

struct IntVec3;

// A helper class to save a shape and calculate index
// start from (0,0,0), 
class IntGrid3
{
public:
	IntGrid3() = default;
	IntGrid3(int sizeX, int sizeY, int sizeZ);

	int GetSizeX() const { return m_sizeX; }
	int GetSizeY() const { return m_sizeY; }
	int GetSizeZ() const { return m_sizeZ; }
	int GetCellCount() const { return m_sizeX * m_sizeY * m_sizeZ; }

	int  Linearize(const IntVec3& coords) const;
	IntVec3 Delinearize(int index) const;
	bool IsInBounds(const IntVec3& coords) const;


private:
	int m_sizeX = 1;
	int m_sizeY = 1;
	int m_sizeZ = 1;
};

