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
#include "AAIThreatMap.h"

#include "LegacyCpp/IGlobalAICallback.h"
#include "LegacyCpp/UnitDef.h"

AAISector::AAISector() :
	m_sectorIndex(0, 0),
	m_distanceToBase(-1),
	m_lostUnits(),
	m_ownBuildingsOfCategory(AAIUnitCategory::numberOfUnitCategories, 0),
	m_enemyCombatUnits(0.0f),
	m_enemyBuildings(0),
	m_alliedBuildings(0),
	m_enemyUnitsDetectedBySensor(0),
	m_skippedAsScoutDestination(0),
	m_failedAttemptsToConstructStaticDefence(0)
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
	m_sectorIndex.x = x;
	m_sectorIndex.y = y;

	// determine map border distance
	const int xEdgeDist = std::min(x, AAIMap::xSectors - 1 - x);
	const int yEdgeDist = std::min(y, AAIMap::ySectors - 1 - y);

	m_minSectorDistanceToMapEdge = std::min(xEdgeDist, yEdgeDist);

	const float3 center = GetCenter();
	m_continentId = AAIMap::GetContinentID(center);

	importance_this_game = 1.0f + (rand()%5)/20.0f;
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
		if(AAIMap::s_teamSectorMap.IsSectorOccupied(m_sectorIndex))
		{
			ai->Log("\nTeam %i could not add sector %i,%i to base, already occupied by ally team %i!\n\n",ai->GetAICallback()->GetMyAllyTeam(), m_sectorIndex.x, m_sectorIndex.y, AAIMap::s_teamSectorMap.GetTeam(m_sectorIndex));
			return false;
		}

		m_distanceToBase = 0;

		importance_this_game = std::min(importance_this_game + 1.0f, AAIConstants::maxSectorImportance);

		AAIMap::s_teamSectorMap.SetSectorAsOccupiedByTeam(m_sectorIndex, ai->GetMyTeamId());

		return true;
	}
	else	// remove from base
	{
		m_distanceToBase = 1;

		AAIMap::s_teamSectorMap.SetSectorAsUnoccupied(m_sectorIndex);

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
	m_lostUnits.MultiplyValues(AAIConstants::lostUnitsMemoryFadeRate);
}

void AAISector::AddMetalSpot(AAIMetalSpot *spot)
{
	metalSpots.push_back(spot);
}

void AAISector::AddExtractor(UnitId unitId, UnitDefId unitDefId, float3 position)
{
	ai->Map()->ConvertPositionToFinalBuildsite(position, ai->s_buildTree.GetFootprint(unitDefId));

	for(auto spot : metalSpots)
	{
		// only check occupied spots
		if(spot->occupied && spot->DoesSpotBelongToPosition(position))
		{
			spot->extractorUnitId = unitId;
			spot->extractorDefId  = unitDefId;
		}
	}
}

void AAISector::FreeMetalSpot(float3 position, UnitDefId extractorDefId)
{
	ai->Map()->ConvertPositionToFinalBuildsite(position, ai->s_buildTree.GetFootprint(extractorDefId));

	// get metalspot according to position
	for(auto spot : metalSpots)
	{
		// only check occupied spots
		if(spot->occupied && spot->DoesSpotBelongToPosition(position) )
		{
			spot->SetUnoccupied();
			return;
		}	
	}
}

float3 AAISector::GetCenter() const
{
	return float3( static_cast<float>(m_sectorIndex.x * AAIMap::xSectorSize +  AAIMap::xSectorSize/2), 0.0f, static_cast<float>(m_sectorIndex.y * AAIMap::ySectorSize + AAIMap::ySectorSize/2));
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
						  (0.1f + GetLocalAttacksBy(ETargetType::AIR, previousGames, currentGame) + ai->Brain()->GetAttacksBy(ETargetType::AIR, gamePhase)) 
						/ (1.0f + GetFriendlyStaticDefencePower(ETargetType::AIR));

			if(m_waterTilesRatio < 0.7f)
			{
				importanceVsTargetType[AAITargetType::surfaceIndex] = baseProximity +
						  (0.1f + GetLocalAttacksBy(ETargetType::SURFACE, previousGames, currentGame) + ai->Brain()->GetAttacksBy(ETargetType::SURFACE, gamePhase)) 
						/ (1.0f + GetFriendlyStaticDefencePower(ETargetType::SURFACE));
			}

			if(m_waterTilesRatio > 0.3f)
			{
				importanceVsTargetType[AAITargetType::floaterIndex] = baseProximity +
						  (0.1f + GetLocalAttacksBy(ETargetType::FLOATER, previousGames, currentGame) + ai->Brain()->GetAttacksBy(ETargetType::FLOATER, gamePhase)) 
						/ (1.0f + GetFriendlyStaticDefencePower(ETargetType::FLOATER));

				importanceVsTargetType[AAITargetType::submergedIndex] = baseProximity +
						  (0.1f + GetLocalAttacksBy(ETargetType::SUBMERGED, previousGames, currentGame) + ai->Brain()->GetAttacksBy(ETargetType::SUBMERGED, gamePhase)) 
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
				const MapPos& enemyBaseCenter = ai->Map()->GetCenterOfEnemyBase();
				const MapPos& baseCenter      = ai->Brain()->GetCenterOfBase();

				const MapPos sectorCenter(m_sectorIndex.x * AAIMap::xSectorSizeMap + AAIMap::xSectorSizeMap/2, m_sectorIndex.y * AAIMap::ySectorSizeMap + AAIMap::ySectorSizeMap/2);

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
			const float dx = static_cast<float>(m_sectorIndex.x - currentSector->m_sectorIndex.x);
			const float dy = static_cast<float>(m_sectorIndex.y - currentSector->m_sectorIndex.y);
			const float dist = fastmath::apxsqrt(dx*dx + dy*dy );

			const float enemyBuildings = static_cast<float>(GetNumberOfEnemyBuildings());

			// prefer sectors with many buildings, few lost units and low defence power/short distance to current sector
			rating = GetTotalLostUnits() * enemyBuildings / ( (1.0f + GetEnemyCombatPowerVsUnits(targetTypeOfUnits)) * (1.0f + dist) );
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

		const float lostUnitsFactor = (maxLostUnits > 1.0f) ? (2.0f - (GetTotalLostUnits() / maxLostUnits) ) : 1.0f;

		const float enemyBuildings = static_cast<float>(GetNumberOfEnemyBuildings());

		// prefer sectors with many buildings, few lost units and low defence power/short distance to own base
		rating = lostUnitsFactor * (2.0f + enemyBuildings) * myAttackPower / ( (1.5f + enemyDefencePower) * static_cast<float>(1 + 2 * m_distanceToBase) );
	}

	return rating;			
}

float AAISector::GetRatingAsNextScoutDestination(const AAIMovementType& scoutMoveType, const AAITargetType& scoutTargetType, const float3& currentPositionOfScout)
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
		const float distanceToCurrentLocationFactor = 0.1f + 0.9f * (1.0f - (dx*dx+dy*dy) / AAIMap::s_maxSquaredMapDist);

		// factor between 1 and 0.4 (depending on number of recently lost units)
		//const float lostUnits =  // scoutMoveType.IsAir() ? m_lostAirUnits : m_lostUnits;
		const float lostScoutsFactor = 0.4f + 0.6f / (0.5f * m_lostUnits.GetValueOfTargetType(scoutTargetType) + 1.0f);

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
	const float totalAttacks = GetTotalLostUnits() + GetTotalAttacksInThisGame();

	float rating(0.0f);

	if(moveType.IsAir())
	{
		rating = (GetFlatTilesRatio() + GetWaterTilesRatio()) / (1.0f + std::min(totalAttacks, 3.0f));
	}
	else
	{
		rating =  std::min(totalAttacks, 6.0f)
				+ std::min(2.0f * edgeDistance, 6.0f)
				+ 3.0f * GetNumberOfBuildings(EUnitCategory::METAL_EXTRACTOR); 
	
		if( moveType.IsGround() )
		{
			rating += 3.0f * GetFlatTilesRatio();
		}
		else if( moveType.IsAmphibious() || moveType.IsHover())
		{
			rating += 3.0f * (GetFlatTilesRatio() + GetWaterTilesRatio());
		}
		else
		{
			rating += 3.0f * GetWaterTilesRatio();
		}
	}

	return rating;
}

float AAISector::GetRatingAsStartSector() const
{
	if(AAIMap::s_teamSectorMap.IsSectorOccupied(m_sectorIndex))
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
	const bool consideredToBeSafe = (m_lostUnits.CalculateSum() < 1.0f) || m_friendlyMobileCombatPower.CalculateSum() > 2.0f;

	return     (IsOccupiedByEnemies() == false)
			&& (GetNumberOfAlliedBuildings() < 3)
			&& (AAIMap::s_teamSectorMap.IsSectorOccupied(m_sectorIndex) == false)
			&& consideredToBeSafe;
}

bool AAISector::ShallBeConsideredForExtractorConstruction() const
{
	const bool consideredToBeSafe = (m_distanceToBase == 0) || (m_lostUnits.CalculateSum() < 1.0f) || m_friendlyMobileCombatPower.CalculateSum() > 2.0f;

	return 	   (AAIMap::s_teamSectorMap.IsOccupiedByOtherTeam(m_sectorIndex, ai->GetMyTeamId()) == false)
			&& (IsOccupiedByEnemies() == false)
			&& (GetNumberOfAlliedBuildings() <= 0)
			&& consideredToBeSafe;	
}

bool AAISector::AreFreeMetalSpotsAvailable() const
{
	for(const auto spot : metalSpots)
	{
		if(spot->occupied == false)
			return true;
	}

	return false;
}

BuildSite AAISector::DetermineRandomBuildsite(UnitDefId buildingDefId, int tries) const
{
	int xStart, xEnd, yStart, yEnd;
	DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);
	return ai->Map()->DetermineRandomBuildsite(buildingDefId, xStart, xEnd, yStart, yEnd, tries);
}

BuildSite AAISector::DetermineElevatedBuildsite(UnitDefId buildingDefId, float range) const
{
	int xStart, xEnd, yStart, yEnd;

	DetermineBuildsiteRectangle(&xStart, &xEnd, &yStart, &yEnd);

	return ai->Map()->DetermineElevatedBuildsite(buildingDefId, xStart, xEnd, yStart, yEnd, range);
}

float3 AAISector::DetermineAttackPosition() const
{
	if(GetNumberOfEnemyBuildings() == 0)
		return GetCenter();
	else
	{
		const int xStart =  m_sectorIndex.x      * AAIMap::xSectorSizeMap;
		const int xEnd   = (m_sectorIndex.x + 1) * AAIMap::xSectorSizeMap;
		const int yStart =  m_sectorIndex.y      * AAIMap::ySectorSizeMap;
		const int yEnd   = (m_sectorIndex.y + 1) * AAIMap::ySectorSizeMap;
		return ai->Map()->DeterminePositionOfEnemyBuildingInSector(xStart, xEnd, yStart, yEnd);
	}
}

void AAISector::DetermineBuildsiteRectangle(int *xStart, int *xEnd, int *yStart, int *yEnd) const
{
	*xStart = m_sectorIndex.x * AAIMap::xSectorSizeMap;
	*xEnd = *xStart + AAIMap::xSectorSizeMap;

	if(*xStart == 0)
		*xStart = 8;

	*yStart = m_sectorIndex.y * AAIMap::ySectorSizeMap;
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

float AAISector::DetermineWaterRatio() const
{
	int waterCells(0);

	for(int yPos = m_sectorIndex.y * AAIMap::ySectorSizeMap; yPos < (m_sectorIndex.y+1) * AAIMap::ySectorSizeMap; ++yPos)
	{
		for(int xPos = m_sectorIndex.x * AAIMap::xSectorSizeMap; xPos < (m_sectorIndex.x+1) * AAIMap::xSectorSizeMap; ++xPos)
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
	const int cliffyCells = ai->Map()->GetCliffyCells(m_sectorIndex.x * AAIMap::xSectorSizeMap, m_sectorIndex.y * AAIMap::ySectorSizeMap, AAIMap::xSectorSizeMap, AAIMap::ySectorSizeMap);
	const int totalCells  = ai->Map()->xSectorSizeMap * ai->Map()->ySectorSizeMap;
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
		const AAITargetType& targetType = ai->s_buildTree.GetTargetType(destroyedDefId);
		m_lostUnits.AddValueForTargetType(targetType, 1.0f);
	}
}

bool AAISector::PosInSector(const float3& pos) const
{
	if(    (pos.x < static_cast<float>(  m_sectorIndex.x    * AAIMap::xSectorSize)) 
		|| (pos.x > static_cast<float>( (m_sectorIndex.x+1) * AAIMap::xSectorSize))
		|| (pos.z < static_cast<float>(  m_sectorIndex.y    * AAIMap::ySectorSize))
		|| (pos.z > static_cast<float>( (m_sectorIndex.y+1) * AAIMap::ySectorSize)) )
		return false;
	else
		return true;
}

bool AAISector::ConnectedToOcean() const
{
	if(m_waterTilesRatio < 0.2f)
		return false;

	const int xStart =  m_sectorIndex.x    * AAIMap::xSectorSizeMap;
	const int xEnd   = (m_sectorIndex.x+1) * AAIMap::xSectorSizeMap;
	const int yStart =  m_sectorIndex.y    * AAIMap::ySectorSizeMap;
	const int yEnd   = (m_sectorIndex.y+1) * AAIMap::ySectorSizeMap;

	return ai->Map()->IsConnectedToOcean(xStart, xEnd, yStart, yEnd);
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

	const float xPosStart = static_cast<float>(m_sectorIndex.x * AAIMap::xSectorSize);
	const float yPosStart = static_cast<float>(m_sectorIndex.y * AAIMap::ySectorSize);

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
			&& (AAIMap::s_teamSectorMap.IsOccupiedByOtherTeam(m_sectorIndex, ai->GetMyTeamId()) == false);
}
