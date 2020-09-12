// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_SECTOR_H
#define AAI_SECTOR_H


#include "System/float3.h"
#include "aidef.h"
#include "AAITypes.h"
#include "AAIUnitTypes.h"
#include "AAIBuildTree.h"

#include <list>
#include <vector>
#include <numeric>

class AAI;
class AAIUnitTable;
class AAIMap;
class BuildMapTileType;
class AAIMetalSpot;

namespace springLegacyAI {
	struct UnitDef;
}
using namespace springLegacyAI;

enum Direction {WEST, EAST, SOUTH, NORTH, CENTER, NO_DIRECTION};

struct DefenceCoverage
{
	Direction direction;
	float defence;
};


class AAISector
{
public:
	friend AAIMap;

	AAISector();
	~AAISector(void);

	//! @brief Adds a metal spot to the list of metal spots in the sector
	void AddMetalSpot(AAIMetalSpot *spot);

	//! @brief Looks for metal spot that corresponds to given position and marks it as free
	void FreeMetalSpot(float3 pos, const UnitDef *extractor);

	//! Update if there are still empty metal spots in the sector
	void UpdateFreeMetalSpots();

	//! @brief Associates an extractor with a metal spot in that sector 
	void AddExtractor(int unit_id, int def_id, float3 *pos);

	void Init(AAI *ai, int x, int y);

	//! @brief Loads sector data from given file
	void LoadDataFromFile(FILE* file);

	//! @brief Saves sector data to given file
	void SaveDataToFile(FILE* file);

	//! @brief Updates learning data for sector
	void UpdateLearnedData();

	// adds/removes the sector from base sectors; returns true if succesful
	bool SetBase(bool base);

	//! @brief Returns the number of metal spots in this sector
	int GetNumberOfMetalSpots() const { return metalSpots.size(); }

	//! @brief Returns the number of buildings of the given category in this sector
	int GetNumberOfBuildings(const AAIUnitCategory& category) const { return m_ownBuildingsOfCategory[category.GetArrayIndex()]; }

	//! @brief Returns the number of buildings belonging to allied players 
	int GetNumberOfAlliedBuildings() const { return m_alliedBuildings; }

	//! @brief Returns the number of buildings belonging to hostile players 
	int GetNumberOfEnemyBuildings() const { return m_enemyBuildings; }

	//! @brief Resets the own combat power / number of allied buildings
	void ResetLocalCombatPower();

	//! @brief Adds an allied bulding to corresponding counter, adds combat power of any friendly units or static defences to respective combat power
	void AddFriendlyUnitData(UnitDefId unitDefId, bool unitBelongsToAlly);

	//! @brief Resets the number / combat power of spotted enemy units
	void ResetScoutedEnemiesData();

	//! @brief Updates enemy combat power and counters
	void AddScoutedEnemyUnit(UnitDefId enemyDefId, int lastUpdateInFrame);

	//! @brief Return the total number of enemy combat units
	float GetTotalEnemyCombatUnits() const { return std::accumulate(m_enemyCombatUnits.begin(), m_enemyCombatUnits.end(), 0.0f); };

	//! @brief Returns whether sector is supsected to be occupied by enemy units
	bool IsOccupiedByEnemies() const{ return (GetTotalEnemyCombatUnits() > 0.1f) || (m_enemyBuildings > 0) || (m_enemyUnitsDetectedBySensor > 0); }

	//! @brief Returns number of enemy units of given category spotted in this sector (float as number decreases over time if sector is not scouted)
	float GetNumberOfEnemyCombatUnits(const AAICombatUnitCategory& category) const  { return m_enemyCombatUnits[category.GetArrayIndex()]; };

	//! @brief Decreases number of lost units by a factor < 1 such that AAI "forgets" about lost unit over time
	void DecreaseLostUnits();

	// returns a buildsite for a defence building
	float3 GetDefenceBuildsite(UnitDefId buildingDefId, const AAITargetType& targetType, float terrainModifier, bool water) const ;

	float3 GetRandomBuildsite(int building, int tries, bool water = false);

	float3 GetRadarArtyBuildsite(int building, float range, bool water);

	//! @brief Returns position of known enemy buildings (or center if no buidlings in sector)
	float3 DetermineAttackPosition() const;

	//! @brief Adds building of category to sector
	void AddBuilding(const AAIUnitCategory& category) { m_ownBuildingsOfCategory[category.GetArrayIndex()] += 1; };

	//! @brief Removes building from sector
	void RemoveBuilding(const AAIUnitCategory& category) { m_ownBuildingsOfCategory[category.GetArrayIndex()] -= 1; };

	//! @brief Returns how often units in sector have been attacked by given mobile target type
	float GetLocalAttacksBy(const AAITargetType& targetType, float previousGames, float currentGame) const;

	//! @brief Get total (mobile + static) defence power of enemy vs given target type (according to spotted units)
	float GetEnemyDefencePower(const AAIValuesForMobileTargetTypes& targetTypeOfUnits) const;

	//! @brief Get total (mobile + static) defence power vs given target type
	float GetEnemyCombatPower(const AAITargetType& targetType) const { return m_enemyStaticCombatPower.GetValueOfTargetType(targetType) + m_enemyMobileCombatPower.GetValueOfTargetType(targetType); }

	//! @brief Returns cmbat power of own/allied static defences against given target type
	float GetFriendlyStaticDefencePower(const AAITargetType& targetType) const { return m_friendlyStaticCombatPower.GetValueOfTargetType(targetType); }

	// returns combat power of units in that and neighbouring sectors vs combat cat
	float GetEnemyAreaCombatPowerVs(const AAITargetType& targetType, float neighbourImportance) const;

	//! @brief Updates threat map storing where own buildings/units got killed
	void UpdateThreatValues(UnitDefId destroyedDefId, UnitDefId attackerDefId);

	//! @brief Returns lost units in that sector
	float GetLostUnits() const { return (m_lostUnits + m_lostAirUnits); }

	//! @brief Returns lost airunits in that sector
	float GetLostAirUnits() const { return m_lostAirUnits; }

	//! @brief Returns number of attacks by the main combat categories (ground, hover, air)
	float GetTotalAttacksInThisGame() const 
	{
		return    m_attacksByTargetTypeInCurrentGame.GetValueOfTargetType(ETargetType::SURFACE)
				+ m_attacksByTargetTypeInCurrentGame.GetValueOfTargetType(ETargetType::AIR)
				+ m_attacksByTargetTypeInCurrentGame.GetValueOfTargetType(ETargetType::FLOATER)
				+ m_attacksByTargetTypeInCurrentGame.GetValueOfTargetType(ETargetType::SUBMERGED);
	}

	//! @brief Returns center position of the sector
	float3 GetCenter() const;

	//! @brief Returns the rating of this sector as destination to attack (0.0f if no suitable target)
	float GetAttackRating(const AAISector* currentSector, bool landSectorSelectable, bool waterSectorSelectable, const AAIValuesForMobileTargetTypes& targetTypeOfUnits) const;

	//! @brief Returns rating as next destination for scout of given movement type
	float GetRatingAsNextScoutDestination(const AAIMovementType& scoutMoveType, const float3& currentPositionOfScout);

	//! @brief Shall be called when scout is sent to this sector (resets counter how often this sector has been skipped)
	void SelectedAsScoutDestination() { m_skippedAsScoutDestination = 0; }
	
	//! @brief Searches for a free position in sector on specified continent (use -1 if continent does not matter). 
	//!        Position stored in pos (ZeroVector if none found). Returns whether search has been successful.
	bool DetermineUnitMovePos(float3& pos, AAIMovementType moveType, int continentId) const;

	//! @brief Returns true if pos lies within this sector
	bool PosInSector(const float3& pos) const;

	//! @brief Determines ratio of water cells of this sector
	float DetermineWaterRatio() const;

	//! @brief Determines ratio of flat cells of this sector
	float DetermineFlatRatio() const;

	// returns true if sector is connected with a big ocean (and not only a small pond)
	bool ConnectedToOcean();

	//! @brief Returns minimum distance to one of the map edges (in sector sizes)
	int GetEdgeDistance() const { return m_minSectorDistanceToMapEdge; }

	//! @brief Determines rectangle for possible buildsite
	void DetermineBuildsiteRectangle(int *xStart, int *xEnd, int *yStart, int *yEnd) const;

	// sector x/y index
	int x, y;

	// water and flat terrain ratio
	float flat_ratio;
	float water_ratio;

	// id of the continent of the center of the sector
	int continent;

	// coordinates of the edges
	float left, right, top, bottom;

	// list of all metal spots in the sector
	std::list<AAIMetalSpot*> metalSpots;

	bool m_freeMetalSpots;

	int distance_to_base;	// 0 = base, 1 = neighbour to base

	//! Bitmask storing movement types that may maneuver in this sector
	uint32_t m_suitableMovementTypes;	

	// how many groups got a rally point in that sector
	int rally_points;

	// how many times aai tried to build defences and could not find possible construction site
	int failed_defences;

	// importance of the sector
	float importance_this_game;
	float importance_learned;

private:

	//! @brief Helper function to determine position to move units to
	bool IsValidMovePos(const float3& pos, BuildMapTileType forbiddenMapTileTypes, int continentId) const;

	AAI *ai;

	//! Minimum distance to one of the map edges (in sector sizes)
	int m_minSectorDistanceToMapEdge;

	//! How many non air units have recently been lost in that sector (float as the number decays over time)
	float m_lostUnits;

	//! How many air units have recently been lost in that sector (float as the number decays over time)
	float m_lostAirUnits;

	//! Number of own buildings of each category in the sector
	std::vector<int> m_ownBuildingsOfCategory;

	//! Number of spotted enemy combat units (float values as number decays over time)
	std::vector<float> m_enemyCombatUnits; // 0 ground, 1 air, 2 hover, 3 sea, 4 submarine

	//! Number of buildings enemy players have constructed in this sector
	int m_enemyBuildings;

	//! Number of buildings allied players have constructed in this sector
	int m_alliedBuildings;

	//! Number of enemy units detected by sensor (radar/sonar)
	int m_enemyUnitsDetectedBySensor;

	//! The combat power against mobile targets of all hostile static defences in this sector
	AAIValuesForMobileTargetTypes m_enemyStaticCombatPower;
	
	//! The combat power against mobile targets of all hostile combat units in this sector
	AAIValuesForMobileTargetTypes m_enemyMobileCombatPower;

	//! The combat power against mobile targets of all hostile static defences in this sector
	AAIValuesForMobileTargetTypes m_friendlyStaticCombatPower;

	//! Stores how often buildings in this sector have been attacked(=destroyed) by a certain target type in previous games
	AAIValuesForMobileTargetTypes m_attacksByTargetTypeInPreviousGames;

	//! Stores how often buildings in this sector have been attacked(=destroyed) by a certain target type in the current game
	AAIValuesForMobileTargetTypes m_attacksByTargetTypeInCurrentGame;

	//! indicates how many times scouts have been sent to another sector
	int m_skippedAsScoutDestination;
};

#endif

