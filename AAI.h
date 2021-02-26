// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_H
#define AAI_H

#include <list>
#include <vector>

#include "ExternalAI/Interface/SSkirmishAICallback.h"
#include "LegacyCpp/IGlobalAI.h"

#include "aidef.h"
#include "AAIBuildTree.h"

namespace springLegacyAI {
	class IAICallback;
}

using namespace springLegacyAI;

class AAIExecute;
class Profiler;
class AAIBrain;
class AAIBuildTask;
class AAIAirForceManager;
class AAIAttackManager;
class AAIBuildTable;
class AAIUnitTable;
class AAIMap;
class AAIGroup;

class AAI : public IGlobalAI
{
public:
	AAI(int skirmishAIId, const struct SSkirmishAICallback* callback);
	virtual ~AAI();

	void InitAI(IGlobalAICallback* callback, int team);

	void UnitCreated(int unit, int builder);								//called when a new unit is created on ai team
	void UnitFinished(int unit);											//called when an unit has finished building
	void UnitIdle(int unit);												//called when a unit go idle and is not assigned to any group
	void UnitDestroyed(int unit, int attacker);								//called when a unit is destroyed
	void UnitDamaged(int damaged,int attacker,float damage,float3 dir);		//called when one of your units are damaged
	void UnitMoveFailed(int unit);

	void EnemyEnterLOS(int enemy);
	void EnemyLeaveLOS(int enemy);

	void EnemyEnterRadar(int enemy);				//called when an enemy enters radar coverage (not called if enemy go directly from not known -> los)
	void EnemyLeaveRadar(int enemy);				//called when an enemy leaves radar coverage (not called if enemy go directly from inlos -> now known)

	void RecvChatMessage(const char* /*msg*/,int /*player*/) {}	//called when someone writes a chat msg
	void RecvLuaMessage(const char* /*inData*/, const char** /*outData*/) {}

	void EnemyDamaged(int /*damaged*/,int /*attacker*/,float /*damage*/,float3 /*dir*/) {}	//called when an enemy inside los or radar is damaged
	void EnemyDestroyed(int enemy, int attacker);
	void Log(const char* format, ...);
	void LogConsole(const char* format, ...);

	int HandleEvent(int msg, const void *data);

	 //! @brief Creates a buildTask (if a building is constructed)
	void ConstructionStarted(UnitId unitId, UnitDefId unitDefId, UnitId constructor);
	
	//! @brief Returns the number of AAI instances
	int GetNumberOfAAIInstances() const { return s_aaiInstances; }

	//! @brief Returns the id of this AAI instance
	int GetAAIInstance() const { return m_aaiInstance; }

	//! @brief Returns current game phase
	const GamePhase& GetGamePhase() const { return m_gamePhase; }

	// called every frame
	void Update();

	//! Workaround to get current LOS Map (ai callback version of legacy CPP interface is bugged)
	const int* GetLosMap();

	//! @brief Returns the unitDefId for a given unitId
	UnitDefId GetUnitDefId(UnitId unitId) const;

	//! @brief Returns the unit definition for the given unit name
	const springLegacyAI::UnitDef* GetUnitDef(const std::string& unitName) const { return m_aiCallback->GetUnitDef(unitName.c_str()); }

	//! @brief Returns pointer to AI callback
	IAICallback* GetAICallback() const { return m_aiCallback; }

	//! @brief Returns the side of this AAI instance
	int GetSide() const { return m_side; }

	//! @brief Return team (not ally team) of this AAI instance
	int GetMyTeamId() const { return m_myTeamId; }

	std::list<AAIBuildTask*>& GetBuildTasks() { return build_tasks; }

	//! @brief Returns the list of units groups for the given unit category
	std::list<AAIGroup*>& GetUnitGroupsList(const AAIUnitCategory& category) 
	{
		return m_unitGroupsOfCategoryLists[category.GetArrayIndex()]; 
	}

	AAIMap*             Map() { return m_map; }
	AAIBrain*           Getbrain() { return m_brain; }
	AAIExecute*         Getexecute() { return execute; }
	AAIUnitTable*       Getut() { return ut; }
	AAIAirForceManager* Getaf() { return af; }
	AAIAttackManager*   Getam() { return am; }
	AAIBuildTable*      Getbt() { return bt; }

	//! The buildtree (who builds what, which unit belongs to which side, ...)
	static AAIBuildTree s_buildTree;

private:
	Profiler* GetProfiler(){ return profiler; }

	//! Pointer to AI callback
	IAICallback* m_aiCallback;

	//! The ID of the AI (used to access the correct SkirmishAICallback)
	int m_skirmishAIId;

	//! The SkirmishAICallback of all AIs
	const struct SSkirmishAICallback* m_skirmishAICallbacks;

	//! LOS Map
	std::vector<int> m_losMap;

	// list of buildtasks
	std::list<AAIBuildTask*> build_tasks;

	//! Stores information about the map (shared between all AAI instances) and AI specific map related data (e.g. build map, threat map, defence maps, sectors, ...)
	AAIMap *m_map;

	//! AAI's brain is responsible for analyzing the current situation of the game and making the appropriate decisions
	AAIBrain *m_brain;


	AAIExecute *execute;		// executes all kinds of tasks
	AAIUnitTable *ut;			// unit table
	AAIBuildTable *bt;			// buildtable for the different units
	
	AAIAirForceManager *af;		// coordinates the airforce
	AAIAttackManager *am;		// coordinates combat forces

	//! List of groups of unit of the different categories
	std::vector< std::list<AAIGroup*> > m_unitGroupsOfCategoryLists;

	Profiler* profiler;

	//! Id of the team (not ally team) of the AAI instance
	int m_myTeamId;

	//! Side of this AAI instance; 0 always neutral, for TA-like mods 1 = Arm, 2 = Core
	int m_side;

	//! File to which log messages are written
	FILE *m_logFile;

	//! Initialization state - true if AAI has been sucessfully initialized and ready to run
	bool m_initialized;

	//! True if game/mod and general config have been loaded successfully
	bool m_configLoaded; 

	//! Counter how many instances of AAI exist - if there is more than one instance of AAI, needed to ensure to allocate/free shared memory (e.g. unit learning data) only once
	static int s_aaiInstances;

	//! Id of this instance of AAI
	int m_aaiInstance;

	//! Current game phase
	GamePhase m_gamePhase; 
};

#endif

