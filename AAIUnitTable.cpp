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
		units[i].group = 0;
		units[i].cons = nullptr;
		units[i].status = UNIT_KILLED;
		units[i].last_order = 0;
	}


	m_activeUnitsOfCategory.resize(AAIUnitCategory::numberOfUnitCategories, 0);
	m_underConstructionUnitsOfCategory.resize(AAIUnitCategory::numberOfUnitCategories, 0);
	m_requestedUnitsOfCategory.resize(AAIUnitCategory::numberOfUnitCategories, 0);

	activeBuilders = futureBuilders = 0;
	activeFactories = futureFactories = 0;

	cmdr = -1;
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
	AAIConstructor *cons = new AAIConstructor(ai, unitId, unitDefId, IsFactory(unitDefId), IsBuilder(unitDefId), IsAssister(unitDefId), ai->Getexecute()->GetBuildqueueOfFactory(unitDefId.id));

	constructors.insert(unitId.id);
	units[unitId.id].cons = cons;

	// increase/decrease number of available/requested builders for all buildoptions of the builder
	ai->Getbt()->ConstructorFinished(unitDefId);

	if(IsBuilder(unitDefId) == true)
	{
		--futureBuilders;
		++activeBuilders;
	}

	if( (IsFactory(unitDefId) == true) && (ai->Getbt()->s_buildTree.GetMovementType(unitDefId).isStatic() == true) )
	{
		--futureFactories;
		++activeFactories;

		// remove future ressource demand now factory has been finished
		ai->Getexecute()->futureRequestedMetal  -= ai->Getbt()->units_static[unitDefId.id].efficiency[0];
		ai->Getexecute()->futureRequestedEnergy -= ai->Getbt()->units_static[unitDefId.id].efficiency[1];
	}
}

void AAIUnitTable::RemoveConstructor(int unit_id, int def_id)
{
	if(IsBuilder(UnitDefId(def_id)) == true)
		activeBuilders -= 1;

	if( (IsFactory(UnitDefId(def_id)) == true) && (ai->Getbt()->s_buildTree.GetMovementType(UnitDefId(def_id)).isStatic() == true) )
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
	AAIConstructor *cons = new AAIConstructor(ai, unitId, unitDefId, IsFactory(unitDefId), IsBuilder(unitDefId), IsAssister(unitDefId), ai->Getexecute()->GetBuildqueueOfFactory(unitDefId.id));
	units[unitId.id].cons = cons;

	constructors.insert(unitId.id);

	cmdr = unitId.id;

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

	// commander has been destroyed, set pointer to zero
	if(unit_id == cmdr)
		cmdr = -1;
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

void AAIUnitTable::AddPowerPlant(int unit_id, int def_id)
{
	power_plants.insert(unit_id);
	ai->Getexecute()->futureAvailableEnergy -= ai->Getbt()->units_static[def_id].efficiency[0];
}

void AAIUnitTable::RemovePowerPlant(int unit_id)
{
	power_plants.erase(unit_id);
}

void AAIUnitTable::AddMetalMaker(int unit_id, int def_id)
{
	metal_makers.insert(unit_id);
	ai->Getexecute()->futureRequestedEnergy -= ai->Getbt()->GetUnitDef(def_id).energyUpkeep;
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

	ai->Getexecute()->futureRequestedEnergy -= ai->Getbt()->units_static[def_id].efficiency[0];
}

void AAIUnitTable::RemoveRecon(int unit_id)
{
	recon.erase(unit_id);
}

void AAIUnitTable::AddJammer(int unit_id, int def_id)
{
	jammers.insert(unit_id);

	ai->Getexecute()->futureRequestedEnergy -= ai->Getbt()->units_static[def_id].efficiency[0];
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
				&& (ai->Getbt()->s_buildTree.CanBuildUnitType(builder->m_myDefId.id, building) == true) )
			{
				// filter out commander (if not allowed)
				if(! (!commander &&  ai->Getbt()->s_buildTree.GetUnitCategory(builder->m_myDefId).isCommander() ) )
					return builder;
			}
		}
	}

	// no builder found
	return 0;
}

AAIConstructor* AAIUnitTable::FindClosestBuilder(int building, const float3 *pos, bool commander, float *min_dist)
{
	float my_dist;
	AAIConstructor *best_builder = nullptr, *builder;
	float3 builder_pos;
	bool suitable;

	int continent = ai->Getmap()->GetContinentID(*pos);
	*min_dist = 100000.0f;

	// look for idle builder
	for(set<int>::iterator i = constructors.begin(); i != constructors.end(); ++i)
	{
		// check all builders
		if(IsBuilder(units[*i].cons->m_myDefId) == true)
		{
			builder = units[*i].cons;

			// find idle or assisting builder, who can build this building
			if(    (builder->IsAvailableForConstruction() == true) 
				&& ( ai->Getbt()->s_buildTree.CanBuildUnitType(builder->m_myDefId.id, building) == true) )
			{
				builder_pos = ai->GetAICallback()->GetUnitPos(builder->m_myUnitId.id);

				const AAIMovementType& moveType = ai->Getbt()->s_buildTree.GetMovementType(builder->m_myDefId);

				// check continent if necessary
				if( moveType.cannotMoveToOtherContinents() )
				{
					if(ai->Getmap()->GetContinentID(builder_pos) == continent)
						suitable = true;
					else
						suitable = false;
				}
				else
					suitable = true;

				// filter out commander
				if(suitable && ( commander || ! ai->Getbt()->s_buildTree.GetUnitCategory(builder->m_myDefId).isCommander() ) )
				{
					my_dist = fastmath::apxsqrt( (builder_pos.x - pos->x) * (builder_pos.x - pos->x) + (builder_pos.z - pos->z) * (builder_pos.z - pos->z) );

					if(ai->Getbt()->GetUnitDef(builder->m_myDefId.id).speed > 0)
						my_dist /= ai->Getbt()->GetUnitDef(builder->m_myDefId.id).speed;

					if(my_dist < *min_dist)
					{
						best_builder = builder;
						*min_dist = my_dist;
					}
				}
			}
		}
	}

	return best_builder;
}

AAIConstructor* AAIUnitTable::FindClosestAssistant(float3 pos, int /*importance*/, bool commander)
{
	AAIConstructor *best_assistant(nullptr);
	float maxDist( 0.0f );
	float3 assistant_pos;
	bool suitable;

	int continent = ai->Getmap()->GetContinentID(pos);

	// find idle builder
	for(set<int>::iterator i = constructors.begin(); i != constructors.end(); ++i)
	{
		// check all assisters
		if(IsAssister(units[*i].cons->m_myDefId) == true)
		{
			AAIConstructor* assistant = units[*i].cons;

			// find idle assister
			if(assistant->IsIdle() == true)
			{
				assistant_pos = ai->GetAICallback()->GetUnitPos(assistant->m_myUnitId.id);

				const AAIMovementType& moveType = ai->Getbt()->s_buildTree.GetMovementType(assistant->m_myDefId.id);

				// check continent if necessary
				if( moveType.cannotMoveToOtherContinents() )
				{
					if(ai->Getmap()->GetContinentID(assistant_pos) == continent)
						suitable = true;
					else
						suitable = false;
				}
				else
					suitable = true;

				// filter out commander
				if(suitable && ( commander || !ai->Getbt()->s_buildTree.GetUnitCategory(assistant->m_myDefId).isCommander() ) )
				{
					const float dist = (pos.x - assistant_pos.x) * (pos.x - assistant_pos.x) + (pos.z - assistant_pos.z) * (pos.z - assistant_pos.z);

					if( (dist < maxDist) || (maxDist == 0.0f) )
					{
						maxDist = dist;
						best_assistant = assistant;
					}
				}
			}
		}
	}

	// no assister found -> request one
	if(!best_assistant)
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

		//ai->Getbt()->AddAssistant(allowedMovementTypes, true);
	}

	return best_assistant;
}

bool AAIUnitTable::IsUnitCommander(int unit_id)
{
	if(cmdr != -1)
		return false;
	else if(cmdr == unit_id)
		return true;
	else
		return false;
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
