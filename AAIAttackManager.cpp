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

AAIAttackManager::AAIAttackManager(AAI *ai)
{
	this->ai = ai;

	m_activeAttacks.resize(cfg->MAX_ATTACKS, nullptr);
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

void AAIAttackManager::Update()
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
			else if( attack->m_attackDestination && ( attack->m_attackDestination->GetNumberOfEnemyBuildings() == 0) )
				AttackNextSectorOrAbort(attack);
		}
		else
			availableAttackId = attackId;
	}

	// at least one attack id is available -> check if new attack should be launched
	if(availableAttackId >= 0)
		TryToLaunchAttack(availableAttackId);
}

void AAIAttackManager::TryToLaunchAttack(int availableAttackId)
{
	//////////////////////////////////////////////////////////////////////////////////////////////
	// get all available combat/aa/arty groups for attack
	//////////////////////////////////////////////////////////////////////////////////////////////
	
	const int numberOfContinents( AAIMap::continents.size() );
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

	std::vector< std::vector<float> > combatPowerOnContinent(numberOfContinents, std::vector<float>(AAITargetType::numberOfTargetTypes, 0.0f) );
	std::vector<float>                combatPowerGlobal(AAITargetType::numberOfTargetTypes, 0.0f);
	std::vector<float>                numberOfAssaultGroupsOfTargetType(AAITargetType::numberOfMobileTargetTypes, 0.0f);

	DetermineCombatPowerOfGroups(availableAssaultGroupsGlobal, combatPowerGlobal, numberOfAssaultGroupsOfTargetType);

	for(size_t continent = 0; continent < availableAssaultGroupsOnContinent.size(); ++continent)
		DetermineCombatPowerOfGroups(availableAssaultGroupsOnContinent[continent], combatPowerOnContinent[continent], numberOfAssaultGroupsOfTargetType);

	//////////////////////////////////////////////////////////////////////////////////////////////
	// determine attack sector
	//////////////////////////////////////////////////////////////////////////////////////////////

	// determine max lost units
	const float maxLostUnits = ai->Getmap()->GetMaximumNumberOfLostUnits();

	float highestRating(0.0f);
	AAISector* selectedSector = nullptr;

	for(int x = 0; x < ai->Getmap()->xSectors; ++x)
	{
		for(int y = 0; y < ai->Getmap()->ySectors; ++y)
		{
			AAISector* sector = &ai->Getmap()->sector[x][y];

			if( (sector->distance_to_base > 0) && (sector->GetNumberOfEnemyBuildings() > 0))
			{
				const float myAttackPower     = combatPowerGlobal[AAITargetType::staticIndex] + combatPowerOnContinent[sector->continent][AAITargetType::staticIndex];
				const float enemyDefencePower =   numberOfAssaultGroupsOfTargetType[AAITargetType::surfaceIndex]   * sector->GetEnemyCombatPower(ETargetType::SURFACE)
												+ numberOfAssaultGroupsOfTargetType[AAITargetType::floaterIndex]   * sector->GetEnemyCombatPower(ETargetType::FLOATER)
												+ numberOfAssaultGroupsOfTargetType[AAITargetType::submergedIndex] * sector->GetEnemyCombatPower(ETargetType::SUBMERGED);
				
				const float lostUnitsFactor = (maxLostUnits > 1.0f) ? (2.0f - (sector->GetLostUnits() / maxLostUnits) ) : 1.0f;

				const float enemyBuildings = static_cast<float>(sector->GetNumberOfEnemyBuildings());

				// prefer sectors with many buildings, few lost units and low defence power/short distance to own base
				float rating = lostUnitsFactor * enemyBuildings * myAttackPower / ( (0.1f + enemyDefencePower) * (float)(2 + sector->distance_to_base) );
	
				if(rating > highestRating)
				{
					selectedSector = sector;
					highestRating  = rating;
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// order attack
	//////////////////////////////////////////////////////////////////////////////////////////////

	if(selectedSector)
	{
		AAIAttack *attack = new AAIAttack(ai);
		m_activeAttacks[availableAttackId] = attack;

		// add combat unit groups
		AddGroupsToAttack(attack, availableAssaultGroupsOnContinent[selectedSector->continent]);
		AddGroupsToAttack(attack, availableAssaultGroupsGlobal);

		// add anti air units if necessary
		if(    (ai->Getbrain()->m_maxSpottedCombatUnitsOfTargetType.GetValueOfTargetType(ETargetType::AIR) > 0.2f)
			|| (ai->Getbrain()->GetRecentAttacksBy(ETargetType::AIR) > 0.9f) )
		{
			std::list<AAIGroup*> antiAirGroups;
			SelectNumberOfGroups(antiAirGroups, 1, availableAAGroupsOnContinent[selectedSector->continent], availableAAGroupsGlobal);

			AddGroupsToAttack(attack, antiAirGroups);
		}
		
		// start the attack
		attack->AttackSector(selectedSector);
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

void AAIAttackManager::AddGroupsToAttack(AAIAttack* attack, const std::list<AAIGroup*>& groupList) const
{
	for(auto group = groupList.begin(); group != groupList.end(); ++group)
	{
		if(attack->AddGroup(*group))
			(*group)->attack = attack; 
	}
}

int AAIAttackManager::DetermineCombatUnitGroupsAvailableForattack(  std::list<AAIGroup*>&                availableAssaultGroupsGlobal,
																	std::list<AAIGroup*>&                availableAAGroupsGlobal,
																	std::vector< std::list<AAIGroup*> >& availableAssaultGroupsOnContinent,
																	std::vector< std::list<AAIGroup*> >& availableAAGroupsOnContinent) const
{
	const std::vector<AAIUnitCategory> combatCategories = { AAIUnitCategory(EUnitCategory::GROUND_COMBAT), 
															AAIUnitCategory(EUnitCategory::HOVER_COMBAT), 
															AAIUnitCategory(EUnitCategory::SEA_COMBAT),
															AAIUnitCategory(EUnitCategory::SUBMARINE_COMBAT) };

	int numberOfAssaultUnitGroups(0);

	for(auto category = combatCategories.begin(); category != combatCategories.end(); ++category)
	{
		for(auto group = ai->GetGroupList()[category->GetArrayIndex()].begin(); group != ai->GetGroupList()[category->GetArrayIndex()].end(); ++group)
		{
			if( (*group)->AvailableForAttack() )
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

void AAIAttackManager::DetermineCombatPowerOfGroups(const std::list<AAIGroup*>& groups, std::vector<float>& combatPower, std::vector<float>& numberOfGroupsOfTargetType) const
{
	for(auto group = groups.begin(); group != groups.end(); ++group)
	{
		numberOfGroupsOfTargetType[(*group)->GetTargetType().GetArrayIndex()] += 1.0f;

		combatPower[AAITargetType::staticIndex] += (*group)->GetCombatPowerVsTargetType(ETargetType::STATIC);

		const AAIUnitCategory& category = (*group)->GetUnitCategoryOfGroup();

		if(category.isGroundCombat())
			combatPower[AAITargetType::surfaceIndex] += (*group)->GetCombatPowerVsTargetType(ETargetType::SURFACE);
		else if(category.isHoverCombat())
		{
			combatPower[AAITargetType::surfaceIndex] += (*group)->GetCombatPowerVsTargetType(ETargetType::SURFACE);
			combatPower[AAITargetType::floaterIndex] += (*group)->GetCombatPowerVsTargetType(ETargetType::FLOATER);
		}
		else if(category.isSeaCombat() || category.isSubmarineCombat())
		{
			combatPower[AAITargetType::floaterIndex]   += (*group)->GetCombatPowerVsTargetType(ETargetType::FLOATER);
			combatPower[AAITargetType::submergedIndex] += (*group)->GetCombatPowerVsTargetType(ETargetType::SUBMERGED);
		}
	}
}

void AAIAttackManager::AbortAttack(AAIAttack* attack)
{
	ai->Log("Attack aborted\n");

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

	AAIMovementType moveType( attack->GetMovementTypeOfAssignedUnits() );
	AAIValuesForMobileTargetTypes targetTypesOfUnits;
	attack->DetermineTargetTypeOfInvolvedUnits(targetTypesOfUnits);

	// get new target sector
	const AAISector *dest = GetNextAttackDest(attack->m_attackDestination, targetTypesOfUnits, moveType);

	if(dest && attack->SufficientCombatPowerToAttackSector(dest, 3.0f))
		attack->AttackSector(dest);
	else
		AbortAttack(attack);
}

const AAISector* AAIAttackManager::GetNextAttackDest(const AAISector *currentSector, const AAIValuesForMobileTargetTypes& targetTypeOfUnits, AAIMovementType moveTypeOfUnits) const
{
	float highestRating(0.0f);
	const AAISector* selectedSector(nullptr);

	const bool landSectorSelectable  = moveTypeOfUnits.IsAir() || moveTypeOfUnits.IsHover() || moveTypeOfUnits.IsAmphibious() || moveTypeOfUnits.IsGround();
	const bool waterSectorSelectable = moveTypeOfUnits.IsAir() || moveTypeOfUnits.IsHover() || moveTypeOfUnits.IsSeaUnit();

	for(int x = 0; x < ai->Getmap()->xSectors; x++)
	{
		for(int y = 0; y < ai->Getmap()->ySectors; y++)
		{
			const float rating = ai->Getmap()->sector[x][y].GetAttackRating(currentSector, landSectorSelectable, waterSectorSelectable, targetTypeOfUnits);
			
			if(rating > highestRating)
			{
				selectedSector = &ai->Getmap()->sector[x][y];
				highestRating  = rating;
			}
		}
	}

	return selectedSector;
}
