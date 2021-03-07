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
#include "AAIBrain.h"

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
		const float3     position = ai->GetAICallback()->GetUnitPos(unitId.id);
		const AAISector* sector   = ai->Map()->GetSectorOfPos(position);

		// check if unit is within the map
		if(sector && (sector->GetLostAirUnits() < AAIConstants::maxLostAirUnitsForAirSupport) )
		{
			if(category.IsAirCombat())
			{
				AAIGroup *group = GetAirGroup(EUnitType::ANTI_AIR, AAIConstants::defendUnitsUrgency);

				if(group)
					group->DefendAirSpace(position);
			}
			else if(category.IsBuilding())
			{
				AAIGroup *group = GetAirGroup(EUnitType::ANTI_STATIC, AAIConstants::defendUnitsUrgency);

				if(group)
					group->BombTarget(unitId, position);
			}
			else
			{
				/*AAIGroup *group = GetAirGroup(EUnitType::ANTI_SURFACE, AAIConstants::defendUnitsUrgency);

				if(group)
					group->AirRaidUnit(unitId);*/
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
	{
		for(auto target : *targetList)
		{
			if(target->GetUnitId() == unitId)
			{
				targetList->erase(target);
				delete target;

				return;
			}
		}
	}
}

float AAIAirForceManager::GetNumberOfBombTargets() const
{
	const float currentTargets = static_cast<float>( m_economyTargets.size() + m_militaryTargets.size());
	return currentTargets / (2.0f * static_cast<float>(cfg->MAX_AIR_TARGETS)); 
}

void AAIAirForceManager::BombBestTarget(float danger)
{
	const int maxNumberOfAvailableBombers = DetermineMaximumNumberOfAvailableBombers(AAIConstants::bombingRunUrgency);

	//ai->Log("Checking bombing run: %i bombers available for %i military and %i economic targets", maxNumberOfAvailableBombers, m_militaryTargets.size(), m_economyTargets.size());

	if(maxNumberOfAvailableBombers <= 0)
		return;

	const MapPos& baseCenter = ai->Brain()->GetCenterOfBase();
	const float3  position( static_cast<float>(baseCenter.x * SQUARE_SIZE), 0.0f, static_cast<float>(baseCenter.y * SQUARE_SIZE) );

	// try to select a military target first
	AirRaidTarget* selectedTarget = SelectBestTarget(m_militaryTargets, danger, maxNumberOfAvailableBombers, position);
	bool highPriorityTarget(true);

	// if no military target found, try to select lower priority economy target
	if(selectedTarget == nullptr)
	{
		selectedTarget = SelectBestTarget(m_economyTargets, danger, maxNumberOfAvailableBombers, position);
		highPriorityTarget = false;
	}

	// try to order bombardment if target & bombers available
	if(selectedTarget)
	{
		//ai->Log(" - target found");

		const int minNumberOfBombers = std::max(static_cast<int>(ai->s_buildTree.GetHealth(selectedTarget->GetUnitDefId()) / cfg->HEALTH_PER_BOMBER), 1);

		const AAIUnitCategory& targetCategory = ai->s_buildTree.GetUnitCategory(selectedTarget->GetUnitDefId());
		const bool  highPriorityTarget = targetCategory.IsStaticArtillery() || targetCategory.IsStaticSupport();

		int bombersSent(0);
		while(bombersSent < minNumberOfBombers)
		{
			AAIGroup *group = GetAirGroup(EUnitType::ANTI_STATIC, 0.9f * AAIConstants::bombingRunUrgency);

			if(group)
			{
				//ai->Log("- bombers sent.\n");
				group->BombTarget(selectedTarget->GetUnitId(), selectedTarget->GetPosition());
				bombersSent += group->GetCurrentSize();
			}
			else
			{
				break;
			}
		}

		if(bombersSent > 0)
			RemoveTarget(selectedTarget->GetUnitId());
	}

	//ai->Log("\n");
}

void AAIAirForceManager::CheckNextBombTarget(AAIGroup* group)
{
	//ai->Log("Checking for new bomb target for %i bombers", group->GetCurrentSize() );

	const float3 position = group->GetGroupPos();

	// try to select a military target first
	AirRaidTarget* selectedTarget = SelectBestTarget(m_militaryTargets, 1.5f, group->GetCurrentSize(), position);
	bool highPriorityTarget(true);

	// if no military target found, try to select lower priority economy target
	if(selectedTarget == nullptr)
	{
		selectedTarget = SelectBestTarget(m_economyTargets, 1.5f, group->GetCurrentSize(), position);
		highPriorityTarget = false;
	}

	if(selectedTarget)
	{
		group->BombTarget(selectedTarget->GetUnitId(), selectedTarget->GetPosition());
		//ai->Log(" - Continuing bombing run with target %s\n", ai->s_buildTree.GetUnitTypeProperties(selectedTarget->GetUnitDefId()).m_name.c_str() );
	}		
	else
	{
		group->TargetUnitKilled();
	}
}

AirRaidTarget* AAIAirForceManager::SelectBestTarget(std::set<AirRaidTarget*>& targetList, float danger, int availableBombers, const float3& position)
{
	float bestRating(4.0f);	// rating should be between 0 (best) and 3 (worst)
	AirRaidTarget* selectedTarget(nullptr);

	for(auto target : targetList)
	{
		const UnitId&    unitId = target->GetUnitId();
		const AAISector* sector = ai->Map()->GetSectorOfPos(target->GetPosition());

		if(sector)
		{
			// check if sufficient number of bombers is available
			const int minNumberOfBombers = std::min(static_cast<int>(ai->s_buildTree.GetHealth(target->GetUnitDefId()) / cfg->HEALTH_PER_BOMBER), cfg->MAX_AIR_GROUP_SIZE);

			if( (availableBombers >= minNumberOfBombers) && (sector->GetLostAirUnits() < 0.8f) )
			{
				const float dx = position.x - target->GetPosition().x;
				const float dy = position.z - target->GetPosition().z;

				// between 0 (target nearby) and 1 (target on the other side of the map)
				const float distFactor = (dx*dx + dy*dy) / AAIMap::s_maxSquaredMapDist;

				// between 0 (no known enemy air defences) and 1 (strong known enemy air defences)
				const float airDefenceFactor = std::min(sector->GetEnemyCombatPower(ETargetType::AIR) / AAIConstants::maxCombatPower, 1.0f);

				// between 0 (no recently lost air units) and 1 (more than 3 recently lost air units)
				const float lostAirUnitsFactor = std::min( sector->GetLostAirUnits() / 3.0f, 1.0f);

				const float rating = distFactor + airDefenceFactor + lostAirUnitsFactor;

				if(rating < bestRating)
				{
					bestRating     = rating;
					selectedTarget = target;
				}
			}
		}
	}

	return selectedTarget;
}

AAIGroup* AAIAirForceManager::GetAirGroup(EUnitType groupType, float importance) const
{
	for(auto group : ai->GetUnitGroupsList(EUnitCategory::AIR_COMBAT))
	{
		if( (group->task_importance < importance) && group->GetUnitTypeOfGroup().IsUnitTypeSet(groupType) )
			return group;
	}
	
	return nullptr;
}

int AAIAirForceManager::DetermineMaximumNumberOfAvailableBombers(float importance) const
{
	int numberOfAvailableBombers(0);

	for(auto group : ai->GetUnitGroupsList(EUnitCategory::AIR_COMBAT))
	{
		if( (group->task_importance < importance) && group->GetUnitTypeOfGroup().IsUnitTypeSet(EUnitType::ANTI_STATIC) )
		{
			numberOfAvailableBombers += group->GetCurrentSize();
		}
	}
	
	return numberOfAvailableBombers;
}
