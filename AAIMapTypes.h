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

#endif
