// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "AAIConfig.h"
#include "AAI.h"
#include "System/SafeCStrings.h"
#include "System/StringUtil.h"

// all paths
#define CFG_PATH "cfg/"
#define MOD_CFG_PATH CFG_PATH "mod/"
#define CONFIG_SUFFIX ".cfg"
#define GENERAL_CFG_FILE "general" CONFIG_SUFFIX


#include "LegacyCpp/UnitDef.h"
using namespace springLegacyAI;


static bool IsFSGoodChar(const char c) {

	if ((c >= '0') && (c <= '9')) {
		return true;
	} else if ((c >= 'a') && (c <= 'z')) {
		return true;
	} else if ((c >= 'A') && (c <= 'Z')) {
		return true;
	} else if ((c == '.') || (c == '_') || (c == '-')) {
		return true;
	}

	return false;
}

// declaration is in aidef.h
std::string MakeFileSystemCompatible(const std::string& str) {

	std::string cleaned = str;

	for (std::string::size_type i=0; i < cleaned.size(); i++) {
		if (!IsFSGoodChar(cleaned[i])) {
			cleaned[i] = '_';
		}
	}

	return cleaned;
}


int AAIConfig::GetInt(AAI* ai, FILE* file)
{
	int val = 0;
	int res = fscanf(file, "%i", &val);
	if (res != 1) {
		ai->Log("Error while parsing config");
	}
	return val;
}

std::string AAIConfig::GetString(AAI* ai, FILE* file)
{
	char buf[128];
	buf[0] = 0; //safety!
	int res = fscanf(file, "%s", buf);
	if (res != 1) {
		ai->Log("Error while parsing config");
	}
	return std::string(buf);
}

float AAIConfig::GetFloat(AAI* ai, FILE* file)
{
	float val = 0.0f;
	int res = fscanf(file, "%f", &val);
	if (res != 1) {
		ai->Log("Error while parsing config");
	}
	return val;
}


AAIConfig::AAIConfig(void)
{
	numberOfSides = 2;
	MIN_ENERGY = 18;  // min energy make value to be considered beeing a power plant
	MAX_UNITS = 5000;
	MAX_SCOUTS = 4;
	MAX_SECTOR_IMPORTANCE = 6;
	MAX_XROW = 16;
	MAX_YROW = 16;
	X_SPACE = 12;
	Y_SPACE = 12;
	MAX_GROUP_SIZE = 12;
	MAX_AIR_GROUP_SIZE = 4;
	MAX_ANTI_AIR_GROUP_SIZE = 4;
	MAX_SUBMARINE_GROUP_SIZE = 4;
	MAX_NAVAL_GROUP_SIZE = 4;
	MAX_ARTY_GROUP_SIZE = 4;
	MAX_BUILDERS = 50;
	MAX_BUILDERS_PER_TYPE = 5;
	MAX_FACTORIES_PER_TYPE = 3;
	MAX_BUILDQUE_SIZE = 12;
	MAX_ASSISTANTS = 4;
	MIN_ASSISTANCE_BUILDTIME = 15;
	MIN_ASSISTANCE_BUILDSPEED = 20;
	MAX_BASE_SIZE = 12;
	SCOUT_SPEED = 95.0;
	GROUND_ARTY_RANGE = 1000.0;
	SEA_ARTY_RANGE = 1300.0;
	HOVER_ARTY_RANGE = 1000.0;
	STATIONARY_ARTY_RANGE = 2000;
	AIR_DEFENCE = 8;
	MIN_ENERGY_STORAGE = 500;
	MIN_METAL_STORAGE = 100;
	MIN_AIR_ATTACK_COST = 150;
	MAX_AIR_TARGETS = 20;
	AIRCRAFT_RATIO = 0.2f;
	HIGH_RANGE_UNITS_RATIO = 0.3f;
	FAST_UNITS_RATIO = 0.2f;
	METAL_ENERGY_RATIO = 25;
	MAX_DEFENCES = 9;
	MIN_SECTOR_THREAT = 6;
	MAX_STAT_ARTY = 3;
	MAX_STORAGE = 6;
	MAX_AIR_BASE = 1;
	AIR_ONLY_MOD = false;
	MAX_METAL_MAKERS = 20;
	MIN_METAL_MAKER_ENERGY = 100;
	MAX_MEX_DISTANCE = 7;
	MAX_MEX_DEFENCE_DISTANCE = 5;
	MIN_FACTORIES_FOR_DEFENCES = 1;
	MIN_FACTORIES_FOR_STORAGE = 1;
	MIN_FACTORIES_FOR_RADAR_JAMMER = 2;
	MIN_AIR_SUPPORT_EFFICIENCY = 2.5f;
	MIN_SUBMARINE_WATERLINE = 15;
	MAX_ATTACKS = 3;

	NON_AMPHIB_MAX_WATERDEPTH = 15.0f;

	MAX_COST_LIGHT_ASSAULT = 0.025f;
	MAX_COST_MEDIUM_ASSAULT = 0.13f;
	MAX_COST_HEAVY_ASSAULT = 0.55f;

	LIGHT_ASSAULT_RATIO = 40.0f;
	MEDIUM_ASSAULT_RATIO = 30.0f;
	HEAVY_ASSAULT_RATIO = 25.0f;
	SUPER_HEAVY_ASSAULT_RATIO = 5.0f;

	MIN_FALLBACK_TURNRATE = 250.0f;

	LEARN_RATE = 5;
	CONSTRUCTION_TIMEOUT = 1500;
	CLIFF_SLOPE = 0.085f;
	WATER_MAP_RATIO = 0.8f;
	LAND_WATER_MAP_RATIO = 0.3f;
}

AAIConfig::~AAIConfig(void)
{
}

std::string AAIConfig::GetFileName(springLegacyAI::IAICallback* cb, const std::string& filename, const std::string& prefix, const std::string& suffix, bool write) const
{
	std::string name = prefix + MakeFileSystemCompatible(filename) + suffix;

	// this size equals the one used in "AIAICallback::GetValue(AIVAL_LOCATE_FILE_..."
	char buffer[2048];
	STRCPY_T(buffer, sizeof(buffer), name.c_str());
	if (write) {
		cb->GetValue(AIVAL_LOCATE_FILE_W, buffer);
	} else {
		cb->GetValue(AIVAL_LOCATE_FILE_R, buffer);
	}
	name.assign(buffer, sizeof(buffer));
	return name;
}

void CheckValueRange(float& x)
{
	if(x < 0.0f)
		x = 0.0f;
	
	if(x > 1.0f)
		x = 1.0f;
}

void ReadUnitNames(AAI *ai, FILE* file, std::list<int>& unitList, std::list< std::string >& unknownUnitsList)
{
	char lineBuffer[2048];
	fgets(lineBuffer, sizeof(lineBuffer), file);

	char* unitName = strtok(lineBuffer," \n");
	while (unitName != NULL)
	{
		const springLegacyAI::UnitDef* unitDef = ai->GetUnitDef(unitName);
		
		if(unitDef)
			unitList.push_back(unitDef->id);
		else
			unknownUnitsList.push_back(unitName);
		
		unitName = strtok(NULL, " \n");
	}
}

bool AAIConfig::LoadGameConfig(AAI *ai)
{
	MAX_UNITS = ai->GetAICallback()->GetMaxUnits();

	std::list<string> possibleConfigFilenames;
	possibleConfigFilenames.push_back(GetFileName(ai->GetAICallback(), ai->GetAICallback()->GetModHumanName(), MOD_CFG_PATH, CONFIG_SUFFIX));
	possibleConfigFilenames.push_back(GetFileName(ai->GetAICallback(), ai->GetAICallback()->GetModName(), MOD_CFG_PATH, CONFIG_SUFFIX));
	possibleConfigFilenames.push_back(GetFileName(ai->GetAICallback(), ai->GetAICallback()->GetModShortName(), MOD_CFG_PATH, CONFIG_SUFFIX));

	FILE* file(NULL);
	std::string configfile;
	for(const std::string& filename: possibleConfigFilenames)
	{
		file = fopen(filename.c_str(), "r");
		if (file != NULL) 
		{
			configfile = filename;
			break;
		}
	}

	if (file == NULL)
	{
		ai->Log("ERROR: Unable to find mod config file (required). Possible file names:\n");
		for(const auto& filename : possibleConfigFilenames)
			ai->Log("%s\n", filename.c_str());
		return false;
   	}

	std::list< std::string > unknownUnits;

	char keyword[64];

	bool errorOccurred = false;

	while(EOF != fscanf(file, "%s", keyword))
	{
		if(!strcmp(keyword,"SIDES")) {
			numberOfSides = GetInt(ai, file);
		}
		else if(!strcmp(keyword, "SIDE_NAMES")) 
		{
			sideNames.resize(numberOfSides+1);
			sideNames[0] = "Neutral";
			for(int i = 1; i <= numberOfSides; ++i) {
				sideNames[i] = GetString(ai, file);
			}
		}
		else if(!strcmp(keyword, "START_UNITS")) 
		{
			ReadUnitNames(ai, file, startUnits, unknownUnits);
		} 
		else if(!strcmp(keyword, "SCOUTS")) 
		{
			ReadUnitNames(ai, file, scouts, unknownUnits);
		}
		else if(!strcmp(keyword, "TRANSPORTERS"))
		{
			ReadUnitNames(ai, file, transporters, unknownUnits);
		}
		else if(!strcmp(keyword, "METAL_MAKERS"))
		{
			ReadUnitNames(ai, file, metalMakers, unknownUnits);
		}
		else if(!strcmp(keyword, "DONT_BUILD")) 
		{
			ReadUnitNames(ai, file, ignoredUnits, unknownUnits);
		}
		else if(!strcmp(keyword,"MIN_ENERGY")) {
			MIN_ENERGY = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_SCOUTS")) {
			MAX_SCOUTS = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_SECTOR_IMPORTANCE")) {
			MAX_SECTOR_IMPORTANCE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_XROW")) {
			MAX_XROW = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_YROW")) {
			MAX_YROW = GetInt(ai, file);
		} else if(!strcmp(keyword, "X_SPACE")) {
			X_SPACE = GetInt(ai, file);
		} else if(!strcmp(keyword, "Y_SPACE")) {
			Y_SPACE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_GROUP_SIZE")) {
			MAX_GROUP_SIZE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_AIR_GROUP_SIZE")) {
			MAX_AIR_GROUP_SIZE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_NAVAL_GROUP_SIZE")) {
			MAX_NAVAL_GROUP_SIZE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_SUBMARINE_GROUP_SIZE")) {
			MAX_SUBMARINE_GROUP_SIZE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_ANTI_AIR_GROUP_SIZE")) {
			MAX_ANTI_AIR_GROUP_SIZE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_ARTY_GROUP_SIZE")) {
			MAX_ARTY_GROUP_SIZE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MIN_FALLBACK_TURNRATE")) {
			MIN_FALLBACK_TURNRATE = GetFloat(ai, file);
		} else if(!strcmp(keyword, "MIN_AIR_SUPPORT_EFFICIENCY")) {
			MIN_AIR_SUPPORT_EFFICIENCY = GetFloat(ai, file);
		} else if(!strcmp(keyword, "MAX_BUILDERS")) {
			MAX_BUILDERS = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_BUILDQUE_SIZE")) {
			MAX_BUILDQUE_SIZE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_ASSISTANTS")) {
			MAX_ASSISTANTS = GetInt(ai, file);
		} else if(!strcmp(keyword, "MIN_ASSISTANCE_BUILDSPEED")) {
			MIN_ASSISTANCE_BUILDSPEED = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_BASE_SIZE")) {
			MAX_BASE_SIZE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_AIR_TARGETS")) {
			MAX_AIR_TARGETS = GetInt(ai, file);
		} else if(!strcmp(keyword, "MIN_AIR_ATTACK_COST")) {
			MIN_AIR_ATTACK_COST = GetInt(ai, file);
		} else if(!strcmp(keyword, "SCOUT_SPEED")) {
			SCOUT_SPEED = GetFloat(ai, file);
		} else if(!strcmp(keyword, "GROUND_ARTY_RANGE")) {
			GROUND_ARTY_RANGE = GetInt(ai, file);
		} else if(!strcmp(keyword, "SEA_ARTY_RANGE")) {
			SEA_ARTY_RANGE = GetFloat(ai, file);
		} else if(!strcmp(keyword, "HOVER_ARTY_RANGE")) {
			HOVER_ARTY_RANGE = GetFloat(ai, file);
		} else if(!strcmp(keyword, "STATIONARY_ARTY_RANGE")) {
			STATIONARY_ARTY_RANGE = GetFloat(ai, file);
		} else if(!strcmp(keyword, "MAX_BUILDERS_PER_TYPE")) {
			MAX_BUILDERS_PER_TYPE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_FACTORIES_PER_TYPE")) {
			MAX_FACTORIES_PER_TYPE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MIN_ASSISTANCE_BUILDTIME")) {
			MIN_ASSISTANCE_BUILDTIME = GetInt(ai, file);
		} else if(!strcmp(keyword, "AIR_DEFENCE")) {
			AIR_DEFENCE = GetInt(ai, file);
		} else if(!strcmp(keyword, "AIRCRAFT_RATIO")) {
			AIRCRAFT_RATIO = GetFloat(ai, file);
		} else if(!strcmp(keyword, "HIGH_RANGE_UNITS_RATIO")) {
			HIGH_RANGE_UNITS_RATIO = GetFloat(ai, file);
		} else if(!strcmp(keyword, "FAST_UNITS_RATIO")) {
			FAST_UNITS_RATIO = GetFloat(ai, file);
		} else if(!strcmp(keyword, "MAX_DEFENCES")) {
			MAX_DEFENCES = GetInt(ai, file);
		} else if(!strcmp(keyword, "MIN_SECTOR_THREAT")) {
			MIN_SECTOR_THREAT = GetFloat(ai, file);
		} else if(!strcmp(keyword, "MAX_STAT_ARTY")) {
			MAX_STAT_ARTY = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_AIR_BASE")) {
			MAX_AIR_BASE = GetInt(ai, file);
		} else if(!strcmp(keyword, "AIR_ONLY_MOD")) {
			AIR_ONLY_MOD = (bool)GetInt(ai, file);
		} else if(!strcmp(keyword, "METAL_ENERGY_RATIO")) {
			METAL_ENERGY_RATIO = GetFloat(ai, file);
		} else if(!strcmp(keyword, "NON_AMPHIB_MAX_WATERDEPTH")) {
			NON_AMPHIB_MAX_WATERDEPTH = GetFloat(ai, file);
		} else if(!strcmp(keyword, "MAX_METAL_MAKERS")) {
			MAX_METAL_MAKERS = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_STORAGE")) {
			MAX_STORAGE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MIN_METAL_MAKER_ENERGY")) {
			MIN_METAL_MAKER_ENERGY = GetFloat(ai, file);
		} else if(!strcmp(keyword, "MAX_MEX_DISTANCE")) {
			MAX_MEX_DISTANCE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_MEX_DEFENCE_DISTANCE")) {
			MAX_MEX_DEFENCE_DISTANCE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MIN_FACTORIES_FOR_DEFENCES")) {
			MIN_FACTORIES_FOR_DEFENCES = GetInt(ai, file);
		} else if(!strcmp(keyword, "MIN_FACTORIES_FOR_STORAGE")) {
			MIN_FACTORIES_FOR_STORAGE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MIN_FACTORIES_FOR_RADAR_JAMMER")) {
			MIN_FACTORIES_FOR_RADAR_JAMMER = GetInt(ai, file);
		} else if(!strcmp(keyword, "MIN_SUBMARINE_WATERLINE")) {
			MIN_SUBMARINE_WATERLINE = GetInt(ai, file);
		} else if(!strcmp(keyword, "MAX_ATTACKS")) {
			MAX_ATTACKS = GetInt(ai, file);
		} else {
			errorOccurred = true;
			break;
		}	
	}

	// ensure values are in range 0.0f to 1.0f
	CheckValueRange(AIRCRAFT_RATIO);
	CheckValueRange(HIGH_RANGE_UNITS_RATIO);
	CheckValueRange(FAST_UNITS_RATIO);

	if(errorOccurred)
	{
		ai->Log("Mod config file %s contains erroneous keyword: %s\n", configfile.c_str(), keyword);
		return false;
	}

	if(unknownUnits.size() > 0)
	{
		ai->Log("Warning: The following unknown units were found when loading the mod configuration:\n");
		for(auto unitName : unknownUnits)
			ai->Log("%s ", unitName.c_str());
		ai->Log("\n");
	}

	fclose(file);
	ai->Log("Mod config file %s loaded\n", configfile.c_str());
	return true;
}

bool AAIConfig::loadGeneralConfig(AAI& ai)
{
	// load general settings
	const std::string filename = GetFileName(ai.GetAICallback(), GENERAL_CFG_FILE, CFG_PATH);

	FILE* file = fopen(filename.c_str(), "r");

	if(file == NULL) {
		ai.Log("ERROR: Couldn't load general config file %s\n", filename.c_str());
		return false;
	}

	char keyword[50];
	bool errorOccurred = false;

	while(EOF != fscanf(file, "%s", keyword))
	{
		if(!strcmp(keyword, "LEARN_RATE")) {
			LEARN_RATE = GetInt(&ai, file);
		} else if(!strcmp(keyword, "WATER_MAP_RATIO")) {
			WATER_MAP_RATIO = GetFloat(&ai, file);
		} else if(!strcmp(keyword, "LAND_WATER_MAP_RATIO")) {
			LAND_WATER_MAP_RATIO = GetFloat(&ai, file);
		}
		else 
		{
			errorOccurred = true;
			break;
		}
	}

	fclose(file);

	if(errorOccurred) {
		ai.Log("General config file contains erroneous keyword %s\n", keyword);
		return false;
	}
	ai.Log("General config file loaded\n");
	return true;
}

const UnitDef* AAIConfig::GetUnitDef(AAI* ai, const std::string& name)
{
	const UnitDef* res = ai->GetAICallback()->GetUnitDef(name.c_str());
	if (res == NULL) {
		ai->Log("ERROR: loading unit - could not find unit %s\n", name.c_str());
	}
	return res;
}

std::string AAIConfig::getUniqueName(springLegacyAI::IAICallback* cb, bool game, bool gamehash, bool map, bool maphash) const
{
	std::string res;
	if (map) {
		if (!res.empty())
			res += "-";
		std::string mapName = MakeFileSystemCompatible(cb->GetMapName());
		mapName.resize(mapName.size() - 4); // cut off extension
		res += mapName;
	}
	if (maphash) {
		if (!res.empty())
			res += "-";
		res += IntToString(cb->GetMapHash(), "%x");
	}
	if (game) {
		if (!res.empty())
			res += "_";
		res += MakeFileSystemCompatible(cb->GetModHumanName());
	}
	if (gamehash) {
		if (!res.empty())
			res += "-";
		res += IntToString(cb->GetModHash(), "%x");
	}
	return res;
}

AAIConfig *cfg = new AAIConfig();
