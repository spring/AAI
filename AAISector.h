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

	//! @brief Associates an extractor with a metal spot in that sector 
	void AddExtractor(UnitId unitId, UnitDefId unitDefId, const float3& pos);

	//! @brief Looks for metal spot that corresponds to given position and marks it as free
	void FreeMetalSpot(float3 pos, const springLegacyAI::UnitDef *extractor);

	//! Update if there are still empty metal spots in the sector
	void UpdateFreeMetalSpots();

	void Init(AAI *ai, int x, int y);

	//! @brief Loads sector data from given file
	void LoadDataFromFile(FILE* file);

	//! @brief Saves sector data to given file
	void SaveDataToFile(FILE* file);

	//! @brief Updates learning data for sector
	void UpdateLearnedData();

	//! @brief Adds/removes the sector from base sectors; returns true if succesful
	bool AddToBase(bool addToBase);

	//! @brief Returns the distance (in sectors) to the base
	int GetDistanceToBase() const { return m_distanceToBase; }

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
	void AddScoutedEnemyUnit(UnitDefId enemyDefId, int framesSinceLastUpdate);

	//! @brief Return the total number of enemy combat units
	float GetTotalEnemyCombatUnits() const { return m_enemyCombatUnits.CalcuateSum(); };

	//! @brief Returns whether sector is supsected to be occupied by enemy units
	bool IsOccupiedByEnemies() const{ return (GetTotalEnemyCombatUnits() > 0.1f) || (m_enemyBuildings > 0) || (m_enemyUnitsDetectedBySensor > 0); }

	//! @brief Returns number of enemy units of given target type spotted in this sector (float as number decreases over time if sector is not scouted)
	float GetNumberOfEnemyCombatUnits(const AAITargetType& targetType) const  { return m_enemyCombatUnits.GetValue(targetType); };
	const TargetTypeValues& GetNumberOfEnemyCombatUnits() const  { return m_enemyCombatUnits; };

	//! @brief Decreases number of lost units by a factor < 1 such that AAI "forgets" about lost unit over time
	void DecreaseLostUnits();

	//! @brief Returns whether sector can be considered for expansion of base
	bool IsSectorSuitableForBaseExpansion() const;

	//! @brief Returns true if sector shall be considered for selection of construction of further metal extractor
	bool ShallBeConsideredForExtractorConstruction() const;

	//! @brief Returns a buildsite that has been chosen randomly (the given number of trials) - ZeroVector if none found
	float3 GetRandomBuildsite(UnitDefId buildingDefId, int trials) const;

	float3 GetRadarArtyBuildsite(int building, float range, bool water);

	//! @brief Returns position of known enemy buildings (or center if no buidlings in sector)
	float3 DetermineAttackPosition() const;

	//! @brief Adds building of category to sector
	void AddBuilding(const AAIUnitCategory& category) { m_ownBuildingsOfCategory[category.GetArrayIndex()] += 1; };

	//! @brief Removes building from sector
	void RemoveBuilding(const AAIUnitCategory& category) { m_ownBuildingsOfCategory[category.GetArrayIndex()] -= 1; };

	//! @brief Returns true if local combat power does not suffice to defend vs attack by given target type
	bool IsSupportNeededToDefenceVs(const AAITargetType& targetType) const;

	//! @brief Returns how often units in sector have been attacked by given mobile target type
	float GetLocalAttacksBy(const AAITargetType& targetType, float previousGames, float currentGame) const;

	//! @brief Get total (mobile + static) defence power of enemy vs given target type (according to spotted units)
	float GetEnemyCombatPowerVsUnits(const MobileTargetTypeValues& unitsOfTargetType) const;

	//! @brief Get total (mobile + static) defence power vs given target type
	float GetEnemyCombatPower(const AAITargetType& targetType) const { return m_enemyStaticCombatPower.GetValueOfTargetType(targetType) + m_enemyMobileCombatPower.GetValueOfTargetType(targetType); }

	//! @brief Returns combat power of own/allied static defences against given target type
	float GetFriendlyStaticDefencePower(const AAITargetType& targetType) const { return m_friendlyStaticCombatPower.GetValueOfTargetType(targetType); }

	//! @brief Returns cmbat power of own/allied static defences against given target type
	float GetFriendlyCombatPower(const AAITargetType& targetType) const { return m_friendlyStaticCombatPower.GetValueOfTargetType(targetType) + m_friendlyMobileCombatPower.GetValueOfTargetType(targetType); }

	//! @brief Returns combat power of units in that and neighbouring sectors vs combat cat
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
		return m_attacksByTargetTypeInCurrentGame.CalculateSum();
	}

	//! @brief Returns number of attacks by the main combat categories (ground, hover, air)
	float GetTotalAttacksInPreviousGames() const 
	{
		return m_attacksByTargetTypeInPreviousGames.CalculateSum();
	}

	//! @brief Returns center position of the sector
	float3 GetCenter() const;

	//! @brief Returns the continent ID of the center of the sector
	int GetContinentID() const { return m_continentId; }

	//! @brief Returns the ratio of flat terrain tiles in this sector
	float GetFlatTilesRatio() const { return m_flatTilesRatio; }

	//! @brief Returns the ratio of water tiles in this sector
	float GetWaterTilesRatio() const { return m_waterTilesRatio; }

	//! @brief Increments corresponding counter (used to avoid trying to build static defences in a sector with no suitable buildsites)
	void FailedToConstructStaticDefence() { ++m_failedAttemptsToConstructStaticDefence; }

	//! @brief Returns the importance of a static defence against the target type with highest priority
	float GetImportanceForStaticDefenceVs(AAITargetType& targetType, const GamePhase& gamePhase, float previousGames, float currentGame);

	//! @brief Returns the rating of this sector as destination to attack (0.0f if no suitable target)
	float GetAttackRating(const AAISector* currentSector, bool landSectorSelectable, bool waterSectorSelectable, const MobileTargetTypeValues& targetTypeOfUnits) const;

	//! @brief Returns the rating of this sector as destination to attack (0.0f if no suitable target)
	float GetAttackRating(const std::vector<float>& globalCombatPower, const std::vector< std::vector<float> >& continentCombatPower, const MobileTargetTypeValues& assaultGroupsOfType, float maxLostUnits) const;

	//! @brief Returns rating as next destination for scout of given movement type
	float GetRatingAsNextScoutDestination(const AAIMovementType& scoutMoveType, const float3& currentPositionOfScout);

	//! @brief Returns the rating to be selected for a rally point for units of given movement type
	float GetRatingForRallyPoint(const AAIMovementType& moveType, int continentId) const;

	//! @brief Returns the rating as starting sector (if a new one has to be selected as the current is already occupied by other AAI player)
	float GetRatingAsStartSector() const;

	//! @brief Returns rating as sector to build a power plant
	float GetRatingForPowerPlant(float weightPreviousGames, float weightCurrentGame) const;

	//! @brief Shall be called when scout is sent to this sector (resets counter how often this sector has been skipped)
	void SelectedAsScoutDestination() { m_skippedAsScoutDestination = 0; }
	
	//! @brief Searches for a free position in sector on specified continent (use -1 if continent does not matter). 
	//!        Returns position or ZeroVector if none found.
	float3 DetermineUnitMovePos(AAIMovementType moveType, int continentId) const;

	//! @brief Returns true if pos lies within this sector
	bool PosInSector(const float3& pos) const;

	//! @brief Determines ratio of water cells of this sector
	float DetermineWaterRatio() const;

	//! @brief Determines ratio of flat cells of this sector
	float DetermineFlatRatio() const;

	//! @brief Returns true if sector is connected with a big ocean (and not only a small pond)
	bool ConnectedToOcean() const;

	//! @brief Returns minimum distance to one of the map edges (in sector sizes)
	int GetEdgeDistance() const { return m_minSectorDistanceToMapEdge; }

	//! @brief Determines rectangle for possible buildsite
	void DetermineBuildsiteRectangle(int *xStart, int *xEnd, int *yStart, int *yEnd) const;

	// sector x/y index
	int x, y;

	// list of all metal spots in the sector
	std::list<AAIMetalSpot*> metalSpots;

	bool m_freeMetalSpots;

	// importance of the sector
	float importance_this_game;
	float importance_learned;

private:

	//! @brief Helper function to determine position to move units to
	bool IsValidMovePos(const float3& pos, BuildMapTileType forbiddenMapTileTypes, int continentId) const;

	//! @brief Returns true if further static defences may be built in this sector
	bool AreFurtherStaticDefencesAllowed() const;

	AAI *ai;

	//! Id of the continent of the center of the sector
	int m_continentId;

	//! Ratio of flat terrain tiles
	float m_flatTilesRatio;

	//! Ratio of water tiles
	float m_waterTilesRatio;

	//! Distance (in sectors) to own base,  i.e 0 = belongs to base, 1 = neighbour to base, ...
	int m_distanceToBase;

	//! Bitmask storing movement types that may maneuver in this sector
	uint32_t m_suitableMovementTypes;	

	//! Minimum distance to one of the map edges (in sector sizes)
	int m_minSectorDistanceToMapEdge;

	//! How many non air units have recently been lost in that sector (float as the number decays over time)
	float m_lostUnits;

	//! How many air units have recently been lost in that sector (float as the number decays over time)
	float m_lostAirUnits;

	//! Number of own buildings of each category in the sector
	std::vector<int> m_ownBuildingsOfCategory;

	//! Number of spotted enemy combat units (float values as number decays over time)
	TargetTypeValues m_enemyCombatUnits; // 0 surface, 1 air, 3 ship, 4 submarine, 5 static defences

	//! Number of buildings enemy players have constructed in this sector
	int m_enemyBuildings;

	//! Number of buildings allied players have constructed in this sector
	int m_alliedBuildings;

	//! Number of enemy units detected by sensor (radar/sonar)
	int m_enemyUnitsDetectedBySensor;

	//! The combat power against mobile targets of all hostile static defences in this sector
	MobileTargetTypeValues m_enemyStaticCombatPower;
	
	//! The combat power against mobile targets of all hostile combat units in this sector
	MobileTargetTypeValues m_enemyMobileCombatPower;

	//! The combat power against mobile targets of all friendly static defences in this sector
	MobileTargetTypeValues m_friendlyStaticCombatPower;

	//! The combat power against mobile targets of all friendly combat units in this sector
	MobileTargetTypeValues m_friendlyMobileCombatPower;

	//! Stores how often buildings in this sector have been attacked(=destroyed) by a certain target type in previous games
	MobileTargetTypeValues m_attacksByTargetTypeInPreviousGames;

	//! Stores how often buildings in this sector have been attacked(=destroyed) by a certain target type in the current game
	MobileTargetTypeValues m_attacksByTargetTypeInCurrentGame;

	//! indicates how many times scouts have been sent to another sector
	int m_skippedAsScoutDestination;

	//! How many times AAI tried to build defences in this sector but failed (because of unavailable buildsite)
	int m_failedAttemptsToConstructStaticDefence;
};

#endif

