// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_AIRFORCEMANAGER_H
#define AAI_AIRFORCEMANAGER_H

#include <set>
#include "System/float3.h"
#include "aidef.h"
#include "AAIUnitTypes.h"

class AAI;

class AirRaidTarget
{
public:
	AirRaidTarget(UnitId unitId, UnitDefId unitDefId, const float3& position) : m_unitId(unitId), m_unitDefId(unitDefId), m_position(position) { }

	const UnitId&    GetUnitId()    const { return m_unitId; }

	const UnitDefId& GetUnitDefId() const { return m_unitDefId; }

	const float3&    GetPosition()  const { return m_position; }

private:
	//! The unit id of the target
	UnitId    m_unitId;

	//! The unit def id of the target
	UnitDefId m_unitDefId;

	// The position of the target
	float3    m_position;
};

class AAIAirForceManager
{
public:
	AAIAirForceManager(AAI *ai);
	~AAIAirForceManager(void);

	//! @brief Checks if a certain unit is worth attacking it and tries to order air units to do it
	void CheckTarget(const UnitId& unitId, const AAITargetType& targetType, float health);

	//! @brief Checks if target is possible bombing target and adds to list of bomb targets (used for buildings e.g. stationary arty, nuke launchers..)
	bool CheckIfStaticBombTarget(UnitId unitId, UnitDefId unitDefId, const float3& position);

	//! @brief Checks all current bomb targets if they are still valid
	void CheckStaticBombTargets();

	//! @brief Removes unit if from list of possible bombing targets
	void RemoveTarget(UnitId unitId);

	//! @brief Returns percentage of detected targets for bombing runs ranging form 0 (none) to 1 (maximum number of targets detected)
	float GetNumberOfBombTargets() const;

	//! @brief Tries to bomb the most promising target
	void BombBestTarget(float danger);

	//! @brief Searches for next suitable target for the given group to attack
	void FindNextBombTarget(AAIGroup* group);

private:
	//! @brief Selects the best target from the given list
	AirRaidTarget* SelectBestTarget(std::set<AirRaidTarget*>& targetList, float danger, int availableBombers, const float3& position);

	//! @brief Returns a group of air units of given type currently occupied with a task of lower priority (or idle) - nullptr if none found
	AAIGroup* GetAirGroup(const AAITargetType& targetType, float minCombatPower, float importance) const;

	//! @brief Determines the maximum number of bombers currently available
	int DetermineMaximumNumberOfAvailableBombers(float importance) const;

	AAI *ai;

	//! Set of possible bombing targets belonging to the enemies economy
	std::set<AirRaidTarget*> m_economyTargets;

	//! Set of possible bombing targets of high military value (static long range artillery, missile launchers)
	std::set<AirRaidTarget*> m_militaryTargets;
};

#endif
