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
#include "AAIMapRelatedTypes.h"
#include "AAIMapTypes.h"
#include "AAIThreatMap.h"
#include "AAIUnitTypes.h"
#include "AAISector.h"
#include "System/float3.h"

#include <vector>
#include <list>
#include <string>

class AAI;

class AAIMap
{
	friend AAIScoutedUnitsMap;

public:
	AAIMap(AAI *ai, int xMapSize, int yMapSize, int losMapResolution);
	~AAIMap(void);

	//! @brief Returns the map type
	const AAIMapType& GetMapType() const { return s_mapType; }

	//! @brief Returns the map containing the sectors
	SectorMap& GetSectorMap() { return m_sectorMap; }

	//! @brief Returns max distance (in sectors) a sector can have to base
	int GetMaxSectorDistanceToBase() const { return (xSectors + ySectors - 2); }

	//! @brief Return the buffer storing unit ids of all units that are currently within line of sight
	std::vector<int>& UnitsInLOS() { return m_unitsInLOS; }

	//! @brief Returns the approximated map coordinates of the center of the enemy base (determined based on scouted enemy buildings)
	const MapPos& GetCenterOfEnemyBase() const { return m_centerOfEnemyBase; }

	//! @brief Returns the distance to the estimated center of enemy base
	float GetDistanceToCenterOfEnemyBase(const float3& position) const;

	//! @brief Converts given position to final building position for the given unit type
	void ConvertPositionToFinalBuildsite(float3& buildsite, const UnitFootprint& footprint) const;

	//! @brief Returns the corresponing build map position (for a given unit map position)
	static MapPos ConvertToBuildMapPosition(const float3& position) { return MapPos(position.x/SQUARE_SIZE, position.z/SQUARE_SIZE); }

	//! @brief Returns the corresponing map position (for a given build map position)
	static float3 ConvertToMapPosition(const MapPos& mapPos) { return float3(static_cast<float>(mapPos.x * SQUARE_SIZE), 0.0f, static_cast<float>(mapPos.y * SQUARE_SIZE)); }

	//! @brief Returns true if the given sector is a neighbour to the current base
	bool IsSectorBorderToBase(int x, int y) const;

	//! @brief Returns continent id with respect to the unit's movement type (e.g. ground (=non amphibious) unit being in shallow water will return id of nearest land continent)
	int DetermineSmartContinentID(float3 pos, const AAIMovementType& moveType) const;

	//! @brief Returns whether continent to which given sector mainly belongs is sea 
	bool IsSectorOnWaterContinent(const AAISector* sector) const { return s_continents[sector->GetContinentID()].water; }

	//! @brief Returns whether the position is located on a small continent (meant to detect "ponds" or "small islands")
	bool LocatedOnSmallContinent(const float3& pos) const 
	{ 
		return static_cast<float>(s_continents[s_continentMap.GetContinentID(pos)].size) < 0.25f * (s_landContinentSizeStatistics.GetAvgValue() + s_seaContinentSizeStatistics.GetAvgValue());
	}

	//! @brief Returns the id of continent the given position belongs to
	static int GetContinentID(const float3& pos) { return s_continentMap.GetContinentID(pos); }

	//! @brief Returns the number of continents
	static int GetNumberOfContinents() { return s_continents.size(); }

	//! @brief Determines the total number of spotted (= currently known) enemy buildings on land / sea
	void DetermineSpottedEnemyBuildingsOnContinentType(int& enemyBuildingsOnLand, int& enemyBuildingsOnSea) const;

	//! @brief Returns a bitmask storing which movement types are suitable for the map type
	uint32_t GetSuitableMovementTypesForMap() const { return GetSuitableMovementTypes(s_mapType); }

	//! @brief Returns the sector in which the given position lies (nullptr if out of sector map -> e.g. aircraft flying outside of the map) 
	AAISector* GetSectorOfPos(const float3& position);

	//! @brief Returns sector index for given position
	static SectorIndex GetSectorIndex(const float3& position) { return SectorIndex(position.x/xSectorSize, position.z/ySectorSize); }

	//! @brief Returns distance to closest edge of the map (in unit map coordinates)
	float GetEdgeDistance(const float3& pos) const;

	//! @brief Searches the given number of tries for a random builsite in the given area, returns ZeroVector if none found 
	BuildSite DetermineRandomBuildsite(UnitDefId unitDefId, int xStart, int xEnd, int yStart, int yEnd, int tries) const;

	//! @brief Searches for a buildsite close to the given unit; returns ZeroVector if none found
	BuildSite FindBuildsiteCloseToUnit(UnitDefId buildingDefId, UnitId unitId) const;

	//! @brief Searches for a buildiste in given sector starting from top left corner 
	BuildSite DetermineBuildsiteInSector(UnitDefId buildingDefId, const AAISector* sector) const;

	//! @brief Searches for a buildsite that is preferably elevated with respect to its surroundings and not close to the map edges
	BuildSite DetermineElevatedBuildsite(UnitDefId buildingDefId, int xStart, int xEnd, int yStart, int yEnd, float range) const;

	//! @brief Determines the most suitable buidliste for the given static defence in the given sector (returns ZeroVector if no buildsite found)
	float3 DetermineBuildsiteForStaticDefence(UnitDefId staticDefence, const AAISector* sector, const AAITargetType& targetType, float terrainModifier) const;

	//! @brief Updates buildmap & defence map (for static defences) and building data of target sector 
	//!        Return true if building will be placed at a valid position, i.e. inside sectors
	bool InitBuilding(const springLegacyAI::UnitDef *def, const float3& position);

	//! @brief Updates the buildmap: (un)block cells + insert/remove spaces (factory exits get some extra space)
	void UpdateBuildMap(const float3& buildPos, const springLegacyAI::UnitDef *def, bool block);

	// returns number of cells with big slope
	int GetCliffyCells(int xPos, int yPos, int xSize, int ySize) const;

	//! @brief Triggers an update of the current units in LOS if there are enough frames since the last update or it is enforced
	void CheckUnitsInLOSUpdate(bool forceUpdate = false);
	
	//! @brief Returns whether given unit is still known to be at given position (used to detect buildings that have been destroyed while not within LOS)
	bool CheckPositionForScoutedUnit(const float3& position, UnitId unitId);

	//! @brief Returns whether given position lies within current LOS
	bool IsPositionInLOS(const float3& position) const;

	//! @brief Returns whether given position lies within map (e.g. aircraft may leave map)
	bool IsPositionWithinMap(const float3& position) const;

	//! @brief Returns whether a water tile belonging to an ocean (i.e. large water continent) lies within the given rectangle
	bool IsConnectedToOcean(int xStart, int xEnd, int yStart, int yEnd) const;

	//! @brief Returns position of first enemy building found in the part of the map (in build map coordinates)
	float3 DeterminePositionOfEnemyBuildingInSector(int xStart, int xEnd, int yStart, int yEnd) const;

	//! @brief Decreases the lost units and updates the the "center of gravity" of the enemy base(s)
	void UpdateSectors(AAIThreatMap *threatMap);

	//! @brief Checks for new neighbours (and removes old ones if necessary)
	void UpdateNeighbouringSectors(std::vector< std::list<AAISector*> >& sectorsInDistToBase);

	//! @brief Adds or removes a defence buidling to/from the defence map
	void AddOrRemoveStaticDefence(const float3& position, UnitDefId defence, bool addDefence);

	//! @brief Determines to which location a given scout schould be sent to next
	float3 GetNewScoutDest(UnitId scoutUnitId);

	//! @brief Returns a sector to proceed with attack (nullptr if none found)
	const AAISector* DetermineSectorToContinueAttack(const AAISector *currentSector, const MobileTargetTypeValues& targetTypeOfUnits, AAIMovementType moveTypeOfUnits) const;

	//! Maximum squared distance on map in unit coordinates (i.e. from one corner to the other, xSize*xSize+ySize*ySize)
	static float s_maxSquaredMapDist; 

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
	static float s_landTilesRatio;

	//! Ratio of water tiles
	static float s_waterTilesRatio;

	//! Number of metal spots in sea
	static int s_metalSpotsInSea;

	//! Number of metal spots on land
	static int s_metalSpotsOnLand;

	//! Indicates if map is considered to be a metal map (i.e. exctractors can be built anywhere)
	static bool s_isMetalMap;

	//! The map storing which sector has been occupied by what team
	static AAITeamSectorMap s_teamSectorMap;

	//! The buildmap stores the type/occupation status of every cell;
	static std::vector<BuildMapTileType> s_buildmap;

	static constexpr int ignoreContinentID = -1;

private:
	//! @brief Updates spotted enemy buildings/units on the map (incl. data per sector)
	void UpdateEnemyUnitsInLOS();

	//! @brief Updates own/allied buildings/units on the map (in each sector)
	void UpdateFriendlyUnitsInLos();

	//! @brief Updates enemy buildings/enemy combat power in sectors based on scout map entris updated by UpdateEnemyUnitsInLOS()
	void UpdateEnemyScoutingData();

	//! @brief Helper function to check if the given building may be constructed at the given map position
	BuildSite CheckIfSuitableBuildSite(const UnitFootprint& footprint, const springLegacyAI::UnitDef* unitDef, const MapPos& mapPos) const;

	//! @brief Converts the given position (in map coordinates) to a position in buildmap coordinates
	void Pos2BuildMapPos(float3* position, const springLegacyAI::UnitDef* def) const;

	// krogothe's metal spot finder
	void DetectMetalSpots();

	//! @brief Returns descriptor for map type (used to save map type)
	const char* GetMapTypeString(const AAIMapType& mapType) const;

	//! @brief Returns which movement types are suitable for the given map type
	uint32_t GetSuitableMovementTypes(const AAIMapType& mapType) const;

	//! @brief Determine the type of every map tile (e.g. water, flat. cliff) and calculates the plateue map
	void AnalyseMap();

	//! @brief Determines the type of map
	void DetermineMapType();

	// calculates learning effect
	void UpdateLearningData();

	//! @brief Read the learning data for this map (or initialize with defualt data if none are available)
	void ReadMapLearnFile();

	//! 
	void InitContinents();

	//! @brief Reads continent data from given cache file (returns whether successful)
	bool ReadContinentFile(const std::string& filename);

	// reads map cache file (and creates new one if necessary)
	// loads mex spots, cliffs etc. from file or creates new one
	void ReadMapCacheFile();

	//! @brief Returns whether x/y specify a valid sector
	bool IsValidSector(const SectorIndex& index) const { return( (index.x >= 0) && (index.y >= 0) && (index.x < xSectors) && (index.y < ySectors) ); }

	//! @brief Returns true if buildmap allows construction of unit with given footprint at goven position
	bool CanBuildAt(const MapPos& mapPos, const UnitFootprint& size) const;

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
	
	//! @brief Returns position (in unit coordinates) for given position (in buildmap coordinates) and footprint
	float3 ConvertMapPosToUnitPos(const MapPos& mapPos, const UnitFootprint& footprint) const
	{
		// shift to center of building and convert to higher resolution
		return float3(	static_cast<float>( SQUARE_SIZE * (mapPos.x + footprint.xSize/2)),
						0.0f,
						static_cast<float>( SQUARE_SIZE * (mapPos.y + footprint.ySize/2)) );
	}

	std::string LocateMapLearnFile() const;
	std::string LocateMapCacheFile() const;

	AAI *ai;

	//! The sectors of the map
	SectorMap          m_sectorMap;

	//! Used for scouting, stores all friendly/enemy units with current line of sight
	std::vector<int>   m_unitsInLOS;

	//! Stores the defId of the building or combat unit placed on that cell (0 if none), same resolution as los map
	AAIScoutedUnitsMap m_scoutedEnemyUnitsMap;

	//! The number of scouted enemy units on the given continent
	std::vector<int>   m_buildingsOnContinent;

	//! Approximate center of enemy base in build map coordinates (not reliable if enemy buldings are spread over map)
	MapPos             m_centerOfEnemyBase;

	//! The frame in which the last update of the units in LOS has been performed
	int                m_lastLOSUpdateInFrame;

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// static (shared with other ai players)
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//! The defence maps (storing combat power by static defences vs the different mobile target types)
	static AAIDefenceMaps s_defenceMaps;

	//! Stores the id of the continent every tiles belongs to and additional information about continents
	static AAIContinentMap s_continentMap;

	//! An array storing the detected continents on the map
	static std::vector<AAIContinent> s_continents;

	//! The map type
	static AAIMapType s_mapType;

	static int losMapResolution;				// resolution of the LOS map
	static int xLOSMapSize, yLOSMapSize;		// x and y size of the LOS map
	static int xDefMapSize, yDefMapSize;		// x and y size of the defence maps (1/4 resolution of map)
	static std::list<AAIMetalSpot> metal_spots;

	static std::vector<int>   blockmap;		// number of buildings which ordered a cell to blocked
	static std::vector<float> plateau_map;	// positive values indicate plateaus, same resolution as continent map 1/4 of resolution of blockmap/buildmap
	static std::vector<int>   ship_movement_map;	// movement maps for different categories, 1/4 of resolution of blockmap/buildmap
	static std::vector<int>   kbot_movement_map;
	static std::vector<int>   vehicle_movement_map;
	static std::vector<int>   hover_movement_map;

	//! Minimum, maximum, and average size (in tiles) of land continents
	static StatisticalData s_landContinentSizeStatistics;

	//! Minimum, maximum, and average size (in tiles) of land continents
	static StatisticalData s_seaContinentSizeStatistics;
};

#endif

