// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_UNITTABLE_H
#define AAI_UNITTABLE_H

#include <set>

#include "aidef.h"
#include "AAIBuildTable.h"

class AAI;
class AAIExecute;
class AAIConstructor;

class AAIUnitTable
{
public:
	AAIUnitTable(AAI *ai);
	~AAIUnitTable(void);

	//! @brief Returns the number of active (i.e. not under construction anymore) units of the given category
	int GetNumberOfActiveUnitsOfCategory(const AAIUnitCategory& category)            const { return m_activeUnitsOfCategory[category.GetArrayIndex()]; };

	//! @brief Returns the number of units under construction of the given category
	int GetNumberOfUnitsUnderConstructionOfCategory(const AAIUnitCategory& category) const { return m_underConstructionUnitsOfCategory[category.GetArrayIndex()]; };

	//! @brief Returns the number of requested (i.e. construction has not started yet) units of the given category
	int GetNumberOfRequestedUnitsOfCategory(const AAIUnitCategory& category)         const { return m_requestedUnitsOfCategory[category.GetArrayIndex()]; };

	//! @brief Returns the number of units requested or under construction of the given category
	int GetNumberOfFutureUnitsOfCategory(const AAIUnitCategory& category)         const { return (  m_requestedUnitsOfCategory[category.GetArrayIndex()] 
	                                                                                              + m_underConstructionUnitsOfCategory[category.GetArrayIndex()]); };

	//! @brief Returns the number of units of the given category that are active, requested or under construction
	int GetTotalNumberOfUnitsOfCategory(const AAIUnitCategory& category)         const { return (   m_requestedUnitsOfCategory[category.GetArrayIndex()]
	                                                                                              + m_underConstructionUnitsOfCategory[category.GetArrayIndex()]
	                                                                                              + m_activeUnitsOfCategory[category.GetArrayIndex()]); };

	bool AddUnit(int unit_id, int def_id, AAIGroup *group = 0, AAIConstructor *cons = 0);
	void RemoveUnit(int unit_id);

	void AddScout(int unit_id);
	void RemoveScout(int unit_id);

	void AddConstructor(UnitId unitId, UnitDefId unitDefId);
	void RemoveConstructor(int unit_id, int def_id);

	void AddCommander(UnitId unitId, UnitDefId unitDefId);
	void RemoveCommander(int unit_id, int def_id);

	void AddExtractor(int unit_id);
	void RemoveExtractor(int unit_id);

	void AddPowerPlant(UnitId unitId, UnitDefId unitDefId);
	void RemovePowerPlant(int unit_id);

	void AddMetalMaker(int unit_id, int def_id);
	void RemoveMetalMaker(int unit_id);

	void AddJammer(int unit_id, int def_id);
	void RemoveJammer(int unit_id);

	void AddRecon(int unit_id, int def_id);
	void RemoveRecon(int unit_id);

	void AddStationaryArty(int unit_id, int def_id);
	void RemoveStationaryArty(int unit_id);

	AAIConstructor* FindBuilder(int building, bool commander);

	//! @brief Finds the closest builder and stores the time it needs to reach the given positon
	AAIConstructor* FindClosestBuilder(UnitDefId building, const float3 *pos, bool commander, float *timeToReachPosition);

	AAIConstructor* FindClosestAssistant(float3 pos, int importance, bool commander);

	void EnemyKilled(int unit);

	void SetUnitStatus(int unit, UnitTask status);

	void AssignGroupToEnemy(int unit, AAIGroup *group);

	// determine whether unit with specified def/unit id is commander/constrcutor
	bool IsBuilder(UnitId unitId);

	//! @brief Shall be called when unit have been requested (i.e. added to buildqueue)
	void UnitRequested(const AAIUnitCategory& category, int number = 1);

	// called when unit request failed (e.g. builder has been killed on the way to the crash site)
	void UnitRequestFailed(const AAIUnitCategory& category);

	// called when unit of specified catgeory has been created (= construction started)
	void UnitCreated(const AAIUnitCategory& category);
	
	//! @brief Shall be called when a unit under construction has been killed to update internal counters
	void UnitUnderConstructionKilled(const AAIUnitCategory& category);

	//! @brief Shall be called when construction of unit has been finished
	void UnitFinished(const AAIUnitCategory& category);

	//! @brief Shall be called when an active (i.e. construction finished) unit has been killed to update internal counters
	void ActiveUnitKilled(const AAIUnitCategory& category);

	// units[i].unitId = -1 -> not used , -2 -> enemy unit
	vector<AAIUnit> units;

	set<int> constructors;
	set<int> metal_makers;
	set<int> jammers;
	set<int> recon;

	// number of active/under construction units of all different types
	int activeBuilders, futureBuilders;
	int activeFactories, futureFactories;

private:
	//! @todo These functions are duplicated in buildtable -> remove duplication after unit category handling is reworked
	bool IsFactory(UnitDefId unitDefId)  const { return ai->s_buildTree.GetUnitType(unitDefId).IsFactory(); };

	bool IsBuilder(UnitDefId unitDefId)  const { return ai->s_buildTree.GetUnitType(unitDefId).IsBuilder(); };

	bool IsAssister(UnitDefId unitDefId) const { return ai->s_buildTree.GetUnitType(unitDefId).IsConstructionAssist(); };

	//! Number of active (i.e. not under construction anymore) units of each unit category
	std::vector<int> m_activeUnitsOfCategory;

	//! Number of units under contsruction of each unit category
	std::vector<int> m_underConstructionUnitsOfCategory;

	//! Number of requested units (i.e. construction has not started yet) of each unit category
	std::vector<int> m_requestedUnitsOfCategory;

	set<int> scouts;
	set<int> extractors;
	set<int> power_plants;
	set<int> stationary_arty;
	AAI *ai;

};

#endif

