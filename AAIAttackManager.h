// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_ATTACKMANAGER_H
#define AAI_ATTACKMANAGER_H

#include "aidef.h"
#include "AAITypes.h"
#include <set>
#include <list>
#include <vector>

class AAI;
class AAIAttack;
class AAISector;

class AAIAttackManager
{
public:
	AAIAttackManager(AAI *ai);
	~AAIAttackManager(void);

	//! @brief Checks all active attacks whether they should be aborted or continue with a different destination
	void Update();

	//! @brief Stops the given attack if it is no longer reasonable (because of lacking combat power or attacking units)
	//!        Returns whether attack has been aborted.
	bool AbortAttackIfFailed(AAIAttack *attack);

	//! @brief Checks if attack can be continued with new target or aborts attack otherwise
	void AttackNextSectorOrAbort(AAIAttack *attack);

	//! @brief Returns a sector to proceed with attack (nullptr if none found)
	const AAISector* GetNextAttackDest(const AAISector *currentSector, const AAIValuesForMobileTargetTypes& targetTypeOfUnits, AAIMovementType moveTypeOfUnits) const;

	//! @brief Checks if units in given combat unit groups have sufficient attack power against enemy stationary defences
	bool SufficientCombatPowerToAttackSector(const AAISector *sector, const std::set<AAIGroup*>& combatGroups, float aggressiveness) const;

	//! @brief Checks if units have sufficient combat power against mobile enemy units assumed to be at destination
	bool SufficientCombatPowerAt(const AAISector *dest, const std::set<AAIGroup*>& combatGroups, float aggressiveness) const;

private:
	//! @brief Determines which groups would be available for an attack globally/on each continent and returns the total number of available assault groups
	int DetermineCombatUnitGroupsAvailableForattack(std::list<AAIGroup*>& availableAssaultGroupsGlobal, std::list<AAIGroup*>& availableAAGroupsGlobal,
													std::vector< std::list<AAIGroup*> >& availableAssaultGroupsOnContinent, std::vector< std::list<AAIGroup*> >& availableAAGroupsOnContinent) const;

	//! @brief Determines the combat power against the different target types for the given list of groups
	void DetermineCombatPowerOfGroups(const std::list<AAIGroup*>& groups, std::vector<float>& combatPower, std::vector<float>& numberOfGroupsOfTargetType) const;

	//! @brief Checks which combat unit groups are available for to attack a target (for each continent), 
	//!        selects a possible target and launches attack if it seems reasonable (i.e. sufficient combat power available)
	void TryToLaunchAttack(int availableAttackId);

	//! @brief Stops the attack and removes it from the list of active attacks
	void AbortAttack(AAIAttack* attack);

	//! The currently active attacks (nullptr if no active attack)
	std::vector<AAIAttack*> m_activeAttacks;

	AAI *ai;
};

#endif
