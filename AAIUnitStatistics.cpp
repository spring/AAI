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
	m_unitCostStatistics.resize( AAIUnitCategory::getNumberOfUnitCategories() );
	m_unitBuildtimeStatistics.resize( AAIUnitCategory::getNumberOfUnitCategories() );
	m_unitPrimaryAbilityStatistics.resize( AAIUnitCategory::getNumberOfUnitCategories() );
	m_unitSecondaryAbilityStatistics.resize( AAIUnitCategory::getNumberOfUnitCategories() );

	m_combatCostStatistics.resize( AAIUnitCategory::getNumberOfUnitCategories() );
	m_combatBuildtimeStatistics.resize( AAIUnitCategory::getNumberOfUnitCategories() );
	m_combatRangeStatistics.resize( AAIUnitCategory::getNumberOfUnitCategories() );
	m_combatSpeedStatistics.resize( AAIUnitCategory::getNumberOfUnitCategories() );
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

void AAIUnitStatistics::Init(const std::vector<UnitTypeProperties>& unitProperties, const std::vector< std::list<int> >& unitsInCategory, const std::vector< std::list<int> >& unitsInCombatCategory)
{
	//-----------------------------------------------------------------------------------------------------------------
	// calculate unit category statistics
	//-----------------------------------------------------------------------------------------------------------------
	for(uint32_t cat = 0; cat < AAIUnitCategory::getNumberOfUnitCategories(); ++cat) 
	{
		for(std::list<int>::const_iterator id = unitsInCategory[cat].begin(); id != unitsInCategory[cat].end(); ++id)
		{
			m_unitBuildtimeStatistics[cat].AddValue( unitProperties[*id].m_buildtime );
			m_unitCostStatistics[cat].AddValue( unitProperties[*id].m_totalCost );
			m_unitPrimaryAbilityStatistics[cat].AddValue( unitProperties[*id].m_range );
			m_unitSecondaryAbilityStatistics[cat].AddValue( unitProperties[*id].m_maxSpeed );
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
	for(uint32_t cat = 0; cat < AAICombatCategory::GetNumberOfCombatCategories(); ++cat) 
	{
		for(std::list<int>::const_iterator id = unitsInCombatCategory[cat].begin(); id != unitsInCombatCategory[cat].end(); ++id)
		{
			m_combatCostStatistics[cat].AddValue( unitProperties[*id].m_totalCost );
			m_combatBuildtimeStatistics[cat].AddValue( unitProperties[*id].m_buildtime );
			m_combatRangeStatistics[cat].AddValue( unitProperties[*id].m_range );
			m_combatSpeedStatistics[cat].AddValue( unitProperties[*id].m_maxSpeed );
		}

		m_combatCostStatistics[cat].Finalize();
		m_combatBuildtimeStatistics[cat].Finalize();
		m_combatRangeStatistics[cat].Finalize();
		m_combatSpeedStatistics[cat].Finalize();
	}
}
