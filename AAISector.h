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

#include <list>
#include <vector>
#include <numeric>
using namespace std;

class AAI;
class AAIUnitTable;
class AAIMap;
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
	AAISector();
	~AAISector(void);

	void AddMetalSpot(AAIMetalSpot *spot);
	void FreeMetalSpot(float3 pos, const UnitDef *extractor);
	void Init(AAI *ai, int x, int y, int left, int right, int top, int bottom);

	// adds/removes the sector from base sectors; returns true if succesful
	bool SetBase(bool base);

	//! @brief Returns the number of metal spots in this sector
	int GetNumberOfMetalSpots() const { return metalSpots.size(); };

	//! @brief Returns the number of buildings of the given category in this sector
	int GetNumberOfBuildings(const AAIUnitCategory& category) const { return m_ownBuildingsOfCategory[category.GetArrayIndex()]; };

	//! @brief Resets the number of spotted enemy units
	void ResetSpottedEnemiesData() { std::fill(m_enemyCombatUnits.begin(), m_enemyCombatUnits.end(), 0.0f); }; 

	//! @brief Return the total number of enemy combat units
	float GetTotalEnemyCombatUnits() const { return std::accumulate(m_enemyCombatUnits.begin(), m_enemyCombatUnits.end(), 0.0f); };

	void AddEnemyCombatUnit(const AAICombatUnitCategory& category, float value)  { m_enemyCombatUnits[category.GetArrayIndex()] += value; };

	//! @brief Returns number of enemy units of given category spotted in this sector (float as number decreases over time if sector is not scouted)
	float GetNumberOfEnemyCombatUnits(const AAICombatUnitCategory& category) const  { return m_enemyCombatUnits[category.GetArrayIndex()]; };

	void Update();

	// associates an extractor with a metal spot in that sector
	void AddExtractor(int unit_id, int def_id, float3 *pos);

	// returns buildsite for a unit in that sector (or zerovector if nothing found)
	float3 GetBuildsite(int building, bool water = false);

	// returns a buildsite for a defence building
	float3 GetDefenceBuildsite(int building, UnitCategory category, float terrain_modifier, bool water);
	float3 GetRandomBuildsite(int building, int tries, bool water = false);
	float3 GetCenterBuildsite(int building, bool water = false);
	float3 GetRadarArtyBuildsite(int building, float range, bool water);

	//! @brief Adds building of category to sector
	void AddBuilding(const AAIUnitCategory& category) { m_ownBuildingsOfCategory[category.GetArrayIndex()] += 1; };

	//! @brief Removes building from sector
	void RemoveBuilding(const AAIUnitCategory& category) { m_ownBuildingsOfCategory[category.GetArrayIndex()] -= 1; };

	// returns threat to the sector by a certain category
	float GetThreatBy(UnitCategory category, float learned, float current);
	float GetThreatByID(int combat_cat_id, float learned, float current);

	float GetEnemyDefencePower(float ground, float air, float hover, float sea, float submarine);

	float GetMyDefencePowerAgainstAssaultCategory(int assault_category);

	//! @brief Returns enemy combat power of all known enemy units/stat defences in the sector
	float getEnemyThreatToMovementType(const AAIMovementType& movementType) const;

	// returns combat power of units in that and neighbouring sectors vs combat cat
	float GetEnemyAreaCombatPowerVs(int combat_category, float neighbour_importance);

	//! @brief Updates threat map storing where own buildings/units got killed
	void UpdateThreatValues(const AAIUnitCategory& destroyedCategory, const AAIUnitCategory& attackerCategory);

	// returns lost units in that sector
	float GetLostUnits(float ground, float air, float hover, float sea, float submarine);

	// returns center of the sector
	float3 GetCenter();


	//! @brief Searches for a free position in sector (regardless of continent). Position stored in pos (ZeroVector if none found)
	//!        Returns whether search has been successful.
	bool determineMovePos(float3 *pos);

	//! @brief Searches for a free position in sector on specified continent. Position stored in pos (ZeroVector if none found)
	//!        Returns whether search has been successful.
	bool determineMovePosOnContinent(float3 *pos, int continent);

	// returns true is pos is within sector
	bool PosInSector(float3 *pos);

	// get water/flat ground ratio
	float GetWaterRatio();
	float GetFlatRatio();

	// returns true if sector is connected with a big ocean (and not only a small pond)
	bool ConnectedToOcean();

	// returns min dist to edge in number of sectors
	int GetEdgeDistance();

	// sector x/y index
	int x, y;

	// minimum distance to edge in number of sectors
	int map_border_dist;

	// water and flat terrain ratio
	float flat_ratio;
	float water_ratio;

	// id of the continent of the center of the sector
	int continent;

	// coordinates of the edges
	float left, right, top, bottom;

	// list of all metal spots in the sector
	list<AAIMetalSpot*> metalSpots;

	bool freeMetalSpots;

	int distance_to_base;	// 0 = base, 1 = neighbour to base

	bool interior;			// true if sector is no inner sector

	//! Bitmask storing movement types that may maneuver in this sector
	uint32_t m_suitableMovementTypes;	

	float enemy_structures;
	float own_structures;
	float allied_structures;

	// how many groups got a rally point in that sector
	int rally_points;

	// how many times aai tried to build defences and could not find possible construction site
	int failed_defences;

	// indicates how many times scouts have been sent to another sector
	float last_scout;

	// importance of the sector
	float importance_this_game;
	float importance_learned;

	// how many times ai has been attacked by a certain assault category in this sector
	vector<float> attacked_by_this_game;
	vector<float> attacked_by_learned;

	// how many battles took place in that sector (of each assault category)
	vector<float> combats_this_game;
	vector<float> combats_learned;

	// how many units of certain type recently lost in that sector
	vector<float> lost_units;

	// combat units in the sector
	vector<short> my_combat_units;

	int enemies_on_radar;

	// stores combat power of all stationary defs/combat unit vs different categories
	vector<float> my_stat_combat_power; // 0 ground, 1 air, 2 hover, 3 sea, 4 submarine @todo: Check if hover really makes sense or can be merged with ground
	vector<float> my_mobile_combat_power; // 0 ground, 1 air, 2 hover, 3 sea, 4 submarine, 5 building

	// stores combat power of all stationary enemy defs/combat unit vs different categories
	vector<float> enemy_stat_combat_power; // 0 ground, 1 air, 2 hover, 3 sea, 4 submarine @todo: Check if hover really makes sense or can be merged with ground
	vector<float> enemy_mobile_combat_power; // 0 ground, 1 air, 2 hover, 3 sea, 4 submarine, 5 building
	AAI* Getai() { return ai; }
private:
	float GetEnemyCombatPowerAgainstCombatCategory(int combat_category);

	float GetMyCombatPowerAgainstCombatCategory(int combat_category);

	float GetEnemyCombatPower(float ground, float air, float hover, float sea, float submarine);

	// returns combat power of all own/known enemy units in the sector
	float GetMyCombatPower(float ground, float air, float hover, float sea, float submarine);

	float GetOverallThreat(float learned, float current);

	// returns the category with the weakest defence in comparison with threat
	UnitCategory GetWeakestCategory();

	// returns defence power of all own/known enemy stat defences in the sector
	float GetMyDefencePower(float ground, float air, float hover, float sea, float submarine);

	float GetEnemyDefencePowerAgainstAssaultCategory(int assault_category);

	// helper functions
	void Pos2SectorMapPos(float3 *pos, const UnitDef* def);
	void SectorMapPos2Pos(float3 *pos, const UnitDef* def);
	float3 GetHighestBuildsite(int building);
	void SetCoordinates(int left, int right, int top, int bottom);
	void SetGridLocation(int x, int y);
	AAIMetalSpot* GetFreeMetalSpot();

	// gets rectangle for possible buildsite
	void GetBuildsiteRectangle(int *xStart, int *xEnd, int *yStart, int *yEnd);

	AAI *ai;

	//! Number of own buildings of each category in the sector
	std::vector<int> m_ownBuildingsOfCategory;

	//! Number of spotted enemy combat units (float values as number decays over time)
	std::vector<float> m_enemyCombatUnits; // 0 ground, 1 air, 2 hover, 3 sea, 4 submarine
};

#endif

