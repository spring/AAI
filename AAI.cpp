// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include <math.h>
#include <stdarg.h>
#include <time.h>

#include "AAI.h"
#include "AAIBuildTable.h"
#include "AAIAirForceManager.h"
#include "AAIExecute.h"
#include "AAIUnitTable.h"
#include "AAIBuildTask.h"
#include "AAIBrain.h"
#include "AAIConstructor.h"
#include "AAIAttackManager.h"
#include "AIExport.h"
#include "AAIConfig.h"
#include "AAIMap.h"
#include "AAIGroup.h"
#include "AAISector.h"
#include "AAIUnitTypes.h"


#include "System/SafeUtil.h"

#include "LegacyCpp/IGlobalAICallback.h"
#include "LegacyCpp/UnitDef.h"
using namespace springLegacyAI;


#include "CUtils/SimpleProfiler.h"
#define AAI_SCOPED_TIMER(part) SCOPED_TIMER(part, profiler);

// C++ < C++17 does not support initialization of static cont within class declaration
const std::vector<int> GamePhase::m_startFrameOfGamePhase  = {0, 10800, 27000, 72000};
const std::vector<std::string> GamePhase::m_gamePhaseNames = {"starting phase", "early phase", "mid phase", "late game"};
const std::vector<std::string> AAICombatCategory::m_combatCategoryNames = {"surface", "air", "floater", "submerged"};

AAIBuildTree AAI::s_buildTree;

int AAI::s_aaiInstances = 0;

AAI::AAI() :
	m_aiCallback(nullptr),
	brain(nullptr),
	execute(nullptr),
	ut(nullptr),
	bt(nullptr),
	map(nullptr),
	af(nullptr),
	am(nullptr),
	profiler(nullptr),
	m_side(0),
	m_logFile(nullptr),
	m_initialized(false),
	m_configLoaded(false),
	m_aaiInstance(0),
	m_gamePhase(0)
{
	// initialize random numbers generator
	srand (time(nullptr));
}

AAI::~AAI()
{
	--s_aaiInstances;
	if (m_initialized == false)
		return;

	// save several AI data
	Log("\nShutting down....\n\n");

	Log("Linking buildtask to builder failed counter: %u\n", execute->GetLinkingBuildTaskToBuilderFailedCounter());

	Log("Unit category active / under construction / requested\n");
	for(AAIUnitCategory category(AAIUnitCategory::GetFirst()); category.End() == false; category.Next())
	{
		Log("%s: %i / %i / %i\n", s_buildTree.GetCategoryName(category).c_str(), 
								ut->GetNumberOfActiveUnitsOfCategory(category), 
								ut->GetNumberOfUnitsUnderConstructionOfCategory(category), 
								ut->GetNumberOfRequestedUnitsOfCategory(category));
	}

	Log("\nGround Groups:    " _STPF_ "\n", group_list[AAIUnitCategory(EUnitCategory::GROUND_COMBAT).GetArrayIndex()].size());
	Log("\nAir Groups:       " _STPF_ "\n", group_list[AAIUnitCategory(EUnitCategory::AIR_COMBAT).GetArrayIndex()].size());
	Log("\nHover Groups:     " _STPF_ "\n", group_list[AAIUnitCategory(EUnitCategory::HOVER_COMBAT).GetArrayIndex()].size());
	Log("\nSea Groups:       " _STPF_ "\n", group_list[AAIUnitCategory(EUnitCategory::SEA_COMBAT).GetArrayIndex()].size());
	Log("\nSubmarine Groups: " _STPF_ "\n\n", group_list[AAIUnitCategory(EUnitCategory::SUBMARINE_COMBAT).GetArrayIndex()].size());

	Log("Future metal/energy request: %i / %i\n", (int)execute->futureRequestedMetal, (int)execute->futureRequestedEnergy);
	Log("Future metal/energy supply:  %i / %i\n\n", (int)execute->futureAvailableMetal, (int)execute->futureAvailableEnergy);

	Log("Future/active builders:      %i / %i\n", ut->futureBuilders, ut->activeBuilders);
	Log("Future/active factories:     %i / %i\n\n", ut->futureFactories, ut->activeFactories);

	Log("Unit production rate: %i\n\n", execute->unitProductionRate);

	Log("Requested constructors:\n");
	for(auto fac = s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, m_side).begin(); fac != s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, m_side).end(); ++fac)
	{
		Log("%-24s: %i\n", s_buildTree.GetUnitTypeProperties(*fac).m_name.c_str(), bt->units_dynamic[fac->id].requested);
	}
	for(auto builder = s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, m_side).begin(); builder != s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, m_side).end(); ++builder)
		Log("%-24s: %i\n", s_buildTree.GetUnitTypeProperties(*builder).m_name.c_str(), bt->units_dynamic[builder->id].requested);

	GamePhase gamePhase(m_aiCallback->GetCurrentFrame());
	const AttackedByRatesPerGamePhase& attackedByRates = brain->GetAttackedByRates();

	Log("\nAttack frequencies (combat unit category / frequency) \n");
	for(GamePhase gamePhaseIterator(0); gamePhaseIterator <= gamePhase; gamePhaseIterator.EnterNextGamePhase())
	{
		Log("Game phase %s:", gamePhaseIterator.GetName().c_str());
		for(AAICombatCategory category(AAICombatCategory::first); category.End() == false; category.Next())
		{
			Log("  %s: %f", category.GetName().c_str(), attackedByRates.GetAttackRate(gamePhaseIterator, category));
		}
		Log("\n");
	}

	// delete buildtasks
	for(list<AAIBuildTask*>::iterator task = build_tasks.begin(); task != build_tasks.end(); ++task)
	{
		delete (*task);
	}
	build_tasks.clear();

	// save game learning data
	bt->SaveBuildTable(gamePhase, brain->GetAttackedByRates(), map->map_type);

	spring::SafeDelete(am);
	spring::SafeDelete(af);

	// delete unit groups
	for(auto groupList = group_list.begin(); groupList != group_list.end(); ++groupList)
	{
		for(std::list<AAIGroup*>::iterator group = groupList->begin(); group != groupList->end(); ++group)
		{
			(*group)->attack = 0;
			delete (*group);
		}
		groupList->clear();
	}

	spring::SafeDelete(brain);
	spring::SafeDelete(execute);
	spring::SafeDelete(ut);
	spring::SafeDelete(map);
	spring::SafeDelete(bt);
	spring::SafeDelete(profiler);

	m_initialized = false;
	fclose(m_logFile);
	m_logFile = NULL;
}


//void AAI::EnemyDamaged(int damaged,int attacker,float damage,float3 dir) {}


void AAI::InitAI(IGlobalAICallback* callback, int team)
{
	char profilerName[16];
	SNPRINTF(profilerName, sizeof(profilerName), "%s:%i", "AAI", team);
	profiler = new Profiler(profilerName);

	AAI_SCOPED_TIMER("InitAI")
	m_aiCallback = callback->GetAICallback();

	// open log file
	// this size equals the one used in "AIAICallback::GetValue(AIVAL_LOCATE_FILE_..."
	char filename[2048];
	SNPRINTF(filename, 2048, "%sAAI_log_team_%d.txt", AILOG_PATH, team);

	m_aiCallback->GetValue(AIVAL_LOCATE_FILE_W, filename);

	m_logFile = fopen(filename,"w");

	Log("AAI %s running game %s\n \n", AAI_VERSION, m_aiCallback->GetModHumanName());

	++s_aaiInstances;
	m_aaiInstance = s_aaiInstances; //! @todo This might not be 100% thread safe (if multiple instances off AAI are initialized by several threads at the same time)
	Log("AAI instance: %i\n", m_aaiInstance); 

	// load config file first
	bool gameConfigLoaded    = cfg->loadGameConfig(this);
	bool generalConfigLoaded = cfg->loadGeneralConfig(*this);

	m_configLoaded = gameConfigLoaded && generalConfigLoaded;

	if (m_configLoaded == false)
	{
		std::string errorMsg =
				std::string("Error: Could not load game and/or general config file."
					" For further information see the config file under: ") +
				filename;
		LogConsole("%s", errorMsg.c_str());
		return;
	}

	// generate buildtree (if not already done by other instance)
	s_buildTree.Generate(m_aiCallback);

	// create buildtable
	bt = new AAIBuildTable(this);
	bt->Init();

	// init unit table
	ut = new AAIUnitTable(this);

	// init map
	map = new AAIMap(this);
	map->Init();

	// init brain
	brain = new AAIBrain(this, map->GetMaxSectorDistanceToBase());

	// init executer
	execute = new AAIExecute(this);

	// create unit groups
	group_list.resize(MOBILE_CONSTRUCTOR+1);

	// init airforce manager
	af = new AAIAirForceManager(this);

	// init attack manager
	am = new AAIAttackManager(this, map->continents.size());

	Log("Tidal/Wind strength: %f / %f\n", m_aiCallback->GetTidalStrength(), (m_aiCallback->GetMaxWind() + m_aiCallback->GetMinWind()) * 0.5f);

	LogConsole("AAI loaded");
}

void AAI::UnitDamaged(int damaged, int attacker, float /*damage*/, float3 /*dir*/)
{
	AAI_SCOPED_TIMER("UnitDamaged")

	// filter out commander
	if (ut->cmdr != -1)
	{
		if (damaged == ut->cmdr)
			brain->DefendCommander(attacker);
	}

	const springLegacyAI::UnitDef* attackedDef = m_aiCallback->GetUnitDef(damaged);

	if(attackedDef != nullptr)
	{
		const AAIUnitCategory& category =  s_buildTree.GetUnitCategory(UnitDefId(attackedDef->id));

		// assault grups may be ordered to retreat
		if (category.isCombatUnit() == true)
			execute->CheckFallBack(damaged, attackedDef->id);
	}

	// known attacker
	if (attacker >= 0)
	{
		// filter out friendly fire
		if (m_aiCallback->GetUnitTeam(attacker) == m_aiCallback->GetMyTeam())
			return;

		const springLegacyAI::UnitDef* attackerDef = m_aiCallback->GetUnitDef(attacker);

		if (attackerDef != nullptr)
		{
			const AAIUnitCategory& category =  s_buildTree.GetUnitCategory(UnitDefId(attackerDef->id));

			// retreat builders
			if (ut->IsBuilder(UnitId(damaged)) == true)
				ut->units[damaged].cons->Retreat(category);
			else
			{
				//if (att_cat >= GROUND_ASSAULT && att_cat <= SUBMARINE_ASSAULT)

				float3 pos = m_aiCallback->GetUnitPos(attacker);
				AAISector *sector = map->GetSectorOfPos(pos);

				if (sector && !am->SufficientDefencePowerAt(sector, 1.2f))
				{
					const AAIUnitCategory& attackerCategory = s_buildTree.GetUnitCategory(UnitDefId(attackerDef->id));

					// building has been attacked
					if (category.isBuilding() == true)
						execute->DefendUnitVS(damaged, attackerCategory, &pos, 115);
					// builder
					else if ( ut->IsBuilder(UnitId(damaged)) == true )
						execute->DefendUnitVS(damaged, attackerCategory, &pos, 110);
					// normal units
					else
						execute->DefendUnitVS(damaged, attackerCategory, &pos, 105);
				}
			}
		}
	}
	// unknown attacker
	else
	{
		// retreat builders
		if (ut->IsBuilder(UnitId(damaged)) == true)
		{
			ut->units[damaged].cons->Retreat(EUnitCategory::UNKNOWN);
		}
	}
}

void AAI::UnitCreated(int unit, int /*builder*/)
{
	AAI_SCOPED_TIMER("UnitCreated")
	if (m_configLoaded == false)
		return;

	// get unit's id
	const springLegacyAI::UnitDef* def = m_aiCallback->GetUnitDef(unit);
	UnitDefId unitDefId(def->id);
	const AAIUnitCategory& category = s_buildTree.GetUnitCategory(unitDefId);

	ut->UnitCreated(category);

	bt->units_dynamic[def->id].requested -= 1;
	bt->units_dynamic[def->id].under_construction += 1;

	// add to unittable
	ut->AddUnit(unit, unitDefId.id);

	// get commander a startup
	if(m_initialized == false)
	{
		// must be called to prevent UnitCreated() some lines above from resulting in -1 requested commanders
		ut->UnitRequested(AAIUnitCategory(EUnitCategory::COMMANDER));

		ut->futureBuilders += 1;

		// set side
		m_side = s_buildTree.GetSideOfUnitType( unitDefId) ;

		execute->InitAI(unit, def);

		Log("Entering %s...\n", m_gamePhase.GetName().c_str());
		m_initialized = true;
		return;
	}

	// resurrected units will be handled differently
	if ( !m_aiCallback->UnitBeingBuilt(unit))
	{
		LogConsole("ressurected", 0);
		Log("Ressurected %s\n", s_buildTree.GetUnitTypeProperties(unitDefId).m_name.c_str() );

		// must be called to prevent UnitCreated() some lines above from resulting in -1 requested commanders
		ut->UnitRequested(category);
		ut->UnitFinished(category);
		bt->units_dynamic[def->id].active += 1;

		if (s_buildTree.GetUnitType(unitDefId).IsFactory())
			ut->futureFactories += 1;

		if (s_buildTree.GetMovementType(unitDefId).isStatic())
		{
			float3 pos = m_aiCallback->GetUnitPos(unit);
			execute->InitBuildingAt(def, pos);
		}
	}
	else
	{
		// construction of building started
		if (s_buildTree.GetMovementType(unitDefId).isStatic())
		{
			float3 pos = m_aiCallback->GetUnitPos(unit);

			// create new buildtask
			execute->createBuildTask(UnitId(unit), unitDefId, &pos);

			// add extractor to the sector
			if (category.isMetalExtractor() == true)
			{
				const int x = pos.x / map->xSectorSize;
				const int y = pos.z / map->ySectorSize;

				if(map->IsValidSector(x,y))
					map->sector[x][y].AddExtractor(unit, def->id, &pos);
			}
		}
	}
}

void AAI::UnitFinished(int unit)
{
	AAI_SCOPED_TIMER("UnitFinished")
	if (m_initialized == false)
    {
        Log("Error: AAI not initialized when unit %i was finished\n", unit);
        return;
    }

	// get unit's id
	const springLegacyAI::UnitDef* def = m_aiCallback->GetUnitDef(unit);
	UnitDefId unitDefId(def->id);

	const AAIUnitCategory& category = s_buildTree.GetUnitCategory(unitDefId);

	ut->UnitFinished(category);

	bt->units_dynamic[def->id].under_construction -= 1;
	bt->units_dynamic[def->id].active += 1;

	// building was completed
	if (s_buildTree.GetMovementType(unitDefId).isStatic() == true)
	{
		// delete buildtask
		for(list<AAIBuildTask*>::iterator task = build_tasks.begin(); task != build_tasks.end(); ++task)
		{
			if ((*task)->unit_id == unit)
			{
				AAIBuildTask *build_task = *task;

				if ((*task)->builder_id >= 0 && ut->units[(*task)->builder_id].cons)
					ut->units[(*task)->builder_id].cons->ConstructionFinished();

				build_tasks.erase(task);
				spring::SafeDelete(build_task);
				break;
			}
		}

		// check if building belongs to one of this groups
		if (category.isMetalExtractor() == true)
		{
			ut->AddExtractor(unit);

			// order defence if necessary
			execute->DefendMex(unit, def->id);
		}
		else if (category.isPowerPlant() == true)
		{
			ut->AddPowerPlant(unit, def->id);
		}
		else if (category.isStorage() == true)
		{
			execute->futureStoredEnergy -= def->energyStorage;
			execute->futureStoredMetal  -= def->metalStorage;
		}
		else if (category.isMetalMaker() == true)
		{
			ut->AddMetalMaker(unit, def->id);
		}
		else if (category.isStaticSensor() == true)
		{
			ut->AddRecon(unit, def->id);
		}
		else if (category.isStaticSupport() == true)
		{
			ut->AddJammer(unit, def->id);
		}
		else if (category.isStaticArtillery() == true)
		{
			ut->AddStationaryArty(unit, def->id);
		}
		else if (category.isStaticConstructor() == true)
		{
			ut->AddConstructor(UnitId(unit), UnitDefId(def->id));

			ut->units[unit].cons->Update();
		}
		return;
	}
	else	// unit was completed
	{
		// unit
		if(category.isCombatUnit())
		{
			execute->AddUnitToGroup(unit, unitDefId);

			brain->AddDefenceCapabilities(unitDefId);

			ut->SetUnitStatus(unit, HEADING_TO_RALLYPOINT);
		}
		// scout
		else if(category.isScout())
		{
			ut->AddScout(unit);

			// cloak scout if cloakable
			if (def->canCloak)
			{
				Command c(CMD_CLOAK);
				c.PushParam(1);

				m_aiCallback->GiveOrder(unit, &c);
			}
		}
		// builder
		else if(category.isMobileConstructor() )
		{
			ut->AddConstructor(UnitId(unit), UnitDefId(def->id));

			ut->units[unit].cons->Update();
		}
	}
}

void AAI::UnitDestroyed(int unit, int attacker)
{
	AAI_SCOPED_TIMER("UnitDestroyed")
	// get unit's id
	const springLegacyAI::UnitDef* def = m_aiCallback->GetUnitDef(unit);
	UnitDefId unitDefId(def->id);

	float3 pos = m_aiCallback->GetUnitPos(unit);
	const int x = pos.x/map->xSectorSize;
	const int y = pos.z/map->ySectorSize;

	// check if unit pos is within a valid sector (e.g. aircraft flying outside of the map)
	const bool validSector = map->IsValidSector(x,y);

	// update threat map
	if (attacker && validSector)
	{
		const springLegacyAI::UnitDef* att_def = m_aiCallback->GetUnitDef(attacker);

		if (att_def)
			map->sector[x][y].UpdateThreatValues(unitDefId, UnitDefId(att_def->id));
	}

	// unfinished unit has been killed
	if (m_aiCallback->UnitBeingBuilt(unit))
	{
		const AAIUnitCategory& category = s_buildTree.GetUnitCategory(unitDefId);
		ut->UnitUnderConstructionKilled(category);
		bt->units_dynamic[def->id].under_construction -= 1;

		// unfinished building
		if (!def->canfly && !def->movedata)
		{
			// delete buildtask
			for(list<AAIBuildTask*>::iterator task = build_tasks.begin(); task != build_tasks.end(); ++task)
			{
				if ((*task)->unit_id == unit)
				{
					(*task)->BuildtaskFailed();
					delete *task;

					build_tasks.erase(task);
					break;
				}
			}
		}
		// unfinished unit
		else
		{
			if (s_buildTree.GetUnitType(unitDefId).IsBuilder())
			{
				--ut->futureBuilders;

				bt->UnfinishedConstructorKilled(unitDefId);
			}
			
			if (s_buildTree.GetUnitType(unitDefId).IsFactory())
			{
				if (category.isStaticConstructor() == true)
					--ut->futureFactories;

				bt->UnfinishedConstructorKilled(unitDefId);
			}
		}
	}
	else	// finished unit/building has been killed
	{
		const AAIUnitCategory& category = s_buildTree.GetUnitCategory(unitDefId);
		ut->ActiveUnitKilled(category);

		bt->units_dynamic[def->id].active -= 1;
		assert(bt->units_dynamic[def->id].active >= 0);

		// update buildtable
		if (attacker)
		{
			const springLegacyAI::UnitDef* def_att = m_aiCallback->GetUnitDef(attacker);

			if (def_att)
			{
				const AAIUnitCategory& categoryAttacker = s_buildTree.GetUnitCategory(UnitDefId(def_att->id));
				const int killer = bt->GetIDOfAssaultCategory( categoryAttacker );
				const int killed = bt->GetIDOfAssaultCategory( s_buildTree.GetUnitCategory(unitDefId) );

				// check if valid id
				if (killer != -1)
				{
					if(categoryAttacker.isCombatUnit())
						brain->AttackedBy(categoryAttacker);

					if (killed != -1)
						bt->UpdateTable(def_att, killer, def, killed);
				}
			}
		}

		// finished building has been killed
		if (s_buildTree.GetMovementType(UnitDefId(def->id)).isStatic() == true)
		{
			// decrease number of units of that category in the target sector
			if (validSector)
				map->sector[x][y].RemoveBuilding(category);

			// check if building belongs to one of this groups
			if (category.isStaticDefence() == true)
			{
				// remove defence from map
				map->RemoveDefence(&pos, def->id);
			}
			else if (category.isMetalExtractor() == true)
			{
				ut->RemoveExtractor(unit);

				// mark spots of destroyed mexes as unoccupied
				map->sector[x][y].FreeMetalSpot(m_aiCallback->GetUnitPos(unit), def);
			}
			else if (category.isPowerPlant() == true)
			{
				ut->RemovePowerPlant(unit);
			}
			else if (category.isStaticArtillery() == true)
			{
				ut->RemoveStationaryArty(unit);
			}
			else if (category.isStaticSensor() == true)
			{
				ut->RemoveRecon(unit);
			}
			else if (category.isStaticSupport() == true)
			{
				ut->RemoveJammer(unit);
			}
			else if (category.isMetalMaker() == true)
			{
				ut->RemoveMetalMaker(unit);
			}

			// clean up buildmap & some other stuff
			if (category.isStaticConstructor() == true)
			{
				ut->RemoveConstructor(unit, def->id);
				// speed up reconstruction
				execute->urgency[STATIONARY_CONSTRUCTOR] += 1.5;
			}
			// hq
			else if (category.isCommander() == true)
			{
				ut->RemoveCommander(unit, def->id);
			}
			
			// unblock cells in buildmap
			map->UpdateBuildMap(pos, def, false);

			// if no buildings left in that sector, remove from base sectors
			if (map->sector[x][y].own_structures == 0 && brain->sectors[0].size() > 2)
			{
				brain->AssignSectorToBase(&map->sector[x][y], false);

				Log("\nRemoving sector %i,%i from base; base size: " _STPF_ " \n", x, y, brain->sectors[0].size());
			}
		}
		else // finished unit has been killed
		{
			// scout
			if (category.isScout() == true)
			{
				ut->RemoveScout(unit);

				// add enemy building to sector
				if (validSector && map->sector[x][y].distance_to_base > 0)
					map->sector[x][y].enemy_structures += 1.0f;

			}
			// assault units
			else if (category.isCombatUnit() == true)
			{
				// look for a safer rallypoint if units get killed on their way
				if (ut->units[unit].status == HEADING_TO_RALLYPOINT)
					ut->units[unit].group->GetNewRallyPoint();

				ut->units[unit].group->RemoveUnit(unit, attacker);
			}
			// builder
			else if (s_buildTree.GetUnitType(unitDefId).IsBuilder())
			{
				ut->RemoveConstructor(unit, def->id);
			}
			else if (category.isCommander() == true)
			{
				ut->RemoveCommander(unit, def->id);
			}
		}
	}

	ut->RemoveUnit(unit);
}

void AAI::UnitIdle(int unit)
{
	AAI_SCOPED_TIMER("UnitIdle")
	// if factory is idle, start construction of further units
	if (ut->units[unit].cons)
	{
		if (ut->units[unit].cons->isBusy() == false)
		{
			ut->SetUnitStatus(unit, UNIT_IDLE);

			ut->units[unit].cons->Idle();

			if (ut->constructors.size() < 4)
				execute->CheckConstruction();
		}
	}
	// idle combat units will report to their groups
	else if (ut->units[unit].group)
	{
		//ut->SetUnitStatus(unit, UNIT_IDLE);
		ut->units[unit].group->UnitIdle(unit);
	}
	else if(s_buildTree.GetUnitCategory(UnitDefId(ut->units[unit].def_id)).isScout() == true)
	{
		execute->SendScoutToNewDest(unit);
	}
	else
		ut->SetUnitStatus(unit, UNIT_IDLE);
}

void AAI::UnitMoveFailed(int unit)
{
	AAI_SCOPED_TIMER("UnitMoveFailed")
	if (ut->units[unit].cons)
	{
		ut->units[unit].cons->CheckIfConstructionFailed();
	}

	float3 pos = m_aiCallback->GetUnitPos(unit);

	pos.x = pos.x - 64 + 32 * (rand()%5);
	pos.z = pos.z - 64 + 32 * (rand()%5);

	if (pos.x < 0)
		pos.x = 0;

	if (pos.z < 0)
		pos.z = 0;

	// workaround: prevent flooding the interface with move orders if a unit gets stuck
	if (m_aiCallback->GetCurrentFrame() - ut->units[unit].last_order < 5)
		return;
	else
		execute->MoveUnitTo(unit, &pos);
}

void AAI::EnemyEnterLOS(int /*enemy*/) {}
void AAI::EnemyLeaveLOS(int /*enemy*/) {}
void AAI::EnemyEnterRadar(int /*enemy*/) {}
void AAI::EnemyLeaveRadar(int /*enemy*/) {}

void AAI::EnemyDestroyed(int enemy, int attacker)
{
	AAI_SCOPED_TIMER("EnemyDestroyed")
	// remove enemy from unittable
	ut->EnemyKilled(enemy);

	if (attacker)
	{
		// get unit's id
		const UnitDef* def = m_aiCallback->GetUnitDef(enemy);
		const UnitDef* def_att = m_aiCallback->GetUnitDef(attacker);

		if (def_att)
		{
			// unit was destroyed
			if (def)
			{
				const int killer = bt->GetIDOfAssaultCategory( s_buildTree.GetUnitCategory(UnitDefId(def_att->id)) );
				const int killed = bt->GetIDOfAssaultCategory( s_buildTree.GetUnitCategory(UnitDefId(def->id)) );

				if (killer != -1 && killed != -1)
					bt->UpdateTable(def_att, killer, def, killed);
			}
		}
	}
}

void AAI::Update()
{
	const int tick = m_aiCallback->GetCurrentFrame();

	if (tick < 0)
	{
		return;
	}

	GamePhase gamePhase(tick);

	if(gamePhase > m_gamePhase)
	{
		m_gamePhase = gamePhase;
		Log("Entering %s...\n", m_gamePhase.GetName().c_str());
	}

	if (m_initialized == false)
	{
		if (!(tick % 450))
		{
			LogConsole("Failed to initialize AAI! Please view ai log for further information and check if AAI supports this game");
		}

		return;
	}

	// scouting
	if (!(tick % cfg->SCOUT_UPDATE_FREQUENCY))
	{
		AAI_SCOPED_TIMER("Scouting_1")
		map->UpdateRecon();
	}

	if (!((tick + 5) % cfg->SCOUT_UPDATE_FREQUENCY))
	{
		AAI_SCOPED_TIMER("Scouting_2")
		map->UpdateEnemyScoutingData();

		if(m_aaiInstance == 1)
		{
			FILE *file = fopen("Scout_debug.txt", "w+");

			fprintf(file, "Enemy Structures:\n");
			for(int y = 0; y < map->ySectors; ++y)
			{
				for(int x = 0; x < map->xSectors; ++x)
				{
					fprintf(file, "%f ", map->sector[x][y].enemy_structures);
					
				}
				fprintf(file, "\n");
			}

			fprintf(file, "Enemy mobile/static combat power:\n");
			for(int y = 0; y < map->ySectors; ++y)
			{
				for(int x = 0; x < map->xSectors; ++x)
				{
					fprintf(file, "%-5f/%-5f ", map->sector[x][y].enemy_mobile_combat_power[0], map->sector[x][y].enemy_stat_combat_power[0]);
					
				}
				fprintf(file, "\n");
			}

			fprintf(file, "Flat ratio/last scout:\n");
			for(int y = 0; y < map->ySectors; ++y)
			{
				for(int x = 0; x < map->xSectors; ++x)
				{
					fprintf(file, "%-3f/%-5f ", map->sector[x][y].flat_ratio, map->sector[x][y].last_scout);
					
				}
				fprintf(file, "\n");
			}


		  	fclose(file);
		}

	}

	// update groups
	if (!(tick % 169))
	{
		AAI_SCOPED_TIMER("Groups")
		for(list<UnitCategory>::const_iterator category = bt->assault_categories.begin(); category != bt->assault_categories.end(); ++category)
		{
			for(list<AAIGroup*>::iterator group = group_list[*category].begin(); group != group_list[*category].end(); ++group)
			{
				(*group)->Update();
			}
		}

		return;
	}

	// unit management
	if (!(tick % 649))
	{
		AAI_SCOPED_TIMER("Unit-Management")
		execute->CheckBuildqueues();
		brain->BuildUnits();
		execute->BuildScouts();
	}

	if (!(tick % 711))
	{
		AAI_SCOPED_TIMER("Check-Attack")
		// check attack
		am->Update();
		af->BombBestUnit(2, 2);
		return;
	}

	// ressource management
	if (!(tick % 199))
	{
		AAI_SCOPED_TIMER("Resource-Management")
		execute->CheckRessources();
	}

	// update sectors
	if (!(tick % 323))
	{
		AAI_SCOPED_TIMER("Update-Sectors")
		brain->UpdateAttackedByValues();
		map->UpdateSectors();

		brain->UpdatePressureByEnemy();

		/*if (brain->enemy_pressure_estimation > 0.01f)
		{
			LogConsole("%f", brain->enemy_pressure_estimation);
		}*/
	}

	// builder management
	if (!(tick % 917))
	{
		AAI_SCOPED_TIMER("Builder-Management")
		brain->UpdateDefenceCapabilities();
	}

	// update income
	if (!(tick % 45))
	{
		AAI_SCOPED_TIMER("Update-Income")
		execute->UpdateRessources();
		brain->UpdateRessources(m_aiCallback);
	}

	// building management
	if (!(tick % 97))
	{
		AAI_SCOPED_TIMER("Building-Management")
		execute->CheckConstruction();
	}

	// builder/factory management
	if (!(tick % 677))
	{
		AAI_SCOPED_TIMER("BuilderAndFactory-Management")
		for (set<int>::iterator builder = ut->constructors.begin(); builder != ut->constructors.end(); ++builder)
		{
			ut->units[(*builder)].cons->Update();
		}
	}

	if (!(tick % 337))
	{
		AAI_SCOPED_TIMER("Check-Factories")
		execute->CheckFactories();
	}

	if (!(tick % 1079))
	{
		AAI_SCOPED_TIMER("Check-Defenses")
		execute->CheckDefences();
	}

	// build radar/jammer
	if (!(tick % 1177))
	{
		execute->CheckRecon();
		execute->CheckJammer();
		execute->CheckStationaryArty();
		execute->CheckAirBase();
	}

	// upgrade mexes
	if (!(tick % 1573))
	{
		AAI_SCOPED_TIMER("Upgrade-Mexes")
		if (brain->enemy_pressure_estimation < 0.05f)
		{
			execute->CheckMexUpgrade();
			execute->CheckRadarUpgrade();
			execute->CheckJammerUpgrade();
		}
	}

	// recheck rally points
	if (!(tick % 1877))
	{
		AAI_SCOPED_TIMER("Recheck-Rally-Points")
		for (list<UnitCategory>::const_iterator category = bt->assault_categories.begin(); category != bt->assault_categories.end(); ++category)
		{
			for (list<AAIGroup*>::iterator group = group_list[*category].begin(); group != group_list[*category].end(); ++group)
			{
				(*group)->UpdateRallyPoint();
			}
		}
	}

	// recalculate efficiency stats
	if (!(tick % 2927))
	{
		AAI_SCOPED_TIMER("Recalculate-Efficiency-Stats")
		if (m_aaiInstance == 1) // only update statistics once (if multiple instances off AAI are running)
		{
			bt->UpdateMinMaxAvgEfficiency();
		}
	}
}

int AAI::HandleEvent(int msg, const void* data)
{
	AAI_SCOPED_TIMER("HandleEvent")
	switch (msg)
	{
		case AI_EVENT_UNITGIVEN: // 1
		case AI_EVENT_UNITCAPTURED: // 2
			{
				const IGlobalAI::ChangeTeamEvent* cte = (const IGlobalAI::ChangeTeamEvent*) data;

				const int myAllyTeamId = m_aiCallback->GetMyAllyTeam();
				const bool oldEnemy = !m_aiCallback->IsAllied(myAllyTeamId, m_aiCallback->GetTeamAllyTeam(cte->oldteam));
				const bool newEnemy = !m_aiCallback->IsAllied(myAllyTeamId, m_aiCallback->GetTeamAllyTeam(cte->newteam));

				if (oldEnemy && !newEnemy) {
					// unit changed from an enemy to an allied team
					// we got a new friend! :)
					EnemyDestroyed(cte->unit, -1);
				} else if (!oldEnemy && newEnemy) {
					// unit changed from an ally to an enemy team
					// we lost a friend! :(
					EnemyCreated(cte->unit);
					if (!m_aiCallback->UnitBeingBuilt(cte->unit)) {
						EnemyFinished(cte->unit);
					}
				}

				if (cte->oldteam == m_aiCallback->GetMyTeam()) {
					// we lost a unit
					UnitDestroyed(cte->unit, -1);
				} else if (cte->newteam == m_aiCallback->GetMyTeam()) {
					// we have a new unit
					UnitCreated(cte->unit, -1);
					if (!m_aiCallback->UnitBeingBuilt(cte->unit)) {
						UnitFinished(cte->unit);
						UnitIdle(cte->unit);
					}
				}
				break;
			}
	}
	return 0;
}

void AAI::Log(const char* format, ...)
{
	if(m_logFile != NULL)
	{
		va_list args;
		va_start(args, format);
		const int bytes = vfprintf(m_logFile, format, args);
		if (bytes<0) { //write to stderr if write to file failed
			vfprintf(stderr, format, args);
		}
		va_end(args);
	}
}

void AAI::LogConsole(const char* format, ...)
{
	char buf[1024];
	va_list args;

	va_start(args, format);
	vsnprintf(buf, 1024, format, args);
	va_end(args);

	m_aiCallback->SendTextMsg(buf, 0);
	Log("%s\n", &buf);
}

