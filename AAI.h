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

#include "LegacyCpp/IGlobalAI.h"
#include <list>
#include <vector>
#include "aidef.h"

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
	AAI();
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
	
	//! @brief Returns the number of AAI instances
	int GetNumberOfAAIInstances() const { return s_aaiInstances; }

	//! @brief Returns the id of this AAI instance
	int GetAAIInstance() const { return m_aaiInstance; }

	//! @brief Returns current game phase
	const GamePhase& GetGamePhase() const { return m_gamePhase; }

	// called every frame
	void Update();

	//! @brief Returns pointer to AI callback
	IAICallback* GetAICallback() const { return m_aiCallback; }

	//! @brief Returns the side of this AAI instance
	int GetSide() const { return m_side; }

	std::list<AAIBuildTask*>& GetBuildTasks() { return build_tasks; }

	std::vector<std::list<AAIGroup*> >& GetGroupList() { return group_list; }


	AAIBrain*           Getbrain() { return brain; }
	AAIExecute*         Getexecute() { return execute; }
	AAIUnitTable*       Getut() { return ut; }
	AAIMap*             Getmap() { return map; }
	AAIAirForceManager* Getaf() { return af; }
	AAIAttackManager*   Getam() { return am; }
	AAIBuildTable*      Getbt() { return bt; }
	

private:
	Profiler* GetProfiler(){ return profiler; }

	//! Pointer to AI callback
	IAICallback* m_aiCallback;

	// list of buildtasks
	std::list<AAIBuildTask*> build_tasks;

	AAIBrain *brain;			// makes decisions
	AAIExecute *execute;		// executes all kinds of tasks
	AAIUnitTable *ut;			// unit table
	AAIBuildTable *bt;			// buildtable for the different units
	AAIMap *map;				// keeps track of all constructed buildings and searches new constr. sites
	AAIAirForceManager *af;		// coordinates the airforce
	AAIAttackManager *am;		// coordinates combat forces

	std::vector<std::list<AAIGroup*> > group_list;  // unit groups

	Profiler* profiler;

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

