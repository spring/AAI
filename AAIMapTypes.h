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
	void Init(int xMapSize, int yMapSize)
	{ 
		m_xDefenceMapSize = xMapSize/defenceMapResolution;
		m_yDefenceMapSize = yMapSize/defenceMapResolution;
		m_defenceMaps.resize(AAITargetType::numberOfMobileTargetTypes, std::vector<float>(m_xDefenceMapSize*m_yDefenceMapSize, 0.0f) );
	}

	//! @brief Return the defence map value of a given map position
	float GetValue(MapPos mapPosition, const AAITargetType& targetType) const 
	{
		const int tileIndex = mapPosition.x/defenceMapResolution + m_xDefenceMapSize * (mapPosition.y/defenceMapResolution);
		return m_defenceMaps[targetType.GetArrayIndex()][tileIndex];
	}

	//! @brief Modifies tiles within range of given position by combat power values
	//!        Used to add or remove defences
	void ModifyTiles(const float3& position, float maxWeaponRange, const UnitFootprint& footprint, const AAICombatPower& combatPower, bool addValues)
	{
		// decide which function shall be used to modify tile values
		void (AAIDefenceMaps::*modifyDefenceMapTile) (int , const AAICombatPower& ) = addValues ? &AddDefence : &RemoveDefence;

		const int range = static_cast<int>(maxWeaponRange) / (SQUARE_SIZE * defenceMapResolution);
		const int xPos  = static_cast<int>(position.x) / (SQUARE_SIZE * defenceMapResolution) + footprint.xSize/defenceMapResolution;
		const int yPos  = static_cast<int>(position.z) / (SQUARE_SIZE * defenceMapResolution) + footprint.ySize/defenceMapResolution;

		// x range will change from line to line -  y range is const
		const int yStart = std::max(yPos - range, 0);
		const int yEnd   = std::min(yPos + range, m_yDefenceMapSize);

		for(int y = yStart; y < yEnd; ++y)
		{
			// determine x-range
			const int xRange = (int) floor( fastmath::apxsqrt2( (float) ( std::max(1, range * range - (y - yPos) * (y - yPos)) ) ) + 0.5f );

			const int xStart = std::max(xPos - xRange, 0);
			const int xEnd   = std::min(xPos + xRange, m_xDefenceMapSize);

			for(int x = xStart; x < xEnd; ++x)
			{
				const int tile = x + m_xDefenceMapSize*y;
				(this->*modifyDefenceMapTile)(tile, combatPower);
			}
		}
	}

private:
	//! @brief Adds combat power values to given tile
	void AddDefence(int tile, const AAICombatPower& combatPower)
	{
		m_defenceMaps[AAITargetType::surfaceIndex][tile]   += combatPower.GetCombatPowerVsTargetType(ETargetType::SURFACE);
		m_defenceMaps[AAITargetType::airIndex][tile]       += combatPower.GetCombatPowerVsTargetType(ETargetType::AIR);
		m_defenceMaps[AAITargetType::floaterIndex][tile]   += combatPower.GetCombatPowerVsTargetType(ETargetType::FLOATER);
		m_defenceMaps[AAITargetType::submergedIndex][tile] += combatPower.GetCombatPowerVsTargetType(ETargetType::SUBMERGED);
	}

	//! @brief Removes combat power values to given tile
	void RemoveDefence(int tile, const AAICombatPower& combatPower)
	{
		m_defenceMaps[AAITargetType::surfaceIndex][tile]   -= combatPower.GetCombatPowerVsTargetType(ETargetType::SURFACE);
		m_defenceMaps[AAITargetType::airIndex][tile]       -= combatPower.GetCombatPowerVsTargetType(ETargetType::AIR);
		m_defenceMaps[AAITargetType::floaterIndex][tile]   -= combatPower.GetCombatPowerVsTargetType(ETargetType::FLOATER);
		m_defenceMaps[AAITargetType::submergedIndex][tile] -= combatPower.GetCombatPowerVsTargetType(ETargetType::SUBMERGED);

		for(int targetTypeIndex = 0; targetTypeIndex < AAITargetType::numberOfMobileTargetTypes; ++targetTypeIndex)
		{
			if(m_defenceMaps[targetTypeIndex][tile] < 0.0f)
				m_defenceMaps[targetTypeIndex][tile] = 0.0f;
		}
	}

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
	void Init(int xMapSize, int yMapSize, int losMapResolution)
	{ 
		m_losToScoutMapResolution = losMapResolution / scoutMapResolution;
		m_xScoutMapSize           = xMapSize/scoutMapResolution;
		m_yScoutMapSize           = yMapSize/scoutMapResolution;

		m_scoutedUnitsMap.resize(m_xScoutMapSize*m_yScoutMapSize, 0);
		m_lastUpdateInFrameMap.resize(m_xScoutMapSize*m_yScoutMapSize, 0);
	}

	//! @brief Converts given build map coordinate to scout map coordinate
	int BuildMapToScoutMapCoordinate(int buildMapCoordinate) const { return buildMapCoordinate/scoutMapResolution; }

	//! @brief Converts given scout map coordinate to build map coordinate
	int ScoutMapToBuildMapCoordinate(int scoutMapCoordinate) const { return scoutMapCoordinate*scoutMapResolution; }

	//! @brief Returns id of unit at given tile
	int GetUnitAt(int x, int y) const { return m_scoutedUnitsMap[x + y * m_xScoutMapSize]; }

	//! @brief Adds unit to tile
	void AddEnemyUnit(UnitDefId defId, ScoutMapTile tile)
	{
		m_scoutedUnitsMap[tile.m_tileIndex] = defId.id;
	}

	//! @brief Erases the given tiles
	void ResetTiles(int xLosMap, int yLosMap, int frame)
	{
		int tileIndex = xLosMap*m_losToScoutMapResolution + yLosMap*m_losToScoutMapResolution * m_xScoutMapSize;

		for(int y = 0; y < m_losToScoutMapResolution; ++y)
		{
			for(int x = 0; x < m_losToScoutMapResolution; ++x)
			{
				m_scoutedUnitsMap[tileIndex]      = 0;
				m_lastUpdateInFrameMap[tileIndex] = frame;

				++tileIndex;
			}

			tileIndex += (m_xScoutMapSize-m_losToScoutMapResolution);
		}
	}

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
	void UpdateSectorWithScoutedUnits(AAISector *sector, int xSectorSizeMap, int ySectorSizeMap )
	{
		const int xStart = (sector->x * xSectorSizeMap) / scoutMapResolution;
		const int yStart = (sector->y * ySectorSizeMap) / scoutMapResolution;
		int tileIndex = xStart + yStart * m_xScoutMapSize;

		const int xCells = xSectorSizeMap/scoutMapResolution;
		const int yCells = ySectorSizeMap/scoutMapResolution;

		for(int y = 0; y < yCells; ++y)
		{
			for(int x = 0; x < xCells; ++x)
			{
				//const int tileIndex = x + y * m_xScoutMapSize;
				const UnitDefId unitDefId(m_scoutedUnitsMap[tileIndex]);

				if(unitDefId.IsValid())
					 sector->AddScoutedEnemyUnit(unitDefId, m_lastUpdateInFrameMap[tileIndex]);
				
				++tileIndex;
			}

			tileIndex += (m_xScoutMapSize-xCells);
		}
	}

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

#endif
