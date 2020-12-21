// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_BUILDTASK_H
#define AAI_BUILDTASK_H

#include "System/float3.h"
#include "aidef.h"
#include "AAIMap.h"
#include "AAIUnitTable.h"

class AAI;

class AAIBuildTask
{
	friend AAIConstructor;

public:
	AAIBuildTask(UnitId unitId, UnitDefId unitDefId, const float3& buildsite, UnitId constructor);

	~AAIBuildTask(void);

	//! @brief Indicates that the responsible construction unit has been killed
	void BuilderDestroyed(AAIMap* map, AAIUnitTable* unitTable);

	//! @brief Checks if task belongs to killed unit (and has thus failed); if yes, cleans up the buildmap and notifies the construction unit
	bool CheckIfConstructionFailed(AAI* ai, UnitId unitId);

	//! @brief Checks if task belongs to finished unit; if yes, notifies the construction unit
	bool CheckIfConstructionFinished(AAIUnitTable* unitTable, UnitId unitId);

	//! @brief Returns true if buildtask belongs to expensive (> 0.7+avg cost) unit/building of given category
	bool IsExpensiveUnitOfCategoryInSector(AAI* ai, const AAIUnitCategory& category, const AAISector* sector) const;

	//! @brief Returns the corresponding constructor (or nullptr if none)
	AAIConstructor* GetConstructor(AAIUnitTable* unitTable) const { return m_constructor.IsValid() ? unitTable->units[m_constructor.id].cons : nullptr; }

private:
	//! The unit id of the unit/building that is being constructed
	UnitId m_unitId;

	//! The unit definition of the unit/building that is being constructed
	UnitDefId m_defId;

	//! The id of the construction unit
	UnitId m_constructor;

	//! The location where the building/unit is being constructed
	float3 m_buildsite;
};

#endif

