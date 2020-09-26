// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_MAP_RELATED_TYPES_H
#define AAI_MAP_RELATED_TYPES_H

#include "Sim/Misc/GlobalConstants.h"

class AAIMap;

//! A position in map coordinates
struct MapPos
{
	MapPos(int xPos, int yPos) : x(xPos), y(yPos) {}

	MapPos() : MapPos(0,0) {}

	int x;
	int y;
};

//! A continent is made up of  tiles of the same type (land or water) that are connected with each other
struct AAIContinent
{
	//! Continent id
	int id;

	//! Size of continent (oin number of map tiles)
	int size;

	//! Flag if its a water continent
	bool water;
};

//! Movement types that are used to describe the movement type of every unit
enum class EBuildMapTileType : uint8_t
{
	NOT_SET       = 0x00u, //!< Unknown/not set
	LAND          = 0x01u, //!< land tile
	WATER         = 0x02u, //!< water tile
	FLAT          = 0x04u, //!< flat terrain (i.e. suitable for contruction of buildings or destination to send units to))
	CLIFF         = 0x08u, //!< cliffy terrain (i.e. not suitable for contruction of building or destination to send units to)
	FREE          = 0x10u, //!< free (i.e. buildings cand be constructed here)
	OCCUPIED      = 0x20u, //!< occupied by buidling
	BLOCKED_SPACE = 0x40u, //!< tiles where no buildings shall be constructed  (e.g. exits of factory)
};

//! Contains convenience functions for tiles of th buildmap
class BuildMapTileType
{
friend AAIMap;

public:
	BuildMapTileType(EBuildMapTileType tileType) { m_tileType = static_cast<uint8_t>(tileType); }

	BuildMapTileType() : BuildMapTileType(EBuildMapTileType::NOT_SET) {}

	BuildMapTileType(EBuildMapTileType tileType1, EBuildMapTileType tileType2) { m_tileType = static_cast<uint8_t>(tileType1) | static_cast<uint8_t>(tileType2); }

	void SetTileType(EBuildMapTileType tileType) { m_tileType |= static_cast<uint8_t>(tileType); }

	bool IsTileTypeSet(BuildMapTileType tileType) const { return static_cast<bool>(m_tileType & tileType.m_tileType); }

	bool IsTileTypeNotSet(BuildMapTileType tileType) const { return !static_cast<bool>(m_tileType & tileType.m_tileType); }

	void BlockTile()
	{
		m_tileType &= ~static_cast<uint8_t>(EBuildMapTileType::FREE);
		m_tileType |= static_cast<uint8_t>(EBuildMapTileType::BLOCKED_SPACE); 
	}

	void OccupyTile()
	{
		m_tileType &= ~static_cast<uint8_t>(EBuildMapTileType::FREE);
		m_tileType |= static_cast<uint8_t>(EBuildMapTileType::OCCUPIED); 
	}

	void FreeTile()
	{ 
		m_tileType &= ~(static_cast<uint8_t>(EBuildMapTileType::OCCUPIED) + static_cast<uint8_t>(EBuildMapTileType::BLOCKED_SPACE)); 
		m_tileType |= static_cast<uint8_t>(EBuildMapTileType::FREE); 
	}

//private:
	uint8_t m_tileType;
};

//! This class provides mapping between map coordinates (used by spring engine) and other, lower resolution maps used by AAI
class MapCoordinates
{
public:
	MapCoordinates() : m_resolution(1), m_xSize(0), m_ySize(0) {} 
	
	void Init(int resolution, int xMapSize, int yMapSize)
	{
		m_resolution = resolution;
		m_xSize = xMapSize / resolution;
		m_ySize = yMapSize / resolution;
	}

	int GetNumberOfTiles() const { return m_xSize*m_ySize; }

	int GetTileIndex(int x, int y) const { return  x + y * m_xSize; }

	bool AreCoordinatesValid(int x, int y) const { return (x >= 0) && (x < m_xSize) && (y >= 0) && (y < m_ySize); }

	int GetCoordinateFromUnitPos(float pos) const { return static_cast<int>(pos) / (m_resolution * SQUARE_SIZE); }

public:
	//! Resolution with respect to build map (i.e. map size defined by GetMapWidth() & GetMapHeight() callback) where values > 1 mean lower resolution of this map type compared to buildmap
	int m_resolution;

	//! Number of tiles in x-direction
	int m_xSize;

	//! number of tiles in y-direction (AAI internal nomenclature, equals z-direction in spring
	int m_ySize;
};

#endif