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
	m_metalAvailable(AAIConfig::INCOME_SAMPLE_POINTS),
	m_energyAvailable(AAIConfig::INCOME_SAMPLE_POINTS),
	m_metalIncome(AAIConfig::INCOME_SAMPLE_POINTS),
	m_energyIncome(AAIConfig::INCOME_SAMPLE_POINTS),
	m_metalSurplus(AAIConfig::INCOME_SAMPLE_POINTS),
	m_energySurplus(AAIConfig::INCOME_SAMPLE_POINTS),
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
	if(sector->GetDistanceToBase() <= 0)
		return true;
	// allow construction close to base for small bases
	else if(m_sectorsInDistToBase[0].size() < 3 && sector->GetDistanceToBase() <= 1)
		return true;
	// allow construction on islands close to base on water maps
	else if(ai->Getmap()->GetMapType().IsWater() && (ai->GetAICallback()->GetElevation(pos->x, pos->z) >= 0) && (sector->GetDistanceToBase() <= 3) )
		return true;
	else
		return false;
}

struct SectorForBaseExpansion
{
	SectorForBaseExpansion(AAISector* _sector, float _distance, float _totalAttacks) :
		sector(_sector), distance(_distance), totalAttacks(_totalAttacks) { }

	AAISector* sector;
	float      distance;
	float      totalAttacks;
};

void AAIBrain::ExpandBaseAtStartup()
{
	if(m_sectorsInDistToBase[0].size() == 0)
	{
		ai->Log("ERROR: Failed to expand initial base - no starting sector set!\n");
		return;
	}

	AAISector* sector = *m_sectorsInDistToBase[0].begin();

	const bool preferSafeSector = (sector->GetEdgeDistance() > 0) ? true : false;

	ai->Getbrain()->ExpandBase( ai->Getmap()->GetMapType(), preferSafeSector);
}

bool AAIBrain::ExpandBase(const AAIMapType& sectorType, bool preferSafeSector)
{
	if(m_sectorsInDistToBase[0].size() >= cfg->MAX_BASE_SIZE)
		return false;

	// if aai is looking for a water sector to expand into ocean, allow greater search_dist
	const bool expandLandBaseInWater = sectorType.IsWater() && (m_baseWaterRatio < 0.1f);
	const int  maxSearchDistance     = expandLandBaseInWater ? 3 : 1;

	//-----------------------------------------------------------------------------------------------------------------
	// assemble a list of potential sectors for base expansion
	//-----------------------------------------------------------------------------------------------------------------
	std::list<SectorForBaseExpansion> expansionCandidateList;
	StatisticalData sectorDistances;
	StatisticalData sectorAttacks;

	for(int distanceToBase = 1; distanceToBase <= maxSearchDistance; ++distanceToBase)
	{
		for(auto sector : m_sectorsInDistToBase[distanceToBase])
		{
			if(sector->IsSectorSuitableForBaseExpansion() )
			{
				float sectorDistance(0.0f);
				for(auto baseSector : m_sectorsInDistToBase[0]) 
				{
					const int deltaX = sector->x - baseSector->x;
					const int deltaY = sector->y - baseSector->y;
					sectorDistance += (deltaX * deltaX + deltaY * deltaY); // try squared distances, use fastmath::apxsqrt() otherwise
				}

				sectorDistances.AddValue(sectorDistance);

				const float totalAttacks = sector->GetTotalAttacksInThisGame() + sector->GetTotalAttacksInPreviousGames();
				sectorAttacks.AddValue(totalAttacks);

				expansionCandidateList.push_back( SectorForBaseExpansion(sector, sectorDistance, totalAttacks) );
			}
		}
	}

	sectorDistances.Finalize();
	sectorAttacks.Finalize();

	//-----------------------------------------------------------------------------------------------------------------
	// select best sector from the list
	//-----------------------------------------------------------------------------------------------------------------
	AAISector *selectedSector(nullptr);
	float highestRating(0.0f);

	for(auto candidate = expansionCandidateList.begin(); candidate != expansionCandidateList.end(); ++candidate)
	{
		// prefer sectors that result in more compact bases, with more metal spots, that are safer (i.e. less attacks in the past)
		float rating = static_cast<float>( candidate->sector->GetNumberOfMetalSpots() );
						+ 4.0f * sectorDistances.GetDeviationFromMax(candidate->distance);

		if(preferSafeSector)
		{
			rating += 4.0f * sectorAttacks.GetDeviationFromMax(candidate->totalAttacks);
			rating += 4.0f / static_cast<float>( candidate->sector->GetEdgeDistance() + 1 );
		}
		else
		{
			rating += std::min(static_cast<float>( candidate->sector->GetEdgeDistance() ), 4.0f);
		}

		if(sectorType.IsLand())
		{
			// prefer flat sectors
			rating += 3.0f * candidate->sector->GetFlatTilesRatio();
		}
		else if(sectorType.IsWater())
		{
			// check for continent size (to prevent AAI to expand into little ponds instead of big ocean)
			if( candidate->sector->ConnectedToOcean() )
				rating += 3.0f * candidate->sector->GetWaterTilesRatio();
		}
		else // LAND_WATER_SECTOR
			rating += 3.0f * (candidate->sector->GetFlatTilesRatio() + candidate->sector->GetWaterTilesRatio());

		if(rating > highestRating)
		{
			highestRating  = rating;
			selectedSector = candidate->sector;
		}			
	}

	//-----------------------------------------------------------------------------------------------------------------
	// assign selected sector to base
	//-----------------------------------------------------------------------------------------------------------------
	if(selectedSector)
	{
		AssignSectorToBase(selectedSector, true);
	
		std::string sectorTypeString = sectorType.IsLand() ? "land" : "water";
		ai->Log("\nAdding %s sector %i,%i to base; base size: " _STPF_, sectorTypeString.c_str(), selectedSector->x, selectedSector->y, m_sectorsInDistToBase[0].size());
		ai->Log("\nNew land : water ratio within base: %f : %f\n\n", m_baseFlatLandRatio, m_baseWaterRatio);
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

	m_metalAvailable.AddValue(cb->GetMetal());
	m_energyAvailable.AddValue(cb->GetEnergy());

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
	// Order construction of units according to determined threat/own defence capabilities
	//-----------------------------------------------------------------------------------------------------------------

	UnitSelectionCriteria unitSelectionCriteria;
	DetermineCombatUnitSelectionCriteria(unitSelectionCriteria);

	std::vector<float> factoryUtilization(ai->s_buildTree.GetNumberOfFactories(), 0.0f);
	ai->Getexecute()->DetermineFactoryUtilization(factoryUtilization, true);

	for(int i = 0; i < ai->Getexecute()->unitProductionRate; ++i)
	{
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
	if( IsRandomNumberBelow(cfg->AIRCRAFT_RATIO) && !gamePhase.IsStartingPhase())
	{
		moveType.AddMovementType(EMovementType::MOVEMENT_TYPE_AIR);
	}
	else
	{
		moveType.AddMovementType(EMovementType::MOVEMENT_TYPE_HOVER);

		int enemyBuildingsOnLand, enemyBuildingsOnSea;
		ai->Getmap()->DetermineSpottedEnemyBuildingsOnContinentType(enemyBuildingsOnLand, enemyBuildingsOnSea);

		if( (enemyBuildingsOnLand+enemyBuildingsOnSea) == 0)
		{
			enemyBuildingsOnLand = 1;
			enemyBuildingsOnSea  = 1;
		}

		const float totalBuildings = static_cast<float>(enemyBuildingsOnLand+enemyBuildingsOnSea);

		// ratio of sea units is determined: 25% water ratio on map, 75% ratio of enemy buildings on sea
		float waterUnitRatio = 0.25f * (AAIMap::s_waterTilesRatio + 3.0f * static_cast<float>(enemyBuildingsOnSea) / totalBuildings);

		if(waterUnitRatio <0.05f)
			waterUnitRatio = 0.0f;
		else if(waterUnitRatio > 0.95f)
			waterUnitRatio = 1.0f;

		if(IsRandomNumberBelow(waterUnitRatio) )
		{
			moveType.AddMovementType(EMovementType::MOVEMENT_TYPE_SEA_FLOATER);
			moveType.AddMovementType(EMovementType::MOVEMENT_TYPE_SEA_SUBMERGED);
		}
		else
		{
			moveType.AddMovementType(EMovementType::MOVEMENT_TYPE_AMPHIBIOUS);

			if(IsRandomNumberBelow(1.0f - waterUnitRatio))
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

				if(sectors[x][y].GetDistanceToBase() < 2)
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

float AAIBrain::GetAveragePowerSurplus() const
{
	const AAIUnitStatistics& unitStatistics      = ai->s_buildTree.GetUnitStatistics(ai->GetSide());
	const StatisticalData&   generatedPowerStats = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::POWER_PLANT);

	return std::max(1.0f, m_energySurplus.GetAverageValue() + 0.03f * m_energyAvailable.GetAverageValue() - 2.0f * generatedPowerStats.GetMinValue());
}

float AAIBrain::GetEnergyUrgency() const
{
	const float avgPowerSurplus = GetAveragePowerSurplus();

	if(avgPowerSurplus > AAIConstants::powerSurplusToStopPowerPlantConstructionThreshold)
		return 0.0f;	
	else 
	{
		// urgency should range from 5 (little income & suplus) towards low values when surplus is large compared to generated energy
		return (0.04f * m_energyIncome.GetAverageValue() + 5.0f) / avgPowerSurplus;
	}
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
	if(    (ai->Getut()->GetNumberOfActiveUnitsOfCategory(EUnitCategory::STORAGE) < cfg->MAX_STORAGE)
		&& (ai->Getut()->GetNumberOfFutureUnitsOfCategory(EUnitCategory::STORAGE) <= 0)
		&& (ai->Getut()->activeFactories >= cfg->MIN_FACTORIES_FOR_STORAGE) )
	{
		const float energyStorage = std::max(ai->GetAICallback()->GetEnergyStorage(), 1.0f);
		
		// urgency ranges from 0 (no energy stored) to 0.3 (storage full)
		return 0.3f * m_energyAvailable.GetAverageValue() / energyStorage;
	}
	else
		return 0.0f;
}

float AAIBrain::GetMetalStorageUrgency() const
{
	const float unusedMetalStorage = ai->GetAICallback()->GetMetalStorage() - ai->GetAICallback()->GetMetal();

	if(    (ai->Getut()->GetNumberOfActiveUnitsOfCategory(EUnitCategory::STORAGE) < cfg->MAX_STORAGE)
		&& (ai->Getut()->GetNumberOfFutureUnitsOfCategory(EUnitCategory::STORAGE) <= 0)
		&& (ai->Getut()->activeFactories >= cfg->MIN_FACTORIES_FOR_STORAGE) )
	{
		const float metalStorage = std::max(ai->GetAICallback()->GetMetalStorage(), 1.0f);

		// urgency ranges from 0 (no energy stored) to 1 (storage full)
		return m_metalAvailable.GetAverageValue() / metalStorage;
	}
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

PowerPlantSelectionCriteria AAIBrain::DeterminePowerPlantSelectionCriteria() const
{
	const float numberOfBuildingsFactor = std::tanh(0.2f * static_cast<float>(ai->Getut()->GetTotalNumberOfUnitsOfCategory(EUnitCategory::POWER_PLANT)) - 2.0f);

	// importance of buildtime ranges between 3 (no excess energy and no plants) to close to 0.25 (sufficient excess energy)
	const float urgency   = (0.04f * m_energyIncome.GetAverageValue() + 0.1f) / GetAveragePowerSurplus();
	const float buildtime = std::min(urgency + 0.25f,  1.75f - 1.25f * numberOfBuildingsFactor);

	// importance of generated power ranges from 0.25 (no power plants) to 2.25f (many power plants)
	const float generatedPower = 1.25f + numberOfBuildingsFactor;

	// cost ranges from 2 (no power plant) to 0.5 (many power plants)
	const float cost = 1.25f - 0.75f * numberOfBuildingsFactor;

	//ai->Log("Power plant selection: income %f   surplus %f   available %f", m_energyIncome.GetAverageValue(), GetAveragEnergySurplus(), 0.03f * m_energyAvailable.GetAverageValue());
	//ai->Log("-> cost %f, buildtime %f, power %f\n", cost, buildtime, generatedPower);

	return PowerPlantSelectionCriteria(cost, buildtime, generatedPower, m_energyIncome.GetAverageValue());
}

StorageSelectionCriteria AAIBrain::DetermineStorageSelectionCriteria() const
{
	const float numberOfBuildingsFactor = std::tanh(static_cast<float>(ai->Getut()->GetTotalNumberOfUnitsOfCategory(EUnitCategory::STORAGE)) - 2.0f);

	const float metalStorage = std::max(ai->GetAICallback()->GetMetalStorage(), 1.0f);
	const float usedMetalStorageCapacity = std::min(1.1f * m_metalAvailable.GetAverageValue() / metalStorage, 1.0f);

	const float energyStorage = std::max(ai->GetAICallback()->GetEnergyStorage(), 1.0f);
	const float usedEnergyStorageCapacity = m_energyAvailable.GetAverageValue() / energyStorage;

	// storedMetal/Energy ranges from 0 (no storage capacity used) to 0.5 (storage full, no storages) - 2.0 (storage full > 4 storages)
	const float storedMetal  = (1.5f  +         numberOfBuildingsFactor) * usedMetalStorageCapacity;
	const float storedEnergy = (1.25f + 0.75f * numberOfBuildingsFactor) * usedEnergyStorageCapacity;

	// cost ranges from 2.0f (no storages) to ~0.5 (> 4 storages)
	const float cost = 1.25f - 0.75f * numberOfBuildingsFactor;
	const float buildtime (cost); 

	return StorageSelectionCriteria(cost, buildtime, storedMetal, storedEnergy);
}
