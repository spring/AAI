// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "AAIUnitStatistics.h"

AAIUnitStatistics::AAIUnitStatistics() 
{
	m_unitCostStatistics.resize( AAIUnitCategory::numberOfUnitCategories );
	m_unitBuildtimeStatistics.resize( AAIUnitCategory::numberOfUnitCategories );
	m_unitPrimaryAbilityStatistics.resize( AAIUnitCategory::numberOfUnitCategories );
	m_unitSecondaryAbilityStatistics.resize( AAIUnitCategory::numberOfUnitCategories );
};

AAIUnitStatistics::~AAIUnitStatistics()
{
	m_unitCostStatistics.clear();
	m_unitBuildtimeStatistics.clear();
	m_unitPrimaryAbilityStatistics.clear();
	m_unitSecondaryAbilityStatistics.clear();
};

void AAIUnitStatistics::Init(const std::vector<const springLegacyAI::UnitDef*>& unitDefs, const std::vector<UnitTypeProperties>& unitProperties, const std::vector< std::list<UnitDefId> >& unitsInCategory, const std::vector< std::list<UnitDefId> >& unitsInCombatCategory)
{
	//-----------------------------------------------------------------------------------------------------------------
	// calculate unit category statistics
	//-----------------------------------------------------------------------------------------------------------------
	for(uint32_t cat = 0; cat < AAIUnitCategory::numberOfUnitCategories; ++cat) 
	{
		for(auto unitDefId : unitsInCategory[cat])
		{
			m_unitBuildtimeStatistics[cat].AddValue( unitProperties[unitDefId.id].m_buildtime );
			m_unitCostStatistics[cat].AddValue( unitProperties[unitDefId.id].m_totalCost );
			m_unitPrimaryAbilityStatistics[cat].AddValue( unitProperties[unitDefId.id].m_primaryAbility );
			m_unitSecondaryAbilityStatistics[cat].AddValue( unitProperties[unitDefId.id].m_secondaryAbility );
		}

		// calculate average values after last value has been added
		m_unitBuildtimeStatistics[cat].Finalize();
		m_unitCostStatistics[cat].Finalize();
		m_unitPrimaryAbilityStatistics[cat].Finalize();
		m_unitSecondaryAbilityStatistics[cat].Finalize();
	}

	m_sensorStatistics.Init(unitDefs, unitProperties, unitsInCategory);
}

void SensorStatistics::Init(const std::vector<const springLegacyAI::UnitDef*>& unitDefs, const std::vector<UnitTypeProperties>& unitProperties, const std::vector< std::list<UnitDefId> >& unitsInCategory)
{
	const int index = AAIUnitCategory(EUnitCategory::STATIC_SENSOR).GetArrayIndex();

	for(auto defId = unitsInCategory[index].begin(); defId != unitsInCategory[index].end(); ++defId)
	{
		if(unitProperties[defId->id].m_unitType.IsRadar())
		{
			m_radarRanges.AddValue( unitDefs[defId->id]->radarRadius );
			m_radarCosts.AddValue( unitProperties[defId->id].m_totalCost );
		}
		
		if(unitProperties[defId->id].m_unitType.IsSonar())
		{
			m_sonarRanges.AddValue( unitDefs[defId->id]->sonarRadius );
			m_sonarCosts.AddValue( unitProperties[defId->id].m_totalCost );
		}

		if(unitProperties[defId->id].m_unitType.IsSeismicDetector())
		{
			m_seismicRanges.AddValue( unitDefs[defId->id]->seismicRadius );
			m_seismicCosts.AddValue( unitProperties[defId->id].m_totalCost );
		}
	}

	m_radarRanges.Finalize();
	m_sonarRanges.Finalize();
	m_seismicRanges.Finalize();

	m_radarCosts.Finalize();
	m_sonarCosts.Finalize();
	m_seismicCosts.Finalize();
}
