// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "AAI.h"
#include "AAIBrain.h"
#include "AAIBuildTable.h"
#include "AAIExecute.h"
#include "AAIUnitTable.h"
#include "AAIConfig.h"
#include "AAIMap.h"
#include "AAIGroup.h"
#include "AAISector.h"

#include <unordered_map>

#include "LegacyCpp/UnitDef.h"
using namespace springLegacyAI;

AttackedByRatesPerGamePhase AAIBrain::s_attackedByRates;

AAIBrain::AAIBrain(AAI *ai, int maxSectorDistanceToBase) :
	m_freeMetalSpotsInBase(false),
	m_baseFlatLandRatio(0.0f),
	m_baseWaterRatio(0.0f),
	m_centerOfBase(ZeroVector),
	m_metalSurplus(AAIConfig::INCOME_SAMPLE_POINTS),
	m_energySurplus(AAIConfig::INCOME_SAMPLE_POINTS),
	m_metalIncome(AAIConfig::INCOME_SAMPLE_POINTS),
	m_energyIncome(AAIConfig::INCOME_SAMPLE_POINTS)
{
	this->ai = ai;

	sectors.resize(maxSectorDistanceToBase);

	enemy_pressure_estimation = 0;
}

AAIBrain::~AAIBrain(void)
{
}

void AAIBrain::InitAttackedByRates(const AttackedByRatesPerGamePhase& attackedByRates)
{
	s_attackedByRates = attackedByRates;
}

void AAIBrain::GetNewScoutDest(float3 *dest, int scout)
{
	*dest = ZeroVector;

	// TODO: take scouts pos into account
	float my_rating, best_rating = 0;
	AAISector *scout_sector = 0, *sector;

	const UnitDef *def = ai->GetAICallback()->GetUnitDef(scout);
	const AAIMovementType& scoutMoveType = ai->s_buildTree.GetMovementType( UnitDefId(def->id) );

	float3 pos = ai->GetAICallback()->GetUnitPos(scout);

	// get continent
	int continent = ai->Getmap()->getSmartContinentID(&pos, scoutMoveType);

	for(int x = 0; x < ai->Getmap()->xSectors; ++x)
	{
		for(int y = 0; y < ai->Getmap()->ySectors; ++y)
		{
			sector = &ai->Getmap()->sector[x][y];

			if(    (sector->distance_to_base > 0) 
			    && (scoutMoveType.isIncludedIn(sector->m_suitableMovementTypes) == true) )
			{
				if(enemy_pressure_estimation > 0.01f && sector->distance_to_base < 2)
					my_rating = sector->importance_this_game * sector->last_scout * (1.0f + sector->GetTotalEnemyCombatUnits());
				else
					my_rating = sector->importance_this_game * sector->last_scout;

				++sector->last_scout;

				if(my_rating > best_rating)
				{
					// possible scout dest, try to find pos in sector
					bool scoutDestFound = sector->DetermineMovePosOnContinent(&pos, continent);

					if(scoutDestFound == true)
					{
						best_rating = my_rating;
						scout_sector = sector;
						*dest = pos;
					}
				}
			}
		}
	}

	// set dest sector as visited
	if(dest->x > 0)
		scout_sector->last_scout = 1;
}

bool AAIBrain::RessourcesForConstr(int /*unit*/, int /*wokertime*/)
{
	//! @todo check metal and energy

	return true;
}

void AAIBrain::AssignSectorToBase(AAISector *sector, bool addToBase)
{
	if(addToBase)
	{
		sectors[0].push_back(sector);
		sector->SetBase(true);
	}
	else
	{
		sectors[0].remove(sector);
		sector->SetBase(false);
	}

	// update base land/water ratio
	m_baseFlatLandRatio = 0.0f;
	m_baseWaterRatio    = 0.0f;

	if(sectors[0].size() > 0)
	{
		for(list<AAISector*>::iterator s = sectors[0].begin(); s != sectors[0].end(); ++s)
		{
			m_baseFlatLandRatio += (*s)->DetermineFlatRatio();
			m_baseWaterRatio += (*s)->DetermineWaterRatio();
		}

		m_baseFlatLandRatio /= (float)sectors[0].size();
		m_baseWaterRatio /= (float)sectors[0].size();
	}

	UpdateNeighbouringSectors();

	UpdateCenterOfBase();
}

void AAIBrain::DefendCommander(int /*attacker*/)
{
//	float3 pos = ai->Getcb()->GetUnitPos(ai->Getut()->cmdr);
	//float importance = 120;
	Command c;

	// evacuate cmdr
	// TODO: FIXME: check/fix?/implement me?
	/*if(ai->cmdr->task != BUILDING)
	{
		AAISector *sector = GetSafestSector();

		if(sector != 0)
		{
			pos = sector->GetCenter();

			if(pos.x > 0 && pos.z > 0)
			{
				pos.y = ai->Getcb()->GetElevation(pos.x, pos.z);
				ai->Getexecute()->MoveUnitTo(ai->cmdr->unit_id, &pos);
			}
		}
	}*/
}

void AAIBrain::UpdateCenterOfBase()
{
	m_centerOfBase = ZeroVector;

	if(sectors[0].size() > 0)
	{
		for(std::list<AAISector*>::iterator sector = sectors[0].begin(); sector != sectors[0].end(); ++sector)
		{
			m_centerOfBase.x += (0.5f + static_cast<float>( (*sector)->x) ) * static_cast<float>(ai->Getmap()->xSectorSize);
			m_centerOfBase.z += (0.5f + static_cast<float>( (*sector)->y) ) * static_cast<float>(ai->Getmap()->ySectorSize);
		}

		m_centerOfBase.x /= static_cast<float>(sectors[0].size());
		m_centerOfBase.z /= static_cast<float>(sectors[0].size());
	}
}

void AAIBrain::UpdateNeighbouringSectors()
{
	int x,y,neighbours;

	// delete old values
	for(x = 0; x < ai->Getmap()->xSectors; ++x)
	{
		for(y = 0; y < ai->Getmap()->ySectors; ++y)
		{
			if(ai->Getmap()->sector[x][y].distance_to_base > 0)
				ai->Getmap()->sector[x][y].distance_to_base = -1;
		}
	}

	for(int i = 1; i < sectors.size(); ++i)
	{
		// delete old sectors
		sectors[i].clear();
		neighbours = 0;

		for(list<AAISector*>::iterator sector = sectors[i-1].begin(); sector != sectors[i-1].end(); ++sector)
		{
			x = (*sector)->x;
			y = (*sector)->y;

			// check left neighbour
			if(x > 0 && ai->Getmap()->sector[x-1][y].distance_to_base == -1)
			{
				ai->Getmap()->sector[x-1][y].distance_to_base = i;
				sectors[i].push_back(&ai->Getmap()->sector[x-1][y]);
				++neighbours;
			}
			// check right neighbour
			if(x < (ai->Getmap()->xSectors - 1) && ai->Getmap()->sector[x+1][y].distance_to_base == -1)
			{
				ai->Getmap()->sector[x+1][y].distance_to_base = i;
				sectors[i].push_back(&ai->Getmap()->sector[x+1][y]);
				++neighbours;
			}
			// check upper neighbour
			if(y > 0 && ai->Getmap()->sector[x][y-1].distance_to_base == -1)
			{
				ai->Getmap()->sector[x][y-1].distance_to_base = i;
				sectors[i].push_back(&ai->Getmap()->sector[x][y-1]);
				++neighbours;
			}
			// check lower neighbour
			if(y < (ai->Getmap()->ySectors - 1) && ai->Getmap()->sector[x][y+1].distance_to_base == -1)
			{
				ai->Getmap()->sector[x][y+1].distance_to_base = i;
				sectors[i].push_back(&ai->Getmap()->sector[x][y+1]);
				++neighbours;
			}

			if(i == 1 && !neighbours)
				(*sector)->interior = true;
		}
	}

	//ai->Log("Base has now %i direct neighbouring sectors\n", sectors[1].size());
}

bool AAIBrain::CommanderAllowedForConstructionAt(AAISector *sector, float3 *pos)
{
	// commander is always allowed in base
	if(sector->distance_to_base <= 0)
		return true;
	// allow construction close to base for small bases
	else if(sectors[0].size() < 3 && sector->distance_to_base <= 1)
		return true;
	// allow construction on islands close to base on water maps
	else if(ai->Getmap()->GetMapType().IsWaterMap() && (ai->GetAICallback()->GetElevation(pos->x, pos->z) >= 0) && (sector->distance_to_base <= 3) )
		return true;
	else
		return false;
}

bool AAIBrain::DetermineRallyPoint(float3& rallyPoint, const AAIMovementType& moveType, int continentId)
{
	AAISector* bestSector(nullptr);
	AAISector* secondBestSector(nullptr);

	float bestRating(0.0f);

	for(int i = 1; i <= 2; ++i)
	{
		for(std::list<AAISector*>::iterator sector = ai->Getbrain()->sectors[i].begin(); sector != ai->Getbrain()->sectors[i].end(); ++sector)
		{
			const float edgeDistance = static_cast<float>( (*sector)->GetEdgeDistance() );
			const float totalAttacks = (*sector)->GetLostUnits() + (*sector)->GetTotalAttacksInThisGame();

			float myRating = std::min(totalAttacks, 5.0f)
			               + std::min(2.0f* edgeDistance, 6.0f)
			               + 3.0f * (*sector)->GetNumberOfBuildings(EUnitCategory::METAL_EXTRACTOR)
						   + 4.0f / (2.0f + static_cast<float>( (*sector)->rally_points ) ); 
			
			if( moveType.IsGround() )
			{
				myRating += 3.0f * (*sector)->flat_ratio;
			}
			else if( moveType.IsAir() || moveType.IsAmphibious() || moveType.IsHover())
			{
				myRating += 3.0f * ((*sector)->flat_ratio + (*sector)->water_ratio);
			}
			else
			{
				myRating += 3.0f * (*sector)->water_ratio;
			}
			
			if(myRating > bestRating)
			{
				bestRating       = myRating;
				secondBestSector = bestSector;
				bestSector       = *sector;
			}
		}
	}

	// continent bound units must get a rally point on their current continent
	const bool continentBound = moveType.CannotMoveToOtherContinents();
	bool rallyPointFound(false);

	if(bestSector)
		rallyPointFound = continentBound ? bestSector->DetermineMovePosOnContinent(&rallyPoint, continentId) : bestSector->DetermineMovePos(&rallyPoint);

	if(!rallyPointFound && secondBestSector)
		rallyPointFound = continentBound ? secondBestSector->DetermineMovePosOnContinent(&rallyPoint, continentId) : secondBestSector->DetermineMovePos(&rallyPoint);

	return rallyPointFound;
}

bool AAIBrain::ExpandBase(SectorType sectorType)
{
	if(sectors[0].size() >= cfg->MAX_BASE_SIZE)
		return false;

	// now targets should contain all neighbouring sectors that are not currently part of the base
	// only once; select the sector with most metalspots and least danger
	int max_search_dist = 1;

	// if aai is looking for a water sector to expand into ocean, allow greater search_dist
	if(sectorType == WATER_SECTOR &&  m_baseWaterRatio < 0.1)
		max_search_dist = 3;

	std::list< std::pair<AAISector*, float> > expansionCandidateList;
	StatisticalData sectorDistances;

	for(int search_dist = 1; search_dist <= max_search_dist; ++search_dist)
	{
		for(std::list<AAISector*>::iterator sector = sectors[search_dist].begin(); sector != sectors[search_dist].end(); ++sector)
		{
			// dont expand if enemy structures in sector && check for allied buildings
			if(!(*sector)->IsOccupiedByEnemies() && (*sector)->GetNumberOfAlliedBuildings() < 3 && !ai->Getmap()->IsAlreadyOccupiedByOtherAAI(*sector))
			{
				float sectorDistance(0.0f);
				for(list<AAISector*>::iterator baseSector = sectors[0].begin(); baseSector != sectors[0].end(); ++baseSector) 
				{
					const int deltaX = (*sector)->x - (*baseSector)->x;
					const int deltaY = (*sector)->y - (*baseSector)->y;
					sectorDistance += (deltaX * deltaX + deltaY * deltaY); // try squared distances, use fastmath::apxsqrt() otherwise
				}
				expansionCandidateList.push_back( std::pair<AAISector*, float>(*sector, sectorDistance) );

				sectorDistances.AddValue(sectorDistance);
			}
		}
	}

	sectorDistances.Finalize();

	AAISector *selectedSector(nullptr);
	float bestRating(0.0f);

	for(auto candidate = expansionCandidateList.begin(); candidate != expansionCandidateList.end(); ++candidate)
	{
		// sectors that result in more compact bases or with more metal spots are rated higher
		float myRating = static_cast<float>( (candidate->first)->GetNumberOfMetalSpots() );
		                + 4.0f * sectorDistances.GetNormalizedDeviationFromMax(candidate->second)
						+ 3.0f / static_cast<float>( (candidate->first)->GetEdgeDistance() + 1 );

		if(sectorType == LAND_SECTOR)
			// prefer flat sectors without water
			myRating += ((candidate->first)->flat_ratio - (candidate->first)->water_ratio) * 16.0f;
		else if(sectorType == WATER_SECTOR)
		{
			// check for continent size (to prevent aai to expand into little ponds instead of big ocean)
			if( ((candidate->first)->water_ratio > 0.1f) && (candidate->first)->ConnectedToOcean() )
				myRating += 16.0f * (candidate->first)->water_ratio;
			else
				myRating = 0.0f;
		}
		else // LAND_WATER_SECTOR
			myRating += ((candidate->first)->flat_ratio + (candidate->first)->water_ratio) * 16.0f;

		if(myRating > bestRating)
		{
			bestRating    = myRating;
			selectedSector = candidate->first;
		}			
	}

	if(selectedSector)
	{
		// add this sector to base
		AssignSectorToBase(selectedSector, true);
	
		// debug purposes:
		if(sectorType == LAND_SECTOR)
		{
			ai->Log("\nAdding land sector %i,%i to base; base size: " _STPF_, selectedSector->x, selectedSector->y, sectors[0].size());
			ai->Log("\nNew land : water ratio within base: %f : %f\n\n", m_baseFlatLandRatio, m_baseWaterRatio);
		}
		else
		{
			ai->Log("\nAdding water sector %i,%i to base; base size: " _STPF_, selectedSector->x, selectedSector->y, sectors[0].size());
			ai->Log("\nNew land : water ratio within base: %f : %f\n\n", m_baseFlatLandRatio, m_baseWaterRatio);
		}

		return true;
	}

	return false;
}

void AAIBrain::UpdateRessources(springLegacyAI::IAICallback* cb)
{
	const float energyIncome = cb->GetEnergyIncome();
	const float metalIncome  = cb->GetMetalIncome();

	float energySurplus = energyIncome - cb->GetEnergyUsage();
	float metalSurplus  = metalIncome  - cb->GetMetalUsage();

	// cap surplus at 0
	if(energySurplus < 0.0f)
		energySurplus = 0.0f;

	if(metalSurplus < 0.0f)
		metalSurplus = 0.0f;

	m_energyIncome.AddValue(energyIncome);
	m_metalIncome.AddValue(metalIncome);

	m_energySurplus.AddValue(energySurplus);
	m_metalSurplus.AddValue(metalSurplus);
}

void AAIBrain::UpdateMaxCombatUnitsSpotted(const AAIValuesForMobileTargetTypes& spottedCombatUnits)
{
	m_maxSpottedCombatUnitsOfTargetType.DecreaseByFactor(0.996f);

	for(AAITargetType targetType(AAITargetType::first); targetType.MobileTargetTypeEnd() == false; targetType.Next())
	{
		// check for new max values
		const float value = spottedCombatUnits.GetValueOfTargetType(targetType);

		if(value > m_maxSpottedCombatUnitsOfTargetType.GetValueOfTargetType(targetType))
			m_maxSpottedCombatUnitsOfTargetType.SetValueForTargetType(targetType, value);
	}
}

void AAIBrain::UpdateAttackedByValues()
{
	m_recentlyAttackedByRates.DecreaseByFactor(0.96f);
}

void AAIBrain::AttackedBy(const AAITargetType& attackerTargetType)
{
	const GamePhase gamePhase(ai->GetAICallback()->GetCurrentFrame());

	// update counter for current game
	m_recentlyAttackedByRates.AddValueForTargetType(attackerTargetType, 1.0f);
	
	// update counter for memory dependent on playtime
	s_attackedByRates.AddAttack(gamePhase, attackerTargetType);
}

void AAIBrain::UpdateDefenceCapabilities()
{
	m_totalMobileCombatPower.Reset();

	// anti air power
	for(auto category = ai->s_buildTree.GetCombatUnitCatgegories().begin(); category != ai->s_buildTree.GetCombatUnitCatgegories().end(); ++category)
	{
		for(auto group = ai->GetGroupList()[category->GetArrayIndex()].begin(); group != ai->GetGroupList()[category->GetArrayIndex()].end(); ++group)
		{
			if((*group)->GetUnitTypeOfGroup().IsAssaultUnit())
			{
				switch((*group)->GetUnitCategoryOfGroup().getUnitCategory())
				{
					case EUnitCategory::GROUND_COMBAT:
						m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SURFACE, (*group)->GetCombatPowerVsTargetType(ETargetType::SURFACE));
						break;
					case EUnitCategory::HOVER_COMBAT:
						m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SURFACE, (*group)->GetCombatPowerVsTargetType(ETargetType::SURFACE));
						m_totalMobileCombatPower.AddValueForTargetType(ETargetType::FLOATER, (*group)->GetCombatPowerVsTargetType(ETargetType::FLOATER));
						break;
					case EUnitCategory::SEA_COMBAT:
						m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SURFACE,   (*group)->GetCombatPowerVsTargetType(ETargetType::SURFACE));
						m_totalMobileCombatPower.AddValueForTargetType(ETargetType::FLOATER,   (*group)->GetCombatPowerVsTargetType(ETargetType::FLOATER));
						m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SUBMERGED, (*group)->GetCombatPowerVsTargetType(ETargetType::SUBMERGED));
						break;
					case EUnitCategory::SUBMARINE_COMBAT:
						m_totalMobileCombatPower.AddValueForTargetType(ETargetType::FLOATER,   (*group)->GetCombatPowerVsTargetType(ETargetType::FLOATER));
						m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SUBMERGED, (*group)->GetCombatPowerVsTargetType(ETargetType::SUBMERGED));
						break;
				}	
			}
			else if((*group)->GetUnitTypeOfGroup().IsAntiAir())
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::AIR, (*group)->GetCombatPowerVsTargetType(ETargetType::AIR));
		}
	}
	
	// debug
	/*ai->Log("Defence capabilities:\n");

	for(int i = 0; i < ai->Getbt()->assault_categories.size(); ++i)
		ai->Log("%-20s %f\n" , ai->Getbt()->GetCategoryString2(ai->Getbt()->GetAssaultCategoryOfID(i)),defence_power_vs[i]);
	*/
}

void AAIBrain::AddDefenceCapabilities(UnitDefId unitDefId)
{
	const AAICombatPower& combatPower = ai->s_buildTree.GetCombatPower(unitDefId);

	if(ai->s_buildTree.GetUnitType(unitDefId).IsAssaultUnit())
	{
		const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(unitDefId);

		switch (category.getUnitCategory())
		{
			case EUnitCategory::GROUND_COMBAT:
			{
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SURFACE, combatPower.GetCombatPowerVsTargetType(ETargetType::SURFACE));
				break;
			}
			case EUnitCategory::HOVER_COMBAT:
			{
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SURFACE, combatPower.GetCombatPowerVsTargetType(ETargetType::SURFACE));
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::FLOATER, combatPower.GetCombatPowerVsTargetType(ETargetType::FLOATER));
				break;
			}
			case EUnitCategory::SEA_COMBAT:
			{
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SURFACE,   combatPower.GetCombatPowerVsTargetType(ETargetType::SURFACE));
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::FLOATER,   combatPower.GetCombatPowerVsTargetType(ETargetType::FLOATER));
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SUBMERGED, combatPower.GetCombatPowerVsTargetType(ETargetType::SUBMERGED));
				break;
			}
			case EUnitCategory::SUBMARINE_COMBAT:
			{
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::FLOATER,   combatPower.GetCombatPowerVsTargetType(ETargetType::FLOATER));
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SUBMERGED, combatPower.GetCombatPowerVsTargetType(ETargetType::SUBMERGED));
			}
			default:
				break;
		}
	}
	else if(ai->s_buildTree.GetUnitType(unitDefId).IsAntiAir())
		m_totalMobileCombatPower.AddValueForTargetType(ETargetType::AIR, combatPower.GetCombatPowerVsTargetType(ETargetType::AIR));
}

float AAIBrain::Affordable()
{
	return 25.0f /(ai->GetAICallback()->GetMetalIncome() + 5.0f);
}

void AAIBrain::BuildUnits()
{
	bool urgent = false;

	GamePhase gamePhase(ai->GetAICallback()->GetCurrentFrame());

	//-----------------------------------------------------------------------------------------------------------------
	// Calculate threat by and defence vs. the different combat categories
	//-----------------------------------------------------------------------------------------------------------------
	AAIValuesForMobileTargetTypes attackedByCategory;
	StatisticalData attackedByCatStatistics;
	StatisticalData unitsSpottedStatistics;
	StatisticalData defenceStatistics;

	for(AAITargetType targetType(AAITargetType::first); targetType.MobileTargetTypeEnd() == false; targetType.Next())
	{
		attackedByCategory.SetValueForTargetType(targetType, GetAttacksBy(targetType, gamePhase) );
		attackedByCatStatistics.AddValue( attackedByCategory.GetValueOfTargetType(targetType) );

		unitsSpottedStatistics.AddValue( m_maxSpottedCombatUnitsOfTargetType.GetValueOfTargetType(targetType) );

		defenceStatistics.AddValue(m_totalMobileCombatPower.GetValueOfTargetType(targetType));
	}

	attackedByCatStatistics.Finalize();
	unitsSpottedStatistics.Finalize();
	defenceStatistics.Finalize();

	//-----------------------------------------------------------------------------------------------------------------
	// Calculate urgency to counter each of the different combat categories
	//-----------------------------------------------------------------------------------------------------------------
	AAICombatPower threatByTargetType;

	for(AAITargetType targetType(AAITargetType::first); targetType.MobileTargetTypeEnd() == false; targetType.Next())
	{
		const float threat =  attackedByCatStatistics.GetNormalizedDeviationFromMin( attackedByCategory.GetValueOfTargetType(targetType) ) 
	                    	+ unitsSpottedStatistics.GetNormalizedDeviationFromMin( m_maxSpottedCombatUnitsOfTargetType.GetValueOfTargetType(targetType) )
	                    	+ 1.5f * defenceStatistics.GetNormalizedDeviationFromMax( m_totalMobileCombatPower.GetValueOfTargetType(targetType)) ;
		threatByTargetType.SetCombatPower(targetType, threat);
	}						 

	threatByTargetType.SetCombatPower(ETargetType::STATIC, threatByTargetType.GetCombatPowerVsTargetType(ETargetType::SURFACE) + threatByTargetType.GetCombatPowerVsTargetType(ETargetType::FLOATER) );

	//-----------------------------------------------------------------------------------------------------------------
	// Order building of units according to determined threat/own defence capabilities
	//-----------------------------------------------------------------------------------------------------------------

	const AAIMapType& mapType = ai->Getmap()->GetMapType();

	for(int i = 0; i < ai->Getexecute()->unitProductionRate; ++i)
	{
		// choose unit category dependend on map type
		if(mapType.IsLandMap())
		{
			AAICombatCategory unitCategory(EMobileTargetType::SURFACE);
		
			if( (rand()%(cfg->AIRCRAFT_RATE * 100) < 100) && !gamePhase.IsStartingPhase())
				unitCategory.setCategory(EMobileTargetType::AIR);

			BuildCombatUnitOfCategory(unitCategory, threatByTargetType, urgent);
		}
		else if(mapType.IsLandWaterMap())
		{
			//! @todo Add selection of Submarines
			int groundRatio = static_cast<int>(100.0f * ai->Getmap()->land_ratio);
			AAICombatCategory unitCategory(EMobileTargetType::SURFACE);

			if(rand()%100 < groundRatio)
				unitCategory.setCategory(EMobileTargetType::FLOATER);

			if( (rand()%(cfg->AIRCRAFT_RATE * 100) < 100) && !gamePhase.IsStartingPhase())
				unitCategory.setCategory(EMobileTargetType::AIR);
			

			BuildCombatUnitOfCategory(unitCategory, threatByTargetType, urgent);
		}
		else if(mapType.IsWaterMap())
		{
			//! @todo Add selection of Submarines
			AAICombatCategory unitCategory(EMobileTargetType::FLOATER);

			if( (rand()%(cfg->AIRCRAFT_RATE * 100) < 100) && !gamePhase.IsStartingPhase())
				unitCategory.setCategory(EMobileTargetType::AIR);

			BuildCombatUnitOfCategory(unitCategory, threatByTargetType, urgent);
		}
	}
}

void AAIBrain::BuildCombatUnitOfCategory(const AAICombatCategory& unitCategory, const AAICombatPower& combatPowerCriteria, bool urgent)
{
	UnitSelectionCriteria unitCriteria;
	unitCriteria.speed      = 0.25f;
	unitCriteria.range      = 0.25f;
	unitCriteria.cost       = 0.5f;
	unitCriteria.power      = 1.0f;
	unitCriteria.efficiency = 1.0f;

	GamePhase gamePhase(ai->GetAICallback()->GetCurrentFrame());

	// prefer cheaper but effective units in the first few minutes
	if(gamePhase.IsStartingPhase())
	{
		unitCriteria.cost       = 2.0f;
		unitCriteria.efficiency = 2.0f;
	}
	else
	{
		// determine speed, range & eff
		if(rand()%cfg->FAST_UNITS_RATE == 1)
		{
			if(rand()%2 == 1)
				unitCriteria.speed = 1.0f;
			else
				unitCriteria.speed = 2.0f;
		}

		if(rand()%cfg->HIGH_RANGE_UNITS_RATE == 1)
		{
			const int t = rand()%1000;

			if(t < 350)
				unitCriteria.range = 0.5f;
			else if(t == 700)
				unitCriteria.range = 1.0f;
			else
				unitCriteria.range = 1.5f;
		}

		if(rand()%3 == 1)
			unitCriteria.power = 2.0f;
	}

	UnitDefId unitDefId = ai->Getbt()->SelectCombatUnit(ai->GetSide(), unitCategory, combatPowerCriteria, unitCriteria, 6, false);

	if( (unitDefId.isValid() == true) && (ai->Getbt()->units_dynamic[unitDefId.id].constructorsAvailable <= 0) )
	{
		if(ai->Getbt()->units_dynamic[unitDefId.id].constructorsRequested <= 0)
			ai->Getbt()->BuildFactoryFor(unitDefId.id);

		unitDefId = ai->Getbt()->SelectCombatUnit(ai->GetSide(), unitCategory, combatPowerCriteria, unitCriteria, 6, true);
	}

	if(unitDefId.isValid() == true)
	{
		if(ai->Getbt()->units_dynamic[unitDefId.id].constructorsAvailable > 0)
		{
			const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(unitDefId);
			const StatisticalData& costStatistics = ai->s_buildTree.GetUnitStatistics(ai->GetSide()).GetUnitCostStatistics(category);

			if(ai->s_buildTree.GetTotalCost(unitDefId) < cfg->MAX_COST_LIGHT_ASSAULT * costStatistics.GetMaxValue())
			{
				if(ai->Getexecute()->AddUnitToBuildqueue(unitDefId, 3, urgent))
				{
					ai->Getbt()->units_dynamic[unitDefId.id].requested += 3;
					ai->Getut()->UnitRequested(category, 3);
				}
			}
			else if(ai->s_buildTree.GetTotalCost(unitDefId) < cfg->MAX_COST_MEDIUM_ASSAULT * costStatistics.GetMaxValue())
			{
				if(ai->Getexecute()->AddUnitToBuildqueue(unitDefId, 2, urgent))
					ai->Getbt()->units_dynamic[unitDefId.id].requested += 2;
					ai->Getut()->UnitRequested(category, 2);
			}
			else
			{
				if(ai->Getexecute()->AddUnitToBuildqueue(unitDefId, 1, urgent))
					ai->Getbt()->units_dynamic[unitDefId.id].requested += 1;
					ai->Getut()->UnitRequested(category);
			}
		}
		else if(ai->Getbt()->units_dynamic[unitDefId.id].constructorsRequested <= 0)
			ai->Getbt()->BuildFactoryFor(unitDefId.id);
	}
}

float AAIBrain::GetAttacksBy(const AAITargetType& targetType, const GamePhase& gamePhase) const
{
	return (  0.3f * s_attackedByRates.GetAttackedByRate(gamePhase, targetType) 
	        + 0.7f * m_recentlyAttackedByRates.GetValueOfTargetType(targetType) );
}

void AAIBrain::UpdatePressureByEnemy()
{
	enemy_pressure_estimation = 0;

	// check base and neighbouring sectors for enemies
	for(list<AAISector*>::iterator s = sectors[0].begin(); s != sectors[0].end(); ++s)
		enemy_pressure_estimation += 0.1f * (*s)->GetTotalEnemyCombatUnits();

	for(list<AAISector*>::iterator s = sectors[1].begin(); s != sectors[1].end(); ++s)
		enemy_pressure_estimation += 0.1f * (*s)->GetTotalEnemyCombatUnits();

	if(enemy_pressure_estimation > 1.0f)
		enemy_pressure_estimation = 1.0f;
}

float AAIBrain::GetEnergyUrgency() const
{
	if(m_energySurplus.GetAverageValue() > 2000.0f)
		return 0.0f;	
	else if(ai->Getut()->GetNumberOfActiveUnitsOfCategory(AAIUnitCategory(EUnitCategory::POWER_PLANT)) > 0)
		return 4.0f / (2.0f * m_energySurplus.GetAverageValue() / AAIConstants::energyToMetalConversionFactor + 0.5f);
	else
		return 7.0f;
}

float AAIBrain::GetMetalUrgency() const
{
	if(ai->Getut()->GetNumberOfActiveUnitsOfCategory(AAIUnitCategory(EUnitCategory::METAL_EXTRACTOR)) > 0)
		return 4.0f / (2.0f * m_metalSurplus.GetAverageValue() + 0.5f);
	else
		return 8.0f;
}

float AAIBrain::GetEnergyStorageUrgency() const
{
	const float unusedEnergyStorage = ai->GetAICallback()->GetEnergyStorage() - ai->GetAICallback()->GetEnergy();

	if(    (m_energySurplus.GetAverageValue() / AAIConstants::energyToMetalConversionFactor > 4.0f)
		&& (unusedEnergyStorage < AAIConstants::minUnusedEnergyStorageCapacityToBuildStorage)
		&& (ai->Getut()->GetNumberOfFutureUnitsOfCategory(EUnitCategory::STORAGE) <= 0) )
		return 0.15f;
	else
		return 0.0f;
}

float AAIBrain::GetMetalStorageUrgency() const
{
	const float unusedMetalStorage = ai->GetAICallback()->GetMetalStorage() - ai->GetAICallback()->GetMetal();

	if( 	(m_metalSurplus.GetAverageValue() > 3.0f)
	  	 && (unusedMetalStorage < AAIConstants::minUnusedMetalStorageCapacityToBuildStorage)
		 && (ai->Getut()->GetNumberOfFutureUnitsOfCategory(EUnitCategory::STORAGE) <= 0) )
		return 0.2f;
	else
		return 0.0f;
}

bool AAIBrain::CheckConstructionAssist(const AAIUnitCategory& category) const
{
	if(  category.isMetalExtractor() ||category.isPowerPlant() )
		return true;
	else if(   (m_metalSurplus.GetAverageValue()  > AAIConstants::minMetalSurplusForConstructionAssist)
			&& (m_energySurplus.GetAverageValue() > AAIConstants::minEnergySurplusForConstructionAssist) )
	{
		return true;
	}

	return false;
}
