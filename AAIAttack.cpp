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
	m_attackDestination(nullptr),
	lastAttack(0)
{
	this->ai = ai;
}

AAIAttack::~AAIAttack(void)
{
	for(std::set<AAIGroup*>::iterator group = combat_groups.begin(); group != combat_groups.end(); ++group)
		(*group)->attack = 0;

	for(std::set<AAIGroup*>::iterator group = aa_groups.begin(); group != aa_groups.end(); ++group)
		(*group)->attack = 0;

	for(std::set<AAIGroup*>::iterator group = arty_groups.begin(); group != arty_groups.end(); ++group)
		(*group)->attack = 0;
}

bool AAIAttack::Failed()
{
	if(!combat_groups.empty())
	{
		// check if still enough power to attack target sector
		//if(ai->Getam()->SufficientAttackPowerVS(dest, &combat_groups, 1.3f))
		{
			// check if sufficient power to combat enemy units
			const float3 pos = (*combat_groups.begin())->GetGroupPos();
			AAISector *sector = ai->Getmap()->GetSectorOfPos(pos);

			if(sector && ai->Getam()->SufficientCombatPowerAt(sector, combat_groups, 2))
				return false;
		}
	}

	return true;
}

void AAIAttack::StopAttack()
{
	for(auto group = combat_groups.begin(); group != combat_groups.end(); ++group)
	{
		// get rally point somewhere between current pos an base
		(*group)->GetNewRallyPoint();

		(*group)->RetreatToRallyPoint();
		(*group)->attack = nullptr;
	}

	for(auto group = aa_groups.begin(); group != aa_groups.end(); ++group)
	{
		// get rally point somewhere between current pos an base
		(*group)->GetNewRallyPoint();

		(*group)->RetreatToRallyPoint();
		(*group)->attack = nullptr;
	}

	for(auto group = arty_groups.begin(); group != arty_groups.end(); ++group)
	{
		// todo
	}

	combat_groups.clear();
	aa_groups.clear();
	arty_groups.clear();
}

AAIMovementType AAIAttack::GetMovementTypeOfAssignedUnits() const
{
	AAIMovementType moveType;

	for(auto group = combat_groups.begin(); group != combat_groups.end(); ++group)
		moveType.AddMovementType( (*group)->GetMovementType() );
	
	for(auto group = aa_groups.begin(); group != aa_groups.end(); ++group)
		moveType.AddMovementType( (*group)->GetMovementType() );

	return moveType;
}

void AAIAttack::DetermineTargetTypeOfInvolvedUnits(AAIValuesForMobileTargetTypes& targetTypesOfUnits) const
{
	for(auto group = combat_groups.begin(); group != combat_groups.end(); ++group)
		targetTypesOfUnits.AddValueForTargetType( (*group)->GetTargetType(), static_cast<float>( (*group)->GetNumberOfUnits() ) );
}

void AAIAttack::AttackSector(const AAISector *sector)
{
	int unit;
	float importance = 110;

	m_attackDestination = sector;

	lastAttack = ai->GetAICallback()->GetCurrentFrame();

	for(std::set<AAIGroup*>::iterator group = combat_groups.begin(); group != combat_groups.end(); ++group)
	{
		(*group)->AttackSector(m_attackDestination, importance);
	}

	// order aa groups to guard combat units
	if(!combat_groups.empty())
	{
		for(std::set<AAIGroup*>::iterator group = aa_groups.begin(); group != aa_groups.end(); ++group)
		{
			unit = (*combat_groups.begin())->GetRandomUnit();

			if(unit >= 0)
			{
				Command c(CMD_GUARD);
				c.PushParam(unit);

				(*group)->GiveOrderToGroup(&c, 110, GUARDING, "Group::AttackSector");
			}
		}
	}

	for(std::set<AAIGroup*>::iterator group = arty_groups.begin(); group != arty_groups.end(); ++group)
	{
		(*group)->AttackSector(m_attackDestination, importance);
	}
}

void AAIAttack::AddGroup(AAIGroup *group)
{
	if(group->GetUnitTypeOfGroup().IsAssaultUnit())
	{
		combat_groups.insert(group);
		group->attack = this;
	}
	else if(group->GetUnitTypeOfGroup().IsAntiAir())
	{
		aa_groups.insert(group);
		group->attack = this;
	}
	else
	{
		arty_groups.insert(group);
		group->attack = this;
	}
}

void AAIAttack::RemoveGroup(AAIGroup *group)
{
	if(group->GetUnitTypeOfGroup().IsAssaultUnit())
	{
		combat_groups.erase(group);
	}
	else if(group->GetUnitTypeOfGroup().IsAntiAir())
	{
		aa_groups.erase(group);
	}
	else
	{
		arty_groups.erase(group);
	}

	ai->Getam()->CheckAttack(this);
}
