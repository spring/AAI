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
#include "AAIBuildTable.h"
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
float AAIExecute::current = 0.5;
float AAIExecute::learned = 2.5;


AAIExecute::AAIExecute(AAI *ai) :
	m_nextDefenceVsTargetType(ETargetType::UNKNOWN),
	m_linkingBuildTaskToBuilderFailed(0u)
{
	issued_orders = 0;

	this->ai = ai;

	unitProductionRate = 1;

	futureAvailableMetal = 0;
	futureAvailableEnergy = 0;
	averageMetalUsage = 0;
	averageEnergyUsage = 0;
	disabledMMakers = 0;

	m_sectorToBuildNextDefence = nullptr;

	for(int i = 0; i <= METAL_MAKER; ++i)
		urgency[i] = 0;
}

AAIExecute::~AAIExecute(void)
{
//	if(buildques)
//	{
//		for(int i = 0; i < numOfFactories; ++i)
//			buildques[i].clear();

//		spring::SafeDeleteArray(buildques);
//	}

//	if(factory_table)
//		spring::SafeDeleteArray(factory_table);
}


void AAIExecute::InitAI(UnitId commanderUnitId, UnitDefId commanderDefId)
{
	//debug
	ai->Log("Playing as %s\n", ai->Getbt()->sideNames[ai->GetSide()].c_str());

	if(ai->GetSide() < 1 || ai->GetSide() > ai->Getbt()->numOfSides)
	{
		ai->LogConsole("ERROR: invalid side id %i\n", ai->GetSide());
		return;
	}

	ai->Log("My team / ally team: %i / %i\n", ai->GetAICallback()->GetMyTeam(), ai->GetAICallback()->GetMyAllyTeam());

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
	if(ai->Getmap()->team_sector_map[x][y] < 0)
	{
		ai->Getbrain()->AssignSectorToBase(&ai->Getmap()->sector[x][y], true);
	}
	else
	{
		// sector already occupied by another aai team (coms starting too close to each other)
		// choose next free sector
		ChooseDifferentStartingSector(x, y);
	}

	const AAIMapType& mapType = ai->Getmap()->GetMapType();

	if(mapType.IsWaterMap())
		ai->Getbrain()->ExpandBase(WATER_SECTOR);
	else if(mapType.IsLandMap())
		ai->Getbrain()->ExpandBase(LAND_SECTOR);
	else
		ai->Getbrain()->ExpandBase(LAND_WATER_SECTOR);

	// now that we know the side, init buildques
	InitBuildques();

	ai->Getbt()->InitCombatEffCache(ai->GetSide());

	ai->Getut()->AddCommander(commanderUnitId, commanderDefId);

	// get economy working
	CheckRessources();
}

void AAIExecute::createBuildTask(UnitId unitId, UnitDefId unitDefId, float3 *pos)
{
	AAIBuildTask *task = new AAIBuildTask(ai, unitId.id, unitDefId.id, pos, ai->GetAICallback()->GetCurrentFrame());
	ai->GetBuildTasks().push_back(task);

	// find builder and associate building with that builder
	task->builder_id = -1;

	bool builderFound = false;

	for(set<int>::iterator i = ai->Getut()->constructors.begin(); i != ai->Getut()->constructors.end(); ++i)
	{
		if(ai->Getut()->units[*i].cons->IsHeadingToBuildsite() == true)
		{
			const float3& buildPos = ai->Getut()->units[*i].cons->GetBuildPos();

			/*if(ai->s_buildTree.GetUnitTypeProperties(unitDefId).m_unitCategory.isStaticDefence() == true)
			{
				ai->Log("Builtask for %s: %f %f %f %f\n", ai->s_buildTree.GetUnitTypeProperties(unitDefId).m_name.c_str(),
					buildPos.x, pos->x, buildPos.z, pos->z);
			}*/

			if((fabs(buildPos.x - pos->x) < 16.0f) && (fabs(buildPos.z - pos->z) < 16.0f))
			{
				builderFound = true;
				task->builder_id = ai->Getut()->units[*i].cons->m_myUnitId.id;
				ai->Getut()->units[*i].cons->ConstructionStarted(unitId, task);
				break;
			}
		}
	}

	if(builderFound == false)
	{
		++m_linkingBuildTaskToBuilderFailed;
		ai->Log("Failed to link buildtask for %s to builder\n", ai->s_buildTree.GetUnitTypeProperties(unitDefId).m_name.c_str() );
	}
}

bool AAIExecute::InitBuildingAt(const UnitDef *def, const float3& position)
{
	// update buildmap
	ai->Getmap()->UpdateBuildMap(position, def, true);

	// update defence map (if necessary)
	UnitDefId unitDefId(def->id);
	if(ai->s_buildTree.GetUnitCategory(unitDefId).isStaticDefence())
		ai->Getmap()->AddStaticDefence(position, unitDefId);

	// determine target sector
	const int x = position.x / ai->Getmap()->xSectorSize;
	const int y = position.z / ai->Getmap()->ySectorSize;

	// drop bad sectors (should only happen when defending mexes at the edge of the map)
	if(x >= 0 && y >= 0 && x < ai->Getmap()->xSectors && y < ai->Getmap()->ySectors)
	{
		// increase number of units of that category in the target sector
		ai->Getmap()->sector[x][y].AddBuilding(ai->s_buildTree.GetUnitCategory(unitDefId));

		return true;
	}
	else
		return false;
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
		const float3 unitPos = ai->GetAICallback()->GetUnitPos(unitDefId.id);
		continentId = ai->Getmap()->GetContinentID(unitPos);
	}

	// try to add unit to an existing group
	const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(unitDefId);
	const AAICombatUnitCategory combatUnitCategory( category );

	//ai->Log("Trying to add unit %s to group of combat category %i\n", ai->s_buildTree.GetUnitTypeProperties(unitDefId).m_name.c_str(), combatUnitCategory.GetArrayIndex());

	for(auto group = ai->GetGroupList()[category.GetArrayIndex()].begin(); group != ai->GetGroupList()[category.GetArrayIndex()].end(); ++group)
	{
		if((*group)->AddUnit(unitId, unitDefId, continentId))
		{
			ai->Getut()->units[unitId.id].group = *group;
			return;
		}
	}

	// end of grouplist has been reached and unit has not been assigned to any group
	// -> create new one

	// get continent for ground assault units, even if they are amphibious (otherwise non amphib ground units will be added no matter which continent they are on)
	if( (category.isGroundCombat())  && (continentId == -1) )  
	{
		const float3 pos = ai->GetAICallback()->GetUnitPos(unitId.id);
		continentId = ai->Getmap()->GetContinentID(pos);
	}

	AAIGroup *new_group = new AAIGroup(ai, unitDefId, continentId);

	ai->GetGroupList()[category.GetArrayIndex()].push_back(new_group);
	new_group->AddUnit(unitId, unitDefId, continentId);
	ai->Getut()->units[unitId.id].group = new_group;
}

void AAIExecute::BuildScouts()
{
	// check number of scouts and order new ones if necessary
	const AAIUnitCategory scout(EUnitCategory::SCOUT);
	if(ai->Getut()->GetTotalNumberOfUnitsOfCategory(scout) < cfg->MAX_SCOUTS)
	{
		bool availableFactoryNeeded = true;
		float cost;
		float sightRange;

		GamePhase gamePhase(ai->GetAICallback()->GetCurrentFrame());

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

		// determine movement type of scout based on map
		uint32_t suitableMovementTypes = ai->Getmap()->GetSuitableMovementTypesForMap();

		// request cloakable scouts from time to time
		bool cloaked = (rand()%5 == 1) ? true : false;
		
		UnitDefId scoutId = ai->Getbt()->selectScout(ai->GetSide(), sightRange, cost, suitableMovementTypes, 10, cloaked, availableFactoryNeeded);

		if(scoutId.isValid() == true)
		{
			bool urgent = (ai->Getut()->GetNumberOfActiveUnitsOfCategory(scout) > 1) ? false : true;

			if(AddUnitToBuildqueue(scoutId.id, 1, urgent))
			{
				ai->Getut()->UnitRequested(scout);
				++ai->Getbt()->units_dynamic[scoutId.id].requested;
			}
		}
	}
}

void AAIExecute::SendScoutToNewDest(int scout)
{
	float3 pos = ZeroVector;

	// get scout dest
	ai->Getbrain()->GetNewScoutDest(&pos, scout);

	if(pos.x > 0)
		MoveUnitTo(scout, &pos);
}

float3 AAIExecute::GetBuildsite(int builder, int building, UnitCategory /*category*/)
{
	float3 pos;
	float3 builder_pos;
	//const UnitDef *def = ai->Getbt()->GetUnitDef(building);

	// check the sector of the builder
	builder_pos = ai->GetAICallback()->GetUnitPos(builder);
	// look in the builders sector first
	int x = builder_pos.x/ai->Getmap()->xSectorSize;
	int y = builder_pos.z/ai->Getmap()->ySectorSize;

	if(ai->Getmap()->sector[x][y].distance_to_base == 0)
	{
		pos = ai->Getmap()->sector[x][y].FindBuildsite(building);

		// if suitable location found, return pos...
		if(pos.x)
			return pos;
	}

	// look in any of the base sectors
	for(list<AAISector*>::iterator s = ai->Getbrain()->sectors[0].begin(); s != ai->Getbrain()->sectors[0].end(); ++s)
	{
		pos = (*s)->FindBuildsite(building);

		// if suitable location found, return pos...
		if(pos.x)
			return pos;
	}

	return ZeroVector;
}

float3 AAIExecute::GetUnitBuildsite(int builder, int unit)
{
	float3 builder_pos = ai->GetAICallback()->GetUnitPos(builder);
	float3 pos = ZeroVector, best_pos = ZeroVector;
	float min_dist = 1000000, dist;

	for(list<AAISector*>::iterator s = ai->Getbrain()->sectors[1].begin(); s != ai->Getbrain()->sectors[1].end(); ++s)
	{
		bool water = ai->s_buildTree.GetMovementType(UnitDefId(unit)).IsSeaUnit();

		pos = (*s)->FindBuildsite(unit, water);

		if(pos.x)
		{
			dist = sqrt( pow(pos.x - builder_pos.x ,2.0f) + pow(pos.z - builder_pos.z, 2.0f) );

			if(dist < min_dist)
			{
				min_dist = dist;
				best_pos = pos;
			}
		}
	}

	return best_pos;
}

list<int>* AAIExecute::GetBuildqueueOfFactory(int def_id)
{
	for(int i = 0; i < numOfFactories; ++i)
	{
		if(factory_table[i] == def_id)
			return &buildques[i];
	}

	return 0;
}

bool AAIExecute::AddUnitToBuildqueue(UnitDefId unitDefId, int number, bool urgent)
{
	list<int> *buildqueue = 0, *temp_buildqueue = 0;

	float my_rating, best_rating = 0.0f;

	for(std::list<UnitDefId>::const_iterator fac = ai->s_buildTree.GetConstructedByList(unitDefId).begin(); fac != ai->s_buildTree.GetConstructedByList(unitDefId).end(); ++fac)
	{
		if(ai->Getbt()->units_dynamic[(*fac).id].active > 0)
		{
			temp_buildqueue = GetBuildqueueOfFactory((*fac).id);

			if(temp_buildqueue)
			{
				my_rating = (1.0f + 2.0f * (float) ai->Getbt()->units_dynamic[(*fac).id].active) / static_cast<float>(temp_buildqueue->size() + 3);

				// @todo rework criterion to reflect available buildspace instead of maptype
				if(    (ai->Getmap()->GetMapType().IsWaterMap()) 
				    && (ai->s_buildTree.GetMovementType(UnitDefId(*fac)).IsStaticSea() == false) )
					my_rating /= 10.0f;
			}
			else
				my_rating = 0.0f;
		}
		else
			my_rating = 0.0f;

		if(my_rating > best_rating)
		{
			best_rating = my_rating;
			buildqueue = temp_buildqueue;
		}
	}

	// determine position
	if(buildqueue)
	{
		if(urgent)
		{
				buildqueue->insert(buildqueue->begin(), number, unitDefId.id);
				return true;
		}
		else if(buildqueue->size() < cfg->MAX_BUILDQUE_SIZE)
		{
			buildqueue->insert(buildqueue->end(), number, unitDefId.id);
			return true;
		}
	}

	return false;
}

void AAIExecute::InitBuildques()
{
	// determine number of factories first
	numOfFactories = 0;

	int side = ai->GetSide();

	// stationary factories
	for(auto cons = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, side).begin(); cons != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, side).end(); ++cons)
	{
		if(ai->s_buildTree.GetUnitType(*cons).IsFactory())
			++numOfFactories;
	}
	// and look for all mobile factories
	for(auto cons = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, side).begin(); cons != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, side).end(); ++cons)
	{
		if(ai->s_buildTree.GetUnitType(*cons).IsFactory())
			++numOfFactories;
	}
	// and add com
	for(auto cons = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::COMMANDER, side).begin(); cons != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::COMMANDER, side).end(); ++cons)
	{
		if(ai->s_buildTree.GetUnitType(*cons).IsFactory())
			++numOfFactories;
	}

//	buildques = new list<int>[numOfFactories];
	buildques.resize(numOfFactories);

	// set up factory buildque identification
//	factory_table = new int[numOfFactories];
	factory_table.resize(numOfFactories);

	int i = 0;

	for(auto cons = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, side).begin(); cons != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, side).end(); ++cons)
	{
		if(ai->s_buildTree.GetUnitType(*cons).IsFactory())
		{
			factory_table[i] = cons->id;
			++i;
		}
	}

	for(auto cons = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, side).begin(); cons != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, side).end(); ++cons)
	{
		if(ai->s_buildTree.GetUnitType(*cons).IsFactory())
		{
			factory_table[i] = cons->id;
			++i;
		}
	}

	for(auto cons = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::COMMANDER, side).begin(); cons != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::COMMANDER, side).end(); ++cons)
	{
		if(ai->s_buildTree.GetUnitType(*cons).IsFactory())
		{
			factory_table[i] = cons->id;
			++i;
		}
	}
}

// ****************************************************************************************************
// all building functions
// ****************************************************************************************************

BuildOrderStatus AAIExecute::TryConstructionOf(UnitDefId landBuilding, UnitDefId seaBuilding, const AAISector* sector)
{
	BuildOrderStatus buildOrderStatus;

	if(sector->water_ratio < 0.15f)
	{
		buildOrderStatus = TryConstructionOf(landBuilding, sector);

	}
	else if(sector->water_ratio < 0.85f)
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
	if(building.isValid())
	{
		const float3 position = sector->FindBuildsite(building.id, false);

		if(position.x > 0)
		{
			float min_dist;
			AAIConstructor* builder = ai->Getut()->FindClosestBuilder(building.id, &position, true, &min_dist);

			if(builder)
			{
				builder->GiveConstructionOrder(building, position);

				if( ai->s_buildTree.GetUnitCategory(building).isPowerPlant() )
					futureAvailableEnergy += ai->s_buildTree.GetPrimaryAbility(building);

				return BuildOrderStatus::SUCCESSFUL;
			}
			else
			{
				ai->Getbt()->BuildBuilderFor(building);
				return BuildOrderStatus::NO_BUILDER_AVAILABLE;
			}
		}
		else
		{
			if(ai->s_buildTree.GetMovementType(building).IsStaticLand() )
				ai->Getbrain()->ExpandBase(LAND_SECTOR);
			else
				ai->Getbrain()->ExpandBase(WATER_SECTOR);

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

		if(landExtractor.isValid())
		{
			AAIConstructor* land_builder  = ai->Getut()->FindBuilder(landExtractor.id, true);

			if(land_builder)
			{
				float3 pos = GetBuildsite(land_builder->m_myUnitId.id, landExtractor.id, EXTRACTOR);

				if(pos.x != 0)
					land_builder->GiveConstructionOrder(landExtractor, pos);

				return true;
			}
			else
			{
				ai->Getbt()->BuildBuilderFor(landExtractor);
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

		if(landExtractor.isValid())
			land_builder = ai->Getut()->FindBuilder(landExtractor.id, true);
	}

	if(ai->Getmap()->water_metal_spots > 0)
	{
		seaExtractor = ai->Getbt()->SelectExtractor(ai->GetSide(), cost, efficiency, false, true);

		if(seaExtractor.isValid())
			water_builder = ai->Getut()->FindBuilder(seaExtractor.id, true);
	}

	// check if there is any builder for at least one of the selected extractors available
	if(!land_builder && !water_builder)
		return false;

	// check the first 10 free spots for the one with least distance to available builder
	const int maxExtractorBuildSpots = 10;
	std::list<PossibleSpotForMetalExtractor> extractorSpots;

	// determine max search dist - prevent crashes on smaller maps
	int max_search_dist = min(cfg->MAX_MEX_DISTANCE, static_cast<int>(ai->Getbrain()->sectors.size()) );
	float min_dist;

	bool freeMetalSpotFound = false;
	
	for(int distanceFromBase = 0; distanceFromBase < max_search_dist; ++distanceFromBase)
	{
		//! @todo Fix possible wrong value if metal spots are skipped because enemy units are within base sector
		if(distanceFromBase == 1)
			ai->Getbrain()->m_freeMetalSpotsInBase = false;

		for(auto sector = ai->Getbrain()->sectors[distanceFromBase].begin(); sector != ai->Getbrain()->sectors[distanceFromBase].end(); ++sector)
		{
			if(    (*sector)->freeMetalSpots 
				&& !ai->Getmap()->IsAlreadyOccupiedByOtherAAI(*sector)
				&& !(*sector)->IsOccupiedByEnemies() )		
			{
				for(auto spot = (*sector)->metalSpots.begin(); spot != (*sector)->metalSpots.end(); ++spot)
				{
					if(!(*spot)->occupied)
					{
						freeMetalSpotFound = true;

						UnitDefId extractor = ((*spot)->pos.y >= 0) ? landExtractor : seaExtractor;

						AAIConstructor* builder = ai->Getut()->FindClosestBuilder(extractor.id, &(*spot)->pos, ai->Getbrain()->CommanderAllowedForConstructionAt(*sector, &(*spot)->pos), &min_dist);

						if(builder)
							extractorSpots.push_back(PossibleSpotForMetalExtractor(*spot, builder, min_dist));

						if(extractorSpots.size() >= maxExtractorBuildSpots)
							break;
					}
				}
			}

			if(extractorSpots.size() >= maxExtractorBuildSpots)
				break;
		}

		// stop looking for metal spots further away from base if already one found
		if( (distanceFromBase > 1) && (extractorSpots.size() > 0) )
			break;
	}

	// look for spot with minimum dist to available builder
	if(extractorSpots.size() > 0)
	{
		PossibleSpotForMetalExtractor& bestSpot = *(extractorSpots.begin());

		float minDistanceToClosestBuilder = (extractorSpots.begin())->m_distanceToClosestBuilder;

		for(auto spot = extractorSpots.begin(); spot != extractorSpots.end(); ++spot)
		{
			if(spot->m_distanceToClosestBuilder < min_dist)
			{
				bestSpot = *spot;
				min_dist = spot->m_distanceToClosestBuilder;
			}
		}

		// order mex construction for best spot
		const UnitDefId& extractor = (bestSpot.m_metalSpot->pos.y < 0.0f) ? seaExtractor : landExtractor;

		bestSpot.m_builder->GiveConstructionOrder(extractor, bestSpot.m_metalSpot->pos);
		bestSpot.m_metalSpot->occupied = true;

		return true;	
	}

	// dont build other things if construction could not be started due to unavailable builders
	if(freeMetalSpotFound)
		return false;
	else
		return true;
}

bool AAIExecute::BuildPowerPlant()
{
	const AAIUnitCategory plant(EUnitCategory::POWER_PLANT);

	if(ai->Getut()->GetNumberOfFutureUnitsOfCategory(plant) > 1)
		return true;
	else if(ai->Getut()->GetNumberOfUnitsUnderConstructionOfCategory(plant) <= 0 && ai->Getut()->GetNumberOfRequestedUnitsOfCategory(plant) > 0)
		return true;
	else if(ai->Getut()->GetNumberOfUnitsUnderConstructionOfCategory(plant) > 0)
	{
		// try to assist construction of other power plants first
		AAIConstructor *builder;

		for(list<AAIBuildTask*>::iterator task = ai->GetBuildTasks().begin(); task != ai->GetBuildTasks().end(); ++task)
		{
			if((*task)->builder_id >= 0)
				builder = ai->Getut()->units[(*task)->builder_id].cons;
			else
				builder = 0;

			// find the power plant that is already under construction
			if(builder && builder->GetCategoryOfConstructedUnit().isPowerPlant() == true)
			{
				// dont build further power plants if already building an expensive plant
				const StatisticalData& costStatistics = ai->s_buildTree.GetUnitStatistics(ai->GetSide()).GetUnitCostStatistics(AAIUnitCategory(EUnitCategory::POWER_PLANT));
				if(ai->s_buildTree.GetTotalCost(builder->m_constructedDefId) > costStatistics.GetAvgValue() )
					return true;

				// try to assist
				if(builder->assistants.size() < cfg->MAX_ASSISTANTS)
				{
					AAIConstructor *assistant = ai->Getut()->FindClosestAssistant(builder->GetBuildPos(), 5, true);

					if(assistant)
					{
						builder->assistants.insert(assistant->m_myUnitId.id);
						assistant->AssistConstruction(builder->m_myUnitId);
						return true;
					}
					else
						return false;
				}
			}
		}

		// power plant construction has not started -> builder is still on its way to construction site, wait until starting a new power plant
		return false;
	}
	else if(ai->Getut()->activeFactories < 1 && ai->Getut()->GetNumberOfActiveUnitsOfCategory(AAIUnitCategory(EUnitCategory::POWER_PLANT)) >= 2)
		return true;

	const float current_energy = ai->GetAICallback()->GetEnergyIncome();

	// stop building power plants if already to much available energy
	if(current_energy > 1.5f * ai->GetAICallback()->GetEnergyUsage() + 200.0f)
		return true;

	// sort sectors according to threat level
	learned = 70000.0f / (float)(ai->GetAICallback()->GetCurrentFrame() + 35000) + 1.0f;
	current = 2.5f - learned;

	if(ai->Getut()->GetNumberOfActiveUnitsOfCategory(plant) >= 2)
		ai->Getbrain()->sectors[0].sort(suitable_for_power_plant);

	const AAIUnitStatistics& unitStatistics      = ai->s_buildTree.GetUnitStatistics(ai->GetSide());
	const StatisticalData&   generatedPowerStats = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::POWER_PLANT);

	float cost( 1.5f );
	float buildtime( 1.5f );
	float generatedPower( 0.5f );

	// check if already one power_plant under construction and energy short
	if(    (ai->Getut()->GetNumberOfFutureUnitsOfCategory(plant) > 0) 
		&& (ai->Getut()->GetNumberOfActiveUnitsOfCategory(plant) > 6) 
		&& (ai->Getbrain()->GetAveragEnergySurplus() < generatedPowerStats.GetMinValue()) )
	{
		buildtime = 3.0f;
	}
	else if(ai->Getut()->GetNumberOfActiveUnitsOfCategory(plant) > 9)
	{
		cost           = 0.75f;
		buildtime      = 0.5f;
		generatedPower = 2.0f;
	}
	else if(ai->Getut()->GetNumberOfActiveUnitsOfCategory(plant) > 4)
	{
		cost           = 1.25f;
		buildtime      = 1.0f;
		generatedPower = 1.0f;
	}

	// get water and ground plant
	UnitDefId landPowerPlant = ai->Getbt()->SelectPowerPlant(ai->GetSide(), cost, buildtime, generatedPower, false);
	UnitDefId seaPowerPlant  = ai->Getbt()->SelectPowerPlant(ai->GetSide(), cost, buildtime, generatedPower, true);

	for(auto sector = ai->Getbrain()->sectors[0].begin(); sector != ai->Getbrain()->sectors[0].end(); ++sector)
	{
		BuildOrderStatus buildOrderStatus = TryConstructionOf(landPowerPlant, seaPowerPlant, *sector);

		// only continue with search in next sector if buildOrderStatus == BuildOrderStatus::NO_BUILDSITE_FOUND - otherwise stop
		if(    (buildOrderStatus == BuildOrderStatus::SUCCESSFUL)
			|| (buildOrderStatus == BuildOrderStatus::BUILDING_INVALID) )
			return true; // construction order given or no storage constructable at the moment -> continue with construction of other buidlings before retry
		else if(buildOrderStatus == BuildOrderStatus::NO_BUILDER_AVAILABLE )
			return false; 	// stop looking for buildsite in next sector and retry next update if no builder is currently available	
	}

	return true;
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

	ai->Getbrain()->sectors[0].sort(least_dangerous);

	for(list<AAISector*>::iterator sector = ai->Getbrain()->sectors[0].begin(); sector != ai->Getbrain()->sectors[0].end(); ++sector)
	{
		if((*sector)->water_ratio < 0.15)
		{
			checkWater = false;
			checkGround = true;
		}
		else if((*sector)->water_ratio < 0.85)
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
			if(maker.isValid() && ai->Getbt()->units_dynamic[maker.id].constructorsAvailable <= 0)
			{
				if(ai->Getbt()->units_dynamic[maker.id].constructorsRequested <= 0)
					ai->Getbt()->BuildBuilderFor(maker);

				maker = ai->Getbt()->GetMetalMaker(ai->GetSide(), cost, efficiency, metal, urgency, false, true);
			}

			if(maker.isValid())
			{
				pos = (*sector)->FindBuildsite(maker.id, false);

				if(pos.x > 0)
				{
					float min_dist;
					builder = ai->Getut()->FindClosestBuilder(maker.id, &pos, true, &min_dist);

					if(builder)
					{
						builder->GiveConstructionOrder(maker, pos);
						return true;
					}
					else
					{
						ai->Getbt()->BuildBuilderFor(maker);
						return false;
					}
				}
				else
				{
					ai->Getbrain()->ExpandBase(LAND_SECTOR);
					ai->Log("Base expanded by BuildMetalMaker()\n");
				}
			}
		}

		if(checkWater)
		{
			UnitDefId maker = ai->Getbt()->GetMetalMaker(ai->GetSide(), ai->Getbrain()->Affordable(),  8.0/(urgency+2.0), 64.0/(16*urgency+2.0), urgency, true, false);

			// currently aai cannot build this building
			if(maker.isValid() && ai->Getbt()->units_dynamic[maker.id].constructorsAvailable <= 0)
			{
				if(ai->Getbt()->units_dynamic[maker.id].constructorsRequested <= 0)
					ai->Getbt()->BuildBuilderFor(maker);

				maker = ai->Getbt()->GetMetalMaker(ai->GetSide(), ai->Getbrain()->Affordable(),  8.0/(urgency+2.0), 64.0/(16*urgency+2.0), urgency, true, true);
			}

			if(maker.isValid())
			{
				pos = (*sector)->FindBuildsite(maker.id, true);

				if(pos.x > 0)
				{
					float min_dist;
					builder = ai->Getut()->FindClosestBuilder(maker.id, &pos, true, &min_dist);

					if(builder)
					{
						builder->GiveConstructionOrder(maker, pos);
						return true;
					}
					else
					{
						ai->Getbt()->BuildBuilderFor(maker);
						return false;
					}
				}
				else
				{
					ai->Getbrain()->ExpandBase(WATER_SECTOR);
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
	if(	   (ai->Getut()->GetNumberOfUnitsUnderConstructionOfCategory(storage) + ai->Getut()->GetNumberOfRequestedUnitsOfCategory(storage) > 0) 
		|| (ai->Getut()->GetNumberOfActiveUnitsOfCategory(storage) >= cfg->MAX_STORAGE) )
		return true;

	if(ai->Getut()->activeFactories < 2)
		return true;

	bool checkWater, checkGround;
	AAIConstructor *builder;
	float3 pos;

	float metal  = 4.0f / (ai->GetAICallback()->GetMetalStorage()  - ai->GetAICallback()->GetMetal()  + 1.0f);
	float energy = 4.0f / (ai->GetAICallback()->GetEnergyStorage() - ai->GetAICallback()->GetEnergy() + 1.0f);

	const float cost = (ai->Getut()->GetNumberOfActiveUnitsOfCategory(storage) < 1) ? 1.5f : 0.75f;
	const float buildtime (cost); 

	UnitDefId landStorage = ai->Getbt()->SelectStorage(ai->GetSide(), cost, buildtime, metal, energy, false);
	UnitDefId seaStorage  = ai->Getbt()->SelectStorage(ai->GetSide(), cost, buildtime, metal, energy, true);

	for(auto sector = ai->Getbrain()->sectors[0].begin(); sector != ai->Getbrain()->sectors[0].end(); ++sector)
	{
		BuildOrderStatus buildOrderStatus = TryConstructionOf(landStorage, seaStorage, *sector);

		// only continue with search in next sector if buildOrderStatus == BuildOrderStatus::NO_BUILDSITE_FOUND - otherwise stop
		if(    (buildOrderStatus == BuildOrderStatus::SUCCESSFUL)
			|| (buildOrderStatus == BuildOrderStatus::BUILDING_INVALID) )
			return true; // construction order given or no storage constructable at the moment -> continue with construction of other buidlings before retry
		else if(buildOrderStatus == BuildOrderStatus::NO_BUILDER_AVAILABLE )
			return false; 	// stop looking for buildsite in next sector and retry next update if no builder is currently available	
	}

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
		if((*sector)->water_ratio < 0.15)
		{
			checkWater = false;
			checkGround = true;
		}
		else if((*sector)->water_ratio < 0.85)
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
		++m_sectorToBuildNextDefence->failed_defences;

	m_sectorToBuildNextDefence = nullptr;

	return true;
}

BuildOrderStatus AAIExecute::BuildStationaryDefenceVS(const AAITargetType& targetType, const AAISector *dest)
{
	// dont build in sectors already occupied by allies
	if(dest->GetNumberOfAlliedBuildings() > 2)
		return BuildOrderStatus::SUCCESSFUL;

	// dont start construction of further defences if expensive defences are already under construction in this sector
	for(list<AAIBuildTask*>::iterator task = ai->GetBuildTasks().begin(); task != ai->GetBuildTasks().end(); ++task)
	{
		if(ai->s_buildTree.GetUnitCategory(UnitDefId((*task)->def_id)).isStaticDefence() == true)
		{
			if(dest->PosInSector((*task)->build_pos))
			{
				const StatisticalData& costStatistics = ai->s_buildTree.GetUnitStatistics(ai->GetSide()).GetUnitCostStatistics(EUnitCategory::STATIC_DEFENCE);

				if( ai->s_buildTree.GetTotalCost(UnitDefId((*task)->def_id)) > 0.7f * costStatistics.GetAvgValue() )
					return BuildOrderStatus::SUCCESSFUL;
			}
		}
	}

	bool checkWater, checkGround;
	float3 pos;
	AAIConstructor *builder;

	float terrain = 2.0f;

	if(dest->distance_to_base > 0)
		terrain = 5.0f;

	if(dest->water_ratio < 0.15f)
	{
		checkWater = false;
		checkGround = true;
	}
	else if(dest->water_ratio < 0.85f)
	{
		checkWater = true;
		checkGround = true;
	}
	else
	{
		checkWater = true;
		checkGround = false;
	}

	float range       = 0.5f;
	float combatPower = 1.0f;
	float cost        = 1.0f;
	float buildtime   = 1.0f;

	const int staticDefences = dest->GetNumberOfBuildings(EUnitCategory::STATIC_DEFENCE);
	if(staticDefences > 2)
	{
		buildtime   = 0.5f;
		combatPower = 2.0f;

		int t = rand()%500;

		if(t < 100)
		{
			range   = 2.0f;
			terrain = 10.0f;
		}
		else if(t < 200)
		{
			range   = 1.0f;
			terrain = 5.0f;
		}
	}
	else if(staticDefences > 0)
	{
		buildtime = 2.0f;
		cost      = 1.5f;
		range     = 0.2f;
	}
	else // no static defences so far
	{
		buildtime = 3.0f;
		cost      = 2.0f;
		range     = 0.2f;
	}

	if(checkGround)
	{
		int randomness(8);
		if( (staticDefences > 4) && (rand()%cfg->LEARN_RATE == 1) ) // select defence more randomly from time to time
			randomness = 20;

		UnitDefId selectedDefence = ai->Getbt()->SelectStaticDefence(ai->GetSide(), cost, buildtime, combatPower, targetType, range, randomness, false);

		if(selectedDefence.isValid())
		{
			pos = dest->GetDefenceBuildsite(selectedDefence.id, targetType, terrain, false);

			if(pos.x > 0)
			{
				float min_dist;
				builder = ai->Getut()->FindClosestBuilder(selectedDefence.id, &pos, true, &min_dist);

				if(builder)
				{
					builder->GiveConstructionOrder(selectedDefence, pos);
					ai->Getmap()->AddStaticDefence(pos, selectedDefence);
					return BuildOrderStatus::SUCCESSFUL;
				}
				else
				{
					ai->Getbt()->BuildBuilderFor(selectedDefence);
					return BuildOrderStatus::NO_BUILDER_AVAILABLE;
				}
			}
			else
				return BuildOrderStatus::NO_BUILDSITE_FOUND;
		}
	}

	if(checkWater)
	{
		int randomness(8);
		if(staticDefences > 4 && (rand()%cfg->LEARN_RATE == 1) )// select defence more randomly from time to time
			randomness = 20;

		UnitDefId selectedDefence = ai->Getbt()->SelectStaticDefence(ai->GetSide(), cost, buildtime, combatPower, targetType, range, randomness, true);


		if(selectedDefence.isValid())
		{
			pos = dest->GetDefenceBuildsite(selectedDefence.id, targetType, terrain, true);

			if(pos.x > 0)
			{
				float min_dist;
				builder = ai->Getut()->FindClosestBuilder(selectedDefence.id, &pos, true, &min_dist);

				if(builder)
				{
					builder->GiveConstructionOrder(selectedDefence, pos);
					ai->Getmap()->AddStaticDefence(pos, selectedDefence);
					return BuildOrderStatus::SUCCESSFUL;
				}
				else
				{
					ai->Getbt()->BuildBuilderFor(selectedDefence);
					return BuildOrderStatus::NO_BUILDER_AVAILABLE;
				}
			}
			else
				return BuildOrderStatus::NO_BUILDSITE_FOUND;
		}
	}

	return BuildOrderStatus::BUILDING_INVALID;
}

bool AAIExecute::BuildArty()
{
	if(ai->Getut()->GetNumberOfFutureUnitsOfCategory(EUnitCategory::STATIC_ARTILLERY) > 0)
		return true;

	const float cost(1.0f);
	const float range(1.5f);

	UnitDefId landArtillery = ai->Getbt()->SelectStaticArtillery(ai->GetSide(), cost, range, false);
	UnitDefId seaArtillery  = ai->Getbt()->SelectStaticArtillery(ai->GetSide(), cost, range, true);

	if(landArtillery.isValid() && (ai->Getbt()->units_dynamic[landArtillery.id].constructorsAvailable <= 0))
	{
		if(ai->Getbt()->units_dynamic[landArtillery.id].constructorsRequested <= 0)
			ai->Getbt()->BuildBuilderFor(landArtillery);
	}

	if(seaArtillery.isValid() && (ai->Getbt()->units_dynamic[seaArtillery.id].constructorsAvailable <= 0))
	{
		if(ai->Getbt()->units_dynamic[seaArtillery.id].constructorsRequested <= 0)
			ai->Getbt()->BuildBuilderFor(seaArtillery);
	}

	//ai->Log("Selected artillery (land/sea): %s / %s\n", ai->s_buildTree.GetUnitTypeProperties(landArtillery).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(seaArtillery).m_name.c_str());

	float  bestRating(0.0f);
	float3 bestPosition(ZeroVector);

	for(auto sector = ai->Getbrain()->sectors[0].begin(); sector != ai->Getbrain()->sectors[0].end(); ++sector)
	{
		if((*sector)->GetNumberOfBuildings(EUnitCategory::STATIC_ARTILLERY) < 2)
		{
			float3 position = ZeroVector;

			if(landArtillery.isValid()  && ((*sector)->water_ratio < 0.9f) )
				position = (*sector)->GetRadarArtyBuildsite(landArtillery.id, ai->s_buildTree.GetMaxRange(landArtillery)/4.0f, false);

			if((position.x <= 0.0f) && seaArtillery.isValid() && ((*sector)->water_ratio > 0.1f) )
				position = (*sector)->GetRadarArtyBuildsite(seaArtillery.id, ai->s_buildTree.GetMaxRange(seaArtillery)/4.0f, true);
			
			if(position.x > 0)
			{
				const float myRating = ai->Getmap()->GetEdgeDistance(&position);

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
		AAIConstructor *builder = ai->Getut()->FindClosestBuilder(artillery.id, &bestPosition, true, &minDistance);

		if(builder)
		{
			builder->GiveConstructionOrder(artillery, bestPosition);
			return true;
		}
		else
		{
			ai->Getbt()->BuildBuilderFor(artillery);
			return false;
		}
	}

	return true;
}

bool AAIExecute::BuildFactory()
{
	const AAIUnitCategory staticConstructor(EUnitCategory::STATIC_CONSTRUCTOR);
	if(ai->Getut()->GetNumberOfFutureUnitsOfCategory(staticConstructor) > 0)
		return true;

	auto factory = ai->Getbt()->GetFactoryBuildqueue().begin();

	while( factory != ai->Getbt()->GetFactoryBuildqueue().end() )
	{
		// find suitable builder
		AAIConstructor* builder = ai->Getut()->FindBuilder(factory->id, true);
	
		if(builder == nullptr)
		{
			// try construction of next factory in queue if there is no active builder (i.e. potential builders not just currently busy)
			if( ai->Getbt()->units_dynamic[factory->id].constructorsAvailable <= 0) 
			{
				++factory;
				continue;
			}
			else
			{
				// keep factory at highest urgency if the construction failed due to (temporarily) unavailable builder
				return false;
			}
		}

		const bool isSeaFactory( ai->s_buildTree.GetMovementType(*factory).IsStaticSea() );
	
		if(isSeaFactory)
			ai->Getbrain()->sectors[0].sort(suitable_for_sea_factory);
		else
			ai->Getbrain()->sectors[0].sort(suitable_for_ground_factory);

		// find buildpos
		const std::list<AAISector*>& baseSectorsList = ai->Getbrain()->sectors[0];
		float3 buildpos;

		for(auto sector = baseSectorsList.begin(); sector != baseSectorsList.end(); ++sector)
		{
			// try random buildpos first
			buildpos = (*sector)->GetRandomBuildsite(factory->id, 20, isSeaFactory);

			if(buildpos.x > 0)
				break;
			else
			{
				// search systematically for buildpos (i.e. search returns a buildpos if one is available in the sector)
				buildpos = (*sector)->FindBuildsite(factory->id, isSeaFactory);

				if(buildpos.x > 0)
					break;
			}
		}

		if(buildpos.x > 0)
		{
			// buildpos found -> l
			float min_dist;
			builder = ai->Getut()->FindClosestBuilder(factory->id, &buildpos, true, &min_dist);

			if(builder != nullptr)
			{
				// give build order
				builder->GiveConstructionOrder(*factory, buildpos);

				ai->Getbt()->ConstructionOrderForFactoryGiven(*factory);
				return true;
			}
			else 
			{
				if(ai->Getbt()->units_dynamic[factory->id].constructorsRequested + ai->Getbt()->units_dynamic[factory->id].constructorsAvailable <= 0)
					ai->Getbt()->BuildBuilderFor(*factory);

				return false;
			}
		}
		else
		{
			// no buildpos found in whole base -> expand base
			bool expanded = false;

			// no suitable buildsite found
			if(isSeaFactory)
			{
				ai->Getbrain()->ExpandBase(WATER_SECTOR);
				ai->Log("Base expanded by BuildFactory() (water sector)\n");
			}
			else
			{
				expanded = ai->Getbrain()->ExpandBase(LAND_SECTOR);
				ai->Log("Base expanded by BuildFactory()\n");
			}

			return false;
		}
	}

	return true;
}


bool AAIExecute::BuildRadar()
{
	const AAIUnitCategory sensor(EUnitCategory::STATIC_SENSOR);
	if(ai->Getut()->GetTotalNumberOfUnitsOfCategory(sensor) > ai->Getbrain()->sectors[0].size())
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
		for(auto sector = ai->Getbrain()->sectors[dist].begin(); sector != ai->Getbrain()->sectors[dist].end(); ++sector)
		{
			if((*sector)->GetNumberOfBuildings(EUnitCategory::STATIC_SENSOR) <= 0)
			{
				float3 myPosition(ZeroVector);
				bool   seaPositionFound(false);

				if( landRadar.isValid() && ((*sector)->water_ratio < 0.9f) )
					myPosition = (*sector)->GetRadarArtyBuildsite(landRadar.id, ai->s_buildTree.GetMaxRange(landRadar), false);

				if( (myPosition.x == 0.0f) && seaRadar.isValid() && ((*sector)->water_ratio > 0.1f) )
				{
					myPosition = (*sector)->GetRadarArtyBuildsite(seaRadar.id, ai->s_buildTree.GetMaxRange(seaRadar), true);
					seaPositionFound = true;
				}

				if(myPosition.x > 0.0f)
				{
					const float myRating = - ai->Getmap()->GetEdgeDistance(&myPosition);

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

	if(selectedRadar.isValid())
	{
		float min_dist;
		AAIConstructor *builder = ai->Getut()->FindClosestBuilder(selectedRadar.id, &bestPosition, true, &min_dist);

		if(builder)
		{
			builder->GiveConstructionOrder(selectedRadar, bestPosition);
			return true;
		}
		else
		{
			ai->Getbt()->BuildBuilderFor(selectedRadar);
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
	if(ai->Getmap()->water_ratio > 0.02f)
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
			if(ground_jammer && (*sector)->water_ratio < 0.9f)
				pos = (*sector)->GetCenterBuildsite(ground_jammer, false);

			if(pos.x == 0 && sea_jammer && (*sector)->water_ratio > 0.1f)
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

void AAIExecute::DefendMex(int mex, int def_id)
{
	if(ai->Getut()->activeFactories < cfg->MIN_FACTORIES_FOR_DEFENCES)
		return;

	float3 pos = ai->GetAICallback()->GetUnitPos(mex);
	const float3& base_pos = ai->Getbrain()->GetCenterOfBase();

	// check if mex is located in a small pond/on a little island
	if(ai->Getmap()->LocatedOnSmallContinent(pos))
		return;

	const AAISector *sector = ai->Getmap()->GetSectorOfPos(pos); 

	if(sector) 
	{
		if(    (sector->distance_to_base > 0)
			&& (sector->distance_to_base <= cfg->MAX_MEX_DEFENCE_DISTANCE)
			&& (sector->GetNumberOfBuildings(EUnitCategory::STATIC_DEFENCE) < 1) )
		{
			const bool water = ai->s_buildTree.GetMovementType(UnitDefId(def_id)).IsStaticSea() ? true : false;
			const AAITargetType targetType( water ? ETargetType::FLOATER : ETargetType::SURFACE); 

			UnitDefId defence = ai->Getbt()->SelectStaticDefence(ai->GetSide(), 2.0f, 2.0f, 1.0f, targetType, 0.2f, 1, water); 

			// find closest builder
			if(defence.isValid())
			{
				// place defences according to the direction of the main base
				if(pos.x > base_pos.x + 500.0f)
					pos.x += 120.0f;
				else if(pos.x > base_pos.x + 300.0f)
					pos.x += 70.0f;
				else if(pos.x < base_pos.x - 500.0f)
					pos.x -= 120.0f;
				else if(pos.x < base_pos.x - 300.0f)
					pos.x -= 70.0f;

				if(pos.z > base_pos.z + 500.0f)
					pos.z += 70.0f;
				else if(pos.z > base_pos.z + 300.0f)
					pos.z += 120.0f;
				else if(pos.z < base_pos.z - 500.0f)
					pos.z -= 120.0f;
				else if(pos.z < base_pos.z - 300.0f)
					pos.z -= 70.0f;

				// get suitable pos
				pos = ai->GetAICallback()->ClosestBuildSite(&ai->Getbt()->GetUnitDef(defence.id), pos, 1400.0f, 2);

				if(pos.x > 0.0f)
				{
					const bool commanderAllowed = (ai->Getbrain()->sectors[0].size() > 2);

					float min_dist;
					AAIConstructor *builder = ai->Getut()->FindClosestBuilder(defence.id, &pos, commanderAllowed, &min_dist);

					if(builder)
						builder->GiveConstructionOrder(defence, pos);
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

	float temp = 0.05f;

	if(temp > urgency[STATIONARY_ARTY])
		urgency[STATIONARY_ARTY] = temp;
}

void AAIExecute::CheckBuildqueues()
{
	int req_units = 0;
	int active_factory_types = 0;

	for(int i = 0; i < numOfFactories; ++i)
	{
		// sum up builque lengths of active factory types
		if(ai->Getbt()->units_dynamic[factory_table[i]].active > 0)
		{
			req_units += (int) buildques[i].size();
			++active_factory_types;
		}
	}

	if(active_factory_types > 0)
	{
		if( (float)req_units / (float)active_factory_types < (float)cfg->MAX_BUILDQUE_SIZE / 2.5f )
		{
			if(unitProductionRate < 70)
				++unitProductionRate;

			//ai->Log("Increasing unit production rate to %i\n", unitProductionRate);
		}
		else if( (float)req_units / (float)active_factory_types > (float)cfg->MAX_BUILDQUE_SIZE / 1.5f )
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
	const AAIUnitCategory staticDefence(EUnitCategory::STATIC_DEFENCE);
	if(    (ai->Getut()->activeFactories < cfg->MIN_FACTORIES_FOR_DEFENCES)
		|| (ai->Getut()->GetNumberOfUnitsUnderConstructionOfCategory(staticDefence) +  ai->Getut()->GetNumberOfRequestedUnitsOfCategory(staticDefence) > 2) )
		return;

	GamePhase gamePhase(ai->GetAICallback()->GetCurrentFrame());

	const int maxSectorDistToBase(2);
	float highestRating(0);

	AAISector *first = nullptr, *second = nullptr;
	AAITargetType targetType1, targetType2;

	for(int dist = 0; dist <= maxSectorDistToBase; ++dist)
	{
		for(list<AAISector*>::iterator sector = ai->Getbrain()->sectors[dist].begin(); sector != ai->Getbrain()->sectors[dist].end(); ++sector)
		{
			// stop building further defences if maximum has been reached / sector contains allied buildings / is occupied by another aai instance
			if(    ((*sector)->GetNumberOfBuildings(EUnitCategory::STATIC_DEFENCE) < cfg->MAX_DEFENCES) 
				&& ((*sector)->GetNumberOfAlliedBuildings() < 3)
				&& (ai->Getmap()->team_sector_map[(*sector)->x][(*sector)->y] != ai->GetAICallback()->GetMyAllyTeam()) )
			{
				if((*sector)->failed_defences > 1)
					(*sector)->failed_defences = 0;
				else
				{
					for(AAITargetType targetType(AAITargetType::first); targetType.MobileTargetTypeEnd() == false; targetType.Next())
					{
						// anti air defences may be built anywhere
						/*if(cfg->AIR_ONLY_MOD || *cat == AIR_ASSAULT)
						{
							//rating = (*sector)->own_structures * (0.25 + ai->Getbrain()->GetAttacksBy(*cat, game_period)) * (0.25 + (*sector)->GetLocalAttacksBy(targetType learned, current)) / ( 0.1 + (*sector)->GetMyDefencePowerAgainstAssaultCategory(*cat));
							// how often did units of category attack that sector compared to current def power
							rating = (1.0f + (*sector)->GetLocalAttacksBy(targetType, learned, current)) / ( 1.0f + (*sector)->GetMyDefencePowerAgainstAssaultCategory(*cat));

							// how often did unist of that category attack anywere in the current period of the game
							rating *= (0.1f + ai->Getbrain()->GetAttacksBy(*cat, game_period));
						}*/
						//else if(!(*sector)->interior)
						if(true) //(*sector)->distance_to_base > 0) // dont build anti ground/hover/sea defences in interior sectors
						{
							// how often did units of category attack that sector compared to current def power
							float rating = (1.0f + (*sector)->GetLocalAttacksBy(targetType, learned, current)) / ( 1.0f + (*sector)->GetFriendlyStaticDefencePower(targetType));

							// how often did units of that category attack anywere in the current period of the game
							rating *= (0.1f + ai->Getbrain()->GetAttacksBy(targetType, gamePhase));

							if(rating > highestRating)
							{
								// dont block empty sectors with too much aa
								if(    (targetType.IsAir() == false) 
									|| ((*sector)->GetNumberOfBuildings(EUnitCategory::POWER_PLANT) > 0)
									|| ((*sector)->GetNumberOfBuildings(EUnitCategory::STATIC_CONSTRUCTOR) > 0 ) ) 
								{
									second = first;
									targetType2 = targetType1;

									first = *sector;
									targetType1 = targetType;

									highestRating = rating;
								}
							}
						}
					}
				}
			}
		}
	}

	if(first)
	{
		// if no builder available retry later
		BuildOrderStatus status = BuildStationaryDefenceVS(targetType1, first);

		if(status == BuildOrderStatus::NO_BUILDER_AVAILABLE)
		{
			float temp = 0.03f + 1.0f / ( static_cast<float>(first->GetNumberOfBuildings(EUnitCategory::STATIC_DEFENCE)) + 0.5f);

			if(urgency[STATIONARY_DEF] < temp)
				urgency[STATIONARY_DEF] = temp;

			m_sectorToBuildNextDefence = first;
			m_nextDefenceVsTargetType = targetType1;
		}
		else if(status == BuildOrderStatus::NO_BUILDSITE_FOUND)
			++first->failed_defences;
	}

	if(second)
		BuildStationaryDefenceVS(targetType2, second);
}

void AAIExecute::CheckRessources()
{
	// prevent float rounding errors
	if(futureAvailableEnergy < 0.0f)
		futureAvailableEnergy = 0.0f;

	// determine how much metal/energy is needed based on net surplus
	const float extractorUrgency  = ai->Getbrain()->GetMetalUrgency();
	if(urgency[EXTRACTOR] < extractorUrgency) // && urgency[EXTRACTOR] > 0.05)
		urgency[EXTRACTOR] = extractorUrgency;

	const float plantUrgency = ai->Getbrain()->GetEnergyUrgency();
	if(urgency[POWER_PLANT] < plantUrgency) // && urgency[POWER_PLANT] > 0.05)
		urgency[POWER_PLANT] = plantUrgency;

	// build storages if needed
	const AAIUnitCategory storage(EUnitCategory::STORAGE);
	if(    (ai->Getut()->GetTotalNumberOfUnitsOfCategory(storage) < cfg->MAX_STORAGE)
		&& (ai->Getut()->activeFactories >= cfg->MIN_FACTORIES_FOR_STORAGE))
	{
		const float storageUrgency = max(ai->Getbrain()->GetMetalStorageUrgency(), ai->Getbrain()->GetEnergyStorageUrgency());

		if(storageUrgency > urgency[STORAGE])
			urgency[STORAGE] = storageUrgency;
	}

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
	if(ai->Getbrain()->m_freeMetalSpotsInBase)
		return;

	float cost = 0.25f + ai->Getbrain()->Affordable() / 8.0f;
	float eff  = 6.0f / (cost + 0.75f);

	int my_team = ai->GetAICallback()->GetMyTeam();

	UnitDefId landExtractor = ai->Getbt()->SelectExtractor(ai->GetSide(), cost, eff, false, false);
	UnitDefId seaExtractor  = ai->Getbt()->SelectExtractor(ai->GetSide(), cost, eff, false, true);

	float landExtractedMetal = 0.0f;
	float seaExtractedMetal  = 0.0f;

	if(landExtractor.isValid())
		landExtractedMetal = ai->s_buildTree.GetMaxRange(landExtractor);

	if(seaExtractor.isValid())
		seaExtractedMetal = ai->s_buildTree.GetMaxRange(seaExtractor);

	float maxExtractedMetalGain(0.0f);
	AAIMetalSpot *selectedMetalSpot = nullptr;

	// check extractor upgrades
	for(int dist = 0; dist < 2; ++dist)
	{
		for(auto sector = ai->Getbrain()->sectors[dist].begin(); sector != ai->Getbrain()->sectors[dist].end(); ++sector)
		{
			for(auto spot = (*sector)->metalSpots.begin(); spot != (*sector)->metalSpots.end(); ++spot)
			{
				// quit when finding empty spots
				if(!(*spot)->occupied && ((*sector)->GetNumberOfEnemyBuildings() <= 0) && ((*sector)->GetLostUnits() < 0.2f) )
					return;

				if((*spot)->extractor_def > 0 && (*spot)->extractor > -1 && (*spot)->extractor < cfg->MAX_UNITS
					&& ai->GetAICallback()->GetUnitTeam((*spot)->extractor) == my_team)	// only upgrade own extractors
				{
					float extractedMetalGain;

					if(ai->s_buildTree.GetMovementType( UnitDefId((*spot)->extractor_def) ).IsStaticLand() )	// land mex
						extractedMetalGain = landExtractedMetal - ai->s_buildTree.GetMaxRange( UnitDefId((*spot)->extractor_def) );
					else	// water mex
						extractedMetalGain = seaExtractedMetal  - ai->s_buildTree.GetMaxRange( UnitDefId((*spot)->extractor_def) );

					if(extractedMetalGain > 0.0001f && extractedMetalGain > maxExtractedMetalGain)
					{
						maxExtractedMetalGain = extractedMetalGain;
						selectedMetalSpot = *spot;
					}
				}
			}
		}
	}

	if(selectedMetalSpot)
	{
		AAIConstructor *builder = ai->Getut()->FindClosestAssistant(selectedMetalSpot->pos, 10, true);

		if(builder)
			builder->GiveReclaimOrder(selectedMetalSpot->extractor);
	}
}


void AAIExecute::CheckRadarUpgrade()
{
	if(ai->Getut()->GetNumberOfFutureUnitsOfCategory(AAIUnitCategory(EUnitCategory::STATIC_SENSOR))  > 0)
		return;

	float cost = ai->Getbrain()->Affordable();
	float range = 10.0f / (cost + 1.0f);

	UnitDefId landRadar(  ai->Getbt()->SelectRadar(ai->GetSide(), cost, range, false) );
	UnitDefId waterRadar( ai->Getbt()->SelectRadar(ai->GetSide(), cost, range, true) );

	// check radar upgrades
	for(set<int>::iterator sensor = ai->Getut()->recon.begin(); sensor != ai->Getut()->recon.end(); ++sensor)
	{
		bool upgradeRadar(false);
		
		if(ai->s_buildTree.GetMovementType(UnitDefId(*sensor)).IsStaticLand() == true )	// land recon
		{
			if( (landRadar.isValid() == true) &&  (ai->s_buildTree.GetMaxRange(UnitDefId(*sensor)) < ai->s_buildTree.GetMaxRange(landRadar)) )
			{
				upgradeRadar = true;
			}
		}
		else	// water radar
		{
			if( (waterRadar.isValid() == true) && (ai->s_buildTree.GetMaxRange(UnitDefId(*sensor)) < ai->s_buildTree.GetMaxRange(waterRadar)) )
			{
				upgradeRadar = true;
			}
		}

		if(upgradeRadar == true)
		{
			// better radar found, clear buildpos
			AAIConstructor *builder = ai->Getut()->FindClosestAssistant(ai->GetAICallback()->GetUnitPos(*sensor), 10, true);

			if(builder)
			{
				builder->GiveReclaimOrder(*sensor);
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
						builder->GiveReclaimOrder(*jammer);
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
						builder->GiveReclaimOrder(*jammer);
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

	for(auto fac = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, ai->GetSide()).begin(); fac != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, ai->GetSide()).end(); ++fac)
	{
		if(ai->Getbt()->units_dynamic[fac->id].requested > 0)
		{
			// at least one requested factory has not been built yet
			float urgency;

			if(ai->Getut()->activeFactories > 0)
				urgency = 0.4f;
			else
				urgency = 3.5f;

			if(this->urgency[STATIONARY_CONSTRUCTOR] < urgency)
				this->urgency[STATIONARY_CONSTRUCTOR] = urgency;

			return;
		}
	}
}

void AAIExecute::CheckRecon()
{
	float urgency;
	
	// do not build radar before at least one factory is finished.
	if(ai->Getut()->GetNumberOfActiveUnitsOfCategory(EUnitCategory::STATIC_CONSTRUCTOR) < 1)
		urgency = 0.0f;
	else
		urgency = 0.02f + 0.5f / ((float)(2 * ai->Getut()->GetNumberOfActiveUnitsOfCategory(AAIUnitCategory(EUnitCategory::STATIC_SENSOR)) + 1));

	if(this->urgency[STATIONARY_RECON] < urgency)
		this->urgency[STATIONARY_RECON] = urgency;
}

void AAIExecute::CheckAirBase()
{
	urgency[AIR_BASE] = 0.0f; // Detection of air base currently broken
	// only build repair pad if any air units have been built yet
	//if(ai->Getut()->activeUnits[AIR_BASE] +  ai->Getut()->requestedUnits[AIR_BASE] + ai->Getut()->futureUnits[AIR_BASE] < cfg->MAX_AIR_BASE && ai->GetGroupList()[AIR_ASSAULT].size() > 0)
	//		urgency[AIR_BASE] = 0.5f;
}

void AAIExecute::CheckJammer()
{
	this->urgency[STATIONARY_JAMMER] = 0; //! @todo Activate construction of stationary jammers later
	
	/*if(ai->Getut()->activeFactories < 2 || ai->Getut()->activeUnits[STATIONARY_JAMMER] > ai->Getbrain()->sectors[0].size())
	{
		this->urgency[STATIONARY_JAMMER] = 0;
	}
	else
	{
		float temp = 0.2f / ((float) (ai->Getut()->activeUnits[STATIONARY_JAMMER]+1)) + 0.05f;

		if(urgency[STATIONARY_JAMMER] < temp)
			urgency[STATIONARY_JAMMER] = temp;
	}*/
}

void AAIExecute::CheckConstruction()
{
	UnitCategory category = UNKNOWN;
	float highest_urgency = 0.5f;		// min urgency (prevents aai from building things it doesnt really need that much)
	bool construction_started = false;

	// get category with highest urgency
	if(ai->Getbrain()->enemy_pressure_estimation > 0.01f)
	{
//		double current_urgency;

		for(int i = 1; i <= METAL_MAKER; ++i)
		{
/*
			current_urgency = urgency[i];

			if(i != STATIONARY_DEF && i != POWER_PLANT && i != EXTRACTOR && i != STATIONARY_CONSTRUCTOR)
				current_urgency *= (1.1f - ai->Getbrain()->enemy_pressure_estimation);
*/
			if(urgency[i] > highest_urgency)
			{
				highest_urgency = urgency[i];
				category = (UnitCategory)i;
			}
		}
	}
	else
	{
		for(int i = 1; i <= METAL_MAKER; ++i)
		{
			if(urgency[i] > highest_urgency)
			{
				highest_urgency = urgency[i];
				category = (UnitCategory)i;
			}
		}
	}

	if(category == POWER_PLANT)
	{
		if(BuildPowerPlant())
			construction_started = true;
	}
	else if(category == EXTRACTOR)
	{
		if(BuildExtractor())
			construction_started = true;
	}
	else if(category == STATIONARY_CONSTRUCTOR)
	{
		if(BuildFactory())
			construction_started = true;
	}
	else if(category == STATIONARY_DEF)
	{
		if(BuildDefences())
			construction_started = true;
	}
	else if(category == STATIONARY_RECON)
	{
		if(BuildRadar())
			construction_started = true;
	}
	else if(category == STATIONARY_JAMMER)
	{
		if(BuildJammer())
			construction_started = true;
	}
	else if(category == STATIONARY_ARTY)
	{
		if(BuildArty())
			construction_started = true;
	}
	else if(category == STORAGE)
	{
		if(BuildStorage())
			construction_started = true;
	}
	else if(category == METAL_MAKER)
	{
		if(BuildMetalMaker())
			construction_started = true;
	}
	else if(category == AIR_BASE)
	{
		if(BuildAirBase())
			construction_started = true;
	}

	if(construction_started)
		urgency[category] = 0;

	for(int i = 1; i <= METAL_MAKER; ++i)
	{
		urgency[i] *= 1.02f;

		if(urgency[i] > 20.0f)
			urgency[i] -= 1.0f;
	}
}

bool AAIExecute::AssistConstructionOfCategory(const AAIUnitCategory& category)
{
	AAIConstructor *builder, *assistant;

	for(list<AAIBuildTask*>::iterator task = ai->GetBuildTasks().begin(); task != ai->GetBuildTasks().end(); ++task)
	{
		if((*task)->builder_id >= 0)
			builder = ai->Getut()->units[(*task)->builder_id].cons;
		else
			builder = nullptr;

		if(   (builder != nullptr) 
		   && (builder->GetCategoryOfConstructedUnit() == category)
		   && (builder->assistants.size() < cfg->MAX_ASSISTANTS) )
		{
			assistant = ai->Getut()->FindClosestAssistant(builder->GetBuildPos(), 5, true);

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
	return ( (2.0f * left->flat_ratio + static_cast<float>( left->GetEdgeDistance() ))
			> (2.0f * right->flat_ratio + static_cast<float>( right->GetEdgeDistance() )) );
}

bool AAIExecute::suitable_for_sea_factory(AAISector *left, AAISector *right)
{
	return ( (2.0f * left->water_ratio + static_cast<float>( left->GetEdgeDistance() ))
			> (2.0f * right->water_ratio + static_cast<float>( right->GetEdgeDistance() )) );
}

bool AAIExecute::suitable_for_ground_rallypoint(AAISector *left, AAISector *right)
{
	return ( (left->flat_ratio  + 0.5f * static_cast<float>( left->GetEdgeDistance() ))/ ((float) (left->rally_points + 1) )
		>  (right->flat_ratio  + 0.5f * static_cast<float>( right->GetEdgeDistance() ))/ ((float) (right->rally_points + 1) ) );
}

bool AAIExecute::suitable_for_sea_rallypoint(AAISector *left, AAISector *right)
{
	return ( (left->water_ratio  + 0.5f * static_cast<float>( left->GetEdgeDistance() ))/ ((float) (left->rally_points + 1) )
		>  (right->water_ratio  + 0.5f * static_cast<float>( right->GetEdgeDistance() ))/ ((float) (right->rally_points + 1) ) );
}

bool AAIExecute::suitable_for_all_rallypoint(AAISector *left, AAISector *right)
{
	return ( (left->flat_ratio + left->water_ratio + 0.5f * static_cast<float>( left->GetEdgeDistance() ))/ ((float) (left->rally_points + 1) )
		>  (right->flat_ratio + right->water_ratio + 0.5f * static_cast<float>( right->GetEdgeDistance() ))/ ((float) (right->rally_points + 1) ) );
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
	const UnitDef *def = &ai->Getbt()->GetUnitDef(unitDefId.id);
	const AAIUnitCategory category = ai->s_buildTree.GetUnitCategory(unitDefId);

	const int  x = build_pos.x/ai->Getmap()->xSectorSize;
	const int  y = build_pos.z/ai->Getmap()->ySectorSize;
	const bool validSector = ai->Getmap()->IsValidSector(x, y);

	// decrease number of units of that category in the target sector
	if(validSector)
		ai->Getmap()->sector[x][y].RemoveBuilding(category);

	// free metalspot if mex was odered to be built
	if(category.isMetalExtractor())
	{
		if(validSector)
			ai->Getmap()->sector[x][y].FreeMetalSpot(build_pos, def);
	}
	else if(category.isPowerPlant())
	{
		futureAvailableEnergy -= ai->s_buildTree.GetPrimaryAbility(unitDefId);

		if(futureAvailableEnergy < 0.0f)
			futureAvailableEnergy = 0.0f;
	}
	else if(category.isStaticDefence())
	{
		ai->Getmap()->RemoveDefence(build_pos, unitDefId);
	}
	else if(category.isStaticConstructor())
	{
		ai->Getut()->futureFactories -= 1;

		ai->Getbt()->UnfinishedConstructorKilled(unitDefId);
	}

	// update buildmap of sector
	ai->Getmap()->UpdateBuildMap(build_pos, def, false);
}

AAIGroup* AAIExecute::GetClosestGroupForDefence(const AAITargetType& attackerTargetType, const float3& pos, int importance) const
{
	const int continentId = ai->Getmap()->GetContinentID(pos);

	AAIGroup *best_group = nullptr;
	float bestRating(0.0f);

	for(auto category = ai->s_buildTree.GetCombatUnitCatgegories().begin(); category != ai->s_buildTree.GetCombatUnitCatgegories().end(); ++category)
	{
		for(list<AAIGroup*>::iterator group = ai->GetGroupList()[category->GetArrayIndex()].begin(); group != ai->GetGroupList()[category->GetArrayIndex()].end(); ++group)
		{
			if( ((*group)->GetContinentId() == -1) || ((*group)->GetContinentId() == continentId) )
			{
				const bool matchingType  = (*group)->CanFightTargetType(attackerTargetType);
				const bool groupAvailble = ((*group)->task == GROUP_IDLE) || ((*group)->task_importance < importance); //!(*group)->attack

				if(matchingType && groupAvailble)
				{
					const float3& group_pos = (*group)->GetGroupPos();

					const float myRating = (*group)->avg_speed / ( 1.0f + fastmath::apxsqrt((pos.x - group_pos.x) * (pos.x - group_pos.x)  + (pos.z - group_pos.z) * (pos.z - group_pos.z) ));

					if(myRating > bestRating)
					{
						best_group = *group;
						bestRating = myRating;
					}
				}
			}
		}
	}

	return best_group;
}

void AAIExecute::DefendUnitVS(const UnitId& unitId, const AAITargetType& attackerTargetType, const float3& attackerPosition, int importance) const
{
	AAIGroup *support = GetClosestGroupForDefence(attackerTargetType, attackerPosition, importance);

	if(support)
		support->Defend(unitId, attackerPosition, importance);
}

float3 AAIExecute::DetermineSafePos(UnitDefId unitDefId, float3 unit_pos) const
{
	float3 selectedPosition(ZeroVector);
	float highestRating(-10000.0f);

	if( ai->s_buildTree.GetMovementType(unitDefId).CannotMoveToOtherContinents() )
	{
		// get continent id of the unit pos
		const int continentId = ai->Getmap()->GetContinentID(unit_pos);

		for(std::list<AAISector*>::iterator sector = ai->Getbrain()->sectors[0].begin(); sector != ai->Getbrain()->sectors[0].end(); ++sector)
		{
			const float3 pos = (*sector)->GetCenter();

			if(ai->Getmap()->GetContinentID(pos) == continentId)
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
		for(std::list<AAISector*>::iterator sector = ai->Getbrain()->sectors[0].begin(); sector != ai->Getbrain()->sectors[0].end(); ++sector)
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
	list<AAISector*> sectors;

	if(x >= 1)
	{
		sectors.push_back( &ai->Getmap()->sector[x-1][y] );

		if(y >= 1)
			sectors.push_back( &ai->Getmap()->sector[x-1][y-1] );

		if(y < ai->Getmap()->ySectors-1)
			sectors.push_back( &ai->Getmap()->sector[x-1][y+1] );
	}

	if(x < ai->Getmap()->xSectors-1)
	{
		sectors.push_back( &ai->Getmap()->sector[x+1][y] );

		if(y >= 1)
			sectors.push_back( &ai->Getmap()->sector[x+1][y-1] );

		if(y < ai->Getmap()->ySectors-1)
			sectors.push_back( &ai->Getmap()->sector[x+1][y+1] );
	}

	if(y >= 1)
		sectors.push_back( &ai->Getmap()->sector[x][y-1] );

	if(y < ai->Getmap()->ySectors-1)
		sectors.push_back( &ai->Getmap()->sector[x][y+1] );

	// choose best
	AAISector *best_sector = 0;
	float my_rating, best_rating = 0;

	for(list<AAISector*>::iterator sector = sectors.begin(); sector != sectors.end(); ++sector)
	{
		if(ai->Getmap()->team_sector_map[(*sector)->x][(*sector)->y] != -1)
			my_rating = 0;
		else
			my_rating = ( (float)(2 * (*sector)->GetNumberOfMetalSpots() + 1) ) * (*sector)->flat_ratio * (*sector)->flat_ratio;

		if(my_rating > best_rating)
		{
			best_rating = my_rating;
			best_sector = *sector;
		}
	}

	// add best sector to base
	if(best_sector)
	{
		ai->Getbrain()->AssignSectorToBase(best_sector, true);
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
	const int numberOfEnemies = ai->GetAICallback()->GetEnemyUnits(&(ai->Getmap()->units_in_los.front()), pos, maxFallbackDist);

	if(numberOfEnemies > 0)
	{
		float3 enemy_pos;

		for(int k = 0; k < numberOfEnemies; ++k)
		{
			float3 enemy_pos = ai->GetAICallback()->GetUnitPos(ai->Getmap()->units_in_los[k]);

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
