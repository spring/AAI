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
#include "AAIBuildTable.h"
#include "AAIAttack.h"
#include "AAIConfig.h"
#include "AAIGroup.h"
#include "AAIMap.h"
#include "AAISector.h"

AAIAttackManager::AAIAttackManager(AAI *ai)
{
	this->ai = ai;
}

AAIAttackManager::~AAIAttackManager(void)
{
	for(list<AAIAttack*>::iterator a = attacks.begin(); a != attacks.end(); ++a)
		delete (*a);

	attacks.clear();
}


void AAIAttackManager::Update(int numberOfContinents)
{
	for(list<AAIAttack*>::iterator a = attacks.begin(); a != attacks.end(); ++a)
	{
		// drop failed attacks
		if((*a)->Failed())
		{
			(*a)->StopAttack();

			delete (*a);
			attacks.erase(a);

			break;
		}

		// check if sector cleared
		if((*a)->dest)
		{
			if((*a)->dest->GetNumberOfEnemyBuildings() == 0)
				GetNextDest(*a);
		}
	}

	if(attacks.size() < cfg->MAX_ATTACKS)
	{
		TryToLaunchAttack(numberOfContinents);
	}
}

void AAIAttackManager::TryToLaunchAttack(int numberOfContinents)
{
	//////////////////////////////////////////////////////////////////////////////////////////////
	// get all available combat/aa/arty groups for attack
	//////////////////////////////////////////////////////////////////////////////////////////////
	
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
				const bool water = ai->Getmap()->IsSectorOnWaterContinent(sector);

				const float enemyDefencePower =   numberOfAssaultGroupsOfTargetType[AAITargetType::surfaceIndex]   * sector->GetEnemyCombatPower(ETargetType::SURFACE)
												+ numberOfAssaultGroupsOfTargetType[AAITargetType::floaterIndex]   * sector->GetEnemyCombatPower(ETargetType::FLOATER)
												+ numberOfAssaultGroupsOfTargetType[AAITargetType::submergedIndex] * sector->GetEnemyCombatPower(ETargetType::SUBMERGED);
				const float myAttackPower     = combatPowerGlobal[AAITargetType::staticIndex] + combatPowerOnContinent[sector->continent][AAITargetType::staticIndex];

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
		attacks.push_back(attack);

		// add combat groups
		for(auto group = availableAssaultGroupsOnContinent[selectedSector->continent].begin(); group != availableAssaultGroupsOnContinent[selectedSector->continent].end(); ++group)
			attack->AddGroup(*group);

		for(auto group = availableAssaultGroupsGlobal.begin(); group != availableAssaultGroupsGlobal.end(); ++group)
			attack->AddGroup(*group);

		// add anti-air defence
		int numberOfSupportingAntirAirGroups = 0;

		// check how much aa sensible
		const int maxNumberOfAntiAirGroups = (ai->Getbrain()->max_combat_units_spotted[1] < 0.2f) ? 0 : 1;

		for(auto group = availableAAGroupsOnContinent[selectedSector->continent].begin(); group != availableAAGroupsOnContinent[selectedSector->continent].end(); ++group)
		{
			if(numberOfSupportingAntirAirGroups >= maxNumberOfAntiAirGroups)
				break;

			attack->AddGroup(*group);
			++numberOfSupportingAntirAirGroups;
		}

		for(auto group = availableAAGroupsGlobal.begin(); group != availableAAGroupsGlobal.end(); ++group)
		{
			if(numberOfSupportingAntirAirGroups >= maxNumberOfAntiAirGroups)
				break;

			attack->AddGroup(*group);
			++numberOfSupportingAntirAirGroups;
		}

		// start the attack
		attack->AttackSector(selectedSector);
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
					if( (*group)->m_moveType.CannotMoveToOtherContinents() )
						availableAssaultGroupsOnContinent[(*group)->GetContinentId()].push_back(*group);
					else
						availableAssaultGroupsGlobal.push_back(*group);
						
					++numberOfAssaultUnitGroups;
				}
				else if(unitType.IsAntiAir())
				{
					if( (*group)->m_moveType.CannotMoveToOtherContinents() )
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

void AAIAttackManager::StopAttack(AAIAttack *attack)
{
	for(list<AAIAttack*>::iterator a = attacks.begin(); a != attacks.end(); ++a)
	{
		if(*a == attack)
		{
			(*a)->StopAttack();

			delete (*a);
			attacks.erase(a);

			break;
		}
	}
}

void AAIAttackManager::CheckAttack(AAIAttack *attack)
{
	// prevent command overflow
	if((ai->GetAICallback()->GetCurrentFrame() - attack->lastAttack) < 30)
		return;

	// drop failed attacks
	if(attack->Failed())
	{
		// delete attack from list
		for(list<AAIAttack*>::iterator a = attacks.begin(); a != attacks.end(); ++a)
		{
			if(*a == attack)
			{
				attacks.erase(a);

				attack->StopAttack();
				delete attack;

				break;
			}
		}
	}
}

void AAIAttackManager::GetNextDest(AAIAttack *attack)
{
	// prevent command overflow
	if((ai->GetAICallback()->GetCurrentFrame() - attack->lastAttack) < 60)
		return;

	// get new target sector
	AAISector *dest = ai->Getbrain()->GetNextAttackDest(attack->dest, attack->land, attack->water);

	//ai->Log("Getting next dest\n");
	if(dest && SufficientAttackPowerVS(dest, attack->combat_groups, 2.0f))
		attack->AttackSector(dest);
	else
		attack->StopAttack();
}

bool AAIAttackManager::SufficientAttackPowerVS(AAISector *dest, const std::set<AAIGroup*>& combatGroups, float aggressiveness) const
{
	if(dest && !combatGroups.empty())
	{
		// determine attack power
		float combatPowerVsBildings(0.0f);
		for(auto group = combatGroups.begin(); group != combatGroups.end(); ++group)
			combatPowerVsBildings += (*group)->GetCombatPowerVsTargetType(ETargetType::STATIC);
		
		//! @todo Fix to work for water units (attack behaviour must be completely reworked anyway)
		const float enemyDefencePower = dest->GetEnemyCombatPower(ETargetType::SURFACE);

		if(aggressiveness * combatPowerVsBildings > enemyDefencePower)
			return true;
	}

	return false;
}

bool AAIAttackManager::SufficientCombatPowerAt(const AAISector *dest, const std::set<AAIGroup*>& combatGroups, float aggressiveness) const
{
	if(dest && !combatGroups.empty())
	{
		//! @todo Must be reworked to work with water units.
		const AAITargetType targetType(ETargetType::SURFACE);

		const float enemyUnits =  dest->GetNumberOfEnemyCombatUnits(ECombatUnitCategory::GROUND_COMBAT) 
		                        + dest->GetNumberOfEnemyCombatUnits(ECombatUnitCategory::HOVER_COMBAT);

		if(enemyUnits <= 1.0f)
			return true;	

		// get total enemy combat power
		const float enemyCombatPower = dest->GetEnemyAreaCombatPowerVs(targetType, 0.25f) / enemyUnits;		

		// get total combat power of available units for attack
		float myCombatPower(0.0f);
		for(std::set<AAIGroup*>::const_iterator group = combatGroups.begin(); group != combatGroups.end(); ++group)
			myCombatPower += (*group)->GetCombatPowerVsTargetType(targetType);

		if(aggressiveness * myCombatPower > enemyCombatPower)
			return true;
	}

	return false;
}

