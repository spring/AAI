// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_MAP_TYPES_H
#define AAI_MAP_TYPES_H

#include "AAIUnitTypes.h"
#include "AAISector.h"
#include "AAIMapRelatedTypes.h"
#include <vector>

//! The map storing which sector has been taken (as base) by which AAI team. Used to avoid that multiple AAI instances expand 
//! into the same sector or build defences in the sector of an allied player.
class AAITeamSectorMap
{
public:
	AAITeamSectorMap() {}
	
	//! @brief Initializes all sectors as unoccupied
	void Init(int xSectors, int ySectors) { m_teamMap.resize(xSectors, std::vector<int>(ySectors, sectorUnoccupied) ); }

	//! Returns whether sector has been occupied by any AAI player (allied, enemy, or own instance)
	bool IsSectorOccupied(int x, int y)            const { return (m_teamMap[x][y] != sectorUnoccupied); }

	//! @brief Returns true is sector is occupied by given team
	bool IsOccupiedByTeam(int x, int y, int team) const { return (m_teamMap[x][y] == team); }

	//! @brief Returns true if sector is occupied by a team other than the given one
	bool IsOccupiedByOtherTeam(int x, int y, int team) const { return (m_teamMap[x][y] != team) && (m_teamMap[x][y] != sectorUnoccupied);}

	//! @brief Returns the team that currently occupied the given sector
	int GetTeam(int x, int y) const { return m_teamMap[x][y]; }

	//! @brief Set sector as occupied by given (ally) team
	void SetSectorAsOccupiedByTeam(int x, int y, int team) { m_teamMap[x][y] = team; }

	//! @brief Set sector as unoccupied
	void SetSectorAsUnoccupied(int x, int y) { m_teamMap[x][y] = sectorUnoccupied; }

private:
	//! Stores the number of ai player which has taken that sector (-1 if none)
	std::vector< std::vector<int> > m_teamMap;	

	//! Valuefor unoccupied sector
	static constexpr int sectorUnoccupied = -1;
};

//! The defence map stores how well a certain map tile is covered by static defences
class AAIDefenceMaps
{
public:
	//! @brief Initializes all sectors as unoccupied
	void Init(int xMapSize, int yMapSize);

	//! @brief Return the defence map value of a given map position
	float GetValue(MapPos mapPosition, const AAITargetType& targetType) const 
	{
		const int tileIndex = mapPosition.x/defenceMapResolution + m_xDefenceMapSize * (mapPosition.y/defenceMapResolution);
		return m_defenceMaps[targetType.GetArrayIndex()][tileIndex];
	}

	//! @brief Modifies tiles within range of given position by combat power values
	//!        Used to add or remove defences
	void ModifyTiles(const float3& position, float maxWeaponRange, const UnitFootprint& footprint, const TargetTypeValues& combatPower, bool addValues);

private:
	//! @brief Adds combat power values to given tile
	void AddDefence(int tile, const TargetTypeValues& combatPower);

	//! @brief Removes combat power values to given tile
	void RemoveDefence(int tile, const TargetTypeValues& combatPower);

	//! The maps itself
	std::vector< std::vector<float> > m_defenceMaps;

	//! Horizontal size of the defence map
	int m_xDefenceMapSize;
	
	//! Vertical size of the defence map
	int m_yDefenceMapSize;

	//! Lower resolution factor with respect to map resolution
	static constexpr int defenceMapResolution = 4;
};

//! This type is used to access a specific tile of a scout map
class ScoutMapTile
{
	friend class AAIScoutedUnitsMap;

public:
	ScoutMapTile(int tileIndex) : m_tileIndex(tileIndex) {}

	bool IsValid() const { return (m_tileIndex >= 0); }

private:
	//! The index of the tile
	int m_tileIndex;
};

//! This map stores the id of scouted units
class AAIScoutedUnitsMap
{
public:
	//! @brief Initializes all tiles as empty
	void Init(int xMapSize, int yMapSize, int losMapResolution);

	//! @brief Converts given build map coordinate to scout map coordinate
	int BuildMapToScoutMapCoordinate(int buildMapCoordinate) const { return buildMapCoordinate/scoutMapResolution; }

	//! @brief Converts given scout map coordinate to build map coordinate
	int ScoutMapToBuildMapCoordinate(int scoutMapCoordinate) const { return scoutMapCoordinate*scoutMapResolution; }

	//! @brief Returns id of unit at given tile
	int GetUnitAt(int x, int y) const { return m_scoutedUnitsMap[x + y * m_xScoutMapSize]; }

	//! @brief Adds unit to tile
	void AddEnemyUnit(UnitDefId defId, ScoutMapTile tile) { m_scoutedUnitsMap[tile.m_tileIndex] = defId.id; }

	//! @brief Erases the given tiles
	void ResetTiles(int xLosMap, int yLosMap, int frame);

	//! @brief Return tile index to corresponding position (int unit coordinates)
	ScoutMapTile GetScoutMapTile(const float3& position) const
	{
		const int xPos = static_cast<int>(position.x) / (scoutMapResolution * SQUARE_SIZE);
		const int yPos = static_cast<int>(position.z) / (scoutMapResolution * SQUARE_SIZE);

		if( (xPos >= 0) && (xPos < m_xScoutMapSize) && (yPos >= 0) && (yPos < m_yScoutMapSize) )
			return ScoutMapTile(xPos + yPos * m_xScoutMapSize);
		else
			return ScoutMapTile(-1);	
	}

	//! @brief Updates the scouted units within the given sector
	void UpdateSectorWithScoutedUnits(AAISector *sector, std::vector<int>& buildingsOnContinent, int currentFrame);

private:
	//! The map containing the unit definition id of a scouted unit on occupying this tile (or 0 if none)
	std::vector<int> m_scoutedUnitsMap;

	//! The map storing the frame of the last update of each tile
	std::vector<int> m_lastUpdateInFrameMap;

	//! Horizontal size of the scouted units map
	int m_xScoutMapSize;
	
	//! Vertical size of the scouted units map
	int m_yScoutMapSize;

	//! Factor how much larger the resolution of the scout map is compared to the LOS map
	int m_losToScoutMapResolution;

	//! Lower resolution factor with respect to map resolution
	static constexpr int scoutMapResolution = 2;
};

//! This class stores the continent map
class AAIContinentMap
{
public:
	//! @brief Initializes all tiles as not belonging to any continent
	void Init(int xMapSize, int yMapSize);

	//! @brief Loads continent map from given file
	void LoadFromFile(FILE* file);

	//! @brief Stores continent map to given file
	void SaveToFile(FILE* file);

	//! @brief Returns the id of continent the cell belongs to
	int GetContinentID(const MapPos& mapPosition) const { return m_continentMap[(mapPosition.y/continentMapResolution) * m_xContMapSize + mapPosition.x / continentMapResolution]; }

	//! @brief Returns the id of continent the given position belongs to
	int GetContinentID(const float3& pos) const;

	//! @brief Returns the number of tiles of the continent map
	int GetSize() const { return m_xContMapSize * m_yContMapSize; }

	//! @brief Determines the continents, i.e. which parts of the map are connected
	void DetectContinents(std::vector<AAIContinent>& continents, const float *heightMap, const int xMapSize, const int yMapSize);

private:
	//! @brief Helper function for detection of continents - checks if a given tile belongs to a continent and sets values accordingly
	void CheckIfTileBelongsToLandContinent(int continentMapTileIndex, float tileHeight, std::vector<AAIContinent>& continents, int continentId, std::vector<int>* nextEdgeCells);

	//! @brief Helper function for detection of continents - checks if a given tile belongs to a sea continent and sets values accordingly
	void CheckIfTileBelongsToSeaContinent(int continentMapTileIndex, float tileHeight, std::vector<AAIContinent>& continents, int continentId, std::vector<int>* nextEdgeCells);

	//! Id of continent a map tile belongs to
	std::vector<int> m_continentMap;

	//! x size of the continent map (1/4 resolution of map)
	int m_xContMapSize;

	//! y size of the continent map (1/4 resolution of map)
	int m_yContMapSize;

	//! Lower resolution factor with respect to map resolution
	static constexpr int continentMapResolution = 4;
};

#endif
