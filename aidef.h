// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_DEF_H
#define AAI_DEF_H

#include "System/float3.h"
#include "Sim/Misc/GlobalConstants.h"
#include <vector>
#include <string>
#include <list>

#define AAI_VERSION aiexport_getVersion()
#define MAP_CACHE_VERSION "MAP_DATA_0_92b"
#define MAP_LEARN_VERSION "MAP_LEARN_0_91"
#define MOD_LEARN_VERSION "MOD_LEARN_0_92"
#define CONTINENT_DATA_VERSION "MOVEMENT_MAPS_0_90"

#define AILOG_PATH "log/"
#define MAP_LEARN_PATH "learn/mod/"
#define MOD_LEARN_PATH "learn/mod/"

//! Constants used within AAI
class AAIConstants
{
public:
	// The length/width of sectors (in map tiles)
	static constexpr float sectorSize = 80.0f;
	
	//! The relative importance of a sector will be capped at this value
	static constexpr float maxSectorImportance = 5.0f;

	//! @todo Make this changeable via optinal mod config file
    static constexpr float energyToMetalConversionFactor = 60.0f;

	//! Minimum combat power value
	static constexpr float minCombatPower = 0.01f;
	
	//! Maximum combat power value
	static constexpr float maxCombatPower = 20.0f;

	//! Minimum initial combat power (if unit is allowed to target units of target category)
	static constexpr float minInitialCombatPower = 1.0f;

	//! Initial combat power if unit is not allowed to target units of target category
	static constexpr float noValidTargetInitialCombatPower = 0.1f;

	//! The maximum change from a single combat (attacker killes certain unit) - prevent odd statistical values from "lucky kills" (e.g. weak units gets last shot on stong one)
	static constexpr float maxCombatPowerChangeAfterSingleCombat = 0.25f;

	//! The factor applied to determine change of combat power for killer/destroyed unit type
	static constexpr float combatPowerLearningFactor = 0.02f;

	//! Minimum combat power for a unit to be considered effective against a certain target type
	static constexpr float minAntiTargetTypeCombatPower = 0.15f;

	//! Minimum combat power vs specific target type such that a group of only one unit may participate in attacks
	static constexpr float minCombatPowerForSoloAttack  = 2.5f;

	//! Minimum weapons range difference to shorter ranged attacked before combat units try to keep their distance
	static constexpr float minWeaponRangeDiffToKeepDistance = 50.0f;

	//! Minimum averaged metal surplus before constrcution of non-resource generating units shall be assisted
	static constexpr float minMetalSurplusForConstructionAssist = 1.0f;

	//! Minimum averaged energy surplus before constrcution of non-resource generating units shall be assisted
	static constexpr float minEnergySurplusForConstructionAssist = 40.0f;

	//! Maximum power surplus until construction of further power plants shall be considered
	static constexpr float powerSurplusToStopPowerPlantConstructionThreshold = 2000.0f;

	//! Maximum distance to rally points for units to be considered to have reached it
	static constexpr float maxSquaredDistToRallyPoint = static_cast<float>( (16*SQUARE_SIZE)*(16*SQUARE_SIZE) );

	//! Distance between individual combat units of the same group that is kept between move/fight/patrol target positions (to avoid blocking line of fire when sending all units to the same position)
	static constexpr float distanceBetweenUnitsInGroup = 5.0f * static_cast<float>(SQUARE_SIZE);

	//! Distance between individual combat units of the same group that is kept between target positions when units are send to rally points (increase chance of intercepting enemy scouts/raiders)
	static constexpr float rallyDistanceBetweenUnitsInGroup = 8.0f * static_cast<float>(SQUARE_SIZE);

	//! The factor applied to the combat power of the own units (when deciding whether to attack)
	static constexpr float attackCombatPowerFactor = 2.5f;

	//! If the local defence power against the target type of the attacker is below this threshold combat units shall be orderd to support.
	static constexpr float localDefencePowerToRequestSupportThreshold = 1.0f;

	//! The minimum number of frames between two updates of the units in current LOS (to avoid too heavy CPU load)
	static constexpr int   minFramesBetweenLOSUpdates = 10;

	//! Number of data points used to calculate smoothed energy/metal income/surplus 
	static constexpr int   incomeSamplePoints = 16;

	//! Maximum number of parallel attacks
	static constexpr int   maxNumberOfAttacks = 3;

	//! Urgency of bombing run
	static constexpr float bombingRunUrgency = 100.0f;

	//! Urgency of defending combat units
	static constexpr float defendUnitsUrgency = 105.0f;

	//! Urgency of defending base
	static constexpr float defendBaseUrgency = 115.0f;

	//! Urgency of defending construction unit
	static constexpr float defendConstructorsUrgency = 110.0f;

	//! Importance of attacking enemy base
	static constexpr float attackEnemyBaseUrgency = 110.0f;

	//! Maximum number of (recently) lost air units in a sector for air support to be sent
	static constexpr float maxLostAirUnitsForAirSupport = 2.5f;

	//! Minimum combat power a group must have vs given target type to be taken into account as air support
	static constexpr float minAirSupportCombatPower = 1.0f;

	//! Threshold for enemy AA power for air raid target to be removed from the list
	static constexpr float maxEnemyAACombatPowerForTarget = 10.0f;
};

enum UnitTask {UNIT_IDLE, UNIT_ATTACKING, DEFENDING, GUARDING, MOVING, BUILDING, SCOUTING, ASSISTING, RECLAIMING, HEADING_TO_RALLYPOINT, UNIT_KILLED, ENEMY_UNIT, BOMB_TARGET};

//! @brief An id identifying a specific unit - used to prevent mixing ids referring to units and unit definitions
struct UnitId
{
public:
	UnitId(int unitId) : id(unitId) { };

	UnitId() : id(-1) { };

	bool operator==(const UnitId& rhs) const { return (id == rhs.id); }

	bool operator<(const UnitId& rhs) const { return (id < rhs.id); }

	bool IsValid() const { return (id >= 0) ? true : false; };

	void Invalidate() { id = -1; };

	int id;
};

//! @brief An id identifying a unit type - used to prevent mixing ids referring to units and unit definitions
class UnitDefId
{
public:
	UnitDefId(int unitDefId) : id(unitDefId) {}

	UnitDefId() : UnitDefId(0) {}
	
	bool operator==(const UnitDefId& rhs) const { return (id == rhs.id); }

	bool IsValid() const { return (id > 0) ? true : false; }

	void Invalidate() { id = 0; }

	int id;
};

//! @brief An id identifying the corresponding buildqueues etc. for factories
class FactoryId
{
public:
	FactoryId(int factoryId) : id(factoryId) {}

	FactoryId() : FactoryId(-1) { }

	bool IsValid() const { return (id >= 0) ? true : false; }

	void Set(int factoryId) { id = factoryId; }

	int id;
};

enum class BuildQueuePosition : int {FRONT, SECOND, END};

//! @brief Helper class to handle buildqueues associated with each type of construction unit 
class Buildqueue
{
public:
	Buildqueue(std::list<UnitDefId>* queue) : m_buildqueue(queue) {}

	Buildqueue() : Buildqueue(nullptr) {}

	bool      IsValid()      const { return (m_buildqueue != nullptr); }

	size_t    GetLength()    const { return m_buildqueue->size(); }

	UnitDefId GetFirstUnit() const { return *(m_buildqueue->begin()); }

	void      RemoveFirstUnit()    { m_buildqueue->pop_front(); }

	void AddUnits(UnitDefId unitDefId, int number, BuildQueuePosition position)
	{
		auto insertPosition = m_buildqueue->begin();

		if( (position == BuildQueuePosition::SECOND) && (m_buildqueue->size() > 0))
			++insertPosition;
		else if(position == BuildQueuePosition::END)
			insertPosition = m_buildqueue->end();

		m_buildqueue->insert(insertPosition, number, unitDefId);
	}

private:
	std::list<UnitDefId>* m_buildqueue;
};

//! This class stores the information required for placing/upgrading metal extractors
class AAIMetalSpot
{
public:
	AAIMetalSpot(const float3& _pos, float _amount):
		pos(_pos),
		occupied(false),
		amount(_amount)
	{}

	AAIMetalSpot():
		pos(ZeroVector),
		occupied(false),
		amount(0.0f)
	{}

	void SetUnoccupied()
	{
		occupied = false;
		extractorUnitId.Invalidate();
		extractorDefId.Invalidate();
	}

	//! @brief Returns whether spot belong to given map position
	bool DoesSpotBelongToPosition(const float3& position) const
	{
		return (std::fabs(pos.x - position.x) < 16.0f) && (std::fabs(pos.z - position.z) < 16.0f);
	}

	//! The position of the metal spot in the map
	float3    pos;

	//! Flag whether the spot is currently occupied by any AAI player
	bool      occupied;

	//! UnitId of the extractor occupying the spot
	UnitId    extractorUnitId;

	//! UnitDefId of the extractor occupying the spot
	UnitDefId extractorDefId;

	//! The ammount of metal that can be extracted from the spot
	float     amount;
};

//! @brief This class encapsulates the determination of the current game phase (ranging from start to late game) 
//!        used to differentiate when making decisions/record learning data
class GamePhase
{
public:
	GamePhase(int frame)
	{
		for(int i = 1; i < numberOfGamePhases; ++i) // starting with i=1 on purpose 
		{
			if(frame < m_startFrameOfGamePhase[i])
			{
				m_gamePhase = i-1;
				return;
			}
		}
		m_gamePhase = 3;
	}

	bool operator>(const GamePhase& rhs) const { return (m_gamePhase > rhs.m_gamePhase); }

	bool operator<(const GamePhase& rhs) const { return (m_gamePhase < rhs.m_gamePhase); }

	bool operator<=(const GamePhase& rhs) const { return (m_gamePhase <= rhs.m_gamePhase); }

	int GetArrayIndex()          const { return m_gamePhase; };

	const std::string& GetName() const { return m_gamePhaseNames[m_gamePhase]; }

	bool IsStartingPhase()       const {return (m_gamePhase == 0); }

	bool IsEarlyPhase()          const {return (m_gamePhase == 1); }

	bool IsIntermediatePhase()   const {return (m_gamePhase == 2); }

	bool IsLatePhase()           const {return (m_gamePhase == 3); }

	void Next() { ++m_gamePhase; }

	bool End() const { return (m_gamePhase >= numberOfGamePhases); }

	static const int numberOfGamePhases = 4;

private:
	int m_gamePhase;

	//! Frame at which respective game phase starts: 0 -> 0 min, 1 -> 6min, 2 -> 15 min, 3 -> 40 min
	const static std::vector<int>         m_startFrameOfGamePhase;

	const static std::vector<std::string> m_gamePhaseNames;
	
	//const static inline std::vector<int> m_startFrameOfGamePhase = {0, 10800, 27000, 72000}; use when switching to Cpp17
	//const static inline std::vector<int> m_gamePhaseNames = {"starting phase", "early phase", "mid phase", "late game"}; use when switching to Cpp17
};

class SmoothedData
{
public:
	SmoothedData(int smoothingLength) : m_averageValue(0.0f), m_nextIndex(0) { m_values.resize(smoothingLength, 0.0f); } 

	float GetAverageValue() const { return m_averageValue; }

	void AddValue(float value) 
	{
		m_averageValue += (value - m_values[m_nextIndex]) / static_cast<float>(m_values.size());
		m_values[m_nextIndex] = value;
		++m_nextIndex;

		if(m_nextIndex >= m_values.size())
			m_nextIndex = 0;
	}

	void FillBuffer(float value)
	{
		m_nextIndex = 0;
		std::fill(m_values.begin(), m_values.end(), value);
		m_averageValue = value;
	}

private:
	//! The values to be averaged
	std::vector<float> m_values;

	//! The current average value
	float m_averageValue;

	//! Index where next value will be added 
	int   m_nextIndex;
};

class AAIGroup;
class AAIConstructor;
struct AAIUnit
{
	int def_id;
	AAIGroup *group;
	AAIConstructor *cons;
	UnitTask status;
	int last_order;
};

#endif

