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

	max_combat_units_spotted.resize(AAIBuildTable::ass_categories, 0.0f);
	m_recentlyAttackedByCategory.resize(AAIBuildTable::combat_categories, 0.0f);
	defence_power_vs.resize(AAIBuildTable::ass_categories, 0.0f);

	enemy_pressure_estimation = 0;
}

AAIBrain::~AAIBrain(void)
{
}


AAISector* AAIBrain::GetAttackDest(bool land, bool water)
{
	float best_rating = 0.0f, my_rating = 0.0f;
	AAISector *dest = 0;

	CombatPower defencePowerWeightsLand(1.0f, 0.0f, 0.3f, 0.0f, 0.0f);
	CombatPower defencePowerWeightsSea(0.0f, 0.0f, 0.5f, 1.0f, 0.5f);

	// TODO: improve destination sector selection
	for(int x = 0; x < ai->Getmap()->xSectors; ++x)
	{
		for(int y = 0; y < ai->Getmap()->ySectors; ++y)
		{
			AAISector* sector = &ai->Getmap()->sector[x][y];

			const bool checkSector = (land && sector->water_ratio > 0.6f) || (water && sector->water_ratio < 0.4f);

			if( checkSector && (sector->distance_to_base > 0) && (sector->enemy_structures > 0.1f) )
			{
				const CombatPower& defencePowerweights = sector->water_ratio < 0.6f ? defencePowerWeightsLand : defencePowerWeightsSea;

				float defencePower = sector->GetEnemyDefencePower(defencePowerweights);

				float myRating;

				if(defencePower > 0.1f) 
				{
					myRating = sector->enemy_structures / defencePower;
				} 
				else 
				{
					myRating = sector->enemy_structures / pow(sector->GetLostUnits() + 1.0f, 1.5f);
				}
				myRating /= static_cast<float>(5 + sector->distance_to_base);
				
				if(myRating > best_rating)
				{
					dest = sector;
					best_rating = myRating;
				}
			}
		}
	}

	return dest;
}

AAISector* AAIBrain::GetNextAttackDest(AAISector *current_sector, bool land, bool water)
{
	float best_rating = 0, my_rating, dist;
	AAISector *dest = 0, *sector;

	CombatPower defencePowerWeightsLand(1.0f, 0.0f, 0.3f, 0.0f, 0.0f);
	CombatPower defencePowerWeightsSea(0.0f, 0.0f, 0.5f, 1.0f, 0.5f);

	// TODO: improve destination sector selection
	for(int x = 0; x < ai->Getmap()->xSectors; x++)
	{
		for(int y = 0; y < ai->Getmap()->ySectors; y++)
		{
			sector = &ai->Getmap()->sector[x][y];

			if(sector->distance_to_base == 0 || sector->enemy_structures < 0.001f)
					my_rating = 0;
			else
			{
				if(land && sector->water_ratio < 0.35)
				{
					dist = sqrt( pow((float)sector->x - current_sector->x, 2) + pow((float)sector->y - current_sector->y , 2) );

					my_rating = 1.0f / (1.0f + pow(sector->GetEnemyDefencePower(defencePowerWeightsLand), 2.0f) + pow(sector->GetLostUnits() + 1.0f, 1.5f));
					my_rating /= (1.0f + dist);

				}
				else if(water && sector->water_ratio > 0.65)
				{
					dist = sqrt( pow((float)(sector->x - current_sector->x), 2) + pow((float)(sector->y - current_sector->y), 2) );

					my_rating = 1.0f / (1.0f + pow(sector->GetEnemyDefencePower(defencePowerWeightsSea), 2.0f) + pow(sector->GetLostUnits() + 1.0f, 1.5f));
					my_rating /= (1.0f + dist);
				}
				else
					my_rating = 0;
			}

			if(my_rating > best_rating)
			{
				dest = sector;
				best_rating = my_rating;
			}
		}
	}

	return dest;
}

void AAIBrain::GetNewScoutDest(float3 *dest, int scout)
{
	*dest = ZeroVector;

	// TODO: take scouts pos into account
	float my_rating, best_rating = 0;
	AAISector *scout_sector = 0, *sector;

	const UnitDef *def = ai->Getcb()->GetUnitDef(scout);
	const AAIMovementType& scoutMoveType = ai->Getbt()->s_buildTree.GetMovementType( UnitDefId(def->id) );

	float3 pos = ai->Getcb()->GetUnitPos(scout);

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
					bool scoutDestFound = sector->determineMovePosOnContinent(&pos, continent);

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

bool AAIBrain::MetalForConstr(int unit, int workertime)
{

	int metal = (ai->Getbt()->GetUnitDef(unit).buildTime/workertime) * (ai->Getcb()->GetMetalIncome()-(ai->Getcb()->GetMetalUsage()) + ai->Getcb()->GetMetal());
	int total_cost = ai->Getbt()->GetUnitDef(unit).metalCost;

	if(metal > total_cost)
		return true;

	return false;
}

bool AAIBrain::EnergyForConstr(int unit, int /*wokertime*/)
{

	// check energy
//	int energy =  ai->Getbt()->unitList[unit-1]->buildTime * (ai->Getcb()->GetEnergyIncome()-(ai->Getcb()->GetEnergyUsage()/2));

	// TODO: FIXME: add code here

	return true;
}

bool AAIBrain::RessourcesForConstr(int /*unit*/, int /*wokertime*/)
{
	// check metal and energy
	/*if(MetalForConstr(unit) && EnergyForConstr(unit))
			return true;

	return false;*/
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
			m_baseFlatLandRatio += (*s)->GetFlatRatio();
			m_baseWaterRatio += (*s)->GetWaterRatio();
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
	else if(ai->Getmap()->map_type == WATER_MAP && ai->Getcb()->GetElevation(pos->x, pos->z) >= 0 && sector->distance_to_base <= 3)
		return true;
	else
		return false;
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
			if(!(*sector)->IsOccupiedByEnemies() && (*sector)->allied_structures < 3.0f && !ai->Getmap()->IsAlreadyOccupiedByOtherAAI(*sector))
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
		                + 4.0f * sectorDistances.GetNormalizedDeviationFromMax(candidate->second);

		if(sectorType == LAND_SECTOR)
			// prefer flat sectors without water
			myRating += ((candidate->first)->flat_ratio - (candidate->first)->water_ratio) * 16.0f;
		else if(sectorType == WATER_SECTOR)
		{
			// check for continent size (to prevent aai to expand into little ponds instead of big ocean)
			if((candidate->first)->water_ratio > 0.1f &&  (candidate->first)->ConnectedToOcean())
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

void AAIBrain::UpdateMaxCombatUnitsSpotted(const std::vector<int>& spottedCombatUnits)
{
	for(int i = 0; i < AAIBuildTable::ass_categories; ++i)
	{
		// decrease old values
		max_combat_units_spotted[i] *= 0.996f;

		// check for new max values
		if((float)spottedCombatUnits[i] > max_combat_units_spotted[i])
			max_combat_units_spotted[i] = (float)spottedCombatUnits[i];
	}
}

void AAIBrain::UpdateAttackedByValues()
{
	for(int i = 0; i < AAIBuildTable::ass_categories; ++i)
	{
		m_recentlyAttackedByCategory[i] *= 0.95f;
	}
}

void AAIBrain::AttackedBy(int combat_category_id)
{
	// update counter for current game
	m_recentlyAttackedByCategory[combat_category_id] += 1.0f;

	// update counter for memory dependent on playtime
	GamePhase gamePhase(ai->Getcb()->GetCurrentFrame());
	ai->Getbt()->attacked_by_category_current[gamePhase.GetArrayIndex()][combat_category_id] += 1.0f;
}

void AAIBrain::UpdateDefenceCapabilities()
{
	for(int i = 0; i < ai->Getbt()->assault_categories.size(); ++i)
		defence_power_vs[i] = 0;
	fill(defence_power_vs.begin(), defence_power_vs.end(), 0.0f);

	if(cfg->AIR_ONLY_MOD)
	{
		for(auto category = ai->Getbt()->s_buildTree.GetCombatUnitCatgegories().begin(); category != ai->Getbt()->s_buildTree.GetCombatUnitCatgegories().end(); ++category)
		{
			for(list<AAIGroup*>::iterator group = ai->Getgroup_list()[category->GetArrayIndex()].begin(); group != ai->Getgroup_list()[category->GetArrayIndex()].end(); ++group)
			{
				defence_power_vs[0] += (*group)->GetCombatPowerVsCategory(0);
				defence_power_vs[1] += (*group)->GetCombatPowerVsCategory(1);
				defence_power_vs[2] += (*group)->GetCombatPowerVsCategory(2);
				defence_power_vs[3] += (*group)->GetCombatPowerVsCategory(3);
			}
		}
	}
	else
	{
		// anti air power
		for(auto category = ai->Getbt()->s_buildTree.GetCombatUnitCatgegories().begin(); category != ai->Getbt()->s_buildTree.GetCombatUnitCatgegories().end(); ++category)
		{
			for(list<AAIGroup*>::iterator group = ai->Getgroup_list()[category->GetArrayIndex()].begin(); group != ai->Getgroup_list()[category->GetArrayIndex()].end(); ++group)
			{
				if((*group)->group_unit_type == ASSAULT_UNIT)
				{
					switch((*group)->category.getUnitCategory())
					{
						case EUnitCategory::GROUND_COMBAT:
							defence_power_vs[0] += (*group)->GetCombatPowerVsCategory(0);
							defence_power_vs[2] += (*group)->GetCombatPowerVsCategory(2);
							break;
						case EUnitCategory::HOVER_COMBAT:
							defence_power_vs[0] += (*group)->GetCombatPowerVsCategory(0);
							defence_power_vs[2] += (*group)->GetCombatPowerVsCategory(2);
							defence_power_vs[3] += (*group)->GetCombatPowerVsCategory(3);
							break;
						case EUnitCategory::SEA_COMBAT:
							defence_power_vs[2] += (*group)->GetCombatPowerVsCategory(2);
							defence_power_vs[3] += (*group)->GetCombatPowerVsCategory(3);
							defence_power_vs[4] += (*group)->GetCombatPowerVsCategory(4);
							break;
						case EUnitCategory::SUBMARINE_COMBAT:
							defence_power_vs[3] += (*group)->GetCombatPowerVsCategory(3);
							defence_power_vs[4] += (*group)->GetCombatPowerVsCategory(4);
							break;
					}	
				}
				else if((*group)->group_unit_type == ANTI_AIR_UNIT)
					defence_power_vs[1] += (*group)->GetCombatPowerVsCategory(1);
			}
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
	if(cfg->AIR_ONLY_MOD)
	{
		defence_power_vs[0] += ai->Getbt()->units_static[unitDefId.id].efficiency[0];
		defence_power_vs[1] += ai->Getbt()->units_static[unitDefId.id].efficiency[1];
		defence_power_vs[2] += ai->Getbt()->units_static[unitDefId.id].efficiency[2];
		defence_power_vs[3] += ai->Getbt()->units_static[unitDefId.id].efficiency[3];
	}
	else
	{
		if(ai->Getbt()->GetUnitType(unitDefId.id) == ASSAULT_UNIT)
		{
			const AAIUnitCategory& category = ai->Getbt()->s_buildTree.GetUnitCategory(unitDefId);

			switch (category.getUnitCategory())
			{
				case EUnitCategory::GROUND_COMBAT:
				{
					defence_power_vs[0] += ai->Getbt()->units_static[unitDefId.id].efficiency[0];
					defence_power_vs[2] += ai->Getbt()->units_static[unitDefId.id].efficiency[2];
					break;
				}
				case EUnitCategory::HOVER_COMBAT:
				{
					defence_power_vs[0] += ai->Getbt()->units_static[unitDefId.id].efficiency[0];
					defence_power_vs[2] += ai->Getbt()->units_static[unitDefId.id].efficiency[2];
					defence_power_vs[3] += ai->Getbt()->units_static[unitDefId.id].efficiency[3];
					break;
				}
				case EUnitCategory::SEA_COMBAT:
				{
					defence_power_vs[2] += ai->Getbt()->units_static[unitDefId.id].efficiency[2];
					defence_power_vs[3] += ai->Getbt()->units_static[unitDefId.id].efficiency[3];
					defence_power_vs[4] += ai->Getbt()->units_static[unitDefId.id].efficiency[4];
					break;
				}
				case EUnitCategory::SUBMARINE_COMBAT:
				{
					defence_power_vs[3] += ai->Getbt()->units_static[unitDefId.id].efficiency[3];
					defence_power_vs[4] += ai->Getbt()->units_static[unitDefId.id].efficiency[4];
				}
				default:
					break;
			}
		}
		else if(ai->Getbt()->GetUnitType(unitDefId.id) == ANTI_AIR_UNIT)
			defence_power_vs[1] += ai->Getbt()->units_static[unitDefId.id].efficiency[1];
	}
}

float AAIBrain::Affordable()
{
	return 25.0f /(ai->Getcb()->GetMetalIncome() + 5.0f);
}

void AAIBrain::BuildUnits()
{
	bool urgent = false;

	GamePhase gamePhase(ai->Getcb()->GetCurrentFrame());

	//-----------------------------------------------------------------------------------------------------------------
	// Calculate threat by and defence vs. the different combat categories
	//-----------------------------------------------------------------------------------------------------------------
	float attackedByCategory[AAIBuildTable::ass_categories];
	StatisticalData attackedByCatStatistics;
	StatisticalData unitsSpottedStatistics;
	StatisticalData defenceStatistics;

	for(int cat = 0; cat < AAIBuildTable::ass_categories; ++cat)
	{
		attackedByCategory[cat] = GetAttacksBy(cat, gamePhase.GetArrayIndex()) + m_recentlyAttackedByCategory[cat];
		attackedByCatStatistics.AddValue( attackedByCategory[cat] );

		unitsSpottedStatistics.AddValue(max_combat_units_spotted[cat]);
		defenceStatistics.AddValue(defence_power_vs[cat]);

	}

	attackedByCatStatistics.Finalize();
	unitsSpottedStatistics.Finalize();
	defenceStatistics.Finalize();

	
	//-----------------------------------------------------------------------------------------------------------------
	// Calculate urgency to counter each of the different combat categories
	//-----------------------------------------------------------------------------------------------------------------
	float urgency[AAIBuildTable::ass_categories];

	for(int cat = 0; cat < AAIBuildTable::ass_categories; ++cat)
	{
		urgency[cat] =    attackedByCatStatistics.GetNormalizedDeviationFromMin(attackedByCategory[cat]) 
	                    + unitsSpottedStatistics.GetNormalizedDeviationFromMin(max_combat_units_spotted[cat])
	                    + 1.5f * defenceStatistics.GetNormalizedDeviationFromMax(defence_power_vs[cat]);
	}
								 

	CombatPower combatCriteria;
	combatCriteria.vsGround    = urgency[0];
	combatCriteria.vsAir       = urgency[1];
	combatCriteria.vsHover     = urgency[2];
	combatCriteria.vsSea       = urgency[3];
	combatCriteria.vsSubmarine = urgency[4];
	combatCriteria.vsBuildings = urgency[0] + urgency[3];

	//-----------------------------------------------------------------------------------------------------------------
	// Order building of units according to determined threat/own defence capabilities
	//-----------------------------------------------------------------------------------------------------------------

	for(int i = 0; i < ai->Getexecute()->unitProductionRate; ++i)
	{
		// choose unit category dependend on map type
		if(ai->Getmap()->map_type == LAND_MAP)
		{
			AAICombatCategory unitCategory(ETargetTypeCategory::SURFACE);
		
			if( (rand()%(cfg->AIRCRAFT_RATE * 100) < 100) && !gamePhase.IsStartingPhase())
				unitCategory.setCategory(ETargetTypeCategory::AIR);

			BuildCombatUnitOfCategory(unitCategory, combatCriteria, urgent);
		}
		else if(ai->Getmap()->map_type == LAND_WATER_MAP)
		{
			//! @todo Add selection of Submarines
			int groundRatio = static_cast<int>(100.0f * ai->Getmap()->land_ratio);
			AAICombatCategory unitCategory(ETargetTypeCategory::SURFACE);

			if(rand()%100 < groundRatio)
				unitCategory.setCategory(ETargetTypeCategory::FLOATER);

			if( (rand()%(cfg->AIRCRAFT_RATE * 100) < 100) && !gamePhase.IsStartingPhase())
				unitCategory.setCategory(ETargetTypeCategory::AIR);
			

			BuildCombatUnitOfCategory(unitCategory, combatCriteria, urgent);
		}
		else if(ai->Getmap()->map_type == WATER_MAP)
		{
			//! @todo Add selection of Submarines
			AAICombatCategory unitCategory(ETargetTypeCategory::FLOATER);

			if( (rand()%(cfg->AIRCRAFT_RATE * 100) < 100) && !gamePhase.IsStartingPhase())
				unitCategory.setCategory(ETargetTypeCategory::AIR);

			BuildCombatUnitOfCategory(unitCategory, combatCriteria, urgent);
		}
	}
	
	/*if(cfg->AIR_ONLY_MOD)
	{
		// determine effectiveness vs several other units
		anti_ground_urgency = (int)( 2 + (0.05f + ground) * (2.0f * attacked_by[0] + 1.0f) * (4.0f * max_combat_units_spotted[0] + 0.2f) / (4.0f * defence_power_vs[0] + 1));
		anti_air_urgency = (int)( 2 + (0.05f + air) * (2.0f * attacked_by[1] + 1.0f) * (4.0f * max_combat_units_spotted[1] + 0.2f) / (4.0f * defence_power_vs[1] + 1));
		anti_hover_urgency = (int)( 2 + (0.05f + hover) * (2.0f * attacked_by[2] + 1.0f) * (4.0f * max_combat_units_spotted[2] + 0.2f) / (4.0f * defence_power_vs[2] + 1));
		anti_sea_urgency = (int) (2 + (0.05f + sea) * (2.0f * attacked_by[3] + 1.0f) * (4.0f * max_combat_units_spotted[3] + 0.2f) / (4.0f * defence_power_vs[3] + 1));

		for(int i = 0; i < ai->Getexecute()->unitProductionRate; ++i)
		{
			ground_eff = 0;
			air_eff = 0;
			hover_eff = 0;
			sea_eff = 0;

			k = rand()%(anti_ground_urgency + anti_air_urgency + anti_hover_urgency + anti_sea_urgency);

			if(k < anti_ground_urgency)
			{
				ground_eff = 4;
			}
			else if(k < anti_ground_urgency + anti_air_urgency)
			{
				air_eff = 4;
			}
			else if(k < anti_ground_urgency + anti_air_urgency + anti_hover_urgency)
			{
				hover_eff = 4;
			}
			else
			{
				sea_eff = 4;
			}

			allowedMoveTypes |= static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_AIR);

			BuildUnitOfMovementType(allowedMoveTypes, cost, ground_eff, air_eff, hover_eff, sea_eff, submarine_eff, stat_eff, urgent);
		}
	}*/
}

void AAIBrain::BuildCombatUnitOfCategory(const AAICombatCategory& unitCategory, const CombatPower& combatCriteria, bool urgent)
{
	UnitSelectionCriteria unitCriteria;
	unitCriteria.speed      = 0.25f;
	unitCriteria.range      = 0.25f;
	unitCriteria.cost       = 0.5f;
	unitCriteria.power      = 1.0f;
	unitCriteria.efficiency = 1.0f;

	GamePhase gamePhase(ai->Getcb()->GetCurrentFrame());

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

	UnitDefId unitDefId = ai->Getbt()->SelectCombatUnit(ai->GetSide(), unitCategory, combatCriteria, unitCriteria, 6, false);

	if( (unitDefId.isValid() == true) && (ai->Getbt()->units_dynamic[unitDefId.id].constructorsAvailable <= 0) )
	{
		if(ai->Getbt()->units_dynamic[unitDefId.id].constructorsRequested <= 0)
			ai->Getbt()->BuildFactoryFor(unitDefId.id);

		unitDefId = ai->Getbt()->SelectCombatUnit(ai->GetSide(), unitCategory, combatCriteria, unitCriteria, 6, true);
	}

	if(unitDefId.isValid() == true)
	{
		if(ai->Getbt()->units_dynamic[unitDefId.id].constructorsAvailable > 0)
		{
			const AAIUnitCategory& category = ai->Getbt()->s_buildTree.GetUnitCategory(unitDefId);
			const StatisticalData& costStatistics = ai->Getbt()->s_buildTree.GetUnitStatistics(ai->GetSide()).GetUnitCostStatistics(category);

			if(ai->Getbt()->s_buildTree.GetTotalCost(unitDefId) < cfg->MAX_COST_LIGHT_ASSAULT * costStatistics.GetMaxValue())
			{
				if(ai->Getexecute()->AddUnitToBuildqueue(unitDefId, 3, urgent))
				{
					ai->Getbt()->units_dynamic[unitDefId.id].requested += 3;
					ai->Getut()->UnitRequested(category, 3);
				}
			}
			else if(ai->Getbt()->s_buildTree.GetTotalCost(unitDefId) < cfg->MAX_COST_MEDIUM_ASSAULT * costStatistics.GetMaxValue())
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

float AAIBrain::GetAttacksBy(int combat_category, int game_period)
{
	return (ai->Getbt()->attacked_by_category_current[game_period][combat_category] + ai->Getbt()->attacked_by_category_learned[ai->Getmap()->map_type][game_period][combat_category]) / 2.0f;
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
