// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_CONFIG_H
#define AAI_CONFIG_H

#include "LegacyCpp/IAICallback.h"

#include <stdio.h>
#include <list>
#include <vector>

using std::string;
using std::list;
using std::vector;

class AAI;
namespace springLegacyAI {
	struct UnitDef;
}

using namespace springLegacyAI;


/// Converts a string to one that can be used in a file name (eg. "Abc.123 $%^*" -> "Abc.123_____")
std::string MakeFileSystemCompatible(const std::string& str);

class AAIConfig
{
public:
	AAIConfig(void);

	// @brief Load configuration for specific game/mod from config file
	bool LoadGameConfig(AAI *ai);

	// @brief Load general AAI config
	bool loadGeneralConfig(AAI& ai);

	// constants (will be loaded in aaiconfig)

	// mod specific
	int MIN_ENERGY;  // min energy make value to be considered beeing a power plant
	int MAX_UNITS;
	int MAX_SCOUTS;
	int MAX_XROW;
	int MAX_YROW;
	int X_SPACE;
	int Y_SPACE;
	int MAX_GROUP_SIZE;
	int MAX_AIR_GROUP_SIZE;
	int MAX_SUBMARINE_GROUP_SIZE;
	int MAX_NAVAL_GROUP_SIZE;
	int MAX_ANTI_AIR_GROUP_SIZE;
	int MAX_ARTY_GROUP_SIZE;

	int MAX_BUILDERS_PER_TYPE; // max builders of same unit type
	int MAX_FACTORIES_PER_TYPE;
	int MAX_BUILDQUE_SIZE;
	int MAX_ASSISTANTS;
	int MIN_ASSISTANCE_BUILDTIME;
	int MAX_BASE_SIZE;
	float SCOUT_SPEED;
	float GROUND_ARTY_RANGE;
	float SEA_ARTY_RANGE;
	float HOVER_ARTY_RANGE;
	float STATIONARY_ARTY_RANGE;
	float AIRCRAFT_RATIO;
	float HIGH_RANGE_UNITS_RATIO;
	float FAST_UNITS_RATIO;

	int MIN_ENERGY_STORAGE;
	int MIN_METAL_STORAGE;
	int MAX_AIR_TARGETS;

	//! The number of different sides (side 0 = neutral will be added)
	int numberOfSides;

	//! The names of the different sides
	std::vector<std::string> sideNames;

	//! The start units (i.e. commanders) for the different sides
	std::list<int> startUnits;

	//! A list of units that shall be considered to be scouts
	std::list<int> scouts;

	//! A list of units that shall be considered to be transport units
	std::list<int> transporters;

	//! A list of units that shall be considered to be metal makers
	std::list<int> metalMakers;

	//! A list of units that shall be ignored (i.e. not assigned to any category and thus not used)
	std::list<int> ignoredUnits;

	//float KBOT_MAX_SLOPE;
	//float VEHICLE_MAX_SLOPE;
	//float HOVER_MAX_SLOPE;
	float NON_AMPHIB_MAX_WATERDEPTH;
	float METAL_ENERGY_RATIO;
	int MAX_DEFENCES;
	int MAX_STAT_ARTY;
	int MAX_AIR_BASE;
	bool AIR_ONLY_MOD;
	int MAX_STORAGE;
	int MAX_MEX_DISTANCE;
	int MAX_MEX_DEFENCE_DISTANCE;
	float MIN_METAL_MAKER_ENERGY;
	int MIN_FACTORIES_FOR_DEFENCES;
	int MIN_FACTORIES_FOR_STORAGE;
	float MIN_AIR_SUPPORT_EFFICIENCY;
	float MAX_COST_LIGHT_ASSAULT;
	float MAX_COST_MEDIUM_ASSAULT;
	float MAX_COST_HEAVY_ASSAULT;
	int MAX_ATTACKS;

	// combat behaviour
	float MIN_FALLBACK_TURNRATE; // units with lower turnrate will not try to fall back

	// internal
	float CLIFF_SLOPE;  // cells with greater slope will be considered to be cliffs
	int MAX_SECTOR_IMPORTANCE;

	// game specific
	int   LEARN_RATE;

	const static int  INCOME_SAMPLE_POINTS = 16; //!< Number of data poiunts used to calculate smoothed energy/metal income/surplus 

	/**
	 * open a file in springs data directory
	 * @param filename relative path of the file in the spring data dir
	 * @param mode mode file to open, see manpage of fopen
	 */
	std::string GetFileName(springLegacyAI::IAICallback* cb, const std::string& filename, const std::string& prefix = "", const std::string& suffix = "", bool write = false) const;
	std::string getUniqueName(springLegacyAI::IAICallback* cb, bool game, bool gamehash, bool map, bool maphash) const;

private:
	~AAIConfig(void);

	const UnitDef* GetUnitDef(AAI* ai, const std::string& name);
	int GetInt(AAI* ai, FILE* file);
	float GetFloat(AAI* ai, FILE* file);
	std::string GetString(AAI* ai, FILE* file);

	int CONSTRUCTION_TIMEOUT;
	float WATER_MAP_RATIO;
	float LAND_WATER_MAP_RATIO;
	float LIGHT_ASSAULT_RATIO;
	int MIN_FACTORIES_FOR_RADAR_JAMMER;
//	int MAX_MEX_DEFENCE_COST;
	int MAX_METAL_MAKERS;
	float MIN_SECTOR_THREAT;
//	float SHIP_MIN_WATERDEPTH;
	int MIN_AIR_ATTACK_COST;
	int MAX_BUILDERS;
	int MIN_ASSISTANCE_BUILDSPEED;
	int AIR_DEFENCE;
	float MEDIUM_ASSAULT_RATIO;
	float HEAVY_ASSAULT_RATIO;
	float SUPER_HEAVY_ASSAULT_RATIO;
	int MIN_SUBMARINE_WATERLINE;

};

extern AAIConfig *cfg;


#endif

