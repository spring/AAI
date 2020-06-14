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
	int def_id;
	int side;				// 0 if side has not been set
	vector<float> efficiency;		// 0 -> ground assault, 1 -> air assault, 2 -> hover assault
									// 3 -> sea assault, 4 -> submarine , 5 -> stat. defences
	float cost;
	UnitCategory category;

	unsigned int unit_type;
};

//! Criteria (combat efficiency vs specific kind of target type) used for selection of units
struct CombatVsCriteria
{
	float efficiencyVsGround;
	float efficiencyVsAir;
	float efficiencyVsHover;
	float efficiencyVsSea;
	float efficiencyVsSubmarine;
	float efficiencyVSBuildings;
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


class AAIBuildTable
{
public:
	AAIBuildTable(AAI* ai);
	~AAIBuildTable(void);

	// call before you want to use the buildtable
	// loads everything from a cache file or creates a new one
	void Init();

	void SaveBuildTable(int game_period, MapType map_type);

	// cache for combat eff (needs side, thus initialized later)
	void InitCombatEffCache(int side);

	// return unit type (for groups)
	UnitType GetUnitType(int def_id);

	//! @brief Updates counters for requested constructors for units that can be built by given construction unit
	void ConstructorRequested(UnitDefId constructor);

	//! @brief Updates counters for available/requested constructors for units that can be built by given construction unit
	void ConstructorFinished(UnitDefId constructor);

	//! @brief Updates counters for available constructors for units that can be built by given construction unit
	void ConstructorKilled(UnitDefId constructor);

	//! @brief Updates counters for requested constructors for units that can be built by given construction unit
	void UnfinishedConstructorKilled(UnitDefId constructor);

	// ******************************************************************************************************
	// the following functions are used to determine units that suit a certain purpose
	// if water == true, only water based units/buildings will be returned
	// randomness == 1 means no randomness at all; never set randomnes to zero -> crash
	// ******************************************************************************************************
	// returns power plant
	int GetPowerPlant(int side, float cost, float urgency, float max_power, float current_energy, bool water, bool geo, bool canBuild);

	// returns a extractor from the list based on certain factors
	int GetMex(int side, float cost, float effiency, bool armed, bool water, bool canBuild);

	// returns mex with the biggest yardmap
	int GetBiggestMex();

	// return defence buildings to counter a certain category
	int DetermineStaticDefence(int side, double efficiency, double combat_power, double cost, double ground_eff, double air_eff, double hover_eff, double sea_eff, double submarine_eff, double urgency, double range, int randomness, bool water, bool canBuild);

	// returns a cheap defence building (= avg_cost taken
	int GetCheapDefenceBuilding(int side, double efficiency, double combat_power, double cost, double urgency, double ground_eff, double air_eff, double hover_eff, double sea_eff, double submarine_eff, bool water);

	// returns a metal maker
	int GetMetalMaker(int side, float cost, float efficiency, float metal, float urgency, bool water, bool canBuild);

	// returns a storage
	int GetStorage(int side, float cost, float metal, float energy, float urgency, bool water, bool canBuild);

	// return repair pad
	int GetAirBase(int side, float cost, bool water, bool canBuild);

	//! @brief Seletcs a combat unit of specified category according to given criteria
	UnitDefId SelectCombatUnit(int side, const AAICombatCategory& category, const CombatVsCriteria& combatCriteria, const UnitSelectionCriteria& unitCriteria, int randomness, bool canBuild);

	// returns a random unit from the list
	int GetRandomUnit(list<int> unit_list);

	int GetRandomDefence(int side, UnitCategory category);

	int GetStationaryArty(int side, float cost, float range, float efficiency, bool water, bool canBuild);

	//! @brief Determines a scout unit with given properties
	UnitDefId selectScout(int side, float sightRange, float cost, uint32_t movementType, int randomness, bool cloakable, bool factoryAvailable);

	int GetRadar(int side, float cost, float range, bool water, bool canBuild);

	int GetJammer(int side, float cost, float range, bool water, bool canBuild);

	// checks which factory is needed for a specific unit and orders it to be built
	void BuildFactoryFor(int unit_def_id);

	// tries to build another builder for a certain building
	void BuildBuilderFor(int building_def_id);

	// tries to build an assistant for the specified kind of unit
	void AddAssistant(uint32_t allowedMovementTypes, bool canBuild);

	float GetFactoryRating(int def_id);

	// updates unit table
	void UpdateTable(const UnitDef* def_killer, int killer, const UnitDef *def_killed, int killed);

	// updates max and average eff. values of the different categories
	void UpdateMinMaxAvgEfficiency();

	// returns max damage of all weapons
	float GetMaxDamage(int unit_id);

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

	// return a units eff. against a certain category
	float GetEfficiencyAgainst(int unit_def_id, UnitCategory category);

	bool IsCommander(int def_id);

	bool IsBuilder(int def_id);
	bool IsFactory(int def_id);

	// returns id of assault category
	int GetIDOfAssaultCategory(UnitCategory category);
	UnitCategory GetAssaultCategoryOfID(int id);


	//
	// these data are shared by several instances of aai
	//

	// number of assault categories
	static const int ass_categories = 5;

	// number of assault cat + arty & stat defences
	static const int combat_categories = 6;

	// path/name of the file in which AAI stores the build table
	static char buildtable_filename[500];

	// cached values of average costs and buildtime
	static vector<vector<float>> avg_cost;
	static vector<vector<float>> avg_buildtime;
	static vector<vector<float>> avg_value;	// used for different things, range of weapons, radar range, mex efficiency
	static vector<vector<float>> max_cost;
	static vector<vector<float>> max_buildtime;
	static vector<vector<float>> max_value;
	static vector<vector<float>> min_cost;
	static vector<vector<float>> min_buildtime;
	static vector<vector<float>> min_value;

	static vector<vector<float>> avg_speed;
	static vector<vector<float>> min_speed;
	static vector<vector<float>> max_speed;
	static vector<vector<float>> group_speed;

	// combat categories that attacked AI in certain game period attacked_by_category_learned[map_type][period][cat]
	static vector< vector< vector<float> > > attacked_by_category_learned;

	// combat categories that attacked AI in certain game period attacked_by_category_current[period][cat]
	static vector< vector<float> > attacked_by_category_current;

	// units of the different categories
	static vector<vector<list<int>>> units_of_category;

	// AAI unit defs (static things like id, side, etc.)
	static vector<UnitTypeStatic> units_static;

	// storage for def. building selection
	static vector<vector<double> > def_power;
	static vector<double> max_pplant_eff;

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

	// for internal use
	const char* GetCategoryString(int def_id);
	const char* GetCategoryString2(UnitCategory category);

	// all assault unit categories
	list<UnitCategory> assault_categories;

	const UnitDef& GetUnitDef(int i) { assert(IsValidUnitDefID(i));	return *unitList[i];}
	bool IsValidUnitDefID(int i) { return (i>=0) && (i<=unitList.size()); }

private:
	std::string GetBuildCacheFileName();
	// precaches speed/cost/buildtime/range stats
	void PrecacheStats();

	// only precaches costs (called after possible cost multipliers have been assigned)
	void PrecacheCosts();

	// returns true, if unitid is in the list
	bool MemberOf(int unit_id, list<int> unit_list);
	// for internal use
	void CalcBuildTree(int unit);

	bool LoadBuildTable();

	void DebugPrint();

	//! @brief Calculates the combat statistics needed for unit selection
	void CalculateCombatPowerForUnits(const std::list<int>& unitList, const AAICombatCategory& category, const CombatVsCriteria& combatCriteria, std::vector<float>& combatPowerValues, StatisticalData& combatPowerStat, StatisticalData& combatEfficiencyStat);

	AAI * ai;

	// all the unit defs, FIXME: this can't be made static as spring seems to free the memory returned by GetUnitDefList()
	std::vector<const UnitDef*> unitList;
};

#endif

