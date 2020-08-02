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

//! @brief This class stores the frequency the AI got attacked by a certain combat category (surface, air, floater, submerged)
class AttackedByRates
{
public:
	AttackedByRates() 
	{ 
		m_attackedByRates.resize(GamePhase::numberOfGamePhases, 0.0f);
	}

	void AddAttack(const AAICombatCategory& attackerCategory)
	{
		m_attackedByRates[attackerCategory.GetArrayIndex()] += 1.0f;
	}

	void SetAttackedByRate(const AAICombatCategory& attackerCategory, float rate)
	{
		m_attackedByRates[attackerCategory.GetArrayIndex()] = rate;
	}

	float GetAttackedByRate(const AAICombatCategory& attackerCategory) const
	{
		return m_attackedByRates[attackerCategory.GetArrayIndex()];
	}

	void DecreaseByFactor(float factor)
	{
		for(int i = 0; i < m_attackedByRates.size(); ++i)
			m_attackedByRates[i] *= factor;
	}

private:
	//! Frequency of attacks
	std::vector<float> m_attackedByRates;
};

//! @brief This class stores the frequency the AI got attacked by a certain combat category (surface, air, floater, submerged) in a certain game phase
class AttackedByRatesPerGamePhase
{
public:
	AttackedByRatesPerGamePhase() 
	{ 
		m_attackedByRatesPerGamePhase.resize(GamePhase::numberOfGamePhases);
	}

	void AddAttack(const GamePhase& gamePhase, const AAICombatCategory& attackerCategory)
	{
		m_attackedByRatesPerGamePhase[gamePhase.GetArrayIndex()].AddAttack(attackerCategory);
	}

	void SetAttackedByRate(const GamePhase& gamePhase, const AAICombatCategory& attackerCategory, float rate)
	{
		m_attackedByRatesPerGamePhase[gamePhase.GetArrayIndex()].SetAttackedByRate(attackerCategory, rate);
	}

	float GetAttackedByRate(const GamePhase& gamePhase, const AAICombatCategory& attackerCategory) const
	{
		return m_attackedByRatesPerGamePhase[gamePhase.GetArrayIndex()].GetAttackedByRate(attackerCategory);
	}

	void DecreaseByFactor(const GamePhase& updateUntilGamePhase, float factor)
	{
		for(int i = 0; i <= updateUntilGamePhase.GetArrayIndex(); ++i)
			m_attackedByRatesPerGamePhase[i].DecreaseByFactor(factor);
	}

private:
	//! Frequency of attacks in a certain game phase
	std::vector< AttackedByRates > m_attackedByRatesPerGamePhase;
};

//! @brief This class stores the frequency the AI got attacked by a certain combat category (surface, air, floater, submerged) in a certain game phase
class AttackedByRatesPerGamePhaseAndMapType
{
public:
	AttackedByRatesPerGamePhaseAndMapType() 
	{ 
		m_attackedByRatesPerGamePhaseAndMapType.resize(AAIMapType::numberOfMapTypes);
	}

	void SetAttackedByRate(const AAIMapType& mapType, const GamePhase& gamePhase, const AAICombatCategory& attackerCategory, float rate)
	{
		m_attackedByRatesPerGamePhaseAndMapType[mapType.GetArrayIndex()].SetAttackedByRate(gamePhase, attackerCategory, rate);
	}

	float GetAttackedByRate(const AAIMapType& mapType, const GamePhase& gamePhase, const AAICombatCategory& attackerCategory) const
	{
		return m_attackedByRatesPerGamePhaseAndMapType[mapType.GetArrayIndex()].GetAttackedByRate(gamePhase, attackerCategory);
	}

	AttackedByRatesPerGamePhase& GetAttackedByRates(const AAIMapType& mapType) 
	{ 
		return m_attackedByRatesPerGamePhaseAndMapType[mapType.GetArrayIndex()]; 
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
	void Init(const std::vector<const springLegacyAI::UnitDef*>& unitDefs, const std::vector<UnitTypeProperties>& unitProperties, const std::vector< std::list<UnitDefId> >& unitsInCategory, const std::vector< std::list<int> >& unitsInCombatCategory);

	const StatisticalData& GetUnitCostStatistics(const AAIUnitCategory& category) const { return m_unitCostStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetUnitBuildtimeStatistics(const AAIUnitCategory& category) const { return m_unitBuildtimeStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetUnitPrimaryAbilityStatistics(const AAIUnitCategory& category) const { return m_unitPrimaryAbilityStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetUnitSecondaryAbilityStatistics(const AAIUnitCategory& category) const { return m_unitSecondaryAbilityStatistics[category.GetArrayIndex()]; }
	
	const StatisticalData& GetCombatCostStatistics(const AAICombatCategory& category) const { return m_combatCostStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetCombatBuildtimeStatistics(const AAICombatCategory& category) const { return m_combatBuildtimeStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetCombatRangeStatistics(const AAICombatCategory& category) const { return m_combatRangeStatistics[category.GetArrayIndex()]; }

	const StatisticalData& GetCombatSpeedStatistics(const AAICombatCategory& category) const { return m_combatSpeedStatistics[category.GetArrayIndex()]; }

	const SensorStatistics& GetSensorStatistics() const { return m_sensorStatistics; }
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

	//! Statistical data for radar, sonar, and seismic sensores
	SensorStatistics             m_sensorStatistics;
};

#endif