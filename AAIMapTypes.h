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

#endif
