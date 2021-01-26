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
int AAIMap::xSectors;
int AAIMap::ySectors;
int AAIMap::xSectorSize;
int AAIMap::ySectorSize;
int AAIMap::xSectorSizeMap;
int AAIMap::ySectorSizeMap;

bool AAIMap::metalMap;
std::list<AAIMetalSpot>  AAIMap::metal_spots;
int AAIMap::land_metal_spots;
int AAIMap::water_metal_spots;

float AAIMap::s_landTilesRatio;
float AAIMap::flat_land_ratio;
float AAIMap::s_waterTilesRatio;

AAIContinentMap               AAIMap::s_continentMap;
AAIDefenceMaps                AAIMap::s_defenceMaps;
AAIMapType                    AAIMap::s_mapType;
AAITeamSectorMap              AAIMap::s_teamSectorMap;
std::vector<BuildMapTileType> AAIMap::s_buildmap;
std::vector<int>              AAIMap::blockmap;
std::vector<float>            AAIMap::plateau_map;

std::vector<AAIContinent> AAIMap::s_continents;
int AAIMap::land_continents;
int AAIMap::water_continents;
int AAIMap::avg_land_continent_size;
int AAIMap::avg_water_continent_size;
int AAIMap::max_land_continent_size;
int AAIMap::max_water_continent_size;
int AAIMap::min_land_continent_size;
int AAIMap::min_water_continent_size;

AAIMap::AAIMap(AAI *ai) :
	m_lastLOSUpdateInFrame(0)
{
	this->ai = ai;
}

AAIMap::~AAIMap(void)
{
	UpdateLearningData();

	// delete common data only if last AAI instance is deleted
	if(ai->GetNumberOfAAIInstances() == 0)
	{
		ai->Log("Saving map learn file\n");

		const std::string mapLearningDataFilename = LocateMapLearnFile();

		// save map data
		FILE *file = fopen(mapLearningDataFilename.c_str(), "w+");

		fprintf(file, "%s\n", MAP_LEARN_VERSION);

		for(int y = 0; y < ySectors; ++y)
		{
			for(int x = 0; x < xSectors; ++x)
				m_sector[x][y].SaveDataToFile(file);

			fprintf(file, "\n");
		}

		fclose(file);

		s_buildmap.clear();
		blockmap.clear();
		plateau_map.clear();
	}

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

		// calculate number of sectors
		xSectors = floor(0.5f + ((float) xMapSize)/AAIConstants::sectorSize);
		ySectors = floor(0.5f + ((float) yMapSize)/AAIConstants::sectorSize);

		// calculate effective sector size
		xSectorSizeMap = floor( ((float) xMapSize) / ((float) xSectors) );
		ySectorSizeMap = floor( ((float) yMapSize) / ((float) ySectors) );

		xSectorSize = xSectorSizeMap * SQUARE_SIZE;
		ySectorSize = ySectorSizeMap * SQUARE_SIZE;

		s_buildmap.resize(xMapSize*yMapSize);
		blockmap.resize(xMapSize*yMapSize, 0);
		plateau_map.resize(xMapSize/4*xMapSize/4, 0.0f);

		s_teamSectorMap.Init(xSectors, ySectors);

		s_defenceMaps.Init(xMapSize, yMapSize);

		s_continentMap.Init(xMapSize, yMapSize);

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
	for(auto spot = metal_spots.begin(); spot != metal_spots.end(); ++spot)
	{
		AAISector* sector = GetSectorOfPos(spot->pos);

		if(sector)
			sector->AddMetalSpot(&(*spot));
	}

	ReadMapLearnFile();

	// for scouting
	m_scoutedEnemyUnitsMap.Init(xMapSize, yMapSize, losMapRes);
	m_buildingsOnContinent.resize(s_continents.size(), 0);

	unitsInLOS.resize(cfg->MAX_UNITS, 0);

	m_centerOfEnemyBase.x = xMapSize/2;
	m_centerOfEnemyBase.y = yMapSize/2;

	// for log file
	ai->Log("Map: %s\n",ai->GetAICallback()->GetMapName());
	ai->Log("Maptype: %s\n", s_mapType.GetName().c_str());
	ai->Log("Land / water ratio: : %f / %f\n", s_landTilesRatio, s_waterTilesRatio);
	ai->Log("Mapsize is %i x %i\n", ai->GetAICallback()->GetMapWidth(),ai->GetAICallback()->GetMapHeight());
	ai->Log("%i sectors in x direction\n", xSectors);
	ai->Log("%i sectors in y direction\n", ySectors);
	ai->Log("x-sectorsize is %i (Map %i)\n", xSectorSize, xSectorSizeMap);
	ai->Log("y-sectorsize is %i (Map %i)\n", ySectorSize, ySectorSizeMap);
	ai->Log( _STPF_ " metal spots found (%i are on land, %i under water) \n \n", metal_spots.size(), land_metal_spots, water_metal_spots);
	ai->Log( _STPF_ " continents found on map\n", s_continents.size());
	ai->Log("%i land and %i water continents\n", land_continents, water_continents);
	ai->Log("Average land continent size is %i\n", avg_land_continent_size);
	ai->Log("Average water continent size is %i\n", avg_water_continent_size);

	//debug
	/*for(int x = 0; x < xMapSize; x+=4)
	{
		for(int y = 0; y < yMapSize; y+=4)
		{
			//if((buildmap[x + y*xMapSize] == 1) || (buildmap[x + y*xMapSize] == 5) )
			if(s_continentMap.GetContinentID(MapPos(x, y)) == 1)
			{
				float3 myPos;
				myPos.x = x;
				myPos.z = y;
				BuildMapPos2Pos(&myPos, ai->GetAICallback()->GetUnitDef("armmine1")); 
				myPos.y = ai->GetAICallback()->GetElevation(myPos.x, myPos.z);
				ai->GetAICallback()->DrawUnit("armmine1", myPos, 0.0f, 4000, ai->GetAICallback()->GetMyAllyTeam(), true, true);	
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
				s_mapType.SetMapType(EMapType::LAND);
			else if(!strcmp(buffer, "LAND_WATER_MAP"))
				s_mapType.SetMapType(EMapType::LAND_WATER);
			else if(!strcmp(buffer, "WATER_MAP"))
				s_mapType.SetMapType(EMapType::WATER);
			else
				s_mapType.SetMapType(EMapType::UNKNOWN);

			ai->LogConsole("%s loaded", s_mapType.GetName().c_str());

			// load water ratio
			fscanf(file, "%f ", &s_waterTilesRatio);

			// load buildmap
			for(int y = 0; y < yMapSize; ++y)
			{
				for(int x = 0; x < xMapSize; ++x)
				{
					unsigned int value;
					fscanf(file, "%u", &value);

					const int cell = x + y * xMapSize;
					s_buildmap[cell].m_tileType = static_cast<uint8_t>(value);
				}
			}

			//const springLegacyAI::UnitDef* def = ai->GetAICallback()->GetUnitDef("armmine1");

			// load plateau map
			for(int y = 0; y < yMapSize/4; ++y)
			{
				for(int x = 0; x < xMapSize/4; ++x)
				{
					const int cell = x + y * (xMapSize/4);
					fscanf(file, "%f ", &plateau_map[cell]);

					/*if(	plateau_map[cell] > 0.0f)
					{
						float3 myPos(static_cast<float>(x*4), 0.0, static_cast<float>(y*4) );
						BuildMapPos2Pos(&myPos, def); 
						myPos.y = ai->GetAICallback()->GetElevation(myPos.x, myPos.z);
						ai->GetAICallback()->DrawUnit("armmine1", myPos, 0.0f, 1000, ai->GetAICallback()->GetMyAllyTeam(), true, true);	
					}*/
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
		// detect cliffs/water and create plateau map
		AnalyseMap();

		DetermineMapType();

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
		fprintf(file, "%f\n", s_waterTilesRatio);

		// save buildmap
		for(int y = 0; y < yMapSize; ++y)
		{
			for(int x = 0; x < xMapSize; ++x)
			{
				const int cell = x + y * xMapSize;
				fprintf(file, "%u ", s_buildmap[cell].m_tileType);
			}
			fprintf(file, "\n");
		}

		// save plateau map
		for(int y = 0; y < yMapSize/4; ++y)
		{
			for(int x = 0; x < xMapSize/4; ++x)
			{
				const int cell = x + y * (xMapSize/4);
				fprintf(file, "%f ", plateau_map[cell]);
			}
			fprintf(file, "\n");
		}
			
		// save mex spots
		land_metal_spots = 0;
		water_metal_spots = 0;

		fprintf(file, "%u\n", static_cast<unsigned int>(metal_spots.size()) );

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
	const std::string filename = cfg->GetFileName(ai->GetAICallback(), cfg->GetUniqueName(ai->GetAICallback(), true, true, true, true), MAP_CACHE_PATH, "_continent.dat", true);
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
			s_continentMap.LoadFromFile(file);

			// load continents
			fscanf(file, "%i ", &temp);

			s_continents.resize(temp);

			for(int i = 0; i < temp; ++i)
			{
				fscanf(file, "%i %i ", &s_continents[i].size, &temp2);

				s_continents[i].water = (bool) temp2;
				s_continents[i].id = i;
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
	const float *heightMap = ai->GetAICallback()->GetHeightMap();
	s_continentMap.DetectContinents(s_continents, heightMap, xMapSize, yMapSize);

	// calculate some statistical data
	land_continents = 0;
	water_continents = 0;

	avg_land_continent_size = 0;
	avg_water_continent_size = 0;
	max_land_continent_size = 0;
	max_water_continent_size = 0;
	min_land_continent_size = s_continentMap.GetSize();
	min_water_continent_size = s_continentMap.GetSize();

	for(size_t i = 0; i < s_continents.size(); ++i)
	{
		if(s_continents[i].water)
		{
			++water_continents;
			avg_water_continent_size += s_continents[i].size;

			if(s_continents[i].size > max_water_continent_size)
				max_water_continent_size = s_continents[i].size;

			if(s_continents[i].size < min_water_continent_size)
				min_water_continent_size = s_continents[i].size;
		}
		else
		{
			++land_continents;
			avg_land_continent_size += s_continents[i].size;

			if(s_continents[i].size > max_land_continent_size)
				max_land_continent_size = s_continents[i].size;

			if(s_continents[i].size < min_land_continent_size)
				min_land_continent_size = s_continents[i].size;
		}
	}

	if(water_continents > 0)
		avg_water_continent_size /= water_continents;

	if(land_continents > 0)
		avg_land_continent_size /= land_continents;

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// save movement maps
	const std::string movementfile = cfg->GetFileName(ai->GetAICallback(), cfg->GetUniqueName(ai->GetAICallback(), true, false, true, false), MAP_CACHE_PATH, "_movement.dat", true);
	file = fopen(movementfile.c_str(), "w+");

	fprintf(file, "%s\n",  CONTINENT_DATA_VERSION);

	// save continent map
	s_continentMap.SaveToFile(file);

	// save continents
	fprintf(file, "\n%u\n", static_cast<unsigned int>(s_continents.size()) );

	for(size_t c = 0; c < s_continents.size(); ++c)
		fprintf(file, "%i %i \n", s_continents[c].size, (int)s_continents[c].water);

	// save statistical data
	fprintf(file, "%i %i %i %i %i %i %i %i\n", land_continents, water_continents, avg_land_continent_size, avg_water_continent_size,
																	max_land_continent_size, max_water_continent_size,
																	min_land_continent_size, min_water_continent_size);

	fclose(file);
}

std::string AAIMap::LocateMapLearnFile() const
{
	return cfg->GetFileName(ai->GetAICallback(), cfg->GetUniqueName(ai->GetAICallback(), true, true, true, true), MAP_LEARN_PATH, "_maplearn.dat", true);
}

std::string AAIMap::LocateMapCacheFile() const
{
	return cfg->GetFileName(ai->GetAICallback(), cfg->GetUniqueName(ai->GetAICallback(), false, false, true, true), MAP_LEARN_PATH, "_mapcache.dat", true);
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
			AAIMapType mapType(EMapType::LAND);

			if(m_sector[i][j].GetWaterTilesRatio() > 0.7f)
				mapType.SetMapType(EMapType::WATER);
			else if(m_sector[i][j].GetWaterTilesRatio() > 0.3f)
				mapType.SetMapType(EMapType::LAND_WATER);

			m_sector[i][j].m_suitableMovementTypes = GetSuitableMovementTypes(mapType);
		}
	}

    //-----------------------------------------------------------------------------------------------------------------
	// determine land/water ratio of total map
	//-----------------------------------------------------------------------------------------------------------------
	flat_land_ratio = 0.0f;
	s_waterTilesRatio     = 0.0f;

	for(int j = 0; j < ySectors; ++j)
	{
		for(int i = 0; i < xSectors; ++i)
		{
			flat_land_ratio += m_sector[i][j].GetFlatTilesRatio();
			s_waterTilesRatio += m_sector[i][j].GetWaterTilesRatio();
		}
	}

	flat_land_ratio /= (float)(xSectors * ySectors);
	s_waterTilesRatio     /= (float)(xSectors * ySectors);
	s_landTilesRatio = 1.0f - s_waterTilesRatio;

	if(load_file)
		fclose(load_file);
	else
		ai->LogConsole("New map-learning file created");
}

void AAIMap::UpdateLearningData()
{
	for(int y = 0; y < ySectors; ++y)
	{
		for(int x = 0; x < xSectors; ++x)
		{
			m_sector[x][y].UpdateLearnedData();
		}
	}
}

bool AAIMap::IsSectorBorderToBase(int x, int y) const
{
	return     (m_sector[x][y].m_distanceToBase > 0) 
			&& (m_sector[x][y].m_alliedBuildings < 5) 
			&& (s_teamSectorMap.IsOccupiedByTeam(x, y, ai->GetMyTeamId()) == false);
}

int AAIMap::DetermineSmartContinentID(float3 pos, const AAIMovementType& moveType) const
{
	// check if non sea/amphib unit in shallow water
	if(     (ai->GetAICallback()->GetElevation(pos.x, pos.z) < 0.0f)
	     && (moveType.GetMovementType() == EMovementType::MOVEMENT_TYPE_GROUND) )
	{
		//look for closest land cell
		for(int k = 1; k < 10; ++k)
		{
			if(ai->GetAICallback()->GetElevation(pos.x + k * 16, pos.z) >= 0.0f)
			{
				pos.x += static_cast<float>(k * 16);
				break;
			}
			else if(ai->GetAICallback()->GetElevation(pos.x - k * 16, pos.z) >= 0.0f)
			{
				pos.x -= static_cast<float>(k * 16);
				break;
			}
			else if(ai->GetAICallback()->GetElevation(pos.x, pos.z + k * 16) >= 0.0f)
			{
				pos.z += static_cast<float>(k * 16);
				break;
			}
			else if(ai->GetAICallback()->GetElevation(pos.x, pos.z - k * 16) >= 0.0f)
			{
				pos.z -= static_cast<float>(k * 16);
				break;
			}
		}
	}

	return s_continentMap.GetContinentID(pos);
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

void AAIMap::ConvertPositionToFinalBuildsite(float3& buildsite, const UnitFootprint& footprint) const
{
	if(footprint.xSize&2) // check if xSize is a multiple of 4
		buildsite.x = floor( (buildsite.x)   / (2*SQUARE_SIZE) ) * 2 * SQUARE_SIZE + 8;
	else
		buildsite.x = floor( (buildsite.x+8) / (2*SQUARE_SIZE) ) * 2 * SQUARE_SIZE;

	if(footprint.ySize&2) // check if ySize is a multiple of 4
		buildsite.z = floor( (buildsite.z)   / (2*SQUARE_SIZE) ) * 2 * SQUARE_SIZE + 8;
	else
		buildsite.z = floor( (buildsite.z+8) / (2*SQUARE_SIZE) ) * 2 * SQUARE_SIZE;
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
				s_buildmap[x+y*xMapSize].OccupyTile();
			else
				s_buildmap[x+y*xMapSize].FreeTile();

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

float3 AAIMap::FindRandomBuildsite(UnitDefId unitDefId, int xStart, int xEnd, int yStart, int yEnd, int tries) const
{
	const UnitFootprint footprint = DetermineRequiredFreeBuildspace(unitDefId);

	const int randomXRange = xEnd - xStart - footprint.xSize;
	const int randomYRange = yEnd - yStart - footprint.ySize;

	for(int i = 0; i < tries; i++)
	{
		// determine random position within given rectangle
		MapPos mapPos(xStart, yStart);

		if( randomXRange > 0)
			mapPos.x += rand()%randomXRange;

		if( randomYRange > 0)
			mapPos.y += rand()%randomYRange;

		// check if buildmap allows construction
		if(CanBuildAt(mapPos, footprint))
		{	
			float3 possibleBuildsite;
			ConvertMapPosToUnitPos(mapPos, possibleBuildsite, footprint);
			ConvertPositionToFinalBuildsite(possibleBuildsite, footprint);

			const springLegacyAI::UnitDef* unitDef  = &ai->Getbt()->GetUnitDef(unitDefId.id);
			if(ai->GetAICallback()->CanBuildAt(unitDef, possibleBuildsite))
			{
				const int x = possibleBuildsite.x/xSectorSize;
				const int y = possibleBuildsite.z/ySectorSize;

				if(IsValidSector(x,y))
					return possibleBuildsite;

			}
		}
	}

	return ZeroVector;
}

float3 AAIMap::FindBuildsiteCloseToUnit(UnitDefId buildingDefId, UnitId unitId) const
{
	const float3 unitPosition = ai->GetAICallback()->GetUnitPos(unitId.id);

	const UnitFootprint            footprint = DetermineRequiredFreeBuildspace(buildingDefId);
	const springLegacyAI::UnitDef* unitDef   = &ai->Getbt()->GetUnitDef(buildingDefId.id);

	// check rect
	const int xStart = unitPosition.x / SQUARE_SIZE;
	const int yStart = unitPosition.z / SQUARE_SIZE;

	for(int xInc = 0; xInc < 24; xInc += 2)
	{
		for(int yInc = 0; yInc < 24; yInc += 2)
		{
			float3 buildsite = CheckConstructionAt(footprint, unitDef, MapPos(xStart - xInc, yStart - yInc) );

			if(buildsite.x == 0.0f)
				buildsite = CheckConstructionAt(footprint, unitDef, MapPos(xStart - xInc, yStart + yInc) );

			if(buildsite.x == 0.0f)
				buildsite = CheckConstructionAt(footprint, unitDef, MapPos(xStart + xInc, yStart - yInc) );

			if(buildsite.x == 0.0f)
				buildsite = CheckConstructionAt(footprint, unitDef, MapPos(xStart + xInc, yStart + yInc) );

			if(buildsite.x > 0.0f)
				return buildsite;
		}
	}

	return ZeroVector;
}

float3 AAIMap::DetermineBuildsiteInSector(UnitDefId buildingDefId, const AAISector* sector) const
{
	int xStart, xEnd, yStart, yEnd;
	sector->DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);

	const UnitFootprint            footprint = DetermineRequiredFreeBuildspace(buildingDefId);
	const springLegacyAI::UnitDef* def       = &ai->Getbt()->GetUnitDef(buildingDefId.id);

	// check rect
	for(int yPos = yStart; yPos < yEnd; yPos += 2)
	{
		for(int xPos = xStart; xPos < xEnd; xPos += 2)
		{
			const MapPos mapPos(xPos, yPos);

			// check if buildmap allows construction
			if(CanBuildAt(mapPos, footprint))
			{
				float3 possibleBuildsite;
				ConvertMapPosToUnitPos(mapPos, possibleBuildsite, footprint);
				ConvertPositionToFinalBuildsite(possibleBuildsite, footprint);

				if(ai->GetAICallback()->CanBuildAt(def, possibleBuildsite))
				{
					const int x = possibleBuildsite.x/xSectorSize;
					const int y = possibleBuildsite.z/ySectorSize;

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
			const MapPos mapPos(xPos, yPos);

			if(CanBuildAt(mapPos, footprint))
			{
				const float edgeDist = static_cast<float>(GetEdgeDistance(xPos, yPos)) / range;

				float rating = 0.04f * (float)(rand()%50) + edgeDist;

				if(!water)
				{
					const int plateauMapCellIndex = xPos/4 + yPos/4 * (xMapSize/4);
					rating += plateau_map[plateauMapCellIndex];
				}
					
				if(rating > highestRating)
				{
					float3 possibleBuildsite;
					ConvertMapPosToUnitPos(mapPos, possibleBuildsite, footprint);
					ConvertPositionToFinalBuildsite(possibleBuildsite, footprint);

					if(ai->GetAICallback()->CanBuildAt(def, possibleBuildsite))
					{
						selectedPosition = possibleBuildsite;
						highestRating = rating;
					}
				}
			}
		}
	}

	return selectedPosition;
}

float3 AAIMap::DetermineBuildsiteForStaticDefence(UnitDefId staticDefence, const AAISector* sector, const AAITargetType& targetType, float terrainModifier) const
{
	const springLegacyAI::UnitDef *def = &ai->Getbt()->GetUnitDef(staticDefence.id);

	const int           range     = static_cast<int>(ai->s_buildTree.GetMaxRange(staticDefence)) / SQUARE_SIZE;
	const UnitFootprint footprint = DetermineRequiredFreeBuildspace(staticDefence);

	//-----------------------------------------------------------------------------------------------------------------
	// determine search horizontal and vertical search range
	//-----------------------------------------------------------------------------------------------------------------
	const int xStart =  sector->x    * xSectorSizeMap;
	const int xEnd   = (sector->x+1) * xSectorSizeMap;
	const int yStart =  sector->y    * ySectorSizeMap;
	const int yEnd   = (sector->y+1) * ySectorSizeMap;

	//-----------------------------------------------------------------------------------------------------------------
	// determine distances to center of base of tiles to be checked and statistcis (for calculation of rating later)
	//-----------------------------------------------------------------------------------------------------------------
	const int numberOfTilesToBeChecked = (1+(xEnd-xStart)/4) * (1+(yEnd-yStart)/4);
	std::vector<float> distancesToBaseCenter(numberOfTilesToBeChecked);
	StatisticalData distanceStatistics;

	int index(0);
	const MapPos& baseCenter = ai->Getbrain()->GetCenterOfBase();

	for(int yPos = yStart; yPos < yEnd; yPos += 4)
	{
		for(int xPos = xStart; xPos < xEnd; xPos += 4)
		{
			const int dx = xPos - baseCenter.x;
			const int dy = yPos - baseCenter.y;
			const float squaredDist = static_cast<float>(dx*dx + dy*dy);

			distancesToBaseCenter[index] = squaredDist;
			distanceStatistics.AddValue(squaredDist);

			++index;
		}
	}

	distanceStatistics.Finalize();

	//-----------------------------------------------------------------------------------------------------------------
	// find highest rated positon with search range
	//-----------------------------------------------------------------------------------------------------------------
	float3 buildsite(ZeroVector);
	float highestRating(0.0f);
	index = 0;

	/*FILE* file(nullptr);
	const std::string filename = cfg->GetFileName(ai->GetAICallback(), "AAIDebug.txt", "", "", true);
	file = fopen(filename.c_str(), "w+");
	fprintf(file, "Base center: %i, %i\n", baseCenter.x, baseCenter.y);
	fprintf(file, "Defence Map: Defence value / distance value / terrain value\n");*/
	
	for(int yPos = yStart; yPos < yEnd; yPos += 4)
	{
		for(int xPos = xStart; xPos < xEnd; xPos += 4)
		{
			const MapPos mapPos(xPos, yPos);

			if(CanBuildAt(mapPos, footprint))
			{
				// criterion 1: how well is tile already covered by existing static defences
				const float defenceValue = 2.5f * AAIConstants::maxCombatPower / (1.0f + 0.35f * s_defenceMaps.GetValue(mapPos, targetType) );

				// criterion 2: distance to center of base (prefer static defences closer to base)
				const float distanceValue = 0.75f * AAIConstants::maxCombatPower * distanceStatistics.GetNormalizedDeviationFromMax(distancesToBaseCenter[index]);

				// criterion 3: terrain (prefer defences on high ground, avoid defences close to walls of canyons/valleys)
				const int cell = (xPos/4 + (xMapSize/4) * yPos/4);
				const float terrainValue = std::min(AAIConstants::maxCombatPower, terrainModifier * plateau_map[cell]);

				float rating = defenceValue + distanceValue + terrainValue + 0.2f * (float)(rand()%10);

				// determine minimum distance from buildpos to the edges of the map
				const int edge_distance = GetEdgeDistance(xPos, yPos);

				// prevent aai from building defences too close to the edges of the map
				if( edge_distance < range)
					rating *= (1.0f - (range - edge_distance) / range);

				if(rating > highestRating)
				{
					float3 possibleBuildsite;
					ConvertMapPosToUnitPos(mapPos, possibleBuildsite, footprint);
					ConvertPositionToFinalBuildsite(possibleBuildsite, footprint);

					if(ai->GetAICallback()->CanBuildAt(def, possibleBuildsite))
					{
						buildsite = possibleBuildsite;
						highestRating = rating;
					}
				}
			}

			++index;
		}
	}

	//fclose(file);

	return buildsite;
}

float3 AAIMap::CheckConstructionAt(const UnitFootprint& footprint, const springLegacyAI::UnitDef* unitDef, const MapPos& mapPos) const
{
	if(CanBuildAt(mapPos, footprint))
	{
		float3 possibleBuildsite;
		ConvertMapPosToUnitPos(mapPos, possibleBuildsite, footprint);
		ConvertPositionToFinalBuildsite(possibleBuildsite, footprint);

		if(ai->GetAICallback()->CanBuildAt(unitDef, possibleBuildsite))
		{
			const int x = possibleBuildsite.x/xSectorSize;
			const int y = possibleBuildsite.z/ySectorSize;

			if(IsValidSector(x,y))
				return possibleBuildsite;
		}
	}

	return ZeroVector;
}

bool AAIMap::CanBuildAt(const MapPos& mapPos, const UnitFootprint& footprint) const
{
	if( (mapPos.x+footprint.xSize > xMapSize) || (mapPos.y+footprint.ySize > yMapSize) )
		return false; // buildsite too close to edges of map
	else
	{
		for(int y = mapPos.y; y < mapPos.y+footprint.ySize; ++y)
		{
			for(int x = mapPos.x; x < mapPos.x+footprint.xSize; ++x)
			{
				// all squares must be valid
				if(s_buildmap[x+y*xMapSize].IsTileTypeSet(footprint.invalidTileTypes))
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
				if(s_buildmap[x+y*xMapSize].IsTileTypeSet(nonOccupiedTile))
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
				if(s_buildmap[x+y*xMapSize].IsTileTypeSet(nonOccupiedTile))
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
				if(s_buildmap[x+y*xMapSize].IsTileTypeSet(nonOccupiedTile))
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
				if(s_buildmap[x+y*xMapSize].IsTileTypeSet(nonOccupiedTile))
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
				if( (blockmap[tileIndex] == 0) && (s_buildmap[tileIndex].IsTileTypeSet(EBuildMapTileType::FREE)) )
					s_buildmap[tileIndex].BlockTile();	

				++blockmap[tileIndex];
			}
			else
			{
				if(blockmap[tileIndex] > 0)
				{
					--blockmap[tileIndex];

					// if cell is not blocked anymore, mark cell on buildmap as empty (only if it has been marked bloked
					//					- if it is not marked as blocked its occupied by another building or unpassable)
					if(blockmap[tileIndex] == 0 && s_buildmap[tileIndex].IsTileTypeSet(EBuildMapTileType::BLOCKED_SPACE))
						s_buildmap[tileIndex].FreeTile();	
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
		if(ai->s_buildTree.GetUnitCategory(unitDefId).IsStaticDefence())
			AddOrRemoveStaticDefence(position, unitDefId, true);

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
		return UnitFootprint(xSize, ySize, ai->s_buildTree.GetFootprint(unitDefId).invalidTileTypes);
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
			if(s_buildmap[x+y*xMapSize].IsTileTypeSet(EBuildMapTileType::CLIFF))
				++cliffs;
		}
	}

	return cliffs;
}

void AAIMap::AnalyseMap()
{
	const float *height_map = ai->GetAICallback()->GetHeightMap();

	const int xPlateauMapSize(xMapSize/4);
	const int yPlateauMapSize(yMapSize/4);

	//-----------------------------------------------------------------------------------------------------------------
	// determine tile type
	//-----------------------------------------------------------------------------------------------------------------
	int waterCells(0);
	for(int y = 0; y < yMapSize; ++y)
	{
		for(int x = 0; x < xMapSize; ++x)
		{
			s_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::FREE);

			// determine tile type (land or water)
			if(height_map[x + y * xMapSize] < 0.0f)
			{
				s_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::WATER);
				++waterCells;
			}
			else
				s_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::LAND);

			// determine slope to detect cliffs
			if( (x < xMapSize - 4) && (y < yMapSize - 4) )
			{
				const float xSlope = (height_map[y * xMapSize + x] - height_map[y * xMapSize + x + 4])/64.0f;

				// check x-direction
				if( (xSlope > cfg->CLIFF_SLOPE) || (-xSlope > cfg->CLIFF_SLOPE) )
					s_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::CLIFF);
				else	// check y-direction
				{
					const float ySlope = (height_map[y * xMapSize + x] - height_map[(y+4) * xMapSize + x])/64.0f;

					if(ySlope > cfg->CLIFF_SLOPE || -ySlope > cfg->CLIFF_SLOPE)
						s_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::CLIFF);
					else
						s_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::FLAT);
				}
			}
			else
				s_buildmap[x+y*xMapSize].SetTileType(EBuildMapTileType::FLAT);
		}
	}

	s_waterTilesRatio = static_cast<float>(waterCells) / static_cast<float>(xMapSize*yMapSize);

	//-----------------------------------------------------------------------------------------------------------------
	// calculate plateau map
	//-----------------------------------------------------------------------------------------------------------------
	constexpr int TERRAIN_DETECTION_RANGE(6);

	for(int y = TERRAIN_DETECTION_RANGE; y < yPlateauMapSize - TERRAIN_DETECTION_RANGE; ++y)
	{
		for(int x = TERRAIN_DETECTION_RANGE; x < xPlateauMapSize - TERRAIN_DETECTION_RANGE; ++x)
		{
			const float height = height_map[4 * (x + y * xMapSize)];

			for(int j = y - TERRAIN_DETECTION_RANGE; j < y + TERRAIN_DETECTION_RANGE; ++j)
			{
				for(int i = x - TERRAIN_DETECTION_RANGE; i < x + TERRAIN_DETECTION_RANGE; ++i)
				{
					const float diff = (height_map[4 * (i + j * xMapSize)] - height);

					if(diff > 0.0f)
					{
						//! @todo Investigate the reason for this check
						if(s_buildmap[4 * (i + j * xMapSize)].IsTileTypeNotSet(EBuildMapTileType::CLIFF) )
							plateau_map[i + j * xPlateauMapSize] += diff;
					}
					else
						plateau_map[i + j * xPlateauMapSize] += diff;
				}
			}
		}
	}

	for(int y = 0; y < yPlateauMapSize; ++y)
	{
		for(int x = 0; x < xPlateauMapSize; ++x)
		{
			if(plateau_map[x + y * xPlateauMapSize] >= 0.0f)
				plateau_map[x + y * xPlateauMapSize] = sqrt(plateau_map[x + y * xPlateauMapSize]);
			else
				plateau_map[x + y * xPlateauMapSize] = -1.0f * sqrt((-1.0f) * plateau_map[x + y * xPlateauMapSize]);
		}
	}
}

void AAIMap::DetermineMapType()
{
	if( (static_cast<float>(max_land_continent_size) < 0.5f * static_cast<float>(max_water_continent_size) ) || (s_waterTilesRatio > 0.8f) )
		s_mapType.SetMapType(EMapType::WATER);
	else if(s_waterTilesRatio > 0.25f)
		s_mapType.SetMapType(EMapType::LAND_WATER);
	else
		s_mapType.SetMapType(EMapType::LAND);
}

// algorithm more or less by krogothe - thx very much
void AAIMap::DetectMetalSpots()
{
	const UnitDefId largestExtractor = ai->Getbt()->GetLargestExtractor();
	if ( largestExtractor.IsValid() == false ) 
	{
		ai->Log("No metal extractor unit known!");
		return;
	}

	const springLegacyAI::UnitDef* def = &ai->Getbt()->GetUnitDef(largestExtractor.id);
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
			//pos.x = coordx * 2 * SQUARE_SIZE;
			//pos.z = coordy * 2 * SQUARE_SIZE;	
			ConvertMapPosToUnitPos(MapPos(2*coordx, 2*coordy), pos, largestExtractorFootprint);
			ConvertPositionToFinalBuildsite(pos, largestExtractorFootprint);

			pos.y = ai->GetAICallback()->GetElevation(pos.x, pos.z);

			temp.amount = TempMetal * ai->GetAICallback()->GetMaxMetal() * MaxMetal / 255.0f;
			temp.occupied = false;
			temp.pos = pos;

			//if(ai->Getcb()->CanBuildAt(def, pos))
			//{
				Pos2BuildMapPos(&pos, def);
				MapPos mapPos(pos.x, pos.z);

				//! @todo Check if this is correct or results in unnecessary shifts / rounding errors.
				if( (mapPos.x >= 2) && (mapPos.y >= 2) && (mapPos.x < xMapSize-2) && (mapPos.y < yMapSize-2) )
				{
					if(CanBuildAt(mapPos, largestExtractorFootprint))
					{
						metal_spots.push_back(temp);
						++SpotsFound;

						ChangeBuildMapOccupation(mapPos.x-2, mapPos.y-2, largestExtractorFootprint.xSize+2, largestExtractorFootprint.ySize+2, true);
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

void AAIMap::CheckUnitsInLOSUpdate(bool forceUpdate)
{
	const int minFrames    = forceUpdate ? 1 : AAIConstants::minFramesBetweenLOSUpdates;
	const int currentFrame = ai->GetAICallback()->GetCurrentFrame();

	if( (currentFrame - m_lastLOSUpdateInFrame) >= minFrames)
	{
		UpdateEnemyUnitsInLOS();
		UpdateFriendlyUnitsInLos();
		UpdateEnemyScoutingData();
		m_lastLOSUpdateInFrame = currentFrame;
	}
}

void AAIMap::UpdateEnemyUnitsInLOS()
{
	//
	// reset scouted buildings for all cells within current los
	//
	const int* losMap = ai->GetLosMap();

	const int frame = ai->GetAICallback()->GetCurrentFrame();

	int cellIndex(0);
	for(int y = 0; y < yLOSMapSize; ++y)
	{
		for(int x = 0; x < xLOSMapSize; ++x)
		{
			if(losMap[cellIndex] > 0)
			{
				m_scoutedEnemyUnitsMap.ResetTiles(x, y, frame);
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
			ScoutMapTile tile = m_scoutedEnemyUnitsMap.GetScoutMapTile(pos);

			// make sure unit is within the map (e.g. no aircraft that has flown outside of the map)
			if(tile.IsValid())
			{
				const UnitDefId defId(def->id);
				const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(defId);

				// add (finished) buildings/combat units to scout map
				if( category.IsBuilding() || category.IsCombatUnit() )
				{
					if(ai->GetAICallback()->UnitBeingBuilt(unitsInLOS[i]) == false)
						m_scoutedEnemyUnitsMap.AddEnemyUnit(defId, tile);
				}

				if(category.IsCombatUnit())
				{
					const AAITargetType& targetType = ai->s_buildTree.GetTargetType(defId);
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

		if( category.IsBuilding() || category.IsCombatUnit() )
		{
			AAISector* sector = GetSectorOfPos( ai->GetAICallback()->GetUnitPos(unitsInLOS[i]) );

			if(sector)
			{
				const bool unitBelongsToAlly( ai->GetAICallback()->GetUnitTeam(unitsInLOS[i]) != ai->GetMyTeamId() );
				sector->AddFriendlyUnitData(UnitDefId(def->id), unitBelongsToAlly);
			}
		}
	}
}

void AAIMap::UpdateEnemyScoutingData()
{
	std::fill(m_buildingsOnContinent.begin(), m_buildingsOnContinent.end(), 0);

	const int currentFrame = ai->GetAICallback()->GetCurrentFrame();
	
	// map of known enemy buildings has been updated -> update sector data
	for(int y = 0; y < ySectors; ++y)
	{
		for(int x = 0; x < xSectors; ++x)
		{
			m_sector[x][y].ResetScoutedEnemiesData();

			m_scoutedEnemyUnitsMap.UpdateSectorWithScoutedUnits(&m_sector[x][y], m_buildingsOnContinent, currentFrame);
		}
	}
}

bool AAIMap::IsPositionInLOS(const float3& position) const
{
	const int* losMap = ai->GetLosMap();

	const int xPos = (int)position.x / (losMapRes * SQUARE_SIZE);
	const int yPos = (int)position.z / (losMapRes * SQUARE_SIZE);

	// make sure unit is within the map
	if( (xPos >= 0) && (xPos < xLOSMapSize) && (yPos >= 0) && (yPos < yLOSMapSize) )
		return (losMap[xPos + yPos * xLOSMapSize] > 0);
	else
		return false;	
}

bool AAIMap::IsPositionWithinMap(const float3& position) const
{
	const int x = static_cast<int>( std::ceil(position.x) );
	const int y = static_cast<int>( std::ceil(position.z) );

	// check if unit is within the map
	return ( (x >= 0) && (x < xSize) && (y >= 0) && (y < ySize) );
}

bool AAIMap::IsConnectedToOcean(int xStart, int xEnd, int yStart, int yEnd) const
{
	// min number of tiles to be considered as "ocean"
	const int minSize = 3 * xSectorSizeMap*ySectorSizeMap;
	
	for(int y = yStart; y < yEnd; y += 2)
	{
		for(int x = xStart; x < xEnd; x += 2)
		{
			const int continentId = s_continentMap.GetContinentID(MapPos(x, y));

			if(s_continents[continentId].water && (s_continents[continentId].size > minSize) )
				return true;
		}
	}
	
	return false;
}

float3 AAIMap::DeterminePositionOfEnemyBuildingInSector(int xStart, int xEnd, int yStart, int yEnd) const
{
	const int xScoutMapStart = m_scoutedEnemyUnitsMap.BuildMapToScoutMapCoordinate(xStart);
	const int xScoutMapEnd   = m_scoutedEnemyUnitsMap.BuildMapToScoutMapCoordinate(xEnd);
	const int yScoutMapStart = m_scoutedEnemyUnitsMap.BuildMapToScoutMapCoordinate(yStart);
	const int yScoutMapEnd   = m_scoutedEnemyUnitsMap.BuildMapToScoutMapCoordinate(yEnd);

	for(int yCell = yScoutMapStart; yCell < yScoutMapEnd; ++yCell)
	{
		for(int xCell = xScoutMapStart; xCell < xScoutMapEnd; ++xCell)
		{	
			const UnitDefId unitDefId( m_scoutedEnemyUnitsMap.GetUnitAt(xCell, yCell) );

			if(unitDefId.IsValid())
			{
				if(ai->s_buildTree.GetUnitCategory(unitDefId).IsBuilding())
				{
					float3 selectedPosition;
					selectedPosition.x = static_cast<float>(m_scoutedEnemyUnitsMap.ScoutMapToBuildMapCoordinate(xCell) * SQUARE_SIZE);
					selectedPosition.z = static_cast<float>(m_scoutedEnemyUnitsMap.ScoutMapToBuildMapCoordinate(yCell) * SQUARE_SIZE);
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
	int scoutedEnemyBuildings(0);
	MapPos sectorLocationOfEnemyBuidlings(0, 0);

	for(int x = 0; x < xSectors; ++x)
	{
		for(int y = 0; y < ySectors; ++y)
		{
			m_sector[x][y].DecreaseLostUnits();

			const int enemyBuildings = m_sector[x][y].GetNumberOfEnemyBuildings();
			if(enemyBuildings > 0)
			{
				scoutedEnemyBuildings += enemyBuildings;

				sectorLocationOfEnemyBuidlings.x += enemyBuildings * x;
				sectorLocationOfEnemyBuidlings.y += enemyBuildings * y;
			}
		}
	}

	if(scoutedEnemyBuildings > 0)
	{
		m_centerOfEnemyBase.x =   static_cast<float>(xSectorSizeMap * sectorLocationOfEnemyBuidlings.x) / static_cast<float>(scoutedEnemyBuildings) 
								+ static_cast<float>(xSectorSizeMap/2);
		m_centerOfEnemyBase.y =   static_cast<float>(ySectorSizeMap * sectorLocationOfEnemyBuidlings.y) / static_cast<float>(scoutedEnemyBuildings) 
								+ static_cast<float>(ySectorSizeMap/2);
	}

	/*ai->Log("Enemies on continent: ");
	for(size_t continentId = 0; continentId < m_buildingsOnContinent.size(); ++continentId)
	{
		ai->Log("%i: %i   ", static_cast<int>(continentId), m_buildingsOnContinent[continentId]);
	}
	ai->Log("\n");*/
}

float AAIMap::GetDistanceToCenterOfEnemyBase(const float3& position) const
{
	const float dx = position.x - m_centerOfEnemyBase.x * SQUARE_SIZE;
	const float dy = position.z - m_centerOfEnemyBase.y * SQUARE_SIZE;

	return fastmath::apxsqrt(dx*dx + dy*dy);
}

void AAIMap::UpdateNeighbouringSectors(std::vector< std::list<AAISector*> >& sectorsInDistToBase)
{
	// delete old values
	for(int x = 0; x < xSectors; ++x)
	{
		for(int y = 0; y < ySectors; ++y)
		{
			if(m_sector[x][y].m_distanceToBase > 0)
				m_sector[x][y].m_distanceToBase = -1;
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
			if( (x > 0) && (m_sector[x-1][y].m_distanceToBase == -1) )
			{
				m_sector[x-1][y].m_distanceToBase = i;
				sectorsInDistToBase[i].push_back(&m_sector[x-1][y]);
			}
			// check right neighbour
			if( (x < (xSectors - 1)) && (m_sector[x+1][y].m_distanceToBase == -1) )
			{
				m_sector[x+1][y].m_distanceToBase = i;
				sectorsInDistToBase[i].push_back(&m_sector[x+1][y]);
			}
			// check upper neighbour
			if( (y > 0) && (m_sector[x][y-1].m_distanceToBase == -1) )
			{
				m_sector[x][y-1].m_distanceToBase = i;
				sectorsInDistToBase[i].push_back(&m_sector[x][y-1]);
			}
			// check lower neighbour
			if( (y < (ySectors - 1)) && (m_sector[x][y+1].m_distanceToBase == -1) )
			{
				m_sector[x][y+1].m_distanceToBase = i;
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
	const bool waterSectorSelectable = moveTypeOfUnits.IsAir() || moveTypeOfUnits.IsHover() || moveTypeOfUnits.IsMobileSea();

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
			const float rating = m_sector[x][y].GetAttackRating(globalCombatPower, continentCombatPower, assaultGroupsOfType, maxLostUnits);

			if(rating > highestRating)
			{
				selectedSector = &m_sector[x][y];
				highestRating  = rating;
			}
		}
	}

	return selectedSector;
}

const char* AAIMap::GetMapTypeString(const AAIMapType& mapType) const
{
	if(mapType.IsLand())
		return "LAND_MAP";
	else if(mapType.IsLandWater())
		return "LAND_WATER_MAP";
	else if(mapType.IsWater())
		return "WATER_MAP";
	else
		return "UNKNOWN_MAP";
}

void AAIMap::DetermineSpottedEnemyBuildingsOnContinentType(int& enemyBuildingsOnLand, int& enemyBuildingsOnSea) const
{
	enemyBuildingsOnLand = 0;
	enemyBuildingsOnSea  = 0;

	for(int continentId = 0; continentId < s_continents.size(); ++continentId)
	{
		if(s_continents[continentId].water)
			enemyBuildingsOnSea += m_buildingsOnContinent[continentId];
		else
			enemyBuildingsOnLand += m_buildingsOnContinent[continentId];
	}
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

void AAIMap::AddOrRemoveStaticDefence(const float3& position, UnitDefId defence, bool addDefence)
{
	// (un-)block area close to static defence
	const TargetTypeValues blockValues(100.0f);
	s_defenceMaps.ModifyTiles(position, 120.0f, ai->s_buildTree.GetFootprint(defence), blockValues, addDefence);

	s_defenceMaps.ModifyTiles(position, ai->s_buildTree.GetMaxRange(defence), ai->s_buildTree.GetFootprint(defence), ai->s_buildTree.GetCombatPower(defence), addDefence);

	/*if(ai->GetAAIInstance() == 1)
	{
		const std::string filename = cfg->GetFileName(ai->GetAICallback(), "AAIDebug.txt", "", "", true);
		FILE* file = fopen(filename.c_str(), "w+");

		for(int y = 0; y < yMapSize; ++y)
		{
			for(int x = 0; x < xMapSize; ++x)
			{
				const float value = s_defenceMaps.GetValue(MapPos(x,y), ETargetType::SURFACE);

				fprintf(file, "%3.1f ", value);
			}

			fprintf(file, "\n");
		}

		fclose(file);
	}*/
}

uint32_t AAIMap::GetSuitableMovementTypes(const AAIMapType& mapType) const
{
	// always: MOVEMENT_TYPE_AIR, MOVEMENT_TYPE_AMPHIB, MOVEMENT_TYPE_HOVER;
	uint32_t suitableMovementTypes =  static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_AIR)
									+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_AMPHIBIOUS)
									+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_HOVER);
	
	// MOVEMENT_TYPE_GROUND allowed on non water maps (i.e. map contains land)
	if(mapType.IsWater() == false)
		suitableMovementTypes |= static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_GROUND);

	// MOVEMENT_TYPE_SEA_FLOATER/SUBMERGED allowed on non land maps (i.e. map contains water)
	if(mapType.IsLand() == false)
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

float AAIMap::GetEdgeDistance(const float3& pos) const
{
	// determine minimum dist to horizontal map edges
	const float distRight(static_cast<float>(xSize) - pos.x);
	const float hDist( std::min(pos.x, distRight) );

	// determine minimum dist to vertical map edges
	const float distBottom(static_cast<float>(ySize) - pos.z);
	const float vDist( std::min(pos.z, distBottom) );

	return std::min(hDist, vDist);
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
