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

//! Used to store the information of a construction unit that is currently available
class AvailableConstructor
{
public:
	AvailableConstructor(AAIConstructor* constructor, float travelTimeToBuildSite) : m_constructor(constructor), m_travelTimeToBuildSite(travelTimeToBuildSite) {}

	AvailableConstructor() : AvailableConstructor(nullptr, 0.0f) {}

	void SetAvailableConstructor(AAIConstructor* constructor, float travelTimeToBuildSite) 
	{
		m_constructor           = constructor;
		m_travelTimeToBuildSite = travelTimeToBuildSite;
	}

	bool            IsValid()               const { return m_constructor != nullptr; }

	AAIConstructor* Constructor()           const { return m_constructor; }

	float           TravelTimeToBuildSite() const { return m_travelTimeToBuildSite; }

private:
	AAIConstructor* m_constructor;

	float           m_travelTimeToBuildSite;
};

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

	//! @brief Returns the UnitDefId of the given (own) unit
	UnitDefId GetUnitDefId(UnitId unitId) const { return UnitDefId(units[unitId.id].def_id); }

	bool AddUnit(int unit_id, int def_id, AAIGroup *group = 0, AAIConstructor *cons = 0);
	void RemoveUnit(int unit_id);

	void AddScout(int unit_id);
	void RemoveScout(int unit_id);

	void AddConstructor(UnitId unitId, UnitDefId unitDefId);
	void RemoveConstructor(UnitId unitId, UnitDefId unitDefId);
	const std::set<UnitId>& GetConstructors() const { return m_constructors; }

	void AddExtractor(int unit_id);
	void RemoveExtractor(int unit_id);

	void AddPowerPlant(UnitId unitId, UnitDefId unitDefId);
	void RemovePowerPlant(int unit_id);

	void AddMetalMaker(int unit_id, int def_id);
	void RemoveMetalMaker(int unit_id);

	void AddJammer(int unit_id, int def_id);
	void RemoveJammer(int unit_id);

	void AddStaticSensor(UnitId unitId);
	void RemoveStaticSensor(UnitId unitId);
	const std::set<UnitId>& GetStaticSensors() const { return m_staticSensors; }

	void AddStationaryArty(int unit_id, int def_id);
	void RemoveStationaryArty(int unit_id);

	//! @brief Returns the number of active builders (incl. commander)
	int GetNumberOfActiveBuilders() const { return m_activeUnitsOfCategory[AAIUnitCategory(EUnitCategory::COMMANDER).GetArrayIndex()] + m_activeUnitsOfCategory[AAIUnitCategory(EUnitCategory::MOBILE_CONSTRUCTOR).GetArrayIndex()]; }

	//! @brief Returns any available builder for the given unit
	AAIConstructor* FindBuilder(UnitDefId building, bool commander);

	//! @brief Finds the closest builder and stores the time it needs to reach the given positon
	AvailableConstructor FindClosestBuilder(UnitDefId building, const float3& position, bool commander);

	//! @brief Finds the closests assistance suitable to assist cosntruction at given position (nullptr if none found) 
	AAIConstructor* FindClosestAssistant(const float3& pos, int importance, bool commander);

	void EnemyKilled(int unit);

	void SetUnitStatus(int unit, UnitTask status);

	void AssignGroupToEnemy(int unit, AAIGroup *group);

	//! @brief Shall be called when unit have been requested (i.e. added to buildqueue)
	void UnitRequested(const AAIUnitCategory& category, int number = 1);

	// called when unit request failed (e.g. builder has been killed on the way to the crash site)
	void UnitRequestFailed(const AAIUnitCategory& category);

	// called when unit of specified catgeory has been created (= construction started)
	void ConstructionStarted(const AAIUnitCategory& category);
	
	//! @brief Shall be called when a unit under construction has been killed to update internal counters
	void UnitUnderConstructionKilled(const AAIUnitCategory& category);

	//! @brief Shall be called when construction of unit has been finished
	void UnitFinished(const AAIUnitCategory& category);

	//! @brief Shall be called when an active (i.e. construction finished) unit has been killed to update internal counters
	void ActiveUnitKilled(const AAIUnitCategory& category);

	//! @brief Calls the update() fucntion for every active constructor (e.g. looks for assistants for constructions, checks if factories are idle, ...)
	void UpdateConstructors();

	AAIUnit& GetUnit(UnitId unitId) { return units[unitId.id]; }

	// units[i].unitId = -1 -> not used , -2 -> enemy unit
	std::vector<AAIUnit> units;

	std::set<int> metal_makers;
	std::set<int> jammers;

	// number of active/under construction units of all different types
	int activeFactories, futureFactories;

private:
	//! Number of active (i.e. not under construction anymore) units of each unit category
	std::vector<int> m_activeUnitsOfCategory;

	//! Number of units under contsruction of each unit category
	std::vector<int> m_underConstructionUnitsOfCategory;

	//! Number of requested units (i.e. construction has not started yet) of each unit category
	std::vector<int> m_requestedUnitsOfCategory;

	std::set<int> scouts;
	std::set<int> extractors;
	std::set<int> power_plants;
	std::set<int> stationary_arty;

	//! A list of all constructors (mobile and static)
	std::set<UnitId> m_constructors;

	//! A list of all static sensors (radar, seismic, jammer)
	std::set<UnitId> m_staticSensors;

	AAI *ai;
};

#endif

