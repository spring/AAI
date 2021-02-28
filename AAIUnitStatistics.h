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
#include "aidef.h"
#include "AAITypes.h"
#include "AAIUnitTypes.h"
#include "LegacyCpp/UnitDef.h"

//! @brief This class stores the frequency the AI got attacked by a certain combat category (surface, air, floater, submerged) in a certain game phase
class AttackedByRatesPerGamePhase
{
public:
	AttackedByRatesPerGamePhase() 
	{ 
		m_attackedByRatesPerGamePhase.resize(GamePhase::numberOfGamePhases);
	}

	void AddAttack(const GamePhase& gamePhase, const AAITargetType& attackerTargetType)
	{
		m_attackedByRatesPerGamePhase[gamePhase.GetArrayIndex()].AddValueForTargetType(attackerTargetType, 1.0f);
	}

	void SetAttackedByRate(const GamePhase& gamePhase, const AAITargetType& attackerTargetType, float rate)
	{
		m_attackedByRatesPerGamePhase[gamePhase.GetArrayIndex()].SetValueForTargetType(attackerTargetType, rate);
	}

	float GetAttackedByRate(const GamePhase& gamePhase, const AAITargetType& attackerTargetType) const
	{
		return m_attackedByRatesPerGamePhase[gamePhase.GetArrayIndex()].GetValueOfTargetType(attackerTargetType);
	}

	void DecreaseByFactor(const GamePhase& updateUntilGamePhase, float factor)
	{
		for(int i = 0; i <= updateUntilGamePhase.GetArrayIndex(); ++i)
			m_attackedByRatesPerGamePhase[i].MultiplyValues(factor);
	}

	float GetAttackedByRateUntilEarlyPhase(const AAITargetType& attackerTargetType) const
	{
		static_assert(GamePhase::numberOfGamePhases >= 2, "Number of game phases does not fit to implementation");
		return (m_attackedByRatesPerGamePhase[0].GetValueOfTargetType(attackerTargetType) + m_attackedByRatesPerGamePhase[1].GetValueOfTargetType(attackerTargetType));
	}

private:
	//! Frequency of attacks in a certain game phase
	std::vector< MobileTargetTypeValues > m_attackedByRatesPerGamePhase;
};

//! @brief This class stores the frequency the AI got attacked by a certain combat category (surface, air, floater, submerged) in a certain game phase
class AttackedByRatesPerGamePhaseAndMapType
{
public:
	AttackedByRatesPerGamePhaseAndMapType() 
	{ 
		m_attackedByRatesPerGamePhaseAndMapType.resize(AAIMapType::numberOfMapTypes);
	}

	void SetAttackedByRate(const AAIMapType& mapType, const GamePhase& gamePhase, const AAITargetType& attackerTargetType, float rate)
	{
		m_attackedByRatesPerGamePhaseAndMapType[mapType.GetArrayIndex()].SetAttackedByRate(gamePhase, attackerTargetType, rate);
	}

	float GetAttackedByRate(const AAIMapType& mapType, const GamePhase& gamePhase, const AAITargetType& attackerTargetType) const
	{
		return m_attackedByRatesPerGamePhaseAndMapType[mapType.GetArrayIndex()].GetAttackedByRate(gamePhase, attackerTargetType);
	}

	AttackedByRatesPerGamePhase& GetAttackedByRates(const AAIMapType& mapType) 
	{ 
		return m_attackedByRatesPerGamePhaseAndMapType[mapType.GetArrayIndex()]; 
	}

	float GetAttackedByRateUntilEarlyPhase(const AAIMapType& mapType, const AAITargetType& attackerTargetType) const
	{
		return m_attackedByRatesPerGamePhaseAndMapType[mapType.GetArrayIndex()].GetAttackedByRateUntilEarlyPhase(attackerTargetType);
	}

private:
	//! Frequency of attacks in a certain game phase
	std::vector< AttackedByRatesPerGamePhase > m_attackedByRatesPerGamePhaseAndMapType;
};

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
	float        GetMinValue()   const { return m_minValue; }
	float        GetMaxValue()   const { return m_maxValue; }
	float        GetAvgValue()   const { return m_avgValue; }
	unsigned int GetSampleSize() const { return m_dataPoints; }

	//! @brief Returns the normalized (interval [0:1]) deviation from max value (value must be between min and max)
	float GetNormalizedDeviationFromMax(float value) const
	{
		if(m_valueRange != 0.0f) // range only exactly 0.0f if insufficient number of data points or difference too small
			return (m_maxValue - value) / m_valueRange;
		else
			return 0.0f;
	}

	//! @brief Returns the deviation from max value normalized by [max value:0] -> [0:1]
	float GetDeviationFromMax(float value) const
	{
		if(m_maxValue != 0.0f) // range only exactly 0.0f if insufficient number of data points or difference too small
			return 1.0f - value / m_maxValue; //(m_maxValue - value) / m_maxValue;
		else
			return 0.0f;
	}

	//! @brief Returns the normalized (interval [0:1]) deviation from max value (value must be between min and max)
	float GetNormalizedSquaredDeviationFromMax(float value) const
	{
		if(m_valueRange != 0.0f) // range only exactly 0.0f if insufficient number of data points or difference too small
		{
			const float x = 1.0f - (m_maxValue - value) / m_valueRange;
			return (1.0f - x*x);
		}
		else
			return 0.0f;
	}

	//! @brief Returns the normalized (interval [0:1]) deviation from min value (value must be between min and max)
	float GetNormalizedDeviationFromMin(float value) const
	{
		if(m_valueRange != 0.0f) // range only exactly 0.0f if insufficient number of data points or difference too small
			return (value - m_minValue) / m_valueRange;
		else
			return 0.0f;
	}

	//! @brief Returns the deviation from 0 normalized by [0:max value] -> [0:1]
	float GetDeviationFromZero(float value) const
	{
		if(m_maxValue != 0.0f) // range only exactly 0.0f if insufficient number of data points or difference too small
			return value / m_maxValue;
		else
			return 0.0f;
	}

	//! @brief Returns the normalized (interval [0:1]) deviation from min value (value must be between min and max)
	float GetNormalizedSquaredDeviationFromMin(float value) const
	{
		if(m_valueRange != 0.0f) // range only exactly 0.0f if insufficient number of data points or difference too small
		{
			const float x = 1.0f - (value - m_minValue) / m_valueRange;
			return (1.0f - x*x);
		}
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

class SensorStatistics
{
public:
	void Init(const std::vector<const springLegacyAI::UnitDef*>& unitDefs, const std::vector<UnitTypeProperties>& unitProperties, const std::vector< std::list<UnitDefId> >& unitsInCategory);

	//! Min,max,avg range for static radars
	StatisticalData m_radarRanges;

	//! Min,max,avg range for static sonar detectors
	StatisticalData m_sonarRanges;

	//! Min,max,avg range for static seismic detectors
	StatisticalData m_seismicRanges;

	//! Min,max,avg range for static radars
	StatisticalData m_radarCosts;

	//! Min,max,avg range for static sonar detectors
	StatisticalData m_sonarCosts;

	//! Min,max,avg range for static seismic detectors
	StatisticalData m_seismicCosts;
};

class AAIUnitStatistics
{
public:
	AAIUnitStatistics();

	~AAIUnitStatistics();

	//! Calculates values for given input data
	void Init(const std::vector<const springLegacyAI::UnitDef*>& unitDefs, const std::vector<UnitTypeProperties>& unitProperties, const std::vector< std::list<UnitDefId> >& unitsInCategory, const std::vector< std::list<UnitDefId> >& unitsInCombatCategory);

	const StatisticalData& GetUnitCostStatistics(const AAIUnitCategory& category) const { return m_unitCostStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetUnitBuildtimeStatistics(const AAIUnitCategory& category) const { return m_unitBuildtimeStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetUnitPrimaryAbilityStatistics(const AAIUnitCategory& category) const { return m_unitPrimaryAbilityStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetUnitSecondaryAbilityStatistics(const AAIUnitCategory& category) const { return m_unitSecondaryAbilityStatistics[category.GetArrayIndex()]; }

	const SensorStatistics& GetSensorStatistics() const { return m_sensorStatistics; }
private:
	//! Min,max,avg cost for every unit category
	std::vector<StatisticalData> m_unitCostStatistics;
	
	//! Min,max,avg buildtime for every unit category
	std::vector<StatisticalData> m_unitBuildtimeStatistics;

	//! Min,max,avg:  sight range for scouts, radar/sonar/artillery range, build speed for construction units/factories
	std::vector<StatisticalData> m_unitPrimaryAbilityStatistics;

	//! Min,max,avg:  speed for scouts, mobile constructors, mobile artillery
	std::vector<StatisticalData> m_unitSecondaryAbilityStatistics;

	//! Statistical data for radar, sonar, and seismic sensores
	SensorStatistics             m_sensorStatistics;
};

#endif