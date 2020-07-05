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
	StatisticalData() : m_minValue(0.0f), m_maxValue(0.0f), m_avgValue(0.0f), m_valueRange(0.0f), m_dataPoints(0u) {};

	//! Updates min and max value if necessary, adds given value to avg value
	void AddValue(float value)
	{
		if((value < m_minValue) || (m_dataPoints == 0u))
			m_minValue = value;

		if(value > m_maxValue)
			m_maxValue = value;
		
		m_avgValue += value;
		++m_dataPoints;
	}

	//! Calculates avg value (assumes update() has been called before)
	void Finalize()
	{
		if(m_dataPoints > 0u)
			m_avgValue /= static_cast<float>(m_dataPoints);

		if(m_dataPoints > 1u)
		{
			m_valueRange = m_maxValue - m_minValue;

			if(m_valueRange < 0.00001f)
				m_valueRange = 0.0f;
		}
	}

	// getter functions
	float GetMinValue() const { return m_minValue; }
	float GetMaxValue() const { return m_maxValue; }
	float GetAvgValue() const { return m_avgValue; }

	//! @brief Returns the normalized (interval [0:1]) deviation from max value (value must be between min and max)
	float GetNormalizedDeviationFromMax(float value) const
	{
		if(m_valueRange != 0.0f) // range only exactly 0.0f if insufficient number of data points or difference too small
			return (m_maxValue - value) / m_valueRange;
		else
			return 0.0f;
	}

	//! @brief Returns the normalized (interval [0:1]) deviation from max value (value must be between min and max)
	float GetNormalizedDeviationFromMin(float value) const
	{
		if(m_valueRange != 0.0f) // range only exactly 0.0f if insufficient number of data points or difference too small
			return (value - m_minValue) / m_valueRange;
		else
			return 0.0f;
	}

private:
	float m_minValue;

	float m_maxValue;

	float m_avgValue;

	float m_valueRange;

	unsigned int m_dataPoints;
};

class AAIUnitStatistics
{
public:
	AAIUnitStatistics();

	~AAIUnitStatistics();

	//! Calculates values for given input data
	void Init(const std::vector<UnitTypeProperties>& unitProperties, const std::vector< std::list<UnitDefId> >& unitsInCategory, const std::vector< std::list<int> >& unitsInCombatCategory);

	const StatisticalData& GetUnitCostStatistics(const AAIUnitCategory& category) const { return m_unitCostStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetUnitBuildtimeStatistics(const AAIUnitCategory& category) const { return m_unitBuildtimeStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetUnitPrimaryAbilityStatistics(const AAIUnitCategory& category) const { return m_unitPrimaryAbilityStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetUnitSecondaryAbilityStatistics(const AAIUnitCategory& category) const { return m_unitSecondaryAbilityStatistics[category.GetArrayIndex()]; }
	
	const StatisticalData& GetCombatCostStatistics(const AAICombatCategory& category) const { return m_combatCostStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetCombatBuildtimeStatistics(const AAICombatCategory& category) const { return m_combatBuildtimeStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetCombatRangeStatistics(const AAICombatCategory& category) const { return m_combatRangeStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetCombatSpeedStatistics(const AAICombatCategory& category) const { return m_combatSpeedStatistics[category.GetArrayIndex()]; }

private:
	//! Min,max,avg cost for every unit category
	std::vector<StatisticalData> m_unitCostStatistics;
	
	//! Min,max,avg buildtime for every unit category
	std::vector<StatisticalData> m_unitBuildtimeStatistics;

	//! Min,max,avg:  sight range for scouts, radar/sonar/artillery range, build 
	std::vector<StatisticalData> m_unitPrimaryAbilityStatistics;

	//! Min,max,avg:  speed for scouts, mobile constructors, mobile artillery
	std::vector<StatisticalData> m_unitSecondaryAbilityStatistics;

	//! Min,max,avg cost for every unit category
	std::vector<StatisticalData> m_combatCostStatistics;
	
	//! Min,max,avg buildtime for every unit category
	std::vector<StatisticalData> m_combatBuildtimeStatistics;

	//! Min.max,avg range of combat unit category
	std::vector<StatisticalData> m_combatRangeStatistics;

	//! Min,max,avg speed for every combat unit category
	std::vector<StatisticalData> m_combatSpeedStatistics;
};

#endif