// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "AAI.h"
#include "AAIUnitTable.h"
#include "AAIExecute.h"
#include "AAIConstructor.h"
#include "AAIBuildTable.h"
#include "AAIAirForceManager.h"
#include "AAIConfig.h"
#include "AAIMap.h"
#include "AAIGroup.h"
#include "AAIConstructor.h"

#include "LegacyCpp/UnitDef.h"
using namespace springLegacyAI;


AAIUnitTable::AAIUnitTable(AAI *ai)
{
	this->ai = ai;

	units.resize(cfg->MAX_UNITS);

	// fill buildtable
	for(int i = 0; i < cfg->MAX_UNITS; ++i)
	{
		units[i].unit_id = -1;
		units[i].def_id = 0;
		units[i].group = nullptr;
		units[i].cons = nullptr;
		units[i].status = UNIT_KILLED;
		units[i].last_order = 0;
	}

	m_activeUnitsOfCategory.resize(AAIUnitCategory::numberOfUnitCategories, 0);
	m_underConstructionUnitsOfCategory.resize(AAIUnitCategory::numberOfUnitCategories, 0);
	m_requestedUnitsOfCategory.resize(AAIUnitCategory::numberOfUnitCategories, 0);
	
	activeFactories = futureFactories = 0;
}

AAIUnitTable::~AAIUnitTable(void)
{
	// delete constructors
	for(set<int>::iterator cons = constructors.begin(); cons != constructors.end(); ++cons)
	{
		delete units[*cons].cons;
	}

	m_activeUnitsOfCategory.clear();
	m_underConstructionUnitsOfCategory.clear();
	m_requestedUnitsOfCategory.clear();
}


bool AAIUnitTable::AddUnit(int unit_id, int def_id, AAIGroup *group, AAIConstructor *cons)
{
	if(unit_id < cfg->MAX_UNITS)
	{
		// clear possible enemies that are still listed (since they had been killed outside of los)
		if(units[unit_id].status == ENEMY_UNIT)
		{
			if(units[unit_id].group)
				units[unit_id].group->TargetUnitKilled();
		}
		else if(units[unit_id].status == BOMB_TARGET)
		{
			ai->Getaf()->RemoveTarget(unit_id);

			if(units[unit_id].group)
				units[unit_id].group->TargetUnitKilled();
		}

		units[unit_id].unit_id = unit_id;
		units[unit_id].def_id = def_id;
		units[unit_id].group = group;
		units[unit_id].cons = cons;
		units[unit_id].status = UNIT_IDLE;
		return true;
	}
	else
	{
		ai->Log("ERROR: AAIUnitTable::AddUnit() index %i out of range", unit_id);
		return false;
	}
}

void AAIUnitTable::RemoveUnit(int unit_id)
{
	if(unit_id < cfg->MAX_UNITS)
	{
		units[unit_id].unit_id = -1;
		units[unit_id].def_id = 0;
		units[unit_id].group = 0;
		units[unit_id].cons = nullptr;
		units[unit_id].status = UNIT_KILLED;
	}
	else
	{
		ai->Log("ERROR: AAIUnitTable::RemoveUnit() index %i out of range", unit_id);
	}
}

void AAIUnitTable::AddConstructor(UnitId unitId, UnitDefId unitDefId)
{
	AAIConstructor *cons = new AAIConstructor(ai, unitId, unitDefId, IsFactory(unitDefId), IsBuilder(unitDefId), IsAssister(unitDefId), ai->Getexecute()->GetBuildqueueOfFactory(unitDefId));

	constructors.insert(unitId.id);
	units[unitId.id].cons = cons;

	// increase/decrease number of available/requested builders for all buildoptions of the builder
	ai->Getbt()->ConstructorFinished(unitDefId);

	if( (IsFactory(unitDefId) == true) && (ai->s_buildTree.GetMovementType(unitDefId).IsStatic() == true) )
	{
		--futureFactories;
		++activeFactories;
	}
}

void AAIUnitTable::RemoveConstructor(int unit_id, int def_id)
{
	if( (IsFactory(UnitDefId(def_id)) == true) && (ai->s_buildTree.GetMovementType(UnitDefId(def_id)).IsStatic() == true) )
		activeFactories -= 1;

	// decrease number of available builders for all buildoptions of the builder
	ai->Getbt()->ConstructorKilled(UnitDefId(def_id));

	// erase from builders list
	constructors.erase(unit_id);

	// clean up memory
	units[unit_id].cons->Killed();
	delete units[unit_id].cons;
	units[unit_id].cons = nullptr;
}

void AAIUnitTable::AddCommander(UnitId unitId, UnitDefId unitDefId)
{
	AAIConstructor *cons = new AAIConstructor(ai, unitId, unitDefId, IsFactory(unitDefId), IsBuilder(unitDefId), IsAssister(unitDefId), ai->Getexecute()->GetBuildqueueOfFactory(unitDefId));
	units[unitId.id].cons = cons;

	constructors.insert(unitId.id);

	// increase number of builders for all buildoptions of the commander
	ai->Getbt()->ConstructorRequested(unitDefId); // commander has not been requested -> increase "requested constructors" counter as it is decreased by ConstructorFinished(...)
	ai->Getbt()->ConstructorFinished(unitDefId);
}

void AAIUnitTable::RemoveCommander(int unit_id, int def_id)
{
	// decrease number of builders for all buildoptions of the commander
	ai->Getbt()->ConstructorKilled(UnitDefId(def_id));

	// erase from builders list
	constructors.erase(unit_id);

	// clean up memory
	units[unit_id].cons->Killed();
	delete units[unit_id].cons;
	units[unit_id].cons = nullptr;
}

void AAIUnitTable::AddExtractor(int unit_id)
{
	extractors.insert(unit_id);
}

void AAIUnitTable::RemoveExtractor(int unit_id)
{
	extractors.erase(unit_id);
}

void AAIUnitTable::AddScout(int unit_id)
{
	scouts.insert(unit_id);
}

void AAIUnitTable::RemoveScout(int unit_id)
{
	scouts.erase(unit_id);
}

void AAIUnitTable::AddPowerPlant(UnitId unitId, UnitDefId unitDefId)
{
	power_plants.insert(unitId.id);
	ai->Getexecute()->futureAvailableEnergy -= ai->s_buildTree.GetPrimaryAbility(unitDefId);
}

void AAIUnitTable::RemovePowerPlant(int unit_id)
{
	power_plants.erase(unit_id);
}

void AAIUnitTable::AddMetalMaker(int unit_id, int def_id)
{
	metal_makers.insert(unit_id);
}

void AAIUnitTable::RemoveMetalMaker(int unit_id)
{
	if(!ai->GetAICallback()->IsUnitActivated(unit_id))
		--ai->Getexecute()->disabledMMakers;

	metal_makers.erase(unit_id);
}

void AAIUnitTable::AddRecon(int unit_id, int def_id)
{
	recon.insert(unit_id);
}

void AAIUnitTable::RemoveRecon(int unit_id)
{
	recon.erase(unit_id);
}

void AAIUnitTable::AddJammer(int unit_id, int def_id)
{
	jammers.insert(unit_id);
}

void AAIUnitTable::RemoveJammer(int unit_id)
{
	jammers.erase(unit_id);
}

void AAIUnitTable::AddStationaryArty(int unit_id, int /*def_id*/)
{
	stationary_arty.insert(unit_id);
}

void AAIUnitTable::RemoveStationaryArty(int unit_id)
{
	stationary_arty.erase(unit_id);
}

AAIConstructor* AAIUnitTable::FindBuilder(int building, bool commander)
{
	AAIConstructor *builder;

	// look for idle builder
	for(set<int>::iterator i = constructors.begin(); i != constructors.end(); ++i)
	{
		// check all builders
		if( IsBuilder(units[*i].cons->m_myDefId) == true )
		{
			builder = units[*i].cons;

			// find unit that can directly build that building
			if(    (builder->IsAvailableForConstruction() == true) 
				&& (ai->s_buildTree.CanBuildUnitType(builder->m_myDefId.id, building) == true) )
			{
				// filter out commander (if not allowed)
				if(! (!commander &&  ai->s_buildTree.GetUnitCategory(builder->m_myDefId).IsCommander() ) )
					return builder;
			}
		}
	}

	// no builder found
	return 0;
}

AAIConstructor* AAIUnitTable::FindClosestBuilder(UnitDefId building, const float3 *pos, bool commander, float *timeToReachPosition)
{
	const int continent = ai->Getmap()->GetContinentID(*pos);

	*timeToReachPosition = 0.0f;
	AAIConstructor *selectedBuilder(nullptr);

	// look for idle builder
	for(set<int>::iterator i = constructors.begin(); i != constructors.end(); ++i)
	{
		// check all builders
		if(IsBuilder(units[*i].cons->m_myDefId))
		{
			AAIConstructor* builder = units[*i].cons;

			// find idle or assisting builder, who can build this building
			if(    builder->IsAvailableForConstruction()
				&& ai->s_buildTree.CanBuildUnitType(builder->m_myDefId, building) )
			{
				const float3 builderPosition = ai->GetAICallback()->GetUnitPos(builder->m_myUnitId.id);

				const bool continentCheckPassed =    (ai->s_buildTree.GetMovementType(builder->m_myDefId).CannotMoveToOtherContinents() == false) 
												  || (ai->Getmap()->GetContinentID(builderPosition) == continent);
				const bool commanderCheckPassed = commander
												  || ! ai->s_buildTree.GetUnitCategory(builder->m_myDefId).IsCommander();

				// filter out commander
				if(continentCheckPassed && commanderCheckPassed)
				{
					float travelTime = fastmath::apxsqrt( (builderPosition.x - pos->x) * (builderPosition.x - pos->x) + (builderPosition.z - pos->z) * (builderPosition.z - pos->z) );

					const float maxSpeed = ai->s_buildTree.GetMaxSpeed(builder->m_myDefId);
					if(maxSpeed > 0.0f)
						travelTime /= maxSpeed;

					if( (travelTime < *timeToReachPosition) || (selectedBuilder == nullptr))
					{
						selectedBuilder      = builder;
						*timeToReachPosition = travelTime;
					}
				}
			}
		}
	}

	return selectedBuilder;
}

AAIConstructor* AAIUnitTable::FindClosestAssistant(const float3& pos, int /*importance*/, bool commander)
{
	const int continent = ai->Getmap()->GetContinentID(pos);
	AAIConstructor *selectedAssistant(nullptr);
	float maxDist(0.0f);

	// find idle builder
	for(auto i = constructors.begin(); i != constructors.end(); ++i)
	{
		// check all assisters
		if(IsAssister(units[*i].cons->m_myDefId))
		{
			AAIConstructor* assistant = units[*i].cons;

			// find idle assister
			if(assistant->IsIdle())
			{
				const float3 assistantPosition = ai->GetAICallback()->GetUnitPos(assistant->m_myUnitId.id);
				const AAIMovementType& moveType = ai->s_buildTree.GetMovementType(assistant->m_myDefId.id);

				const bool continentCheckPassed = (moveType.CannotMoveToOtherContinents() == false) || (ai->Getmap()->GetContinentID(assistantPosition) == continent);
				const bool commanderCheckPassed = (commander || (ai->s_buildTree.GetUnitCategory(assistant->m_myDefId).IsCommander() == false) );

				// filter out commander
				if(continentCheckPassed && commanderCheckPassed)
				{
					const float dx = (pos.x - assistantPosition.x);
					const float dy = (pos.z - assistantPosition.z);
					const float squaredDist = dx * dx + dy * dy;

					if( (squaredDist < maxDist) || (maxDist == 0.0f) )
					{
						maxDist = squaredDist;
						selectedAssistant = assistant;
					}
				}
			}
		}
	}

	// no assister found -> request one
	/*if(!best_assistant)
	{
		uint32_t allowedMovementTypes =   static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_AIR)
										+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_HOVER);

		if(ai->GetAICallback()->GetElevation(pos.x, pos.z) < 0.0f)
		{
			allowedMovementTypes |= static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_SEA_FLOATER);
			allowedMovementTypes |= static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_SEA_SUBMERGED);
		}
		else
		{
			allowedMovementTypes |= static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_GROUND);
		}

		ai->Getbt()->AddAssistant(allowedMovementTypes, true);
	}*/

	return selectedAssistant;
}

void AAIUnitTable::EnemyKilled(int unit)
{
	if(units[unit].status == BOMB_TARGET)
		ai->Getaf()->RemoveTarget(unit);


	if(units[unit].group)
		units[unit].group->TargetUnitKilled();

	RemoveUnit(unit);
}

void AAIUnitTable::AssignGroupToEnemy(int unit, AAIGroup *group)
{
	units[unit].unit_id = unit;
	units[unit].group = group;
	units[unit].status = ENEMY_UNIT;
}


void AAIUnitTable::SetUnitStatus(int unit, UnitTask status)
{
	units[unit].status = status;
}

bool AAIUnitTable::IsBuilder(UnitId unitId)
{
	if(units[unitId.id].cons != nullptr)
	{
		return IsBuilder(units[unitId.id].cons->m_myDefId);
	}
	return false;
}

void AAIUnitTable::UnitRequested(const AAIUnitCategory& category, int number)
{
	m_requestedUnitsOfCategory[category.GetArrayIndex()] += number;
}

void AAIUnitTable::UnitRequestFailed(const AAIUnitCategory& category)
{
	--m_requestedUnitsOfCategory[category.GetArrayIndex()];
}

void AAIUnitTable::UnitCreated(const AAIUnitCategory& category)
{
	--m_requestedUnitsOfCategory[category.GetArrayIndex()];
	++m_underConstructionUnitsOfCategory[category.GetArrayIndex()];
}

void AAIUnitTable::UnitUnderConstructionKilled(const AAIUnitCategory& category)
{
	--m_underConstructionUnitsOfCategory[category.GetArrayIndex()];
}

void AAIUnitTable::UnitFinished(const AAIUnitCategory& category)
{
	--m_underConstructionUnitsOfCategory[category.GetArrayIndex()];
	++m_activeUnitsOfCategory[category.GetArrayIndex()];
}

void AAIUnitTable::ActiveUnitKilled(const AAIUnitCategory& category)
{
	--m_activeUnitsOfCategory[category.GetArrayIndex()];
}
