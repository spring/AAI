// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------


#include "AAI.h"
#include "AAIExecute.h"
#include "AAIBrain.h"
#include "AAIUnitTable.h"
#include "AAIConstructor.h"
#include "AAIBuildTask.h"
#include "AAIConfig.h"
#include "AAIMap.h"
#include "AAIGroup.h"
#include "AAISector.h"

#include "LegacyCpp/UnitDef.h"
#include "LegacyCpp/CommandQueue.h"
using namespace springLegacyAI;


// all the static vars
float AAIExecute::current = 0.5f;
float AAIExecute::learned = 2.5f;


AAIExecute::AAIExecute(AAI *ai) :
	m_constructionUrgency(AAIUnitCategory::numberOfUnitCategories, 0.0f),
	m_constructionFunctions(AAIUnitCategory::numberOfUnitCategories, nullptr),
	m_sectorToBuildNextDefence(nullptr),	
	m_nextDefenceVsTargetType(ETargetType::UNKNOWN),
	m_linkingBuildTaskToBuilderFailed(0u)
{
	issued_orders = 0;

	this->ai = ai;

	unitProductionRate = 1;

	averageMetalUsage = 0;
	averageEnergyUsage = 0;
	disabledMMakers = 0;

	m_constructionFunctions[AAIUnitCategory(EUnitCategory::STATIC_DEFENCE).GetArrayIndex()]     = &AAIExecute::BuildDefences;
	m_constructionFunctions[AAIUnitCategory(EUnitCategory::STATIC_ARTILLERY).GetArrayIndex()]   = &AAIExecute::BuildArty;
	m_constructionFunctions[AAIUnitCategory(EUnitCategory::STORAGE).GetArrayIndex()]            = &AAIExecute::BuildStorage;
	m_constructionFunctions[AAIUnitCategory(EUnitCategory::STATIC_CONSTRUCTOR).GetArrayIndex()] = &AAIExecute::BuildStaticConstructor;
	m_constructionFunctions[AAIUnitCategory(EUnitCategory::STATIC_SENSOR).GetArrayIndex()]      = &AAIExecute::BuildRadar;
	m_constructionFunctions[AAIUnitCategory(EUnitCategory::POWER_PLANT).GetArrayIndex()]        = &AAIExecute::BuildPowerPlant;
	m_constructionFunctions[AAIUnitCategory(EUnitCategory::METAL_EXTRACTOR).GetArrayIndex()]    = &AAIExecute::BuildExtractor;
	m_constructionFunctions[AAIUnitCategory(EUnitCategory::METAL_MAKER).GetArrayIndex()]        = &AAIExecute::BuildMetalMaker;
}

AAIExecute::~AAIExecute(void)
{
}

void AAIExecute::InitAI(UnitId commanderUnitId, UnitDefId commanderDefId)
{
	//debug
	ai->Log("Playing as %s\n", cfg->sideNames[ai->GetSide()].c_str());

	if(ai->GetSide() < 1 || ai->GetSide() > cfg->numberOfSides)
	{
		ai->LogConsole("ERROR: invalid side id %i\n", ai->GetSide());
		return;
	}

	ai->Log("My team / ally team: %i / %i\n", ai->GetMyTeamId(), ai->GetAICallback()->GetMyAllyTeam());

	// tell the brain about the starting sector
	const float3 pos = ai->GetAICallback()->GetUnitPos(commanderUnitId.id);
	int x = pos.x/ai->Getmap()->xSectorSize;
	int y = pos.z/ai->Getmap()->ySectorSize;

	if(x < 0)
		x = 0;
	if(y < 0 )
		y = 0;
	if(x >= ai->Getmap()->xSectors)
		x = ai->Getmap()->xSectors-1;
	if(y >= ai->Getmap()->ySectors)
		y = ai->Getmap()->ySectors-1;

	// set sector as part of the base
	if(AAIMap::s_teamSectorMap.IsSectorOccupied(x,y) )
	{
		// sector already occupied by another aai team (coms starting too close to each other)
		// choose next free sector
		ChooseDifferentStartingSector(x, y);
	}
	else
		ai->Getbrain()->AssignSectorToBase(&ai->Getmap()->m_sector[x][y], true);
	
	ai->Getbrain()->ExpandBaseAtStartup();

	// now that we know the side, init buildques
	InitBuildques();

	ai->Getut()->AddConstructor(commanderUnitId, commanderDefId);

	// get economy working
	CheckRessources();
}

void AAIExecute::MoveUnitTo(int unit, float3 *position)
{
	Command c(CMD_MOVE);
	c.PushPos(*position);

	//ai->Getcb()->GiveOrder(unit, &c);
	GiveOrder(&c, unit, "MoveUnitTo");
	ai->Getut()->SetUnitStatus(unit, MOVING);
}

void AAIExecute::stopUnit(int unit)
{
	Command c(CMD_STOP);

	//ai->Getcb()->GiveOrder(unit, &c);
	GiveOrder(&c, unit, "StopUnit");
	ai->Getut()->SetUnitStatus(unit, UNIT_IDLE);
}

// returns true if unit is busy
bool AAIExecute::IsBusy(int unit)
{
	const CCommandQueue* commands = ai->GetAICallback()->GetCurrentUnitCommands(unit);

	if(commands->empty())
		return false;
	return true;
}

// adds a unit to the group of attackers
void AAIExecute::AddUnitToGroup(const UnitId& unitId, const UnitDefId& unitDefId)
{
	// determine continent if necessary
	int continentId = -1;
	
	const AAIMovementType& moveType = ai->s_buildTree.GetMovementType(unitDefId);
	if( moveType.CannotMoveToOtherContinents() )
	{
		const float3 unitPos = ai->GetAICallback()->GetUnitPos(unitId.id);
		continentId = AAIMap::GetContinentID(unitPos);
	}

	// try to add unit to an existing group
	const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(unitDefId);
	for(auto group = ai->GetUnitGroupsList(category).begin(); group != ai->GetUnitGroupsList(category).end(); ++group)
	{
		if((*group)->AddUnit(unitId, unitDefId, continentId))
		{
			ai->Getut()->units[unitId.id].group = *group;
			return;
		}
	}

	// end of grouplist has been reached and unit has not been assigned to any group
	// -> create new one
	AAIGroup *new_group = new AAIGroup(ai, unitDefId, continentId);
	new_group->AddUnit(unitId, unitDefId, continentId);
	ai->Getut()->units[unitId.id].group = new_group;

	ai->GetUnitGroupsList(category).push_back(new_group);
}

void AAIExecute::BuildCombatUnitOfCategory(const AAIMovementType& moveType, const TargetTypeValues& combatPowerCriteria, const UnitSelectionCriteria& unitSelectionCriteria, const std::vector<float>& factoryUtilization, bool urgent)
{
	// determine random float in [0:1]
	const float randomValue = 0.01f * static_cast<float>(std::rand()%101);

	// select unit independently from available constructor from time to time (to make sure AAI will order factories for advanced units as the game progresses)
	const bool constructorAvailable = (randomValue < 0.8f) && (ai->Getut()->activeFactories > 0);

	const UnitDefId unitDefId = ai->Getbt()->SelectCombatUnit(ai->GetSide(), moveType, combatPowerCriteria, unitSelectionCriteria, factoryUtilization, 6, constructorAvailable);

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
		
		if( ai->Getbt()->units_dynamic[unitDefId.id].constructorsAvailable <= 0 )
			ai->Getbt()->RequestFactoryFor(unitDefId);
		else
			AddUnitToBuildqueue(unitDefId, numberOfUnits, BuildQueuePosition::END);
	}
}

void AAIExecute::BuildScouts()
{
	if(ai->Getut()->GetTotalNumberOfUnitsOfCategory(EUnitCategory::SCOUT) < cfg->MAX_SCOUTS)
	{
		bool availableFactoryNeeded = true;
		float cost;
		float sightRange;

		const GamePhase gamePhase(ai->GetAICallback()->GetCurrentFrame());

		if(gamePhase.IsStartingPhase())
		{
			cost = 2.0f;
			sightRange = 0.5f;
		}
		else if(gamePhase.IsEarlyPhase())
		{
			cost = 1.0f;
			sightRange = 1.0f;
		}
		else
		{
			if(ai->Getut()->GetNumberOfActiveUnitsOfCategory(EUnitCategory::SCOUT) == 0)
			{
				cost = 2.0f;
				sightRange = 0.5f;
			}
			else
			{
				// sometimes prefer scouts with large los in late game
				if(rand()%3 == 1)
				{
					cost = 0.5f;
					sightRange = 4.0f;
					availableFactoryNeeded = false;
				}
				else
				{
					cost = 1.0f;
					sightRange = 1.0f;
				}
			}
		}

		// determine movement type of scout based on map
		const uint32_t suitableMovementTypes = ai->Getmap()->GetSuitableMovementTypesForMap();

		// request cloakable scouts from time to time
		const float cloaked = (rand()%5 == 1) ? 1.0f : 0.25f;
		
		const UnitDefId scoutId = ai->Getbt()->SelectScout(ai->GetSide(), sightRange, cost, cloaked, suitableMovementTypes, 10, availableFactoryNeeded);

		if(scoutId.IsValid())
		{
			const BuildQueuePosition queuePosition = (ai->Getut()->GetNumberOfActiveUnitsOfCategory(EUnitCategory::SCOUT) > 1) ? BuildQueuePosition::END : BuildQueuePosition::FRONT;

			AddUnitToBuildqueue(scoutId.id, 1, queuePosition);
		}
	}
}

void AAIExecute::SendScoutToNewDest(int scout)
{
	float3 nextScoutDestination = ai->Getmap()->GetNewScoutDest(UnitId(scout));

	if(nextScoutDestination.x > 0.0f)
		MoveUnitTo(scout, &nextScoutDestination);
}

float3 AAIExecute::DetermineBuildsite(UnitId builder, UnitDefId buildingDefId) const
{
	//-----------------------------------------------------------------------------------------------------------------
	// check the sector of the builder first
	//-----------------------------------------------------------------------------------------------------------------
	const float3 builderPosition = ai->GetAICallback()->GetUnitPos(builder.id);
	const AAISector* sector = ai->Getmap()->GetSectorOfPos(builderPosition);

	if(sector && (sector->GetDistanceToBase() == 0) )
	{
		const float3 buildsite = ai->Getmap()->DetermineBuildsiteInSector(buildingDefId, sector);

		if(buildsite.x > 0.0f)
			return buildsite;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// look in any of the base sectors
	//-----------------------------------------------------------------------------------------------------------------
	for(auto sector = ai->Getbrain()->m_sectorsInDistToBase[0].begin(); sector != ai->Getbrain()->m_sectorsInDistToBase[0].end(); ++sector)
	{
		const float3 buildsite = ai->Getmap()->DetermineBuildsiteInSector(buildingDefId, *sector);

		if(buildsite.x > 0.0f)
			return buildsite;
	}

	return ZeroVector;
}

float3 AAIExecute::DetermineBuildsiteInSector(UnitDefId building, const AAISector* sector) const
{
	// try random buildpos first
	const float3 buildsite = sector->GetRandomBuildsite(building, 20);

	if(buildsite.x > 0.0f)
		return buildsite;
	else
	{
		// search systematically for buildpos (i.e. search returns a buildpos if one is available in the sector)
		return ai->Getmap()->DetermineBuildsiteInSector(building, sector);
	}
}

float3 AAIExecute::DetermineBuildsiteForUnit(UnitId constructor, UnitDefId unitDefId) const
{
	const float3 constructorPosition = ai->GetAICallback()->GetUnitPos(constructor.id);
	
	float3 selectedBuildsite(ZeroVector);
	float minDist = AAIMap::maxSquaredMapDist;

	for(auto sector = ai->Getbrain()->m_sectorsInDistToBase[1].begin(); sector != ai->Getbrain()->m_sectorsInDistToBase[1].end(); ++sector)
	{
		const float3 pos = ai->Getmap()->DetermineBuildsiteInSector(unitDefId, *sector);

		if(pos.x > 0.0f)
		{
			const float dx = pos.x - constructorPosition.x;
			const float dy = pos.z - constructorPosition.z;
			const float squaredDist = dx*dx +dy*dy;

			if(squaredDist < minDist)
			{
				minDist = squaredDist;
				selectedBuildsite = pos;
			}
		}
	}

	return selectedBuildsite;
}

bool AAIExecute::AddUnitToBuildqueue(UnitDefId unitDefId, int number, BuildQueuePosition queuePosition, bool ignoreMaxQueueLength)
{
	std::list<UnitDefId>* selectedBuildqueue(nullptr);
	float highestRating(0.0f);

	for(auto constructor : ai->s_buildTree.GetConstructedByList(unitDefId))
	{
		if(ai->Getbt()->units_dynamic[constructor.id].active > 0)
		{
			std::list<UnitDefId>* buildqueue = GetBuildqueueOfFactory(constructor);

			if(buildqueue)
			{
				const float rating = (1.0f + 2.0f * (float) ai->Getbt()->units_dynamic[constructor.id].active) / static_cast<float>(buildqueue->size() + 2);

				if(rating > highestRating)
				{
					highestRating      = rating;
					selectedBuildqueue = buildqueue;
				}
			}	
		}
	}

	// determine position
	if(selectedBuildqueue)
	{
		if( ignoreMaxQueueLength || (selectedBuildqueue->size() < cfg->MAX_BUILDQUE_SIZE))
		{
			auto insertPosition = selectedBuildqueue->begin();

			if( (queuePosition == BuildQueuePosition::SECOND) && (selectedBuildqueue->size() > 0))
				++insertPosition;
			else if(queuePosition == BuildQueuePosition::END)
				insertPosition = selectedBuildqueue->end();
	
			selectedBuildqueue->insert(insertPosition, number, unitDefId);
			ai->Getbt()->units_dynamic[unitDefId.id].requested += number;
			ai->Getut()->UnitRequested(ai->s_buildTree.GetUnitCategory(unitDefId), number);
			return true;
		}
	}

	return false;
}

std::list<UnitDefId>* AAIExecute::GetBuildqueueOfFactory(UnitDefId constructorDefId)
{
	const FactoryId& factoryId = ai->s_buildTree.GetUnitTypeProperties(constructorDefId).m_factoryId;

	if( factoryId.IsValid() )
		return &m_buildqueues[factoryId.id];
	else
		return nullptr;
}

void AAIExecute::DetermineFactoryUtilization(std::vector<float>& factoryUtilization, bool considerOnlyActiveFactoryTypes) const
{
	const std::vector<UnitDefId>& factoryTable = ai->s_buildTree.GetFactoryDefIdLookupTable();

	for(int factoryId = 0; factoryId < ai->s_buildTree.GetNumberOfFactories(); ++factoryId)
	{
		const UnitTypeDynamic& unitTypeData = ai->Getbt()->GetDynamicUnitTypeData(factoryTable[factoryId]);

		if(    (considerOnlyActiveFactoryTypes == false)
			|| (unitTypeData.active > 0) )
		{
			const float queueLength = static_cast<float>(m_buildqueues[factoryId].size());
			factoryUtilization[factoryId] = 1.0f - ( queueLength / static_cast<float>(cfg->MAX_BUILDQUE_SIZE+1) );
		}
	}
}

void AAIExecute::InitBuildques()
{
	m_buildqueues.resize( ai->s_buildTree.GetNumberOfFactories() );
}

// ****************************************************************************************************
// all building functions
// ****************************************************************************************************

BuildOrderStatus AAIExecute::TryConstructionOf(UnitDefId landBuilding, UnitDefId seaBuilding, const AAISector* sector)
{
	BuildOrderStatus buildOrderStatus;

	if(sector->GetWaterTilesRatio() < 0.15f)
	{
		buildOrderStatus = TryConstructionOf(landBuilding, sector);

	}
	else if(sector->GetWaterTilesRatio() < 0.85f)
	{
		buildOrderStatus = TryConstructionOf(landBuilding, sector);

		if(buildOrderStatus != BuildOrderStatus::SUCCESSFUL)
			buildOrderStatus = TryConstructionOf(seaBuilding, sector);
	}
	else
	{
		buildOrderStatus = TryConstructionOf(seaBuilding, sector);
	}

	return buildOrderStatus;
}

BuildOrderStatus AAIExecute::TryConstructionOf(UnitDefId building, const AAISector* sector)
{
	if(building.IsValid())
	{
		const float3 buildsite = ai->Getmap()->DetermineBuildsiteInSector(building, sector);

		if(buildsite.x > 0.0f)
		{
			float min_dist;
			AAIConstructor* builder = ai->Getut()->FindClosestBuilder(building, &buildsite, true, &min_dist);

			if(builder)
			{
				builder->GiveConstructionOrder(building, buildsite);
				return BuildOrderStatus::SUCCESSFUL;
			}
			else
			{
				ai->Getbt()->RequestBuilderFor(building);
				return BuildOrderStatus::NO_BUILDER_AVAILABLE;
			}
		}
		else
		{
			if(ai->s_buildTree.GetMovementType(building).IsStaticLand() )
				ai->Getbrain()->ExpandBase(EMapType::LAND);
			else
				ai->Getbrain()->ExpandBase(EMapType::WATER);

			ai->Log("Base expanded when looking for buildsite for %s\n", ai->s_buildTree.GetUnitTypeProperties(building).m_name.c_str());
			return BuildOrderStatus::NO_BUILDSITE_FOUND;
		}
	}

	return BuildOrderStatus::BUILDING_INVALID;
}

bool AAIExecute::BuildExtractor()
{
	//-----------------------------------------------------------------------------------------------------------------
	// metal map
	//-----------------------------------------------------------------------------------------------------------------
	if(ai->Getmap()->metalMap)
	{
		// get id of an extractor and look for suitable builder
		UnitDefId landExtractor = ai->Getbt()->SelectExtractor(ai->GetSide(), 1.0f, 0.5f, false, false);

		if(landExtractor.IsValid())
		{
			AAIConstructor* land_builder  = ai->Getut()->FindBuilder(landExtractor.id, true);

			if(land_builder)
			{
				const float3 pos = DetermineBuildsite(land_builder->m_myUnitId, landExtractor);

				if(pos.x > 0.0f)
					land_builder->GiveConstructionOrder(landExtractor, pos);

				return true;
			}
			else
			{
				ai->Getbt()->RequestBuilderFor(landExtractor);
				return false;
			}
		}
	}

	//-----------------------------------------------------------------------------------------------------------------
	// normal map
	//-----------------------------------------------------------------------------------------------------------------

	const GamePhase& gamePhase = ai->GetGamePhase();

	float cost       = 0.5f;
	float efficiency = 2.0f;

	if(gamePhase.IsStartingPhase())
	{
		efficiency = 1.0;
		cost       = 2.0f;
	}
	else if(gamePhase.IsEarlyPhase())
	{
		efficiency = 1.0;
		cost       = 1.5f;
	}
	else
	{
		float metalSurplus = ai->Getbrain()->GetAverageMetalSurplus();

		if(metalSurplus < 0.5f && ai->Getut()->GetNumberOfActiveUnitsOfCategory(EUnitCategory::METAL_EXTRACTOR) < 4)
		{
			efficiency = 1.0;
			cost       = 2.0f;
		}
	}

	// select a land/water mex
	UnitDefId landExtractor, seaExtractor;

	AAIConstructor *land_builder = nullptr, *water_builder = nullptr;

	if(ai->Getmap()->land_metal_spots > 0)
	{
		landExtractor = ai->Getbt()->SelectExtractor(ai->GetSide(), cost, efficiency, false, false);

		if(landExtractor.IsValid())
			land_builder = ai->Getut()->FindBuilder(landExtractor.id, true);
	}

	if(ai->Getmap()->water_metal_spots > 0)
	{
		seaExtractor = ai->Getbt()->SelectExtractor(ai->GetSide(), cost, efficiency, false, true);

		if(seaExtractor.IsValid())
			water_builder = ai->Getut()->FindBuilder(seaExtractor.id, true);
	}

	// check if there is any builder for at least one of the selected extractors available
	if(!land_builder && !water_builder)
		return false;

	// check the first 10 free spots for the one with least distance to available builder
	const int maxExtractorBuildSpots = 10;
	std::list<PossibleSpotForMetalExtractor> extractorSpots;

	// determine max search dist - prevent crashes on smaller maps
	const int maxSearchDist = min(cfg->MAX_MEX_DISTANCE, static_cast<int>(ai->Getbrain()->m_sectorsInDistToBase.size()) );

	bool freeMetalSpotFound = false;
	
	for(int distanceFromBase = 0; distanceFromBase < maxSearchDist; ++distanceFromBase)
	{
		for(auto sector : ai->Getbrain()->m_sectorsInDistToBase[distanceFromBase])
		{
			if( sector->ShallBeConsideredForExtractorConstruction() )
			{
				for(auto spot : sector->metalSpots)
				{
					if(!spot->occupied)
					{
						freeMetalSpotFound = true;

						const UnitDefId extractor = (spot->pos.y >= 0.0f) ? landExtractor : seaExtractor;

						float distanceToClosestBuilder;
						AAIConstructor* builder = ai->Getut()->FindClosestBuilder(extractor, &spot->pos, ai->Getbrain()->CommanderAllowedForConstructionAt(sector, &spot->pos), &distanceToClosestBuilder);
						
						const float rating = (1.0f + ai->Getmap()->GetDistanceToCenterOfEnemyBase(spot->pos)) / (1.0f + distanceToClosestBuilder);

						if(builder)
							extractorSpots.push_back(PossibleSpotForMetalExtractor(spot, builder, rating));

						if(extractorSpots.size() >= maxExtractorBuildSpots)
							break;
					}
				}
			}

			if(extractorSpots.size() >= maxExtractorBuildSpots)
				break;
		}

		// stop looking for metal spots further away from base if already one found
		if( (distanceFromBase > 3) && (extractorSpots.size() > 0) )
			break;
	}

	// look for spot with minimum dist to available builder
	if(extractorSpots.size() > 0)
	{
		PossibleSpotForMetalExtractor& bestSpot = *(extractorSpots.begin());

		float highestRating(0.0f);

		for(auto spot = extractorSpots.begin(); spot != extractorSpots.end(); ++spot)
		{
			if(spot->m_rating > highestRating)
			{
				bestSpot = *spot;
				highestRating = spot->m_rating;
			}
		}

		// order mex construction for best spot
		const UnitDefId& extractor = (bestSpot.m_metalSpot->pos.y < 0.0f) ? seaExtractor : landExtractor;

		bestSpot.m_builder->GiveConstructionOrder(extractor, bestSpot.m_metalSpot->pos);
		bestSpot.m_metalSpot->occupied = true;

		AAISector* sector = ai->Getmap()->GetSectorOfPos(bestSpot.m_metalSpot->pos);

		if(sector)
			sector->UpdateFreeMetalSpots();

		return true;	
	}

	// dont build other things if construction could not be started due to unavailable builders
	if(freeMetalSpotFound)
		return false;
	else
		return true;
}

template<typename T>
struct InsertByRatingComparator
{
	bool operator()(const std::pair<T, float>& lhs, const std::pair<T, float>& rhs) const
	{
		return lhs.second > rhs.second;
	}
};

bool AAIExecute::BuildPowerPlant()
{
	const bool minimumNumberOfFactoriesNotMet =    (ai->Getut()->activeFactories < 1) 
												&& (ai->Getut()->GetNumberOfActiveUnitsOfCategory(EUnitCategory::POWER_PLANT) >= 2);

	// stop building power plants if 
	// - if construction of power plant ordered but not yet started
	// - already to much available energy
	// - minimum number of factories not constructed
	if(    (ai->Getut()->GetNumberOfRequestedUnitsOfCategory(EUnitCategory::POWER_PLANT) > 0) 
		|| (ai->Getbrain()->GetAveragePowerSurplus() > AAIConstants::powerSurplusToStopPowerPlantConstructionThreshold)
		|| minimumNumberOfFactoriesNotMet)
		return true;

	// if power plant is already under construction try to assist construction of other power plants first
	if(ai->Getut()->GetNumberOfUnitsUnderConstructionOfCategory(EUnitCategory::POWER_PLANT) > 0)
		return AssistConstructionOfCategory(EUnitCategory::POWER_PLANT);

	//-----------------------------------------------------------------------------------------------------------------
	// determine eligible sector (and sort them according to their rating)
	//-----------------------------------------------------------------------------------------------------------------

	std::list<AAISector*> sectors;
	DetermineSectorsToConstructEco(sectors);

	//-----------------------------------------------------------------------------------------------------------------
	// try to build power plant (start with highest rated sector)
	//-----------------------------------------------------------------------------------------------------------------
	PowerPlantSelectionCriteria selectionCriteria = ai->Getbrain()->DeterminePowerPlantSelectionCriteria();

	// do not try offshore construction if base edoes not contain water
	bool offshoreConstructionAttempted( (ai->Getbrain()->GetBaseWaterRatio() < 0.05f) ); 
	BuildOrderStatus buildOrderStatus(BuildOrderStatus::BUILDING_INVALID);

	// probability of trying to build sea power plant first is related to current water ratio of the base
	// determine random float in [0:1]
	const float randomValue = 0.01f * static_cast<float>(std::rand()%101);

	if( randomValue < ai->Getbrain()->GetBaseWaterRatio() )
	{
		UnitDefId seaPowerPlant  = ai->Getbt()->SelectPowerPlant(ai->GetSide(), selectionCriteria, true);
		buildOrderStatus = ConstructBuildingInSectors(seaPowerPlant, sectors);
		offshoreConstructionAttempted = true;
	}

	// try construction on land (if not already successful on water)
	if(buildOrderStatus != BuildOrderStatus::SUCCESSFUL)
	{
		UnitDefId landPowerPlant = ai->Getbt()->SelectPowerPlant(ai->GetSide(), selectionCriteria, false);
		buildOrderStatus = ConstructBuildingInSectors(landPowerPlant, sectors);
	}

	// try construction on water (if not already tried and construction on land has not been successful)
	if(!offshoreConstructionAttempted && (buildOrderStatus != BuildOrderStatus::SUCCESSFUL) )
	{
		UnitDefId seaPowerPlant  = ai->Getbt()->SelectPowerPlant(ai->GetSide(), selectionCriteria, true);
		buildOrderStatus = ConstructBuildingInSectors(seaPowerPlant, sectors);
	}

	if(buildOrderStatus == BuildOrderStatus::NO_BUILDER_AVAILABLE)
		return false;

	//-----------------------------------------------------------------------------------------------------------------
	// expand base if no suitable buildsite found
	//-----------------------------------------------------------------------------------------------------------------

	return true;
}

void AAIExecute::DetermineSectorsToConstructEco(std::list<AAISector*>& sectors) const
{
	const float previousGamesWeight = 54000.0f / static_cast<float>(2*ai->GetAICallback()->GetCurrentFrame() + 54000);
	const float currentGameWeight   = 1.0f - previousGamesWeight;

	std::set< std::pair<AAISector*, float>, InsertByRatingComparator<AAISector*> > availableSectors; 

	for(auto sector : ai->Getbrain()->m_sectorsInDistToBase[0])
	{
		const float rating = sector->GetRatingForPowerPlant(previousGamesWeight, currentGameWeight);

		if(rating > 0.0f)
			availableSectors.insert( std::pair<AAISector*, float>(sector, rating) );
	}

	for(auto sector : availableSectors)
		sectors.push_back(sector.first);
}

BuildOrderStatus AAIExecute::ConstructBuildingInSectors(UnitDefId building, std::list<AAISector*>& availableSectors)
{
	if(building.IsValid() == false)
		return BuildOrderStatus::BUILDING_INVALID;

	const bool water = ai->s_buildTree.GetMovementType(building).IsSea();

	for(auto sector : availableSectors)
	{
		if(    ( water && sector->GetWaterTilesRatio() > 0.05f)
			|| (!water && sector->GetFlatTilesRatio()  > 0.05f) )
		{
			BuildOrderStatus buildOrderStatus = TryConstructionOfBuilding(building, sector);

			// continue with next sector if no buildsite found in current sector - abort if successful or no constrcution unit available
			if(buildOrderStatus == BuildOrderStatus::SUCCESSFUL)
				return buildOrderStatus;
			else if(buildOrderStatus == BuildOrderStatus::NO_BUILDER_AVAILABLE)
			{
				ai->Getbt()->RequestBuilderFor(building);
				return buildOrderStatus;
			}
		}
	}

	return BuildOrderStatus::NO_BUILDSITE_FOUND;
}

BuildOrderStatus AAIExecute::TryConstructionOfBuilding(UnitDefId building, AAISector* sector)
{
	const float3 buildsite = ai->Getmap()->DetermineBuildsiteInSector(building, sector);

	if(buildsite.x > 0.0f)
	{
		float min_dist;
		AAIConstructor* builder = ai->Getut()->FindClosestBuilder(building, &buildsite, true, &min_dist);

		if(builder)
		{
			builder->GiveConstructionOrder(building, buildsite);
			return BuildOrderStatus::SUCCESSFUL;
		}
		else
			return BuildOrderStatus::NO_BUILDER_AVAILABLE;
	}
	else
		return BuildOrderStatus::NO_BUILDSITE_FOUND;
}

bool AAIExecute::BuildMetalMaker()
{
	const AAIUnitCategory metalMaker(EUnitCategory::METAL_EXTRACTOR);
	if( (ai->Getut()->activeFactories < 1) && (ai->Getut()->GetNumberOfActiveUnitsOfCategory(metalMaker) >= 2) )
		return true;

	if(    (ai->Getut()->GetNumberOfFutureUnitsOfCategory(metalMaker) > 0) 
		|| (disabledMMakers >= 1) )
		return true;

	bool checkWater, checkGround;
	AAIConstructor *builder;
	float3 pos;
	// urgency < 4

	float urgency = ai->Getbrain()->GetMetalUrgency() / 2.0f;

	float cost = 0.25f + ai->Getbrain()->Affordable() / 2.0f;

	float efficiency = 0.25f + ai->Getut()->GetNumberOfActiveUnitsOfCategory(AAIUnitCategory(EUnitCategory::METAL_MAKER)) / 4.0f ;
	float metal = efficiency;


	// sort sectors according to threat level
	learned = 70000.0 / (ai->GetAICallback()->GetCurrentFrame() + 35000) + 1;
	current = 2.5 - learned;

	ai->Getbrain()->m_sectorsInDistToBase[0].sort(least_dangerous);

	for(auto sector = ai->Getbrain()->m_sectorsInDistToBase[0].begin(); sector != ai->Getbrain()->m_sectorsInDistToBase[0].end(); ++sector)
	{
		if((*sector)->GetWaterTilesRatio() < 0.15f)
		{
			checkWater = false;
			checkGround = true;
		}
		else if((*sector)->GetWaterTilesRatio() < 0.85f)
		{
			checkWater = true;
			checkGround = true;
		}
		else
		{
			checkWater = true;
			checkGround = false;
		}

		if(checkGround)
		{
			UnitDefId maker = ai->Getbt()->GetMetalMaker(ai->GetSide(), cost,  efficiency, metal, urgency, false, false);

			// currently aai cannot build this building
			if(maker.IsValid() && ai->Getbt()->units_dynamic[maker.id].constructorsAvailable <= 0)
			{
				if(ai->Getbt()->units_dynamic[maker.id].constructorsRequested <= 0)
					ai->Getbt()->RequestBuilderFor(maker);

				maker = ai->Getbt()->GetMetalMaker(ai->GetSide(), cost, efficiency, metal, urgency, false, true);
			}

			if(maker.IsValid())
			{
				pos = ai->Getmap()->DetermineBuildsiteInSector(maker, *sector);

				if(pos.x > 0)
				{
					float min_dist;
					builder = ai->Getut()->FindClosestBuilder(maker, &pos, true, &min_dist);

					if(builder)
					{
						builder->GiveConstructionOrder(maker, pos);
						return true;
					}
					else
					{
						ai->Getbt()->RequestBuilderFor(maker);
						return false;
					}
				}
				else
				{
					ai->Getbrain()->ExpandBase(EMapType::LAND);
					ai->Log("Base expanded by BuildMetalMaker()\n");
				}
			}
		}

		if(checkWater)
		{
			UnitDefId maker = ai->Getbt()->GetMetalMaker(ai->GetSide(), ai->Getbrain()->Affordable(),  8.0/(urgency+2.0), 64.0/(16*urgency+2.0), urgency, true, false);

			// currently aai cannot build this building
			if(maker.IsValid() && ai->Getbt()->units_dynamic[maker.id].constructorsAvailable <= 0)
			{
				if(ai->Getbt()->units_dynamic[maker.id].constructorsRequested <= 0)
					ai->Getbt()->RequestBuilderFor(maker);

				maker = ai->Getbt()->GetMetalMaker(ai->GetSide(), ai->Getbrain()->Affordable(),  8.0/(urgency+2.0), 64.0/(16*urgency+2.0), urgency, true, true);
			}

			if(maker.IsValid())
			{
				pos = ai->Getmap()->DetermineBuildsiteInSector(maker, *sector);

				if(pos.x > 0)
				{
					float min_dist;
					builder = ai->Getut()->FindClosestBuilder(maker, &pos, true, &min_dist);

					if(builder)
					{
						builder->GiveConstructionOrder(maker, pos);
						return true;
					}
					else
					{
						ai->Getbt()->RequestBuilderFor(maker);
						return false;
					}
				}
				else
				{
					ai->Getbrain()->ExpandBase(EMapType::WATER);
					ai->Log("Base expanded by BuildMetalMaker() (water sector)\n");
				}
			}
		}
	}

	return true;
}

bool AAIExecute::BuildStorage()
{
	const AAIUnitCategory storage(EUnitCategory::STORAGE);
	if(	   (ai->Getut()->GetNumberOfFutureUnitsOfCategory(storage) > 0) 
		|| (ai->Getut()->GetNumberOfActiveUnitsOfCategory(storage) >= cfg->MAX_STORAGE)
		|| (ai->Getut()->activeFactories < 1) )
		return true;

	//-----------------------------------------------------------------------------------------------------------------
	// determine eligible sector (and sort them according to their rating)
	//-----------------------------------------------------------------------------------------------------------------

	std::list<AAISector*> sectors;
	DetermineSectorsToConstructEco(sectors);

	//-----------------------------------------------------------------------------------------------------------------
	// try to build storage (start with highest rated sector)
	//-----------------------------------------------------------------------------------------------------------------
	
	StorageSelectionCriteria selectionCriteria = ai->Getbrain()->DetermineStorageSelectionCriteria();

	// do not try offshore construction if base edoes not contain water
	bool offshoreConstructionAttempted( (ai->Getbrain()->GetBaseWaterRatio() < 0.05f) ); 
	BuildOrderStatus buildOrderStatus(BuildOrderStatus::BUILDING_INVALID);

	// probability of trying to build sea power plant first is related to current water ratio of the base
	// determine random float in [0:1]
	const float randomValue = 0.01f * static_cast<float>(std::rand()%101);

	if( randomValue < ai->Getbrain()->GetBaseWaterRatio() )
	{
		UnitDefId seaStorage  = ai->Getbt()->SelectStorage(ai->GetSide(), selectionCriteria, true);
		buildOrderStatus = ConstructBuildingInSectors(seaStorage, sectors);
		offshoreConstructionAttempted = true;
	}

	// try construction on land (if not already successful on water)
	if(buildOrderStatus != BuildOrderStatus::SUCCESSFUL)
	{
		UnitDefId landStorage = ai->Getbt()->SelectStorage(ai->GetSide(), selectionCriteria, false);
		buildOrderStatus = ConstructBuildingInSectors(landStorage, sectors);
	}

	// try construction on water (if not already tried and construction on land has not been successful)
	if(!offshoreConstructionAttempted && (buildOrderStatus != BuildOrderStatus::SUCCESSFUL) )
	{
		UnitDefId seaStorage  = ai->Getbt()->SelectStorage(ai->GetSide(), selectionCriteria, true);
		buildOrderStatus = ConstructBuildingInSectors(seaStorage, sectors);
	}

	if(buildOrderStatus == BuildOrderStatus::NO_BUILDER_AVAILABLE)
		return false;

	//-----------------------------------------------------------------------------------------------------------------
	// expand base if no suitable buildsite found
	//-----------------------------------------------------------------------------------------------------------------

	return true;
}

bool AAIExecute::BuildAirBase()
{
	return true; //! @todo detection of air base currently broken
	/*if(ai->Getut()->futureUnits[AIR_BASE] + ai->Getut()->requestedUnits[AIR_BASE] > 0 || ai->Getut()->activeUnits[AIR_BASE] >= cfg->MAX_AIR_BASE)
		return true;

	int airbase = 0;
	bool checkWater, checkGround;
	AAIConstructor *builder;
	float3 pos;

	for(list<AAISector*>::iterator sector = ai->Getbrain()->sectors[0].begin(); sector != ai->Getbrain()->sectors[0].end(); ++sector)
	{
		if((*sector)->GetWaterTilesRatio() < 0.15)
		{
			checkWater = false;
			checkGround = true;
		}
		else if((*sector)->GetWaterTilesRatio() < 0.85)
		{
			checkWater = true;
			checkGround = true;
		}
		else
		{
			checkWater = true;
			checkGround = false;
		}

		if(checkGround)
		{

			airbase = ai->Getbt()->GetAirBase(ai->Getside(), ai->Getbrain()->Affordable(), false, false);

			if(airbase && ai->Getbt()->units_dynamic[airbase].constructorsAvailable <= 0)
			{
				if(ai->Getbt()->units_dynamic[airbase].constructorsRequested <= 0)
					ai->Getbt()->BuildBuilderFor(UnitDefId(airbase));

				airbase = ai->Getbt()->GetAirBase(ai->Getside(), ai->Getbrain()->Affordable(), false, true);
			}

			if(airbase)
			{
				pos = (*sector)->GetBuildsite(airbase, false);

				if(pos.x > 0)
				{
					float min_dist;
					builder = ai->Getut()->FindClosestBuilder(airbase, &pos, true, &min_dist);

					if(builder)
					{
						builder->GiveConstructionOrder(airbase, pos, false);
						return true;
					}
					else
					{
						ai->Getbt()->BuildBuilderFor(UnitDefId(airbase));
						return false;
					}
				}
				else
				{
					ai->Getbrain()->ExpandBase(LAND_SECTOR);
					ai->Log("Base expanded by BuildAirBase()\n");
				}
			}
		}

		if(checkWater)
		{
			airbase = ai->Getbt()->GetAirBase(ai->Getside(), ai->Getbrain()->Affordable(), true, false);

			if(airbase && ai->Getbt()->units_dynamic[airbase].constructorsAvailable <= 0 )
			{
				if(ai->Getbt()->units_dynamic[airbase].constructorsRequested <= 0)
					ai->Getbt()->BuildBuilderFor(UnitDefId(airbase));

				airbase = ai->Getbt()->GetAirBase(ai->Getside(), ai->Getbrain()->Affordable(), true, true);
			}

			if(airbase)
			{
				pos = (*sector)->GetBuildsite(airbase, true);

				if(pos.x > 0)
				{
					float min_dist;
					builder = ai->Getut()->FindClosestBuilder(airbase, &pos, true, &min_dist);

					if(builder)
					{
						builder->GiveConstructionOrder(airbase, pos, true);
						return true;
					}
					else
					{
						ai->Getbt()->BuildBuilderFor(UnitDefId(airbase));
						return false;
					}
				}
				else
				{
					ai->Getbrain()->ExpandBase(WATER_SECTOR);
					ai->Log("Base expanded by BuildAirBase() (water sector)\n");
				}
			}
		}
	}

	return true;*/
}

bool AAIExecute::BuildDefences()
{
	if(    (ai->Getut()->GetNumberOfFutureUnitsOfCategory(EUnitCategory::STATIC_DEFENCE) > 2) 
		|| (m_sectorToBuildNextDefence == nullptr) )
		return true;

	BuildOrderStatus status = BuildStationaryDefenceVS(m_nextDefenceVsTargetType, m_sectorToBuildNextDefence);

	if(status == BuildOrderStatus::NO_BUILDER_AVAILABLE)
		return false;
	else if(status == BuildOrderStatus::NO_BUILDSITE_FOUND)
		m_sectorToBuildNextDefence->FailedToConstructStaticDefence();

	m_sectorToBuildNextDefence = nullptr;

	return true;
}

BuildOrderStatus AAIExecute::BuildStationaryDefenceVS(const AAITargetType& targetType, const AAISector *dest)
{
	// dont build in sectors already occupied by allies
	if(dest->GetNumberOfAlliedBuildings() > 2)
		return BuildOrderStatus::SUCCESSFUL;

	//-----------------------------------------------------------------------------------------------------------------
	// dont start construction of further defences if expensive defences are already under construction in this sector
	//-----------------------------------------------------------------------------------------------------------------
	for(const auto task : ai->GetBuildTasks())
	{
		if(task->IsExpensiveUnitOfCategoryInSector(ai, EUnitCategory::STATIC_DEFENCE, dest) )
			return BuildOrderStatus::SUCCESSFUL;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// determine criteria for selection of static defence and its buildsite
	//-----------------------------------------------------------------------------------------------------------------
	StaticDefenceSelectionCriteria selectionCriteria(targetType);
	ai->Getbrain()->DetermineStaticDefenceSelectionCriteria(selectionCriteria, dest);

	//-----------------------------------------------------------------------------------------------------------------
	// try construction of static defence according to determined criteria
	//-----------------------------------------------------------------------------------------------------------------
	BuildOrderStatus status(BuildOrderStatus::BUILDING_INVALID);

	if(dest->GetWaterTilesRatio() < 0.85f)
		status = BuildStaticDefence(dest, selectionCriteria, false);

	if( (dest->GetWaterTilesRatio() > 0.15f) && (status != BuildOrderStatus::SUCCESSFUL))
		status = BuildStaticDefence(dest, selectionCriteria, true);

	return status;
}

BuildOrderStatus AAIExecute::BuildStaticDefence(const AAISector* sector, const StaticDefenceSelectionCriteria& selectionCriteria, bool water) const
{
	const UnitDefId selectedDefence = ai->Getbt()->SelectStaticDefence(ai->GetSide(), selectionCriteria, water);

	if(selectedDefence.IsValid())
	{
		const float3 buildsite = ai->Getmap()->DetermineBuildsiteForStaticDefence(selectedDefence, sector, selectionCriteria.targetType, selectionCriteria.terrain);

		if(buildsite.x > 0.0f)
		{
			float min_dist;
			AAIConstructor *builder = ai->Getut()->FindClosestBuilder(selectedDefence, &buildsite, true, &min_dist);

			if(builder)
			{
				builder->GiveConstructionOrder(selectedDefence, buildsite);
				ai->Getmap()->AddOrRemoveStaticDefence(buildsite, selectedDefence, true);
				return BuildOrderStatus::SUCCESSFUL;
			}
			else
			{
				ai->Getbt()->RequestBuilderFor(selectedDefence);
				return BuildOrderStatus::NO_BUILDER_AVAILABLE;
			}
		}
		else
			return BuildOrderStatus::NO_BUILDSITE_FOUND;
	}
	else
	{
		ai->Log("No static Defence found!\n");
		return BuildOrderStatus::BUILDING_INVALID;
	}
}

bool AAIExecute::BuildArty()
{
	if(ai->Getut()->GetNumberOfFutureUnitsOfCategory(EUnitCategory::STATIC_ARTILLERY) > 0)
		return true;

	const float cost(1.0f);
	const float range(1.5f);

	UnitDefId landArtillery = ai->Getbt()->SelectStaticArtillery(ai->GetSide(), cost, range, false);
	UnitDefId seaArtillery  = ai->Getbt()->SelectStaticArtillery(ai->GetSide(), cost, range, true);

	if(landArtillery.IsValid() && (ai->Getbt()->units_dynamic[landArtillery.id].constructorsAvailable <= 0))
	{
		if(ai->Getbt()->units_dynamic[landArtillery.id].constructorsRequested <= 0)
			ai->Getbt()->RequestBuilderFor(landArtillery);
	}

	if(seaArtillery.IsValid() && (ai->Getbt()->units_dynamic[seaArtillery.id].constructorsAvailable <= 0))
	{
		if(ai->Getbt()->units_dynamic[seaArtillery.id].constructorsRequested <= 0)
			ai->Getbt()->RequestBuilderFor(seaArtillery);
	}

	//ai->Log("Selected artillery (land/sea): %s / %s\n", ai->s_buildTree.GetUnitTypeProperties(landArtillery).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(seaArtillery).m_name.c_str());

	float  bestRating(0.0f);
	float3 bestPosition(ZeroVector);

	for(auto sector = ai->Getbrain()->m_sectorsInDistToBase[0].begin(); sector != ai->Getbrain()->m_sectorsInDistToBase[0].end(); ++sector)
	{
		if((*sector)->GetNumberOfBuildings(EUnitCategory::STATIC_ARTILLERY) < 2)
		{
			float3 position = ZeroVector;

			if(landArtillery.IsValid()  && ((*sector)->GetWaterTilesRatio() < 0.9f) )
				position = (*sector)->GetRadarArtyBuildsite(landArtillery.id, ai->s_buildTree.GetMaxRange(landArtillery)/4.0f, false);

			if((position.x <= 0.0f) && seaArtillery.IsValid() && ((*sector)->GetWaterTilesRatio() > 0.1f) )
				position = (*sector)->GetRadarArtyBuildsite(seaArtillery.id, ai->s_buildTree.GetMaxRange(seaArtillery)/4.0f, true);
			
			if(position.x > 0)
			{
				const float myRating = ai->Getmap()->GetEdgeDistance(position);

				if(myRating > bestRating)
				{
					bestRating   = myRating;
					bestPosition = position;
				}
			}
		}
	}

	// Check if suitable position for artillery has been found
	if(bestPosition.x > 0.0f)
	{
		UnitDefId artillery = (bestPosition.y > 0.0f) ? landArtillery : seaArtillery;

		//ai->Log("Position for %s found\n", ai->s_buildTree.GetUnitTypeProperties(artillery).m_name.c_str());

		float minDistance;
		AAIConstructor *builder = ai->Getut()->FindClosestBuilder(artillery, &bestPosition, true, &minDistance);

		if(builder)
		{
			builder->GiveConstructionOrder(artillery, bestPosition);
			return true;
		}
		else
		{
			ai->Getbt()->RequestBuilderFor(artillery);
			return false;
		}
	}

	return true;
}

bool AAIExecute::BuildStaticConstructor()
{
	if(ai->Getut()->GetNumberOfFutureUnitsOfCategory(EUnitCategory::STATIC_CONSTRUCTOR) > 0)
		return true;

	//-----------------------------------------------------------------------------------------------------------------
	// determine which factories have the highest priority
	//-----------------------------------------------------------------------------------------------------------------

	std::set< std::pair<UnitDefId, float>, InsertByRatingComparator<UnitDefId> > requestedFactories;

	//ai->Log("Building next factory:\n");

	for(auto factory : ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, ai->GetSide()) )
	{
		if(ai->Getbt()->GetDynamicUnitTypeData(factory).requested > 0)
		{
			const float urgency = ai->Getbrain()->DetermineConstructionUrgencyOfFactory(factory);
			requestedFactories.insert( std::pair<UnitDefId, float>(factory, urgency) );
			//ai->Log("Added %s - %f\n", ai->s_buildTree.GetUnitTypeProperties(factory).m_name.c_str(), urgency);
		}
	}
	
	//-----------------------------------------------------------------------------------------------------------------
	// try to build factories according to their priority
	//-----------------------------------------------------------------------------------------------------------------

	for(auto requestedFactory : requestedFactories)
	{
		// find suitable builder
		AAIConstructor* builder = ai->Getut()->FindBuilder(requestedFactory.first.id, true);
	
		if(builder == nullptr)
		{
			// try construction of next factory in queue if there is no active builder (i.e. potential builders not just currently busy)
			if( ai->Getbt()->GetDynamicUnitTypeData(requestedFactory.first).constructorsAvailable <= 0) 
			{
				continue;
			}
			else
			{
				// keep factory at highest urgency if the construction failed due to (temporarily) unavailable builder
				return false;
			}
		}

		//-----------------------------------------------------------------------------------------------------------------
		// builder is available -> look for suitable buildsite
		//-----------------------------------------------------------------------------------------------------------------
		const bool isSeaFactory( ai->s_buildTree.GetMovementType(requestedFactory.first).IsStaticSea() );

		ai->Getbrain()->m_sectorsInDistToBase[0].sort(isSeaFactory ? suitable_for_sea_factory : suitable_for_ground_factory);

		for(auto sector : ai->Getbrain()->m_sectorsInDistToBase[0])
		{
			const float3 buildsite = DetermineBuildsiteInSector(requestedFactory.first, sector);

			if(buildsite.x > 0.0f)
			{
				float min_dist;
				builder = ai->Getut()->FindClosestBuilder(requestedFactory.first, &buildsite, true, &min_dist);

				if(builder != nullptr)
				{
					builder->GiveConstructionOrder(requestedFactory.first, buildsite);

					ai->Getbt()->ConstructionOrderForFactoryGiven(requestedFactory.first);
					return true;
				}
				else 
				{
					if(ai->Getbt()->GetTotalNumberOfConstructorsForUnit(requestedFactory.first) <= 0)
						ai->Getbt()->RequestBuilderFor(requestedFactory.first);

					return false;
				}
			}
		}

		// no buildpos found in whole base -> expand base
		bool expanded = false;

		// no suitable buildsite found
		if(isSeaFactory)
		{
			ai->Getbrain()->ExpandBase(EMapType::WATER, false);
			ai->Log("Base expanded by BuildFactory() (water sector)\n");
		}
		else
		{
			expanded = ai->Getbrain()->ExpandBase(EMapType::LAND, false);
			ai->Log("Base expanded by BuildFactory()\n");
		}

		return false;	
	}

	return true;
}

bool AAIExecute::BuildRadar()
{
	const AAIUnitCategory sensor(EUnitCategory::STATIC_SENSOR);
	if(ai->Getut()->GetTotalNumberOfUnitsOfCategory(sensor) > ai->Getbrain()->m_sectorsInDistToBase[0].size())
		return true;


	float3 bestPosition(ZeroVector);
	float  bestRating(-100000.0f);

	const float cost = ai->Getbrain()->Affordable();
	const float range = 10.0 / (cost + 1);

	const UnitDefId	landRadar = ai->Getbt()->SelectRadar(ai->GetSide(), cost, range, false);
	const UnitDefId	seaRadar  = ai->Getbt()->SelectRadar(ai->GetSide(), cost, range, true);

	UnitDefId selectedRadar;
	
	for(int dist = 0; dist < 2; ++dist)
	{
		for(auto sector = ai->Getbrain()->m_sectorsInDistToBase[dist].begin(); sector != ai->Getbrain()->m_sectorsInDistToBase[dist].end(); ++sector)
		{
			if((*sector)->GetNumberOfBuildings(EUnitCategory::STATIC_SENSOR) <= 0)
			{
				float3 myPosition(ZeroVector);
				bool   seaPositionFound(false);

				if( landRadar.IsValid() && ((*sector)->GetWaterTilesRatio() < 0.9f) )
					myPosition = (*sector)->GetRadarArtyBuildsite(landRadar.id, ai->s_buildTree.GetMaxRange(landRadar), false);

				if( (myPosition.x == 0.0f) && seaRadar.IsValid() && ((*sector)->GetWaterTilesRatio() > 0.1f) )
				{
					myPosition = (*sector)->GetRadarArtyBuildsite(seaRadar.id, ai->s_buildTree.GetMaxRange(seaRadar), true);
					seaPositionFound = true;
				}

				if(myPosition.x > 0.0f)
				{
					const float myRating = - ai->Getmap()->GetEdgeDistance(myPosition);

					if(myRating > bestRating)
					{
						selectedRadar = seaPositionFound ? seaRadar : landRadar;
						bestPosition  = myPosition;
						bestRating    = myRating;
					}
				}
			}
		}
	}

	if(selectedRadar.IsValid())
	{
		float min_dist;
		AAIConstructor *builder = ai->Getut()->FindClosestBuilder(selectedRadar, &bestPosition, true, &min_dist);

		if(builder)
		{
			builder->GiveConstructionOrder(selectedRadar, bestPosition);
			return true;
		}
		else
		{
			ai->Getbt()->RequestBuilderFor(selectedRadar);
			return false;
		}
	}

	return true;
}

bool AAIExecute::BuildJammer()
{
	return true; //! @todo Reactivate buidling of stationary jammers
	/*if(ai->Getut()->futureUnits[STATIONARY_JAMMER] + ai->Getut()->requestedUnits[STATIONARY_JAMMER] > 0)
		return true;

	float3 pos = ZeroVector;

	float cost = ai->Getbrain()->Affordable();
	float range = 10.0 / (cost + 1);

	int ground_jammer = 0;
	int sea_jammer = 0;

	// get ground jammer
	if(ai->Getmap()->land_ratio > 0.02f)
	{
		ground_jammer = ai->Getbt()->GetJammer(ai->Getside(), cost, range, false, false);

		if(ground_jammer && ai->Getbt()->units_dynamic[ground_jammer].constructorsAvailable <= 0)
		{
			if(ai->Getbt()->units_dynamic[ground_jammer].constructorsRequested <= 0)
				ai->Getbt()->BuildBuilderFor(UnitDefId(ground_jammer));

			ground_jammer = ai->Getbt()->GetJammer(ai->Getside(), cost, range, false, true);
		}
	}

	// get sea jammer
	if(ai->Getmap()->GetWaterTilesRatio() > 0.02f)
	{
		sea_jammer = ai->Getbt()->GetJammer(ai->Getside(), cost, range, false, false);

		if(sea_jammer && ai->Getbt()->units_dynamic[sea_jammer].constructorsAvailable <= 0)
		{
			if(ai->Getbt()->units_dynamic[sea_jammer].constructorsRequested <= 0)
				ai->Getbt()->BuildBuilderFor(UnitDefId(sea_jammer));

			sea_jammer = ai->Getbt()->GetJammer(ai->Getside(), cost, range, false, true);
		}
	}

	for(list<AAISector*>::iterator sector = ai->Getbrain()->sectors[0].begin(); sector != ai->Getbrain()->sectors[0].end(); ++sector)
	{
		if((*sector)->my_buildings[STATIONARY_JAMMER] <= 0)
		{
			if(ground_jammer && (*sector)->GetWaterTilesRatio() < 0.9f)
				pos = (*sector)->GetCenterBuildsite(ground_jammer, false);

			if(pos.x == 0 && sea_jammer && (*sector)->GetWaterTilesRatio() > 0.1f)
			{
				pos = (*sector)->GetCenterBuildsite(sea_jammer, true);

				if(pos.x > 0)
					ground_jammer = sea_jammer;
			}

			if(pos.x > 0)
			{
				float min_dist;
				AAIConstructor *builder = ai->Getut()->FindClosestBuilder(ground_jammer, &pos, true, &min_dist);

				if(builder)
				{
					builder->GiveConstructionOrder(ground_jammer, pos, false);
					return true;
				}
				else
				{
					ai->Getbt()->BuildBuilderFor(UnitDefId(ground_jammer));
					return false;
				}
			}
		}
	}

	return true;*/
}

void AAIExecute::BuildStaticDefenceForExtractor(UnitId extractorId, UnitDefId extractorDefId) const
{
	if(ai->Getut()->activeFactories < cfg->MIN_FACTORIES_FOR_DEFENCES)
		return;

	const float3 extractorPos = ai->GetAICallback()->GetUnitPos(extractorId.id);

	const MapPos& centerOfBase = ai->Getbrain()->GetCenterOfBase(); 
	const float3 base_pos(centerOfBase.x * SQUARE_SIZE, 0.0f, centerOfBase.y * SQUARE_SIZE);
	
	// check if mex is located in a small pond/on a little island
	if(ai->Getmap()->LocatedOnSmallContinent(extractorPos))
		return;

	const AAISector *sector = ai->Getmap()->GetSectorOfPos(extractorPos); 

	if(sector) 
	{
		if(    (sector->GetDistanceToBase() > 0)
			&& (sector->GetDistanceToBase() <= cfg->MAX_MEX_DEFENCE_DISTANCE)
			&& (sector->GetNumberOfBuildings(EUnitCategory::STATIC_DEFENCE) < 2) )
		{
			const bool water = ai->s_buildTree.GetMovementType(extractorDefId).IsStaticSea() ? true : false;
			const AAITargetType targetType( water ? ETargetType::FLOATER : ETargetType::SURFACE);

			const StaticDefenceSelectionCriteria selectionCriteria(targetType, 1.0f, 0.1f, 2.0f, 3.0f, 1.0f, 0);
			const UnitDefId defence = ai->Getbt()->SelectStaticDefence(ai->GetSide(), selectionCriteria, water); 

			// find closest builder
			if(defence.IsValid())
			{
				const MapPos& enemyBase = ai->Getmap()->GetCenterOfEnemyBase();

				float xDir = static_cast<float>(SQUARE_SIZE*enemyBase.x) - extractorPos.x;
				float yDir = static_cast<float>(SQUARE_SIZE*enemyBase.y) - extractorPos.z;

				const float inverseNorm = fastmath::isqrt_nosse(xDir*xDir+yDir*yDir);
				xDir *= inverseNorm;
				yDir *= inverseNorm;

				// static defence shall be placed in sufficient distance to extractor in direction of assumed center of enemy base
				const UnitFootprint extractorFootprint = ai->s_buildTree.GetFootprint(extractorDefId);
				const float distToExtratcor = 80.f + static_cast<float>( SQUARE_SIZE * std::max(extractorFootprint.xSize, extractorFootprint.ySize) );

				float3 defenceBuildPos;
				defenceBuildPos.x = extractorPos.x + distToExtratcor * xDir;
				defenceBuildPos.z = extractorPos.z + distToExtratcor * yDir;

				// find final buildsite (close to previously determined location)
				const float3 finalDefenceBuildPos = ai->GetAICallback()->ClosestBuildSite(&ai->Getbt()->GetUnitDef(defence.id), defenceBuildPos, 1400.0f, 2);

				if(finalDefenceBuildPos.x > 0.0f)
				{
					const AAISector* sector = ai->Getmap()->GetSectorOfPos(finalDefenceBuildPos);

					const bool commanderAllowed = sector ? (sector->GetDistanceToBase() < 3) : false;

					float min_dist;
					AAIConstructor *builder = ai->Getut()->FindClosestBuilder(defence, &finalDefenceBuildPos, commanderAllowed, &min_dist);

					if(builder)
						builder->GiveConstructionOrder(defence, finalDefenceBuildPos);
					else
						ai->Log("No construction unit found to defend extractor %s!\n", ai->s_buildTree.GetUnitTypeProperties(defence).m_name.c_str());
					
				}
			}
		}
	}
}

void AAIExecute::CheckStationaryArty()
{
	if(cfg->MAX_STAT_ARTY == 0)
		return;

	const AAIUnitCategory staticArtillery(EUnitCategory::STATIC_ARTILLERY);

	if(ai->Getut()->GetNumberOfUnitsUnderConstructionOfCategory(staticArtillery) +  ai->Getut()->GetNumberOfRequestedUnitsOfCategory(staticArtillery) > 0)
		return;

	if(ai->Getut()->GetNumberOfActiveUnitsOfCategory(staticArtillery) >= cfg->MAX_STAT_ARTY)
		return;

	const float temp = 0.05f;

	SetConstructionUrgencyIfHigher(EUnitCategory::STATIC_ARTILLERY, temp);
}

void AAIExecute::CheckBuildqueues()
{
	int totalQueuedUnits(0);
	int numberOfActiveFactoryTypes(0);

	const auto& factoryTable = ai->s_buildTree.GetFactoryDefIdLookupTable();

	for(int factoryId = 0; factoryId < factoryTable.size(); ++factoryId)
	{
		if(ai->Getbt()->units_dynamic[ factoryTable[factoryId].id ].active > 0)
		{
			totalQueuedUnits += static_cast<int>(m_buildqueues[factoryId].size());
			++numberOfActiveFactoryTypes;
		}
	}

	if(numberOfActiveFactoryTypes > 0)
	{
		const float queuedUnitsPerFactoryType = static_cast<float>(totalQueuedUnits) / static_cast<float>(numberOfActiveFactoryTypes);

		if(queuedUnitsPerFactoryType < 0.3f * static_cast<float>(cfg->MAX_BUILDQUE_SIZE) )
		{
			if(unitProductionRate < 70)
				++unitProductionRate;

			//ai->Log("Increasing unit production rate to %i\n", unitProductionRate);
		}
		else if( queuedUnitsPerFactoryType >  0.75f * static_cast<float>(cfg->MAX_BUILDQUE_SIZE) )
		{
			if(unitProductionRate > 1)
			{
				--unitProductionRate;
				//ai->Log("Decreasing unit production rate to %i\n", unitProductionRate);
			}
		}
	}
}

void AAIExecute::CheckDefences()
{
	if(    (ai->Getut()->activeFactories < cfg->MIN_FACTORIES_FOR_DEFENCES)
		|| (ai->Getut()->GetNumberOfFutureUnitsOfCategory(EUnitCategory::STATIC_DEFENCE) > 2) )
		return;

	const GamePhase gamePhase(ai->GetAICallback()->GetCurrentFrame());

	constexpr int maxSectorDistToBase(3);
	float highestImportance(0.0f);

	AAISector *first(nullptr), *second(nullptr);
	AAITargetType targetType1, targetType2;

	for(int dist = 1; dist <= maxSectorDistToBase; ++dist)
	{
		for(const auto sector : ai->Getbrain()->m_sectorsInDistToBase[dist])
		{
			// stop building further defences if maximum has been reached / sector contains allied buildings / is occupied by another aai instance
			AAITargetType targetType;		
			const float importance = sector->GetImportanceForStaticDefenceVs(targetType, gamePhase, learned, current);

			if(importance > highestImportance)
			{
				second = first;
				targetType2 = targetType1;

				first = sector;
				targetType1 = targetType;

				highestImportance = importance;
			}
		}
	}

	if(first)
	{
		// if no builder available retry later
		BuildOrderStatus status = BuildStationaryDefenceVS(targetType1, first);

		if(status == BuildOrderStatus::NO_BUILDER_AVAILABLE)
		{
			const float urgencyOfStaticDefence = 0.03f + 1.0f / ( static_cast<float>(first->GetNumberOfBuildings(EUnitCategory::STATIC_DEFENCE)) + 0.5f);

			SetConstructionUrgencyIfHigher(EUnitCategory::STATIC_DEFENCE, urgencyOfStaticDefence);

			m_sectorToBuildNextDefence = first;
			m_nextDefenceVsTargetType = targetType1;
		}
		else if(status == BuildOrderStatus::NO_BUILDSITE_FOUND)
			first->FailedToConstructStaticDefence();
	}

	if(second)
		BuildStationaryDefenceVS(targetType2, second);
}

void AAIExecute::CheckRessources()
{
	SetConstructionUrgencyIfHigher(EUnitCategory::METAL_EXTRACTOR, ai->Getbrain()->GetMetalUrgency());
	SetConstructionUrgencyIfHigher(EUnitCategory::POWER_PLANT,     ai->Getbrain()->GetEnergyUrgency());

	const float storageUrgency = max(ai->Getbrain()->GetMetalStorageUrgency(), ai->Getbrain()->GetEnergyStorageUrgency());
	SetConstructionUrgencyIfHigher(EUnitCategory::STORAGE, storageUrgency);

	// energy low
	if(ai->Getbrain()->GetAveragEnergySurplus() < 0.1f * ai->GetAICallback()->GetEnergyIncome())
	{
		// try to accelerate power plant construction
		const AAIUnitCategory plant(EUnitCategory::POWER_PLANT);
		if(ai->Getut()->GetNumberOfUnitsUnderConstructionOfCategory(plant) > 0)
			AssistConstructionOfCategory(plant);

		// try to disbale some metal makers
		if((ai->Getut()->GetNumberOfActiveUnitsOfCategory(AAIUnitCategory(EUnitCategory::METAL_MAKER)) - disabledMMakers) > 0)
		{
			for(set<int>::iterator maker = ai->Getut()->metal_makers.begin(); maker != ai->Getut()->metal_makers.end(); ++maker)
			{
				if(ai->GetAICallback()->IsUnitActivated(*maker))
				{
					Command c(CMD_ONOFF);
					c.PushParam(0);
					//ai->Getcb()->GiveOrder(*maker, &c);
					GiveOrder(&c, *maker, "ToggleMMaker");

					++disabledMMakers;
					break;
				}
			}
		}
	}
	// try to enable some metal makers
	else if(ai->Getbrain()->GetAveragEnergySurplus() > cfg->MIN_METAL_MAKER_ENERGY && disabledMMakers > 0)
	{
		for(set<int>::iterator maker = ai->Getut()->metal_makers.begin(); maker != ai->Getut()->metal_makers.end(); ++maker)
		{
			if(!ai->GetAICallback()->IsUnitActivated(*maker))
			{
				float usage = ai->GetAICallback()->GetUnitDef(*maker)->energyUpkeep;

				if(ai->Getbrain()->GetAveragEnergySurplus() > usage * 0.7f)
				{
					Command c(CMD_ONOFF);
					c.PushParam(1);
					//ai->Getcb()->GiveOrder(*maker, &c);
					GiveOrder(&c, *maker, "ToggleMMaker");

					--disabledMMakers;
					break;
				}
			}
		}
	}

	// metal low
	if(ai->Getbrain()->GetAverageMetalSurplus() < AAIConstants::minMetalSurplusForConstructionAssist)
	{
		// try to accelerate mex construction
		const AAIUnitCategory extractor(EUnitCategory::METAL_EXTRACTOR);
		if(ai->Getut()->GetNumberOfUnitsUnderConstructionOfCategory(extractor) > 0)
			AssistConstructionOfCategory(extractor);

		// try to accelerate mex construction
		const AAIUnitCategory metalMaker(EUnitCategory::METAL_MAKER);
		if( (ai->Getut()->GetNumberOfUnitsUnderConstructionOfCategory(metalMaker) > 0) && (ai->Getbrain()->GetAveragEnergySurplus() > cfg->MIN_METAL_MAKER_ENERGY) )
			AssistConstructionOfCategory(metalMaker);
	}
}

void AAIExecute::CheckMexUpgrade()
{
	//-----------------------------------------------------------------------------------------------------------------
	// skip check for extarctor upgrade if there are empty metal spots or extractors under construction
	//-----------------------------------------------------------------------------------------------------------------
	for(auto sector : ai->Getbrain()->m_sectorsInDistToBase[0])
	{
		for(auto spot : sector->metalSpots)
		{
			if(spot->occupied == false)
				return;
		}
	}

	if(ai->Getut()->GetNumberOfFutureUnitsOfCategory(EUnitCategory::METAL_EXTRACTOR) > 0)
		return;

	//-----------------------------------------------------------------------------------------------------------------
	// determine which type of extractor could be build on land/sea
	//-----------------------------------------------------------------------------------------------------------------
	const float cost = 0.25f + ai->Getbrain()->Affordable() / 8.0f;
	const float extractedMetal  = 6.0f / (cost + 0.75f);

	const UnitDefId landExtractor = ai->Getbt()->SelectExtractor(ai->GetSide(), cost, extractedMetal, false, false);
	const UnitDefId seaExtractor  = ai->Getbt()->SelectExtractor(ai->GetSide(), cost, extractedMetal, false, true);

	const float landExtractedMetal = landExtractor.IsValid() ? ai->s_buildTree.GetMaxRange(landExtractor) : 0.0f;
	const float seaExtractedMetal  = seaExtractor.IsValid()  ? ai->s_buildTree.GetMaxRange(seaExtractor)  : 0.0f;

	//-----------------------------------------------------------------------------------------------------------------
	// check existing extractors within/close to base for possible upgrade
	//-----------------------------------------------------------------------------------------------------------------
	float maxExtractedMetalGain(0.0f);
	AAIMetalSpot *selectedMetalSpot = nullptr;

	for(int dist = 0; dist < 2; ++dist)
	{
		for(auto sector : ai->Getbrain()->m_sectorsInDistToBase[dist])
		{
			for(auto spot : sector->metalSpots)
			{
				// quit when finding empty spots
				if(!spot->occupied && (sector->GetNumberOfEnemyBuildings() <= 0) && (sector->GetLostUnits() < 0.2f) )
					return;

				if(    spot->extractorDefId.IsValid() 
				    && spot->extractorUnitId.IsValid()
					&& ai->GetAICallback()->GetUnitTeam(spot->extractorUnitId.id) == ai->GetMyTeamId())	// only upgrade own extractors
				{
					const bool isLand = ai->s_buildTree.GetMovementType( spot->extractorDefId ).IsStaticLand();

					const float extractedMetalGain =  (isLand ? landExtractedMetal : seaExtractedMetal) 
													- ai->s_buildTree.GetMaxRange( spot->extractorDefId );

					if( (extractedMetalGain > 0.0001f) && (extractedMetalGain > maxExtractedMetalGain) )
					{
						maxExtractedMetalGain = extractedMetalGain;
						selectedMetalSpot     = spot;
					}
				}
			}
		}
	}
	
	//-----------------------------------------------------------------------------------------------------------------
	// order builder to reclaim exctractor which shall be upgraded
	//-----------------------------------------------------------------------------------------------------------------
	if(selectedMetalSpot)
	{
		AAIConstructor *builder = ai->Getut()->FindClosestAssistant(selectedMetalSpot->pos, 10, true);

		if(builder)
			builder->GiveReclaimOrder(selectedMetalSpot->extractorUnitId);
	}
}


void AAIExecute::CheckRadarUpgrade()
{
	if(ai->Getut()->GetNumberOfFutureUnitsOfCategory(AAIUnitCategory(EUnitCategory::STATIC_SENSOR))  > 0)
		return;

	const float cost = ai->Getbrain()->Affordable();
	const float range = 10.0f / (cost + 1.0f);

	// check all existing sensors for upgrades
	for(auto sensor : ai->Getut()->GetStaticSensors())
	{
		const UnitDefId sensorDefId = ai->Getut()->GetUnitDefId(sensor);
		const bool water = ai->s_buildTree.GetMovementType(sensorDefId).IsStaticSea();

		const UnitDefId upgradedSensor = ai->Getbt()->SelectRadar(ai->GetSide(), cost, range, water);
		
		const bool upgrade = upgradedSensor.IsValid() && (ai->s_buildTree.GetMaxRange(sensorDefId) < ai->s_buildTree.GetMaxRange(upgradedSensor));

		if(upgrade)
		{
			// better radar found, clear buildpos
			AAIConstructor *builder = ai->Getut()->FindClosestAssistant(ai->GetAICallback()->GetUnitPos(sensor.id), 10, true);

			if(builder)
			{
				builder->GiveReclaimOrder(sensor);
				return;
			}
		}
	}
}

void AAIExecute::CheckJammerUpgrade()
{
	/*if(ai->Getut()->futureUnits[STATIONARY_JAMMER] + ai->Getut()->requestedUnits[STATIONARY_JAMMER]  > 0)
		return;

	float cost = ai->Getbrain()->Affordable();
	float range = 10.0 / (cost + 1);

	const UnitDef *my_def;
	const UnitDef *land_def = 0;
	const UnitDef *water_def = 0;

	int land_jammer = ai->Getbt()->GetJammer(ai->Getside(), cost, range, false, true);
	int water_jammer = ai->Getbt()->GetJammer(ai->Getside(), cost, range, true, true);

	if(land_jammer)
		land_def = &ai->Getbt()->GetUnitDef(land_jammer);

	if(water_jammer)
		water_def = &ai->Getbt()->GetUnitDef(water_jammer);

	// check jammer upgrades
	for(set<int>::iterator jammer = ai->Getut()->jammers.begin(); jammer != ai->Getut()->jammers.end(); ++jammer)
	{
		my_def = ai->Getcb()->GetUnitDef(*jammer);

		if(my_def)
		{
			if(my_def->minWaterDepth <= 0)	// land jammer
			{
				if(land_def && my_def->jammerRadius < land_def->jammerRadius)
				{
					// better jammer found, clear buildpos
					AAIConstructor *builder = ai->Getut()->FindClosestAssistant(ai->Getcb()->GetUnitPos(*jammer), 10, true);

					if(builder)
					{
						builder->GiveReclaimOrder( UnitId(*jammer) );
						return;
					}
				}
			}
			else	// water jammer
			{
				if(water_def && my_def->jammerRadius < water_def->jammerRadius)
				{
					// better radar found, clear buildpos
					AAIConstructor *builder = ai->Getut()->FindClosestAssistant(ai->Getcb()->GetUnitPos(*jammer), 10, true);

					if(builder)
					{
						builder->GiveReclaimOrder( UnitId(*jammer) );
						return;
					}
				}
			}
		}
	}*/
}

void AAIExecute::CheckFactories()
{
	if(ai->Getut()->GetNumberOfFutureUnitsOfCategory(AAIUnitCategory(EUnitCategory::STATIC_CONSTRUCTOR)) > 0)
		return;

	for(auto factory : ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, ai->GetSide()) )
	{
		if(ai->Getbt()->units_dynamic[factory.id].requested > 0)
		{
			// at least one requested factory has not been built yet
			const float urgency = (ai->Getut()->activeFactories > 0) ? 0.5f : 3.5f;

			SetConstructionUrgencyIfHigher(EUnitCategory::STATIC_CONSTRUCTOR, urgency);

			return;
		}
	}
}

void AAIExecute::CheckRecon()
{
	float radarUrgency(0.0f);
	
	// do not build radar before at least one factory is finished.
	if(ai->Getut()->GetNumberOfActiveUnitsOfCategory(EUnitCategory::STATIC_CONSTRUCTOR) > 0)
		radarUrgency = 0.02f + 0.5f / ((float)(2 * ai->Getut()->GetNumberOfActiveUnitsOfCategory(EUnitCategory::STATIC_SENSOR) + 1));

	SetConstructionUrgencyIfHigher(EUnitCategory::STATIC_SENSOR, radarUrgency);
}

struct CompareConstructionUrgency
{
	bool operator()(const std::pair<int, float>& lhs, const std::pair<int, float>& rhs) const
	{
		return lhs.second > rhs.second;
	}
};

void AAIExecute::CheckConstruction()
{
	float highestUrgency(0.5f);		// min urgency (prevents aai from building things it doesnt really need that much)
	AAIUnitCategory buildingCategory;

	std::set< std::pair<int, float>, CompareConstructionUrgency> categoriesToBeChecked;

	// ----------------------------------------------------------------------------------------------------------------
	// determine category with highest urgency
	// ----------------------------------------------------------------------------------------------------------------
	for(int i = 0; i < m_constructionUrgency.size(); ++i)
	{
		m_constructionUrgency[i] *= 1.02f;

		if(m_constructionUrgency[i] > 20.0f)
			m_constructionUrgency[i] -= 1.0f;

		if(m_constructionUrgency[i] > 2.5f)
		{
			categoriesToBeChecked.insert( std::pair<int, float>(i, m_constructionUrgency[i]) );
		}
		else if(m_constructionUrgency[i] > highestUrgency)
		{
			highestUrgency = m_constructionUrgency[i];
			buildingCategory = static_cast<EUnitCategory>(i);
		}
	}

	// ----------------------------------------------------------------------------------------------------------------
	// check construction for selected building categories
	// ----------------------------------------------------------------------------------------------------------------
	if(categoriesToBeChecked.size() > 0)
	{
		for(auto category = categoriesToBeChecked.begin(); category != categoriesToBeChecked.end(); ++category)
		{
			TryConstruction(static_cast<EUnitCategory>(category->first));
		}
	}
	else if(buildingCategory.IsValid())
	{
		TryConstruction(buildingCategory);
	}
}

void AAIExecute::TryConstruction(const AAIUnitCategory& category)
{
	bool (AAIExecute::*constructionFunction) () = m_constructionFunctions[category.GetArrayIndex()];

	bool constructionStarted(false);

	if(constructionFunction != nullptr)
		constructionStarted = (this->*constructionFunction)();
	else
		constructionStarted = true;
	
	if(constructionStarted)
		m_constructionUrgency[category.GetArrayIndex()] = 0.0f;
}

bool AAIExecute::AssistConstructionOfCategory(const AAIUnitCategory& category)
{
	for(auto task : ai->GetBuildTasks())
	{
		AAIConstructor *builder = task->GetConstructor(ai->Getut());

		if(   (builder != nullptr) 
		   && (builder->GetCategoryOfConstructedUnit() == category)
		   && (builder->assistants.size() < cfg->MAX_ASSISTANTS) )
		{
			AAIConstructor* assistant = ai->Getut()->FindClosestAssistant(builder->GetBuildPos(), 5, true);

			if(assistant)
			{
				builder->assistants.insert(assistant->m_myUnitId.id);
				assistant->AssistConstruction(builder->m_myUnitId);
				return true;
			}
		}
	}

	return false;
}

float AAIExecute::sector_threat(const AAISector *sector)
{
	const float threat = sector->GetLocalAttacksBy(ETargetType::SURFACE, learned, current)
					   + sector->GetLocalAttacksBy(ETargetType::AIR, learned, current)
					   + sector->GetLocalAttacksBy(ETargetType::FLOATER, learned, current)
					   + sector->GetLocalAttacksBy(ETargetType::SUBMERGED, learned, current);
	return threat;
}

bool AAIExecute::least_dangerous(const AAISector *left, const AAISector *right)
{
	return sector_threat(left) < sector_threat(right);
}

bool AAIExecute::suitable_for_power_plant(AAISector *left, AAISector *right)
{
	return sector_threat(left) * static_cast<float>( left->GetEdgeDistance() ) < sector_threat(right) * static_cast<float>( right->GetEdgeDistance() );
}

bool AAIExecute::suitable_for_ground_factory(AAISector *left, AAISector *right)
{
	return ( (2.0f * left->GetFlatTilesRatio()  + static_cast<float>( left->GetEdgeDistance() ))
		   > (2.0f * right->GetFlatTilesRatio() + static_cast<float>( right->GetEdgeDistance() )) );
}

bool AAIExecute::suitable_for_sea_factory(AAISector *left, AAISector *right)
{
	return ( (2.0f * left->GetWaterTilesRatio()  + static_cast<float>( left->GetEdgeDistance() ))
		   > (2.0f * right->GetWaterTilesRatio() + static_cast<float>( right->GetEdgeDistance() )) );
}

bool AAIExecute::defend_vs_ground(const AAISector *left, const AAISector *right)
{
	return ((2.0f + left->GetLocalAttacksBy(ETargetType::SURFACE, learned, current)) / (left->GetFriendlyStaticDefencePower(ETargetType::SURFACE) + 0.5f))
		>  ((2.0f + right->GetLocalAttacksBy(ETargetType::SURFACE, learned, current)) / (right->GetFriendlyStaticDefencePower(ETargetType::SURFACE) + 0.5f));
}

bool AAIExecute::defend_vs_air(const AAISector *left, const AAISector *right)
{
	return ((2.0f + left->GetLocalAttacksBy(ETargetType::AIR, learned, current)) / (left->GetFriendlyStaticDefencePower(ETargetType::AIR) + 0.5f))
		>  ((2.0f + right->GetLocalAttacksBy(ETargetType::AIR, learned, current)) / (right->GetFriendlyStaticDefencePower(ETargetType::AIR) + 0.5f));
}

bool AAIExecute::defend_vs_hover(const AAISector *left, const AAISector *right)
{
	return ((2.0f + left->GetLocalAttacksBy(ETargetType::SURFACE, learned, current)) / (left->GetFriendlyStaticDefencePower(ETargetType::SURFACE) + 0.5f))
		>  ((2.0f + right->GetLocalAttacksBy(ETargetType::SURFACE, learned, current)) / (right->GetFriendlyStaticDefencePower(ETargetType::SURFACE) + 0.5f));
}

bool AAIExecute::defend_vs_sea(const AAISector *left, const AAISector *right)
{
	return ((2.0f + left->GetLocalAttacksBy(ETargetType::FLOATER, learned, current)) / (left->GetFriendlyStaticDefencePower(ETargetType::FLOATER) + 0.5f))
		>  ((2.0f + right->GetLocalAttacksBy(ETargetType::FLOATER, learned, current)) / (right->GetFriendlyStaticDefencePower(ETargetType::FLOATER) + 0.5f));
}

bool AAIExecute::defend_vs_submarine(const AAISector *left, const AAISector *right)
{
	return ((2.0f + left->GetLocalAttacksBy(ETargetType::SUBMERGED, learned, current)) / (left->GetFriendlyStaticDefencePower(ETargetType::SUBMERGED) + 0.5f))
		>  ((2.0f + right->GetLocalAttacksBy(ETargetType::SUBMERGED, learned, current)) / (right->GetFriendlyStaticDefencePower(ETargetType::SUBMERGED) + 0.5f));
}

void AAIExecute::ConstructionFailed(float3 build_pos, UnitDefId unitDefId)
{
	const springLegacyAI::UnitDef *def = &ai->Getbt()->GetUnitDef(unitDefId.id);
	const AAIUnitCategory category = ai->s_buildTree.GetUnitCategory(unitDefId);

	const int  x = build_pos.x/ai->Getmap()->xSectorSize;
	const int  y = build_pos.z/ai->Getmap()->ySectorSize;
	const bool validSector = ai->Getmap()->IsValidSector(x, y);

	// decrease number of units of that category in the target sector
	if(validSector)
		ai->Getmap()->m_sector[x][y].RemoveBuilding(category);

	// free metalspot if mex was odered to be built
	if(category.IsMetalExtractor())
	{
		if(validSector)
			ai->Getmap()->m_sector[x][y].FreeMetalSpot(build_pos, def);
	}
	else if(category.IsStaticDefence())
	{
		ai->Getmap()->AddOrRemoveStaticDefence(build_pos, unitDefId, false);
	}
	else if(category.IsStaticConstructor())
	{
		ai->Getut()->futureFactories -= 1;

		ai->Getbt()->UnfinishedConstructorKilled(unitDefId);
	}

	// update buildmap of sector
	ai->Getmap()->UpdateBuildMap(build_pos, def, false);
}

AAIGroup* AAIExecute::GetClosestGroupForDefence(const AAITargetType& attackerTargetType, const float3& pos, int importance) const
{
	const int continentId = AAIMap::GetContinentID(pos);

	AAIGroup *selectedGroup(nullptr);
	float highestRating(0.0f);

	for(auto category = ai->s_buildTree.GetCombatUnitCatgegories().begin(); category != ai->s_buildTree.GetCombatUnitCatgegories().end(); ++category)
	{
		for(auto group = ai->GetUnitGroupsList(*category).begin(); group != ai->GetUnitGroupsList(*category).end(); ++group)
		{
			const float rating = (*group)->GetDefenceRating(attackerTargetType, pos, importance, continentId);
			
			if(rating > highestRating)
			{
				selectedGroup = *group;
				highestRating = rating;
			}
		}
	}

	return selectedGroup;
}

void AAIExecute::DefendUnitVS(const UnitId& unitId, const AAITargetType& attackerTargetType, const float3& attackerPosition, int importance) const
{
	AAISector* sector = ai->Getmap()->GetSectorOfPos(attackerPosition);

	if(sector)
	{
		ai->Getmap()->CheckUnitsInLOSUpdate();

		if(sector->IsSupportNeededToDefenceVs(attackerTargetType))
		{
			AAIGroup *support = GetClosestGroupForDefence(attackerTargetType, attackerPosition, importance);

			if(support)
				support->Defend(unitId, attackerPosition, importance);
		}
	}
}

float3 AAIExecute::DetermineSafePos(UnitDefId unitDefId, float3 unit_pos) const
{
	float3 selectedPosition(ZeroVector);
	float highestRating(-10000.0f);

	const AAIMovementType moveType = ai->s_buildTree.GetMovementType(unitDefId);
	if( moveType.CannotMoveToOtherContinents() )
	{
		// get continent id of the unit pos
		const int continentId = AAIMap::GetContinentID(unit_pos);

		for(std::list<AAISector*>::iterator sector = ai->Getbrain()->m_sectorsInDistToBase[0].begin(); sector != ai->Getbrain()->m_sectorsInDistToBase[0].end(); ++sector)
		{
			//! @todo Implement more refined selection
			const float3 pos = (*sector)->DetermineUnitMovePos(moveType, continentId);

			if(pos.x > 0.0f)
			{
				const float rating = static_cast<float>( (*sector)->GetEdgeDistance() ) - (*sector)->GetEnemyCombatPower(ai->s_buildTree.GetTargetType(unitDefId));

				if(rating > highestRating)
				{
					highestRating    = rating;
					selectedPosition = pos;
				}
			}
		}
	}
	else // non continent bound movement types (air, hover, amphibious)
	{
		for(std::list<AAISector*>::iterator sector = ai->Getbrain()->m_sectorsInDistToBase[0].begin(); sector != ai->Getbrain()->m_sectorsInDistToBase[0].end(); ++sector)
		{
			const float rating = static_cast<float>( (*sector)->GetEdgeDistance() ) - (*sector)->GetEnemyCombatPower(ai->s_buildTree.GetTargetType(unitDefId));

			if(rating > highestRating)
			{
				highestRating    = rating;
				selectedPosition = (*sector)->GetCenter();
			}
		}
	}

	return selectedPosition;
}

void AAIExecute::ChooseDifferentStartingSector(int x, int y)
{
	// get possible start sectors
	std::list<AAISector*> sectors;

	if(x >= 1)
	{
		sectors.push_back( &ai->Getmap()->m_sector[x-1][y] );

		if(y >= 1)
			sectors.push_back( &ai->Getmap()->m_sector[x-1][y-1] );

		if(y < ai->Getmap()->ySectors-1)
			sectors.push_back( &ai->Getmap()->m_sector[x-1][y+1] );
	}

	if(x < ai->Getmap()->xSectors-1)
	{
		sectors.push_back( &ai->Getmap()->m_sector[x+1][y] );

		if(y >= 1)
			sectors.push_back( &ai->Getmap()->m_sector[x+1][y-1] );

		if(y < ai->Getmap()->ySectors-1)
			sectors.push_back( &ai->Getmap()->m_sector[x+1][y+1] );
	}

	if(y >= 1)
		sectors.push_back( &ai->Getmap()->m_sector[x][y-1] );

	if(y < ai->Getmap()->ySectors-1)
		sectors.push_back( &ai->Getmap()->m_sector[x][y+1] );

	// choose best
	AAISector *selectedSector(nullptr);
	float highestRating(0.0f);

	for(auto sector = sectors.begin(); sector != sectors.end(); ++sector)
	{
		const float rating = (*sector)->GetRatingAsStartSector();

		if(rating > highestRating)
		{
			highestRating  = rating;
			selectedSector = *sector;
		}
	}

	// add best sector to base
	if(selectedSector)
	{
		ai->Getbrain()->AssignSectorToBase(selectedSector, true);
	}
}

void AAIExecute::CheckKeepDistanceToEnemy(UnitId unit, UnitDefId unitDefId, UnitDefId enemyDefId)
{
	const float weaponRange      = ai->s_buildTree.GetMaxRange(unitDefId);
	const float enemyWeaponRange = ai->s_buildTree.GetMaxRange(enemyDefId);

	const bool rangeOk    = (weaponRange > enemyWeaponRange + AAIConstants::minWeaponRangeDiffToKeepDistance);
	const bool turnrateOk = (ai->Getbt()->GetUnitDef(unitDefId.id).turnRate >= cfg->MIN_FALLBACK_TURNRATE);

	if(rangeOk && turnrateOk)
	{
		const float fallbackDist = std::min(1.25f * enemyWeaponRange, weaponRange);

		float3 pos = GetFallBackPos( ai->GetAICallback()->GetUnitPos(unit.id), fallbackDist);

		if(pos.x > 0.0f)
		{
			Command c(CMD_MOVE);

			c.PushParam(pos.x);
			c.PushParam(ai->GetAICallback()->GetElevation(pos.x, pos.z));
			c.PushParam(pos.z);

			//ai->Getcb()->GiveOrder(unit_id, &c);
			GiveOrder(&c, unit.id, "Fallback");
		}
	}
}

float3 AAIExecute::GetFallBackPos(const float3& pos, float maxFallbackDist) const
{
	float3 fallbackPosition(ZeroVector);

	// units without range should not end up here; this is for attacking units only
	// prevents a NaN
	assert(maxFallbackDist != 0.0f);

	// get list of enemies within weapons range
	const int numberOfEnemies = ai->GetAICallback()->GetEnemyUnits(&(ai->Getmap()->unitsInLOS.front()), pos, maxFallbackDist);

	if(numberOfEnemies > 0)
	{
		float3 enemy_pos;

		for(int k = 0; k < numberOfEnemies; ++k)
		{
			float3 enemy_pos = ai->GetAICallback()->GetUnitPos(ai->Getmap()->unitsInLOS[k]);

			// get distance to enemy
			float dx   = enemy_pos.x - pos.x;
			float dz   = enemy_pos.z - pos.z;
			float dist = fastmath::apxsqrt(dx*dx + dz*dz);

			// get dir from unit to enemy
			enemy_pos.x -= pos.x;
			enemy_pos.z -= pos.z;

			// move closer to enemy if we are out of range,
			// and away if we are closer then our max range
			fallbackPosition.x += ((dist / maxFallbackDist) - 1.0f) * enemy_pos.x;
			fallbackPosition.z += ((dist / maxFallbackDist) - 1.0f) * enemy_pos.z;
		}

		// move less if lots of enemies are close
		fallbackPosition.x /= (float)numberOfEnemies;
		fallbackPosition.z /= (float)numberOfEnemies;

		// apply relative move distance to the current position
		// to get the target position
		fallbackPosition.x += pos.x;
		fallbackPosition.z += pos.z;
	}

	return fallbackPosition;
}

void AAIExecute::GiveOrder(Command *c, int unit, const char *owner)
{
	++issued_orders;

	if(issued_orders%500 == 0)
		ai->Log("%i th order has been given by %s in frame %i\n", issued_orders, owner,  ai->GetAICallback()->GetCurrentFrame());

	ai->Getut()->units[unit].last_order = ai->GetAICallback()->GetCurrentFrame();

	ai->GetAICallback()->GiveOrder(unit, c);
}
