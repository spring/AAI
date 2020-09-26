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

	//! @brief Determines starting sector, adds another sector to base and initializes buildqueues
	void InitAI(UnitId commanderUnitId, UnitDefId commanderDefId);

    //! @brief creates a BuildTask for given unit and links it to responsible construction unit
	void createBuildTask(UnitId unitId, UnitDefId unitDefId, float3 *pos);

	void MoveUnitTo(int unit, float3 *position);

	//! @brief Add the given unit to an existing group (or create new one if necessary)
	void AddUnitToGroup(const UnitId& unitId, const UnitDefId& unitDefId);

	void BuildScouts();

	void SendScoutToNewDest(int scout);

	unsigned int GetLinkingBuildTaskToBuilderFailedCounter() const { return m_linkingBuildTaskToBuilderFailed; };

	//! @brief Searches for a position to retreat unit of certain type
	float3 DetermineSafePos(UnitDefId unitDefId, float3 unit_pos) const;

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

	//! @brief Orders construction of static defence to protect metal extractor
	void BuildStaticDefenceForExtractor(UnitId extractorId, UnitDefId extractorDefId) const;

	//! @brief Returns a position for the unit to withdraw from close quarters combat (but try to keep enemies in weapons range)
	//!        Returns ZeroVector if no suitable pos found (or no enemies close enough)
	float3 GetFallBackPos(const float3& pos, float maxFallbackDist) const;

	//! @brief Checks if a combat unit attacked by given enemy shall move back a little to maintain distance to attacker
	void CheckKeepDistanceToEnemy(UnitId unitId, UnitDefId unitDefId, UnitDefId enemyDefId);

	//! @brief Tries to call support against specific attacker (e.g. air)
	void DefendUnitVS(const UnitId& unitId, const AAITargetType& attackerTargetType, const float3& attackerPosition, int importance) const;

	//! @brief Adds the given number of units to the most suitable buildqueue
	bool AddUnitToBuildqueue(UnitDefId unitDefId, int number, bool urgent);

	// returns buildque for a certain factory
	std::list<int>* GetBuildqueueOfFactory(int def_id);

	//! @brief Determines buildsite for a unit (not building) that shall be constructed by the given construction unit
	float3 DetermineBuildsiteForUnit(UnitId constructor, UnitDefId unitDefId) const;

	int unitProductionRate;

	// ressource management
	// tells ai, how many times additional metal/energy has been requested
	float futureAvailableMetal;
	float futureAvailableEnergy;
	int disabledMMakers;

	// urgency of construction of building of the different categories
	float urgency[METAL_MAKER+1];

	// sector where next def vs category needs to be built (0 if none)

	// debug
	void GiveOrder(Command *c, int unit, const char *owner);

private:
	// custom relations
	float static sector_threat(const AAISector *sector);

	bool static least_dangerous(const AAISector *left, const AAISector *right);
	bool static suitable_for_power_plant(AAISector *left, AAISector *right);
	bool static suitable_for_ground_factory(AAISector *left, AAISector *right);
	bool static suitable_for_sea_factory(AAISector *left, AAISector *right);
	bool static defend_vs_ground(const AAISector *left, const AAISector *right);
	bool static defend_vs_air(const AAISector *left, const AAISector *right);
	bool static defend_vs_hover(const AAISector *left, const AAISector *right);
	bool static defend_vs_sea(const AAISector *left, const AAISector *rightt);
	bool static defend_vs_submarine(const AAISector *left, const AAISector *right);
	bool static suitable_for_ground_rallypoint(AAISector *left, AAISector *right);
	bool static suitable_for_sea_rallypoint(AAISector *left, AAISector *right);
	bool static suitable_for_all_rallypoint(AAISector *left, AAISector *right);

	float static learned;
	float static current;

	// buildques for the factories
	std::vector<std::list<int> > buildques;

	//! Number of factories (both mobile and sationary)
	int numOfFactories;

	//! @brief Tries to build a defence building vs target type in the specified sector
	//!        returns BUILDORDER_SUCCESSFUL if successful
	BuildOrderStatus BuildStationaryDefenceVS(const AAITargetType& targetType, const AAISector *dest);

	//! @brief Tries to build a defence fitting to given criteria
	BuildOrderStatus BuildStaticDefence(const AAISector* sector, const StaticDefenceSelectionCriteria& selectionCriteria, bool water) const;

	//! @brief Returns true if a construction unit was ordered to assist construction of a building of givn category
	bool AssistConstructionOfCategory(const AAIUnitCategory& category);

	// chooses a starting sector close to specified sector
	void ChooseDifferentStartingSector(int x, int y);

	//! @brief Returns closest (taking into account movement speed) group with units of specified unit type that may reach the location
	AAIGroup* GetClosestGroupForDefence(const AAITargetType& attackerTargetType, const float3& pos, int importance)  const;
	
	//! @brief Determines buildsite for a building that shall be constructed by the given construction unit
	float3 DetermineBuildsite(UnitId builder, UnitDefId buildingDefId) const;
	
	void InitBuildques();

	void stopUnit(int unit);
	void ConstructBuildingAt(int building, int builder, float3 position);
	bool IsBusy(int unit);

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

	//! Sector where next static defence shall be build (nullptr if none)
	AAISector*    m_sectorToBuildNextDefence;

	//! Target type against which which next defence shall be effective
	AAITargetType m_nextDefenceVsTargetType;

	int issued_orders;

	AAI *ai;

	// stores which buildque belongs to what kind of factory
	std::vector<int> factory_table;

	//! Number of times a building was created but no suitable builder could be identfied.
	unsigned int m_linkingBuildTaskToBuilderFailed;
};

#endif

