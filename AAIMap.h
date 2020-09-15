// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_MAP_H
#define AAI_MAP_H

#include "aidef.h"
#include "AAITypes.h"
#include "AAIMapTypes.h"
#include "AAIUnitTypes.h"
#include "AAISector.h"
#include "System/float3.h"

#include <vector>
#include <list>
#include <string>
using namespace std;

class AAI;

namespace springLegacyAI {
	struct UnitDef;
}
using namespace springLegacyAI;

class AAIMap
{
public:
	AAIMap(AAI *ai);
	~AAIMap(void);

	void Init();

	//! @brief Returns the map type
	const AAIMapType& GetMapType() const { return s_mapType; }

	//! @brief Returns max distance (in sectors) a sector can have to base
	int GetMaxSectorDistanceToBase() const { return (xSectors + ySectors - 2); }

	//! @brief Converts given position to final building position for the given unit type
	void Pos2FinalBuildPos(float3 *pos, const UnitDef *def) const;

	//! @brief Returns whether x/y specify a valid sector
	bool IsValidSector(int x, int y) const { return( (x >= 0) && (y >= 0) && (x < xSectors) && (y < ySectors) ); }

	//! @brief Returns the id of continent the cell belongs to
	int GetContinentID(int x, int y) const;

	//! @brief Returns the id of continent the given position belongs to
	int GetContinentID(const float3& pos) const;

	//! @brief Returns whether continent to which given sector mainly belongs is sea 
	bool IsSectorOnWaterContinent(const AAISector* sector) const { return continents[sector->continent].water; }

	//! @brief Returns whether the position is located on a small continent (meant to detect "ponds" or "small islands")
	bool LocatedOnSmallContinent(const float3& pos) { return (continents[GetContinentID(pos)].size < (avg_land_continent_size + avg_water_continent_size)/4); }

	//! @brief Returns continent id with respect to the unit's movement type (e.g. ground (=non amphibious) unit being in shallow water will return id of nearest land continent)
	int DetermineSmartContinentID(float3 pos, const AAIMovementType& moveType) const;

	//! @brief Returns a bitmask storing which movement types are suitable for the map type
	uint32_t GetSuitableMovementTypesForMap() const { return GetSuitableMovementTypes(s_mapType); }

	//! @brief Returns whether the given sector is already occupied by another AAI player of the same team
	bool IsAlreadyOccupiedByOtherAAI(const AAISector* sector) const { return (team_sector_map[sector->x][sector->y] != -1) && (team_sector_map[sector->x][sector->y] != m_myTeamId); }

	//! @brief Returns the sector in which the given position lies (nullptr if out of sector map -> e.g. aircraft flying outside of the map) 
	AAISector* GetSectorOfPos(const float3& pos);

	float GetEdgeDistance(float3 *pos);

	//! @brief Returns the maximum number of units lost in any sector of the map
	float GetMaximumNumberOfLostUnits() const;

	float3 GetRandomBuildsite(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, int tries, bool water = false);

	//!  @brief Searches for a buildiste in given sector starting from top left corner 
	float3 DetermineBuildsiteInSector(UnitDefId buildingDefId, const AAISector* sector) const;

	// prefer buildsites that are on plateus and not too close to the edge of the map
	float3 GetRadarArtyBuildsite(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, float range, bool water);

	// return rating of a the best buidliste fpr a def. building vs category within specified rect (and stores pos in pointer)
	float GetDefenceBuildsite(float3 *buildPos, const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, const AAITargetType& targetType, float terrainModifier, bool water) const;

	//! @brief Updates buildmap & defence map (for static defences) and building data of target sector 
	//!        Return true if building will be placed at a valid position, i.e. inside sectors
	bool InitBuilding(const UnitDef *def, const float3& position);

	//! @brief Updates the buildmap: (un)block cells + insert/remove spaces (factory exits get some extra space)
	void UpdateBuildMap(const float3& buildPos, const UnitDef *def, bool block);

	// returns number of cells with big slope
	int GetCliffyCells(int xPos, int yPos, int xSize, int ySize) const;

	// updates spotted ennemy/ally buildings/units on the map
	void UpdateEnemyUnitsInLOS();

	void UpdateFriendlyUnitsInLos();

	// updates enemy buildings/enemy stat. combat strength in sectors based on scouted_buildings_map
	void UpdateEnemyScoutingData();

	//! @brief Returns position of first enemy building found in the part of the map (in build map coordinates)
	float3 DeterminePositionOfEnemyBuildingInSector(int xStart, int xEnd, int yStart, int yEnd) const;

	void UpdateSectors();

	//! @brief Checks for new neighbours (and removes old ones if necessary)
	void UpdateNeighbouringSectors(std::vector< std::list<AAISector*> >& sectorsInDistToBase);

	//! @brief Adds a defence buidling to the defence map
	void AddStaticDefence(const float3& position, UnitDefId defence);

	//! @brief Removes a defence buidling from the defence map
	void RemoveDefence(const float3& pos, UnitDefId defence);

	//! @brief Determines to which location a given scout schould be sent to next
	float3 GetNewScoutDest(UnitId scoutUnitId);

	//! @brief Returns a sector to proceed with attack (nullptr if none found)
	const AAISector* DetermineSectorToContinueAttack(const AAISector *currentSector, const MobileTargetTypeValues& targetTypeOfUnits, AAIMovementType moveTypeOfUnits) const;

	//! @brief Returns the sector which is the highest rated attack target (or nullptr if none found)
	const AAISector* DetermineSectorToAttack(const std::vector<float>& globalCombatPower, const std::vector< std::vector<float> >& continentCombatPower, const MobileTargetTypeValues& assaultGroupsOfType) const;

	//! The sectors of the map
	std::vector< std::vector<AAISector> > m_sector;

	// used for scouting, used to get all friendly/enemy units in los
	std::vector<int> unitsInLOS;

	//! Maximum squared distance on map in unit coordinates (i.e. from one corner to the other, xSize*xSize+ySize*ySize)
	static float maxSquaredMapDist; 

	//! x and y size of the map (unit coordinates)
	static int xSize, ySize;

	//! x and y size of the map (map coordinates, i.e. map tiles)
	static int xMapSize, yMapSize;

	//! Number of sectors in x/y direction
	static int xSectors, ySectors;

	//! Size of sectors (in unit coordinates)
	static int xSectorSize, ySectorSize;

	//! Size of sectors (in map coordinates, i.e. tiles = 1/8 xSize/ySize)
	static int xSectorSizeMap, ySectorSizeMap;

	//! Ration of land tiles
	static float land_ratio;

	//! Ratio of water tiles
	static float water_ratio;

	//! Number of metal spots in sea
	static int water_metal_spots;

	//! Number of metal spots on land
	static int land_metal_spots;

	//! Indicates if map is considered to be a metal map (i.e. exctractors can be built anywhere)
	static bool metalMap;
	
	static vector< vector<int> > team_sector_map;	// stores the number of ai player which has taken that sector (-1 if none)
											// this helps preventing aai from expanding into sectors of other aai players

	//! The buildmap stores the type/occupation status of every cell;
	static std::vector<BuildMapTileType> m_buildmap;

	static vector<AAIContinent> continents;
	static int avg_water_continent_size;

	static constexpr int ignoreContinentID = -1;

private:

	// defence maps
	vector<float> defence_map;	//  ground/sea defence map has 1/2 of resolution of blockmap/buildmap
	vector<float> air_defence_map; // air defence map has 1/2 of resolution of blockmap/buildmap
	vector<float> submarine_defence_map; // submarine defence map has 1/2 of resolution of blockmap/buildmap
	
	//! @brief Converts the given position (in map coordinates) to a position in buildmap coordinates
	void Pos2BuildMapPos(float3* position, const UnitDef* def) const;

	//! Stores the defId of the building or combat unit placed on that cell (0 if none), same resolution as los map
	std::vector<int> m_scoutedEnemyUnitsMap;

	//! Stores the frame of the last update of a cell (same resolution as los map)
	std::vector<int> m_lastLOSUpdateInFrameMap;

	// krogothe's metal spot finder
	void DetectMetalSpots();

	// determines type of map (land, land/water or water map)
	void DetectMapType();

	//! @brief Returns descriptor for map type (used to save map type)
	const char* GetMapTypeString(const AAIMapType& mapType) const;

	//! @brief Returns which movement types are suitable for the given map type
	uint32_t GetSuitableMovementTypes(const AAIMapType& mapType) const;

	void CalculateWaterRatio();

	// calculates which parts of the are connected
	void CalculateContinentMaps();

	// determines water, high slopes, defence map
	void AnalyseMap();

	// calculates learning effect
	void Learn();

	//! @brief Read the learning data for this map (or initialize with defualt data if none are available)
	void ReadMapLearnFile();

	// reads continent cache file (and creates new one if necessary)
	void ReadContinentFile();

	// reads map cache file (and creates new one if necessary)
	// loads mex spots, cliffs etc. from file or creates new one
	void ReadMapCacheFile();

	//! @brief Returns true if buildmap allows construction of unit with given footprint at goven position
	bool CanBuildAt(int xPos, int yPos, const UnitFootprint& size, bool water = false) const;

	//! @brief Blocks/unblocks map tiles (to prevent AAI from packing buildings too close to each other)
	//!        Automatically clamps given values to map size (avoids running over any map edges)
	void BlockTiles(int xPos, int yPos, int width, int height, bool block);

	//! @brief Prevents AAI from building too many buildings in a row by adding blocking spaces if necessary
	void CheckRows(int xPos, int yPos, int xSize, int ySize, bool add);

	//! @brief Returns the size which is needed for this building (building size + exit for factories)
	UnitFootprint DetermineRequiredFreeBuildspace(UnitDefId unitDefId) const;

	//! @brief Returns distance to closest edge of the map (in build_map coordinates)
	int GetEdgeDistance(int xPos, int yPos) const;

	//! @brief Occupies/frees the given cells of the buildmap
	void ChangeBuildMapOccupation(int xPos, int yPos, int xSize, int ySize, bool occupy);

public:
	void BuildMapPos2Pos(float3 *pos, const UnitDef* def) const;

private:
	std::string LocateMapLearnFile() const;
	std::string LocateMapCacheFile() const;

	AAI *ai;

	//! Id of the team (not ally team) of the AAI instance
	int m_myTeamId;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// static (shared with other ai players)
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! The map type
	static AAIMapType s_mapType;

	static int aai_instances;	// how many AAI instances have been initialized

	static int losMapRes;				// resolution of the LOS map
	static int xLOSMapSize, yLOSMapSize;		// x and y size of the LOS map
	static int xDefMapSize, yDefMapSize;		// x and y size of the defence maps (1/4 resolution of map)
	static int xContMapSize, yContMapSize;		// x and y size of the continent maps (1/4 resolution of map)
	static std::list<AAIMetalSpot> metal_spots;
	static float flat_land_ratio;
	static vector<int> blockmap;		// number of buildings which ordered a cell to blocked
	static vector<float> plateau_map;	// positive values indicate plateaus, same resolution as continent map 1/4 of resolution of blockmap/buildmap
	static vector<int> continent_map;	// id of continent a cell belongs to

	static vector<int> ship_movement_map;	// movement maps for different categories, 1/4 of resolution of blockmap/buildmap
	static vector<int> kbot_movement_map;
	static vector<int> vehicle_movement_map;
	static vector<int> hover_movement_map;
	static int land_continents;
	static int water_continents;

	static int avg_land_continent_size;
	static int max_land_continent_size;
	static int max_water_continent_size;
	static int min_land_continent_size;
	static int min_water_continent_size;
};

#endif

