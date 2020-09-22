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
			const AAISector* sector = ai->Getmap()->GetSectorOfPos(pos);

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
	
		if( ai->Getmap()->IsPositionInLOS(targetPosition) )
		{
			// if target is in LOS but no enemy units in LOS target is supposed to be cleared
			const int numberOfEnemyUnits = ai->GetAICallback()->GetEnemyUnits(&(ai->Getmap()->unitsInLOS.front()), targetPosition, 128.0f);
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

	const AAISector *dest = ai->Getmap()->DetermineSectorToContinueAttack(m_attackDestination, targetTypesOfUnits, moveType);

	if(dest && SufficientCombatPowerToAttackSector(dest, AAIConstants::attackCombatPowerFactor))
		return dest;
	else
		return nullptr;
}

bool AAIAttack::SufficientCombatPowerAt(const AAISector *sector, float aggressiveness) const
{
	if(sector && !m_combatUnitGroups.empty())
	{
		//! @todo Must be reworked to work with water units.
		const AAITargetType targetType(ETargetType::SURFACE);

		const float enemyUnits =  sector->GetNumberOfEnemyCombatUnits(ECombatUnitCategory::GROUND_COMBAT) 
		                        + sector->GetNumberOfEnemyCombatUnits(ECombatUnitCategory::HOVER_COMBAT);

		if(enemyUnits <= 1.0f)
			return true;	

		// get total enemy combat power
		const float enemyCombatPower = sector->GetEnemyAreaCombatPowerVs(targetType, 0.25f) / enemyUnits;		

		// get total combat power of available units for attack
		float myCombatPower(0.0f);
		for(std::set<AAIGroup*>::const_iterator group = m_combatUnitGroups.begin(); group != m_combatUnitGroups.end(); ++group)
			myCombatPower += (*group)->GetCombatPowerVsTargetType(targetType);

		if(aggressiveness * myCombatPower > enemyCombatPower)
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
	int unit;
	float importance = 110;

	m_attackDestination = sector;

	m_lastAttackOrderInFrame = ai->GetAICallback()->GetCurrentFrame();

	for(std::set<AAIGroup*>::iterator group = m_combatUnitGroups.begin(); group != m_combatUnitGroups.end(); ++group)
	{
		(*group)->AttackSector(m_attackDestination, importance);
	}

	// order aa groups to guard combat units
	if(!m_combatUnitGroups.empty())
	{
		for(std::set<AAIGroup*>::iterator group = m_antiAirUnitGroups.begin(); group != m_antiAirUnitGroups.end(); ++group)
		{
			UnitId unitId = (*m_combatUnitGroups.begin())->GetRandomUnit();

			if(unitId.IsValid())
			{
				Command c(CMD_GUARD);
				c.PushParam(unitId.id);

				(*group)->GiveOrderToGroup(&c, 110, GUARDING, "Group::AttackSector");
			}
		}
	}
}

void AAIAttack::StopAttack()
{
	for(auto group = m_combatUnitGroups.begin(); group != m_combatUnitGroups.end(); ++group)
	{
		// get rally point somewhere between current pos an base
		(*group)->GetNewRallyPoint();

		(*group)->RetreatToRallyPoint();
		(*group)->attack = nullptr;
	}

	for(auto group = m_antiAirUnitGroups.begin(); group != m_antiAirUnitGroups.end(); ++group)
	{
		// get rally point somewhere between current pos an base
		(*group)->GetNewRallyPoint();

		(*group)->RetreatToRallyPoint();
		(*group)->attack = nullptr;
	}

	m_combatUnitGroups.clear();
	m_antiAirUnitGroups.clear();
}

AAIMovementType AAIAttack::GetMovementTypeOfAssignedUnits() const
{
	AAIMovementType moveType;

	for(auto group = m_combatUnitGroups.begin(); group != m_combatUnitGroups.end(); ++group)
		moveType.AddMovementType( (*group)->GetMovementType() );
	
	for(auto group = m_antiAirUnitGroups.begin(); group != m_antiAirUnitGroups.end(); ++group)
		moveType.AddMovementType( (*group)->GetMovementType() );

	return moveType;
}

void AAIAttack::DetermineTargetTypeOfInvolvedUnits(MobileTargetTypeValues& targetTypesOfUnits) const
{
	for(auto group = m_combatUnitGroups.begin(); group != m_combatUnitGroups.end(); ++group)
		targetTypesOfUnits.AddValueForTargetType( (*group)->GetTargetType(), static_cast<float>( (*group)->GetCurrentSize() ) );
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
