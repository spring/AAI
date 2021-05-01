// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "AAIAttackManager.h"
#include "AAI.h"
#include "AAIBrain.h"
#include "AAIAttack.h"
#include "AAIConfig.h"
#include "AAIGroup.h"
#include "AAIMap.h"
#include "AAISector.h"

AAIAttackManager::AAIAttackManager(AAI *ai) :
	ai(ai),
	m_activeAttacks(AAIConstants::maxNumberOfAttacks, nullptr)
{
}

AAIAttackManager::~AAIAttackManager(void)
{
	for(auto attack = m_activeAttacks.begin(); attack != m_activeAttacks.end(); ++attack)
	{
		if(*attack)
			delete (*attack);
	}

	m_activeAttacks.clear();
}

void AAIAttackManager::Update(AAIThreatMap& threatMap)
{
	int availableAttackId(-1);

	for(int attackId = 0; attackId < m_activeAttacks.size(); ++attackId)
	{
		AAIAttack* attack = m_activeAttacks[attackId];

		if(attack)
		{
			// drop failed attacks
			if( AbortAttackIfFailed(attack) )
				availableAttackId = attackId;
			// check if sector cleared
			else if( attack->HasTargetBeenCleared() )
				AttackNextSectorOrAbort(attack);
		}
		else
			availableAttackId = attackId;
	}

	// at least one attack id is available -> check if new attack should be launched
	if(availableAttackId >= 0)
		TryToLaunchAttack(availableAttackId, threatMap);
}

void AAIAttackManager::TryToLaunchAttack(int availableAttackId, AAIThreatMap& threatMap)
{
	//////////////////////////////////////////////////////////////////////////////////////////////
	// get all available combat/aa/arty groups for attack
	//////////////////////////////////////////////////////////////////////////////////////////////
	
	const int numberOfContinents( AAIMap::GetNumberOfContinents() );
	std::vector< std::list<AAIGroup*> > availableAssaultGroupsOnContinent(numberOfContinents);
	std::vector< std::list<AAIGroup*> > availableAAGroupsOnContinent(numberOfContinents);

	std::list<AAIGroup*> availableAssaultGroupsGlobal;
	std::list<AAIGroup*> availableAAGroupsGlobal;

	const int numberOfAssaultUnitGroups = DetermineCombatUnitGroupsAvailableForattack(availableAssaultGroupsGlobal, availableAAGroupsGlobal,
																				availableAssaultGroupsOnContinent, availableAAGroupsOnContinent);

	// stop planning an attack if there are no combat groups available at the moment
	if(numberOfAssaultUnitGroups == 0)
		return;

	//////////////////////////////////////////////////////////////////////////////////////////////
	// calculate max attack power vs the different target types for each continent
	//////////////////////////////////////////////////////////////////////////////////////////////

	MobileTargetTypeValues numberOfAssaultGroupsOfTargetType;

	for(const auto group : availableAssaultGroupsGlobal)
	{
		numberOfAssaultGroupsOfTargetType.AddValueForTargetType(group->GetTargetType(), 1.0f);
	}

	for(size_t continent = 0; continent < availableAssaultGroupsOnContinent.size(); ++continent)
	{
		for(const auto group : availableAssaultGroupsOnContinent[continent])
		{
			numberOfAssaultGroupsOfTargetType.AddValueForTargetType(group->GetTargetType(), 1.0f);
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// determine target types of attackers
	//////////////////////////////////////////////////////////////////////////////////////////////

	std::list<AAITargetType> attackerTargetTypes;

	for(auto targetType : AAITargetType::m_mobileTargetTypes)
	{
		if(numberOfAssaultGroupsOfTargetType.GetValueOfTargetType(targetType) > 0)
			attackerTargetTypes.push_back(targetType);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// for every possible attacker target type, determine whether suitable target is available and
	// order attack
	//////////////////////////////////////////////////////////////////////////////////////////////

	for(auto targetType : attackerTargetTypes)
	{
		threatMap.UpdateLocalEnemyCombatPower(targetType, ai->Map()->GetSectorMap());

		const MapPos baseCenter = ai->Brain()->GetCenterOfBase();
		const AAISector* targetSector = threatMap.DetermineSectorToAttack(targetType, baseCenter, ai->Map()->GetSectorMap());

		// order groups of given target type to attack
		if(targetSector)
		{
			const float3 targetPosition = targetSector->DetermineAttackPosition();
			const int    continentId    = AAIMap::GetContinentID(targetPosition);

			AAIAttack *attack = new AAIAttack(ai, targetSector);

			// add combat unit groups
			attack->AddGroupsOfTargetType(availableAssaultGroupsOnContinent[continentId], targetType);
			attack->AddGroupsOfTargetType(availableAssaultGroupsGlobal, targetType);

			// add anti air units if necessary
			if(    (ai->Brain()->m_maxSpottedCombatUnitsOfTargetType.GetValueOfTargetType(ETargetType::AIR) > 0.2f)
				|| (ai->Brain()->GetRecentAttacksBy(ETargetType::AIR) > 0.9f) )
			{
				std::list<AAIGroup*> antiAirGroups;
				SelectNumberOfGroups(antiAirGroups, 1, availableAAGroupsOnContinent[continentId], availableAAGroupsGlobal);

				attack->AddGroupsOfTargetType(antiAirGroups, targetType);
			}

			if( attack->CheckIfFailed() )
			{
				// insufficient combat power of attacking units -> abort attack
				delete attack;
			}
			else
			{
				// start the attack
				m_activeAttacks[availableAttackId] = attack;
				attack->AttackPosition(targetPosition);
			}
		}
	}
}

void AAIAttackManager::SelectNumberOfGroups(std::list<AAIGroup*> selectedGroupList, int maxNumberOfGroups, std::list<AAIGroup*> groupList1, std::list<AAIGroup*> groupList2)
{
	int numberOfSelectedGroups(0);

	for(auto group = groupList1.begin(); group != groupList1.end(); ++group)
	{
		if(numberOfSelectedGroups >= maxNumberOfGroups)
			break;

		selectedGroupList.push_back(*group);
		++numberOfSelectedGroups;
	}

	for(auto group = groupList2.begin(); group != groupList2.end(); ++group)
	{
		if(numberOfSelectedGroups >= maxNumberOfGroups)
			break;

		selectedGroupList.push_back(*group);
		++numberOfSelectedGroups;
	}
}

int AAIAttackManager::DetermineCombatUnitGroupsAvailableForattack(  std::list<AAIGroup*>&                availableAssaultGroupsGlobal,
																	std::list<AAIGroup*>&                availableAAGroupsGlobal,
																	std::vector< std::list<AAIGroup*> >& availableAssaultGroupsOnContinent,
																	std::vector< std::list<AAIGroup*> >& availableAAGroupsOnContinent) const
{
	const std::vector<AAIUnitCategory> combatUnitCategories = { AAIUnitCategory(EUnitCategory::GROUND_COMBAT), 
															AAIUnitCategory(EUnitCategory::HOVER_COMBAT), 
															AAIUnitCategory(EUnitCategory::SEA_COMBAT),
															AAIUnitCategory(EUnitCategory::SUBMARINE_COMBAT) };

	int numberOfAssaultUnitGroups(0);

	for(auto category = combatUnitCategories.begin(); category != combatUnitCategories.end(); ++category)
	{
		for(auto group = ai->GetUnitGroupsList(*category).begin(); group != ai->GetUnitGroupsList(*category).end(); ++group)
		{
			if( (*group)->IsAvailableForAttack() )
			{
				const AAIUnitType& unitType = (*group)->GetUnitTypeOfGroup();
				if(unitType.IsAssaultUnit())
				{
					if( (*group)->GetMovementType().CannotMoveToOtherContinents() )
						availableAssaultGroupsOnContinent[(*group)->GetContinentId()].push_back(*group);
					else
						availableAssaultGroupsGlobal.push_back(*group);
						
					++numberOfAssaultUnitGroups;
				}
				else if(unitType.IsAntiAir())
				{
					if( (*group)->GetMovementType().CannotMoveToOtherContinents() )
						availableAAGroupsOnContinent[(*group)->GetContinentId()].push_back(*group);
					else
						availableAAGroupsGlobal.push_back(*group);
				}	
			}
		}
	}

	return numberOfAssaultUnitGroups;
}

void AAIAttackManager::AbortAttack(AAIAttack* attack)
{
	attack->StopAttack();

	for(auto a = m_activeAttacks.begin(); a != m_activeAttacks.end(); ++a)
	{
		if(*a == attack)
		{
			*a = nullptr;
			break;
		}
	}

	delete attack;
}

bool AAIAttackManager::AbortAttackIfFailed(AAIAttack *attack)
{
	if((ai->GetAICallback()->GetCurrentFrame() - attack->m_lastAttackOrderInFrame) < 30) 	// prevent command overflow
		return false;
	else if(attack->CheckIfFailed())
	{
		AbortAttack(attack);
		return true;
	}
	else
		return false;
}

void AAIAttackManager::AttackNextSectorOrAbort(AAIAttack* attack)
{
	// prevent command overflow
	if((ai->GetAICallback()->GetCurrentFrame() - attack->m_lastAttackOrderInFrame) < 60)
		return;

	const AAISector* sector = attack->DetermineSectorToContinueAttack();

	if(sector)
	{
		const float3 position = sector->DetermineAttackPosition();

		attack->AttackPosition(position);
	}	
	else
	{
		AbortAttack(attack);
	}		
}
