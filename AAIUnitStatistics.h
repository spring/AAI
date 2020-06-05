// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_UNIT_STATISTICS_H
#define AAI_UNIT_STATISTICS_H

#include <vector>
#include <list>
#include "AAIUnitTypes.h"

//! This class stores the statistical data (min, max, average) for each Unit Category (e.g. build cost)
class StatisticalData
{
public:
	StatisticalData() : m_minValue(0.0f), m_maxValue(0.0f), m_avgValue(0.0f), m_dataPoints(0u) {};

	//! Updates min and max value if necessary, adds given value to avg value
	void AddValue(float value)
	{
		if((value < m_minValue) || (m_dataPoints == 0u))
			m_minValue = value;

		if(value > m_maxValue)
			m_maxValue = value;
		
		m_avgValue += value;
		++m_dataPoints;
	};

	//! Calculates avg value (assumes update() has been called before)
	void Finalize()
	{
		if(m_dataPoints > 0u)
			m_avgValue /= static_cast<float>(m_dataPoints);
	}

	// getter functions
	float GetMinValue() const { return m_minValue; };
	float GetMaxValue() const { return m_maxValue; };
	float GetAvgValue() const { return m_avgValue; };

private:
	float m_minValue;

	float m_maxValue;

	float m_avgValue;

	unsigned int m_dataPoints;
};

class AAIUnitStatistics
{
public:
	AAIUnitStatistics();

	~AAIUnitStatistics();

	//! Calculates values for given input data
	void Init(const std::vector<UnitTypeProperties>& unitProperties, const std::vector< std::list<int> >& unitsInCategory);

	const StatisticalData& getCostStatistics(const AAIUnitCategory& category) const { return m_costStatistics[category.getCategoryIndex()]; };

	const StatisticalData& getBuildtimeStatistics(const AAIUnitCategory& category) const { return m_buildtimeStatistics[category.getCategoryIndex()]; };

	const StatisticalData& getRangeStatistics(const AAIUnitCategory& category) const { return m_range[category.getCategoryIndex()]; };

private:
	//! Min,max,avg cost for every unit category
	std::vector<StatisticalData> m_costStatistics;
	
	//! Min,max,avg buildtime for every unit category
	std::vector<StatisticalData> m_buildtimeStatistics;

	//! Range of unit category relevant ability: max range of weapons (Combat units, artillery and static defences), line of sight (scouts), radar/sonar
	std::vector<StatisticalData> m_range;
};

#endif