// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_BRAIN_H
#define AAI_BRAIN_H

class AAI;
class AAIExecute;
class AIIMap;
class AAISector;

#include "aidef.h"
#include "AAIMapRelatedTypes.h"
#include "AAIUnitStatistics.h"
#include "AAIBuildTable.h"

enum SectorType {UNKNOWN_SECTOR, LAND_SECTOR, LAND_WATER_SECTOR, WATER_SECTOR};

class AAIBrain
{
public:
	AAIBrain(AAI *ai, int maxSectorDistanceToBase);
	~AAIBrain(void);

	void InitAttackedByRates(const AttackedByRatesPerGamePhase& attackedByRates);

	//! @brief Returns the current estimation how much the AAI instance is under pressure by the enemies, values ranging from 0 (min) to 1 (max).
	float GetPressureByEnemy() const { return m_estimatedPressureByEnemies; }	

	float GetAverageMetalSurplus() const { return m_metalSurplus.GetAverageValue(); }

	float GetAveragEnergySurplus() const { return m_energySurplus.GetAverageValue(); }

	float GetBaseFlatLandRatio() const { return m_baseFlatLandRatio; }

	float GetBaseWaterRatio() const { return m_baseWaterRatio; }

	//! @brief Returns the center of the base in map coordinates
	const MapPos& GetCenterOfBase() const { return m_centerOfBase; }

	//! @brief Adds/removes the given sector to the base
	void AssignSectorToBase(AAISector *sector, bool addToBase);

	//! @brief Updates the (smoothened) energy/metal income
	void UpdateRessources(springLegacyAI::IAICallback* cb);

	//! @brief Updates the maximum number of spotted combat units for each category (old max values decrease over time)
	void UpdateMaxCombatUnitsSpotted(const MobileTargetTypeValues& spottedCombatUnits);

	void UpdateAttackedByValues();

	//! @brief Update counters after AAI has been attacked by a certain unit
	void AttackedBy(const AAITargetType& attackerTargetType);

	//! @brief Returns the frequencies of attacks by different combat unit categories in different phases of the game
	const AttackedByRatesPerGamePhase& GetAttackedByRates() const { return s_attackedByRates; }

	// recalculates def capabilities of all units
	void UpdateDefenceCapabilities();

	//! @brief Adds the combat power of the given unit type to the global defence capabilities 
	void AddDefenceCapabilities(UnitDefId unitDefId);

	//! @brief Tries to add a new sectors to base, returns true if successful (may fail because base already reached maximum size or no suitable sectors found)
	bool ExpandBase(SectorType sectorType);

	// returns how much ressources can be spent for unit construction atm
	float Affordable();

	// returns true if commander is allowed for construction at the specified position in the sector
	bool CommanderAllowedForConstructionAt(AAISector *sector, float3 *pos);

	void DefendCommander(int attacker);

	void BuildUnits();

	void UpdatePressureByEnemy();

	//! @brief Returns the frequency of attacks by units of specified combat category
	//!        The value is determined according to to current game phase, data from this game and learned data.
	float GetAttacksBy(const AAITargetType& targetType, const GamePhase& gamePhase) const;

	//! @brief Returns the recent attacks by the given target type
	float GetRecentAttacksBy(const AAITargetType& targetType) const { return m_recentlyAttackedByRates.GetValueOfTargetType(targetType); }

	//! @brief Returns urgency to build power plant
	float GetEnergyUrgency() const;

	//! @brief Returns urgency to build metal extractor
	float GetMetalUrgency() const;

	//! @brief Returns urgency to build energy storage
	float GetEnergyStorageUrgency() const;

	//! @brief Returns urgency to build metal storage
	float GetMetalStorageUrgency() const;

	//! @brief Returns whether construction of unit of given type shall be assisted (taking current resources into account)
	bool SufficientResourcesToAssistsConstructionOf(UnitDefId defId) const;

	//! @brief Determines the construction priority of the given factory
	float DetermineConstructionUrgencyOfFactory(UnitDefId factoryDefId) const;

	//! @brief Determines the selection criteria for a power plant
	PowerPlantSelectionCriteria DeterminePowerPlantSelectionCriteria() const;

	//! A list of sectors with ceratain distance (in number of sectors) to base; 0 = sectors the ai uses to build its base, 1 = direct neighbours etc.
	std::vector< std::list<AAISector*> > m_sectorsInDistToBase;

	//! Holding max number of units of a category spotted at the same time (float as maximum values will slowly decay over time)
	MobileTargetTypeValues m_maxSpottedCombatUnitsOfTargetType;

private:
	//! @brief Recalculates the center of the base (needs to be called after sectors have been added or removed)
	void UpdateCenterOfBase();

	// returns true if sufficient ressources to build unit are availbale
	bool RessourcesForConstr(int unit, int workertime = 175);

	//! @brief Returns the movement type of the next combat unit that shall be ordered
	AAIMovementType DetermineMovementTypeForCombatUnitConstruction(const GamePhase& gamePhase) const;

	//! @brief Selects combat unit according to given criteria and tries to order its construction
	void BuildCombatUnitOfCategory(const AAIMovementType& moveType, const TargetTypeValues& combatPowerCriteria, const UnitSelectionCriteria& unitSelectionCriteria, const std::vector<float>& factoryUtilization, bool urgent);

	//! @brief Determines criteria for combat unit selection based on current game phase (@todo: Take other criteria into account)
	void DetermineCombatUnitSelectionCriteria(UnitSelectionCriteria& unitSelectionCriteria) const;

	//! The combat power of all mobile units against the different target types
	MobileTargetTypeValues m_totalMobileCombatPower;

	//! Ratio of cells with flat land of all base sectors (ranging from 0 (none) to 1(all))
	float m_baseFlatLandRatio;

	//! Ratio of cells with water of all base sectors (ranging from 0 (none) to 1(all))
	float m_baseWaterRatio;

	//! Center of base (mean value of centers of all base sectors) in build map coordinates
	MapPos m_centerOfBase;

	//! Average metal surplus over the last AAIConfig::INCOME_SAMPLE_POINTS frames
	SmoothedData m_metalSurplus;

	//! Average energy surplus over the last AAIConfig::INCOME_SAMPLE_POINTS frames
	SmoothedData m_energySurplus;

	//! Average stored energy over the last AAIConfig::INCOME_SAMPLE_POINTS frames
	SmoothedData m_energyAvailable;

	//! Average metal income over the last AAIConfig::INCOME_SAMPLE_POINTS frames
	SmoothedData m_metalIncome;

	//! Average energy income over the last AAIConfig::INCOME_SAMPLE_POINTS frames
	SmoothedData m_energyIncome;

	//! Counter by what enemy unit category own units/buidlings have been killed (counter is decreasing over time)
	MobileTargetTypeValues m_recentlyAttackedByRates;

	//! Frequency of attacks by different combat categories throughout the gane
	static AttackedByRatesPerGamePhase s_attackedByRates;

	//! Estimation how much the AAI instance is under pressure in the current game situation, values ranging from 0 (min) to 1 (max).
	float m_estimatedPressureByEnemies;	

	AAI *ai;
};

#endif

