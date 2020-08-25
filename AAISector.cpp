// ------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// ------------------------------------------------------------------------

#include "AAISector.h"
#include "AAI.h"
#include "AAIBuildTable.h"
#include "AAIBrain.h"
#include "AAIConfig.h"
#include "AAIMap.h"

#include "LegacyCpp/IGlobalAICallback.h"
#include "LegacyCpp/UnitDef.h"
using namespace springLegacyAI;


AAISector::AAISector()
{
}

AAISector::~AAISector(void)
{
	m_enemyCombatUnits.clear();

	m_ownBuildingsOfCategory.clear();
}

void AAISector::Init(AAI *ai, int x, int y, int left, int right, int top, int bottom)
{
	this->ai = ai;

	// set coordinates of the corners
	this->x = x;
	this->y = y;

	this->left = left;
	this->right = right;
	this->top = top;
	this->bottom = bottom;

	// determine map border distance
	const int xEdgeDist = std::min(x, ai->Getmap()->xSectors - 1 - x);
	const int yEdgeDist = std::min(y, ai->Getmap()->ySectors - 1 - y);

	m_minSectorDistanceToMapEdge = std::min(xEdgeDist, yEdgeDist);

	const float3 center = GetCenter();
	continent = ai->Getmap()->GetContinentID(center);

	// init all kind of stuff
	freeMetalSpots = false;
	interior = false;
	distance_to_base = -1;
	last_scout = 1;
	rally_points = 0;

	// nothing sighted in that sector
	enemies_on_radar = 0;
	m_enemyBuildings  = 0;
	m_alliedBuildings = 0;
	failed_defences = 0;

	int categories = ai->Getbt()->assault_categories.size();

	importance_this_game = 1.0f + (rand()%5)/20.0f;

	m_enemyCombatUnits.resize(AAICombatUnitCategory::numberOfCombatUnitCategories, 0.0f);

	m_ownBuildingsOfCategory.resize(AAIUnitCategory::numberOfUnitCategories, 0);
}

void AAISector::LoadDataFromFile(FILE* file)
{
	if(file != nullptr)
	{
		fscanf(file, "%f %f %f", &flat_ratio, &water_ratio, &importance_learned);
			
		if(importance_learned < 1.0f)
			importance_learned += (rand()%5)/20.0f;

		m_attacksByTargetTypeInPreviousGames.LoadFromFile(file);
	}
	else // no learning data available -> init with default data
	{
		importance_learned = 1.0f + (rand()%5)/20.0f;
		flat_ratio  = DetermineFlatRatio();
		water_ratio = DetermineWaterRatio();
	}

	importance_this_game = importance_learned;
	//m_attacksByTargetTypeInCurrentGame = m_attacksByTargetTypeInPreviousGames;
}

void AAISector::SaveDataToFile(FILE* file)
{
	fprintf(file, "%f %f %f ", flat_ratio, water_ratio, importance_this_game);

	m_attacksByTargetTypeInPreviousGames.SaveToFile(file);
}

void AAISector::UpdateLearnedData()
{
	importance_this_game = 0.93f * (importance_this_game + 3.0f * importance_learned) / 4.0f;

	if(importance_this_game < 1.0f)
		importance_this_game = 1.0f;

	m_attacksByTargetTypeInCurrentGame.AddMobileTargetValues(m_attacksByTargetTypeInPreviousGames, 3.0f);
	m_attacksByTargetTypeInCurrentGame.DecreaseByFactor(0.225f); // 0.225f = 0.9f / 4.0f ->decrease by 0.9 and account for 3.0f in line above
}

void AAISector::AddMetalSpot(AAIMetalSpot *spot)
{
	metalSpots.push_back(spot);
	freeMetalSpots = true;
}

bool AAISector::SetBase(bool base)
{
	if(base)
	{
		// check if already occupied (may happen if two coms start in same sector)
		if(ai->Getmap()->team_sector_map[x][y] >= 0)
		{
			ai->Log("\nTeam %i could not add sector %i,%i to base, already occupied by ally team %i!\n\n",ai->GetAICallback()->GetMyTeam(), x, y, ai->Getmap()->team_sector_map[x][y]);
			return false;
		}

		distance_to_base = 0;

		// if free metal spots in this sectors, base has free spots
		for(list<AAIMetalSpot*>::iterator spot = metalSpots.begin(); spot != metalSpots.end(); ++spot)
		{
			if(!(*spot)->occupied)
			{
				ai->Getbrain()->m_freeMetalSpotsInBase = true;
				break;
			}
		}

		// increase importance
		importance_this_game += 1;

		ai->Getmap()->team_sector_map[x][y] = ai->GetAICallback()->GetMyTeam();

		if(importance_this_game > cfg->MAX_SECTOR_IMPORTANCE)
			importance_this_game = cfg->MAX_SECTOR_IMPORTANCE;

		return true;
	}
	else	// remove from base
	{
		distance_to_base = 1;

		ai->Getmap()->team_sector_map[x][y] = -1;

		return true;
	}
}

void AAISector::ResetLocalCombatPower() 
{
	m_alliedBuildings = 0;
	m_friendlyStaticCombatPower.Reset();
}

void AAISector::ResetScoutedEnemiesData() 
{ 
	m_enemyBuildings = 0;
	std::fill(m_enemyCombatUnits.begin(),  m_enemyCombatUnits.end(), 0.0f); 
	m_enemyStaticCombatPower.Reset();
	m_enemyMobileCombatPower.Reset();
};

void AAISector::AddFriendlyUnitData(UnitDefId unitDefId, bool unitBelongsToAlly)
{
	const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(unitDefId);

	// add building to sector (and update stat_combat_power if it's a stat defence)
	if(category.isBuilding())
	{
		if(unitBelongsToAlly)
			++m_alliedBuildings;

		if(category.isStaticDefence())
			m_friendlyStaticCombatPower.AddCombatPower( ai->s_buildTree.GetCombatPower(unitDefId) );
	}
}

void AAISector::AddScoutedEnemyUnit(UnitDefId enemyDefId, int lastUpdateInFrame)
{
	const AAIUnitCategory& categoryOfEnemyUnit = ai->s_buildTree.GetUnitCategory(enemyDefId);
	// add building to sector (and update stat_combat_power if it's a stat defence)
	if(categoryOfEnemyUnit.isBuilding())
	{
		++m_enemyBuildings;

		if(categoryOfEnemyUnit.isStaticDefence())
		{
			m_enemyStaticCombatPower.AddCombatPower( ai->s_buildTree.GetCombatPower(enemyDefId) );
		}
	}
	// add unit to sector and update mobile_combat_power
	else if(categoryOfEnemyUnit.isCombatUnit())
	{
		// units that have been scouted long time ago matter less
		const float lastSeen = exp(cfg->SCOUTING_MEMORY_FACTOR * ((float)(lastUpdateInFrame - ai->GetAICallback()->GetCurrentFrame())) / 3600.0f  );
		const AAICombatUnitCategory& combatCategory( categoryOfEnemyUnit );

		m_enemyCombatUnits[combatCategory.GetArrayIndex()] += lastSeen;

		m_enemyMobileCombatPower.AddCombatPower( ai->s_buildTree.GetCombatPower(enemyDefId), lastSeen );
	}
}

void AAISector::Update()
{
	// decrease values (so the ai "forgets" values from time to time)...
	//ground_threat *= 0.995;
	//air_threat *= 0.995;
	m_lostUnits    *= 0.95f;
	m_lostAirUnits *= 0.95f;
}

AAIMetalSpot* AAISector::GetFreeMetalSpot()
{
	// look for the first unoccupied metalspot
	for(list<AAIMetalSpot*>::iterator i = metalSpots.begin(); i != metalSpots.end(); ++i)
	{
		// if metalspot is occupied, try next one
		if(!(*i)->occupied)
			return *i;
	}


	return 0;
}
void AAISector::FreeMetalSpot(float3 pos, const UnitDef *extractor)
{
	float3 spot_pos;

	// get metalspot according to position
	for(list<AAIMetalSpot*>::iterator spot = metalSpots.begin(); spot != metalSpots.end(); ++spot)
	{
		// only check occupied spots
		if((*spot)->occupied)
		{
			// compare positions
			spot_pos = (*spot)->pos;
			ai->Getmap()->Pos2FinalBuildPos(&spot_pos, extractor);

			if(pos.x == spot_pos.x && pos.z == spot_pos.z)
			{
				(*spot)->occupied = false;
				(*spot)->extractor = -1;
				(*spot)->extractor_def = -1;

				freeMetalSpots = true;

				// if part of the base, tell the brain that the base has now free spots again
				if(distance_to_base == 0)
					ai->Getbrain()->m_freeMetalSpotsInBase = true;

				return;
			}
		}
	}
}

void AAISector::AddExtractor(int unit_id, int def_id, float3 *pos)
{
	float3 spot_pos;

	// get metalspot according to position
	for(list<AAIMetalSpot*>::iterator spot = metalSpots.begin(); spot != metalSpots.end(); ++spot)
	{
		// only check occupied spots
		if((*spot)->occupied)
		{
			// compare positions
			spot_pos = (*spot)->pos;
			ai->Getmap()->Pos2FinalBuildPos(&spot_pos, &ai->Getbt()->GetUnitDef(def_id));

			if(pos->x == spot_pos.x && pos->z == spot_pos.z)
			{
				(*spot)->extractor = unit_id;
				(*spot)->extractor_def = def_id;
			}
		}
	}
}

float3 AAISector::GetCenter()
{
	float3 pos;
	pos.x = (left + right)/2.0;
	pos.z = (top + bottom)/2.0;

	return pos;
}

/*float3 AAISector::GetCenterRallyPoint()
{

	return ZeroVector;
}*/


float3 AAISector::FindBuildsite(int building, bool water) const
{
	int xStart, xEnd, yStart, yEnd;

	DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);

	return ai->Getmap()->GetBuildSiteInRect(&ai->Getbt()->GetUnitDef(building), xStart, xEnd, yStart, yEnd, water);
}

float3 AAISector::GetDefenceBuildsite(UnitDefId buildingDefId, const AAITargetType& targetType, float terrainModifier, bool water) const
{
	float3 best_pos = ZeroVector, pos;
	
	int my_team = ai->GetAICallback()->GetMyAllyTeam();

	float my_rating, best_rating = -10000;

	std::list<Direction> directions;

	// get possible directions
	if(targetType.IsAir() && !cfg->AIR_ONLY_MOD)
	{
		directions.push_back(CENTER);
	}
	else
	{
		if(distance_to_base > 0)
			directions.push_back(CENTER);
		else
		{
			// filter out frontiers to other base sectors
			if(x > 0 && ai->Getmap()->sector[x-1][y].distance_to_base > 0 && (ai->Getmap()->sector[x-1][y].m_alliedBuildings < 5) && ai->Getmap()->team_sector_map[x-1][y] != my_team )
				directions.push_back(WEST);

			if(x < ai->Getmap()->xSectors-1 && ai->Getmap()->sector[x+1][y].distance_to_base > 0 && (ai->Getmap()->sector[x+1][y].m_alliedBuildings < 5) && ai->Getmap()->team_sector_map[x+1][y] != my_team)
				directions.push_back(EAST);

			if(y > 0 && ai->Getmap()->sector[x][y-1].distance_to_base > 0 && (ai->Getmap()->sector[x][y-1].m_alliedBuildings < 5) && ai->Getmap()->team_sector_map[x][y-1] != my_team)
				directions.push_back(NORTH);

			if(y < ai->Getmap()->ySectors-1 && ai->Getmap()->sector[x][y+1].distance_to_base > 0 && (ai->Getmap()->sector[x][y+1].m_alliedBuildings < 5) && ai->Getmap()->team_sector_map[x][y+1] != my_team)
				directions.push_back(SOUTH);
		}
	}

	int xStart = 0;
	int xEnd = 0;
	int yStart = 0;
	int yEnd = 0;

	// check possible directions
	for(list<Direction>::iterator dir =directions.begin(); dir != directions.end(); ++dir)
	{
		// get area to perform search
		if(*dir == CENTER)
		{
			xStart = x * ai->Getmap()->xSectorSizeMap;
			xEnd = (x+1) * ai->Getmap()->xSectorSizeMap;
			yStart = y * ai->Getmap()->ySectorSizeMap;
			yEnd = (y+1) * ai->Getmap()->ySectorSizeMap;
		}
		else if(*dir == WEST)
		{
			xStart = x * ai->Getmap()->xSectorSizeMap;
			xEnd = x * ai->Getmap()->xSectorSizeMap + ai->Getmap()->xSectorSizeMap/4.0f;
			yStart = y * ai->Getmap()->ySectorSizeMap;
			yEnd = (y+1) * ai->Getmap()->ySectorSizeMap;
		}
		else if(*dir == EAST)
		{
			xStart = (x+1) * ai->Getmap()->xSectorSizeMap - ai->Getmap()->xSectorSizeMap/4.0f;
			xEnd = (x+1) * ai->Getmap()->xSectorSizeMap;
			yStart = y * ai->Getmap()->ySectorSizeMap;
			yEnd = (y+1) * ai->Getmap()->ySectorSizeMap;
		}
		else if(*dir == NORTH)
		{
			xStart = x * ai->Getmap()->xSectorSizeMap;
			xEnd = (x+1) * ai->Getmap()->xSectorSizeMap;
			yStart = y * ai->Getmap()->ySectorSizeMap;
			yEnd = y * ai->Getmap()->ySectorSizeMap + ai->Getmap()->ySectorSizeMap/4.0f;
		}
		else if(*dir == SOUTH)
		{
			xStart = x * ai->Getmap()->xSectorSizeMap ;
			xEnd = (x+1) * ai->Getmap()->xSectorSizeMap;
			yStart = (y+1) * ai->Getmap()->ySectorSizeMap - ai->Getmap()->ySectorSizeMap/4.0f;
			yEnd = (y+1) * ai->Getmap()->ySectorSizeMap;
		}

		//
		const UnitDef *def = &ai->Getbt()->GetUnitDef(buildingDefId.id);
		my_rating = ai->Getmap()->GetDefenceBuildsite(&pos, def, xStart, xEnd, yStart, yEnd, targetType, terrainModifier, water);

		if(my_rating > best_rating)
		{
			best_pos = pos;
			best_rating = my_rating;
		}
	}

	return best_pos;
}

float3 AAISector::GetCenterBuildsite(int building, bool water)
{
	int xStart, xEnd, yStart, yEnd;

	DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);

	return ai->Getmap()->GetCenterBuildsite(&ai->Getbt()->GetUnitDef(building), xStart, xEnd, yStart, yEnd, water);
}

float3 AAISector::GetRadarArtyBuildsite(int building, float range, bool water)
{
	int xStart, xEnd, yStart, yEnd;

	DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);

	return ai->Getmap()->GetRadarArtyBuildsite(&ai->Getbt()->GetUnitDef(building), xStart, xEnd, yStart, yEnd, range, water);
}

float3 AAISector::GetHighestBuildsite(int building)
{
	if(building < 1)
	{
		ai->Log("ERROR: Invalid building def id %i passed to AAISector::GetRadarBuildsite()\n", building);
		return ZeroVector;
	}

	int xStart, xEnd, yStart, yEnd;

	DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);

	return ai->Getmap()->GetHighestBuildsite(&ai->Getbt()->GetUnitDef(building), xStart, xEnd, yStart, yEnd);
}

float3 AAISector::GetRandomBuildsite(int building, int tries, bool water)
{
	if(building < 1)
	{
		ai->Log("ERROR: Invalid building def id %i passed to AAISector::GetRadarBuildsite()\n", building);
		return ZeroVector;
	}

	int xStart, xEnd, yStart, yEnd;

	DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);

	return ai->Getmap()->GetRandomBuildsite(&ai->Getbt()->GetUnitDef(building), xStart, xEnd, yStart, yEnd, tries, water);
}

void AAISector::DetermineBuildsiteRectangle(int *xStart, int *xEnd, int *yStart, int *yEnd) const
{
	*xStart = x * ai->Getmap()->xSectorSizeMap;
	*xEnd = *xStart + ai->Getmap()->xSectorSizeMap;

	if(*xStart == 0)
		*xStart = 8;

	*yStart = y * ai->Getmap()->ySectorSizeMap;
	*yEnd = *yStart + ai->Getmap()->ySectorSizeMap;

	if(*yStart == 0)
		*yStart = 8;

	// reserve buildspace for def. buildings
	if(x > 0 && ai->Getmap()->sector[x-1][y].distance_to_base > 0 )
		*xStart += ai->Getmap()->xSectorSizeMap/8;

	if(x < ai->Getmap()->xSectors-1 && ai->Getmap()->sector[x+1][y].distance_to_base > 0)
		*xEnd -= ai->Getmap()->xSectorSizeMap/8;

	if(y > 0 && ai->Getmap()->sector[x][y-1].distance_to_base > 0)
		*yStart += ai->Getmap()->ySectorSizeMap/8;

	if(y < ai->Getmap()->ySectors-1 && ai->Getmap()->sector[x][y+1].distance_to_base > 0)
		*yEnd -= ai->Getmap()->ySectorSizeMap/8;
}

// converts unit positions to cell coordinates
void AAISector::Pos2SectorMapPos(float3 *pos, const UnitDef* def)
{
	// get cell index of middlepoint
	pos->x = ((int) pos->x/SQUARE_SIZE)%ai->Getmap()->xSectorSizeMap;
	pos->z = ((int) pos->z/SQUARE_SIZE)%ai->Getmap()->ySectorSizeMap;

	// shift to the leftmost uppermost cell
	pos->x -= def->xsize/2;
	pos->z -= def->zsize/2;

	// check if pos is still in that scetor, otherwise retun 0
	if(pos->x < 0 && pos->z < 0)
		pos->x = pos->z = 0;
}

void AAISector::SectorMapPos2Pos(float3 *pos, const UnitDef *def)
{
	// shift to middlepoint
	pos->x += def->xsize/2;
	pos->z += def->zsize/2;

	// get cell position on complete map
	pos->x += x * ai->Getmap()->xSectorSizeMap;
	pos->z += y * ai->Getmap()->ySectorSizeMap;

	// back to unit coordinates
	pos->x *= SQUARE_SIZE;
	pos->z *= SQUARE_SIZE;
}

float AAISector::GetLocalAttacksBy(const AAITargetType& targetType, float previousGames, float currentGame) const
{
	const float totalAttacks = (previousGames * m_attacksByTargetTypeInPreviousGames.GetValueOfTargetType(targetType) + currentGame * m_attacksByTargetTypeInCurrentGame.GetValueOfTargetType(targetType) );
	return  totalAttacks / (previousGames + currentGame);
}

float AAISector::GetEnemyDefencePower(const CombatPower& combatCategoryWeigths) const
{
	return (combatCategoryWeigths.vsGround  * (m_enemyStaticCombatPower.GetValueOfTargetType(ETargetType::SURFACE)   + m_enemyMobileCombatPower.GetValueOfTargetType(ETargetType::SURFACE))
		+ combatCategoryWeigths.vsAir       * (m_enemyStaticCombatPower.GetValueOfTargetType(ETargetType::AIR)       + m_enemyMobileCombatPower.GetValueOfTargetType(ETargetType::AIR))
		+ combatCategoryWeigths.vsHover     * (m_enemyStaticCombatPower.GetValueOfTargetType(ETargetType::SURFACE)   + m_enemyMobileCombatPower.GetValueOfTargetType(ETargetType::SURFACE))
		+ combatCategoryWeigths.vsSea       * (m_enemyStaticCombatPower.GetValueOfTargetType(ETargetType::FLOATER)   + m_enemyMobileCombatPower.GetValueOfTargetType(ETargetType::FLOATER))
		+ combatCategoryWeigths.vsSubmarine * (m_enemyStaticCombatPower.GetValueOfTargetType(ETargetType::SUBMERGED) + m_enemyMobileCombatPower.GetValueOfTargetType(ETargetType::SUBMERGED)) );
}

float AAISector::GetEnemyAreaCombatPowerVs(const AAITargetType& targetType, float neighbourImportance) const
{
	float result = GetEnemyCombatPower(targetType);

	// take neighbouring sectors into account (if possible)
	if(x > 0)
		result += neighbourImportance * ai->Getmap()->sector[x-1][y].GetEnemyCombatPower(targetType);

	if(x < ai->Getmap()->xSectors-1)
		result += neighbourImportance * ai->Getmap()->sector[x+1][y].GetEnemyCombatPower(targetType);

	if(y > 0)
		result += neighbourImportance * ai->Getmap()->sector[x][y-1].GetEnemyCombatPower(targetType);

	if(y < ai->Getmap()->ySectors-1)
		result += neighbourImportance * ai->Getmap()->sector[x][y+1].GetEnemyCombatPower(targetType);

	return result;
}

float AAISector::DetermineWaterRatio() const
{
	int waterCells(0);

	for(int yPos = y * ai->Getmap()->ySectorSizeMap; yPos < (y+1) * ai->Getmap()->ySectorSizeMap; ++yPos)
	{
		for(int xPos = x * ai->Getmap()->xSectorSizeMap; xPos < (x+1) * ai->Getmap()->xSectorSizeMap; ++xPos)
		{
			if(ai->Getmap()->buildmap[xPos + yPos * ai->Getmap()->xMapSize] == 4)
				++waterCells;
		}
	}

	const int totalCells = ai->Getmap()->xSectorSizeMap * ai->Getmap()->ySectorSizeMap;

	return waterCells / static_cast<float>(totalCells);
}

float AAISector::DetermineFlatRatio() const
{
	// get number of cliffy & flat cells
	const int cliffyCells = ai->Getmap()->GetCliffyCells(left/SQUARE_SIZE, top/SQUARE_SIZE, ai->Getmap()->xSectorSizeMap, ai->Getmap()->ySectorSizeMap);
	const int totalCells  = ai->Getmap()->xSectorSizeMap * ai->Getmap()->ySectorSizeMap;
	const int flatCells   = totalCells - cliffyCells;

	return static_cast<float>(flatCells) / static_cast<float>(totalCells);
}

void AAISector::UpdateThreatValues(UnitDefId destroyedDefId, UnitDefId attackerDefId)
{
	const AAIUnitCategory& destroyedCategory = ai->s_buildTree.GetUnitCategory(destroyedDefId);
	const AAIUnitCategory& attackerCategory  = ai->s_buildTree.GetUnitCategory(attackerDefId);

	// if lost unit is a building, increase attacked_by
	if(destroyedCategory.isBuilding())
	{
		if(attackerCategory.isCombatUnit())
		{
			const float increment = interior ? 0.3f : 1.0f;
			
			m_attacksByTargetTypeInCurrentGame.AddValueForTargetType(ai->s_buildTree.GetTargetType(attackerDefId) , increment);
		}
	}
	else // unit was lost
	{
		if(ai->s_buildTree.GetMovementType(destroyedDefId).IsAir())
			m_lostAirUnits += 1.0f;
		else
			m_lostUnits += 1.0f;
	}
}

bool AAISector::PosInSector(const float3& pos) const
{
	if( (pos.x < left) || (pos.x > right) || (pos.z < top) || (pos.z > bottom) )
		return false;
	else
		return true;
}

bool AAISector::ConnectedToOcean()
{
	if(water_ratio < 0.2)
		return false;

	// find water cell
	int x_cell = (left + right) / 16.0f;
	int y_cell = (top + bottom) / 16.0f;

	// get continent
	int cont = ai->Getmap()->GetContinentID(x_cell, y_cell);

	if(ai->Getmap()->continents[cont].water)
	{
		if(ai->Getmap()->continents[cont].size > 1200 && ai->Getmap()->continents[cont].size > 0.5f * (float)ai->Getmap()->avg_water_continent_size )
			return true;
	}

	return false;
}

bool AAISector::determineMovePos(float3 *pos)
{
	int x,y;
	*pos = ZeroVector;

	// try to get random spot
	for(int i = 0; i < 6; ++i)
	{
		pos->x = left + ai->Getmap()->xSectorSize * (0.2f + 0.06f * (float)(rand()%11) );
		pos->z = top + ai->Getmap()->ySectorSize * (0.2f + 0.06f * (float)(rand()%11) );

		// check if blocked by  building
		x = (int) (pos->x / SQUARE_SIZE);
		y = (int) (pos->z / SQUARE_SIZE);

		if(ai->Getmap()->buildmap[x + y * ai->Getmap()->xMapSize] != 1)
			return true;
	}

	// search systematically
	for(int i = 0; i < ai->Getmap()->xSectorSizeMap; i += 8)
	{
		for(int j = 0; j < ai->Getmap()->ySectorSizeMap; j += 8)
		{
			pos->x = left + i * SQUARE_SIZE;
			pos->z = top + j * SQUARE_SIZE;

			// get cell index of middlepoint
			x = (int) (pos->x / SQUARE_SIZE);
			y = (int) (pos->z / SQUARE_SIZE);

			if(ai->Getmap()->buildmap[x + y * ai->Getmap()->xMapSize] != 1)
				return true;
		}
	}

	// no free cell found (should not happen)
	*pos = ZeroVector;
	return false;
}

bool AAISector::determineMovePosOnContinent(float3 *pos, int continent)
{
	int x,y;
	*pos = ZeroVector;

	// try to get random spot
	for(int i = 0; i < 6; ++i)
	{
		pos->x = left + ai->Getmap()->xSectorSize * (0.2f + 0.06f * (float)(rand()%11) );
		pos->z = top + ai->Getmap()->ySectorSize * (0.2f + 0.06f * (float)(rand()%11) );

		// check if blocked by  building
		x = (int) (pos->x / SQUARE_SIZE);
		y = (int) (pos->z / SQUARE_SIZE);

		if(ai->Getmap()->buildmap[x + y * ai->Getmap()->xMapSize] != 1)
		{
			//check continent
			if(ai->Getmap()->GetContinentID(*pos) == continent)
				return true;
		}
	}

	// search systematically
	for(int i = 0; i < ai->Getmap()->xSectorSizeMap; i += 8)
	{
		for(int j = 0; j < ai->Getmap()->ySectorSizeMap; j += 8)
		{
			pos->x = left + i * SQUARE_SIZE;
			pos->z = top + j * SQUARE_SIZE;

			// get cell index of middlepoint
			x = (int) (pos->x / SQUARE_SIZE);
			y = (int) (pos->z / SQUARE_SIZE);

			if(ai->Getmap()->buildmap[x + y * ai->Getmap()->xMapSize] != 1)
			{
				if(ai->Getmap()->GetContinentID(*pos) == continent)
					return true;
			}
		}
	}

	*pos = ZeroVector;
	return false;
}
