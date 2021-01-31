// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include <set>

#include "AAI.h"
#include "AAIConstructor.h"
#include "AAIBuildTask.h"
#include "AAIExecute.h"
#include "AAIBrain.h"
#include "AAIBuildTable.h"
#include "AAIUnitTable.h"
#include "AAIConfig.h"
#include "AAIMap.h"
#include "AAISector.h"


#include "LegacyCpp/UnitDef.h"
#include "LegacyCpp/CommandQueue.h"
using namespace springLegacyAI;


AAIConstructor::AAIConstructor(AAI *ai, UnitId unitId, UnitDefId defId, bool factory, bool builder, bool assistant, std::list<UnitDefId>* buildqueue) :
	m_myUnitId(unitId),
	m_myDefId(defId),
	m_constructedUnitId(),
	m_constructedDefId(),
	m_isFactory(factory),
	m_isBuilder(builder),
	m_isAssistant(assistant),
 	m_buildPos(ZeroVector),
	m_assistUnitId(),
	m_activity(EConstructorActivity::IDLE),
	m_buildqueue(buildqueue)
{
	this->ai = ai;
	build_task = 0;
}

AAIConstructor::~AAIConstructor(void)
{
}

bool AAIConstructor::isBusy() const
{
	const CCommandQueue *commands = ai->GetAICallback()->GetCurrentUnitCommands(m_myUnitId.id);

	if(commands->empty())
		return false;
	else
		return true;
}

void AAIConstructor::Idle()
{
	if(m_isBuilder)
	{
		if(m_activity.IsCarryingOutConstructionOrder() == true)
		{
			if(m_constructedUnitId.IsValid() == false)
			{
				//ai->Getbt()->units_dynamic[construction_def_id].active -= 1;
				//assert(ai->Getbt()->units_dynamic[construction_def_id].active >= 0);
				ai->Getut()->UnitRequestFailed(ai->s_buildTree.GetUnitCategory(m_constructedDefId));

				// clear up buildmap etc. (make sure conctructor wanted to build a building and not a unit)
				if( ai->s_buildTree.GetMovementType(m_constructedDefId).IsStatic() == true )
					ai->Getexecute()->ConstructionFailed(m_buildPos, m_constructedDefId.id);

				// free builder
				ConstructionFinished();
			}
		}
		else if(m_activity.IsDestroyed() == false)
		{
			m_activity.SetActivity(EConstructorActivity::IDLE);
			m_assistUnitId.Invalidate();

			ReleaseAllAssistants();
		}
	}

	if(m_isFactory)
	{
		ConstructionFinished();

		Update();
	}
}

void AAIConstructor::Update()
{
	if(m_isFactory && m_buildqueue != nullptr)
	{
		if( (m_activity.IsConstructing() == false) && (m_buildqueue->empty() == false) )
		{
			UnitDefId constructedUnitDefId(*m_buildqueue->begin());

			// check if mobile or stationary builder
			if(ai->s_buildTree.GetMovementType(m_myDefId).IsStatic() == true )  
			{
				// give build order
				Command c(-constructedUnitDefId.id);
				ai->GetAICallback()->GiveOrder(m_myUnitId.id, &c);

				m_constructedDefId = constructedUnitDefId.id;
				m_activity.SetActivity(EConstructorActivity::CONSTRUCTING);

				//if(ai->Getbt()->IsFactory(def_id))
				//	++ai->futureFactories;

				m_buildqueue->pop_front();
			}
			else
			{
				// find buildpos for the unit
				const BuildSite buildSite = ai->Getexecute()->DetermineBuildsiteForUnit(m_myUnitId, constructedUnitDefId);

				if(buildSite.IsValid())
				{
					// give build order
					Command c(-constructedUnitDefId.id);
					c.PushPos(buildSite.Position());

					ai->GetAICallback()->GiveOrder(m_myUnitId.id, &c);
					m_constructedDefId = constructedUnitDefId.id;
					m_activity.SetActivity(EConstructorActivity::CONSTRUCTING); //! @todo Should be HEADING_TO_BUILDSITE

					ai->Getut()->UnitRequested(ai->s_buildTree.GetUnitCategory(constructedUnitDefId)); // request must be called before create to keep unit counters correct
					//ai->Getut()->UnitCreated(ai->s_buildTree.GetUnitCategory(constructedUnitDefId));

					m_buildqueue->pop_front();
				}
			}

			return;
		}

		CheckAssistance();
	}

	if(m_isBuilder)
	{
		if(m_activity.IsCarryingOutConstructionOrder() == true)
		{
			// if building has begun, check for possible assisters
			if(m_constructedUnitId.IsValid() == true)
				CheckAssistance();
			// if building has not yet begun, check if something unexpected happened (buildsite blocked)
			else if(isBusy() == false) // && !ai->Getbt()->IsValidUnitDefID(construction_unit_id))
			{
				ConstructionFailed();
			}
		}
		/*else if(task == UNIT_IDLE)
		{
			float3 pos = ai->Getcb()->GetUnitPos(unit_id);

			if(pos.x > 0)
			{
				int x = pos.x/ai->Getmap()->xSectorSize;
				int y = pos.z/ai->Getmap()->ySectorSize;

				if(x >= 0 && y >= 0 && x < ai->Getmap()->xSectors && y < ai->Getmap()->ySectors)
				{
					if(ai->Getmap()->sector[x][y].distance_to_base < 2)
					{
						pos = ai->Getmap()->sector[x][y].GetCenter();

						Command c;
						const UnitDef *def;

						def = ai->Getcb()->GetUnitDef(unit_id);
						// can this thing resurrect? If so, maybe we should raise the corpses instead of consuming them?
						if(def->canResurrect)
						{
							if(rand()%2 == 1)
								c.id = CMD_RESURRECT;
							else
								c.id = CMD_RECLAIM;
						}
						else
							c.id = CMD_RECLAIM;

						c.PushParam(pos.x);
						c.PushParam(ai->Getcb()->GetElevation(pos.x, pos.z));
						c.PushParam(pos.z);
						c.PushParam(500.0);

						//ai->Getcb()->GiveOrder(unit_id, &c);
						task = RECLAIMING;
						ai->Getexecute()->GiveOrder(&c, unit_id, "Builder::Reclaming");
					}
				}
			}
		}
		*/
	}
}

bool AAIConstructor::IsAssitanceByNanoTurretDesired() const
{
	//! @todo Take production time of units into account
	return ai->s_buildTree.GetMovementType(m_myDefId).IsStatic() && (m_buildqueue->size() >= cfg->MAX_BUILDQUE_SIZE - 2);
}

void AAIConstructor::CheckAssistance()
{
	//-----------------------------------------------------------------------------------------------------------------
	// Check construction assistance for factories
	//-----------------------------------------------------------------------------------------------------------------
	if(m_isFactory && (m_buildqueue != nullptr))
	{
		// check if another factory of that type needed
		if( (m_buildqueue->size() >= cfg->MAX_BUILDQUE_SIZE - 1) && (assistants.size() > 1) )
		{
			if(ai->Getbt()->GetTotalNumberOfUnits(m_myDefId.id) < cfg->MAX_FACTORIES_PER_TYPE)
			{
				ai->Getbt()->units_dynamic[m_myDefId.id].requested += 1;

				ai->Getbt()->ConstructorRequested(m_myDefId);
			}
		}

		// check if support needed
		const bool assistanceNeeded = DoesFactoryNeedAssistance();

		if(assistanceNeeded)
		{
			AAIConstructor* assistant = ai->Getut()->FindClosestAssistant(ai->GetAICallback()->GetUnitPos(m_myUnitId.id), 5, true);

			if(assistant)
			{
				assistants.insert(assistant->m_myUnitId.id);
				assistant->AssistConstruction(m_myUnitId, true);
			}
		}
		// check if assistants are needed anymore
		else if(!assistants.empty() && m_buildqueue->empty() && (m_constructedDefId.IsValid() == false))
		{
			//ai->LogConsole("factory releasing assistants");
			ReleaseAllAssistants();
		}
	}

	//-----------------------------------------------------------------------------------------------------------------
	// Check construction assistance for builders
	//-----------------------------------------------------------------------------------------------------------------
	if(m_isBuilder && build_task)
	{
		// prevent assisting when low on ressources
		if(    ai->Getbrain()->SufficientResourcesToAssistsConstructionOf(m_constructedDefId)
			&& (GetBuildtimeOfUnit(m_constructedDefId) > static_cast<float>(cfg->MIN_ASSISTANCE_BUILDTIME) ) 
			&& (assistants.size() < cfg->MAX_ASSISTANTS))
		{
			// commander only allowed if buildpos is inside the base
			const AAISector* sector = ai->Getmap()->GetSectorOfPos(m_buildPos);
			const bool commanderAllowed = (sector && (sector->GetDistanceToBase() == 0) ) ? true : false;

			AAIConstructor* assistant = ai->Getut()->FindClosestAssistant(m_buildPos, 5, commanderAllowed);

			if(assistant)
			{
				assistants.insert(assistant->m_myUnitId.id);
				assistant->AssistConstruction(m_myUnitId);
			}
		}	
	}
}

bool AAIConstructor::DoesFactoryNeedAssistance() const
{
	if(assistants.size() < cfg->MAX_ASSISTANTS)
	{
		if(m_buildqueue->size() > 2)
			return true;

		if(m_constructedDefId.IsValid() ) 
		{
			const float buildtime = GetBuildtimeOfUnit(m_constructedDefId);

			if (buildtime > static_cast<float>(cfg->MIN_ASSISTANCE_BUILDTIME))
				return true;
		}
	}

	return false;
}

float AAIConstructor::GetBuildtimeOfUnit(UnitDefId constructedUnitDefId) const
{
	const float buildspeed( ai->s_buildTree.GetBuildspeed(m_myDefId) ); 

	if(buildspeed > 0.0f)
		return ai->s_buildTree.GetBuildtime(constructedUnitDefId) / buildspeed;
	else
		return 0.0f;
}

void AAIConstructor::GiveReclaimOrder(UnitId unitId)
{
	if(m_assistUnitId.IsValid())
	{
		ai->Getut()->units[m_assistUnitId.id].cons->RemoveAssitant(m_myUnitId.id);
		m_assistUnitId.Invalidate();
	}

	m_activity.SetActivity(EConstructorActivity::RECLAIMING);

	Command c(CMD_RECLAIM);
	c.PushParam(unitId.id);
	//ai->Getcb()->GiveOrder(this->unit_id, &c);
	ai->Getexecute()->GiveOrder(&c, m_myUnitId.id, "Builder::GiveRelaimOrder");
}


void AAIConstructor::GiveConstructionOrder(UnitDefId building, const float3& pos)
{
	// get def and final position
	const springLegacyAI::UnitDef *def = &ai->Getbt()->GetUnitDef(building.id);

	// give order if building can be placed at the desired position (position lies within a valid sector)
	const bool buildingInitializationSuccessful = ai->Getmap()->InitBuilding(def, pos);

	if(buildingInitializationSuccessful)
	{
		// check if builder was previously assisting other builders/factories
		if(m_assistUnitId.IsValid())
		{
			ai->Getut()->units[m_assistUnitId.id].cons->RemoveAssitant(m_myUnitId.id);
			m_assistUnitId.Invalidate();
		}

		// set building as current task and order construction
		m_buildPos         = pos;
		m_constructedDefId = building;

		m_activity.SetActivity(EConstructorActivity::HEADING_TO_BUILDSITE);

		// order builder to construct building
		Command c(-m_constructedDefId.id);
		c.PushPos(m_buildPos);

		ai->GetAICallback()->GiveOrder(m_myUnitId.id, &c);

		// increase number of active units of that type/category
		ai->Getbt()->units_dynamic[def->id].requested += 1;

		ai->Getut()->UnitRequested(ai->s_buildTree.GetUnitCategory(building));

		if(ai->s_buildTree.GetUnitType(building).IsFactory())
			ai->Getut()->futureFactories += 1;
	}
}

void AAIConstructor::AssistConstruction(UnitId constructorUnitId, bool factory)
{
	//Command c(factory ? CMD_GUARD: CMD_REPAIR);
	Command c(CMD_GUARD);
	c.PushParam(constructorUnitId.id);

	//ai->Getcb()->GiveOrder(unit_id, &c);
	ai->Getexecute()->GiveOrder(&c, m_myUnitId.id, "Builder::Assist");

	m_activity.SetActivity(EConstructorActivity::ASSISTING);
	m_assistUnitId = UnitId(constructorUnitId.id);
}

void AAIConstructor::TakeOverConstruction(AAIBuildTask *build_task)
{
	if(m_assistUnitId.IsValid())
	{
		ai->Getut()->units[m_assistUnitId.id].cons->RemoveAssitant(m_myUnitId.id);
		m_assistUnitId.Invalidate();
	}

	m_constructedDefId  = build_task->m_defId;
	m_constructedUnitId = build_task->m_unitId;
	assert(m_constructedDefId.IsValid());
	assert(m_constructedUnitId.IsValid());

	m_buildPos = build_task->m_buildsite;

	Command c(CMD_REPAIR);
	c.PushParam(build_task->m_unitId.id);

	m_activity.SetActivity(EConstructorActivity::CONSTRUCTING);
	ai->GetAICallback()->GiveOrder(m_myUnitId.id, &c);
}

void AAIConstructor::CheckIfConstructionFailed()
{
	if( (m_activity.IsCarryingOutConstructionOrder() == true) && (m_constructedUnitId.IsValid() == false))
	{
		ConstructionFailed();
	}
}

void AAIConstructor::ConstructionFailed()
{
	--ai->Getbt()->units_dynamic[m_constructedDefId.id].requested;
	ai->Getut()->UnitRequestFailed(ai->s_buildTree.GetUnitCategory(m_constructedDefId));

	// clear up buildmap etc.
	if(ai->s_buildTree.GetMovementType(m_constructedDefId).IsStatic() == true)
		ai->Getexecute()->ConstructionFailed(m_buildPos, m_constructedDefId.id);

	// tells the builder construction has finished
	ConstructionFinished();
}


void AAIConstructor::ConstructionStarted(UnitId unitId, AAIBuildTask *buildTask)
{
	m_constructedUnitId = unitId;
	build_task = buildTask;
	m_activity.SetActivity(EConstructorActivity::CONSTRUCTING);
	CheckAssistance();
}

void AAIConstructor::ConstructionFinished()
{
  	m_activity.SetActivity(EConstructorActivity::IDLE);

	m_buildPos = ZeroVector;
	m_constructedUnitId.Invalidate();
	m_constructedDefId.Invalidate();

	build_task = nullptr;

	// release assisters
	ReleaseAllAssistants();
}

void AAIConstructor::ReleaseAllAssistants()
{
	// release assisters
	for(set<int>::iterator i = assistants.begin(); i != assistants.end(); ++i)
	{
		 if(ai->Getut()->units[*i].cons)
			 ai->Getut()->units[*i].cons->StopAssisting();
	}

	assistants.clear();
}

void AAIConstructor::StopAssisting()
{
	m_activity.SetActivity(EConstructorActivity::IDLE);
	m_assistUnitId.Invalidate();

	Command c(CMD_STOP);
	//ai->Getcb()->GiveOrder(unit_id, &c);
	ai->Getexecute()->GiveOrder(&c, m_myUnitId.id, "Builder::StopAssisting");
}
void AAIConstructor::RemoveAssitant(int unit_id)
{
	assistants.erase(unit_id);
}
void AAIConstructor::Killed()
{
	// when builder was killed on the way to the buildsite, inform ai that construction
	// of building hasnt been started
	if(m_activity.IsHeadingToBuildsite() == true)
	{
		// clear up buildmap etc.
		ConstructionFailed();
	}
	else if(m_activity.IsConstructing() == true)
	{
		if(build_task)
			build_task->BuilderDestroyed(ai->Getmap(), ai->Getut());
	}
	else if(m_activity.IsAssisting() == true)
	{
			ai->Getut()->units[m_assistUnitId.id].cons->RemoveAssitant(m_myUnitId.id);
	}

	ReleaseAllAssistants();
	m_activity.SetActivity(EConstructorActivity::DESTROYED);
}

void AAIConstructor::CheckRetreatFromAttackBy(const AAIUnitCategory& attackedByCategory)
{
	if(m_activity.IsDestroyed() == false)
	{
		const float3 unitPos = ai->GetAICallback()->GetUnitPos(m_myUnitId.id);

		const AAISector* sector = ai->Getmap()->GetSectorOfPos(unitPos);

		if(sector)
		{
			// dont flee within base
			if(sector->GetDistanceToBase() == 0)
				return;
			else
			{
				// dont flee outside from scouts of the base if health is > 50%
				if(   attackedByCategory.IsScout()
				   && (ai->GetAICallback()->GetUnitHealth(m_myUnitId.id) > 0.5f * ai->s_buildTree.GetHealth(m_myDefId)) )
					return;	
			}
		}

		const float3 retreatPos = ai->Getexecute()->DetermineSafePos(m_myDefId, unitPos);

		if(retreatPos.x > 0.0f)
		{
			Command c(CMD_MOVE);
			c.PushParam(retreatPos.x);
			c.PushParam(ai->GetAICallback()->GetElevation(retreatPos.x, retreatPos.z));
			c.PushParam(retreatPos.z);

			ai->Getexecute()->GiveOrder(&c, m_myUnitId.id, "BuilderRetreat");
			//ai->Getcb()->GiveOrder(unit_id, &c);
		}
	}
}
