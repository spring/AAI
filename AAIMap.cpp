// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "AAIMap.h"
#include "AAI.h"
#include "AAIBuildTable.h"
#include "AAIBrain.h"
#include "AAIConfig.h"
#include "AAISector.h"

#include "System/SafeUtil.h"
#include "LegacyCpp/UnitDef.h"

#include <inttypes.h>

using namespace springLegacyAI;

#define MAP_CACHE_PATH "cache/"

float AAIMap::maxSquaredMapDist;
int AAIMap::xSize;
int AAIMap::ySize;
int AAIMap::xMapSize;
int AAIMap::yMapSize;
int AAIMap::losMapRes;
int AAIMap::xLOSMapSize;
int AAIMap::yLOSMapSize;
int AAIMap::xDefMapSize;
int AAIMap::yDefMapSize;
int AAIMap::xContMapSize;
int AAIMap::yContMapSize;
int AAIMap::xSectors;
int AAIMap::ySectors;
int AAIMap::xSectorSize;
int AAIMap::ySectorSize;
int AAIMap::xSectorSizeMap;
int AAIMap::ySectorSizeMap;

std::list<AAIMetalSpot>  AAIMap::metal_spots;

int AAIMap::land_metal_spots;
int AAIMap::water_metal_spots;

float AAIMap::land_ratio;
float AAIMap::flat_land_ratio;
float AAIMap::water_ratio;

bool AAIMap::metalMap;
AAIMapType AAIMap::s_mapType;

std::vector< std::vector<int> > AAIMap::team_sector_map;
std::vector<BuildMapTileType> AAIMap::m_buildmap;
std::vector<int> AAIMap::blockmap;
std::vector<float> AAIMap::plateau_map;
std::vector<int> AAIMap::continent_map;

std::vector<AAIContinent> AAIMap::continents;
int AAIMap::land_continents;
int AAIMap::water_continents;
int AAIMap::avg_land_continent_size;
int AAIMap::avg_water_continent_size;
int AAIMap::max_land_continent_size;
int AAIMap::max_water_continent_size;
int AAIMap::min_land_continent_size;
int AAIMap::min_water_continent_size;

AAIMap::AAIMap(AAI *ai)
{
	this->ai = ai;
	m_myTeamId = ai->GetAICallback()->GetMyTeam();
}

AAIMap::~AAIMap(void)
{
	// delete common data only if last AAI instance is deleted
	if(ai->GetNumberOfAAIInstances() == 0)
	{
		ai->Log("Saving map learn file\n");

		Learn();

		const std::string mapLearn_filename = LocateMapLearnFile();

		// save map data
		FILE *save_file = fopen(mapLearn_filename.c_str(), "w+");

		fprintf(save_file, "%s \n", MAP_LEARN_VERSION);

		for(int y = 0; y < ySectors; y++)
		{
			for(int x = 0; x < xSectors; x++)
				m_sector[x][y].SaveDataToFile(save_file);
			
			fprintf(save_file, "\n");
		}

		fclose(save_file);

		m_buildmap.clear();
		blockmap.clear();
		plateau_map.clear();
		continent_map.clear();
	}

	defence_map.clear();
	air_defence_map.clear();
	submarine_defence_map.clear();

	m_scoutedEnemyUnitsMap.clear();
	m_lastLOSUpdateInFrameMap.clear();

	unitsInLOS.clear();
}

void AAIMap::Init()
{
	// all static vars are only initialized by the first AAI instance
	if(ai->GetAAIInstance() == 1)
	{
		// get size
		xMapSize = ai->GetAICallback()->GetMapWidth();
		yMapSize = ai->GetAICallback()->GetMapHeight();

		xSize = xMapSize * SQUARE_SIZE;
		ySize = yMapSize * SQUARE_SIZE;

		maxSquaredMapDist = xSize*xSize + ySize*ySize;

		losMapRes = std::sqrt(ai->GetAICallback()->GetLosMapResolution());
		xLOSMapSize = xMapSize / losMapRes;
		yLOSMapSize = yMapSize / losMapRes;

		xDefMapSize = xMapSize / 4;
		yDefMapSize = yMapSize / 4;

		xContMapSize = xMapSize / 4;
		yContMapSize = yMapSize / 4;

		// calculate number of sectors
		xSectors = floor(0.5f + ((float) xMapSize)/AAIConstants::sectorSize);
		ySectors = floor(0.5f + ((float) yMapSize)/AAIConstants::sectorSize);

		// calculate effective sector size
		xSectorSizeMap = floor( ((float) xMapSize) / ((float) xSectors) );
		ySectorSizeMap = floor( ((float) yMapSize) / ((float) ySectors) );

		xSectorSize = xSectorSizeMap * SQUARE_SIZE;
		ySectorSize = ySectorSizeMap * SQUARE_SIZE;

		m_buildmap.resize(xMapSize*yMapSize);
		blockmap.resize(xMapSize*yMapSize, 0);
		continent_map.resize(xContMapSize*yContMapSize, -1);
		plateau_map.resize(xContMapSize*yContMapSize, 0.0f);

		// create map that stores which aai player has occupied which sector (visible to all aai players)
		team_sector_map.resize(xSectors, std::vector<int>(ySectors, -1) );

		ReadContinentFile();

		ReadMapCacheFile();
	}

	ai->Log("Map size: %i x %i    LOS map size: %i x %i  (los res: %i)\n", xMapSize, yMapSize, xLOSMapSize, yLOSMapSize, losMapRes);

	m_sector.resize(xSectors, std::vector<AAISector>(ySectors));

	for(int x = 0; x < xSectors; ++x)
	{
		for(int y = 0; y < ySectors; ++y)
			// provide ai callback to sectors & set coordinates of the sectors
			m_sector[x][y].Init(ai, x, y);
	}

	// add metalspots to their sectors
	for(std::list<AAIMetalSpot>::iterator spot = metal_spots.begin(); spot != metal_spots.end(); ++spot)
	{
		AAISector* sector = GetSectorOfPos(spot->pos);

		if(sector)
			sector->AddMetalSpot(&(*spot));
	}

	ReadMapLearnFile();

	// for scouting
	m_scoutedEnemyUnitsMap.resize(xLOSMapSize*yLOSMapSize, 0);
	m_lastLOSUpdateInFrameMap.resize(xLOSMapSize*yLOSMapSize, 0);

	unitsInLOS.resize(cfg->MAX_UNITS, 0);

	// create defence
	defence_map.resize(xDefMapSize*yDefMapSize, 0.0f);
	air_defence_map.resize(xDefMapSize*yDefMapSize, 0.0f);
	submarine_defence_map.resize(xDefMapSize*yDefMapSize, 0.0f);

	// for log file
	ai->Log("Map: %s\n",ai->GetAICallback()->GetMapName());
	ai->Log("Maptype: %s\n", s_mapType.GetName().c_str());
	ai->Log("Land / water ratio: : %f / %f\n", land_ratio, water_ratio);
	ai->Log("Mapsize is %i x %i\n", ai->GetAICallback()->GetMapWidth(),ai->GetAICallback()->GetMapHeight());
	ai->Log("%i sectors in x direction\n", xSectors);
	ai->Log("%i sectors in y direction\n", ySectors);
	ai->Log("x-sectorsize is %i (Map %i)\n", xSectorSize, xSectorSizeMap);
	ai->Log("y-sectorsize is %i (Map %i)\n", ySectorSize, ySectorSizeMap);
	ai->Log( _STPF_ " metal spots found (%i are on land, %i under water) \n \n", metal_spots.size(), land_metal_spots, water_metal_spots);
	ai->Log( _STPF_ " continents found on map\n", continents.size());
	ai->Log("%i land and %i water continents\n", land_continents, water_continents);
	ai->Log("Average land continent size is %i\n", avg_land_continent_size);
	ai->Log("Average water continent size is %i\n", avg_water_continent_size);

	//debug
	/*for(int x = 0; x < xMapSize; x+=2)
	{
		for(int y = 0; y < yMapSize; y+=2)
		{
			if((buildmap[x + y*xMapSize] == 1) || (buildmap[x + y*xMapSize] == 5) )
			{
				float3 myPos;
				myPos.x = x;
				myPos.z = y;
				BuildMapPos2Pos(&myPos, ai->Getcb()->GetUnitDef("armmine1")); 
				myPos.y = ai->Getcb()->GetElevation(myPos.x, myPos.z);
				ai->Getcb()->DrawUnit("armmine1", myPos, 0.0f, 4000, ai->Getcb()->GetMyAllyTeam(), true, true);	
			}
		}
	}*/
}

void AAIMap::ReadMapCacheFile()
{
	// try to read cache file
	bool loaded = false;

	const size_t buffer_sizeMax = 512;
	char buffer[buffer_sizeMax];

	const std::string mapCache_filename = LocateMapCacheFile();

	FILE *file;

	if((file = fopen(mapCache_filename.c_str(), "r")) != NULL)
	{
		// check if correct version
		fscanf(file, "%s ", buffer);

		if(strcmp(buffer, MAP_CACHE_VERSION))
		{
			ai->LogConsole("Mapcache out of date - creating new one");
			fclose(file);
		}
		else
		{
			int temp;
			float temp_float;

			// load if its a metal map
			fscanf(file, "%i ", &temp);
			metalMap = (bool)temp;

			// load map type
			fscanf(file, "%s ", buffer);

			if(!strcmp(buffer, "LAND_MAP"))
				s_mapType.SetMapType(EMapType::LAND_MAP);
			else if(!strcmp(buffer, "LAND_WATER_MAP"))
				s_mapType.SetMapType(EMapType::LAND_WATER_MAP);
			else if(!strcmp(buffer, "WATER_MAP"))
				s_mapType.SetMapType(EMapType::WATER_MAP);
			else
				s_mapType.SetMapType(EMapType::UNKNOWN_MAP);

			ai->LogConsole("%s loaded", s_mapType.GetName().c_str());

			// load water ratio
			fscanf(file, "%f ", &water_ratio);

			// load buildmap
			for(int y = 0; y < yMapSize; ++y)
			{
				for(int x = 0; x < xMapSize; ++x)
				{
					unsigned int value;
					fscanf(file, "%u", &value);

					const int cell = x + y * xMapSize;
					m_buildmap[cell].m_tileType = static_cast<uint8_t>(value);
				}
			}

			// load plateau map
			for(int y = 0; y < yContMapSize; ++y)
			{
				for(int x = 0; x < xContMapSize; ++x)
				{
					const int cell = x + y * xContMapSize;
					fscanf(file, "%f ", &plateau_map[cell]);
				}
			}

			// load metal spots
			AAIMetalSpot spot;
			fscanf(file, "%i ", &temp);

			for(int i = 0; i < temp; ++i)
			{
				fscanf(file, "%f %f %f %f ", &(spot.pos.x), &(spot.pos.y), &(spot.pos.z), &(spot.amount));
				spot.occupied = false;
				metal_spots.push_back(spot);
			}

			fscanf(file, "%i %i ", &land_metal_spots, &water_metal_spots);

			fclose(file);

			ai->Log("Map cache file successfully loaded\n");

			loaded = true;
		}
	}

	if(!loaded)  // create new map data
	{
		CalculateWaterRatio();

		// detect cliffs/water and create plateau map
		AnalyseMap();

		DetectMapType();

		// search for metal spots after analysis of map for cliffs/water to avoid overriding of blocked underwater metal spots (5) with water (4)
		DetectMetalSpots();

		//////////////////////////////////////////////////////////////////////////////////////////////////////
		// save mod independent map data
		const std::string mapCache_filename = LocateMapCacheFile();

		file = fopen(mapCache_filename.c_str(), "w+");

		fprintf(file, "%s\n", MAP_CACHE_VERSION);

		// save if its a metal map
		fprintf(file, "%i\n", (int)metalMap);

		const char *temp_buffer = GetMapTypeString(s_mapType);

		// save map type
		fprintf(file, "%s\n", temp_buffer);

		// save water ratio
		fprintf(file, "%f\n", water_ratio);

		// save buildmap
		for(int y = 0; y < yMapSize; ++y)
		{
			for(int x = 0; x < xMapSize; ++x)
			{
				const int cell = x + y * xMapSize;
				fprintf(file, "%u ", m_buildmap[cell].m_tileType);
			}
			fprintf(file, "\n");
		}

		// save plateau map
		for(int y = 0; y < yContMapSize; ++y)
		{
			for(int x = 0; x < xContMapSize; ++x)
			{
				const int cell = x + y * xContMapSize;
				fprintf(file, "%f ", plateau_map[cell]);
			}
			fprintf(file, "\n");
		}
			
		// save mex spots
		land_metal_spots = 0;
		water_metal_spots = 0;

		fprintf(file, _STPF_ " \n", metal_spots.size());

		for(std::list<AAIMetalSpot>::iterator spot = metal_spots.begin(); spot != metal_spots.end(); ++spot)
		{
			fprintf(file, "%f %f %f %f \n", spot->pos.x, spot->pos.y, spot->pos.z, spot->amount);

			if(spot->pos.y >= 0.0f)
				++land_metal_spots;
			else
				++water_metal_spots;
		}

		fprintf(file, "%i %i\n", land_metal_spots, water_metal_spots);

		fclose(file);

		ai->Log("New map cache-file created\n");
	}
}

void AAIMap::ReadContinentFile()
{
	const std::string filename = cfg->GetFileName(ai->GetAICallback(), cfg->getUniqueName(ai->GetAICallback(), true, true, true, true), MAP_CACHE_PATH, "_continent.dat", true);
	FILE* file = fopen(filename.c_str(), "r");

	if(file != NULL)
	{
		char buffer[4096];
		// check if correct version
		fscanf(file, "%s ", buffer);

		if(strcmp(buffer, CONTINENT_DATA_VERSION))
		{
			ai->LogConsole("Continent cache out of date - creating new one");
			fclose(file);
		}
		else
		{
			int temp, temp2;

			// load continent map
			for(int j = 0; j < yContMapSize; ++j)
			{
				for(int i = 0; i < xContMapSize; ++i)
				{
					fscanf(file, "%i ", &temp);
					continent_map[j * xContMapSize + i] = temp;
				}
			}

			// load continents
			fscanf(file, "%i ", &temp);

			continents.resize(temp);

			for(int i = 0; i < temp; ++i)
			{
				fscanf(file, "%i %i ", &continents[i].size, &temp2);

				continents[i].water = (bool) temp2;
				continents[i].id = i;
			}

			// load statistical data
			fscanf(file, "%i %i %i %i %i %i %i %i", &land_continents, &water_continents, &avg_land_continent_size, &avg_water_continent_size,
																			&max_land_continent_size, &max_water_continent_size,
																			&min_land_continent_size, &min_water_continent_size);

			fclose(file);

			ai->Log("Continent cache file successfully loaded\n");

			return;
		}
	}


	// loading has not been succesful -> create new continent maps

	// create continent/movement map
	CalculateContinentMaps();

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// save movement maps
	const std::string movementfile = cfg->GetFileName(ai->GetAICallback(), cfg->getUniqueName(ai->GetAICallback(), true, false, true, false), MAP_CACHE_PATH, "_movement.dat", true);
	file = fopen(movementfile.c_str(), "w+");

	fprintf(file, "%s\n",  CONTINENT_DATA_VERSION);

	// save continent map
	for(int j = 0; j < yContMapSize; ++j)
	{
		for(int i = 0; i < xContMapSize; ++i)
			fprintf(file, "%i ", continent_map[j * xContMapSize + i]);

		fprintf(file, "\n");
	}

	// save continents
	fprintf(file, "\n" _STPF_ " \n", continents.size());

	for(size_t c = 0; c < continents.size(); ++c)
		fprintf(file, "%i %i \n", continents[c].size, (int)continents[c].water);

	// save statistical data
	fprintf(file, "%i %i %i %i %i %i %i %i\n", land_continents, water_continents, avg_land_continent_size, avg_water_continent_size,
																	max_land_continent_size, max_water_continent_size,
																	min_land_continent_size, min_water_continent_size);

	fclose(file);
}

std::string AAIMap::LocateMapLearnFile() const
{
	return cfg->GetFileName(ai->GetAICallback(), cfg->getUniqueName(ai->GetAICallback(), true, true, true, true), MAP_LEARN_PATH, "_maplearn.dat", true);
}

std::string AAIMap::LocateMapCacheFile() const
{
	return cfg->GetFileName(ai->GetAICallback(), cfg->getUniqueName(ai->GetAICallback(), false, false, true, true), MAP_LEARN_PATH, "_mapcache.dat", true);
}

void AAIMap::ReadMapLearnFile()
{
	const std::string mapLearn_filename = LocateMapLearnFile();

	const size_t buffer_sizeMax = 2048;
	char buffer[buffer_sizeMax];

	// open learning files
	FILE *load_file = fopen(mapLearn_filename.c_str(), "r");

	// check if correct map file version
	if(load_file)
	{
		fscanf(load_file, "%s", buffer);

		// file version out of date
		if(strcmp(buffer, MAP_LEARN_VERSION))
		{
			ai->LogConsole("Map learning file version out of date, creating new one");
			fclose(load_file);
			load_file = 0;
		}
	}

	// load sector data from file or init with default values
	for(int j = 0; j < ySectors; ++j)
	{
		for(int i = 0; i < xSectors; ++i)
		{
			//---------------------------------------------------------------------------------------------------------
			// load learned sector data from file (if available) or init with default data
			//---------------------------------------------------------------------------------------------------------

			m_sector[i][j].LoadDataFromFile(load_file);

			//---------------------------------------------------------------------------------------------------------
			// determine movement types that are suitable to maneuvre
			//---------------------------------------------------------------------------------------------------------
			AAIMapType mapType(EMapType::LAND_MAP);

			if(m_sector[i][j].water_ratio > 0.7f)
				mapType.SetMapType(EMapType::WATER_MAP);
			else if(m_sector[i][j].water_ratio > 0.3f)
				mapType.SetMapType(EMapType::LAND_WATER_MAP);

			m_sector[i][j].m_suitableMovementTypes = GetSuitableMovementTypes(mapType);
		}
	}

    //-----------------------------------------------------------------------------------------------------------------
	// determine land/water ratio of total map
	//-----------------------------------------------------------------------------------------------------------------
	flat_land_ratio = 0.0f;
	water_ratio     = 0.0f;

	for(int j = 0; j < ySectors; ++j)
	{
		for(int i = 0; i < xSectors; ++i)
		{
			flat_land_ratio += m_sector[i][j].flat_ratio;
			water_ratio += m_sector[i][j].water_ratio;
		}
	}

	flat_land_ratio /= (float)(xSectors * ySectors);
	water_ratio     /= (float)(xSectors * ySectors);
	land_ratio = 1.0f - water_ratio;

	if(load_file)
		fclose(load_file);
	else
		ai->LogConsole("New map-learning file created");
}

void AAIMap::Learn()
{
	for(int y = 0; y < ySectors; ++y)
	{
		for(int x = 0; x < xSectors; ++x)
		{
			m_sector[x][y].UpdateLearnedData();
		}
	}
}

// converts unit positions to cell coordinates
void AAIMap::Pos2BuildMapPos(float3 *position, const UnitDef* def) const
{
	// get cell index of middlepoint
	int x = (int) (position->x/SQUARE_SIZE);
	int z = (int) (position->z/SQUARE_SIZE);

	// shift to the leftmost uppermost cell
	x -= def->xsize/2;
	z -= def->zsize/2;

	// check if pos is still in that map, otherwise retun 0
	if(x < 0)
		position->x = 0.0f;
	else
		position->x = static_cast<float>(x);

	if(z < 0)
		position->z = 0.0f;
	else
		position->z = static_cast<float>(z);
}

void AAIMap::BuildMapPos2Pos(float3 *pos, const UnitDef *def) const
{
	// shift to middlepoint
	pos->x += def->xsize/2;
	pos->z += def->zsize/2;

	// back to unit coordinates
	pos->x *= SQUARE_SIZE;
	pos->z *= SQUARE_SIZE;
}

void AAIMap::Pos2FinalBuildPos(float3 *pos, const UnitDef* def) const
{
	if(def->xsize&2) // check if xsize is a multiple of 4
		pos->x=floor((pos->x)/(SQUARE_SIZE*2))*SQUARE_SIZE*2+8;
	else
		pos->x=floor((pos->x+8)/(SQUARE_SIZE*2))*SQUARE_SIZE*2;

	if(def->zsize&2)
		pos->z=floor((pos->z)/(SQUARE_SIZE*2))*SQUARE_SIZE*2+8;
	else
		pos->z=floor((pos->z+8)/(SQUARE_SIZE*2))*SQUARE_SIZE*2;
}

void AAIMap::ChangeBuildMapOccupation(int xPos, int yPos, int xSize, int ySize, bool occupy)
{
	// ensure that that cell index may not run outside of the map (should not happen as buildings cannot be placed so close to the map edge)
	const int xEnd = std::min(xPos + xSize, xMapSize);
	const int yEnd = std::min(yPos + ySize, yMapSize);

	for(int y = yPos; y < yEnd; ++y)
	{
		for(int x = xPos; x < xEnd; ++x)
		{
			if(occupy)
				m_buildmap[x+y*xMapSize].OccupyTile();
			else
				m_buildmap[x+y*xMapSize].FreeTile();

			// debug
			/*if(x%2 == 0 && y%2 == 0)
			{
				float3 myPos;
				myPos.x = x;
				myPos.z = y;
				BuildMapPos2Pos(&myPos, ai->Getcb()->GetUnitDef("armmine1")); 
				myPos.y = ai->Getcb()->GetElevation(myPos.x, myPos.z);
				ai->Getcb()->DrawUnit("armmine1", myPos, 0.0f, 2000, ai->Getcb()->GetMyAllyTeam(), true, true);
			}*/
		}
	}	
}

float3 AAIMap::GetRandomBuildsite(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, int tries, bool water)
{
	const UnitFootprint footprint = DetermineRequiredFreeBuildspace(UnitDefId(def->id));

	for(int i = 0; i < tries; i++)
	{
		float3 pos;

		// get random pos within rectangle
		if(xEnd - xStart - footprint.xSize < 1)
			pos.x = xStart;
		else
			pos.x = xStart + rand()%(xEnd - xStart - footprint.xSize);

		if(yEnd - yStart - footprint.ySize < 1)
			pos.z = yStart;
		else
			pos.z = yStart + rand()%(yEnd - yStart - footprint.ySize);

		// check if buildmap allows construction
		if(CanBuildAt(pos.x, pos.z, footprint, water))
		{
			// buildmap allows construction, now check if otherwise blocked
			BuildMapPos2Pos(&pos, def);
			Pos2FinalBuildPos(&pos, def);

			if(ai->GetAICallback()->CanBuildAt(def, pos))
			{
				AAISector* sector = GetSectorOfPos(pos);

				if(sector)
					return pos;
			}
		}
	}

	return ZeroVector;
}

float3 AAIMap::DetermineBuildsiteInSector(UnitDefId buildingDefId, const AAISector* sector) const
{
	int xStart, xEnd, yStart, yEnd;
	sector->DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);

	const UnitFootprint footprint = DetermineRequiredFreeBuildspace(buildingDefId);
	const bool          water     = ai->s_buildTree.GetMovementType(buildingDefId).IsSea();
	const UnitDef*      def       = &ai->Getbt()->GetUnitDef(buildingDefId.id);

	// check rect
	for(int yPos = yStart; yPos < yEnd; yPos += 2)
	{
		for(int xPos = xStart; xPos < xEnd; xPos += 2)
		{
			// check if buildmap allows construction
			if(CanBuildAt(xPos, yPos, footprint, water))
			{
				float3 possibleBuildsite(static_cast<float>(xPos), 0.0f, static_cast<float>(yPos));

				// buildmap allows construction, now check if otherwise blocked
				BuildMapPos2Pos(&possibleBuildsite, def);
				Pos2FinalBuildPos(&possibleBuildsite, def);

				if(ai->GetAICallback()->CanBuildAt(def, possibleBuildsite))
				{
					int x = possibleBuildsite.x/xSectorSize;
					int y = possibleBuildsite.z/ySectorSize;

					if(IsValidSector(x,y))
						return possibleBuildsite;
				}
			}
		}
	}

	return ZeroVector;
}

float3 AAIMap::GetRadarArtyBuildsite(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, float range, bool water)
{
	float3 selectedPosition = ZeroVector;

	float highestRating(-10000.0f);

	// convert range from unit coordinates to build map coordinates
	range /= 8.0f;

	const UnitFootprint footprint = DetermineRequiredFreeBuildspace(UnitDefId(def->id));

	// go through rect
	for(int yPos = yStart; yPos < yEnd; yPos += 2)
	{
		for(int xPos = xStart; xPos < xEnd; xPos += 2)
		{
			if(CanBuildAt(xPos, yPos, footprint, water))
			{
				const float edgeDist = static_cast<float>(GetEdgeDistance(xPos, yPos)) / range;

				float rating = 0.04f * (float)(rand()%50) + edgeDist;

				if(!water)
				{
					const int plateauMapCellIndex = xPos/4 + yPos/4 * xContMapSize;
					rating += plateau_map[plateauMapCellIndex];
				}
					
				if(rating > highestRating)
				{
					float3 pos;
					pos.x = xPos;
					pos.z = yPos;

					// buildmap allows construction, now check if otherwise blocked
					BuildMapPos2Pos(&pos, def);
					Pos2FinalBuildPos(&pos, def);

					if(ai->GetAICallback()->CanBuildAt(def, pos))
					{
						selectedPosition = pos;
						highestRating = rating;
					}
				}
			}
		}
	}

	return selectedPosition;
}

float AAIMap::GetDefenceBuildsite(float3 *buildPos, const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, const AAITargetType& targetType, float terrainModifier, bool water) const
{
	*buildPos = ZeroVector;

	const std::vector<float> *map = &defence_map;

	if(targetType.IsAir() )
		map = &air_defence_map;
	else if(targetType.IsSubmerged() )
		map = &submarine_defence_map;

	const float         range     =  ai->s_buildTree.GetMaxRange(UnitDefId(def->id)) / 8.0f;
	const UnitFootprint footprint = DetermineRequiredFreeBuildspace(UnitDefId(def->id));

	const std::string filename = cfg->GetFileName(ai->GetAICallback(), "AAIDebug.txt", "", "", true);
	FILE* file = fopen(filename.c_str(), "w+");
	fprintf(file, "Search area: (%i, %i) x (%i, %i)\n", xStart, yStart, xEnd, yEnd);
	fprintf(file, "Range: %g\n", range);

	float highestRating(-10000.0f);

	// check rect
	for(int yPos = yStart; yPos < yEnd; yPos += 4)
	{
		for(int xPos = xStart; xPos < xEnd; xPos += 4)
		{
			// check if buildmap allows construction
			if(CanBuildAt(xPos, yPos, footprint, water))
			{
				const int cell = (xPos/4 + xDefMapSize * yPos/4);

				float rating = terrainModifier * plateau_map[cell] - (*map)[cell] + 0.5f * (float)(rand()%10);
				//my_rating = - (*map)[cell];

				// determine minimum distance from buildpos to the edges of the map
				const int edge_distance = GetEdgeDistance(xPos, yPos);

				fprintf(file, "Pos: (%i,%i) -> Def map cell %i -> rating: %i  , edge_dist: %i\n",xPos, yPos, cell, (int)rating, edge_distance);

				// prevent aai from building defences too close to the edges of the map
				if( (float)edge_distance < range)
					rating -= (range - (float)edge_distance) * (range - (float)edge_distance);

				if(rating > highestRating)
				{
					float3 pos;
					pos.x = static_cast<float>(xPos);
					pos.z = static_cast<float>(yPos);

					// buildmap allows construction, now check if otherwise blocked
					BuildMapPos2Pos(&pos, def);
					Pos2FinalBuildPos(&pos, def);

					if(ai->GetAICallback()->CanBuildAt(def, pos))
					{
						*buildPos = pos;
						highestRating = rating;
					}
				}
			}
		}
	}

	fclose(file);

	return highestRating;
}

bool AAIMap::CanBuildAt(int xPos, int yPos, const UnitFootprint& size, bool water) const
{
	if( (xPos+size.xSize > xMapSize) || (yPos+size.ySize > yMapSize) )
		return false; // buildsite too close to edges of map
	else
	{
		BuildMapTileType invalidTileTypes(EBuildMapTileType::OCCUPIED, EBuildMapTileType::BLOCKED_SPACE);

		if(water)
			invalidTileTypes.SetTileType(EBuildMapTileType::LAND);
		else
		{
			invalidTileTypes.SetTileType(EBuildMapTileType::WATER);
			invalidTileTypes.SetTileType(EBuildMapTileType::CLIFF);
		}

		for(int y = yPos; y < yPos+size.ySize; ++y)
		{
			for(int x = xPos; x < xPos+size.xSize; ++x)
			{
				// all squares must be valid
				if(m_buildmap[x+y*xMapSize].IsTileTypeSet(invalidTileTypes))
					return false;
			}
		}

		return true;
	}
}

void AAIMap::CheckRows(int xPos, int yPos, int xSize, int ySize, bool add)
{
	const BuildMapTileType nonOccupiedTile(EBuildMapTileType::FREE, EBuildMapTileType::BLOCKED_SPACE);

	// check horizontal space
	if( (xPos+xSize+cfg->MAX_XROW <= xMapSize) && (xPos - cfg->MAX_XROW >= 0) )
	{
		for(int y = yPos; y < yPos + ySize; ++y)
		{
			if(y >= yMapSize)
			{
				ai->Log("ERROR: y = %i index out of range when checking horizontal rows", y);
				return;
			}

			// check to the right
			int occupiedMapTiles(xSize);
			int xRight(-1);
			for(int x = xPos+xSize; x < xPos+xSize+cfg->MAX_XROW; ++x)
			{
				// abort when first non occupied tile is found
				if(m_buildmap[x+y*xMapSize].IsTileTypeSet(nonOccupiedTile))
				{
					xRight = x;
					break;
				}

				++occupiedMapTiles;
			}

			// check to the left
			int xLeft(-1);
			for(int x = xPos-1; x >= xPos - cfg->MAX_XROW; --x)
			{
				if(m_buildmap[x+y*xMapSize].IsTileTypeSet(nonOccupiedTile))
				{
					xLeft = x;
					break;
				}

				++occupiedMapTiles;
			}
			
			// avoid spaces for buildings with xSize > occupiedMapTiles
			if( (occupiedMapTiles > cfg->MAX_XROW) && (occupiedMapTiles > xSize) )
			{
				if(xRight != -1)
				{
					BlockTiles(xRight, y, cfg->X_SPACE, 1, add);

					//add blocking of the edges
					if(y == yPos)
						BlockTiles(xRight, yPos - cfg->Y_SPACE, cfg->X_SPACE, cfg->Y_SPACE, add);
					if(y == yPos + ySize - 1)
						BlockTiles(xRight, yPos + ySize, cfg->X_SPACE, cfg->Y_SPACE, add);
				}

				if(xLeft != -1)
				{
					BlockTiles(xLeft-cfg->X_SPACE, y, cfg->X_SPACE, 1, add);

					// add diagonal blocks
					if(y == yPos )
						BlockTiles(xLeft-cfg->X_SPACE, yPos - cfg->Y_SPACE, cfg->X_SPACE, cfg->Y_SPACE, add);
					if(y == yPos + ySize - 1)
						BlockTiles(xLeft-cfg->X_SPACE, yPos + ySize, cfg->X_SPACE, cfg->Y_SPACE, add);

				}
			}
		}
	}

	// check vertical space
	if(yPos+ySize+cfg->MAX_YROW <= yMapSize && yPos - cfg->MAX_YROW >= 0)
	{
		for(int x = xPos; x < xPos + xSize; ++x)
		{
			if(x >= xMapSize)
			{
				ai->Log("ERROR: x = %i index out of range when checking vertical rows", x);
				return;
			}

			// check downwards
			int occupiedMapTiles(ySize);
			int yBottom(-1);
			for(int y = yPos+ySize; y < yPos+ySize+cfg->MAX_YROW; ++y)
			{
				if(m_buildmap[x+y*xMapSize].IsTileTypeSet(nonOccupiedTile))
				{
					yBottom = y;
					break;
				}

				++occupiedMapTiles;
			}

			// check upwards
			int yTop(-1);
			for(int y = yPos-1; y >= yPos - cfg->MAX_YROW; --y)
			{
				if(m_buildmap[x+y*xMapSize].IsTileTypeSet(nonOccupiedTile))
				{
					yTop = y;
					break;
				}

				++occupiedMapTiles;
			}
			
			if( (occupiedMapTiles > cfg->MAX_YROW) && (occupiedMapTiles > ySize) )
			{
				if(yBottom != -1)
				{
					BlockTiles(x, yBottom, 1, cfg->Y_SPACE, add);

					// add diagonal blocks
					if( (x == xPos) )
						BlockTiles(xPos-cfg->X_SPACE, yBottom, cfg->X_SPACE, cfg->Y_SPACE, add);
					if(x == xPos + xSize - 1)
						BlockTiles(xPos + xSize, yBottom, cfg->X_SPACE, cfg->Y_SPACE, add);
				}

				// upwards
				if(yTop != -1)
				{
					BlockTiles(x, yTop-cfg->Y_SPACE, 1, cfg->Y_SPACE, add);

					// add diagonal blocks
					if(x == xPos)
						BlockTiles(xPos-cfg->X_SPACE, yTop-cfg->Y_SPACE, cfg->X_SPACE, cfg->Y_SPACE, add);
					if(x == xPos + xSize - 1)
						BlockTiles(xPos + xSize, yTop-cfg->Y_SPACE, cfg->X_SPACE, cfg->Y_SPACE, add);
				}
			}
		}
	}
}

void AAIMap::BlockTiles(int xPos, int yPos, int width, int height, bool block)
{
	// make sure to stay within map if too close to the edges
	const int xStart = std::max(xPos, 0);
	const int yStart = std::max(yPos, 0);
	const int xEnd   = std::min(xPos + width, xMapSize);
	const int yEnd   = std::min(yPos + height, yMapSize);

	for(int y = yStart; y < yEnd; ++y)
	{
		for(int x = xStart; x < xEnd; ++x)
		{
			const int tileIndex = x + xMapSize*y;

			if(block)	// block cells
			{
				// if no building ordered that cell to be blocked, update buildmap
				// (only if space is not already occupied by a building)
				if( (blockmap[tileIndex] == 0) && (m_buildmap[tileIndex].IsTileTypeSet(EBuildMapTileType::FREE)) )
					m_buildmap[tileIndex].BlockTile();	

				++blockmap[tileIndex];
			}
			else
			{
				if(blockmap[tileIndex] > 0)
				{
					--blockmap[tileIndex];

					// if cell is not blocked anymore, mark cell on buildmap as empty (only if it has been marked bloked
					//					- if it is not marked as blocked its occupied by another building or unpassable)
					if(blockmap[tileIndex] == 0 && m_buildmap[tileIndex].IsTileTypeSet(EBuildMapTileType::BLOCKED_SPACE))
						m_buildmap[tileIndex].FreeTile();	
				}
			}

			// debug
			/*if(x%2 == 0 && y%2 == 0)
			{
				float3 myPos;
				myPos.x = x;
				myPos.z = y;
				BuildMapPos2Pos(&myPos, ai->GetAICallback()->GetUnitDef("armmine1")); 
				myPos.y = ai->GetAICallback()->GetElevation(myPos.x, myPos.z);
				ai->GetAICallback()->DrawUnit("armmine1", myPos, 0.0f, 2000, ai->GetAICallback()->GetMyAllyTeam(), true, true);
			}*/
		}
	}
}

bool AAIMap::InitBuilding(const UnitDef *def, const float3& position)
{
	AAISector* sector = GetSectorOfPos(position);

	// drop bad sectors (should only happen when defending mexes at the edge of the map)
	if(sector == nullptr)
		return false;
	else
	{
		// update buildmap
		UpdateBuildMap(position, def, true);

		// update defence map (if necessary)
		UnitDefId unitDefId(def->id);
		if(ai->s_buildTree.GetUnitCategory(unitDefId).isStaticDefence())
			AddStaticDefence(position, unitDefId);

		// increase number of units of that category in the target sector
		sector->AddBuilding(ai->s_buildTree.GetUnitCategory(unitDefId));

		return true;
	}
}
	
void AAIMap::UpdateBuildMap(const float3& buildPos, const UnitDef *def, bool block)
{
	const bool factory = ai->s_buildTree.GetUnitType(UnitDefId(def->id)).IsFactory();
	
	float3 buildMapPos = buildPos;
	Pos2BuildMapPos(&buildMapPos, def);

	// remove spaces before freeing up buildspace
	if(!block)
		CheckRows(buildMapPos.x, buildMapPos.z, def->xsize, def->zsize, block);

	ChangeBuildMapOccupation(buildMapPos.x, buildMapPos.z, def->xsize, def->zsize, block);

	if(factory)
	{
		// extra space for factories to keep exits clear
		BlockTiles(buildMapPos.x,              buildMapPos.z - cfg->Y_SPACE ,          def->xsize, cfg->Y_SPACE, block);
		BlockTiles(buildMapPos.x + def->xsize, buildMapPos.z - cfg->Y_SPACE ,          cfg->X_SPACE, def->zsize + 2*cfg->Y_SPACE, block);
		BlockTiles(buildMapPos.x,              buildMapPos.z + def->zsize, def->xsize, cfg->Y_SPACE , block);
	}

	// add spaces after blocking buildspace
	if(block)
		CheckRows(buildMapPos.x, buildMapPos.z, def->xsize, def->zsize, block);
}

UnitFootprint AAIMap::DetermineRequiredFreeBuildspace(UnitDefId unitDefId) const
{
	// if building is a factory additional vertical space is needed to keep exits free
	if(ai->s_buildTree.GetUnitType(unitDefId).IsFactory())
	{
		const int xSize = ai->s_buildTree.GetFootprint(unitDefId).xSize + cfg->X_SPACE;
		const int ySize = ai->s_buildTree.GetFootprint(unitDefId).ySize + 2 * cfg->Y_SPACE;
		return UnitFootprint(xSize, ySize);
	}
	else
		return ai->s_buildTree.GetFootprint(unitDefId);
}

int AAIMap::GetCliffyCells(int xPos, int yPos, int xSize, int ySize) const
{
	int cliffs(0);

	// count cells with big slope
	for(int y = yPos; y < yPos + ySize; ++y)
	{
		for(int x = xPos; x < xPos + xSize; ++x)
		{
			if(m_buildmap[x+y*xMapSize].IsTileTypeSet(EBuildMapTileType::CLIFF))
				++cliffs;
		}
	}

	return cliffs;
}

void AAIMap::AnalyseMap()
{
	const float *height_map = ai->GetAICallback()->GetHeightMap();

	// get water/cliffs
	for(int x = 0; x < xMapSize; ++x)
	{
		for(int y = 0; y < yMapSize; ++y)
		{
			m_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::FREE);

			// check for water
			if(height_map[x + y * xMapSize] < 0.0f)
				m_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::WATER);
			else
				m_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::LAND);	
			
			// check slope
			if( (x < xMapSize - 4) && (y < yMapSize - 4) )
			{
				const float xSlope = (height_map[y * xMapSize + x] - height_map[y * xMapSize + x + 4])/64.0f;

				// check x-direction
				if( (xSlope > cfg->CLIFF_SLOPE) || (-xSlope > cfg->CLIFF_SLOPE) )
					m_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::CLIFF);
				else	// check y-direction
				{
					const float ySlope = (height_map[y * xMapSize + x] - height_map[(y+4) * xMapSize + x])/64.0f;

					if(ySlope > cfg->CLIFF_SLOPE || -ySlope > cfg->CLIFF_SLOPE)
						m_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::CLIFF);
					else
						m_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::FLAT);
				}
			}
			else
				m_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::FLAT);	
		}
	}

	// calculate plateau map
	int TERRAIN_DETECTION_RANGE = 6;
	float my_height, diff;

	for(int y = TERRAIN_DETECTION_RANGE; y < yContMapSize - TERRAIN_DETECTION_RANGE; ++y)
	{
		for(int x = TERRAIN_DETECTION_RANGE; x < xContMapSize - TERRAIN_DETECTION_RANGE; ++x)
		{
			my_height = height_map[4 * (x + y * xMapSize)];

			for(int j = y - TERRAIN_DETECTION_RANGE; j < y + TERRAIN_DETECTION_RANGE; ++j)
			{
					for(int i = x - TERRAIN_DETECTION_RANGE; i < x + TERRAIN_DETECTION_RANGE; ++i)
				{
					 diff = (height_map[4 * (i + j * xMapSize)] - my_height);

					 if(diff > 0)
					 {
						 //! @todo Investigate the reason for this check
						 if(m_buildmap[4 * (i + j * xMapSize)].IsTileTypeNotSet(EBuildMapTileType::CLIFF) )
							 plateau_map[i + j * xContMapSize] += diff;
					 }
					 else
						 plateau_map[i + j * xContMapSize] += diff;
				}
			}
		}
	}

	for(int y = 0; y < yContMapSize; ++y)
	{
		for(int x = 0; x < xContMapSize; ++x)
		{
			if(plateau_map[x + y * xContMapSize] >= 0.0f)
				plateau_map[x + y * xContMapSize] = sqrt(plateau_map[x + y * xContMapSize]);
			else
				plateau_map[x + y * xContMapSize] = -1.0f * sqrt((-1.0f) * plateau_map[x + y * xContMapSize]);
		}
	}
}

void AAIMap::DetectMapType()
{
	ai->Log("Water ratio: %f\n", water_ratio);

	if( (static_cast<float>(max_land_continent_size) < 0.5f * static_cast<float>(max_water_continent_size) ) || (water_ratio > 0.8f) )
		s_mapType.SetMapType(EMapType::WATER_MAP);
	else if(water_ratio > 0.25f)
		s_mapType.SetMapType(EMapType::LAND_WATER_MAP);
	else
		s_mapType.SetMapType(EMapType::LAND_MAP);
}

void AAIMap::CalculateWaterRatio()
{
	int waterCells(0);

	for(int y = 0; y < yMapSize; ++y)
	{
		for(int x = 0; x < xMapSize; ++x)
		{
			if(m_buildmap[x + y*xMapSize].IsTileTypeSet(EBuildMapTileType::WATER))
				++waterCells;
		}
	}

	water_ratio = static_cast<float>(waterCells) / static_cast<float>(xMapSize*yMapSize);
}

void AAIMap::CalculateContinentMaps()
{
	vector<int> *new_edge_cells;
	vector<int> *old_edge_cells;

	vector<int> a, b;

	old_edge_cells = &a;
	new_edge_cells = &b;

	const float *height_map = ai->GetAICallback()->GetHeightMap();

	int x, y;

	AAIContinent temp;
	int continent_id = 0;


	for(int i = 0; i < xContMapSize; i += 1)
	{
		for(int j = 0; j < yContMapSize; j += 1)
		{
			// add new continent if cell has not been visited yet
			if(continent_map[j * xContMapSize + i] < 0 && height_map[4 * (j * xMapSize + i)] >= 0)
			{
				temp.id = continent_id;
				temp.size = 1;
				temp.water = false;

				continents.push_back(temp);

				continent_map[j * xContMapSize + i] = continent_id;

				old_edge_cells->push_back(j * xContMapSize + i);

				// check edges of the continent as long as new cells have been added to the continent during the last loop
				while(old_edge_cells->size() > 0)
				{
					for(vector<int>::iterator cell = old_edge_cells->begin(); cell != old_edge_cells->end(); ++cell)
					{
						// get cell indizes
						x = (*cell)%xContMapSize;
						y = ((*cell) - x) / xContMapSize;

						// check edges
						if(x > 0 && continent_map[y * xContMapSize + x - 1] == -1)
						{
							if(height_map[4 * (y * xMapSize + x - 1)] >= 0)
							{
								continent_map[y * xContMapSize + x - 1] = continent_id;
								continents[continent_id].size += 1;
								new_edge_cells->push_back( y * xContMapSize + x - 1 );
							}
							else if(height_map[4 * (y * xMapSize + x - 1)] >= - cfg->NON_AMPHIB_MAX_WATERDEPTH)
							{
								continent_map[y * xContMapSize + x - 1] = -2;
								new_edge_cells->push_back( y * xContMapSize + x - 1 );
							}
						}

						if(x < xContMapSize-1 && continent_map[y * xContMapSize + x + 1] == -1)
						{
							if(height_map[4 * (y * xMapSize + x + 1)] >= 0)
							{
								continent_map[y * xContMapSize + x + 1] = continent_id;
								continents[continent_id].size += 1;
								new_edge_cells->push_back( y * xContMapSize + x + 1 );
							}
							else if(height_map[4 * (y * xMapSize + x + 1)] >= - cfg->NON_AMPHIB_MAX_WATERDEPTH)
							{
								continent_map[y * xContMapSize + x + 1] = -2;
								new_edge_cells->push_back( y * xContMapSize + x + 1 );
							}
						}

						if(y > 0 && continent_map[(y - 1) * xContMapSize + x] == -1)
						{
							if(height_map[4 * ( (y - 1) * xMapSize + x)] >= 0)
							{
								continent_map[(y - 1) * xContMapSize + x] = continent_id;
								continents[continent_id].size += 1;
								new_edge_cells->push_back( (y - 1) * xContMapSize + x);
							}
							else if(height_map[4 * ( (y - 1) * xMapSize + x)] >= - cfg->NON_AMPHIB_MAX_WATERDEPTH)
							{
								continent_map[(y - 1) * xContMapSize + x] = -2;
								new_edge_cells->push_back( (y - 1) * xContMapSize + x );
							}
						}

						if(y < yContMapSize-1 && continent_map[(y + 1 ) * xContMapSize + x] == -1)
						{
							if(height_map[4 * ( (y + 1) * xMapSize + x)] >= 0)
							{
								continent_map[(y + 1) * xContMapSize + x] = continent_id;
								continents[continent_id].size += 1;
								new_edge_cells->push_back( (y + 1) * xContMapSize + x );
							}
							else if(height_map[4 * ( (y + 1) * xMapSize + x)] >= - cfg->NON_AMPHIB_MAX_WATERDEPTH)
							{
								continent_map[(y + 1) * xContMapSize + x] = -2;
								new_edge_cells->push_back( (y + 1) * xContMapSize + x );
							}
						}
					}

					old_edge_cells->clear();

					// invert pointers to new/old edge cells
					if(new_edge_cells == &a)
					{
						new_edge_cells = &b;
						old_edge_cells = &a;
					}
					else
					{
						new_edge_cells = &a;
						old_edge_cells = &b;
					}
				}

				// finished adding continent
				++continent_id;
				old_edge_cells->clear();
				new_edge_cells->clear();
			}
		}
	}

	// water continents
	for(int i = 0; i < xContMapSize; i += 1)
	{
		for(int j = 0; j < yContMapSize; j += 1)
		{
			// add new continent if cell has not been visited yet
			if(continent_map[j * xContMapSize + i] < 0)
			{
				temp.id = continent_id;
				temp.size = 1;
				temp.water = true;

				continents.push_back(temp);

				continent_map[j * xContMapSize + i] = continent_id;

				old_edge_cells->push_back(j * xContMapSize + i);

				// check edges of the continent as long as new cells have been added to the continent during the last loop
				while(old_edge_cells->size() > 0)
				{
					for(vector<int>::iterator cell = old_edge_cells->begin(); cell != old_edge_cells->end(); ++cell)
					{
						// get cell indizes
						x = (*cell)%xContMapSize;
						y = ((*cell) - x) / xContMapSize;

						// check edges
						if(x > 0 && continent_map[y * xContMapSize + x - 1] < 0)
						{
							if(height_map[4 * (y * xMapSize + x - 1)] < 0)
								{
								continent_map[y * xContMapSize + x - 1] = continent_id;
								continents[continent_id].size += 1;
								new_edge_cells->push_back( y * xContMapSize + x - 1 );
							}
						}

						if(x < xContMapSize-1 && continent_map[y * xContMapSize + x + 1] < 0)
						{
							if(height_map[4 * (y * xMapSize + x + 1)] < 0)
							{
								continent_map[y * xContMapSize + x + 1] = continent_id;
								continents[continent_id].size += 1;
								new_edge_cells->push_back( y * xContMapSize + x + 1 );
							}
						}

						if(y > 0 && continent_map[(y - 1) * xContMapSize + x ] < 0)
						{
							if(height_map[4 * ( (y - 1) * xMapSize + x )] < 0)
							{
								continent_map[(y - 1) * xContMapSize + x ] = continent_id;
								continents[continent_id].size += 1;
								new_edge_cells->push_back( (y - 1) * xContMapSize + x );
							}
						}

						if(y < yContMapSize-1 && continent_map[(y + 1) * xContMapSize + x ] < 0)
						{
							if(height_map[4 * ( (y + 1) * xMapSize + x)] < 0)
							{
								continent_map[(y + 1) * xContMapSize + x ] = continent_id;
								continents[continent_id].size += 1;
								new_edge_cells->push_back( (y + 1) * xContMapSize + x  );
							}
						}
					}

					old_edge_cells->clear();

					// invert pointers to new/old edge cells
					if(new_edge_cells == &a)
					{
						new_edge_cells = &b;
						old_edge_cells = &a;
					}
					else
					{
						new_edge_cells = &a;
						old_edge_cells = &b;
					}
				}

				// finished adding continent
				++continent_id;
				old_edge_cells->clear();
				new_edge_cells->clear();
			}
		}
	}

	// calculate some statistical data
	land_continents = 0;
	water_continents = 0;

	avg_land_continent_size = 0;
	avg_water_continent_size = 0;
	max_land_continent_size = 0;
	max_water_continent_size = 0;
	min_land_continent_size = xContMapSize * yContMapSize;
	min_water_continent_size = xContMapSize * yContMapSize;

	for(size_t i = 0; i < continents.size(); ++i)
	{
		if(continents[i].water)
		{
			++water_continents;
			avg_water_continent_size += continents[i].size;

			if(continents[i].size > max_water_continent_size)
				max_water_continent_size = continents[i].size;

			if(continents[i].size < min_water_continent_size)
				min_water_continent_size = continents[i].size;
		}
		else
		{
			++land_continents;
			avg_land_continent_size += continents[i].size;

			if(continents[i].size > max_land_continent_size)
				max_land_continent_size = continents[i].size;

			if(continents[i].size < min_land_continent_size)
				min_land_continent_size = continents[i].size;
		}
	}

	if(water_continents > 0)
		avg_water_continent_size /= water_continents;

	if(land_continents > 0)
		avg_land_continent_size /= land_continents;
}

// algorithm more or less by krogothe - thx very much
void AAIMap::DetectMetalSpots()
{
	const UnitDefId largestExtractor = ai->Getbt()->GetLargestExtractor();
	if ( largestExtractor.isValid() == false ) 
	{
		ai->Log("No metal extractor unit known!");
		return;
	}

	const UnitDef* def = &ai->Getbt()->GetUnitDef(largestExtractor.id);
	const UnitFootprint largestExtractorFootprint = ai->s_buildTree.GetFootprint(largestExtractor);

	metalMap = false;
	bool Stopme = false;
	int TotalMetal = 0;
	int MaxMetal = 0;
	int TempMetal = 0;
	int SpotsFound = 0;
	int coordx = 0, coordy = 0;
//	float AverageMetal;

	AAIMetalSpot temp;
	float3 pos;

	int MinMetalForSpot = 30; // from 0-255, the minimum percentage of metal a spot needs to have
							//from the maximum to be saved. Prevents crappier spots in between taken spaces.
							//They are still perfectly valid and will generate metal mind you!
	int MaxSpots = 5000; //If more spots than that are found the map is considered a metalmap, tweak this as needed

	int MetalMapHeight = ai->GetAICallback()->GetMapHeight() / 2; //metal map has 1/2 resolution of normal map
	int MetalMapWidth = ai->GetAICallback()->GetMapWidth() / 2;
	int TotalCells = MetalMapHeight * MetalMapWidth;
	unsigned char XtractorRadius = ai->GetAICallback()->GetExtractorRadius()/ 16.0;
	unsigned char DoubleRadius = ai->GetAICallback()->GetExtractorRadius() / 8.0;
	int SquareRadius = (ai->GetAICallback()->GetExtractorRadius() / 16.0) * (ai->GetAICallback()->GetExtractorRadius() / 16.0); //used to speed up loops so no recalculation needed
	int DoubleSquareRadius = (ai->GetAICallback()->GetExtractorRadius() / 8.0) * (ai->GetAICallback()->GetExtractorRadius() / 8.0); // same as above
//	int CellsInRadius = PI * XtractorRadius * XtractorRadius; //yadda yadda
	unsigned char* MexArrayA = new unsigned char [TotalCells];
	unsigned char* MexArrayB = new unsigned char [TotalCells];
	int* TempAverage = new int [TotalCells];

	//Load up the metal Values in each pixel
	for (int i = 0; i != TotalCells - 1; i++)
	{
		MexArrayA[i] = *(ai->GetAICallback()->GetMetalMap() + i);
		TotalMetal += MexArrayA[i];		// Count the total metal so you can work out an average of the whole map
	}

//	AverageMetal = ((float)TotalMetal) / ((float)TotalCells);  //do the average

	// Now work out how much metal each spot can make by adding up the metal from nearby spots
	for (int y = 0; y != MetalMapHeight; y++)
	{
		for (int x = 0; x != MetalMapWidth; x++)
		{
			TotalMetal = 0;
			for (int myx = x - XtractorRadius; myx != x + XtractorRadius; myx++)
			{
				if (myx >= 0 && myx < MetalMapWidth)
				{
					for (int myy = y - XtractorRadius; myy != y + XtractorRadius; myy++)
					{
						if (myy >= 0 && myy < MetalMapHeight && ((x - myx)*(x - myx) + (y - myy)*(y - myy)) <= SquareRadius)
						{
							TotalMetal += MexArrayA[myy * MetalMapWidth + myx]; //get the metal from all pixels around the extractor radius
						}
					}
				}
			}
			TempAverage[y * MetalMapWidth + x] = TotalMetal; //set that spots metal making ability (divide by cells to values are small)
			if (MaxMetal < TotalMetal)
				MaxMetal = TotalMetal;  //find the spot with the highest metal to set as the map's max
		}
	}
	for (int i = 0; i != TotalCells; i++) // this will get the total metal a mex placed at each spot would make
	{
		MexArrayB[i] = spring::SafeDivide(TempAverage[i] * 255,  MaxMetal);  //scale the metal so any map will have values 0-255, no matter how much metal it has
	}

	for (int a = 0; a != MaxSpots; a++)
	{
		if(!Stopme)
			TempMetal = 0; //reset tempmetal so it can find new spots
		for (int i = 0; i != TotalCells; i=i+2)
		{			//finds the best spot on the map and gets its coords
			if (MexArrayB[i] > TempMetal && !Stopme)
			{
				TempMetal = MexArrayB[i];
				coordx = i%MetalMapWidth;
				coordy = i/MetalMapWidth;
			}
		}
		if (TempMetal < MinMetalForSpot)
			Stopme = true; // if the spots get too crappy it will stop running the loops to speed it all up

		if (!Stopme)
		{
			pos.x = coordx * 2 * SQUARE_SIZE;
			pos.z = coordy * 2 * SQUARE_SIZE;
			pos.y = ai->GetAICallback()->GetElevation(pos.x, pos.z);

			Pos2FinalBuildPos(&pos, def);

			temp.amount = TempMetal * ai->GetAICallback()->GetMaxMetal() * MaxMetal / 255.0f;
			temp.occupied = false;
			temp.pos = pos;

			//if(ai->Getcb()->CanBuildAt(def, pos))
			//{
				Pos2BuildMapPos(&pos, def);

				if(pos.z >= 2 && pos.x >= 2 && pos.x < xMapSize-2 && pos.z < yMapSize-2)
				{
					const bool water = temp.pos.y < 0.0f;
					if(CanBuildAt(pos.x, pos.z, largestExtractorFootprint, water))
					{
						metal_spots.push_back(temp);
						++SpotsFound;

						ChangeBuildMapOccupation(pos.x-2, pos.z-2, def->xsize+2, def->zsize+2, true);
					}
				}
			//}

			for (int myx = coordx - XtractorRadius; myx != coordx + XtractorRadius; myx++)
			{
				if (myx >= 0 && myx < MetalMapWidth)
				{
					for (int myy = coordy - XtractorRadius; myy != coordy + XtractorRadius; myy++)
					{
						if (myy >= 0 && myy < MetalMapHeight && ((coordx - myx)*(coordx - myx) + (coordy - myy)*(coordy - myy)) <= SquareRadius)
						{
							MexArrayA[myy * MetalMapWidth + myx] = 0; //wipes the metal around the spot so its not counted twice
							MexArrayB[myy * MetalMapWidth + myx] = 0;
						}
					}
				}
			}

			// Redo the whole averaging process around the picked spot so other spots can be found around it
			for (int y = coordy - DoubleRadius; y != coordy + DoubleRadius; y++)
			{
				if(y >=0 && y < MetalMapHeight)
				{
					for (int x = coordx - DoubleRadius; x != coordx + DoubleRadius; x++)
					{//funcion below is optimized so it will only update spots between r and 2r, greatly speeding it up
						if((coordx - x)*(coordx - x) + (coordy - y)*(coordy - y) <= DoubleSquareRadius && x >=0 && x < MetalMapWidth && MexArrayB[y * MetalMapWidth + x])
						{
							TotalMetal = 0;
							for (int myx = x - XtractorRadius; myx != x + XtractorRadius; myx++)
							{
								if (myx >= 0 && myx < MetalMapWidth)
								{
									for (int myy = y - XtractorRadius; myy != y + XtractorRadius; myy++)
									{
										if (myy >= 0 && myy < MetalMapHeight && ((x - myx)*(x - myx) + (y - myy)*(y - myy)) <= SquareRadius)
										{
											TotalMetal += MexArrayA[myy * MetalMapWidth + myx]; //recalculate nearby spots to account for deleted metal from chosen spot
										}
									}
								}
							}
							MexArrayB[y * MetalMapWidth + x] = spring::SafeDivide(TotalMetal * 255, MaxMetal); //set that spots metal amount
						}
					}
				}
			}
		}
	}

	if(SpotsFound > 500)
	{
		metalMap = true;
		metal_spots.clear();
		ai->Log("Map is considered to be a metal map\n");
	}
	else
		metalMap = false;

	spring::SafeDeleteArray(MexArrayA);
	spring::SafeDeleteArray(MexArrayB);
	spring::SafeDeleteArray(TempAverage);
}

void AAIMap::UpdateEnemyUnitsInLOS()
{
	//
	// reset scouted buildings for all cells within current los
	//
	const unsigned short *losMap = ai->GetAICallback()->GetLosMap();

	int cellIndex(0);
	for(int y = 0; y < yLOSMapSize; ++y)
	{
		for(int x = 0; x < xLOSMapSize; ++x)
		{
			if(losMap[cellIndex] > 0u)
			{
				m_scoutedEnemyUnitsMap[cellIndex]    = 0;
				m_lastLOSUpdateInFrameMap[cellIndex] = ai->GetAICallback()->GetCurrentFrame();
			}

			++cellIndex;
		}
	}

	for(int y = 0; y < ySectors; ++y)
	{
		for(int x = 0; x < xSectors; ++x)
			m_sector[x][y].m_enemyUnitsDetectedBySensor = 0;
	}

	// update enemy units
	MobileTargetTypeValues spottedEnemyCombatUnitsByTargetType;
	const int numberOfEnemyUnits = ai->GetAICallback()->GetEnemyUnitsInRadarAndLos(&(unitsInLOS.front()));

	for(int i = 0; i < numberOfEnemyUnits; ++i)
	{
		const float3   pos = ai->GetAICallback()->GetUnitPos(unitsInLOS[i]);
		const UnitDef* def = ai->GetAICallback()->GetUnitDef(unitsInLOS[i]);

		if(def) // unit is within los
		{
			const int x_pos = (int)pos.x / (losMapRes * SQUARE_SIZE);
			const int y_pos = (int)pos.z / (losMapRes * SQUARE_SIZE);

			// make sure unit is within the map (e.g. no aircraft that has flown outside of the map)
			if( (x_pos >= 0) && (x_pos < xLOSMapSize) && (y_pos >= 0) && (y_pos < yLOSMapSize) )
			{
				const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(UnitDefId(def->id));

				// add buildings/combat units to scout map
				if( category.isBuilding() || category.isCombatUnit() )
				{
					m_scoutedEnemyUnitsMap[x_pos + y_pos * xLOSMapSize] = def->id;
				}

				if(category.isCombatUnit())
				{
					const AAITargetType& targetType = ai->s_buildTree.GetTargetType(UnitDefId(def->id));
					spottedEnemyCombatUnitsByTargetType.AddValueForTargetType(targetType, 1.0f);
				}
			}
		}
		else // unit on radar only
		{
			AAISector* sector = GetSectorOfPos(pos);

			if(sector)
				sector->m_enemyUnitsDetectedBySensor += 1;
		}
	}

	ai->Getbrain()->UpdateMaxCombatUnitsSpotted(spottedEnemyCombatUnitsByTargetType);
}

void AAIMap::UpdateFriendlyUnitsInLos()
{
	for(int y = 0; y < ySectors; ++y)
	{
		for(int x = 0; x < xSectors; ++x)
			m_sector[x][y].ResetLocalCombatPower();
	}

	const int numberOfFriendlyUnits = ai->GetAICallback()->GetFriendlyUnits(&(unitsInLOS.front()));

	for(int i = 0; i < numberOfFriendlyUnits; ++i)
	{
		// get unit def & category
		const UnitDef* def = ai->GetAICallback()->GetUnitDef(unitsInLOS[i]);
		const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(UnitDefId(def->id));

		if( category.isBuilding() || category.isCombatUnit() )
		{
			AAISector* sector = GetSectorOfPos( ai->GetAICallback()->GetUnitPos(unitsInLOS[i]) );

			if(sector)
			{
				const bool unitBelongsToAlly( ai->GetAICallback()->GetUnitTeam(unitsInLOS[i]) != m_myTeamId );
				sector->AddFriendlyUnitData(UnitDefId(def->id), unitBelongsToAlly);
			}
		}
	}
}

void AAIMap::UpdateEnemyScoutingData()
{
	// map of known enemy buildings has been updated -> update sector data
	for(int y = 0; y < ySectors; ++y)
	{
		for(int x = 0; x < xSectors; ++x)
		{
			m_sector[x][y].ResetScoutedEnemiesData();

			for(int yCell = y * ySectorSizeMap/losMapRes; yCell < (y + 1) * ySectorSizeMap/losMapRes; ++yCell)
			{
				for(int xCell = x * xSectorSizeMap/losMapRes; xCell < (x + 1) * xSectorSizeMap/losMapRes; ++xCell)
				{
					const int cellIndex = xCell + yCell * xLOSMapSize;
					const UnitDefId unitDefId(m_scoutedEnemyUnitsMap[cellIndex]);

					if(unitDefId.isValid())
						m_sector[x][y].AddScoutedEnemyUnit(unitDefId, m_lastLOSUpdateInFrameMap[cellIndex]);
				}
			}
		}
	}
}

float3 AAIMap::DeterminePositionOfEnemyBuildingInSector(int xStart, int xEnd, int yStart, int yEnd) const
{
	for(int yCell = yStart/losMapRes; yCell < yEnd/losMapRes; ++yCell)
	{
		for(int xCell = xStart/losMapRes; xCell < xEnd/losMapRes; ++xCell)
		{	
			const int tileIndex = xCell + xLOSMapSize * yCell;
			const UnitDefId unitDefId(m_scoutedEnemyUnitsMap[tileIndex]);

			if(unitDefId.isValid())
			{
				if(ai->s_buildTree.GetUnitCategory(unitDefId).isBuilding())
				{
					float3 selectedPosition;
					selectedPosition.x = static_cast<float>(xCell * losMapRes * SQUARE_SIZE);
					selectedPosition.z = static_cast<float>(yCell * losMapRes * SQUARE_SIZE);
					selectedPosition.y = ai->GetAICallback()->GetElevation(selectedPosition.x, selectedPosition.z);
					return selectedPosition;
				}
			}
		}
	}

	ai->Log("Error: Could not find position of enemy building in sector (%i, %i) despite enemy buildings in sector!\n", xStart/xSectorSizeMap, yStart/ySectorSizeMap);

	float3 selectedPosition;
	selectedPosition.x = static_cast<float>(xStart * SQUARE_SIZE);
	selectedPosition.z = static_cast<float>(yStart * SQUARE_SIZE);
	selectedPosition.y = ai->GetAICallback()->GetElevation(selectedPosition.x, selectedPosition.z);
	return selectedPosition;
}

void AAIMap::UpdateSectors()
{
	for(int x = 0; x < xSectors; ++x)
	{
		for(int y = 0; y < ySectors; ++y)
			m_sector[x][y].DecreaseLostUnits();
	}
}

void AAIMap::UpdateNeighbouringSectors(std::vector< std::list<AAISector*> >& sectorsInDistToBase)
{
	// delete old values
	for(int x = 0; x < xSectors; ++x)
	{
		for(int y = 0; y < ySectors; ++y)
		{
			if(m_sector[x][y].distance_to_base > 0)
				m_sector[x][y].distance_to_base = -1;
		}
	}

	for(int i = 1; i < sectorsInDistToBase.size(); ++i)
	{
		// delete old sectors
		sectorsInDistToBase[i].clear();

		for(std::list<AAISector*>::iterator sector = sectorsInDistToBase[i-1].begin(); sector != sectorsInDistToBase[i-1].end(); ++sector)
		{
			const int x = (*sector)->x;
			const int y = (*sector)->y;

			// check left neighbour
			if( (x > 0) && (m_sector[x-1][y].distance_to_base == -1) )
			{
				m_sector[x-1][y].distance_to_base = i;
				sectorsInDistToBase[i].push_back(&m_sector[x-1][y]);
			}
			// check right neighbour
			if( (x < (xSectors - 1)) && (m_sector[x+1][y].distance_to_base == -1) )
			{
				m_sector[x+1][y].distance_to_base = i;
				sectorsInDistToBase[i].push_back(&m_sector[x+1][y]);
			}
			// check upper neighbour
			if( (y > 0) && (m_sector[x][y-1].distance_to_base == -1) )
			{
				m_sector[x][y-1].distance_to_base = i;
				sectorsInDistToBase[i].push_back(&m_sector[x][y-1]);
			}
			// check lower neighbour
			if( (y < (ySectors - 1)) && (m_sector[x][y+1].distance_to_base == -1) )
			{
				m_sector[x][y+1].distance_to_base = i;
				sectorsInDistToBase[i].push_back(&m_sector[x][y+1]);
			}
		}
	}
}

float3 AAIMap::GetNewScoutDest(UnitId scoutUnitId)
{
	const UnitDef* def = ai->GetAICallback()->GetUnitDef(scoutUnitId.id);
	const AAIMovementType& scoutMoveType = ai->s_buildTree.GetMovementType( UnitDefId(def->id) );
	
	const float3 currentPositionOfScout  = ai->GetAICallback()->GetUnitPos(scoutUnitId.id);
	const int    continentId             = scoutMoveType.CannotMoveToOtherContinents() ? ai->Getmap()->DetermineSmartContinentID(currentPositionOfScout, scoutMoveType) : AAIMap::ignoreContinentID;

	float3     selectedScoutDestination(ZeroVector);
	AAISector* selectedScoutSector(nullptr);
	float      highestRating(0.0f);

	for(int x = 0; x < xSectors; ++x)
	{
		for(int y = 0; y < ySectors; ++y)
		{
			const float rating = m_sector[x][y].GetRatingAsNextScoutDestination(scoutMoveType, currentPositionOfScout);

			if(rating > highestRating)
			{
				// possible scout dest, try to find pos in sector
				const float3 possibleScoutDestination = m_sector[x][y].DetermineUnitMovePos(scoutMoveType, continentId);

				if(possibleScoutDestination.x > 0.0f)
				{
					highestRating            = rating;
					selectedScoutSector      = &m_sector[x][y];
					selectedScoutDestination = possibleScoutDestination;
				}
			}
		}
	}

	// set dest sector as visited
	if(selectedScoutSector)
		selectedScoutSector->SelectedAsScoutDestination();

	return selectedScoutDestination;
}

const AAISector* AAIMap::DetermineSectorToContinueAttack(const AAISector *currentSector, const MobileTargetTypeValues& targetTypeOfUnits, AAIMovementType moveTypeOfUnits) const
{
	float highestRating(0.0f);
	const AAISector* selectedSector(nullptr);

	const bool landSectorSelectable  = moveTypeOfUnits.IsAir() || moveTypeOfUnits.IsHover() || moveTypeOfUnits.IsAmphibious() || moveTypeOfUnits.IsGround();
	const bool waterSectorSelectable = moveTypeOfUnits.IsAir() || moveTypeOfUnits.IsHover() || moveTypeOfUnits.IsSeaUnit();

	for(int x = 0; x < xSectors; x++)
	{
		for(int y = 0; y < ySectors; y++)
		{
			const float rating = m_sector[x][y].GetAttackRating(currentSector, landSectorSelectable, waterSectorSelectable, targetTypeOfUnits);
			
			if(rating > highestRating)
			{
				selectedSector = &m_sector[x][y];
				highestRating  = rating;
			}
		}
	}

	return selectedSector;
}

const AAISector* AAIMap::DetermineSectorToAttack(const std::vector<float>& globalCombatPower, const std::vector< std::vector<float> >& continentCombatPower, const MobileTargetTypeValues& assaultGroupsOfType) const
{
	const float maxLostUnits = GetMaximumNumberOfLostUnits();

	float highestRating(0.0f);
	const AAISector* selectedSector = nullptr;

	for(int x = 0; x < xSectors; ++x)
	{
		for(int y = 0; y < ySectors; ++y)
		{
			const AAISector* sector = &m_sector[x][y];

			if( (sector->distance_to_base > 0) && (sector->GetNumberOfEnemyBuildings() > 0))
			{
				const float myAttackPower     =   globalCombatPower[AAITargetType::staticIndex] + continentCombatPower[sector->continent][AAITargetType::staticIndex];
				const float enemyDefencePower =   assaultGroupsOfType.GetValueOfTargetType(ETargetType::SURFACE)   * sector->GetEnemyCombatPower(ETargetType::SURFACE)
												+ assaultGroupsOfType.GetValueOfTargetType(ETargetType::FLOATER)   * sector->GetEnemyCombatPower(ETargetType::FLOATER)
												+ assaultGroupsOfType.GetValueOfTargetType(ETargetType::SUBMERGED) * sector->GetEnemyCombatPower(ETargetType::SUBMERGED);
				
				const float lostUnitsFactor = (maxLostUnits > 1.0f) ? (2.0f - (sector->GetLostUnits() / maxLostUnits) ) : 1.0f;

				const float enemyBuildings = static_cast<float>(sector->GetNumberOfEnemyBuildings());

				// prefer sectors with many buildings, few lost units and low defence power/short distance to own base
				float rating = lostUnitsFactor * enemyBuildings * myAttackPower / ( (0.1f + enemyDefencePower) * (float)(2 + sector->distance_to_base) );
	
				if(rating > highestRating)
				{
					selectedSector = sector;
					highestRating  = rating;
				}
			}
		}
	}

	return selectedSector;
}

const char* AAIMap::GetMapTypeString(const AAIMapType& mapType) const
{
	if(mapType.IsLandMap())
		return "LAND_MAP";
	else if(mapType.IsLandWaterMap())
		return "LAND_WATER_MAP";
	else if(mapType.IsWaterMap())
		return "WATER_MAP";
	else
		return "UNKNOWN_MAP";
}

AAISector* AAIMap::GetSectorOfPos(const float3& pos)
{
	const int x = pos.x/xSectorSize;
	const int y = pos.z/ySectorSize;

	if(IsValidSector(x,y))
		return &(m_sector[x][y]);
	else
		return nullptr;
}

void AAIMap::AddStaticDefence(const float3& position, UnitDefId defence)
{
	const int range = static_cast<int>( ai->s_buildTree.GetMaxRange(defence) ) / (SQUARE_SIZE * 4);
	const int xPos = (position.x + ai->Getbt()->GetUnitDef(defence.id).xsize/2)/ (SQUARE_SIZE * 4);
	const int yPos = (position.z + ai->Getbt()->GetUnitDef(defence.id).zsize/2)/ (SQUARE_SIZE * 4);

	// x range will change from line to line -  y range is const
	int yStart = yPos - range;
	int yEnd = yPos + range;

	if(yStart < 0)
		yStart = 0;
	if(yEnd > yDefMapSize)
		yEnd = yDefMapSize;

	for(int y = yStart; y < yEnd; ++y)
	{
		// determine x-range
		int xRange = (int) floor( fastmath::apxsqrt2( (float) ( std::max(1, range * range - (y - yPos) * (y - yPos)) ) ) + 0.5f );

		int xStart = xPos - xRange;
		int xEnd = xPos + xRange;

		if(xStart < 0)
			xStart = 0;
		if(xEnd > xDefMapSize)
			xEnd = xDefMapSize;

		for(int x = xStart; x < xEnd; ++x)
		{
			const int cell = x + xDefMapSize*y;

			const AAICombatPower& combatPower = ai->s_buildTree.GetCombatPower(defence);

			defence_map[cell]           += combatPower.GetCombatPowerVsTargetType(ETargetType::SURFACE);
			air_defence_map[cell]       += combatPower.GetCombatPowerVsTargetType(ETargetType::AIR);
			submarine_defence_map[cell] += combatPower.GetCombatPowerVsTargetType(ETargetType::FLOATER) + combatPower.GetCombatPowerVsTargetType(ETargetType::SUBMERGED);
		}
	}

	// further increase values close around the bulding (to prevent aai from packing buildings too close together)
	int xStart = xPos - 2;
	int xEnd = xPos + 2;
	yStart = yPos - 2;
	yEnd = yPos + 2;

	if(xStart < 0)
		xStart = 0;
	if(xEnd >= xDefMapSize)
		xEnd = xDefMapSize-1;

	if(yStart < 0)
		yStart = 0;
	if(yEnd >= yDefMapSize)
		yEnd = yDefMapSize-1;

	for(int y = yStart; y <= yEnd; ++y)
	{
		for(int x = xStart; x <= xEnd; ++x)
		{
			const int cell = x + xDefMapSize*y;

			defence_map[cell] += 5000.0f;
			air_defence_map[cell] += 5000.0f;
			submarine_defence_map[cell] += 5000.0f;

			/*float3 my_pos;
			my_pos.x = x * 32;
			my_pos.z = y * 32;
			my_pos.y = ai->Getcb()->GetElevation(my_pos.x, my_pos.z);
			ai->Getcb()->DrawUnit("ARMMINE1", my_pos, 0.0f, 4000, ai->Getcb()->GetMyAllyTeam(), false, true);
			my_pos.x = (x+1) * 32;
			my_pos.z = (y+1) * 32;
			my_pos.y = ai->Getcb()->GetElevation(my_pos.x, my_pos.z);
			ai->Getcb()->DrawUnit("ARMMINE1", my_pos, 0.0f, 4000, ai->Getcb()->GetMyAllyTeam(), false, true);*/
		}
	}

	const std::string filename = cfg->GetFileName(ai->GetAICallback(), "AAIDefMap.txt", "", "", true);
	FILE* file = fopen(filename.c_str(), "w+");
	for(int y = 0; y < yDefMapSize; ++y)
	{
		for(int x = 0; x < xDefMapSize; ++x)
		{
			fprintf(file, "%i ", (int) defence_map[x + y *xDefMapSize]);
		}

		fprintf(file, "\n");
	}
	fclose(file);
}

void AAIMap::RemoveDefence(const float3& pos, UnitDefId defence)
{
	const int range = static_cast<int>( ai->s_buildTree.GetMaxRange(defence) ) / (SQUARE_SIZE * 4);

	int xPos = (pos.x + ai->Getbt()->GetUnitDef(defence.id).xsize/2) / (SQUARE_SIZE * 4);
	int yPos = (pos.z + ai->Getbt()->GetUnitDef(defence.id).zsize/2) / (SQUARE_SIZE * 4);

	// further decrease values close around the bulding (to prevent aai from packing buildings too close together)
	int xStart = xPos - 2;
	int xEnd = xPos + 2;
	int yStart = yPos - 2;
	int yEnd = yPos + 2;

	if(xStart < 0)
		xStart = 0;
	if(xEnd >= xDefMapSize)
		xEnd = xDefMapSize-1;

	if(yStart < 0)
		yStart = 0;
	if(yEnd >= yDefMapSize)
		yEnd = yDefMapSize-1;

	for(int y = yStart; y <= yEnd; ++y)
	{
		for(int x = xStart; x <= xEnd; ++x)
		{
			const int cell = x + xDefMapSize*y;

			defence_map[cell] -= 5000.0f;
			air_defence_map[cell] -= 5000.0f;
			submarine_defence_map[cell] -= 5000.0f;
		}
	}

	// y range is const
	yStart = yPos - range;
	yEnd = yPos + range;

	if(yStart < 0)
		yStart = 0;
	if(yEnd > yDefMapSize)
		yEnd = yDefMapSize;

	for(int y = yStart; y < yEnd; ++y)
	{
		// determine x-range
		int xRange = (int) floor( fastmath::apxsqrt2( (float) ( std::max(1, range * range - (y - yPos) * (y - yPos)) ) ) + 0.5f );

		xStart = xPos - xRange;
		xEnd = xPos + xRange;

		if(xStart < 0)
			xStart = 0;
		if(xEnd > xDefMapSize)
			xEnd = xDefMapSize;

		for(int x = xStart; x < xEnd; ++x)
		{
			const int cell = x + xDefMapSize*y;
			
			const AAICombatPower& combatPower = ai->s_buildTree.GetCombatPower(defence);

			defence_map[cell]           -= combatPower.GetCombatPowerVsTargetType(ETargetType::SURFACE);
			air_defence_map[cell]       -= combatPower.GetCombatPowerVsTargetType(ETargetType::AIR);
			submarine_defence_map[cell] -= (combatPower.GetCombatPowerVsTargetType(ETargetType::FLOATER) + combatPower.GetCombatPowerVsTargetType(ETargetType::SUBMERGED));

			if(defence_map[cell] < 0.0f)
				defence_map[cell] = 0.0f;

			if(air_defence_map[cell] < 0.0f)
				air_defence_map[cell] = 0.0f;

			if(submarine_defence_map[cell] < 0.0f)
				submarine_defence_map[cell] = 0.0f;
		}
	}
}

int AAIMap::GetContinentID(int x, int y) const
{
	return continent_map[(y/4) * xContMapSize + x / 4];
}

int AAIMap::GetContinentID(const float3& pos) const
{
	int x = static_cast<int>(pos.x) / 32;
	int y = static_cast<int>(pos.z) / 32;

	// check if pos inside of the map
	if(x < 0)
		x = 0;
	else if(x >= xContMapSize)
		x = xContMapSize - 1;

	if(y < 0)
		y = 0;
	else if(y >= yContMapSize)
		y = yContMapSize - 1;

	return continent_map[y * xContMapSize + x];
}

int AAIMap::DetermineSmartContinentID(float3 pos, const AAIMovementType& moveType) const
{
	// check if non sea/amphib unit in shallow water
	if(     (ai->GetAICallback()->GetElevation(pos.x, pos.z) < 0)
	     && (moveType.GetMovementType() == EMovementType::MOVEMENT_TYPE_GROUND) )
	{
		//look for closest land cell
		for(int k = 1; k < 10; ++k)
		{
			if(ai->GetAICallback()->GetElevation(pos.x + k * 16, pos.z) > 0)
			{
				pos.x += k *16;
				break;
			}
			else if(ai->GetAICallback()->GetElevation(pos.x - k * 16, pos.z) > 0)
			{
				pos.x -= k *16;
				break;
			}
			else if(ai->GetAICallback()->GetElevation(pos.x, pos.z + k * 16) > 0)
			{
				pos.z += k *16;
				break;
			}
			else if(ai->GetAICallback()->GetElevation(pos.x, pos.z - k * 16) > 0)
			{
				pos.z -= k *16;
				break;
			}
		}
	}

	int x = pos.x / 32;
	int y = pos.z / 32;

	// ensure determined position lies inside of the map
	if(x < 0)
		x = 0;
	else if(x >= xContMapSize)
		x = xContMapSize - 1;

	if(y < 0)
		y = 0;
	else if(y >= yContMapSize)
		y = yContMapSize - 1;

	return continent_map[x + y * xContMapSize];
}

uint32_t AAIMap::GetSuitableMovementTypes(const AAIMapType& mapType) const
{
	// always: MOVEMENT_TYPE_AIR, MOVEMENT_TYPE_AMPHIB, MOVEMENT_TYPE_HOVER;
	uint32_t suitableMovementTypes =  static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_AIR)
									+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_AMPHIBIOUS)
									+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_HOVER);
	
	// MOVEMENT_TYPE_GROUND allowed on non water maps (i.e. map contains land)
	if(mapType.IsWaterMap() == false)
		suitableMovementTypes |= static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_GROUND);

	// MOVEMENT_TYPE_SEA_FLOATER/SUBMERGED allowed on non land maps (i.e. map contains water)
	if(mapType.IsLandMap() == false)
	{
		suitableMovementTypes |= static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_SEA_FLOATER);
		suitableMovementTypes |= static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_SEA_SUBMERGED);	
	}

	return suitableMovementTypes;
}

int AAIMap::GetEdgeDistance(int xPos, int yPos) const
{
	int edge_distance = xPos;

	if(xMapSize - xPos < edge_distance)
		edge_distance = xMapSize - xPos;

	if(yPos < edge_distance)
		edge_distance = yPos;

	if(yMapSize - yPos < edge_distance)
		edge_distance = yMapSize - yPos;

	return edge_distance;
}

float AAIMap::GetEdgeDistance(float3 *pos)
{
	float edge_distance = pos->x;

	if(xSize - pos->x < edge_distance)
		edge_distance = xSize - pos->x;

	if(pos->z < edge_distance)
		edge_distance = pos->z;

	if(ySize - pos->z < edge_distance)
		edge_distance = ySize - pos->z;

	return edge_distance;
}

float AAIMap::GetMaximumNumberOfLostUnits() const
{
	float maxLostUnits(0.0f);

	for(int x = 0; x < ai->Getmap()->xSectors; ++x)
	{
		for(int y = 0; y < ai->Getmap()->ySectors; ++y)
		{
			const float lostUnits = ai->Getmap()->m_sector[x][y].GetLostUnits();

			if(lostUnits > maxLostUnits)
				maxLostUnits = lostUnits;
		}
	}

	return maxLostUnits;
}
