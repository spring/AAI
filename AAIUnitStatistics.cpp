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
	m_costStatistics.resize( AAIUnitCategory::getNumberOfUnitCategories() );
	m_buildtimeStatistics.resize( AAIUnitCategory::getNumberOfUnitCategories() );
	m_range.resize( AAIUnitCategory::getNumberOfUnitCategories() );
};

AAIUnitStatistics::~AAIUnitStatistics()
{
	m_costStatistics.clear();
	m_buildtimeStatistics.clear();
	m_range.clear();
};

void AAIUnitStatistics::Init(const std::vector<UnitTypeProperties>& unitProperties, const std::vector< std::list<int> >& unitsInCategory)
{
	for(int cat = 0; cat < AAIUnitCategory::getNumberOfUnitCategories(); ++cat) 
	{
		for(std::list<int>::const_iterator id = unitsInCategory[cat].begin(); id != unitsInCategory[cat].end(); ++id)
		{
			m_buildtimeStatistics[cat].AddValue( unitProperties[*id].m_buildtime );
			m_costStatistics[cat].AddValue( unitProperties[*id].m_totalCost );
			m_range[cat].AddValue( unitProperties[*id].m_range );
		}

		// calculate average values after last value has been added
		m_buildtimeStatistics[cat].Finalize();
		m_costStatistics[cat].Finalize();
		m_range[cat].Finalize();
	}
}