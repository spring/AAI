// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "AAIAttack.h"
#include "AAI.h"
#include "AAIAttackManager.h"
#include "AAIMap.h"
#include "AAIGroup.h"

#include "LegacyCpp/IAICallback.h"
using namespace springLegacyAI;

AAIAttack::AAIAttack(AAI *ai):
	m_lastAttackOrderInFrame(0),
	m_attackDestination(nullptr)
{
	this->ai = ai;
}

AAIAttack::~AAIAttack(void)
{
	for(std::set<AAIGroup*>::iterator group = m_combatUnitGroups.begin(); group != m_combatUnitGroups.end(); ++group)
		(*group)->attack = nullptr;

	for(std::set<AAIGroup*>::iterator group = m_antiAirUnitGroups.begin(); group != m_antiAirUnitGroups.end(); ++group)
		(*group)->attack = nullptr;
}

bool AAIAttack::CheckIfFailed()
{
	if(!m_combatUnitGroups.empty())
	{
		// check if still enough power to attack target sector
		if(SufficientCombatPowerToAttackSector(m_attackDestination, AAIConstants::attackCombatPowerFactor))
		{
			// check if sufficient power to combat enemy units
			const float3 pos = (*m_combatUnitGroups.begin())->GetGroupPos();
			const AAISector* sector = ai->Map()->GetSectorOfPos(pos);

			if(sector && SufficientCombatPowerAt(sector, AAIConstants::attackCombatPowerFactor))
				return false;
		}
	}

	return true;
}

bool AAIAttack::HasTargetBeenCleared()
{
	if(m_attackDestination == nullptr)
		return true;
	else
	{
		if(m_attackDestination->GetNumberOfEnemyBuildings() == 0)
			return true;
		
		// m_combatUnitGroups cannot be empty, otherwise attack would have been aborted before this function gets called
		const float3& targetPosition = (*m_combatUnitGroups.begin())->GetTargetPosition();
	
		if( ai->Map()->IsPositionInLOS(targetPosition) )
		{
			// if target is in LOS but no enemy units in LOS target is supposed to be cleared
			const int numberOfEnemyUnits = ai->GetAICallback()->GetEnemyUnits(&(ai->Map()->UnitsInLOS().front()), targetPosition, 128.0f);
			return (numberOfEnemyUnits == 0);
		}
		
		return false;
	}
}

const AAISector* AAIAttack::DetermineSectorToContinueAttack()
{
	AAIMovementType moveType( GetMovementTypeOfAssignedUnits() );

	MobileTargetTypeValues targetTypesOfUnits;
	DetermineTargetTypeOfInvolvedUnits(targetTypesOfUnits);

	const AAISector *dest = ai->Map()->DetermineSectorToContinueAttack(m_attackDestination, targetTypesOfUnits, moveType);

	if(dest && SufficientCombatPowerToAttackSector(dest, AAIConstants::attackCombatPowerFactor))
		return dest;
	else
		return nullptr;
}

bool AAIAttack::SufficientCombatPowerAt(const AAISector *sector, float aggressiveness) const
{
	if(sector && !m_combatUnitGroups.empty())
	{
		// determine target types of own units and combat efficiency against different types of enemy units
		MobileTargetTypeValues numberOfMyCombatUnits;
		TargetTypeValues       myCombatPower;

		for(auto group : m_combatUnitGroups)
		{
			numberOfMyCombatUnits.AddValueForTargetType(group->GetTargetType(), static_cast<float>(group->GetCurrentSize()) );
			group->AddGroupCombatPower(myCombatPower);
		}

		numberOfMyCombatUnits.Normalize();

		// determine enemy combat power (weighted by own units)
		const float enemyDefencePower = sector->GetEnemyCombatPowerVsUnits(numberOfMyCombatUnits);	

		TargetTypeValues numberOfEnemyUnits = sector->GetNumberOfEnemyCombatUnits();
		const float totalEnemyUnits = numberOfEnemyUnits.CalcuateSum();

		if(totalEnemyUnits > 0.0f)
		{
			// normalize relative number of enemy units
			numberOfEnemyUnits.MultiplyValues(1.0f / totalEnemyUnits);
			const float myAttackPower = myCombatPower.CalculateWeightedSum(numberOfEnemyUnits);

			/*ai->Log("My units: %f, %f, %f, %f\n", numberOfMyCombatUnits.GetValueOfTargetType(ETargetType::SURFACE), 
			numberOfMyCombatUnits.GetValueOfTargetType(ETargetType::AIR), 
			numberOfMyCombatUnits.GetValueOfTargetType(ETargetType::FLOATER), 
			numberOfMyCombatUnits.GetValueOfTargetType(ETargetType::SUBMERGED));
			ai->Log("Enemy units: %f, %f, %f, %f, %f\n", numberOfEnemyUnits.GetValue(ETargetType::SURFACE), 
			numberOfEnemyUnits.GetValue(ETargetType::AIR), numberOfEnemyUnits.GetValue(ETargetType::FLOATER), 
			numberOfEnemyUnits.GetValue(ETargetType::SUBMERGED),numberOfEnemyUnits.GetValue(ETargetType::STATIC));
			ai->Log("My attack/enemy defence power: %f / %f\n", myAttackPower, enemyDefencePower);*/

			if(aggressiveness * myAttackPower > enemyDefencePower)
				return true;
		}
		else
			return true;
	}

	return false;
}

bool AAIAttack::SufficientCombatPowerToAttackSector(const AAISector *sector, float aggressiveness) const
{
	if(sector && !m_combatUnitGroups.empty())
	{
		// determine total combat power vs static  & how it is distributed over different target types
		float combatPowerVsBildings(0.0f);
		MobileTargetTypeValues targetTypeWeights;

		for(auto group = m_combatUnitGroups.begin(); group != m_combatUnitGroups.end(); ++group)
		{
			const float combatPower = (*group)->GetCombatPowerVsTargetType(ETargetType::STATIC);
			targetTypeWeights.AddValueForTargetType( (*group)->GetTargetType(), combatPower);

			combatPowerVsBildings += combatPower;
		}
		
		// determine combat power by static enemy defences with respect to target type of attacking units
		const float enemyDefencePower = targetTypeWeights.GetValueOfTargetType(ETargetType::SURFACE)   * sector->GetEnemyCombatPower(ETargetType::SURFACE)
									  + targetTypeWeights.GetValueOfTargetType(ETargetType::FLOATER)   * sector->GetEnemyCombatPower(ETargetType::FLOATER)
									  + targetTypeWeights.GetValueOfTargetType(ETargetType::SUBMERGED) * sector->GetEnemyCombatPower(ETargetType::SUBMERGED);

		if(aggressiveness * combatPowerVsBildings > enemyDefencePower)
			return true;
	}

	return false;
}

void AAIAttack::AttackSector(const AAISector *sector)
{
	m_attackDestination = sector;

	m_lastAttackOrderInFrame = ai->GetAICallback()->GetCurrentFrame();

	for(auto group : m_combatUnitGroups)
	{
		group->AttackSector(m_attackDestination, AAIConstants::attackEnemyBaseUrgency);
	}

	// order aa groups to guard combat units
	if(!m_combatUnitGroups.empty())
	{
		for(auto group : m_antiAirUnitGroups)
		{
			const UnitId unitId = (*m_combatUnitGroups.begin())->GetRandomUnit();

			if(unitId.IsValid())
			{
				Command c(CMD_GUARD);
				c.PushParam(unitId.id);

				group->GiveOrderToGroup(&c, AAIConstants::defendUnitsUrgency, GUARDING, "Group::AttackSector");
			}
		}
	}
}

void AAIAttack::StopAttack()
{
	for(auto group : m_combatUnitGroups)
	{
		// get rally point somewhere between current pos an base
		//group->GetNewRallyPoint();
		group->RetreatToRallyPoint();
	}

	for(auto group : m_antiAirUnitGroups)
	{
		// get rally point somewhere between current pos an base
		//group->GetNewRallyPoint();
		group->RetreatToRallyPoint();
	}

	m_combatUnitGroups.clear();
	m_antiAirUnitGroups.clear();
}

AAIMovementType AAIAttack::GetMovementTypeOfAssignedUnits() const
{
	AAIMovementType moveType;

	for(auto group : m_combatUnitGroups)
		moveType.AddMovementType( group->GetMovementType() );
	
	for(auto group : m_antiAirUnitGroups)
		moveType.AddMovementType( group->GetMovementType() );

	return moveType;
}

void AAIAttack::DetermineTargetTypeOfInvolvedUnits(MobileTargetTypeValues& targetTypesOfUnits) const
{
	for(auto group : m_combatUnitGroups)
		targetTypesOfUnits.AddValueForTargetType( group->GetTargetType(), static_cast<float>( group->GetCurrentSize() ) );
}

bool AAIAttack::AddGroup(AAIGroup *group)
{
	if(group->GetUnitTypeOfGroup().IsAssaultUnit())
	{
		m_combatUnitGroups.insert(group);
		return true;
	}
	else if(group->GetUnitTypeOfGroup().IsAntiAir())
	{
		m_antiAirUnitGroups.insert(group);
		return true;
	}

	return false;
}

void AAIAttack::RemoveGroup(AAIGroup *group)
{
	if(group->GetUnitTypeOfGroup().IsAssaultUnit())
	{
		m_combatUnitGroups.erase(group);
	}
	else if(group->GetUnitTypeOfGroup().IsAntiAir())
	{
		m_antiAirUnitGroups.erase(group);
	}
}
