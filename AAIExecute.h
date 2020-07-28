// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_EXECUTE_H
#define AAI_EXECUTE_H

#include "aidef.h"
#include "AAITypes.h"
#include "AAIUnitTypes.h"

namespace springLegacyAI {
	struct UnitDef;
}
using namespace springLegacyAI;

enum class BuildOrderStatus : int {BUILDING_INVALID, NO_BUILDSITE_FOUND, NO_BUILDER_AVAILABLE, SUCCESSFUL};

class AAI;
class AAIBuildTable;
class AAIBrain;
class AAIMap;
class AAIUnitTable;
class AAISector;

struct PossibleSpotForMetalExtractor
{
	PossibleSpotForMetalExtractor(AAIMetalSpot* metalSpot, AAIConstructor* builder, float distanceToClosestBuilder) :
		m_metalSpot(metalSpot),
		m_builder(builder),
		m_distanceToClosestBuilder(m_distanceToClosestBuilder)
		{}

	AAIMetalSpot*   m_metalSpot;
	AAIConstructor* m_builder;
	float           m_distanceToClosestBuilder;
};

class AAIExecute
{
public:
	AAIExecute(AAI* ai);
	~AAIExecute(void);

	void InitAI(int commander_unit_id, const UnitDef *commander_def);

	//! @brief Updates buildmap & defence map (for static defences) and building data of target sector 
	//!        Return true if building will be placed at a valid position, i.e. inside sectors
	bool InitBuildingAt(const UnitDef *def, const float3& position);

    //! @brief creates a BuildTask for given unit and links it to responsible construction unit
	void createBuildTask(UnitId unitId, UnitDefId unitDefId, float3 *pos);

	void MoveUnitTo(int unit, float3 *position);

	//! @brief Add the given unit to an existing group (or create new one if necessary)
	void AddUnitToGroup(const UnitId& unitId, const UnitDefId& unitDefId);

	void BuildScouts();

	void SendScoutToNewDest(int scout);

	unsigned int GetLinkingBuildTaskToBuilderFailedCounter() const { return m_linkingBuildTaskToBuilderFailed; };

	//! @brief Searches for a position to retreat unit of certain type
	float3 determineSafePos(UnitDefId unitDefId, float3 unit_pos);

	// updates average ressource usage
	void UpdateRessources();

	// checks if ressources are sufficient and orders construction of new buildings
	void CheckRessources();


	// checks if buildings of that type could be replaced with more efficient one (e.g. mex -> moho)
	void CheckMexUpgrade();
	void CheckRadarUpgrade();
	void CheckJammerUpgrade();

	// checks which building type is most important to be constructed and tries to start construction
	void CheckConstruction();

	// the following functions determine how urgent it is to build a further building of the specified type
	void CheckFactories();
	void CheckAirBase();
	void CheckRecon();
	void CheckJammer();
	void CheckStationaryArty();

	// checks length of buildqueues and adjusts rate of unit production
	void CheckBuildqueues();

	//
	void CheckDefences();

	// builds all kind of buildings

//	void BuildUnit(UnitCategory category, float speed, float cost, float range, float power, float ground_eff, float air_eff, float hover_eff, float sea_eff, float submarine_eff, float stat_eff, float eff, bool urgent);

	// called when building has been finished / contruction failed
	void ConstructionFailed(float3 build_pos, UnitDefId unitDefId);

	// builds defences around mex spot if necessary
	void DefendMex(int mex, int def_id);

	// returns a position for the unit to withdraw from close quarters combat (but try to keep enemies in weapons range)
	// returns ZeroVector if no suitable pos found (or no enemies close enough)
	void GetFallBackPos(float3 *pos, int unit_id, float max_weapon_range) const;

	void CheckFallBack(int unit_id, int def_id);

	//! @brief Tries to call support against specific attacker (e.g. air)
	void DefendUnitVS(int unit, const AAIMovementType& attackerMoveType, float3 *enemy_pos, int importance);

	//! @brief Adds the given number of units to the most suitable buildqueue
	bool AddUnitToBuildqueue(UnitDefId unitDefId, int number, bool urgent);

	// returns buildque for a certain factory
	std::list<int>* GetBuildqueueOfFactory(int def_id);

	float3 GetUnitBuildsite(int builder, int unit);

	int unitProductionRate;

	// ressource management
	// tells ai, how many times additional metal/energy has been requested
	float futureRequestedMetal;
	float futureRequestedEnergy;
	float futureAvailableMetal;
	float futureAvailableEnergy;
	float futureStoredMetal;
	float futureStoredEnergy;
	float averageMetalSurplus;
	float averageEnergySurplus;
	int disabledMMakers;


	// urgency of construction of building of the different categories
	float urgency[METAL_MAKER+1];

	// sector where next def vs category needs to be built (0 if none)

	// debug
	void GiveOrder(Command *c, int unit, const char *owner);

private:
	// accelerates game startup
	void AddStartFactory();

	// custom relations
	float static sector_threat(AAISector *);

	bool static least_dangerous(AAISector *left, AAISector *right);
	bool static suitable_for_power_plant(AAISector *left, AAISector *right);
	bool static suitable_for_ground_factory(AAISector *left, AAISector *right);
	bool static suitable_for_sea_factory(AAISector *left, AAISector *right);
	bool static defend_vs_ground(AAISector *left, AAISector *right);
	bool static defend_vs_air(AAISector *left, AAISector *right);
	bool static defend_vs_hover(AAISector *left, AAISector *right);
	bool static defend_vs_sea(AAISector *left, AAISector *right);
	bool static defend_vs_submarine(AAISector *left, AAISector *right);
	bool static suitable_for_ground_rallypoint(AAISector *left, AAISector *right);
	bool static suitable_for_sea_rallypoint(AAISector *left, AAISector *right);
	bool static suitable_for_all_rallypoint(AAISector *left, AAISector *right);

	// cache to speed things up a bit
	float static learned;
	float static current;
	// buildques for the factories
	std::vector<std::list<int> > buildques;
	// number of factories (both mobile and sationary)

	int numOfFactories;

	//! @brief Tries to build a defence building vs category in the specified sector
	//!        returns BUILDORDER_SUCCESSFUL if successful
	BuildOrderStatus BuildStationaryDefenceVS(const AAIUnitCategory& category, const AAISector *dest);

	//! @brief Returns true if a construction unit was ordered to assist construction of a building of givn category
	bool AssistConstructionOfCategory(const AAIUnitCategory& category);

	// returns the the total ground offensive power of all units
	float GetTotalGroundPower();

	// returns the the total air defence power of all units
	float GetTotalAirPower();

	// chooses a starting sector close to specified sector
	void ChooseDifferentStartingSector(int x, int y);

	// returns closest (taking into account movement speed) group with units of specified unit type that may reach the location

	AAIGroup* GetClosestGroupForDefence(UnitType group_type, float3 *pos, int continent, int importance);
	
	float3 GetBuildsite(int builder, int building, UnitCategory category);
	void InitBuildques();

	void stopUnit(int unit);
	void ConstructBuildingAt(int building, int builder, float3 position);
	bool IsBusy(int unit);

	float GetEnergyUrgency();

	float GetMetalUrgency();
	float GetEnergyStorageUrgency();
	float GetMetalStorageUrgency();

	//! @brief Tries to find a suitable buildsite and builder to start the construction of the given building;
	BuildOrderStatus TryConstructionOf(UnitDefId building, const AAISector* sector);

	BuildOrderStatus TryConstructionOf(UnitDefId landBuilding, UnitDefId seaBuilding, const AAISector* sector);

	bool BuildFactory();
	bool BuildDefences();
	bool BuildRadar();
	bool BuildJammer();
	bool BuildExtractor();
	bool BuildMetalMaker();
	bool BuildStorage();
	bool BuildPowerPlant();
	bool BuildArty();
	bool BuildAirBase();

	float averageMetalUsage;
	float averageEnergyUsage;
	int counter;
	float metalSurplus[8];
	float energySurplus[8];
	AAISector *next_defence;

	AAIUnitCategory m_nextDefenceVsCategory;

	int issued_orders;

	AAI *ai;

	// stores which buildque belongs to what kind of factory
	std::vector<int> factory_table;

	//! Number of times a building was created but no suitable builder could be identfied.
	unsigned int m_linkingBuildTaskToBuilderFailed;
};

#endif

