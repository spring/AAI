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

void AAISector::Init(AAI *ai, int x, int y)
{
	this->ai = ai;

	// set coordinates of the corners
	this->x = x;
	this->y = y;

	left   =     x * AAIMap::xSectorSize;
	right  = (x+1) * AAIMap::xSectorSize;
	top    =     y * AAIMap::ySectorSize;
	bottom = (y+1) * AAIMap::ySectorSize;

	// determine map border distance
	const int xEdgeDist = std::min(x, AAIMap::xSectors - 1 - x);
	const int yEdgeDist = std::min(y, AAIMap::ySectors - 1 - y);

	m_minSectorDistanceToMapEdge = std::min(xEdgeDist, yEdgeDist);

	const float3 center = GetCenter();
	continent = ai->Getmap()->GetContinentID(center);

	// init all kind of stuff
	m_freeMetalSpots = false;
	distance_to_base = -1;
	m_skippedAsScoutDestination = 0;
	rally_points = 0;

	// nothing sighted in that sector
	m_enemyUnitsDetectedBySensor = 0;
	m_enemyBuildings  = 0;
	m_alliedBuildings = 0;
	m_failedAttemptsToConstructStaticDefence = 0;

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

bool AAISector::SetBase(bool base)
{
	if(base)
	{
		// check if already occupied (may happen if two coms start in same sector)
		if(AAIMap::s_teamSectorMap.IsSectorOccupied(x,y))
		{
			ai->Log("\nTeam %i could not add sector %i,%i to base, already occupied by ally team %i!\n\n",ai->GetAICallback()->GetMyAllyTeam(), x, y, AAIMap::s_teamSectorMap.GetTeam(x, y));
			return false;
		}

		distance_to_base = 0;

		// if free metal spots in this sectors, base has free spots
		for(auto spot = metalSpots.begin(); spot != metalSpots.end(); ++spot)
		{
			if(!(*spot)->occupied)
			{
				ai->Getbrain()->m_freeMetalSpotsInBase = true;
				break;
			}
		}

		// increase importance
		importance_this_game += 1;

		AAIMap::s_teamSectorMap.SetSectorAsOccupiedByTeam(x, y, ai->GetMyTeamId());

		if(importance_this_game > cfg->MAX_SECTOR_IMPORTANCE)
			importance_this_game = cfg->MAX_SECTOR_IMPORTANCE;

		return true;
	}
	else	// remove from base
	{
		distance_to_base = 1;

		AAIMap::s_teamSectorMap.SetSectorAsUnoccupied(x, y);

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

void AAISector::DecreaseLostUnits()
{
	// decrease values (so the ai "forgets" values from time to time)...
	m_lostUnits    *= 0.95f;
	m_lostAirUnits *= 0.95f;
}

void AAISector::AddMetalSpot(AAIMetalSpot *spot)
{
	metalSpots.push_back(spot);
	m_freeMetalSpots = true;
}

void AAISector::FreeMetalSpot(float3 pos, const UnitDef *extractor)
{
	// get metalspot according to position
	for(auto spot = metalSpots.begin(); spot != metalSpots.end(); ++spot)
	{
		// only check occupied spots
		if((*spot)->occupied)
		{
			// compare positions
			ai->Getmap()->Pos2FinalBuildPos(&(*spot)->pos, extractor);

			//! @todo Replace with comparison accounting for floating point inaccuracy
			if(pos.x == (*spot)->pos.x && pos.z == (*spot)->pos.z)
			{
				(*spot)->occupied = false;
				(*spot)->extractor = -1;
				(*spot)->extractor_def = -1;

				m_freeMetalSpots = true;

				// if part of the base, tell the brain that the base has now free spots again
				if(distance_to_base == 0)
					ai->Getbrain()->m_freeMetalSpotsInBase = true;

				return;
			}
		}
	}
}

void AAISector::UpdateFreeMetalSpots()
{
	m_freeMetalSpots = false;

	for(auto spot = metalSpots.begin(); spot != metalSpots.end(); ++spot)
	{
		if((*spot)->occupied == false)
		{
			m_freeMetalSpots = true;
			return;
		}
	}
}

void AAISector::AddExtractor(int unit_id, int def_id, float3 *pos)
{
	for(std::list<AAIMetalSpot*>::iterator spot = metalSpots.begin(); spot != metalSpots.end(); ++spot)
	{
		// only check occupied spots
		if((*spot)->occupied)
		{
			ai->Getmap()->Pos2FinalBuildPos(&(*spot)->pos, &ai->Getbt()->GetUnitDef(def_id));

			if(pos->x == (*spot)->pos.x && pos->z == (*spot)->pos.z)
			{
				(*spot)->extractor = unit_id;
				(*spot)->extractor_def = def_id;
			}
		}
	}
}

float3 AAISector::GetCenter() const
{
	float3 pos;
	pos.x = (left + right)/2.0f;
	pos.z = (top + bottom)/2.0f;

	return pos;
}

float AAISector::GetImportanceForStaticDefenceVs(AAITargetType& targetType, const GamePhase& gamePhase, float previousGames, float currentGame)
{
	if( AreFurtherStaticDefencesAllowed() )
	{
		if(m_failedAttemptsToConstructStaticDefence < 2) // do not try to build defences if last two attempts failed
		{
			std::vector<float> importanceVsTargetType(AAITargetType::numberOfMobileTargetTypes, 0.0f);

			importanceVsTargetType[AAITargetType::airIndex] =  
						  (0.1f + GetLocalAttacksBy(ETargetType::AIR, previousGames, currentGame) + ai->Getbrain()->GetAttacksBy(ETargetType::AIR, gamePhase)) 
						/ (1.0f + GetFriendlyStaticDefencePower(ETargetType::AIR));

			if(water_ratio < 0.7f)
			{
				importanceVsTargetType[AAITargetType::surfaceIndex] =  
						  (0.1f + GetLocalAttacksBy(ETargetType::SURFACE, previousGames, currentGame) + ai->Getbrain()->GetAttacksBy(ETargetType::SURFACE, gamePhase)) 
						/ (1.0f + GetFriendlyStaticDefencePower(ETargetType::SURFACE));
			}

			if(water_ratio > 0.3f)
			{
				importanceVsTargetType[AAITargetType::floaterIndex] =  
						  (0.1f + GetLocalAttacksBy(ETargetType::FLOATER, previousGames, currentGame) + ai->Getbrain()->GetAttacksBy(ETargetType::FLOATER, gamePhase)) 
						/ (1.0f + GetFriendlyStaticDefencePower(ETargetType::FLOATER));

				importanceVsTargetType[AAITargetType::submergedIndex] =  
						  (0.1f + GetLocalAttacksBy(ETargetType::SUBMERGED, previousGames, currentGame) + ai->Getbrain()->GetAttacksBy(ETargetType::SUBMERGED, gamePhase)) 
						/ (1.0f + GetFriendlyStaticDefencePower(ETargetType::SUBMERGED));
			}

			float highestImportance(0.0f);

			for(int targetTypeId = 0; targetTypeId < importanceVsTargetType.size(); ++targetTypeId)
			{
				if( importanceVsTargetType[targetTypeId] > highestImportance )
				{
					highestImportance = importanceVsTargetType[targetTypeId];
					targetType 		  = AAITargetType(static_cast<ETargetType>(targetTypeId));
				}
			}

			// modify importance based on location of sector (higher importance for sectors "facing" the enemy)
			if(highestImportance > 0.0f)
			{
				const MapPos& enemyBaseCenter = ai->Getmap()->GetCenterOfEnemyBase();
				const MapPos& baseCenter      = ai->Getbrain()->GetCenterOfBase();

				MapPos sectorCenter(x * AAIMap::xSectorSizeMap + AAIMap::xSectorSizeMap/2, y * AAIMap::ySectorSizeMap + AAIMap::ySectorSizeMap/2);

				const int distEnemyBase =   (enemyBaseCenter.x - sectorCenter.x)*(enemyBaseCenter.x - sectorCenter.x)
										  + (enemyBaseCenter.y - sectorCenter.y)*(enemyBaseCenter.y - sectorCenter.y);

				const int distOwnToEnemyBase  =   (enemyBaseCenter.x - baseCenter.x)*(enemyBaseCenter.x - baseCenter.x)
										  		+ (enemyBaseCenter.y - baseCenter.y)*(enemyBaseCenter.y - baseCenter.y);

				if(distEnemyBase < distOwnToEnemyBase)
					highestImportance *= 2.0f;

				highestImportance *= static_cast<float>(2 + this->GetEdgeDistance());
			}

			return highestImportance;
		}

		m_failedAttemptsToConstructStaticDefence = 0;
	}

	return 0.0f;
}

float AAISector::GetAttackRating(const AAISector* currentSector, bool landSectorSelectable, bool waterSectorSelectable, const MobileTargetTypeValues& targetTypeOfUnits) const
{
	float rating(0.0f);

	if( (distance_to_base > 0) && (GetNumberOfEnemyBuildings() > 0) )
	{
		const bool landCheckPassed  = landSectorSelectable  && (water_ratio < 0.35f);
		const bool waterCheckPassed = waterSectorSelectable && (water_ratio > 0.65f);

		if(landCheckPassed || waterCheckPassed)
		{
			const float dx = static_cast<float>(x - currentSector->x);
			const float dy = static_cast<float>(y - currentSector->y);
			const float dist = fastmath::apxsqrt(dx*dx + dy*dy );

			const float enemyBuildings = static_cast<float>(GetNumberOfEnemyBuildings());

			// prefer sectors with many buildings, few lost units and low defence power/short distance to current sector
			rating = GetLostUnits() * enemyBuildings / ( (1.0f + GetEnemyDefencePower(targetTypeOfUnits)) * (1.0f + dist) );
		}
	}

	return rating;			
}


float AAISector::GetAttackRating(const std::vector<float>& globalCombatPower, const std::vector< std::vector<float> >& continentCombatPower, const MobileTargetTypeValues& assaultGroupsOfType, float maxLostUnits) const
{
	float rating(0.0f);

	if( (distance_to_base > 0) && (GetNumberOfEnemyBuildings() > 0))
	{
		const float myAttackPower     =   globalCombatPower[AAITargetType::staticIndex] + continentCombatPower[continent][AAITargetType::staticIndex];
		const float enemyDefencePower =   assaultGroupsOfType.GetValueOfTargetType(ETargetType::SURFACE)   * GetEnemyCombatPower(ETargetType::SURFACE)
										+ assaultGroupsOfType.GetValueOfTargetType(ETargetType::FLOATER)   * GetEnemyCombatPower(ETargetType::FLOATER)
										+ assaultGroupsOfType.GetValueOfTargetType(ETargetType::SUBMERGED) * GetEnemyCombatPower(ETargetType::SUBMERGED);

		const float lostUnitsFactor = (maxLostUnits > 1.0f) ? (2.0f - (GetLostUnits() / maxLostUnits) ) : 1.0f;

		const float enemyBuildings = static_cast<float>(GetNumberOfEnemyBuildings());

		// prefer sectors with many buildings, few lost units and low defence power/short distance to own base
		rating = lostUnitsFactor * (2.0f + enemyBuildings) * myAttackPower / ( (1.5f + enemyDefencePower) * static_cast<float>(1 + 2 * distance_to_base) );
	}

	return rating;			
}

float AAISector::GetRatingAsNextScoutDestination(const AAIMovementType& scoutMoveType, const float3& currentPositionOfScout)
{
	if(   (distance_to_base == 0) 
	   || (scoutMoveType.isIncludedIn(m_suitableMovementTypes) == false) 
	   || (GetNumberOfAlliedBuildings() > 0) )
		return 0.0f;
	else
	{
		++m_skippedAsScoutDestination;

		const float3 center = GetCenter();
		const float dx = currentPositionOfScout.x - center.x;
		const float dy = currentPositionOfScout.z - center.z;

		// factor between 0.1 (max dist from one corner of the map tpo the other) and 1.0 
		const float distanceToCurrentLocationFactor = 0.1f + 0.9f * (1.0f - (dx*dx+dy*dy) / AAIMap::maxSquaredMapDist);

		// factor between 1 and 0.4 (depending on number of recently lost units)
		const float lostUnits = scoutMoveType.IsAir() ? m_lostAirUnits : m_lostUnits;
		const float lostScoutsFactor = 0.4f + 0.6f / (0.5f * lostUnits + 1.0f);

		const float metalSpotsFactor = 2.0f + static_cast<float>(metalSpots.size());

		//! @todo Take learned starting locations into account in early phase
		return metalSpotsFactor * distanceToCurrentLocationFactor * lostScoutsFactor * static_cast<float>(m_skippedAsScoutDestination);
	}
}

float AAISector::GetStartSectorRating() const
{
	if(AAIMap::s_teamSectorMap.IsSectorOccupied(x, y))
		return 0.0f;
	else
		return ( static_cast<float>(2 * GetNumberOfMetalSpots() + 1) ) * flat_ratio * flat_ratio;
}

bool AAISector::IsSectorSuitableForBaseExpansion() const
{
	return     (IsOccupiedByEnemies() == false)
			&& (GetNumberOfAlliedBuildings() < 3)
			&& (AAIMap::s_teamSectorMap.IsSectorOccupied(x, y) == false);
}

bool AAISector::ShallBeConsideredForExtractorConstruction() const
{
	return 	   m_freeMetalSpots 
			&& (AAIMap::s_teamSectorMap.IsOccupiedByOtherTeam(x, y, ai->GetMyTeamId()) == false)
			&& (IsOccupiedByEnemies() == false);	
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

float3 AAISector::GetRadarArtyBuildsite(int building, float range, bool water)
{
	int xStart, xEnd, yStart, yEnd;

	DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);

	return ai->Getmap()->GetRadarArtyBuildsite(&ai->Getbt()->GetUnitDef(building), xStart, xEnd, yStart, yEnd, range, water);
}

float3 AAISector::DetermineAttackPosition() const
{
	if(GetNumberOfEnemyBuildings() == 0)
		return GetCenter();
	else
	{
		const int xStart( x   * AAIMap::xSectorSizeMap);
		const int xEnd( (x+1) * AAIMap::xSectorSizeMap);
		const int yStart( y   * AAIMap::ySectorSizeMap);
		const int yEnd( (y+1) * AAIMap::ySectorSizeMap);
		return ai->Getmap()->DeterminePositionOfEnemyBuildingInSector(xStart, xEnd, yStart, yEnd);
	}
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
	/*if(x > 0 && ai->Getmap()->m_sector[x-1][y].distance_to_base > 0 )
		*xStart += ai->Getmap()->xSectorSizeMap/8;

	if(x < ai->Getmap()->xSectors-1 && ai->Getmap()->m_sector[x+1][y].distance_to_base > 0)
		*xEnd -= ai->Getmap()->xSectorSizeMap/8;

	if(y > 0 && ai->Getmap()->m_sector[x][y-1].distance_to_base > 0)
		*yStart += ai->Getmap()->ySectorSizeMap/8;

	if(y < ai->Getmap()->ySectors-1 && ai->Getmap()->m_sector[x][y+1].distance_to_base > 0)
		*yEnd -= ai->Getmap()->ySectorSizeMap/8;*/
}

float AAISector::GetLocalAttacksBy(const AAITargetType& targetType, float previousGames, float currentGame) const
{
	const float totalAttacks = (previousGames * m_attacksByTargetTypeInPreviousGames.GetValueOfTargetType(targetType) + currentGame * m_attacksByTargetTypeInCurrentGame.GetValueOfTargetType(targetType) );
	return  totalAttacks / (previousGames + currentGame);
}

float AAISector::GetEnemyDefencePower(const MobileTargetTypeValues& targetTypeOfUnits) const
{
	float defencePower(0.0f);
	for(AAITargetType targetType(AAITargetType::first); targetType.MobileTargetTypeEnd() == false; targetType.Next())
	{
		const float totalDefPower = m_enemyStaticCombatPower.GetValueOfTargetType(targetType) + m_enemyMobileCombatPower.GetValueOfTargetType(targetType);
		defencePower += targetTypeOfUnits.GetValueOfTargetType(targetType) * totalDefPower;
	}

	return defencePower;
}

float AAISector::GetEnemyAreaCombatPowerVs(const AAITargetType& targetType, float neighbourImportance) const
{
	float result = GetEnemyCombatPower(targetType);

	// take neighbouring sectors into account (if possible)
	if(x > 0)
		result += neighbourImportance * ai->Getmap()->m_sector[x-1][y].GetEnemyCombatPower(targetType);

	if(x < ai->Getmap()->xSectors-1)
		result += neighbourImportance * ai->Getmap()->m_sector[x+1][y].GetEnemyCombatPower(targetType);

	if(y > 0)
		result += neighbourImportance * ai->Getmap()->m_sector[x][y-1].GetEnemyCombatPower(targetType);

	if(y < ai->Getmap()->ySectors-1)
		result += neighbourImportance * ai->Getmap()->m_sector[x][y+1].GetEnemyCombatPower(targetType);

	return result;
}

float AAISector::DetermineWaterRatio() const
{
	int waterCells(0);

	for(int yPos = y * AAIMap::ySectorSizeMap; yPos < (y+1) * AAIMap::ySectorSizeMap; ++yPos)
	{
		for(int xPos = x * AAIMap::xSectorSizeMap; xPos < (x+1) * AAIMap::xSectorSizeMap; ++xPos)
		{
			if(AAIMap::s_buildmap[xPos + yPos * AAIMap::xMapSize].IsTileTypeSet(EBuildMapTileType::WATER))
				++waterCells;
		}
	}

	const int totalCells = AAIMap::xSectorSizeMap * AAIMap::ySectorSizeMap;

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
			const float increment = (distance_to_base == 0) ? 0.5f : 1.0f;
			
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

float3 AAISector::DetermineUnitMovePos(AAIMovementType moveType, int continentId) const
{
	BuildMapTileType forbiddenMapTileTypes(EBuildMapTileType::OCCUPIED);
	forbiddenMapTileTypes.SetTileType(EBuildMapTileType::BLOCKED_SPACE); 

	if(moveType.IsSeaUnit())
		forbiddenMapTileTypes.SetTileType(EBuildMapTileType::LAND);
	else if(moveType.IsAmphibious() || moveType.IsHover())
		forbiddenMapTileTypes.SetTileType(EBuildMapTileType::CLIFF);
	else if(moveType.IsGround())
	{
		forbiddenMapTileTypes.SetTileType(EBuildMapTileType::WATER);
		forbiddenMapTileTypes.SetTileType(EBuildMapTileType::CLIFF);
	}

	// try to get random spot
	for(int i = 0; i < 6; ++i)
	{
		float3 position;
		position.x = left + AAIMap::xSectorSize * (0.2f + 0.06f * (float)(rand()%11) );
		position.z = top  + AAIMap::ySectorSize * (0.2f + 0.06f * (float)(rand()%11) );

		if(IsValidMovePos(position, forbiddenMapTileTypes, continentId))
		{
			position.y = ai->GetAICallback()->GetElevation(position.x, position.z);
			return position;
		}
	}

	// search systematically
	for(int i = 0; i < AAIMap::xSectorSizeMap; i += 4)
	{
		for(int j = 0; j < AAIMap::ySectorSizeMap; j += 4)
		{
			float3 position;
			position.x = left + i * SQUARE_SIZE;
			position.z = top  + j * SQUARE_SIZE;

			if(IsValidMovePos(position, forbiddenMapTileTypes, continentId))
			{
				position.y = ai->GetAICallback()->GetElevation(position.x, position.z);
				return position;
			}
		}
	}

	return ZeroVector;
}

bool AAISector::IsValidMovePos(const float3& pos, BuildMapTileType forbiddenMapTileTypes, int continentId) const
{
	const int x = (int) (pos.x / SQUARE_SIZE);
	const int y = (int) (pos.z / SQUARE_SIZE);

	if(AAIMap::s_buildmap[x + y * AAIMap::xMapSize].IsTileTypeNotSet(forbiddenMapTileTypes))
	{
		if( (continentId == AAIMap::ignoreContinentID) || (ai->Getmap()->GetContinentID(pos) == continentId) )
			return true;
	}
	return false;
}

bool AAISector::AreFurtherStaticDefencesAllowed() const
{
	return 	   (GetNumberOfBuildings(EUnitCategory::STATIC_DEFENCE) < cfg->MAX_DEFENCES) 
			&& (GetNumberOfAlliedBuildings() < 3)
			&& (AAIMap::s_teamSectorMap.IsOccupiedByOtherTeam(x, y, ai->GetMyTeamId()) == false);
}
