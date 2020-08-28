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
using namespace springLegacyAI;

#define MAP_CACHE_PATH "cache/"

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

list<AAIMetalSpot>  AAIMap::metal_spots;

int AAIMap::land_metal_spots;
int AAIMap::water_metal_spots;

float AAIMap::land_ratio;
float AAIMap::flat_land_ratio;
float AAIMap::water_ratio;

bool AAIMap::metalMap;
AAIMapType AAIMap::s_mapType;

vector< vector<int> > AAIMap::team_sector_map;
vector<int> AAIMap::buildmap;
vector<int> AAIMap::blockmap;
vector<float> AAIMap::plateau_map;
vector<int> AAIMap::continent_map;

vector<AAIContinent> AAIMap::continents;
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
				sector[x][y].SaveDataToFile(save_file);
			
			fprintf(save_file, "\n");
		}

		fclose(save_file);

		buildmap.clear();
		blockmap.clear();
		plateau_map.clear();
		continent_map.clear();
	}

	defence_map.clear();
	air_defence_map.clear();
	submarine_defence_map.clear();

	m_scoutedEnemyUnitsMap.clear();
	m_lastLOSUpdateInFrameMap.clear();

	units_in_los.clear();
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

		losMapRes = std::sqrt(ai->GetAICallback()->GetLosMapResolution());
		xLOSMapSize = xMapSize / losMapRes;
		yLOSMapSize = yMapSize / losMapRes;

		xDefMapSize = xMapSize / 4;
		yDefMapSize = yMapSize / 4;

		xContMapSize = xMapSize / 4;
		yContMapSize = yMapSize / 4;

		// calculate number of sectors
		xSectors = floor(0.5f + ((float) xMapSize)/cfg->SECTOR_SIZE);
		ySectors = floor(0.5f + ((float) yMapSize)/cfg->SECTOR_SIZE);

		// calculate effective sector size
		xSectorSizeMap = floor( ((float) xMapSize) / ((float) xSectors) );
		ySectorSizeMap = floor( ((float) yMapSize) / ((float) ySectors) );

		xSectorSize = 8 * xSectorSizeMap;
		ySectorSize = 8 * ySectorSizeMap;

		buildmap.resize(xMapSize*yMapSize, 0);
		blockmap.resize(xMapSize*yMapSize, 0);
		continent_map.resize(xContMapSize*yContMapSize, -1);
		plateau_map.resize(xContMapSize*yContMapSize, 0.0f);

		// create map that stores which aai player has occupied which sector (visible to all aai players)
		team_sector_map.resize(xSectors);

		for(int x = 0; x < xSectors; ++x)
			team_sector_map[x].resize(ySectors, -1);

		ReadContinentFile();

		ReadMapCacheFile();
	}

	ai->Log("Map size: %i x %i    LOS map size: %i x %i  (los res: %i)\n", xMapSize, yMapSize, xLOSMapSize, yLOSMapSize, losMapRes);

	// create field of sectors
	sector.resize(xSectors);

	for(int x = 0; x < xSectors; ++x)
		sector[x].resize(ySectors);

	for(int j = 0; j < ySectors; ++j)
	{
		for(int i = 0; i < xSectors; ++i)
			// provide ai callback to sectors & set coordinates of the sectors
			sector[i][j].Init(ai, i, j, xSectorSize*i, xSectorSize*(i+1), ySectorSize * j, ySectorSize * (j+1));
	}

	// add metalspots to their sectors
	for(std::list<AAIMetalSpot>::iterator spot = metal_spots.begin(); spot != metal_spots.end(); ++spot)
	{
		const int x = spot->pos.x/xSectorSize;
		const int y = spot->pos.z/ySectorSize;

		if(IsValidSector(x,y))
			sector[x][y].AddMetalSpot(&(*spot));
	}

	readMapLearnFile();

	// for scouting
	m_scoutedEnemyUnitsMap.resize(xLOSMapSize*yLOSMapSize, 0u);
	m_lastLOSUpdateInFrameMap.resize(xLOSMapSize*yLOSMapSize, 0);

	units_in_los.resize(cfg->MAX_UNITS, 0);

	// create defence
	defence_map.resize(xDefMapSize*yDefMapSize, 0);
	air_defence_map.resize(xDefMapSize*yDefMapSize, 0);
	submarine_defence_map.resize(xDefMapSize*yDefMapSize, 0);

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
					const int cell = x + y * xMapSize;
					fscanf(file, "%i ", &buildmap[cell]);
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
				fprintf(file, "%i ", buildmap[cell]);
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

		for(list<AAIMetalSpot>::iterator spot = metal_spots.begin(); spot != metal_spots.end(); ++spot)
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

void AAIMap::readMapLearnFile()
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

			sector[i][j].LoadDataFromFile(load_file);

			//---------------------------------------------------------------------------------------------------------
			// determine movement types that are suitable to maneuvre
			//---------------------------------------------------------------------------------------------------------
			AAIMapType mapType(EMapType::LAND_MAP);

			if(sector[i][j].water_ratio > 0.7f)
				mapType.SetMapType(EMapType::WATER_MAP);
			else if(sector[i][j].water_ratio > 0.3f)
				mapType.SetMapType(EMapType::LAND_WATER_MAP);

			sector[i][j].m_suitableMovementTypes = GetSuitableMovementTypes(mapType);
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
			flat_land_ratio += sector[i][j].flat_ratio;
			water_ratio += sector[i][j].water_ratio;
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
			sector[x][y].UpdateLearnedData();
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

bool AAIMap::SetBuildMap(int xPos, int yPos, int xSize, int ySize, int value, int ignore_value)
{
	if(xPos+xSize <= xMapSize && yPos+ySize <= yMapSize)
	{
		for(int x = xPos; x < xSize+xPos; x++)
		{
			for(int y = yPos; y < ySize+yPos; y++)
			{
				if(buildmap[x+y*xMapSize] != ignore_value)
				{
					buildmap[x+y*xMapSize] = value;

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
		return true;
	}
	return false;
}

float3 AAIMap::GetBuildSiteInRect(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, bool water) const
{
	float3 pos;

	// get required cell-size of the building
	int xSize, ySize, xPos, yPos;
	GetSize(def, &xSize, &ySize);

	// check rect
	for(yPos = yStart; yPos < yEnd; yPos += 2)
	{
		for(xPos = xStart; xPos < xEnd; xPos += 2)
		{
			// check if buildmap allows construction
			if(CanBuildAt(xPos, yPos, xSize, ySize, water))
			{
				if(ai->s_buildTree.GetUnitType(UnitDefId(def->id)).IsFactory())
					yPos += 8;

				pos.x = xPos;
				pos.z = yPos;

				// buildmap allows construction, now check if otherwise blocked
				BuildMapPos2Pos(&pos, def);
				Pos2FinalBuildPos(&pos, def);

				if(ai->GetAICallback()->CanBuildAt(def, pos))
				{
					int x = pos.x/xSectorSize;
					int y = pos.z/ySectorSize;

					if(x < xSectors && x  >= 0 && y < ySectors && y >= 0)
						return pos;
				}
			}
		}
	}

	return ZeroVector;
}

float3 AAIMap::GetRadarArtyBuildsite(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, float range, bool water)
{
	float3 pos;
	float3 best_pos = ZeroVector;

	float my_rating;
	float best_rating = -10000.0f;

	// convert range from unit coordinates to build map coordinates
	range /= 8.0f;

	// get required cell-size of the building
	int xSize, ySize, xPos, yPos;
	GetSize(def, &xSize, &ySize);

	// go through rect
	for(yPos = yStart; yPos < yEnd; yPos += 2)
	{
		for(xPos = xStart; xPos < xEnd; xPos += 2)
		{
			if(CanBuildAt(xPos, yPos, xSize, ySize, water))
			{
				const float edgeDist = static_cast<float>(GetEdgeDistance(xPos, yPos)) / range;

				my_rating = 0.04f * (float)(rand()%50) + edgeDist;

				if(!water)
				{
					const int plateauMapCellIndex = xPos/4 + yPos/4 * xContMapSize;
					my_rating += plateau_map[plateauMapCellIndex];
				}
					
				if(my_rating > best_rating)
				{
					pos.x = xPos;
					pos.z = yPos;

					// buildmap allows construction, now check if otherwise blocked
					BuildMapPos2Pos(&pos, def);
					Pos2FinalBuildPos(&pos, def);

					if(ai->GetAICallback()->CanBuildAt(def, pos))
					{
						best_pos = pos;
						best_rating = my_rating;
					}
				}
			}
		}
	}

	return best_pos;
}

float3 AAIMap::GetCenterBuildsite(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, bool water)
{
	float3 pos, temp_pos;
	bool vStop = false, hStop = false;
	int vCenter = yStart + (yEnd-yStart)/2;
	int hCenter = xStart + (xEnd-xStart)/2;
	int hIterator = 1, vIterator = 1;

	// get required cell-size of the building
	int xSize, ySize;
	GetSize(def, &xSize, &ySize);

	// check rect
	while(!vStop || !hStop)
	{

		pos.z = vCenter - vIterator;
		pos.x = hCenter - hIterator;

		if(!vStop)
		{
			while(pos.x < hCenter+hIterator)
			{
				// check if buildmap allows construction
				if(CanBuildAt(pos.x, pos.z, xSize, ySize, water))
				{
					temp_pos.x = pos.x;
					temp_pos.y = 0;
					temp_pos.z = pos.z;

					if(ai->s_buildTree.GetUnitType(UnitDefId(def->id)).IsFactory())
						temp_pos.z += 8;

					// buildmap allows construction, now check if otherwise blocked
					BuildMapPos2Pos(&temp_pos, def);
					Pos2FinalBuildPos(&temp_pos, def);

					if(ai->GetAICallback()->CanBuildAt(def, temp_pos))
					{
						int	x = temp_pos.x/xSectorSize;
						int	y = temp_pos.z/ySectorSize;

						if(x < xSectors && x  >= 0 && y < ySectors && y >= 0)
							return temp_pos;
					}

				}
				else if(CanBuildAt(pos.x, pos.z + 2 * vIterator, xSize, ySize, water))
				{
					temp_pos.x = pos.x;
					temp_pos.y = 0;
					temp_pos.z = pos.z + 2 * vIterator;

					if(ai->s_buildTree.GetUnitType(UnitDefId(def->id)).IsFactory())
						temp_pos.z += 8;

					// buildmap allows construction, now check if otherwise blocked
					BuildMapPos2Pos(&temp_pos, def);
					Pos2FinalBuildPos(&temp_pos, def);

					if(ai->GetAICallback()->CanBuildAt(def, temp_pos))
					{
						int x = temp_pos.x/xSectorSize;
						int y = temp_pos.z/ySectorSize;

						if(x < xSectors && x  >= 0 && y < ySectors && y >= 0)
							return temp_pos;
					}
				}

				pos.x += 2;
			}
		}

		if (!hStop)
		{
			hIterator += 2;

			if (hCenter - hIterator < xStart || hCenter + hIterator > xEnd)
			{
				hStop = true;
				hIterator -= 2;
			}
		}

		if(!hStop)
		{
			while(pos.z < vCenter+vIterator)
			{
				// check if buildmap allows construction
				if(CanBuildAt(pos.x, pos.z, xSize, ySize, water))
				{
					temp_pos.x = pos.x;
					temp_pos.y = 0;
					temp_pos.z = pos.z;

					if(ai->s_buildTree.GetUnitType(UnitDefId(def->id)).IsFactory())
						temp_pos.z += 8;

					// buildmap allows construction, now check if otherwise blocked
					BuildMapPos2Pos(&temp_pos, def);
					Pos2FinalBuildPos(&temp_pos, def);

					if(ai->GetAICallback()->CanBuildAt(def, temp_pos))
					{
						int x = temp_pos.x/xSectorSize;
						int y = temp_pos.z/ySectorSize;

						if(x < xSectors || x  >= 0 || y < ySectors || y >= 0)
							return temp_pos;
					}
				}
				else if(CanBuildAt(pos.x + 2 * hIterator, pos.z, xSize, ySize, water))
				{
					temp_pos.x = pos.x + 2 * hIterator;
					temp_pos.y = 0;
					temp_pos.z = pos.z;

					if(ai->s_buildTree.GetUnitType(UnitDefId(def->id)).IsFactory())
						temp_pos.z += 8;

					// buildmap allows construction, now check if otherwise blocked
					BuildMapPos2Pos(&temp_pos, def);
					Pos2FinalBuildPos(&temp_pos, def);

					if(ai->GetAICallback()->CanBuildAt(def, temp_pos))
					{
						int x = temp_pos.x/xSectorSize;
						int y = temp_pos.z/ySectorSize;

						if(x < xSectors && x  >= 0 && y < ySectors && y >= 0)
							return temp_pos;
					}
				}

				pos.z += 2;
			}
		}

		vIterator += 2;

		if(vCenter - vIterator < yStart || vCenter + vIterator > yEnd)
			vStop = true;
	}

	return ZeroVector;
}

float3 AAIMap::GetRandomBuildsite(const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, int tries, bool water)
{
	float3 pos;

	// get required cell-size of the building
	int xSize, ySize;
	GetSize(def, &xSize, &ySize);

	for(int i = 0; i < tries; i++)
	{

		// get random pos within rectangle
		if(xEnd - xStart - xSize < 1)
			pos.x = xStart;
		else
			pos.x = xStart + rand()%(xEnd - xStart - xSize);

		if(yEnd - yStart - ySize < 1)
			pos.z = yStart;
		else
			pos.z = yStart + rand()%(yEnd - yStart - ySize);

		// check if buildmap allows construction
		if(CanBuildAt(pos.x, pos.z, xSize, ySize, water))
		{
			if(ai->s_buildTree.GetUnitType(UnitDefId(def->id)).IsFactory())
				pos.z += 8;

			// buildmap allows construction, now check if otherwise blocked
			BuildMapPos2Pos(&pos, def);
			Pos2FinalBuildPos(&pos, def);

			if(ai->GetAICallback()->CanBuildAt(def, pos))
			{
				int x = pos.x/xSectorSize;
				int y = pos.z/ySectorSize;

				if(x < xSectors && x  >= 0 && y < ySectors && y >= 0)
					return pos;
			}
		}
	}

	return ZeroVector;
}

bool AAIMap::CanBuildAt(int xPos, int yPos, int xSize, int ySize, bool water) const
{
	if( (xPos+xSize <= xMapSize) && (yPos+ySize <= yMapSize) )
	{
		// check if all squares the building needs are empty
		for(int y = yPos; y < ySize+yPos; ++y)
		{
			for(int x = xPos; x < xSize+xPos; ++x)
			{
				// check if cell already blocked by something
				if(!water && buildmap[x+y*xMapSize] != 0)
					return false;
				else if(water && buildmap[x+y*xMapSize] != 4)
					return false;
			}
		}
		return true;
	}
	else
		return false;
}

void AAIMap::CheckRows(int xPos, int yPos, int xSize, int ySize, bool add, bool water)
{
	bool insert_space;
	int cell;
	int building;

	if(water)
		building = 5;
	else
		building = 1;

	// check horizontal space
	if(xPos+xSize+cfg->MAX_XROW <= xMapSize && xPos - cfg->MAX_XROW >= 0)
	{
		for(int y = yPos; y < yPos + ySize; ++y)
		{
			if(y >= yMapSize)
			{
				ai->Log("ERROR: y = %i index out of range when checking horizontal rows", y);
				return;
			}

			// check to the right
			insert_space = true;
			for(int x = xPos+xSize; x < xPos+xSize+cfg->MAX_XROW; x++)
			{
				if(buildmap[x+y*xMapSize] != building)
				{
					insert_space = false;
					break;
				}
			}

			// check to the left
			if(!insert_space)
			{
				insert_space = true;

				for(int x = xPos-1; x >= xPos - cfg->MAX_XROW; x--)
				{
					if(buildmap[x+y*xMapSize] != building)
					{
						insert_space = false;
						break;
					}
				}
			}

			if(insert_space)
			{
				// right side
				cell = GetNextX(1, xPos+xSize, y, building);

				if(cell != -1 && xPos+xSize+cfg->X_SPACE <= xMapSize)
				{
					BlockCells(cell, y, cfg->X_SPACE, 1, add, water);

					//add blocking of the edges
					if(y == yPos && (yPos - cfg->Y_SPACE) >= 0)
						BlockCells(cell, yPos - cfg->Y_SPACE, cfg->X_SPACE, cfg->Y_SPACE, add, water);
					if(y == yPos + ySize - 1)
						BlockCells(cell, yPos + ySize, cfg->X_SPACE, cfg->Y_SPACE, add, water);
				}

				// left side
				cell = GetNextX(0, xPos-1, y, building);

				if(cell != -1 && cell-cfg->X_SPACE >= 0)
				{
					BlockCells(cell-cfg->X_SPACE, y, cfg->X_SPACE, 1, add, water);

					// add diagonal blocks
					if(y == yPos && (yPos - cfg->Y_SPACE) >= 0)
							BlockCells(cell-cfg->X_SPACE, yPos - cfg->Y_SPACE, cfg->X_SPACE, cfg->Y_SPACE, add, water);
					if(y == yPos + ySize - 1)
							BlockCells(cell-cfg->X_SPACE, yPos + ySize, cfg->X_SPACE, cfg->Y_SPACE, add, water);

				}
			}
		}
	}

	// check vertical space
	if(yPos+ySize+cfg->MAX_YROW <= yMapSize && yPos - cfg->MAX_YROW >= 0)
	{
		for(int x = xPos; x < xPos + xSize; x++)
		{
			if(x >= xMapSize)
			{
				ai->Log("ERROR: x = %i index out of range when checking vertical rows", x);
				return;
			}

			// check downwards
			insert_space = true;
			for(int y = yPos+ySize; y < yPos+ySize+cfg->MAX_YROW; ++y)
			{
				if(buildmap[x+y*xMapSize] != building)
				{
					insert_space = false;
					break;
				}
			}

			// check upwards
			if(!insert_space)
			{
				insert_space = true;

				for(int y = yPos-1; y >= yPos - cfg->MAX_YROW; --y)
				{
					if(buildmap[x+y*xMapSize] != building)
					{
						insert_space = false;
						break;
					}
				}
			}

			if(insert_space)
			{
				// downwards
				cell = GetNextY(1, x, yPos+ySize, building);

				if(cell != -1 && yPos+ySize+cfg->Y_SPACE <= yMapSize)
				{
					BlockCells(x, cell, 1, cfg->Y_SPACE, add, water);

					// add diagonal blocks
					if(x == xPos && (xPos - cfg->X_SPACE) >= 0)
						BlockCells(xPos-cfg->X_SPACE, cell, cfg->X_SPACE, cfg->Y_SPACE, add, water);
					if(x == xPos + xSize - 1)
						BlockCells(xPos + xSize, cell, cfg->X_SPACE, cfg->Y_SPACE, add, water);
				}

				// upwards
				cell = GetNextY(0, x, yPos-1, building);

				if(cell != -1 && cell-cfg->Y_SPACE >= 0)
				{
					BlockCells(x, cell-cfg->Y_SPACE, 1, cfg->Y_SPACE, add, water);

					// add diagonal blocks
					if(x == xPos && (xPos - cfg->X_SPACE) >= 0)
						BlockCells(xPos-cfg->X_SPACE, cell-cfg->Y_SPACE, cfg->X_SPACE, cfg->Y_SPACE, add, water);
					if(x == xPos + xSize - 1)
						BlockCells(xPos + xSize, cell-cfg->Y_SPACE, cfg->X_SPACE, cfg->Y_SPACE, add, water);
				}
			}
		}
	}
}

void AAIMap::BlockCells(int xPos, int yPos, int width, int height, bool block, bool water)
{
	// make sure to stay within map if too close to the edges
	int xEnd = xPos + width;
	int yEnd = yPos + height;

	if(xEnd > xMapSize)
		xEnd = xMapSize;

	if(yEnd > yMapSize)
		yEnd = yMapSize;

	//float3 my_pos;
	const int emptyCellValue = water ? 4 : 0;

	if(block)	// block cells
	{
		for(int y = yPos; y < yEnd; ++y)
		{
			for(int x = xPos; x < xEnd; ++x)
			{
				const int cell = x + xMapSize*y;

				// if no building ordered that cell to be blocked, update buildmap
				// (only if space is not already occupied by a building)
				if( (blockmap[cell] == 0) && (buildmap[cell] == emptyCellValue) )
				{
					buildmap[cell] = 2;

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

				++blockmap[cell];
			}
		}
	}
	else		// unblock cells
	{
		for(int y = yPos; y < yEnd; ++y)
		{
			for(int x = xPos; x < xEnd; ++x)
			{
				const int cell = x + xMapSize*y;

				if(blockmap[cell] > 0)
				{
					--blockmap[cell];

					// if cell is not blocked anymore, mark cell on buildmap as empty (only if it has been marked bloked
					//					- if it is not marked as blocked its occupied by another building or unpassable)
					if(blockmap[cell] == 0 && buildmap[cell] == 2)
					{
						buildmap[cell] = emptyCellValue;

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
		}
	}
}

void AAIMap::UpdateBuildMap(const float3& buildPos, const UnitDef *def, bool block)
{
	const bool water   = ai->s_buildTree.GetMovementType(UnitDefId(def->id)).IsStaticSea();
	const bool factory = ai->s_buildTree.GetUnitType(UnitDefId(def->id)).IsFactory();
	
	float3 buildMapPos = buildPos;
	Pos2BuildMapPos(&buildMapPos, def);

	if(block)
	{
		const int blockValue = water ? 5 : 1;
		SetBuildMap(buildMapPos.x, buildMapPos.z, def->xsize, def->zsize, blockValue);
	}
	else
	{
		// remove spaces before freeing up buildspace
		CheckRows(buildMapPos.x, buildMapPos.z, def->xsize, def->zsize, block, water);

		const int unblockValue = water ? 5 : 1;
		SetBuildMap(buildMapPos.x, buildMapPos.z, def->xsize, def->zsize, unblockValue);
	}

	if(factory)
	{
		// extra space for factories to keep exits clear
		BlockCells(buildMapPos.x,              buildMapPos.z - 8,          def->xsize, 8, block, water);
		BlockCells(buildMapPos.x + def->xsize, buildMapPos.z - 8,          cfg->X_SPACE, def->zsize + 1.5f * (float)cfg->Y_SPACE, block, water);
		BlockCells(buildMapPos.x,              buildMapPos.z + def->zsize, def->xsize, 1.5f * (float)cfg->Y_SPACE - 8, block, water);
	}

	// add spaces after blocking buildspace
	if(block)
		CheckRows(buildMapPos.x, buildMapPos.z, def->xsize, def->zsize, block, water);
}



int AAIMap::GetNextX(int direction, int xPos, int yPos, int value)
{
	int x = xPos;

	if(direction)
	{
		while(buildmap[x+yPos*xMapSize] == value)
		{
			++x;

			// search went out of map
			if(x >= xMapSize)
				return -1;
		}
	}
	else
	{
		while(buildmap[x+yPos*xMapSize] == value)
		{
			--x;

			// search went out of map
			if(x < 0)
				return -1;
		}
	}

	return x;
}

int AAIMap::GetNextY(int direction, int xPos, int yPos, int value)
{
	int y = yPos;

	if(direction)
	{
		// scan line until next free cell found
		while(buildmap[xPos+y*xMapSize] == value)
		{
			++y;

			// search went out of map
			if(y >= yMapSize)
				return -1;
		}
	}
	else
	{
		// scan line until next free cell found
		while(buildmap[xPos+y*xMapSize] == value)
		{
			--y;

			// search went out of map
			if(y < 0)
				return -1;
		}
	}

	return y;
}

void AAIMap::GetSize(const UnitDef *def, int *xSize, int *ySize) const
{
	// calculate size of building
	*xSize = def->xsize;
	*ySize = def->zsize;

	// if building is a factory additional vertical space is needed
	if(ai->s_buildTree.GetUnitType(UnitDefId(def->id)).IsFactory())
	{
		*xSize += cfg->X_SPACE;
		*ySize += ((float)cfg->Y_SPACE)*1.5;
	}
}

int AAIMap::GetCliffyCells(int xPos, int yPos, int xSize, int ySize)
{
	int cliffs = 0;

	// count cells with big slope
	for(int x = xPos; x < xPos + xSize; ++x)
	{
		for(int y = yPos; y < yPos + ySize; ++y)
		{
			if(buildmap[x+y*xMapSize] == 3)
				++cliffs;
		}
	}

	return cliffs;
}

void AAIMap::AnalyseMap()
{
	float slope;

	const float *height_map = ai->GetAICallback()->GetHeightMap();

	// get water/cliffs
	for(int x = 0; x < xMapSize; ++x)
	{
		for(int y = 0; y < yMapSize; ++y)
		{
			// check for water
			if(height_map[y * xMapSize + x] < 0.0f)
				buildmap[x+y*xMapSize] = 4;
			else if(x < xMapSize - 4 && y < yMapSize - 4)
			// check slope
			{
				slope = (height_map[y * xMapSize + x] - height_map[y * xMapSize + x + 4])/64.0;

				// check x-direction
				if(slope > cfg->CLIFF_SLOPE || -slope > cfg->CLIFF_SLOPE)
					buildmap[x+y*xMapSize] = 3;
				else	// check y-direction
				{
					slope = (height_map[y * xMapSize + x] - height_map[(y+4) * xMapSize + x])/64.0;

					if(slope > cfg->CLIFF_SLOPE || -slope > cfg->CLIFF_SLOPE)
						buildmap[x+y*xMapSize] = 3;
				}
			}
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
						 if(buildmap[4 * (i + j * xMapSize)] != 3)
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
	int waterCells = 0;

	for(int y = 0; y < yMapSize; ++y)
	{
		for(int x = 0; x < xMapSize; ++x)
		{
			if(buildmap[x + y*xMapSize] == 4)
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

	// clear variables, just in case!
	TotalMetal = 0;
	MaxMetal = 0;
	SpotsFound = 0;

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
					if(CanBuildAt(pos.x, pos.z, def->xsize, def->zsize, water))
					{
						metal_spots.push_back(temp);
						++SpotsFound;

						SetBuildMap(pos.x-2, pos.z-2, def->xsize+2, def->zsize+2, water ? 5 : 1);
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

	for(int y = 0; y < yLOSMapSize; ++y)
	{
		for(int x = 0; x < xLOSMapSize; ++x)
		{
			const int cellIndex = x + y * xLOSMapSize;

			if(losMap[cellIndex] > 0u)
			{
				m_scoutedEnemyUnitsMap[cellIndex]    = 0u;
				m_lastLOSUpdateInFrameMap[cellIndex] = ai->GetAICallback()->GetCurrentFrame();;
			}
		}
	}

	for(int y = 0; y < ySectors; ++y)
	{
		for(int x = 0; x < xSectors; ++x)
			sector[x][y].m_enemyUnitsDetectedBySensor = 0;
	}

	// update enemy units
	AAIValuesForMobileTargetTypes spottedEnemyCombatUnitsByTargetType;
	const int numberOfEnemyUnits = ai->GetAICallback()->GetEnemyUnitsInRadarAndLos(&(units_in_los.front()));

	for(int i = 0; i < numberOfEnemyUnits; ++i)
	{
		const float3   pos = ai->GetAICallback()->GetUnitPos(units_in_los[i]);
		const UnitDef* def = ai->GetAICallback()->GetUnitDef(units_in_los[i]);

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
			sector[x][y].ResetLocalCombatPower();
	}

	const int numberOfFriendlyUnits = ai->GetAICallback()->GetFriendlyUnits(&(units_in_los.front()));

	for(int i = 0; i < numberOfFriendlyUnits; ++i)
	{
		// get unit def & category
		const UnitDef* def = ai->GetAICallback()->GetUnitDef(units_in_los[i]);
		const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(UnitDefId(def->id));

		if( category.isBuilding() || category.isCombatUnit() )
		{
			AAISector* sector = GetSectorOfPos( ai->GetAICallback()->GetUnitPos(units_in_los[i]) );

			if(sector)
			{
				const bool unitBelongsToAlly( ai->GetAICallback()->GetUnitTeam(units_in_los[i]) != m_myTeamId );
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
			sector[x][y].ResetScoutedEnemiesData();

			for(int yCell = y * ySectorSizeMap/losMapRes; yCell < (y + 1) * ySectorSizeMap/losMapRes; ++yCell)
			{
				for(int xCell = x * xSectorSizeMap/losMapRes; xCell < (x + 1) * xSectorSizeMap/losMapRes; ++xCell)
				{
					const int cellIndex = xCell + yCell * xLOSMapSize;
					const UnitDefId unitDefId( static_cast<int>(m_scoutedEnemyUnitsMap[cellIndex]) );

					if(unitDefId.isValid())
						sector[x][y].AddScoutedEnemyUnit(unitDefId, m_lastLOSUpdateInFrameMap[cellIndex]);
				}
			}
		}
	}
}

void AAIMap::UpdateSectors()
{
	for(int x = 0; x < xSectors; ++x)
	{
		for(int y = 0; y < ySectors; ++y)
			sector[x][y].DecreaseLostUnits();
	}
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
		return &(sector[x][y]);
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

float AAIMap::GetDefenceBuildsite(float3 *buildPos, const UnitDef *def, int xStart, int xEnd, int yStart, int yEnd, const AAITargetType& targetType, float terrainModifier, bool water) const
{
	*buildPos = ZeroVector;
	float my_rating, best_rating = -10000.0f;

	// get required cell-size of the building
	int xSize, ySize, xPos, yPos, cell;
	GetSize(def, &xSize, &ySize);

	const std::vector<float> *map = &defence_map;

	if(targetType.IsAir() )
		map = &air_defence_map;
	else if(targetType.IsSubmerged() )
		map = &submarine_defence_map;

	float range =  ai->s_buildTree.GetMaxRange(UnitDefId(def->id)) / 8.0f;

	const std::string filename = cfg->GetFileName(ai->GetAICallback(), "AAIDebug.txt", "", "", true);
	FILE* file = fopen(filename.c_str(), "w+");
	fprintf(file, "Search area: (%i, %i) x (%i, %i)\n", xStart, yStart, xEnd, yEnd);
	fprintf(file, "Range: %g\n", range);

	// check rect
	for(yPos = yStart; yPos < yEnd; yPos += 4)
	{
		for(xPos = xStart; xPos < xEnd; xPos += 4)
		{
			// check if buildmap allows construction
			if(CanBuildAt(xPos, yPos, xSize, ySize, water))
			{
				cell = (xPos/4 + xDefMapSize * yPos/4);

				my_rating = terrainModifier * plateau_map[cell] - (*map)[cell] + 0.5f * (float)(rand()%10);
				//my_rating = - (*map)[cell];

				// determine minimum distance from buildpos to the edges of the map
				const int edge_distance = GetEdgeDistance(xPos, yPos);

				fprintf(file, "Pos: (%i,%i) -> Def map cell %i -> rating: %i  , edge_dist: %i\n",xPos, yPos, cell, (int)my_rating, edge_distance);

				// prevent aai from building defences too close to the edges of the map
				if( (float)edge_distance < range)
					my_rating -= (range - (float)edge_distance) * (range - (float)edge_distance);

				if(my_rating > best_rating)
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
						best_rating = my_rating;
					}
				}
			}
		}
	}

	fclose(file);

	return best_rating;
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

int AAIMap::getSmartContinentID(float3 *pos, const AAIMovementType& moveType) const
{
	// check if non sea/amphib unit in shallow water
	if(     (ai->GetAICallback()->GetElevation(pos->x, pos->z) < 0)
	     && (moveType.GetMovementType() == EMovementType::MOVEMENT_TYPE_GROUND) )
	{
		//look for closest land cell
		for(int k = 1; k < 10; ++k)
		{
			if(ai->GetAICallback()->GetElevation(pos->x + k * 16, pos->z) > 0)
			{
				pos->x += k *16;
				break;
			}
			else if(ai->GetAICallback()->GetElevation(pos->x - k * 16, pos->z) > 0)
			{
				pos->x -= k *16;
				break;
			}
			else if(ai->GetAICallback()->GetElevation(pos->x, pos->z + k * 16) > 0)
			{
				pos->z += k *16;
				break;
			}
			else if(ai->GetAICallback()->GetElevation(pos->x, pos->z - k * 16) > 0)
			{
				pos->z -= k *16;
				break;
			}
		}
	}

	int x = pos->x / 32;
	int y = pos->z / 32;

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
			const float lostUnits = ai->Getmap()->sector[x][y].GetLostUnits();

			if(lostUnits > maxLostUnits)
				maxLostUnits = lostUnits;
		}
	}

	return maxLostUnits;
}
