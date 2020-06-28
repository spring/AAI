// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_BRAIN_H
#define AAI_BRAIN_H

class AAI;
class AAIExecute;
class AIIMap;
class AAISector;

#include "aidef.h"
#include "AAIUnitTypes.h"
#include "AAIBuildTable.h"

enum SectorType {UNKNOWN_SECTOR, LAND_SECTOR, LAND_WATER_SECTOR, WATER_SECTOR};


class AAIBrain
{
public:
	AAIBrain(AAI *ai);
	~AAIBrain(void);

	// adds/removes sector to the base
	void AddSector(AAISector *sector);
	void RemoveSector(AAISector *sector);

	// returns dest attack sector
	AAISector* GetAttackDest(bool land, bool water);

	// returns a sector to proceed with attack
	AAISector* GetNextAttackDest(AAISector *current_sector, bool land, bool water);

	// checks for new neighbours (and removes old ones if necessary)
	void UpdateNeighbouringSectors();

	// recalculates the center of the base
	void UpdateBaseCenter();

	// updates max units spotted
	void UpdateMaxCombatUnitsSpotted(vector<unsigned short>& units_spotted);

	void UpdateAttackedByValues();

	void AttackedBy(int combat_category_id);

	// recalculates def capabilities of all units
	void UpdateDefenceCapabilities();

	//! @brief Adds the combat power of the given unit type to the global defence capabilities 
	void AddDefenceCapabilities(UnitDefId unitDefId);

	// returns pos where scout schould be sent to
	void GetNewScoutDest(float3 *dest, int scout);

	// adds new sectors to base
	bool ExpandBase(SectorType sectorType);

	// returns how much ressources can be spent for unit construction atm
	float Affordable();

	// returns true if commander is allowed for construction at the specified position in the sector
	bool CommanderAllowedForConstructionAt(AAISector *sector, float3 *pos);

	// returns true if AAI may build a mex in this sector (e.g. safe sector)
	bool MexConstructionAllowedInSector(AAISector *sector);

	void DefendCommander(int attacker);

	void BuildUnits();

	// returns game period
	int GetGamePeriod();

	void UpdatePressureByEnemy();

	// returns the probability that units of specified combat category will be used to attack (determine value with respect to game period, current and learning data)
	float GetAttacksBy(int combat_category, int game_period);

	//  0 = sectors the ai uses to build its base, 1 = direct neighbours etc.
	vector<list<AAISector*> > sectors;

	// ratio of  flat land/water cells in all base sectors
	float baseLandRatio;
	float baseWaterRatio;

	int max_distance;

	// center of base (mean value of centers of all base sectors)
	float3 base_center;

	// are there any free metal spots within the base
	bool freeBaseSpots;
	bool expandable;

	// holding max number of units of a category spotted at the same time
	vector<float> max_combat_units_spotted;

	// current estimations of game situation , values ranging from 0 (min) to 1 max

	float enemy_pressure_estimation;	// how much pressure done to the ai by enemy units

	// pos where com spawned
	float3 start_pos;

private:
	// returns true if sufficient ressources to build unit are availbale
	bool RessourcesForConstr(int unit, int workertime = 175);

	// returns true if enough metal for constr.
	bool MetalForConstr(int unit, int workertime = 175);

	// returns true if enough energy for constr.
	bool EnergyForConstr(int unit, int wokertime = 175);

	// returns true if sector is considered to be safe
	bool IsSafeSector(AAISector *sector);

	void BuildCombatUnitOfCategory(const AAICombatCategory& unitCategory, const CombatPower& combatCriteria, bool urgent);

	bool SectorInList(list<AAISector*> mylist, AAISector *sector);
	list<AAISector*> GetSectors();
	vector<float> defence_power_vs;

	//! Counter by what enemy unit category own units/buidlings have been killed (counter is decreasing over time)
	std::vector<float> recentlyAttackedByCategory;

	AAI *ai;
};

#endif

