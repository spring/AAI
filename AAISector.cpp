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

AAISector::AAISector() :
	m_distanceToBase(-1), 
	m_lostUnits(0.0f),
	m_lostAirUnits(0.0f),
	m_enemyCombatUnits(0.0f),
	m_skippedAsScoutDestination(0)
{
}

AAISector::~AAISector(void)
{
	m_ownBuildingsOfCategory.clear();
}

void AAISector::Init(AAI *ai, int x, int y)
{
	this->ai = ai;

	// set coordinates of the corners
	this->x = x;
	this->y = y;

	// determine map border distance
	const int xEdgeDist = std::min(x, AAIMap::xSectors - 1 - x);
	const int yEdgeDist = std::min(y, AAIMap::ySectors - 1 - y);

	m_minSectorDistanceToMapEdge = std::min(xEdgeDist, yEdgeDist);

	const float3 center = GetCenter();
	m_continentId = AAIMap::GetContinentID(center);

	m_freeMetalSpots = false;

	// nothing sighted in that sector
	m_enemyUnitsDetectedBySensor = 0;
	m_enemyBuildings  = 0;
	m_alliedBuildings = 0;
	m_failedAttemptsToConstructStaticDefence = 0;

	importance_this_game = 1.0f + (rand()%5)/20.0f;

	m_ownBuildingsOfCategory.resize(AAIUnitCategory::numberOfUnitCategories, 0);
}

void AAISector::LoadDataFromFile(FILE* file)
{
	if(file != nullptr)
	{
		fscanf(file, "%f %f %f", &m_flatTilesRatio, &m_waterTilesRatio, &importance_learned);
			
		if(importance_learned < 1.0f)
			importance_learned += (rand()%5)/20.0f;

		m_attacksByTargetTypeInPreviousGames.LoadFromFile(file);
	}
	else // no learning data available -> init with default data
	{
		importance_learned = 1.0f + (rand()%5)/20.0f;
		m_flatTilesRatio  = DetermineFlatRatio();
		m_waterTilesRatio = DetermineWaterRatio();
	}

	importance_this_game = importance_learned;
}

void AAISector::SaveDataToFile(FILE* file)
{
	fprintf(file, "%f %f %f ", m_flatTilesRatio, m_waterTilesRatio, importance_this_game);

	m_attacksByTargetTypeInPreviousGames.SaveToFile(file);
}

void AAISector::UpdateLearnedData()
{
	importance_this_game = 0.93f * (importance_this_game + 3.0f * importance_learned) / 4.0f;

	if(importance_this_game < 1.0f)
		importance_this_game = 1.0f;

	m_attacksByTargetTypeInPreviousGames.MultiplyValues(3.0f);
	m_attacksByTargetTypeInPreviousGames.AddMobileTargetValues(m_attacksByTargetTypeInCurrentGame);
	m_attacksByTargetTypeInPreviousGames.MultiplyValues(0.225f); // 0.225f = 0.9f / 4.0f ->decrease by 0.9 and account for 3.0f in line above
}

bool AAISector::AddToBase(bool addToBase)
{
	if(addToBase)
	{
		// check if already occupied (may happen if two coms start in same sector)
		if(AAIMap::s_teamSectorMap.IsSectorOccupied(x,y))
		{
			ai->Log("\nTeam %i could not add sector %i,%i to base, already occupied by ally team %i!\n\n",ai->GetAICallback()->GetMyAllyTeam(), x, y, AAIMap::s_teamSectorMap.GetTeam(x, y));
			return false;
		}

		m_distanceToBase = 0;

		importance_this_game += 1;

		AAIMap::s_teamSectorMap.SetSectorAsOccupiedByTeam(x, y, ai->GetMyTeamId());

		if(importance_this_game > cfg->MAX_SECTOR_IMPORTANCE)
			importance_this_game = cfg->MAX_SECTOR_IMPORTANCE;

		return true;
	}
	else	// remove from base
	{
		m_distanceToBase = 1;

		AAIMap::s_teamSectorMap.SetSectorAsUnoccupied(x, y);

		return true;
	}
}

void AAISector::ResetLocalCombatPower() 
{
	m_alliedBuildings = 0;
	m_friendlyStaticCombatPower.Reset();
	m_friendlyMobileCombatPower.Reset();
}

void AAISector::ResetScoutedEnemiesData() 
{ 
	m_enemyBuildings = 0;
	m_enemyCombatUnits.Fill(0.0f);
	m_enemyStaticCombatPower.Reset();
	m_enemyMobileCombatPower.Reset();
};

void AAISector::AddFriendlyUnitData(UnitDefId unitDefId, bool unitBelongsToAlly)
{
	const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(unitDefId);

	// add building to sector (and update stat_combat_power if it's a stat defence)
	if(category.IsBuilding())
	{
		if(unitBelongsToAlly)
			++m_alliedBuildings;

		if(category.IsStaticDefence())
			m_friendlyStaticCombatPower.AddCombatPower( ai->s_buildTree.GetCombatPower(unitDefId) );

		if(category.IsCombatUnit())
			m_friendlyMobileCombatPower.AddCombatPower( ai->s_buildTree.GetCombatPower(unitDefId) );
	}
}

void AAISector::AddScoutedEnemyUnit(UnitDefId enemyDefId, int framesSinceLastUpdate)
{
	const AAIUnitCategory& categoryOfEnemyUnit = ai->s_buildTree.GetUnitCategory(enemyDefId);
	// add building to sector (and update stat_combat_power if it's a stat defence)
	if(categoryOfEnemyUnit.IsBuilding())
	{
		++m_enemyBuildings;

		if(categoryOfEnemyUnit.IsStaticDefence())
		{
			m_enemyStaticCombatPower.AddCombatPower( ai->s_buildTree.GetCombatPower(enemyDefId) );
			m_enemyCombatUnits.AddValue(ETargetType::STATIC, 1.0f);
		}
	}
	// add unit to sector and update mobile_combat_power
	else if(categoryOfEnemyUnit.IsCombatUnit())
	{
		// units that have been scouted long time ago matter less (1 min ~ 70%, 2 min ~ 48%, 5 min ~ 16%)
		const float lastSeen = exp(- static_cast<float>(framesSinceLastUpdate) / 5000.0f );
		const AAITargetType& targetType = ai->s_buildTree.GetTargetType(enemyDefId);

		m_enemyCombatUnits.AddValue(targetType, lastSeen);

		m_enemyMobileCombatPower.AddCombatPower( ai->s_buildTree.GetCombatPower(enemyDefId), lastSeen );
	}
}

void AAISector::DecreaseLostUnits()
{
	// decrease values (so the ai "forgets" values from time to time)...
	m_lostUnits    *= 0.985f;
	m_lostAirUnits *= 0.985f;
}

void AAISector::AddMetalSpot(AAIMetalSpot *spot)
{
	metalSpots.push_back(spot);
	m_freeMetalSpots = true;
}

void AAISector::AddExtractor(UnitId unitId, UnitDefId unitDefId, float3 pos)
{
	ai->Getmap()->ConvertPositionToFinalBuildsite(pos, ai->s_buildTree.GetFootprint(unitDefId));

	for(auto spot : metalSpots)
	{
		// only check occupied spots
		if(spot->occupied && spot->DoesSpotBelongToPosition(pos))
		{
			spot->extractorUnitId = unitId;
			spot->extractorDefId  = unitDefId;
		}
	}
}

void AAISector::FreeMetalSpot(float3 pos, UnitDefId extractorDefId)
{
	ai->Getmap()->ConvertPositionToFinalBuildsite(pos, ai->s_buildTree.GetFootprint(extractorDefId));

	// get metalspot according to position
	for(auto spot : metalSpots)
	{
		// only check occupied spots
		if(spot->occupied && spot->DoesSpotBelongToPosition(pos) )
		{
			spot->SetUnoccupied();

			m_freeMetalSpots = true;
			return;
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

float3 AAISector::GetCenter() const
{
	return float3( static_cast<float>(x * AAIMap::xSectorSize +  AAIMap::xSectorSize/2), 0.0f, static_cast<float>(y * AAIMap::ySectorSize + AAIMap::ySectorSize/2));
}

float AAISector::GetImportanceForStaticDefenceVs(AAITargetType& targetType, const GamePhase& gamePhase, float previousGames, float currentGame)
{
	if( AreFurtherStaticDefencesAllowed() )
	{
		if(m_failedAttemptsToConstructStaticDefence < 2) // do not try to build defences if last two attempts failed
		{
			const float baseProximity = (m_distanceToBase <= 1) ? 1.0f : 0.0f;

			std::vector<float> importanceVsTargetType(AAITargetType::numberOfMobileTargetTypes, 0.0f);

			importanceVsTargetType[AAITargetType::airIndex] = baseProximity +
						  (0.1f + GetLocalAttacksBy(ETargetType::AIR, previousGames, currentGame) + ai->Getbrain()->GetAttacksBy(ETargetType::AIR, gamePhase)) 
						/ (1.0f + GetFriendlyStaticDefencePower(ETargetType::AIR));

			if(m_waterTilesRatio < 0.7f)
			{
				importanceVsTargetType[AAITargetType::surfaceIndex] = baseProximity +
						  (0.1f + GetLocalAttacksBy(ETargetType::SURFACE, previousGames, currentGame) + ai->Getbrain()->GetAttacksBy(ETargetType::SURFACE, gamePhase)) 
						/ (1.0f + GetFriendlyStaticDefencePower(ETargetType::SURFACE));
			}

			if(m_waterTilesRatio > 0.3f)
			{
				importanceVsTargetType[AAITargetType::floaterIndex] = baseProximity +
						  (0.1f + GetLocalAttacksBy(ETargetType::FLOATER, previousGames, currentGame) + ai->Getbrain()->GetAttacksBy(ETargetType::FLOATER, gamePhase)) 
						/ (1.0f + GetFriendlyStaticDefencePower(ETargetType::FLOATER));

				importanceVsTargetType[AAITargetType::submergedIndex] = baseProximity +
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

				highestImportance *= static_cast<float>(2 + this->GetEdgeDistance()) * (2.0f /  static_cast<float>(m_distanceToBase+1));
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

	if( (m_distanceToBase > 0) && (GetNumberOfEnemyBuildings() > 0) )
	{
		const bool landCheckPassed  = landSectorSelectable  && (m_waterTilesRatio < 0.35f);
		const bool waterCheckPassed = waterSectorSelectable && (m_waterTilesRatio > 0.65f);

		if(landCheckPassed || waterCheckPassed)
		{
			const float dx = static_cast<float>(x - currentSector->x);
			const float dy = static_cast<float>(y - currentSector->y);
			const float dist = fastmath::apxsqrt(dx*dx + dy*dy );

			const float enemyBuildings = static_cast<float>(GetNumberOfEnemyBuildings());

			// prefer sectors with many buildings, few lost units and low defence power/short distance to current sector
			rating = GetLostUnits() * enemyBuildings / ( (1.0f + GetEnemyCombatPowerVsUnits(targetTypeOfUnits)) * (1.0f + dist) );
		}
	}

	return rating;			
}


float AAISector::GetAttackRating(const std::vector<float>& globalCombatPower, const std::vector< std::vector<float> >& continentCombatPower, const MobileTargetTypeValues& assaultGroupsOfType, float maxLostUnits) const
{
	float rating(0.0f);

	if( (m_distanceToBase > 0) && (GetNumberOfEnemyBuildings() > 0))
	{
		const float myAttackPower     =   globalCombatPower[AAITargetType::staticIndex] + continentCombatPower[m_continentId][AAITargetType::staticIndex];
		const float enemyDefencePower =   assaultGroupsOfType.GetValueOfTargetType(ETargetType::SURFACE)   * GetEnemyCombatPower(ETargetType::SURFACE)
										+ assaultGroupsOfType.GetValueOfTargetType(ETargetType::FLOATER)   * GetEnemyCombatPower(ETargetType::FLOATER)
										+ assaultGroupsOfType.GetValueOfTargetType(ETargetType::SUBMERGED) * GetEnemyCombatPower(ETargetType::SUBMERGED);

		const float lostUnitsFactor = (maxLostUnits > 1.0f) ? (2.0f - (GetLostUnits() / maxLostUnits) ) : 1.0f;

		const float enemyBuildings = static_cast<float>(GetNumberOfEnemyBuildings());

		// prefer sectors with many buildings, few lost units and low defence power/short distance to own base
		rating = lostUnitsFactor * (2.0f + enemyBuildings) * myAttackPower / ( (1.5f + enemyDefencePower) * static_cast<float>(1 + 2 * m_distanceToBase) );
	}

	return rating;			
}

float AAISector::GetRatingAsNextScoutDestination(const AAIMovementType& scoutMoveType, const float3& currentPositionOfScout)
{
	if(   (m_distanceToBase == 0) 
	   || (scoutMoveType.IsIncludedIn(m_suitableMovementTypes) == false) 
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

float AAISector::GetRatingForRallyPoint(const AAIMovementType& moveType, int continentId) const
{
	if( (continentId != AAIMap::ignoreContinentID) && (continentId != m_continentId) )
		return 0.0f;

	const float edgeDistance = static_cast<float>( GetEdgeDistance() );
	const float totalAttacks = GetLostUnits() + GetTotalAttacksInThisGame();

	float rating = std::min(totalAttacks, 5.0f)
				 + std::min(2.0f * edgeDistance, 6.0f)
				 + 3.0f * GetNumberOfBuildings(EUnitCategory::METAL_EXTRACTOR); 
	
	if( moveType.IsGround() )
	{
		rating += 3.0f * GetFlatTilesRatio();
	}
	else if( moveType.IsAir() || moveType.IsAmphibious() || moveType.IsHover())
	{
		rating += 3.0f * (GetFlatTilesRatio() + GetWaterTilesRatio());
	}
	else
	{
		rating += 3.0f * GetWaterTilesRatio();
	}

	return rating;
}

float AAISector::GetRatingAsStartSector() const
{
	if(AAIMap::s_teamSectorMap.IsSectorOccupied(x, y))
		return 0.0f;
	else
		return ( static_cast<float>(2 * GetNumberOfMetalSpots() + 1) ) * m_flatTilesRatio * m_flatTilesRatio;
}

float AAISector::GetRatingForPowerPlant(float weightPreviousGames, float weightCurrentGame) const
{
	if(m_ownBuildingsOfCategory[AAIUnitCategory(EUnitCategory::STATIC_CONSTRUCTOR).GetArrayIndex()] > 1)
		return 0.0f;
	else
	{
		const float attacks = 0.1f + weightPreviousGames * m_attacksByTargetTypeInPreviousGames.CalculateSum() + weightCurrentGame * m_attacksByTargetTypeInCurrentGame.CalculateSum();

		return  1.0f / (attacks * static_cast<float>(GetEdgeDistance()+1) );
	}
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
			&& (IsOccupiedByEnemies() == false)
			&& (IsOccupiedByAllies()  == false);	
}

float3 AAISector::DetermineRandomBuildsite(UnitDefId buildingDefId, int tries) const
{
	int xStart, xEnd, yStart, yEnd;
	DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);
	return ai->Getmap()->FindRandomBuildsite(buildingDefId, xStart, xEnd, yStart, yEnd, tries);
}

BuildSite AAISector::DetermineElevatedBuildsite(UnitDefId buildingDefId, float range) const
{
	int xStart, xEnd, yStart, yEnd;

	DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);

	return ai->Getmap()->DetermineElevatedBuildsite(buildingDefId, xStart, xEnd, yStart, yEnd, range);
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
	*xStart = x * AAIMap::xSectorSizeMap;
	*xEnd = *xStart + AAIMap::xSectorSizeMap;

	if(*xStart == 0)
		*xStart = 8;

	*yStart = y * AAIMap::ySectorSizeMap;
	*yEnd = *yStart + AAIMap::ySectorSizeMap;

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

bool AAISector::IsSupportNeededToDefenceVs(const AAITargetType& targetType) const
{
	const float enemyCombatPower    = GetEnemyCombatPower(targetType);
	const float friendlyCombatPower = GetFriendlyCombatPower(targetType);
	
	if(enemyCombatPower < 0.02f)
	{
		if(friendlyCombatPower < AAIConstants::localDefencePowerToRequestSupportThreshold)
			return true;
	}
	else
	{
		if(friendlyCombatPower < enemyCombatPower)
			return true;
	}

	return false;
}

float AAISector::GetLocalAttacksBy(const AAITargetType& targetType, float previousGames, float currentGame) const
{
	const float totalAttacks = (previousGames * m_attacksByTargetTypeInPreviousGames.GetValueOfTargetType(targetType) + currentGame * m_attacksByTargetTypeInCurrentGame.GetValueOfTargetType(targetType) );
	return  totalAttacks / (previousGames + currentGame);
}

float AAISector::GetEnemyCombatPowerVsUnits(const MobileTargetTypeValues& unitsOfTargetType) const
{
	float defencePower(0.0f);
	for(const auto& targetType : AAITargetType::m_mobileTargetTypes)
	{
		const float totalDefPower = m_enemyStaticCombatPower.GetValueOfTargetType(targetType) + m_enemyMobileCombatPower.GetValueOfTargetType(targetType);
		defencePower += unitsOfTargetType.GetValueOfTargetType(targetType) * totalDefPower;
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
	const int cliffyCells = ai->Getmap()->GetCliffyCells(x * AAIMap::xSectorSizeMap, y * AAIMap::ySectorSizeMap, AAIMap::xSectorSizeMap, AAIMap::ySectorSizeMap);
	const int totalCells  = ai->Getmap()->xSectorSizeMap * ai->Getmap()->ySectorSizeMap;
	const int flatCells   = totalCells - cliffyCells;

	return static_cast<float>(flatCells) / static_cast<float>(totalCells);
}

void AAISector::UpdateThreatValues(UnitDefId destroyedDefId, UnitDefId attackerDefId)
{
	const AAIUnitCategory& destroyedCategory = ai->s_buildTree.GetUnitCategory(destroyedDefId);
	const AAIUnitCategory& attackerCategory  = ai->s_buildTree.GetUnitCategory(attackerDefId);

	// if lost unit is a building, increase attacked_by
	if(destroyedCategory.IsBuilding())
	{
		if(attackerCategory.IsCombatUnit())
		{
			const float increment = (m_distanceToBase == 0) ? 0.5f : 1.0f;
			
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
	if(    (pos.x < static_cast<float>(  x    * AAIMap::xSectorSize)) 
		|| (pos.x > static_cast<float>( (x+1) * AAIMap::xSectorSize))
		|| (pos.z < static_cast<float>(  y    * AAIMap::ySectorSize))
		|| (pos.z > static_cast<float>( (y+1) * AAIMap::ySectorSize)) )
		return false;
	else
		return true;
}

bool AAISector::ConnectedToOcean() const
{
	if(m_waterTilesRatio < 0.2f)
		return false;

	const int xStart( x   * AAIMap::xSectorSizeMap);
	const int xEnd( (x+1) * AAIMap::xSectorSizeMap);
	const int yStart( y   * AAIMap::ySectorSizeMap);
	const int yEnd( (y+1) * AAIMap::ySectorSizeMap);

	return ai->Getmap()->IsConnectedToOcean(xStart, xEnd, yStart, yEnd);
}

float3 AAISector::DetermineUnitMovePos(AAIMovementType moveType, int continentId) const
{
	BuildMapTileType forbiddenMapTileTypes(EBuildMapTileType::OCCUPIED);
	forbiddenMapTileTypes.SetTileType(EBuildMapTileType::BLOCKED_SPACE); 

	if(moveType.IsMobileSea())
		forbiddenMapTileTypes.SetTileType(EBuildMapTileType::LAND);
	else if(moveType.IsAmphibious() || moveType.IsHover())
		forbiddenMapTileTypes.SetTileType(EBuildMapTileType::CLIFF);
	else if(moveType.IsGround())
	{
		forbiddenMapTileTypes.SetTileType(EBuildMapTileType::WATER);
		forbiddenMapTileTypes.SetTileType(EBuildMapTileType::CLIFF);
	}

	const float xPosStart = static_cast<float>(x * AAIMap::xSectorSize);
	const float yPosStart = static_cast<float>(y * AAIMap::ySectorSize);

	// try to get random spot
	for(int i = 0; i < 6; ++i)
	{
		float3 position;
		position.x = xPosStart + static_cast<float>(AAIMap::xSectorSize) * (0.1f + 0.08f * (float)(rand()%11) );
		position.z = yPosStart + static_cast<float>(AAIMap::ySectorSize) * (0.1f + 0.08f * (float)(rand()%11) );

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
			position.x = xPosStart + static_cast<float>(i * SQUARE_SIZE);
			position.z = yPosStart + static_cast<float>(j * SQUARE_SIZE);

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
		if( (continentId == AAIMap::ignoreContinentID) || (AAIMap::GetContinentID(pos) == continentId) )
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
