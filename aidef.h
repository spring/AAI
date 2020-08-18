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
#include <vector>
#include <string>

//#ifdef _MSC_VER
//#pragma warning(disable: 4244 4018) // signed/unsigned and loss of precision...
//#endif

#define AAI_VERSION aiexport_getVersion()
#define MAP_CACHE_VERSION "MAP_DATA_0_91"
#define MAP_LEARN_VERSION "MAP_LEARN_0_89"
#define MOD_LEARN_VERSION "MOD_LEARN_0_90c"
#define CONTINENT_DATA_VERSION "MOVEMENT_MAPS_0_87"

#define AILOG_PATH "log/"
#define MAP_LEARN_PATH "learn/mod/"
#define MOD_LEARN_PATH "learn/mod/"

//! Constants used within AAI
class AAIConstants
{
public:
	//! @todo Make this changeable via optinal mod config file
    static constexpr float energyToMetalConversionFactor = 60.0f;

	//! Minimum combat power value
	static constexpr float minCombatPower = 0.01f;
	
	//! Maximum combat power value
	static constexpr float maxCombatPower = 1000.0f;

	//! The maximum change from a single combat (attacker killes certain unit) - prevent odd statistical values from "lucky kills" (e.g. weak units gets last shot on stong one)
	static constexpr float maxCombatPowerChangeAfterSingleCombat = 0.5f;

	//! Minimum combat power for a unit to be considered effective against a certain target type
	static constexpr float minAntiTargetTypeCombatPower = 0.25f;

	//! Minimum combat power vs specific target type such that a group of only one unit may participate in attacks
	static constexpr float minCombatPowerForSoloAttack  = 2.5f;

	//! Minimum unused metal storage capcity befor construction of metal storage is taken into account
	static constexpr float minUnusedMetalStorageCapacityToBuildStorage = 100.0f;

	//! Minimum unused metal storage capcity befor construction of energy storage is taken into account
	static constexpr float minUnusedEnergyStorageCapacityToBuildStorage = 600.0f;

	//! Minimum averaged metal surplus before constrcution of non-resource generating units shall be assisted
	static constexpr float minMetalSurplusForConstructionAssist = 0.5f;

		//! Minimum averaged energy surplus before constrcution of non-resource generating units shall be assisted
	static constexpr float minEnergySurplusForConstructionAssist = 40.0f;
};

class AAIMetalSpot
{
public:
	AAIMetalSpot(float3 _pos, float _amount):
		pos(_pos),
		occupied(false),
		extractor(-1),
		extractor_def(-1),
		amount(_amount)
	{}
	AAIMetalSpot():
		pos(ZeroVector),
		occupied(false),
		extractor(-1),
		extractor_def(-1),
		amount(0)
	{}

	float3 pos;
	bool occupied;
	int extractor;		// -1 if unocuppied
	int extractor_def;	// -1 if unocuppied
	float amount;
};


//! Criteria (combat efficiency vs specific kind of target type) used for selection of units
class CombatPower
{
public:
	CombatPower() {}

	CombatPower(float initialValue) :
			vsGround(initialValue), vsAir(initialValue), vsHover(initialValue), vsSea(initialValue), vsSubmarine(initialValue), vsBuildings(initialValue) {}

	CombatPower(float ground, float air, float hover, float sea, float submarine, float buildings = 0.0f) :
			vsGround(ground), vsAir(air), vsHover(hover), vsSea(sea), vsSubmarine(submarine), vsBuildings(buildings) {}

	float CalculateSum() const
	{
		return vsGround + vsAir + vsHover + vsSea + vsSubmarine + vsBuildings;
	}

	float vsGround;
	float vsAir;
	float vsHover;
	float vsSea;
	float vsSubmarine;
	float vsBuildings;
};

enum UnitCategory {UNKNOWN, STATIONARY_DEF, STATIONARY_ARTY, STORAGE, STATIONARY_CONSTRUCTOR, AIR_BASE,
STATIONARY_RECON, STATIONARY_JAMMER, STATIONARY_LAUNCHER, DEFLECTION_SHIELD, POWER_PLANT, EXTRACTOR, METAL_MAKER,
COMMANDER, GROUND_ASSAULT, AIR_ASSAULT, HOVER_ASSAULT, SEA_ASSAULT, SUBMARINE_ASSAULT, GROUND_ARTY, SEA_ARTY, HOVER_ARTY,
SCOUT, MOBILE_TRANSPORT, MOBILE_JAMMER, MOBILE_LAUNCHER, MOBILE_CONSTRUCTOR};

enum UnitTask {UNIT_IDLE, UNIT_ATTACKING, DEFENDING, GUARDING, MOVING, BUILDING, SCOUTING, ASSISTING, RECLAIMING, HEADING_TO_RALLYPOINT, UNIT_KILLED, ENEMY_UNIT, BOMB_TARGET};
enum MapType {LAND_MAP, LAND_WATER_MAP, WATER_MAP, UNKNOWN_MAP};

//! @brief An id identifying a specific unit - used to prevent mixing ids referring to units and unit definitions
struct UnitId
{
public:
	UnitId(int unitId) : id(unitId) { };

	UnitId() : id(-1) { };

	bool isValid() const { return (id >= 0) ? true : false; };

	void invalidate() { id = -1; };

	int id;
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
	int unit_id;
	int def_id;
	AAIGroup *group;
	AAIConstructor *cons;
	UnitTask status;
	int last_order;
};

#endif

