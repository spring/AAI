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

namespace springLegacyAI {
	struct UnitDef;
}
using namespace springLegacyAI;


#include "aidef.h"
#include "AAIBuildTree.h"
#include <assert.h>
#include <list>
#include <vector>
#include <string>

using namespace std;

struct UnitTypeDynamic
{
	int under_construction;	// how many units of that type are under construction
	int requested;			// how many units of that type have been requested
	int active;				// how many units of that type are currently alive
	int constructorsAvailable;	// how many factories/builders available being able to build that unit
	int constructorsRequested;	// how many factories/builders requested being able to build that unit
};

struct UnitTypeStatic
{
	vector<float> efficiency;		// 0 -> ground assault, 1 -> air assault, 2 -> hover assault
									// 3 -> sea assault, 4 -> submarine , 5 -> stat. defences
};

//! Criteria used for selection of units
struct UnitSelectionCriteria
{
	float power;      //! Combat power for combat units; Buildpower for construction units 
	float efficiency; //! Power relative to cost
	float cost;       //! Unit cost
	float speed;	  //! Speed of unit
	float range;	  //! max range for combat units/artillery, los for scouts
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

	// call before you want to use the buildtable
	// loads everything from a cache file or creates a new one
	void Init();

	void SaveBuildTable(const GamePhase& gamePhase, const MapType& mapType);

	// cache for combat eff (needs side, thus initialized later)
	void InitCombatEffCache(int side);

	//! @brief Determines the unit type
	AAIUnitType GetUnitType(UnitDefId unitDefId) const;

	//! @brief Updates counters for requested constructors for units that can be built by given construction unit
	void ConstructorRequested(UnitDefId constructor);

	//! @brief Updates counters for available/requested constructors for units that can be built by given construction unit
	void ConstructorFinished(UnitDefId constructor);

	//! @brief Updates counters for available constructors for units that can be built by given construction unit
	void ConstructorKilled(UnitDefId constructor);

	//! @brief Updates counters for requested constructors for units that can be built by given construction unit
	void UnfinishedConstructorKilled(UnitDefId constructor);

	//! @brief Determines the first type of factory to be built and orders its contsruction (selected factory is returned)
	UnitDefId RequestInitialFactory(int side, MapType mapType);

	//! @brief Determines the weight factor for every combat unit category based on map type and how often AI had been attacked by 
	//!        this category in the first phase of the game in the past
	void DetermineCombatPowerWeights(CombatPower& combatPowerWeights, const MapType mapType) const;

	//! @brief Updates counters/buildqueue if a buildorder for a certain factory has been given
	void ConstructionOrderForFactoryGiven(const UnitDefId& factoryDefId)
	{
		units_dynamic[factoryDefId.id].requested -= 1;
		m_factoryBuildqueue.remove(factoryDefId);
	}
	
	//! @brief Returns the list containing which factories shall be built next
	const std::list<UnitDefId>& GetFactoryBuildqueue() const { return m_factoryBuildqueue; }

	// ******************************************************************************************************
	// the following functions are used to determine units that suit a certain purpose
	// if water == true, only water based units/buildings will be returned
	// randomness == 1 means no randomness at all; never set randomnes to zero -> crash
	// ******************************************************************************************************
	//! @brief Selects a power plant according to given criteria; a builder is requested if none available and a different power plant is chosen.
	UnitDefId SelectPowerPlant(int side, float cost, float buildtime, float powerGeneration, bool water);

	//! @brief Selects a metal extractor according to given criteria; a builder is requested if none available and a different extractor is chosen.
	UnitDefId SelectExtractor(int side, float cost, float extractedMetal, bool armed, bool water);

	//! @brief Selects a radar according to given criteria; a builder is requested if none available and a different radar is chosen.
	UnitDefId SelectRadar(int side, float cost, float range, bool water);

	//! @brief Selects a static defence according to given criteria; a builder is requested if none available and a different static defence is chosen.
	UnitDefId SelectStaticDefence(int side, float cost, float buildtime, float combatPower, const CombatPower& combatCriteria, float range, int randomness, bool water);

	// returns a metal maker
	int GetMetalMaker(int side, float cost, float efficiency, float metal, float urgency, bool water, bool canBuild);

	//! @brief Selects a storage according to given criteria; a builder is requested if none available and a different storage is chosen.
	UnitDefId SelectStorage(int side, float cost, float buildtime, float metal, float energy, bool water);

	// return repair pad
	int GetAirBase(int side, float cost, bool water, bool canBuild);

	//! @brief Selects a combat unit of specified category according to given criteria
	UnitDefId SelectCombatUnit(int side, const AAICombatCategory& category, const CombatPower& combatCriteria, const UnitSelectionCriteria& unitCriteria, int randomness, bool canBuild);

	//! @brief Selects a static artillery according to given criteria
	UnitDefId SelectStaticArtillery(int side, float cost, float range, bool water) const;

	//! @brief Determines a scout unit with given properties
	UnitDefId selectScout(int side, float sightRange, float cost, uint32_t movementType, int randomness, bool cloakable, bool factoryAvailable);

	int GetJammer(int side, float cost, float range, bool water, bool canBuild);

	// checks which factory is needed for a specific unit and orders it to be built
	void BuildFactoryFor(int unit_def_id);

	//! @brief Looks for most suitable construction unit for given building and places buildorder if such a unit is not already under construction/requested
	void BuildBuilderFor(UnitDefId building, float cost = 1.0f, float buildtime = 0.5f, float buildpower = 0.75f, float constructableBuilderBonus = 0.25f);

	// @brief Tries to build an assistant for the specified kind of unit
	//void AddAssistant(uint32_t allowedMovementTypes, bool mustBeConstructable);

	// updates unit table
	void UpdateTable(const UnitDef* def_killer, int killer, const UnitDef *def_killed, int killed);

	// updates max and average eff. values of the different categories
	void UpdateMinMaxAvgEfficiency();

	//! @brief Returns metal extractor with the largest yardmap
	UnitDefId GetLargestExtractor() const;

	// returns true, if unit is arty
	bool IsArty(int id);

	// returns true, if unit is a scout
	bool IsScout(int id);

	// returns true if the unit is marked as attacker (so that it won't be classed as something else even if it can build etc.)
	bool IsAttacker(int id);

	bool IsMissileLauncher(int def_id);

	bool IsDeflectionShieldEmitter(int def_id);

	// returns false if unit is a member of the dont_build list
	bool AllowedToBuild(int id);

	//sadly can't detect metal makers anymore, read them from config
	bool IsMetalMaker(int id);

	// returns true, if unit is a transporter
	bool IsTransporter(int id);

	//! @brief Returns target type id of given unit category
	int GetIDOfAssaultCategory(const AAIUnitCategory& category) const;

	//! @brief Returns the unit category for a given index (0 to 5) of an combat unit type (ground, air, hover, sea, submarine, static)
	AAIUnitCategory GetUnitCategoryOfCombatUnitIndex(int index) const;

	//
	// these data are shared by several instances of aai
	//

	// number of assault categories
	static const int ass_categories = 5;

	// number of assault cat + arty & stat defences
	static const int combat_categories = 6;

	// combat categories that attacked AI in certain game period attacked_by_category_learned[map_type][period][cat]
	static vector< vector< vector<float> > > attacked_by_category_learned;

	// combat categories that attacked AI in certain game period attacked_by_category_current[period][cat]
	static vector< vector<float> > attacked_by_category_current;

	// AAI unit defs (static things like id, side, etc.)
	static vector<UnitTypeStatic> units_static;

	// cached combat efficiencies
	static vector< vector< vector<float> > > avg_eff;
	static vector< vector< vector<float> > > max_eff;
	static vector< vector< vector<float> > > min_eff;
	static vector< vector< vector<float> > > total_eff;

	// stores the combat eff. of units at the beginning of the game. due to learning these values will change during the game
	// however for some purposes its necessary to have constant values (e.g. adding and subtracting stationary defences to/from the defense map)
	static vector< vector<float> > fixed_eff;

	//! The buildtree (who builds what, which unit belongs to which side, ...)
	static AAIBuildTree s_buildTree;

	//
	//	non static variales
	//

	// number of sides
	int numOfSides;

	// side names
	vector<string> sideNames;

	vector<float> combat_eff;

	// true if initialized correctly
	bool initialized;

	// AAI unit defs with aai-instance specific information (number of requested, active units, etc.)
	vector<UnitTypeDynamic> units_dynamic;

	// all assault unit categories
	std::list<UnitCategory> assault_categories;

	const UnitDef& GetUnitDef(int i) const { assert(IsValidUnitDefID(i));	return *unitList[i]; };
	bool IsValidUnitDefID(int i) { return (i>=0) && (i<=unitList.size()); }

private:
	std::string GetBuildCacheFileName();
	// precaches speed/cost/buildtime/range stats
	void PrecacheStats();

	bool LoadBuildTable();

	//! @brief Helper function used for building selection
	bool IsBuildingSelectable(UnitDefId building, bool water, bool mustBeConstructable) const;

	//! @brief Returns a power plant based on the given criteria
	UnitDefId SelectPowerPlant(int side, float cost, float buildtime, float powerGeneration, bool water, bool mustBeConstructable) const;

	//! @brief Returns an extractor based on the given criteria
	UnitDefId SelectExtractor(int side, float cost, float extractedMetal, bool armed, bool water, bool canBuild) const;

	//! @brief Returns a radar according to given criteria
	UnitDefId SelectRadar(int side, float cost, float range, bool water, bool canBuild) const;

	//! @brief Selects a defence building according to given criteria
	UnitDefId SelectStaticDefence(int side, float cost, float buildtime, float combatPower, const CombatPower& combatCriteria, float range, int randomness, bool water, bool constructable) const;

	//! @brief Selects a storage according to given criteria
	UnitDefId SelectStorage(int side, float cost, float buildtime, float metal, float energy, bool water, bool mustBeConstructable) const;

	//! @brief Calculates the rating of the given factory for the given map type
	void CalculateFactoryRating(FactoryRatingInputData& ratingData, const UnitDefId factoryDefId, const CombatPower& combatPowerWeights, const MapType mapType) const;

	//! @brief Calculates the combat statistics needed for unit selection
	void CalculateCombatPowerForUnits(const std::list<int>& unitList, const AAICombatCategory& category, const CombatPower& combatCriteria, std::vector<float>& combatPowerValues, StatisticalData& combatPowerStat, StatisticalData& combatEfficiencyStat);

	//! A list containing the next factories that shall be built
	std::list<UnitDefId> m_factoryBuildqueue;

	AAI *ai;

	// all the unit defs, FIXME: this can't be made static as spring seems to free the memory returned by GetUnitDefList()
	std::vector<const UnitDef*> unitList;
};

#endif

