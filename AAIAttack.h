// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_ATTACK_H
#define AAI_ATTACK_H

#include <set>
#include "AAITypes.h"

class AAI;
class AAISector;
class AAIGroup;

class AAIAttack
{
public:
	AAIAttack(AAI *ai);
	~AAIAttack(void);

	void AddGroup(AAIGroup *group);

	void RemoveGroup(AAIGroup *group);

	// returns true if attack has failed
	bool Failed();

	//! @brief Orders units to attack specidied sector
	void AttackSector(const AAISector *sector);

	//! @brief Orders all units involved to retreat
	void StopAttack();

	//! @brief Returns the movement types of the units participating in this attack
	AAIMovementType GetMovementTypeOfAssignedUnits() const;

	//! @brief Determines how many units of which target type participate in attack
	void DetermineTargetTypeOfInvolvedUnits(AAIValuesForMobileTargetTypes& targetTypesOfUnits) const;

	// target sector
	const AAISector* m_attackDestination;

	// tick when last attack order has been given (to prevent overflow when unit gets stuck and sends "unit idel" all the time)
	int lastAttack;

	// groups participating
	std::set<AAIGroup*> combat_groups;
private:
	std::set<AAIGroup*> aa_groups;

	std::set<AAIGroup*> arty_groups;
	
	AAI *ai;
};

#endif
