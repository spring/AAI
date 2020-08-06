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

	m_combatCostStatistics.resize( AAIUnitCategory::numberOfUnitCategories );
	m_combatBuildtimeStatistics.resize( AAIUnitCategory::numberOfUnitCategories );
	m_combatRangeStatistics.resize( AAIUnitCategory::numberOfUnitCategories );
	m_combatSpeedStatistics.resize( AAIUnitCategory::numberOfUnitCategories );
};

AAIUnitStatistics::~AAIUnitStatistics()
{
	m_unitCostStatistics.clear();
	m_unitBuildtimeStatistics.clear();
	m_unitPrimaryAbilityStatistics.clear();
	m_unitSecondaryAbilityStatistics.clear();

	m_combatCostStatistics.clear();
	m_combatBuildtimeStatistics.clear();
	m_combatRangeStatistics.clear();
	m_combatSpeedStatistics.clear();
};

void AAIUnitStatistics::Init(const std::vector<const springLegacyAI::UnitDef*>& unitDefs, const std::vector<UnitTypeProperties>& unitProperties, const std::vector< std::list<UnitDefId> >& unitsInCategory, const std::vector< std::list<int> >& unitsInCombatCategory)
{
	//-----------------------------------------------------------------------------------------------------------------
	// calculate unit category statistics
	//-----------------------------------------------------------------------------------------------------------------
	for(uint32_t cat = 0; cat < AAIUnitCategory::numberOfUnitCategories; ++cat) 
	{
		for(auto defId = unitsInCategory[cat].begin(); defId != unitsInCategory[cat].end(); ++defId)
		{
			m_unitBuildtimeStatistics[cat].AddValue( unitProperties[defId->id].m_buildtime );
			m_unitCostStatistics[cat].AddValue( unitProperties[defId->id].m_totalCost );
			m_unitPrimaryAbilityStatistics[cat].AddValue( unitProperties[defId->id].m_primaryAbility );
			m_unitSecondaryAbilityStatistics[cat].AddValue( unitProperties[defId->id].m_maxSpeed );
		}

		// calculate average values after last value has been added
		m_unitBuildtimeStatistics[cat].Finalize();
		m_unitCostStatistics[cat].Finalize();
		m_unitPrimaryAbilityStatistics[cat].Finalize();
		m_unitSecondaryAbilityStatistics[cat].Finalize();
	}

	//-----------------------------------------------------------------------------------------------------------------
	// calculate combat category statistics
	//-----------------------------------------------------------------------------------------------------------------
	for(uint32_t cat = 0; cat < AAICombatCategory::numberOfCombatCategories; ++cat) 
	{
		for(auto id = unitsInCombatCategory[cat].begin(); id != unitsInCombatCategory[cat].end(); ++id)
		{
			m_combatCostStatistics[cat].AddValue( unitProperties[*id].m_totalCost );
			m_combatBuildtimeStatistics[cat].AddValue( unitProperties[*id].m_buildtime );
			m_combatRangeStatistics[cat].AddValue( unitProperties[*id].m_primaryAbility );
			m_combatSpeedStatistics[cat].AddValue( unitProperties[*id].m_maxSpeed );
		}

		m_combatCostStatistics[cat].Finalize();
		m_combatBuildtimeStatistics[cat].Finalize();
		m_combatRangeStatistics[cat].Finalize();
		m_combatSpeedStatistics[cat].Finalize();
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
