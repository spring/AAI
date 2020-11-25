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
	m_baseFlatLandRatio(0.0f),
	m_baseWaterRatio(0.0f),
	m_centerOfBase(0, 0),
	m_metalSurplus(AAIConfig::INCOME_SAMPLE_POINTS),
	m_energySurplus(AAIConfig::INCOME_SAMPLE_POINTS),
	m_metalIncome(AAIConfig::INCOME_SAMPLE_POINTS),
	m_energyIncome(AAIConfig::INCOME_SAMPLE_POINTS),
	m_estimatedPressureByEnemies(0.0f)
{
	this->ai = ai;

	m_sectorsInDistToBase.resize(maxSectorDistanceToBase);
}

AAIBrain::~AAIBrain(void)
{
}

void AAIBrain::InitAttackedByRates(const AttackedByRatesPerGamePhase& attackedByRates)
{
	s_attackedByRates = attackedByRates;
}

bool AAIBrain::RessourcesForConstr(int /*unit*/, int /*wokertime*/)
{
	//! @todo check metal and energy

	return true;
}

void AAIBrain::AssignSectorToBase(AAISector *sector, bool addToBase)
{
	const bool successful = sector->AddToBase(addToBase);

	if(successful)
	{
		if(addToBase)
			m_sectorsInDistToBase[0].push_back(sector);
		else
			m_sectorsInDistToBase[0].remove(sector);
	}

	// update base land/water ratio
	m_baseFlatLandRatio = 0.0f;
	m_baseWaterRatio    = 0.0f;

	if(m_sectorsInDistToBase[0].size() > 0)
	{
		//for(auto sector = m_sectorsInDistToBase[0].begin(); sector != m_sectorsInDistToBase[0].end(); ++sector)
		for(auto sector : m_sectorsInDistToBase[0])
		{
			m_baseFlatLandRatio += sector->GetFlatTilesRatio();
			m_baseWaterRatio    += sector->GetWaterTilesRatio();
		}

		m_baseFlatLandRatio /= static_cast<float>(m_sectorsInDistToBase[0].size());
		m_baseWaterRatio    /= static_cast<float>(m_sectorsInDistToBase[0].size());
	}

	ai->Getmap()->UpdateNeighbouringSectors(m_sectorsInDistToBase);

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
	m_centerOfBase.x = 0;
	m_centerOfBase.y = 0;

	if(m_sectorsInDistToBase[0].size() > 0)
	{
		for(std::list<AAISector*>::iterator sector = m_sectorsInDistToBase[0].begin(); sector != m_sectorsInDistToBase[0].end(); ++sector)
		{
			m_centerOfBase.x += (*sector)->x;
			m_centerOfBase.y += (*sector)->y;
		}

		m_centerOfBase.x *= AAIMap::xSectorSizeMap;
		m_centerOfBase.y *= AAIMap::ySectorSizeMap;

		m_centerOfBase.x /= m_sectorsInDistToBase[0].size();
		m_centerOfBase.y /= m_sectorsInDistToBase[0].size();

		m_centerOfBase.x += AAIMap::xSectorSizeMap/2;
		m_centerOfBase.y += AAIMap::ySectorSizeMap/2;
	}
}

bool AAIBrain::CommanderAllowedForConstructionAt(AAISector *sector, float3 *pos)
{
	// commander is always allowed in base
	if(sector->distance_to_base <= 0)
		return true;
	// allow construction close to base for small bases
	else if(m_sectorsInDistToBase[0].size() < 3 && sector->distance_to_base <= 1)
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

	float highestRating(0.0f);

	for(int i = 1; i <= 2; ++i)
	{
		for(auto sector : ai->Getbrain()->m_sectorsInDistToBase[i])
		{
			const float rating = sector->GetRatingForRallyPoint(moveType, continentId);
			
			if(rating > highestRating)
			{
				highestRating    = rating;
				secondBestSector = bestSector;
				bestSector       = sector;
			}
		}
	}

	// continent bound units must get a rally point on their current continent
	const int useContinentID = moveType.CannotMoveToOtherContinents() ? continentId : AAIMap::ignoreContinentID;

	if(bestSector)
	{
		rallyPoint = bestSector->DetermineUnitMovePos(moveType, useContinentID);

		if(rallyPoint.x > 0.0f)
			return true;
		else if(secondBestSector)
			rallyPoint = secondBestSector->DetermineUnitMovePos(moveType, useContinentID);

		if(rallyPoint.x > 0.0f)
			return true;
	}

	return false;
}

bool AAIBrain::ExpandBase(SectorType sectorType)
{
	if(m_sectorsInDistToBase[0].size() >= cfg->MAX_BASE_SIZE)
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
		for(auto sector = m_sectorsInDistToBase[search_dist].begin(); sector != m_sectorsInDistToBase[search_dist].end(); ++sector)
		{
			if((*sector)->IsSectorSuitableForBaseExpansion() )
			{
				float sectorDistance(0.0f);
				for(auto baseSector = m_sectorsInDistToBase[0].begin(); baseSector != m_sectorsInDistToBase[0].end(); ++baseSector) 
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
			myRating += ((candidate->first)->GetFlatTilesRatio() - (candidate->first)->GetWaterTilesRatio()) * 16.0f;
		else if(sectorType == WATER_SECTOR)
		{
			// check for continent size (to prevent aai to expand into little ponds instead of big ocean)
			if( ((candidate->first)->GetWaterTilesRatio() > 0.1f) && (candidate->first)->ConnectedToOcean() )
				myRating += 16.0f * (candidate->first)->GetWaterTilesRatio();
			else
				myRating = 0.0f;
		}
		else // LAND_WATER_SECTOR
			myRating += ((candidate->first)->GetFlatTilesRatio() + (candidate->first)->GetWaterTilesRatio()) * 16.0f;

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
			ai->Log("\nAdding land sector %i,%i to base; base size: " _STPF_, selectedSector->x, selectedSector->y, m_sectorsInDistToBase[0].size());
			ai->Log("\nNew land : water ratio within base: %f : %f\n\n", m_baseFlatLandRatio, m_baseWaterRatio);
		}
		else
		{
			ai->Log("\nAdding water sector %i,%i to base; base size: " _STPF_, selectedSector->x, selectedSector->y, m_sectorsInDistToBase[0].size());
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

void AAIBrain::UpdateMaxCombatUnitsSpotted(const MobileTargetTypeValues& spottedCombatUnits)
{
	m_maxSpottedCombatUnitsOfTargetType.MultiplyValues(0.996f);

	for(const auto& targetType : AAITargetType::m_mobileTargetTypes)
	{
		// check for new max values
		const float value = spottedCombatUnits.GetValueOfTargetType(targetType);

		if(value > m_maxSpottedCombatUnitsOfTargetType.GetValueOfTargetType(targetType))
			m_maxSpottedCombatUnitsOfTargetType.SetValueForTargetType(targetType, value);
	}
}

void AAIBrain::UpdateAttackedByValues()
{
	m_recentlyAttackedByRates.MultiplyValues(0.96f);
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
		for(auto group = ai->GetUnitGroupsList(*category).begin(); group != ai->GetUnitGroupsList(*category).end(); ++group)
		{
			if((*group)->GetUnitTypeOfGroup().IsAssaultUnit())
			{
				switch((*group)->GetUnitCategoryOfGroup().GetUnitCategory())
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
}

void AAIBrain::AddDefenceCapabilities(UnitDefId unitDefId)
{
	const TargetTypeValues& combatPower = ai->s_buildTree.GetCombatPower(unitDefId);

	if(ai->s_buildTree.GetUnitType(unitDefId).IsAssaultUnit())
	{
		const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(unitDefId);

		switch (category.GetUnitCategory())
		{
			case EUnitCategory::GROUND_COMBAT:
			{
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SURFACE, combatPower.GetValue(ETargetType::SURFACE));
				break;
			}
			case EUnitCategory::HOVER_COMBAT:
			{
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SURFACE, combatPower.GetValue(ETargetType::SURFACE));
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::FLOATER, combatPower.GetValue(ETargetType::FLOATER));
				break;
			}
			case EUnitCategory::SEA_COMBAT:
			{
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SURFACE,   combatPower.GetValue(ETargetType::SURFACE));
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::FLOATER,   combatPower.GetValue(ETargetType::FLOATER));
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SUBMERGED, combatPower.GetValue(ETargetType::SUBMERGED));
				break;
			}
			case EUnitCategory::SUBMARINE_COMBAT:
			{
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::FLOATER,   combatPower.GetValue(ETargetType::FLOATER));
				m_totalMobileCombatPower.AddValueForTargetType(ETargetType::SUBMERGED, combatPower.GetValue(ETargetType::SUBMERGED));
			}
			default:
				break;
		}
	}
	else if(ai->s_buildTree.GetUnitType(unitDefId).IsAntiAir())
		m_totalMobileCombatPower.AddValueForTargetType(ETargetType::AIR, combatPower.GetValue(ETargetType::AIR));
}

float AAIBrain::Affordable()
{
	return 25.0f / (ai->GetAICallback()->GetMetalIncome() + 5.0f);
}

void AAIBrain::BuildUnits()
{
	const GamePhase gamePhase(ai->GetAICallback()->GetCurrentFrame());

	//-----------------------------------------------------------------------------------------------------------------
	// Calculate threat by and defence vs. the different combat categories
	//-----------------------------------------------------------------------------------------------------------------
	MobileTargetTypeValues attackedByCategory;
	StatisticalData attackedByCatStatistics;
	StatisticalData unitsSpottedStatistics;
	StatisticalData defenceStatistics;

	for(const auto& targetType : AAITargetType::m_mobileTargetTypes)
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
	TargetTypeValues threatByTargetType;

	for(const auto& targetType : AAITargetType::m_mobileTargetTypes)
	{
		const float threat =  attackedByCatStatistics.GetNormalizedDeviationFromMin( attackedByCategory.GetValueOfTargetType(targetType) ) 
	                    	+ unitsSpottedStatistics.GetNormalizedDeviationFromMin( m_maxSpottedCombatUnitsOfTargetType.GetValueOfTargetType(targetType) )
	                    	+ 1.5f * defenceStatistics.GetNormalizedDeviationFromMax( m_totalMobileCombatPower.GetValueOfTargetType(targetType)) ;
		threatByTargetType.SetValue(targetType, threat);
	}						 

	threatByTargetType.SetValue(ETargetType::STATIC, threatByTargetType.GetValue(ETargetType::SURFACE) + threatByTargetType.GetValue(ETargetType::FLOATER) );

	//-----------------------------------------------------------------------------------------------------------------
	// Order building of units according to determined threat/own defence capabilities
	//-----------------------------------------------------------------------------------------------------------------

	UnitSelectionCriteria unitSelectionCriteria;
	DetermineCombatUnitSelectionCriteria(unitSelectionCriteria);

	std::vector<float> factoryUtilization(ai->s_buildTree.GetNumberOfFactories(), 0.0f);
	ai->Getexecute()->DetermineFactoryUtilization(factoryUtilization, true);

	for(int i = 0; i < ai->Getexecute()->unitProductionRate; ++i)
	{
		//const AAITargetType targetType = DetermineTargetTypeForCombatUnitConstruction(gamePhase);
		const AAIMovementType moveType = DetermineMovementTypeForCombatUnitConstruction(gamePhase);
		const bool urgent(false);
	
		BuildCombatUnitOfCategory(moveType, threatByTargetType, unitSelectionCriteria, factoryUtilization, urgent);
	}
}

bool IsRandomNumberBelow(float threshold)
{
	// determine random float in [0:1]
	const float randomValue = 0.01f * static_cast<float>(std::rand()%101);
	return randomValue < threshold;
}

AAIMovementType AAIBrain::DetermineMovementTypeForCombatUnitConstruction(const GamePhase& gamePhase) const
{
	AAIMovementType moveType;

	const AAIMapType& mapType = ai->Getmap()->GetMapType();

	if( IsRandomNumberBelow(cfg->AIRCRAFT_RATIO) && !gamePhase.IsStartingPhase())
	{
		moveType.AddMovementType(EMovementType::MOVEMENT_TYPE_AIR);
	}
	else
	{
		moveType.AddMovementType(EMovementType::MOVEMENT_TYPE_HOVER);

		const float waterRatio = (AAIMap::s_waterTilesRatio > 0.15f) ? AAIMap::s_waterTilesRatio : 0.0f;

		if(IsRandomNumberBelow(waterRatio) )
		{
			moveType.AddMovementType(EMovementType::MOVEMENT_TYPE_SEA_FLOATER);
			moveType.AddMovementType(EMovementType::MOVEMENT_TYPE_SEA_SUBMERGED);
		}
		else
		{
			moveType.AddMovementType(EMovementType::MOVEMENT_TYPE_AMPHIBIOUS);

			if(IsRandomNumberBelow(1.0f - waterRatio))
				moveType.AddMovementType(EMovementType::MOVEMENT_TYPE_GROUND);
		}
	}
	
	return moveType;
}

void AAIBrain::BuildCombatUnitOfCategory(const AAIMovementType& moveType, const TargetTypeValues& combatPowerCriteria, const UnitSelectionCriteria& unitSelectionCriteria, const std::vector<float>& factoryUtilization, bool urgent)
{
	// Select unit according to determined criteria
	const UnitDefId unitDefId = ai->Getbt()->SelectCombatUnit(ai->GetSide(), moveType, combatPowerCriteria, unitSelectionCriteria, factoryUtilization, 6);

	// Order construction of selected unit
	if(unitDefId.IsValid())
	{
		const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(unitDefId);
		const StatisticalData& costStatistics = ai->s_buildTree.GetUnitStatistics(ai->GetSide()).GetUnitCostStatistics(category);

		int numberOfUnits(1);

		if(ai->s_buildTree.GetTotalCost(unitDefId) < cfg->MAX_COST_LIGHT_ASSAULT * costStatistics.GetMaxValue())
			numberOfUnits = 3;
		else if(ai->s_buildTree.GetTotalCost(unitDefId) < cfg->MAX_COST_MEDIUM_ASSAULT * costStatistics.GetMaxValue())
			numberOfUnits = 2;
		
		ai->Getexecute()->AddUnitToBuildqueue(unitDefId, numberOfUnits, BuildQueuePosition::END);
	}
}

void AAIBrain::DetermineCombatUnitSelectionCriteria(UnitSelectionCriteria& unitSelectionCriteria) const
{
	unitSelectionCriteria.range      = 0.25f;
	unitSelectionCriteria.cost       = 0.5f;
	unitSelectionCriteria.power      = 1.0f;
	unitSelectionCriteria.efficiency = 1.0f;
	unitSelectionCriteria.factoryUtilization = 2.0f;

	// prefer faster units from time to time if enemy pressure
	if( (m_estimatedPressureByEnemies < 0.25f) && IsRandomNumberBelow(cfg->FAST_UNITS_RATIO) )
	{
		if(rand()%100 < 70)
			unitSelectionCriteria.speed = 1.0f;
		else
			unitSelectionCriteria.speed = 2.0f;
	}
	else
		unitSelectionCriteria.speed      = 0.1f + (1.0f - m_estimatedPressureByEnemies) * 0.3f;

	const GamePhase gamePhase(ai->GetAICallback()->GetCurrentFrame());

	// prefer cheaper but effective units in the first few minutes
	if(gamePhase.IsStartingPhase())
	{
		unitSelectionCriteria.speed      = 0.25f;
		unitSelectionCriteria.cost       = 2.0f;
		unitSelectionCriteria.efficiency = 2.0f;
	}
	else if(gamePhase.IsEarlyPhase())
	{
		unitSelectionCriteria.cost       = 1.0f;
		unitSelectionCriteria.efficiency = 1.5f;
	}
	else
	{
		// determine speed, range & eff
		if( IsRandomNumberBelow(cfg->HIGH_RANGE_UNITS_RATIO) )
		{
			const int t = rand()%1000;

			if(t < 350)
				unitSelectionCriteria.range = 0.75f;
			else if(t < 700)
				unitSelectionCriteria.range = 1.2f;
			else
				unitSelectionCriteria.range = 1.5f;
		}

		if( IsRandomNumberBelow(0.25f) )
			unitSelectionCriteria.power = 2.5f;
		else
			unitSelectionCriteria.power = 1.0f + (1.0f - m_estimatedPressureByEnemies) * 0.5f;

		unitSelectionCriteria.cost = 0.5f + m_estimatedPressureByEnemies * 1.0f;
	}
}

float AAIBrain::GetAttacksBy(const AAITargetType& targetType, const GamePhase& gamePhase) const
{
	return (  0.3f * s_attackedByRates.GetAttackedByRate(gamePhase, targetType) 
	        + 0.7f * m_recentlyAttackedByRates.GetValueOfTargetType(targetType) );
}

void AAIBrain::UpdatePressureByEnemy()
{
	int sectorsOccupiedByEnemies(0);
	int sectorsNearBaseOccupiedByEnemies(0);

	const auto sectors = ai->Getmap()->m_sector;

	for(int x = 0; x < AAIMap::xSectors; ++x)
	{
		for(int y = 0; y < AAIMap::ySectors; ++y)
		{
			if(sectors[x][y].IsOccupiedByEnemies())
			{
				++sectorsOccupiedByEnemies;

				if(sectors[x][y].distance_to_base < 2)
					++sectorsNearBaseOccupiedByEnemies;
			}
		}
	}

	const float sectorsWithEnemiesRatio         = static_cast<float>(sectorsOccupiedByEnemies)         / static_cast<float>(AAIMap::xSectors * AAIMap::ySectors);
	const float sectorsNearBaseWithEnemiesRatio = static_cast<float>(sectorsNearBaseOccupiedByEnemies) / static_cast<float>( m_sectorsInDistToBase[0].size() + m_sectorsInDistToBase[1].size() );

	m_estimatedPressureByEnemies = sectorsWithEnemiesRatio + 2.0f * sectorsNearBaseWithEnemiesRatio;

	if(m_estimatedPressureByEnemies > 1.0f)
		m_estimatedPressureByEnemies = 1.0f;

	//ai->Log("Current enemy pressure: %f  - map: %f    near base: %f \n", m_estimatedPressureByEnemies, sectorsWithEnemiesRatio, sectorsNearBaseWithEnemiesRatio);
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

bool AAIBrain::SufficientResourcesToAssistsConstructionOf(UnitDefId defId) const
{
	const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(defId);

	if(  category.IsMetalExtractor() || category.IsPowerPlant() )
		return true;
	else if(   (m_metalSurplus.GetAverageValue()  > AAIConstants::minMetalSurplusForConstructionAssist)
			&& (m_energySurplus.GetAverageValue() > AAIConstants::minEnergySurplusForConstructionAssist) )
	{
		return true;
	}

	return false;
}

float AAIBrain::DetermineConstructionUrgencyOfFactory(UnitDefId factoryDefId) const
{
	const StatisticalData& costs  = ai->s_buildTree.GetUnitStatistics(ai->GetSide()).GetUnitCostStatistics(EUnitCategory::STATIC_CONSTRUCTOR);
	
	
	float rating =    ai->Getbt()->DetermineFactoryRating(factoryDefId)
					+ costs.GetDeviationFromMax( ai->s_buildTree.GetTotalCost(factoryDefId) );
					+ static_cast<float>(ai->Getbt()->GetDynamicUnitTypeData(factoryDefId).active + 1);

	const AAIMovementType& moveType = ai->s_buildTree.GetMovementType(factoryDefId);

	if(moveType.IsSea())
		rating *= (0.3f + 0.35f * (AAIMap::s_waterTilesRatio + m_baseWaterRatio) );
	else if(moveType.IsGround() || moveType.IsStaticLand())
		rating *= (0.3f + 0.35f * (AAIMap::s_landTilesRatio  + m_baseFlatLandRatio) );

	return rating;
}
