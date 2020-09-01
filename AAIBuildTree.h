// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_BUILDTREE_H
#define AAI_BUILDTREE_H

#include "AAIUnitTypes.h"
#include "AAITypes.h"
#include "AAIUnitStatistics.h"
#include "LegacyCpp/IAICallback.h"

#include <stdio.h>
#include <list>
#include <vector>

//! @brief This class stores the build-tree, this includes which unit builds another, to which side each unit belongs
class AAIBuildTree
{
public:
	AAIBuildTree();

	~AAIBuildTree(void);

	//! @brief Generates buildtree for current game/mod
	bool Generate(springLegacyAI::IAICallback* cb);

	//! @brief Saves the combat power of units to given 
	void SaveCombatPowerOfUnits(FILE* saveFile) const;

	//! @brief Initializes the combat power of units and invokes update of the unit types (returns true if successful)
	bool LoadCombatPowerOfUnits(FILE* inputFile);

	//! @brief Initializes the combat power (called if no saved data from previous games available)
	void InitCombatPowerOfUnits(springLegacyAI::IAICallback* cb);

	//! @brief Updates combat power statistics when a unit kills another
	void UpdateCombatPowerStatistics(UnitDefId attackerDefId, UnitDefId killedUnitDefId);

	//! @brief Prints summary of newly created buildtree
	void PrintSummaryToFile(const std::string& filename, springLegacyAI::IAICallback* cb) const;

	//! @brief Returns whether given the given unit type can be constructed by the given constructor unit type
	bool CanBuildUnitType(UnitDefId unitDefIdBuilder, UnitDefId unitDefId) const;

	//! @brief Return side of given unit type (0 if not initialized)
	int GetSideOfUnitType(UnitDefId unitDefId) const { return m_initialized ? m_sideOfUnitType[unitDefId.id] : 0; }

	//! @brief Returns the list of units that can construct the given unit.
	const std::list<UnitDefId>& GetConstructedByList(UnitDefId unitDefId) const { return m_unitTypeCanBeConstructedtByLists[unitDefId.id]; }

	//! @brief Returns the list of units that can be construct by the given unit.
	const std::list<UnitDefId>& GetCanConstructList(UnitDefId unitDefId) const { return m_unitTypeCanConstructLists[unitDefId.id]; }

	//! @brief Returns the number of sides
	int GetNumberOfSides() const { return m_numberOfSides; }

	//! @brief Returns whether a given unit type is a starting unit for one side
	bool IsStartingUnit(UnitDefId unitDefId) const;

	//! @brief Returns start units (probably not needed anymore when refactoring AAIBuildTable is finished)
	UnitDefId GetStartUnit(int side) const { return m_initialized ? UnitDefId(m_startUnitsOfSide[side]) : UnitDefId(0); }

	//! @brief Returns the unit type properties of the given unit type
	const UnitTypeProperties& GetUnitTypeProperties(UnitDefId unitDefId) const { return m_unitTypeProperties[unitDefId.id]; }

	//! @brief Return the total cost of the given unit type
	const float GetTotalCost(UnitDefId unitDefId) const { return m_unitTypeProperties[unitDefId.id].m_totalCost; }

	//! @brief Return the buildtime of the given unit type
	const float GetBuildtime(UnitDefId unitDefId) const { return m_unitTypeProperties[unitDefId.id].m_buildtime; }

	//! @brief Returns the primary ability (equal to maximum weapons range for combat units)
	const float GetMaxRange(UnitDefId unitDefId) const { return m_unitTypeProperties[unitDefId.id].m_primaryAbility; }
	const float GetPrimaryAbility(UnitDefId unitDefId) const { return m_unitTypeProperties[unitDefId.id].m_primaryAbility; }

	//! @brief Returns the buildspeed for static and mobile constructors, range otherwise (buildspeed is stored in range variable)
	const float GetBuildspeed(UnitDefId unitDefId) const { return m_unitTypeProperties[unitDefId.id].m_primaryAbility; }

	//! @brief Returns the footprint of the given unit, i.e. number of map tiles occupied in horizontal/vertical direction
	const UnitFootprint& GetFootprint(UnitDefId unitDefId) const { return m_unitTypeProperties[unitDefId.id].m_footprint; }

	//! @brief Returns the category that the given unit belongs to
	const AAIUnitCategory& GetUnitCategory(UnitDefId unitDefId) const { return m_unitTypeProperties[unitDefId.id].m_unitCategory; }

	//! @brief Returns movement type of given unit type
	const AAIMovementType& GetMovementType(UnitDefId unitDefId) const  { return m_unitTypeProperties[unitDefId.id].m_movementType; }

	//! @brief Returns the unit type
	const AAIUnitType& GetUnitType(UnitDefId unitDefId)         const  { return m_unitTypeProperties[unitDefId.id].m_unitType; }

	//! @brief Returns the target type
	const AAITargetType& GetTargetType(UnitDefId unitDefId)     const  { return m_unitTypeProperties[unitDefId.id].m_targetType; }

	//! @brief Return the maximum speed
	const float GetMaxSpeed(UnitDefId unitDefId) const { return m_unitTypeProperties[unitDefId.id].m_maxSpeed; }

	const AAICombatPower& GetCombatPower(UnitDefId unitDefId) const { return m_combatPowerOfUnits[unitDefId.id]; }

	//! @brief Returns the list of units of the given category for given side
	const std::list<UnitDefId>& GetUnitsInCategory(const AAIUnitCategory& category, int side) const { return m_unitsInCategory[side-1][category.GetArrayIndex()]; }

	//! @brief Returns the list of units of the given combat category for given side
	const std::list<int>& GetCombatUnitsOfTargetType(const AAITargetType& targetType, int side) const { return m_unitsInCombatCategory[side-1][targetType.GetArrayIndex()]; }

	//! @brief Returns the unit category statistics for given side
	const AAIUnitStatistics& GetUnitStatistics(int side) const { return m_unitCategoryStatisticsOfSide[side-1]; }

	//! @brief Returns the corresponding human readable name of the given category
	const std::string& GetCategoryName(const AAIUnitCategory& category) const { return m_unitCategoryNames[category.GetArrayIndex()]; }

	//! @brief Returns a list containing all unit categories of combat units
	const std::list<AAIUnitCategory>& GetCombatUnitCatgegories() const { return m_combatUnitCategories; }

private:
	//! @brief Sets side for given unit type, and recursively calls itself for all unit types that can be constructed by it.
	void AssignSideToUnitType(int side, UnitDefId unitDefId);

	//! @brief 	Returns the primary ability (weapon range for combat units, artillery, or static defences, los for scout, radar(jammer) range, 
	//!         buildtime for constructors, metal extraction for extractors, metal storage capacity for storages), generated power for power plants
	float DeterminePrimaryAbility(const springLegacyAI::UnitDef* unitDef, const AAIUnitCategory& unitCategory, springLegacyAI::IAICallback* cb) const;
	
	//! @brief 	Returns the secondary ability (movement speed for combat units, artillery, scouts, or mobile constructors, 
	//!         sonar(jammer) range, energy storage capacity for storages)
	float DetermineSecondaryAbility(const springLegacyAI::UnitDef* unitDef, const AAIUnitCategory& unitCategory) const;
	
	//! @brief Returns movement type of given unit definition
	EMovementType DetermineMovementType(const springLegacyAI::UnitDef* unitDef) const;

	//! @brief Returns target type of given movement type
	ETargetType DetermineTargetType(const AAIMovementType& moveType) const;

	//! @brief Returns Unit Category for given unit definition
	EUnitCategory DetermineUnitCategory(const springLegacyAI::UnitDef* unitDef) const;

	//! @brief Determines and sets the unit types for the given unit.
	void UpdateUnitTypes(UnitDefId unitDefId, const springLegacyAI::UnitDef* unitDef);

	//! @brief Determines the unit type of combat units (called after combat power has been loaded/initialized)
	void UpdateUnitTypesOfCombatUnits();

	//! @brief Calculates the value for the update of the combar power of the given attacker and killed unit type
	float CalculateCombatPowerChange(UnitDefId attackerUnitDefId, UnitDefId killedUnitDefId) const;

	//-----------------------------------------------------------------------------------------------------------------
	// helper functions for determineUnitCategory(...)
	//-----------------------------------------------------------------------------------------------------------------
	bool IsScout(const springLegacyAI::UnitDef* unitDef) const;
	bool IsMobileTransport(const springLegacyAI::UnitDef* unitDef) const;
	bool IsArtillery(const springLegacyAI::UnitDef* unitDef, float artilleryRangeThreshold) const;
	bool IsMissileLauncher(const springLegacyAI::UnitDef* unitDef) const;
	bool IsDeflectionShieldEmitter(const springLegacyAI::UnitDef* unitDef) const;
    float GetMaxDamage(const springLegacyAI::UnitDef* unitDef) const;

	//-----------------------------------------------------------------------------------------------------------------
	// member variables
	//-----------------------------------------------------------------------------------------------------------------

	//! Flag if build tree is initialized
	bool                                          m_initialized;

	//! For every unit type, a list of unit types (unit type id) that may contsruct it 
	std::vector< std::list<UnitDefId> >           m_unitTypeCanBeConstructedtByLists;

	//! For every unit type, a list of unit types (unit type id) that it may contsruct (e.g. empty if it cannot construct any units) 
	std::vector< std::list<UnitDefId> >           m_unitTypeCanConstructLists;

	//! Properties of every unit type needed by other parts of AAI for decision making
	std::vector< UnitTypeProperties >             m_unitTypeProperties;

	//! For every unit type, the side/faction it belongs to (0 if no side)
	std::vector< int >                            m_sideOfUnitType;

	//! For every side, the start unit, i.e. root of the buildtree (commander for original TA like mods)
	std::vector< int >                            m_startUnitsOfSide;

	//! The number of sides (i.e. groups of units with disjunct buildtree)
	int                                           m_numberOfSides;

	//! For every side (not neutral), a list of units that belong to a certain category (order: m_unitsInCategory[side][category])
	std::vector< std::vector< std::list<UnitDefId> > >  m_unitsInCategory;

	//! For every side (not neutral), a list of units that belong to a certain combat category (order: m_unitsInCombatCategory[side][category])
	std::vector< std::vector< std::list<int> > >  m_unitsInCombatCategory;

	//! A list containing all catgeories of combat units
	std::list< AAIUnitCategory >                  m_combatUnitCategories;

	//! For every side, min/max/avg values for various data (e.g. cost) for every unit category
	std::vector< AAIUnitStatistics >              m_unitCategoryStatisticsOfSide;

	//! For each unit category, a human readable description of it
	std::vector< std::string >                    m_unitCategoryNames;

	//! The combat power of every unit
	std::vector<AAICombatPower>                   m_combatPowerOfUnits;
};

#endif