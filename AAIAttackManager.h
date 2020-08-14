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
#include <set>
#include <list>
#include <vector>

using namespace std;

class AAI;
class AAIBrain;
class AAIBuildTable;
class AAIMap;
class AAIAttack;
class AAISector;


class AAIAttackManager
{
public:
	AAIAttackManager(AAI *ai);
	~AAIAttackManager(void);

	void CheckAttack(AAIAttack *attack);

	// true if units have sufficient combat power to face mobile units in dest
	bool SufficientCombatPowerAt(const AAISector *dest, const std::set<AAIGroup*>& combatGroups, float aggressiveness) const;

	// true if combat groups have sufficient attack power to face stationary defences
	bool SufficientAttackPowerVS(AAISector *dest, const std::set<AAIGroup*>& combatGroups, float aggressiveness) const;

	void GetNextDest(AAIAttack *attack);

	void Update(int numberOfContinents);

private:
	//! @brief Determines which groups would be available for an attack globally/on each continent and returns the total number of available assault groups
	int DetermineCombatUnitGroupsAvailableForattack(std::list<AAIGroup*>& availableAssaultGroupsGlobal, std::list<AAIGroup*>& availableAAGroupsGlobal,
													std::vector< std::list<AAIGroup*> >& availableAssaultGroupsOnContinent, std::vector< std::list<AAIGroup*> >& availableAAGroupsOnContinent) const;

	//! @brief Determines the combat power against the different target types for the given list of groups
	void DetermineCombatPowerOfGroups(const std::list<AAIGroup*>& groups, std::vector<float>& combatPower, std::vector<float>& numberOfGroupsOfTargetType) const;

	void TryToLaunchAttack(int numberOfContinents);

	void StopAttack(AAIAttack *attack);

	std::list<AAIAttack*> attacks;

	AAI *ai;
};

#endif
