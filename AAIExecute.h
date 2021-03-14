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

#include <list>
#include "aidef.h"
#include "AAITypes.h"
#include "AAIUnitTypes.h"
#include "AAIBuildTable.h"

namespace springLegacyAI {
	struct UnitDef;
}
using namespace springLegacyAI;

enum class BuildOrderStatus : int {BUILDING_INVALID, NO_BUILDSITE_FOUND, NO_BUILDER_AVAILABLE, SUCCESSFUL};

enum class BuildQueuePosition : int {FRONT, SECOND, END};

class AAI;
class AAIBuildTable;
class AAIBrain;
class AAIMap;
class AAIUnitTable;
class AAISector;

struct AvailableMetalSpot
{
	AvailableMetalSpot(AAIMetalSpot* metalSpot, AAIConstructor* builder, UnitDefId extractor) :
		metalSpot(metalSpot),
		builder(builder),
		extractor(extractor)
		{}

	AAIMetalSpot*   metalSpot;
	AAIConstructor* builder;
	UnitDefId       extractor;
};

class AAIExecute
{
public:
	AAIExecute(AAI* ai);
	~AAIExecute(void);

	//! @brief Determines starting sector, adds another sector to base and initializes buildqueues
	void InitAI(UnitId commanderUnitId, UnitDefId commanderDefId);

	//! @brief Orders the unit to move to the given position
	void SendUnitToPosition(UnitId unitId, const float3& position) const;

	//! @brief Add the given unit to an existing group (or create new one if necessary)
	void AddUnitToGroup(const UnitId& unitId, const UnitDefId& unitDefId);

	//! @brief Selects combat unit according to given criteria and tries to order its construction
	void BuildCombatUnitOfCategory(const AAIMovementType& moveType, const TargetTypeValues& combatPowerCriteria, const UnitSelectionCriteria& unitSelectionCriteria, const std::vector<float>& factoryUtilization, bool urgent);

	void BuildScouts();

	//! @brief Looks for the next suitable destination for the given scout and orders it to move there
	void SendScoutToNewDest(UnitId scoutId) const;

	//! @brief Returns the current unit production rate (i.e. how many unit AAI tries to order per unit production update step)
	int GetUnitProductionRate() const { return m_unitProductionRate; }

	//! @brief Returns the number of Build Tasks that could not be linked to a construction unit (for debugging only - should be zero)
	unsigned int GetLinkingBuildTaskToBuilderFailedCounter() const { return m_linkingBuildTaskToBuilderFailed; };

	//! @brief Searches for a position to retreat unit of certain type
	float3 DetermineSafePos(UnitDefId unitDefId, float3 unit_pos) const;

	// checks if ressources are sufficient and orders construction of new buildings
	void CheckRessources();

	// checks if buildings of that type could be replaced with more efficient one (e.g. mex -> moho)
	void CheckExtractorUpgrade();
	void CheckRadarUpgrade();
	void CheckJammerUpgrade();

	// checks which building type is most important to be constructed and tries to start construction
	void CheckConstruction();

	// the following functions determine how urgent it is to build a further building of the specified type
	void CheckFactories();

	void CheckRecon();

	void CheckStationaryArty();

	// checks length of buildqueues and adjusts rate of unit production
	void CheckBuildqueues();

	//
	void CheckDefences();

	//! @brief Check if construction nano turrets shall be ordered to support busy static constructors
	void CheckConstructionOfNanoTurret();

	//! @brief Cleans up buildmap/updates internal counters/frees metalspots if contruction has failed
	void ConstructionFailed(const float3& buildsite, UnitDefId unitDefId);

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
	bool AddUnitToBuildqueue(UnitDefId unitDefId, int number, BuildQueuePosition queuePosition, bool ignoreMaxQueueLength = false);

	//! @brief Returns buildque for a certain constructor
	std::list<UnitDefId>* GetBuildqueueOfFactory(UnitDefId constructorDefId);

	//! @brief Determines the utilization (i.e. how long is the buildqueue) of the different factories
	void DetermineFactoryUtilization(std::vector<float>& factoryUtilization, bool considerOnlyActiveFactoryTypes) const;

	//! @brief Determines buildsite for a unit (not building) that shall be constructed by the given construction unit
	BuildSite DetermineBuildsiteForUnit(UnitId constructor, UnitDefId unitDefId) const;

	//! @brief Sets urgency to construct building of given category to given value if larger than current value
	void SetConstructionUrgencyIfHigher(const AAIUnitCategory& category, float urgency)
	{
		if(m_constructionUrgency[category.GetArrayIndex()] < urgency)
			m_constructionUrgency[category.GetArrayIndex()] = urgency;
	}

	// debug
	void GiveOrder(Command *c, int unit, const char *owner) const;

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

	float static learned;
	float static current;

	//! @brief Calls construction fucntion for given category and resets urgency to 0.0f if construction order has been given
	void TryConstruction(const AAIUnitCategory& category);

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
	BuildSite DetermineBuildsite(UnitId builder, UnitDefId buildingDefId) const;

	//! @brief Determines buildiste for the given building in the given sector, returns ZeroVector if none found
	BuildSite DetermineBuildsiteInSector(UnitDefId building, const AAISector* sector) const;
	
	void InitBuildques();

	void stopUnit(int unit);
	void ConstructBuildingAt(int building, int builder, float3 position);
	bool IsBusy(int unit);

	//! @brief Determine sectors that are suitable to construct eco (power plants, storage, metal makers); highest ranked sector is first in the list
	void DetermineSectorsToConstructEco(std::list<AAISector*>& sectors) const;

	//! @brief Tries to order construction of given building in one of the given sectors
	BuildOrderStatus ConstructBuildingInSectors(UnitDefId building, std::list<AAISector*>& availableSectors);

	//! @brief Helper function for construction of buildings
	BuildOrderStatus TryConstructionOfBuilding(UnitDefId building, AAISector* sector);

	//! @brief Tries to find a suitable buildsite and builder to start the construction of the given building;
	BuildOrderStatus TryConstructionOf(UnitDefId building, const AAISector* sector);

	BuildOrderStatus TryConstructionOf(UnitDefId landBuilding, UnitDefId seaBuilding, const AAISector* sector);

	bool BuildStaticConstructor();
	bool BuildDefences();
	bool BuildRadar();
	bool BuildJammer();
	bool BuildExtractor();
	bool BuildMetalMaker();
	bool BuildStorage();
	bool BuildPowerPlant();
	bool BuildArty();
	bool BuildAirBase();

	//! Buildqueues for the factories
	std::vector< std::list<UnitDefId> > m_buildqueues;

	//! Urgency of construction of building of the different categories
	std::vector<float> m_constructionUrgency;

	//! Pointer to correspondind construction function for each category (or nullptr if none)
	std::vector< bool (AAIExecute::*) ()> m_constructionFunctions;

	//! Sector where next static defence shall be build (nullptr if none)
	AAISector*    m_sectorToBuildNextDefence;

	//! Target type against which which next defence shall be effective
	AAITargetType m_nextDefenceVsTargetType;

	//! The number of units the AI tries to order in every unit production update step
	int m_unitProductionRate;

	//! The total number of issued orders (primarily for debug purposes)
	mutable int m_numberOfIssuedOrders;

	//! Number of times a building was created but no suitable builder could be identfied (should be zero - just for debug purposes)
	unsigned int m_linkingBuildTaskToBuilderFailed;

	AAI *ai;
};

#endif

