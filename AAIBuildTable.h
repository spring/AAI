// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_BUILDTABLE_H
#define AAI_BUILDTABLE_H

class AAI;

#include "aidef.h"
#include "AAIBuildTree.h"
#include "AAIUnitTypes.h"
#include <assert.h>
#include <list>
#include <vector>
#include <string>

//using namespace std;

struct UnitTypeDynamic
{
	int underConstruction;	    //!< how many units of that type are under construction
	int requested;			    //!< how many units of that type have been requested
	int active;				    //!< how many units of that type are currently alive
	int constructorsAvailable;	//!< how many factories/builders available being able to build that unit
	int constructorsRequested;	//!< how many factories/builders requested being able to build that unit
};

//! Criteria used for selection of units
struct UnitSelectionCriteria
{
	float power;      //!< Combat power for combat units; Buildpower for construction units
	float efficiency; //!< Power relative to cost
	float cost;       //!< Unit cost
	float speed;	  //!< Speed of unit
	float range;	  //!< max range for combat units/artillery, los for scouts
	float factoryUtilization; //!< the current utilization of the factories that can construct the unit
};

//! Criteria used for selection of scouts
struct ScoutSelectionCriteria
{
	float cost;       //!< Unit cost
	float speed;	  //!< Speed of unit
	float sightRange; //!< LOS
	float cloakable;  //!< Bonus for cloakable units
};

//! Criteria used for selection of static defences
struct StaticDefenceSelectionCriteria
{
	StaticDefenceSelectionCriteria(const AAITargetType& targetType) : targetType(targetType) {}

	StaticDefenceSelectionCriteria(const AAITargetType& targetType, float combatPower, float range, float cost, float buildtime, float terrain, int randomness) : 
		targetType(targetType), combatPower(combatPower), range(range), cost(cost), buildtime(buildtime), terrain(terrain), randomness(randomness) {}

	AAITargetType targetType; //!< The target type the static defence shall counter
	float combatPower;        //!< Combat power
	float range;	          //!< Maximum range (i.e. range of highest ranged weapon) 
	float cost;               //!< Total cost of static defence
	float buildtime;          //!< Buildtime of static defence
	float terrain;            //!< How important placement on elevated ground (e.g. for long ranged weapons)
	int   randomness;         //!< Randomness applied (starting from 0, random addition to rating of up to randomness * 0.05)
};

//! Criteria used for selection of power plants
struct PowerPlantSelectionCriteria
{
	PowerPlantSelectionCriteria(float cost, float buildtime, float powerProduction, float currentEnergyIncome) 
		: cost(cost), buildtime(buildtime), powerProduction(powerProduction), currentEnergyIncome(currentEnergyIncome) {}

	float cost;            //!< Total cost of power plant
	float buildtime;       //!< Buildtime of power plant
	float powerProduction; //!< Power generation of power plant
	float currentEnergyIncome; //!< The current energy production
};

//! Criteria used for selection of storages
struct StorageSelectionCriteria
{
	StorageSelectionCriteria(float cost, float buildtime, float storedMetal, float storedEnergy) 
		: cost(cost), buildtime(buildtime), storedMetal(storedMetal), storedEnergy(storedEnergy) {}

	float cost;         //!< Total cost of power plant
	float buildtime;    //!< Buildtime of power plant
	float storedMetal;  //!< Storage capacity for metal
	float storedEnergy; //!< Storage capacity for energy
};

//! Criteria used for selection of metal extractors
struct ExtractorSelectionCriteria
{
	ExtractorSelectionCriteria(float cost, float extractedMetal, float armed) : cost(cost), extractedMetal(extractedMetal), armed(armed) {}

	float cost;           //!< Total cost of the extractor
	float extractedMetal; //!< The ammount of extracted metal
	float armed;          //!< Extra rating if extractor is armed
};

//! Data used to calculate rating of factories
class FactoryRatingInputData
{
public:
	FactoryRatingInputData() : factoryDefId(), combatPowerRating(0.0f), canConstructBuilder(false), canConstructScout(false) { }

	UnitDefId   factoryDefId;
	float       combatPowerRating;
	bool        canConstructBuilder;
	bool        canConstructScout;
};

class AAIBuildTable
{
public:
	AAIBuildTable(AAI* ai);
	~AAIBuildTable(void);

	//! @brief Updates the stored combat efficiencies and attack frequencies by enemy target types for the given map type
	void SaveModLearnData(const GamePhase& gamePhase, const AttackedByRatesPerGamePhase& atackedByRates, const AAIMapType& mapType) const;

	//! @brief Updates counters for requested constructors for units that can be built by given construction unit
	void ConstructorRequested(UnitDefId constructor);

	//! @brief Updates counters for available/requested constructors for units that can be built by given construction unit
	void ConstructorFinished(UnitDefId constructor);

	//! @brief Updates counters for available constructors for units that can be built by given construction unit
	void ConstructorKilled(UnitDefId constructor);

	//! @brief Updates counters for requested constructors for units that can be built by given construction unit
	void UnfinishedConstructorKilled(UnitDefId constructor);

	//! @brief Determines the weight factor for every combat unit category based on map type and how often AI had been attacked by 
	//!        this category in the first phase of the game in the past
	void DetermineCombatPowerWeights(MobileTargetTypeValues& combatPowerWeights, const AAIMapType& mapType) const;

	//! @brief Updates counters/buildqueue if a buildorder for a certain factory has been given
	void ConstructionOrderForFactoryGiven(const UnitDefId& factoryDefId)
	{
		units_dynamic[factoryDefId.id].requested -= 1;
		m_factoryBuildqueue.remove(factoryDefId);
	}

	//! @brief Determines a suitable buildqueue to add the given unit (returned queue is invalid if none found)
	Buildqueue DetermineBuildqueue(UnitDefId unitDefId);

	//! @brief Returns the buildqueue for a given constructor
	Buildqueue GetBuildqueueOfFactory(UnitDefId constructorDefId);
	
	//! @brief Returns the list containing which factories shall be built next
	const std::list<UnitDefId>& GetFactoryBuildqueue() const { return m_factoryBuildqueue; }

	//! @brief Calculates the average buildqueue length
	float CalculateAverageBuildqueueLength() const;

	//! @brief Determines the utilization (i.e. how long is the buildqueue) of the different factories
	void DetermineFactoryUtilization(std::vector<float>& factoryUtilization, bool considerOnlyActiveFactoryTypes) const;

	//! @brief Calculates the rating of the given factory
	float DetermineFactoryRating(UnitDefId factoryDefId, const TargetTypeValues& combatPowerVsTargetType) const;

	//! @brief Returns the attackedByRates read from the mod learning file upon initialization
	const AttackedByRatesPerGamePhase& GetAttackedByRates(const AAIMapType& mapType) const { return s_attackedByRates.GetAttackedByRates(mapType); }

	//! @brief Indicates that construction of unit has started
	void ConstructionStarted(UnitDefId unitDefId)
	{ 
		units_dynamic[unitDefId.id].requested -= 1;
		units_dynamic[unitDefId.id].underConstruction += 1;
	}

	//! @brief Indicates that construction of unit has finished
	void ConstructionFinished(UnitDefId unitDefId)
	{ 
		units_dynamic[unitDefId.id].underConstruction -= 1;
		units_dynamic[unitDefId.id].active += 1;
	}

	//! @brief Returns the future number (under construction and requested) of units of the given type
	int GetNumberOfFutureUnits(UnitDefId unitDefId) const { return (units_dynamic[unitDefId.id].underConstruction + units_dynamic[unitDefId.id].requested); }

	//! @brief Returns the total number (active, under construction, and requested) of units of the given type
	int GetTotalNumberOfUnits(UnitDefId unitDefId) const { return (units_dynamic[unitDefId.id].active + units_dynamic[unitDefId.id].underConstruction + units_dynamic[unitDefId.id].requested); }

	//! @brief Returns the total number (available and requested) of constructors for the given unit type
	int GetTotalNumberOfConstructorsForUnit(UnitDefId unitDefId) const { return (units_dynamic[unitDefId.id].constructorsAvailable + units_dynamic[unitDefId.id].constructorsRequested); }

	//! @brief Returns the number of available constructors for the given unit type
	int GetNumberOfAvailableConstructorsForUnit(UnitDefId unitDefId) const { return units_dynamic[unitDefId.id].constructorsAvailable; }

	// ******************************************************************************************************
	// the following functions are used to determine units that suit a certain purpose
	// if water == true, only water based units/buildings will be returned
	// randomness == 1 means no randomness at all; never set randomnes to zero -> crash
	// ******************************************************************************************************
	//! @brief Selects a power plant according to given criteria; a builder is requested if none available and a different power plant is chosen.
	UnitDefId SelectPowerPlant(int side, const PowerPlantSelectionCriteria& selectionCriteria, bool water);

	//! @brief Selects a metal extractor according to given criteria; a builder is requested if none available and a different extractor is chosen.
	UnitDefId SelectExtractor(int side, const ExtractorSelectionCriteria& selectionCriteria, bool water);

	//! @brief Selects a radar according to given criteria; a builder is requested if none available and a different radar is chosen.
	UnitDefId SelectRadar(int side, float cost, float range, bool water);

	//! @brief Selects a static defence according to given criteria; a builder is requested if none available and a different static defence is chosen.
	UnitDefId SelectStaticDefence(int side, const StaticDefenceSelectionCriteria& selectionCriteria, bool water);

	//! @brief Selects a nano turret
	UnitDefId SelectNanoTurret(int side, bool water) const;

	//! @brief Selects a metal maker - currently not implemented (returns no valid unit def id)
	UnitDefId GetMetalMaker(int side, float cost, float efficiency, float metal, float urgency, bool water, bool canBuild) const;

	//! @brief Selects a storage according to given criteria; a builder is requested if none available and a different storage is chosen.
	UnitDefId SelectStorage(int side, const StorageSelectionCriteria& selectionCriteria, bool water);

	// return repair pad
	int GetAirBase(int side, float cost, bool water, bool canBuild);

	//! @brief Selects a combat unit of specified movement type according to given criteria
	UnitDefId SelectCombatUnit(int side, const AAIMovementType& moveType, const TargetTypeValues& combatPowerCriteria, const UnitSelectionCriteria& unitCriteria, const std::vector<float>& factoryUtilization, int randomness, bool constructorAvailable) const;

	//! @brief Selects a static artillery according to given criteria
	UnitDefId SelectStaticArtillery(int side, float cost, float range, bool water) const;

	//! @brief Determines a scout unit with given properties
	UnitDefId SelectScout(int side, const ScoutSelectionCriteria& scoutSelectionCriteria, uint32_t movementType, bool factoryAvailable);

	int GetJammer(int side, float cost, float range, bool water, bool canBuild);

	//! @brief Determines most suitable factory to construct given unit and requests its construnction (returns the requested type)
	UnitDefId RequestFactoryFor(UnitDefId unitDefId);

	//! @brief Looks for most suitable construction unit for given building and places buildorder if such a unit is not already under construction/requested
	void RequestBuilderFor(UnitDefId building);

	// @brief Tries to build an assistant for the specified kind of unit
	//void AddAssistant(uint32_t allowedMovementTypes, bool mustBeConstructable);

	//! @brief Returns the dynamic unit type data for the given unitDefId
	const UnitTypeDynamic& GetDynamicUnitTypeData(UnitDefId unitDefId) const { return units_dynamic[unitDefId.id]; }

	// AAI unit defs with aai-instance specific information (number of requested, active units, etc.)
	std::vector<UnitTypeDynamic> units_dynamic;

	const springLegacyAI::UnitDef& GetUnitDef(int i) const { return *unitList[i]; };

private:
	std::string GetBuildCacheFileName() const;

	//! @brief Loads mod learn data from file
	bool LoadModLearnData();

	//! @brief Helper function used for building selection
	bool IsBuildingSelectable(UnitDefId building, bool water, bool mustBeConstructable) const;

	//! @brief Adds combat units matching the given criteria to the list
	void SelectCombatUnits(std::list<UnitDefId>& unitList, int side, const AAIMovementType& allowedMoveTypes, bool constructorAvailable) const;

	//! @brief Returns a power plant based on the given criteria
	UnitDefId SelectPowerPlant(int side, const PowerPlantSelectionCriteria& selectionCriteria, bool water, bool mustBeConstructable) const;

	//! @brief Returns an extractor based on the given criteria
	UnitDefId SelectExtractor(int side, const ExtractorSelectionCriteria& selectionCriteria, bool water, bool canBuild) const;

	//! @brief Returns a radar according to given criteria
	UnitDefId SelectRadar(int side, float cost, float range, bool water, bool canBuild) const;

	//! @brief Selects a defence building according to given criteria
	UnitDefId SelectStaticDefence(int side, const StaticDefenceSelectionCriteria& selectionCriteria, bool water, bool constructable) const;

	//! @brief Selects a storage according to given criteria
	UnitDefId SelectStorage(int side, const StorageSelectionCriteria& selectionCriteria, bool water, bool mustBeConstructable) const;

	//! @brief Determines the most suitable constructor for the given unit (mobile unit or building)
	UnitDefId SelectConstructorFor(UnitDefId unitDefId) const;

	//! @brief Order construction of the given construction unit (factory or builder); returns whether construction unit has been added to buildqueue of a factory
	bool RequestMobileConstructor(UnitDefId constructor);

	//! @brief Calculates the rating of the given factory for the given map type
	void CalculateFactoryRating(FactoryRatingInputData& ratingData, const UnitDefId factoryDefId, const MobileTargetTypeValues& combatPowerWeights, const AAIMapType& mapType) const;

	//! A list containing the next factories that shall be built
	std::list<UnitDefId> m_factoryBuildqueue;

	//! Buildqueues of the different factories
	std::vector< std::list<UnitDefId> > m_buildqueues;

	//! Rates of attacks by different combat categories per map and game phase
	static AttackedByRatesPerGamePhaseAndMapType s_attackedByRates;

	AAI *ai;

	// all the unit defs, FIXME: this can't be made static as spring seems to free the memory returned by GetUnitDefList()
	std::vector<const springLegacyAI::UnitDef*> unitList;
};

#endif

