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
#include "AAIMap.h"
#include "AAIBrain.h"
#include "AAIBuildTable.h"
#include "AAIAirForceManager.h"
#include "AAIExecute.h"
#include "AAIUnitTable.h"
#include "AAIBuildTask.h"
#include "AAIConstructor.h"
#include "AAIAttackManager.h"
#include "AIExport.h"
#include "AAIConfig.h"
#include "AAIGroup.h"
#include "AAISector.h"
#include "AAIUnitTypes.h"

#include "System/SafeUtil.h"

#include "LegacyCpp/IGlobalAICallback.h"
#include "LegacyCpp/UnitDef.h"
using namespace springLegacyAI;


#include "CUtils/SimpleProfiler.h"
#define AAI_SCOPED_TIMER(part) SCOPED_TIMER(part, profiler);

// C++ < C++17 does not support initialization of static const within class declaration
const std::vector<int> GamePhase::m_startFrameOfGamePhase  = {0, 10800, 27000, 72000};
const std::vector<std::string> GamePhase::m_gamePhaseNames = {"starting phase", "early phase", "mid phase", "late game"};
const std::vector<std::string> AAITargetType::m_targetTypeNames = {"surface", "air", "floater", "submerged"};
const std::vector<std::string> AAICombatUnitCategory::m_combatCategoryNames = {"Surface", "Air", "Sea"};
const std::vector<std::string> AAIMapType::m_mapTypeNames = {"land map", "mixed land water map", "water map"};

constexpr std::array<ECombatUnitCategory, 3> AAICombatUnitCategory::m_combatUnitCategories;
constexpr std::array<ETargetType, 4>         AAITargetType::m_mobileTargetTypes;
constexpr std::array<ETargetType, 5>         AAITargetType::m_targetTypes;

AAIBuildTree AAI::s_buildTree;

int AAI::s_aaiInstances = 0;

AAI::AAI(int skirmishAIId, const struct SSkirmishAICallback* callback) :
	m_aiCallback(nullptr),
	m_skirmishAIId(skirmishAIId),
	m_skirmishAICallbacks(callback),
	m_map(nullptr),
	m_brain(nullptr),
	m_execute(nullptr),
	m_unitTable(nullptr),
	m_buildTable(nullptr),
	m_airForceManager(nullptr),
	m_attackManager(nullptr),
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

	Log("Linking buildtask to builder failed counter: %u\n", m_execute->GetLinkingBuildTaskToBuilderFailedCounter());

	Log("Unit category active / under construction / requested\n");
	for(AAIUnitCategory category(AAIUnitCategory::GetFirst()); category.End() == false; category.Next())
	{
		Log("%s: %i / %i / %i\n", s_buildTree.GetCategoryName(category).c_str(), 
								m_unitTable->GetNumberOfActiveUnitsOfCategory(category), 
								m_unitTable->GetNumberOfUnitsUnderConstructionOfCategory(category), 
								m_unitTable->GetNumberOfRequestedUnitsOfCategory(category));
	}

	Log("\nGround Groups:    " _STPF_ "\n", GetUnitGroupsList(EUnitCategory::GROUND_COMBAT).size());
	Log("Air Groups:       " _STPF_ "\n",   GetUnitGroupsList(EUnitCategory::AIR_COMBAT).size());
	Log("Hover Groups:     " _STPF_ "\n",   GetUnitGroupsList(EUnitCategory::HOVER_COMBAT).size());
	Log("Sea Groups:       " _STPF_ "\n",   GetUnitGroupsList(EUnitCategory::SEA_COMBAT).size());
	Log("Submarine Groups: " _STPF_ "\n\n", GetUnitGroupsList(EUnitCategory::SUBMARINE_COMBAT).size());

	Log("\nGround group details - unit type, current number, continent id:\n");
	for(auto group = GetUnitGroupsList(EUnitCategory::GROUND_COMBAT).begin(); group != GetUnitGroupsList(EUnitCategory::GROUND_COMBAT).end(); ++group)
		Log("%s %i %i\n", s_buildTree.GetUnitTypeProperties( (*group)->GetUnitDefIdOfGroup() ).m_name.c_str(), (*group)->GetCurrentSize(), (*group)->GetContinentId());

	Log("Future/active factories:     %i / %i\n\n", m_unitTable->futureFactories, m_unitTable->activeFactories);

	Log("Unit production rate: %i\n\n", m_execute->GetUnitProductionRate());

	Log("Active/under construction/requested constructors:\n");
	for(const auto factory : s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, m_side))
	{
		Log("%-30s: %i %i %i\n", s_buildTree.GetUnitTypeProperties(factory).m_name.c_str(), m_buildTable->units_dynamic[factory.id].active, m_buildTable->units_dynamic[factory.id].underConstruction, m_buildTable->units_dynamic[factory.id].requested);
	}
	for(const auto builder : s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, m_side))
		Log("%-30s: %i %i %i\n", s_buildTree.GetUnitTypeProperties(builder).m_name.c_str(), m_buildTable->units_dynamic[builder.id].active, m_buildTable->units_dynamic[builder.id].underConstruction, m_buildTable->units_dynamic[builder.id].requested);

	GamePhase gamePhase(m_aiCallback->GetCurrentFrame());
	const AttackedByRatesPerGamePhase& attackedByRates = m_brain->GetAttackedByRates();

	Log("\nAttack frequencies (combat unit category / frequency) \n");
	for(GamePhase gamePhaseIterator(0); gamePhaseIterator <= gamePhase; gamePhaseIterator.Next())
	{
		Log("Game phase %s:", gamePhaseIterator.GetName().c_str());
		for(const auto& targetType : AAITargetType::m_mobileTargetTypes)
		{
			Log("  %s: %f", AAITargetType(targetType).GetName().c_str(), attackedByRates.GetAttackedByRate(gamePhaseIterator, targetType));
		}
		Log("\n");
	}

	// delete buildtasks
	for(std::list<AAIBuildTask*>::iterator task = build_tasks.begin(); task != build_tasks.end(); ++task)
	{
		delete (*task);
	}
	build_tasks.clear();

	// save game learning data
	if(GetAAIInstance() == 1)
		m_buildTable->SaveModLearnData(gamePhase, m_brain->GetAttackedByRates(), m_map->GetMapType());

	spring::SafeDelete(m_attackManager);
	spring::SafeDelete(m_airForceManager);

	// delete unit groups
	for(auto groupList = m_unitGroupsOfCategoryLists.begin(); groupList != m_unitGroupsOfCategoryLists.end(); ++groupList)
	{
		for(std::list<AAIGroup*>::iterator group = groupList->begin(); group != groupList->end(); ++group)
			delete (*group);
		
		groupList->clear();
	}

	spring::SafeDelete(m_brain);
	spring::SafeDelete(m_execute);
	spring::SafeDelete(m_unitTable);
	spring::SafeDelete(m_map);
	spring::SafeDelete(m_buildTable);
	spring::SafeDelete(profiler);

	m_initialized = false;
	fclose(m_logFile);
	m_logFile = nullptr;

	// last instance of AAI shall clean up config
	if(s_aaiInstances == 0)
	{
		AAIConfig::Delete();
	}
}

//void AAI::EnemyDamaged(int damaged,int attacker,float damage,float3 dir) {}

void AAI::InitAI(IGlobalAICallback* callback, int team)
{
	char profilerName[16];
	SNPRINTF(profilerName, sizeof(profilerName), "%s:%i", "AAI", team);
	profiler = new Profiler(profilerName);

	AAI_SCOPED_TIMER("InitAI")
	m_aiCallback = callback->GetAICallback();

	m_myTeamId = m_aiCallback->GetMyTeam();

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

	// init config (if not already done by other instance of AAI) and load from file
	AAIConfig::Init();
	cfg = AAIConfig::GetConfig();

	const bool gameConfigLoaded    = cfg->LoadGameConfig(this);
	const bool generalConfigLoaded = cfg->LoadGeneralConfig(this);

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
	m_buildTable = new AAIBuildTable(this);

	// init unit table
	m_unitTable = new AAIUnitTable(this);

	// init map
	m_map = new AAIMap(this, m_aiCallback->GetMapWidth(), m_aiCallback->GetMapHeight(), std::sqrt(m_aiCallback->GetLosMapResolution()) );

	// init threat map
	m_threatMap = new AAIThreatMap(AAIMap::xSectors, AAIMap::ySectors);

	// init brain
	m_brain = new AAIBrain(this, m_map->GetMaxSectorDistanceToBase());

	if(GetAAIInstance() == 1)
	{
		std::string filename = cfg->GetFileName(m_aiCallback, cfg->GetUniqueName(m_aiCallback, true, true, false, false), AILOG_PATH, "_buildtree.txt", true);
		s_buildTree.PrintSummaryToFile(filename, m_aiCallback);

		m_brain->InitAttackedByRates( m_buildTable->GetAttackedByRates(m_map->GetMapType()) );
	}

	m_execute = new AAIExecute(this);

	// create unit groups
	m_unitGroupsOfCategoryLists.resize(AAIUnitCategory::numberOfUnitCategories);

	// init airforce manager
	m_airForceManager = new AAIAirForceManager(this);

	// init attack manager
	m_attackManager = new AAIAttackManager(this);

	Log("Tidal/Wind strength: %f / %f\n", m_aiCallback->GetTidalStrength(), (m_aiCallback->GetMaxWind() + m_aiCallback->GetMinWind()) * 0.5f);

	LogConsole("AAI loaded");
}

void AAI::UnitDamaged(int damaged, int attacker, float /*damage*/, float3 /*dir*/)
{
	AAI_SCOPED_TIMER("UnitDamaged")

	const springLegacyAI::UnitDef* attackedDef = m_aiCallback->GetUnitDef(damaged);
	if(attackedDef == nullptr)
		return;
		
	const UnitDefId unitDefId(attackedDef->id);
	const AAIUnitCategory& category = s_buildTree.GetUnitCategory(unitDefId);

	if(category.IsCommander())
		m_brain->DefendCommander(attacker);

	const springLegacyAI::UnitDef* attackerDef = m_aiCallback->GetUnitDef(attacker);

	if(attackerDef == nullptr)
	{
		// ------------------------------------------------------------------------------------------------------------
		// unknown attacker
		// ------------------------------------------------------------------------------------------------------------

		// retreat builders
		if (category.IsMobileConstructor() && m_unitTable->units[damaged].cons)
			m_unitTable->units[damaged].cons->CheckRetreatFromAttackBy(EUnitCategory::UNKNOWN);	
	}
	else 
	{
		// ------------------------------------------------------------------------------------------------------------
		// known attacker
		// ------------------------------------------------------------------------------------------------------------

		// filter out friendly fire
		if (m_aiCallback->GetUnitAllyTeam(attacker) == m_aiCallback->GetMyAllyTeam())
			return;

		const UnitId    unit(damaged);
		const UnitDefId enemyDefId(attackerDef->id);

		if (category.IsCombatUnit())
			m_execute->CheckKeepDistanceToEnemy(unit, unitDefId, enemyDefId);

		const AAITargetType&  enemyTargetType = s_buildTree.GetTargetType(enemyDefId);
		const float3          pos = m_aiCallback->GetUnitPos(attacker);
		
		// building has been attacked
		if (category.IsBuilding() )
			m_execute->DefendUnitVS(unit, enemyTargetType, pos, AAIConstants::defendBaseUrgency);
		// builder
		else if ( category.IsMobileConstructor() )
		{
			const AAIUnitCategory&  enemyCategory = s_buildTree.GetUnitCategory(enemyDefId);

			m_execute->DefendUnitVS(unit, enemyTargetType, pos, AAIConstants::defendConstructorsUrgency);

			if(m_unitTable->units[damaged].cons)
				m_unitTable->units[damaged].cons->CheckRetreatFromAttackBy(enemyCategory);
		}
		// normal units
		else
		{
			if(enemyTargetType.IsAir() && (s_buildTree.GetUnitType(unitDefId).CanFightTargetType(enemyTargetType) == false) ) 
				m_execute->DefendUnitVS(unit, enemyTargetType, pos, AAIConstants::defendUnitsUrgency);
		}	
		
	}
}

void AAI::UnitCreated(int unit, int builder)
{
	AAI_SCOPED_TIMER("UnitCreated")
	if (m_configLoaded == false)
		return;

	// get unit's id
	const springLegacyAI::UnitDef* def = m_aiCallback->GetUnitDef(unit);
	const UnitDefId unitDefId(def->id);
	
	m_unitTable->AddUnit(unit, unitDefId.id);

	// get commander a startup
	if(m_initialized == false)
	{
		// set side
		m_side = s_buildTree.GetSideOfUnitType( unitDefId) ;
		
		const AAIUnitCategory& category = s_buildTree.GetUnitCategory(unitDefId);
		m_unitTable->UnitRequested(category);
		m_unitTable->ConstructionStarted(category);

		if(category.IsCommander() == false)
			Log("Error: Starting unit is not in unit category \"commander\"!\n");

		m_execute->InitAI(UnitId(unit), unitDefId);

		Log("Entering %s...\n", m_gamePhase.GetName().c_str());
		m_initialized = true;
		return;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// resurrected or gifted units
	//-----------------------------------------------------------------------------------------------------------------
	if ( !m_aiCallback->UnitBeingBuilt(unit))
	{
		//Log("Ressurected/gifted %s\n", s_buildTree.GetUnitTypeProperties(unitDefId).m_name.c_str() );

		const AAIUnitCategory& category = s_buildTree.GetUnitCategory(unitDefId);
		m_unitTable->UnitRequested(category);
		m_unitTable->ConstructionStarted(category);

		m_buildTable->units_dynamic[def->id].underConstruction += 1;

		if (s_buildTree.GetUnitType(unitDefId).IsFactory())
			m_unitTable->activeFactories += 1;

		if (s_buildTree.GetMovementType(unitDefId).IsStatic())
		{
			const float3 pos = m_aiCallback->GetUnitPos(unit);
			m_map->InitBuilding(def, pos);
		}
	}
	//-----------------------------------------------------------------------------------------------------------------
	// "regular" units where construction just started
	//-----------------------------------------------------------------------------------------------------------------
	else
	{
		ConstructionStarted(UnitId(unit), unitDefId, UnitId(builder));
	}
}

void AAI::ConstructionStarted(UnitId unitId, UnitDefId unitDefId, UnitId constructor)
{
	const AAIUnitCategory& category = s_buildTree.GetUnitCategory(unitDefId);
	m_unitTable->ConstructionStarted(category);

	m_buildTable->ConstructionStarted(unitDefId);

	// construction of building started
	if (s_buildTree.GetMovementType(unitDefId).IsStatic())
	{
		const float3 buildsite = m_aiCallback->GetUnitPos(unitId.id);

		// create new buildtask
		AAIBuildTask *task = new AAIBuildTask(unitId, unitDefId, buildsite, constructor);
		build_tasks.push_back(task);

		m_unitTable->units[constructor.id].cons->ConstructionStarted(unitId, task);

		// add extractor to the sector
		if (category.IsMetalExtractor())
		{
			AAISector* sector = m_map->GetSectorOfPos(buildsite);

			if(sector)
				sector->AddExtractor(unitId, unitDefId, buildsite);
		}
	}
}

void AAI::UnitFinished(int unit)
{
	AAI_SCOPED_TIMER("UnitFinished")
	if (m_initialized == false)
        return;

	// get unit's id
	const springLegacyAI::UnitDef* def = m_aiCallback->GetUnitDef(unit);
	const UnitDefId unitDefId(def->id);
	const UnitId    unitId(unit);

	const AAIUnitCategory& category = s_buildTree.GetUnitCategory(unitDefId);

	m_unitTable->UnitFinished(category);
	m_buildTable->ConstructionFinished(unitDefId);

	// building was completed
	if (s_buildTree.GetMovementType(unitDefId).IsStatic())
	{
		// delete buildtask
		for(auto task = build_tasks.begin(); task != build_tasks.end(); ++task)
		{
			if( (*task)->CheckIfConstructionFinished(m_unitTable, unitId) )
			{
				AAIBuildTask *build_task = *task;
				build_tasks.erase(task);
				spring::SafeDelete(build_task);
				break;
			}
		}

		// check if building belongs to one of this groups
		if (category.IsMetalExtractor())
		{
			m_unitTable->AddExtractor(unit);

			// order defence if necessary
			m_execute->BuildStaticDefenceForExtractor(unitId, unitDefId);
		}
		else if (category.IsPowerPlant())
		{
			m_unitTable->AddPowerPlant(unitId, unitDefId);
			m_brain->PowerPlantFinished(unitDefId);
		}
		else if (category.IsMetalMaker())
		{
			m_unitTable->AddMetalMaker(unit, def->id);
		}
		else if (category.IsStaticSensor())
		{
			m_unitTable->AddStaticSensor(unitId);
		}
		else if (category.IsStaticSupport())
		{
			const AAIUnitType& unitType = s_buildTree.GetUnitType(unitDefId);
			if(unitType.IsRadarJammer() || unitType.IsSonarJammer())
				m_unitTable->AddJammer(unit, def->id);
		}
		else if (category.IsStaticArtillery())
		{
			m_unitTable->AddStationaryArty(unit, def->id);
		}
		else if (category.IsStaticConstructor())
		{
			if (m_unitTable->GetConstructors().size() < 2)
				m_execute->CheckConstruction();

			m_unitTable->AddConstructor(unitId, unitDefId);
			m_unitTable->units[unit].cons->Idle();
		}
		else if(category.IsStaticAssistance())
		{
			float3 position = GetAICallback()->GetUnitPos(unit);
			position.x += 32.0f;
			position.z += 32.0f;

			Command c(CMD_PATROL);
			c.PushPos(position);
			GetAICallback()->GiveOrder(unit, &c);
		}
		return;
	}
	else	// unit was completed
	{
		// unit
		if(category.IsCombatUnit())
		{
			m_execute->AddUnitToGroup(unit, unitDefId);

			m_brain->AddDefenceCapabilities(unitDefId);

			m_unitTable->SetUnitStatus(unit, HEADING_TO_RALLYPOINT);
		}
		// scout
		else if(category.IsScout())
		{
			m_unitTable->AddScout(unit);

			// cloak scout if cloakable
			if (def->canCloak)
			{
				Command c(37382, 0u, 1);
				//Command c(CMD_CLOAK);
				//c.PushParam(1);

				m_aiCallback->GiveOrder(unit, &c);
			}

			m_execute->SendScoutToNewDest(unitId);
		}
		// builder
		else if(category.IsMobileConstructor() )
		{
			m_unitTable->AddConstructor(unitId, unitDefId);

			m_unitTable->units[unit].cons->Update();
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

	AAISector* sector = m_map->GetSectorOfPos(pos);

	// update threat map
	if (attacker && sector)
	{
		const springLegacyAI::UnitDef* att_def = m_aiCallback->GetUnitDef(attacker);

		if (att_def)
			sector->UpdateThreatValues(unitDefId, UnitDefId(att_def->id));
	}

	// unfinished unit has been killed
	if (m_aiCallback->UnitBeingBuilt(unit))
	{
		const AAIUnitCategory& category = s_buildTree.GetUnitCategory(unitDefId);
		m_unitTable->UnitUnderConstructionKilled(category);
		m_buildTable->units_dynamic[def->id].underConstruction -= 1;

		// unfinished building
		if( category.IsBuilding() )
		{
			// delete buildtask
			for(auto task = build_tasks.begin(); task != build_tasks.end(); ++task)
			{
				if( (*task)->CheckIfConstructionFailed(this, UnitId(unit)) )
				{
					AAIBuildTask *buildTask = *task;
					build_tasks.erase(task);
					spring::SafeDelete(buildTask);
					break;
				}
			}
		}
		// unfinished unit
		else
		{
			if (s_buildTree.GetUnitType(unitDefId).IsBuilder())
			{
				m_buildTable->UnfinishedConstructorKilled(unitDefId);
			}
			
			if (s_buildTree.GetUnitType(unitDefId).IsFactory())
			{
				if (category.IsStaticConstructor() == true)
					--m_unitTable->futureFactories;

				m_buildTable->UnfinishedConstructorKilled(unitDefId);
			}
		}
	}
	else	// finished unit/building has been killed
	{
		const AAIUnitCategory& category = s_buildTree.GetUnitCategory(unitDefId);
		m_unitTable->ActiveUnitKilled(category);

		m_buildTable->units_dynamic[def->id].active -= 1;
		assert(m_buildTable->units_dynamic[def->id].active >= 0);

		// update buildtable
		if(UnitId(attacker).IsValid() )
		{
			const springLegacyAI::UnitDef* defAttacker = m_aiCallback->GetUnitDef(attacker);

			if(defAttacker)
			{
				UnitDefId attackerDefId(defAttacker->id);

				if(defAttacker)
					s_buildTree.UpdateCombatPowerStatistics(attackerDefId, unitDefId);

				const AAIUnitCategory& categoryAttacker = s_buildTree.GetUnitCategory(attackerDefId);
				if(categoryAttacker.IsCombatUnit())
						m_brain->AttackedBy( s_buildTree.GetTargetType(attackerDefId) );
			}
		}

		// finished building has been killed
		if (s_buildTree.GetMovementType(unitDefId).IsStatic())
		{
			// decrease number of units of that category in the target sector
			if(sector)
				sector->RemoveBuilding(category);

			// check if building belongs to one of this groups
			if (category.IsStaticDefence())
			{
				// remove defence from map
				m_map->AddOrRemoveStaticDefence(pos, unitDefId, false);
			}
			else if (category.IsMetalExtractor())
			{
				m_unitTable->RemoveExtractor(unit);

				// mark spots of destroyed mexes as unoccupied
				if(sector)
					sector->FreeMetalSpot(m_aiCallback->GetUnitPos(unit), unitDefId);
			}
			else if (category.IsPowerPlant())
			{
				m_unitTable->RemovePowerPlant(unit);
			}
			else if (category.IsStaticArtillery())
			{
				m_unitTable->RemoveStationaryArty(unit);
			}
			else if (category.IsStaticSensor())
			{
				m_unitTable->RemoveStaticSensor(UnitId(unit));
			}
			else if (category.IsStaticSupport())
			{
				m_unitTable->RemoveJammer(unit);
			}
			else if (category.IsMetalMaker())
			{
				m_unitTable->RemoveMetalMaker(unit);
			}

			// clean up buildmap & some other stuff
			if(s_buildTree.GetUnitType(unitDefId).IsFactory() || s_buildTree.GetUnitType(unitDefId).IsBuilder() )
			{
				m_unitTable->RemoveConstructor(UnitId(unit), unitDefId);
			}
			
			// unblock cells in buildmap
			m_map->UpdateBuildMap(pos, def, false);

			// if no buildings left in that sector, remove from base sectors
			/*if (map->sector[x][y].own_structures == 0 && brain->sectors[0].size() > 2)
			{
				brain->AssignSectorToBase(&map->sector[x][y], false);

				Log("\nRemoving sector %i,%i from base; base size: " _STPF_ " \n", x, y, brain->sectors[0].size());
			}*/
		}
		else // finished unit has been killed
		{
			// scout
			if (category.IsScout())
			{
				m_map->CheckUnitsInLOSUpdate(true);

				m_unitTable->RemoveScout(unit);
			}
			// assault units
			else if (category.IsCombatUnit())
			{
				// look for a safer rallypoint if units get killed on their way
				if (m_unitTable->units[unit].status == HEADING_TO_RALLYPOINT)
					m_unitTable->units[unit].group->UpdateRallyPoint();

				m_unitTable->units[unit].group->RemoveUnit(UnitId(unit), UnitId(attacker) );
			}
			// builder (incl. commander)
			else if (s_buildTree.GetUnitType(unitDefId).IsBuilder())
			{
				m_unitTable->RemoveConstructor(UnitId(unit), unitDefId);
			}
		}
	}

	m_unitTable->RemoveUnit(unit);
}

void AAI::UnitIdle(int unit)
{
	const UnitId unitId(unit);

	AAI_SCOPED_TIMER("UnitIdle")
	// if factory is idle, start construction of further units
	if (m_unitTable->units[unit].cons)
	{
		if (m_unitTable->units[unit].cons->isBusy() == false)
		{
			if (m_unitTable->GetConstructors().size() < 4)
				m_execute->CheckConstruction();

			m_unitTable->SetUnitStatus(unit, UNIT_IDLE);

			m_unitTable->units[unit].cons->Idle();
		}
	}
	// idle combat units will report to their groups
	else if (m_unitTable->units[unit].group)
	{
		//ut->SetUnitStatus(unit, UNIT_IDLE);
		m_unitTable->units[unit].group->UnitIdle(unitId, m_attackManager);
	}
	else if(s_buildTree.GetUnitCategory(UnitDefId(m_unitTable->units[unit].def_id)).IsScout())
	{
		m_execute->SendScoutToNewDest(unitId);
	}
	else
		m_unitTable->SetUnitStatus(unit, UNIT_IDLE);
}

void AAI::UnitMoveFailed(int unit)
{
	AAI_SCOPED_TIMER("UnitMoveFailed")
	if (m_unitTable->units[unit].cons)
	{
		m_unitTable->units[unit].cons->CheckIfConstructionFailed();
	}

	float3 pos = m_aiCallback->GetUnitPos(unit);

	pos.x = pos.x - 64 + 32 * (rand()%5);
	pos.z = pos.z - 64 + 32 * (rand()%5);

	if (pos.x < 0)
		pos.x = 0;

	if (pos.z < 0)
		pos.z = 0;

	// workaround: prevent flooding the interface with move orders if a unit gets stuck
	if (m_aiCallback->GetCurrentFrame() - m_unitTable->units[unit].last_order < 5)
		return;
	else
		m_execute->SendUnitToPosition(UnitId(unit), pos);
}

void AAI::EnemyEnterLOS(int /*enemy*/) {}
void AAI::EnemyLeaveLOS(int /*enemy*/) {}
void AAI::EnemyEnterRadar(int /*enemy*/) {}
void AAI::EnemyLeaveRadar(int /*enemy*/) {}

void AAI::EnemyDestroyed(int enemy, int attacker)
{
	AAI_SCOPED_TIMER("EnemyDestroyed")
	// remove enemy from unittable
	if(UnitId(enemy).IsValid())
		m_unitTable->EnemyKilled(enemy);

	if(UnitId(attacker).IsValid())
	{
		// get unit's id
		const UnitDef* defKilled   = m_aiCallback->GetUnitDef(enemy);
		const UnitDef* defAttacker = m_aiCallback->GetUnitDef(attacker);

		if (defAttacker && defKilled)
			s_buildTree.UpdateCombatPowerStatistics(UnitDefId(defAttacker->id), UnitDefId(defKilled->id));
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
	if (!((tick + 2 * GetAAIInstance()) % 45))
	{
		AAI_SCOPED_TIMER("Scouting_1")
		m_map->CheckUnitsInLOSUpdate();
	}

	// update groups
	if (!((tick+7) % 150))
	{
		AAI_SCOPED_TIMER("Groups")
		for (const auto& category : s_buildTree.GetCombatUnitCatgegories())
		{
			for (auto group : GetUnitGroupsList(category))
			{
				group->Update();
			}
		}

		return;
	}

	// unit management
	if (!(tick % 650))
	{
		AAI_SCOPED_TIMER("Unit-Management")
		m_execute->AdjustUnitProductionRate();
		m_brain->BuildUnits();
		m_execute->BuildScouts();
	}

	if (!((tick+39) % 500))
	{
		AAI_SCOPED_TIMER("Check-Attack")
		// check attack
		m_attackManager->Update(*m_threatMap);

		//! @todo refactor storage/handling of threat map
		m_threatMap->UpdateLocalEnemyCombatPower(ETargetType::AIR, Map()->GetSectorMap());
		m_airForceManager->CheckStaticBombTargets(*m_threatMap);
		m_airForceManager->AirRaidBestTarget(2.0f);
		return;
	}

	// ressource management
	if (!(tick % 200))
	{
		AAI_SCOPED_TIMER("Resource-Management")
		m_execute->CheckRessources();
	}

	// update sectors
	if (!((tick+15) % 120))
	{
		AAI_SCOPED_TIMER("Update-Sectors")
		m_brain->UpdateAttackedByValues();
		m_map->UpdateSectors(m_threatMap);
		m_brain->UpdatePressureByEnemy(m_map->GetSectorMap());
	}

	// builder management
	if (!(tick % 917))
	{
		AAI_SCOPED_TIMER("Builder-Management")
		m_brain->UpdateDefenceCapabilities();
	}

	// update income
	if (!(tick % 30))
	{
		AAI_SCOPED_TIMER("Update-Income")
		m_brain->UpdateResources(m_aiCallback);
	}

	// building management
	if (!(tick % 97))
	{
		AAI_SCOPED_TIMER("Building-Management")
		m_execute->CheckConstruction();
	}

	// builder/factory management
	if (!(tick % 677))
	{
		AAI_SCOPED_TIMER("BuilderAndFactory-Management")
		m_unitTable->UpdateConstructors();
		m_execute->CheckConstructionOfNanoTurret();
	}

	if (!(tick % 337))
	{
		AAI_SCOPED_TIMER("Check-Factories")
		m_execute->CheckFactories();
	}

	if (!(tick % 1079))
	{
		AAI_SCOPED_TIMER("Check-Defenses")
		m_execute->CheckDefences();
	}

	// build radar/jammer
	if (!((tick+77) % 1200))
	{
		m_execute->CheckRecon();
		//execute->CheckJammer();
		m_execute->CheckStationaryArty();
		//execute->CheckAirBase();
	}

	// upgrade mexes
	if (!((tick+11) % 300))
	{
		AAI_SCOPED_TIMER("Check Upgrades")
		m_execute->CheckExtractorUpgrade();
		m_execute->CheckRadarUpgrade();
		//execute->CheckJammerUpgrade();
	}

	// recheck rally points
	if (!(tick % 1877))
	{
		AAI_SCOPED_TIMER("Recheck-Rally-Points")
		for (auto category = s_buildTree.GetCombatUnitCatgegories().begin();  category != s_buildTree.GetCombatUnitCatgegories().end(); ++category)
		{
			for (auto group = GetUnitGroupsList(*category).begin(); group != GetUnitGroupsList(*category).end(); ++group)
			{
				(*group)->CheckUpdateOfRallyPoint();
			}
		}
	}
}

const int* AAI::GetLosMap()
{
	if (m_losMap.empty()) {
		m_losMap.resize(m_skirmishAICallbacks->Map_getLosMap(m_skirmishAIId, nullptr, 0));
	}

	m_skirmishAICallbacks->Map_getLosMap(m_skirmishAIId, &m_losMap[0], m_losMap.size());

	return &m_losMap[0];
}

UnitDefId AAI::GetUnitDefId(UnitId unitId) const
{
	const springLegacyAI::UnitDef* def = m_aiCallback->GetUnitDef(unitId.id);

	if(def)
		return UnitDefId(def->id);
	else
		return UnitDefId();
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

