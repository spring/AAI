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
	AAIBrain(AAI *ai, int maxSectorDistanceToBase);
	~AAIBrain(void);

	const SmoothedData& GetSmoothedMetalSurplus() const { return m_metalSurplus; }

	float GetBaseFlatLandRatio() const { return m_baseFlatLandRatio; }

	float GetBaseWaterRatio() const { return m_baseWaterRatio; }

	const float3& GetCenterOfBase() const { return m_centerOfBase; }

	//! @brief Adds/removes the given sector to the base
	void AssignSectorToBase(AAISector *sector, bool addToBase);

	// returns dest attack sector
	AAISector* GetAttackDest(bool land, bool water);

	// returns a sector to proceed with attack
	AAISector* GetNextAttackDest(AAISector *current_sector, bool land, bool water);

	//! @brief Updates the (smoothened) energy/metal income
	void UpdateRessources(springLegacyAI::IAICallback* cb);

	//! @brief Updates the maximum number of spotted combat units for each category (old max values decrease over time)
	void UpdateMaxCombatUnitsSpotted(const std::vector<int>& spottedCombatUnits);

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

	void DefendCommander(int attacker);

	void BuildUnits();

	void UpdatePressureByEnemy();

	// returns the probability that units of specified combat category will be used to attack (determine value with respect to game period, current and learning data)
	float GetAttacksBy(int combat_category, int game_period);

	//  0 = sectors the ai uses to build its base, 1 = direct neighbours etc.
	vector<list<AAISector*> > sectors;

	// are there any free metal spots within the base
	bool m_freeMetalSpotsInBase;

	// holding max number of units of a category spotted at the same time
	vector<float> max_combat_units_spotted;

	// current estimations of game situation , values ranging from 0 (min) to 1 max
	float enemy_pressure_estimation;	// how much pressure done to the ai by enemy units

private:
	//! @brief Checks for new neighbours (and removes old ones if necessary)
	void UpdateNeighbouringSectors();

	//! @brief Recalculates the center of the base (needs to be called after sectors have been added or removed)
	void UpdateCenterOfBase();

	// returns true if sufficient ressources to build unit are availbale
	bool RessourcesForConstr(int unit, int workertime = 175);

	// returns true if enough metal for constr.
	bool MetalForConstr(int unit, int workertime = 175);

	// returns true if enough energy for constr.
	bool EnergyForConstr(int unit, int wokertime = 175);

	void BuildCombatUnitOfCategory(const AAICombatCategory& unitCategory, const CombatPower& combatCriteria, bool urgent);

	vector<float> defence_power_vs;

	//! Ratio of cells with flat land of all base sectors (ranging from 0 (none) to 1(all))
	float m_baseFlatLandRatio;

	//! Ratio of cells with water of all base sectors (ranging from 0 (none) to 1(all))
	float m_baseWaterRatio;

	//! Center of base (mean value of centers of all base sectors)
	float3 m_centerOfBase;

	//! Counter by what enemy unit category own units/buidlings have been killed (counter is decreasing over time)
	std::vector<float> m_recentlyAttackedByCategory;

	//! Average metal surplus over the last AAIConfig::INCOME_SAMPLE_POINTS frames
	SmoothedData m_metalSurplus;

	//! Average energy surplus over the last AAIConfig::INCOME_SAMPLE_POINTS frames
	SmoothedData m_energySurplus;

	//! Average metal income over the last AAIConfig::INCOME_SAMPLE_POINTS frames
	SmoothedData m_metalIncome;

	//! Average energy income over the last AAIConfig::INCOME_SAMPLE_POINTS frames
	SmoothedData m_energyIncome;

	AAI *ai;
};

#endif

