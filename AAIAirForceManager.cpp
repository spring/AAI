// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include <list>

#include "AAIAirForceManager.h"

#include "AAIMap.h"
#include "AAI.h"
#include "AAIBuildTable.h"
#include "AAIUnitTable.h"
#include "AAIConfig.h"
#include "AAIGroup.h"
#include "AAISector.h"

#include "LegacyCpp/UnitDef.h"

AAIAirForceManager::AAIAirForceManager(AAI *ai)
{
	this->ai = ai;
}

AAIAirForceManager::~AAIAirForceManager(void)
{
}

void AAIAirForceManager::CheckTarget(const UnitId& unitId, const AAIUnitCategory& category, float health)
{
	// do not attack own units
	if(ai->GetAICallback()->GetUnitTeam(unitId.id) != ai->GetMyTeamId()) 
	{
		float3 position = ai->GetAICallback()->GetUnitPos(unitId.id);

		// calculate in which sector unit is located
		AAISector* sector = ai->Getmap()->GetSectorOfPos(position);

		// check if unit is within the map
		if(sector)
		{
			// check for anti air defences if low on units
			if( (sector->GetLostAirUnits() > 2.5f) && (ai->GetUnitGroupsList(EUnitCategory::AIR_COMBAT).size() < 5) )
				return;

			AAIGroup *group(nullptr);
			int max_groups;

			if(health > 8000)
				max_groups = 3;
			else if(health > 4000)
				max_groups = 2;
			else
				max_groups = 1;

			for(int i = 0; i < max_groups; ++i)
			{
				if(category.IsAirCombat() == true)
				{
					group = GetAirGroup(EUnitType::ANTI_AIR, 100.0f);

					if(group)
						group->DefendAirSpace(&position);
				}
				else if(category.IsBuilding() == true)
				{
					group = GetAirGroup(EUnitType::ANTI_STATIC, 100.0f);

					if(group)
						group->BombTarget(unitId, position);
				}
				else
				{
					group = GetAirGroup(EUnitType::ANTI_SURFACE, 100.0f);

					if(group)
						group->AirRaidUnit(unitId.id);
				}
			}
		}
	}
}

bool AAIAirForceManager::CheckStaticBombTarget(UnitId unitId, UnitDefId unitDefId, const float3& position)
{
	std::set<AirRaidTarget*>* targets(nullptr);
	const AAIUnitCategory&    category = ai->s_buildTree.GetUnitCategory(unitDefId);

	if(category.IsStaticArtillery() || category.IsStaticSupport())
	{
		targets = &m_militaryTargets;
	}
	else if(category.IsPowerPlant() || category.IsMetalExtractor() || category.IsMetalMaker())
	{
		targets = &m_economyTargets;
	}

	if(targets != nullptr)
	{
		// dont continue if target list already full
		if(targets->size() >= cfg->MAX_AIR_TARGETS)
			return false;

		AirRaidTarget* target = new AirRaidTarget(unitId, unitDefId, position);
		targets->insert(target);

		//ai->Log("Target added...\n");
		return true;
	}

	return false;
}

void AAIAirForceManager::RemoveTarget(UnitId unitId)
{
	std::array< std::set<AirRaidTarget*>*, 2> targetLists = {&m_economyTargets, &m_militaryTargets};
	for(auto targetList : targetLists)
	for(auto target : *targetList)
	{
		if(target->GetUnitId() == unitId)
		{
			targetList->erase(target);
			delete target;

			//ai->Log("Target removed...\n");
			return;
		}
	}
}

void AAIAirForceManager::BombBestTarget(float danger)
{
	// try to select a military target first
	AirRaidTarget* selectedTarget = SelectBestTarget(m_militaryTargets, danger);
	bool highPriorityTarget(true);

	// if no military traget found, try to select lower priority economy target
	if(selectedTarget == nullptr)
	{
		selectedTarget = SelectBestTarget(m_economyTargets, danger);
		highPriorityTarget = false;
	}

	// try to order bombardment of traget if traget & bombers available
	if(selectedTarget)
	{
		const int minNumberOfBombers = std::min(static_cast<int>(ai->s_buildTree.GetHealth(selectedTarget->GetUnitDefId()) / cfg->HEALTH_PER_BOMBER), cfg->MAX_AIR_GROUP_SIZE);

		const AAIUnitCategory& targetCategory = ai->s_buildTree.GetUnitCategory(selectedTarget->GetUnitDefId());
		const bool  highPriorityTarget = targetCategory.IsStaticArtillery() || targetCategory.IsStaticSupport();
		const float urgency            = highPriorityTarget ? 120.f : 90.0f;

		AAIGroup *group = GetAirGroup(EUnitType::ANTI_STATIC, urgency, minNumberOfBombers);

		if(group)
		{
			//ai->Log("Bombing...\n");
			group->BombTarget(selectedTarget->GetUnitId(), selectedTarget->GetPosition());

			auto& targetList = highPriorityTarget ? m_militaryTargets : m_economyTargets;
			targetList.erase(selectedTarget);
			delete selectedTarget;
		}
	}
}

AirRaidTarget* AAIAirForceManager::SelectBestTarget(std::set<AirRaidTarget*>& targetList, float danger)
{
	float highestRating(0.0f);
	AirRaidTarget* selectedTarget(nullptr);

	for(auto target : targetList)
	{
		const UnitId&    unitId   = target->GetUnitId();
		const AAISector* sector   = ai->Getmap()->GetSectorOfPos(target->GetPosition());

		if(sector)
		{
			// favor already damaged targets
			const float healthRating = ai->s_buildTree.GetHealth(target->GetUnitDefId()) / ai->GetAICallback()->GetUnitHealth(unitId.id);

			const float rating = healthRating * ai->s_buildTree.GetTotalCost(target->GetUnitDefId()) / (1.0f + sector->GetEnemyCombatPower(ETargetType::AIR) * danger);

			if( (rating > highestRating) && (sector->GetLostAirUnits() < 0.8f) )
			{
				highestRating  = rating;
				selectedTarget = target;
			}
		}
	}

	return selectedTarget;
}

AAIGroup* AAIAirForceManager::GetAirGroup(EUnitType groupType, float importance, int minSize) const
{
	for(auto group : ai->GetUnitGroupsList(EUnitCategory::AIR_COMBAT))
	{
		if( (group->task_importance < importance) && group->GetUnitTypeOfGroup().IsUnitTypeSet(groupType) && (group->GetCurrentSize() >= minSize) )
			return group;
	}
	
	return nullptr;
}
