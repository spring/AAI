// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include <set>
using namespace std;

#include "AAIBuildTask.h"
#include "AAI.h"
#include "AAIConstructor.h"
#include "AAIBuildTable.h"
#include "AAIExecute.h"
#include "AAISector.h"

AAIBuildTask::AAIBuildTask(UnitId unitId, UnitDefId unitDefId, const float3& buildsite, UnitId constructor) :
	m_unitId(unitId),
	m_defId(unitDefId),
	m_buildsite(buildsite),
	m_constructor(constructor)
{
}

AAIBuildTask::~AAIBuildTask(void)
{
}

void AAIBuildTask::BuilderDestroyed(AAIMap* map, AAIUnitTable* unitTable)
{
	m_constructor.Invalidate();

	// com only allowed if buildpos is inside the base
	bool commander = false;

	AAISector* sector = map->GetSectorOfPos(m_buildsite);

	if(sector && (sector->GetDistanceToBase() == 0) )
		commander = true;

	// look for new builder
	AAIConstructor* nextBuilder = unitTable->FindClosestAssistant(m_buildsite, 10, commander);

	if(nextBuilder)
	{
		nextBuilder->TakeOverConstruction(this);
		m_constructor = nextBuilder->m_myUnitId;
	}
}

bool AAIBuildTask::CheckIfConstructionFailed(AAI* ai, UnitId unitId)
{
	if(m_unitId == unitId)
	{
		// cleanup buildmap etc.
		if(ai->s_buildTree.GetMovementType(m_defId).IsStatic())
			ai->Execute()->ConstructionFailed(m_buildsite, m_defId);

		AAIConstructor* constructor = GetConstructor(ai->UnitTable());

		if(constructor)
			constructor->ConstructionFinished();

		return true;
	}
	else
		return false;
}

bool AAIBuildTask::CheckIfConstructionFinished(AAIUnitTable* unitTable, UnitId unitId)
{
	if (m_unitId == unitId)
	{
		AAIConstructor* constructor = GetConstructor(unitTable);

		if(constructor)
			constructor->ConstructionFinished();

		return true;
	}
	else
		return false;	
}

bool AAIBuildTask::IsExpensiveUnitOfCategoryInSector(AAI* ai, const AAIUnitCategory& category, const AAISector* sector) const
{
	if(    (ai->s_buildTree.GetUnitCategory(m_defId) == category)
		&& sector->PosInSector(m_buildsite) )
	{
		const StatisticalData& costStatistics = ai->s_buildTree.GetUnitStatistics(ai->GetSide()).GetUnitCostStatistics(category);

		if( ai->s_buildTree.GetTotalCost(m_defId) > 0.7f * costStatistics.GetAvgValue() )
			return true;
	}

	return false;
}

