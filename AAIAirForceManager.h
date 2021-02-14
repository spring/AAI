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
	void CheckTarget(const UnitId& unitId, const AAIUnitCategory& category, float health);

	//! @brief Checks if target is possible bombing target and adds to list of bomb targets (used for buildings e.g. stationary arty, nuke launchers..)
	bool CheckStaticBombTarget(UnitId unitId, UnitDefId unitDefId, const float3& position);	

	//! @brief Removes unit if from list of possible bombing targets
	void RemoveTarget(UnitId unitId);

	// attacks the most promising target
	void BombBestUnit(float cost, float danger);

private:
	//! @brief Returns a group of air units of given type currently occupied with a task of lower priority (or idle) - nullptr if none found
	AAIGroup* GetAirGroup(EUnitType groupType, float importance, int minSize = 1) const;

	//! @brief Returns true if given unit is already in target list
	bool IsTarget(UnitId unitId) const;

	AAI *ai;

	//! Set of possible bombing targets
	std::set<AirRaidTarget*> m_targets;
};

#endif
