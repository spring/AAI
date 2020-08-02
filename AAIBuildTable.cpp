// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "System/SafeUtil.h"
#include "AAIBuildTable.h"
#include "AAI.h"
#include "AAIBrain.h"
#include "AAIExecute.h"
#include "AAIUnitTable.h"
#include "AAIConfig.h"
#include "AAIMap.h"

#include "LegacyCpp/UnitDef.h"
#include "LegacyCpp/MoveData.h"
using namespace springLegacyAI;


// all the static vars
vector< vector< vector<float> > > AAIBuildTable::attacked_by_category_learned;
vector<UnitTypeStatic> AAIBuildTable::units_static;
/*float* AAIBuildTable::max_builder_buildtime;
float* AAIBuildTable::max_builder_cost;
float* AAIBuildTable::max_builder_buildspeed;*/
vector< vector< vector<float> > > AAIBuildTable::avg_eff;
vector< vector< vector<float> > > AAIBuildTable::max_eff;
vector< vector< vector<float> > > AAIBuildTable::min_eff;
vector< vector< vector<float> > > AAIBuildTable::total_eff;
vector< vector<float> > AAIBuildTable::fixed_eff;

AAIBuildTable::AAIBuildTable(AAI* ai) :
	initialized(false)
{
	this->ai = ai;

	numOfSides = cfg->SIDES;
	sideNames.resize(numOfSides+1);
	sideNames[0] = "Neutral";

	for(int i = 0; i < numOfSides; ++i)
	{
		sideNames[i+1].assign(cfg->SIDE_NAMES[i]);
	}

	// add assault categories
	assault_categories.push_back(GROUND_ASSAULT);
	assault_categories.push_back(AIR_ASSAULT);
	assault_categories.push_back(HOVER_ASSAULT);
	assault_categories.push_back(SEA_ASSAULT);
	assault_categories.push_back(SUBMARINE_ASSAULT);

	// only set up static things if first aai instance is initialized
	if(ai->GetAAIInstance() == 1)
	{
		// set up attacked_by table
		attacked_by_category_learned.resize(3,  vector< vector<float> >(GamePhase::numberOfGamePhases, vector<float>(combat_categories, 0)));

		// init eff stats
		avg_eff.resize(numOfSides, vector< vector<float> >(combat_categories, vector<float>(combat_categories, 1.0f)));
		max_eff.resize(numOfSides, vector< vector<float> >(combat_categories, vector<float>(combat_categories, 1.0f)));
		min_eff.resize(numOfSides, vector< vector<float> >(combat_categories, vector<float>(combat_categories, 1.0f)));
		total_eff.resize(numOfSides, vector< vector<float> >(combat_categories, vector<float>(combat_categories, 1.0f)));
	}
}

AAIBuildTable::~AAIBuildTable(void)
{
	// delete common data only if last AAI instance is deleted
	if(ai->GetNumberOfAAIInstances() == 0)
	{
		attacked_by_category_learned.clear();

		avg_eff.clear();
		max_eff.clear();
		min_eff.clear();
		total_eff.clear();
	}
	unitList.clear();
}

void AAIBuildTable::Init()
{
	// get number of units and alloc memory for unit list
	const int numOfUnits = ai->GetAICallback()->GetNumUnitDefs();

	// one more than needed because 0 is dummy object (so UnitDef->id can be used to adress that unit in the array)
	units_dynamic.resize(numOfUnits+1);

	for(int i = 0; i <= numOfUnits; ++i)
	{
		units_dynamic[i].active = 0;
		units_dynamic[i].requested = 0;
		units_dynamic[i].constructorsAvailable = 0;
		units_dynamic[i].constructorsRequested = 0;
	}

	// get unit defs from game
	if(unitList.empty())
	{
		//spring first unitdef id is 1, we remap it so id = is position in array
		unitList.resize(numOfUnits+1);
		ai->GetAICallback()->GetUnitDefList(&unitList[1]);
		UnitDef* tmp = new UnitDef();
		tmp->id=0;
		unitList[0] = tmp;
		#ifndef NDEBUG
		for(int i=0; i<numOfUnits; i++) {
			assert(i == GetUnitDef(i).id);
		}
		#endif
	}

	// Try to load buildtable; if not possible, create a new one
	if(LoadBuildTable() == false)
	{
		// one more than needed because 0 is dummy object
		// (so UnitDef->id can be used to address that unit in the array)
		units_static.resize(numOfUnits+1);
		fixed_eff.resize(numOfUnits+1, vector<float>(combat_categories));

		// now calculate efficiency of combat units and get max range
		for(int i = 1; i <= numOfUnits; i++)
		{
			const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(UnitDefId(i));
			if( (category.isCombatUnit() == true) || (category.isStaticDefence() == true) )
			{
				const int side                  = ai->s_buildTree.GetSideOfUnitType(UnitDefId(i));
				const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(UnitDefId(i));
				const float cost                = ai->s_buildTree.GetTotalCost(UnitDefId(i));

				if(side > 0)
				{
					units_static[i].efficiency.resize(combat_categories, 0.2f);

					const float eff = 1.0f + 5.0f * ai->s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(category).GetNormalizedDeviationFromMin(cost);

					if(category.isGroundCombat() == true)
					{
						units_static[i].efficiency[0] = eff;
						units_static[i].efficiency[2] = eff;
						units_static[i].efficiency[5] = eff;
						fixed_eff[i][0] = eff;
						fixed_eff[i][2] = eff;
						fixed_eff[i][5] = eff;
					}
					if(category.isAirCombat() == true)
					{
						units_static[i].efficiency[0] = 0.5f * eff;
						units_static[i].efficiency[1] = eff;
						units_static[i].efficiency[2] = 0.5f * eff;
						units_static[i].efficiency[3] = 0.5f * eff;
						units_static[i].efficiency[5] = 0.5f * eff;
						fixed_eff[i][0] = eff;
						fixed_eff[i][1] = eff;
						fixed_eff[i][2] = eff;
						fixed_eff[i][3] = eff;
						fixed_eff[i][5] = eff;
					}
					else if(category.isHoverCombat() == true)
					{
						units_static[i].efficiency[0] = eff;
						units_static[i].efficiency[2] = eff;
						units_static[i].efficiency[3] = eff;
						units_static[i].efficiency[5] = eff;
						fixed_eff[i][0] = eff;
						fixed_eff[i][2] = eff;
						fixed_eff[i][3] = eff;
						fixed_eff[i][5] = eff;
					}
					else if(category.isSeaCombat() == true)
					{
						units_static[i].efficiency[2] = eff;
						units_static[i].efficiency[3] = eff;
						units_static[i].efficiency[4] = eff;
						units_static[i].efficiency[5] = eff;
						fixed_eff[i][2] = eff;
						fixed_eff[i][3] = eff;
						fixed_eff[i][4] = eff;
						fixed_eff[i][5] = eff;
					}
					else if(category.isSubmarineCombat() == true)
					{
						units_static[i].efficiency[3] = eff;
						units_static[i].efficiency[4] = eff;
						units_static[i].efficiency[5] = eff;
						fixed_eff[i][3] = eff;
						fixed_eff[i][4] = eff;
						fixed_eff[i][5] = eff;
					}
					else if(category.isStaticDefence() == true)
					{
						if(ai->s_buildTree.GetMovementType(UnitDefId(i)).isStaticLand() == true)
						{
							units_static[i].efficiency[0] = eff;
							units_static[i].efficiency[2] = eff;
							fixed_eff[i][0] = eff;
							fixed_eff[i][2] = eff;
						}
						else
						{
							units_static[i].efficiency[2] = eff;
							units_static[i].efficiency[3] = eff;
							units_static[i].efficiency[4] = eff;
							fixed_eff[i][2] = eff;
							fixed_eff[i][3] = eff;
							fixed_eff[i][4] = eff;
						}
					}
				}
				else
				{
					units_static[i].efficiency.resize(combat_categories, 0.0f);
				}
			}
			else
			{
				// get memory for eff
				units_static[i].efficiency.resize(combat_categories, 0.0f);
			}
		}

		// precache stats
		PrecacheStats();

		ai->LogConsole("New BuildTable has been created");
	}

	// only once
	if(ai->GetAAIInstance() == 1)
	{
		UpdateMinMaxAvgEfficiency();
	}

	// buildtable is initialized
	initialized = true;
}

void AAIBuildTable::InitCombatEffCache(int side)
{
	size_t maxNumberOfUnits = 0;

	for(int cat = 0; cat < combat_categories; ++cat)
	{
		const AAIUnitCategory& category = GetUnitCategoryOfCombatUnitIndex(cat);
		const size_t numberOfUnits = ai->s_buildTree.GetUnitsInCategory(category, side).size();
		if( numberOfUnits > maxNumberOfUnits)
			maxNumberOfUnits = numberOfUnits;
	}

	combat_eff.resize(maxNumberOfUnits, 0);
}

void AAIBuildTable::ConstructorRequested(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = ai->s_buildTree.GetCanConstructList(constructor).begin();  id != ai->s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		++units_dynamic[(*id).id].constructorsRequested;
	}
}

void AAIBuildTable::ConstructorFinished(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = ai->s_buildTree.GetCanConstructList(constructor).begin();  id != ai->s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		++units_dynamic[(*id).id].constructorsAvailable;
		--units_dynamic[(*id).id].constructorsRequested;
	}
}

void AAIBuildTable::ConstructorKilled(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = ai->s_buildTree.GetCanConstructList(constructor).begin();  id != ai->s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		--units_dynamic[(*id).id].constructorsAvailable;
	}
}

void AAIBuildTable::UnfinishedConstructorKilled(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = ai->s_buildTree.GetCanConstructList(constructor).begin();  id != ai->s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		--units_dynamic[(*id).id].constructorsRequested;
	}
}

void AAIBuildTable::PrecacheStats()
{
	for(int side = 1; side <= numOfSides; ++side)
	{
		// precache efficiency of metalmakers
		for(auto metalMaker = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_MAKER, side).begin(); metalMaker != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_MAKER, side).end(); ++metalMaker)
		{
			if (GetUnitDef(metalMaker->id).makesMetal <= 0.1f) {
				units_static[metalMaker->id].efficiency[0] = 12.0f/600.0f; //FIXME: this somehow is broken...
			} else {
				units_static[metalMaker->id].efficiency[0] = GetUnitDef(metalMaker->id).makesMetal/(GetUnitDef(metalMaker->id).energyUpkeep+1);
			}
		}

		// precache average metal and energy consumption of factories
		float average_metal, average_energy;
		for(auto factory = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, side).begin(); factory != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, side).end(); ++factory)
		{
			average_metal = average_energy = 0;

			for(auto unit = ai->s_buildTree.GetCanConstructList(UnitDefId(factory->id)).begin(); unit != ai->s_buildTree.GetCanConstructList(UnitDefId(factory->id)).end(); ++unit)
			{
				average_metal += ( GetUnitDef((*unit).id).metalCost * GetUnitDef(factory->id).buildSpeed ) / GetUnitDef((*unit).id).buildTime;
				average_energy += ( GetUnitDef((*unit).id).energyCost * GetUnitDef(factory->id).buildSpeed ) / GetUnitDef((*unit).id).buildTime;
			}

			units_static[factory->id].efficiency[0] = average_metal  / ai->s_buildTree.GetCanConstructList(UnitDefId(factory->id)).size();
			units_static[factory->id].efficiency[1] = average_energy / ai->s_buildTree.GetCanConstructList(UnitDefId(factory->id)).size();
		}

		// precache usage of jammers
		for(auto jammer = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SUPPORT, side).begin(); jammer != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SUPPORT, side).end(); ++jammer)
		{
			if(ai->s_buildTree.GetUnitType(*jammer).IsRadarJammer() && (GetUnitDef(jammer->id).energyUpkeep - GetUnitDef(jammer->id).energyMake > 0))
				units_static[jammer->id].efficiency[0] = GetUnitDef(jammer->id).energyUpkeep - GetUnitDef(jammer->id).energyMake;
		}

		// precache usage of radar
		for(auto radar = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SENSOR, side).begin(); radar != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SENSOR, side).end(); ++radar)
		{
			if(GetUnitDef(radar->id).energyUpkeep - GetUnitDef(radar->id).energyMake > 0)
				units_static[radar->id].efficiency[0] = GetUnitDef(radar->id).energyUpkeep - GetUnitDef(radar->id).energyMake;
		}
	}
}

AAIUnitType AAIBuildTable::GetUnitType(UnitDefId unitDefId) const
{
	if(cfg->AIR_ONLY_MOD)
	{
		return AAIUnitType(EUnitType::ANTI_SURFACE);
	}
	else
	{
		if (units_static.empty()) 
			return AAIUnitType(EUnitType::UNKNOWN);

		const AAIUnitCategory& category = ai->s_buildTree.GetUnitCategory(unitDefId);

		int side = ai->s_buildTree.GetSideOfUnitType(unitDefId)-1;

		if(side < 0)
			return AAIUnitType(EUnitType::UNKNOWN);

		if(category.isGroundCombat())
		{
			if( units_static[unitDefId.id].efficiency[1] / max_eff[side][0][1]  > 6.0f * units_static[unitDefId.id].efficiency[0] / max_eff[side][0][0] )
				return AAIUnitType(EUnitType::ANTI_AIR);
			else
				return AAIUnitType(EUnitType::ANTI_SURFACE);
		}
		else if(category.isAirCombat())
		{
			float vs_building = units_static[unitDefId.id].efficiency[5] / max_eff[side][1][5];

			float vs_units = (units_static[unitDefId.id].efficiency[0] / max_eff[side][1][0]
							+ units_static[unitDefId.id].efficiency[3] / max_eff[side][1][3]) / 2.0f;

			if( units_static[unitDefId.id].efficiency[1]  / max_eff[side][1][1] > 2 * (vs_building + vs_units) )
				return AAIUnitType(EUnitType::ANTI_AIR);
			else
			{
				if(vs_building > 4 * vs_units || GetUnitDef(unitDefId.id).type == string("Bomber"))
					return AAIUnitType(EUnitType::ANTI_STATIC);
				else
					return AAIUnitType(EUnitType::ANTI_SURFACE);
			}
		}
		else if(category.isHoverCombat())
		{
			if( units_static[unitDefId.id].efficiency[1] / max_eff[side][2][1] > 6.0f * units_static[unitDefId.id].efficiency[0] / max_eff[side][2][0] )
				return AAIUnitType(EUnitType::ANTI_AIR);
			else
				return AAIUnitType(EUnitType::ANTI_SURFACE);
		}
		else if(category.isSeaCombat())
		{
			if( units_static[unitDefId.id].efficiency[1] / max_eff[side][3][1] > 6.0f * units_static[unitDefId.id].efficiency[3] / max_eff[side][3][3] )
				return AAIUnitType(EUnitType::ANTI_AIR);
			else
				return AAIUnitType(EUnitType::ANTI_SURFACE);
		}
		else if(category.isSubmarineCombat())
		{
			if( units_static[unitDefId.id].efficiency[1] / max_eff[side][4][1] > 6 * units_static[unitDefId.id].efficiency[3] / max_eff[side][4][3] )
				return AAIUnitType(EUnitType::ANTI_AIR);
			else
				return AAIUnitType(EUnitType::ANTI_SURFACE);
		}
		else
			return AAIUnitType(EUnitType::UNKNOWN);
	}
}

bool AAIBuildTable::IsBuildingSelectable(UnitDefId building, bool water, bool mustBeConstructable) const
{
	const bool constructablePassed = !mustBeConstructable || (units_dynamic[building.id].constructorsAvailable > 0);
	const bool landCheckPassed     = !water    && ai->s_buildTree.GetMovementType(building.id).isStaticLand();
	const bool seaCheckPassed      =  water    && ai->s_buildTree.GetMovementType(building.id).isStaticSea();

	return constructablePassed && (landCheckPassed || seaCheckPassed );
}

UnitDefId AAIBuildTable::SelectPowerPlant(int side, float cost, float buildtime, float powerGeneration, bool water)
{
	UnitDefId powerPlant = SelectPowerPlant(side, cost, buildtime, powerGeneration, water, false);

	if(powerPlant.isValid() && (units_dynamic[powerPlant.id].constructorsAvailable <= 0) && (units_dynamic[powerPlant.id].constructorsRequested <= 0) )
	{
		ai->Getbt()->BuildBuilderFor(powerPlant);
		powerPlant = SelectPowerPlant(side, cost, buildtime, powerGeneration, water, true);
	}

	return powerPlant;
}

UnitDefId AAIBuildTable::SelectPowerPlant(int side, float cost, float buildtime, float powerGeneration, bool water, bool mustBeConstructable) const
{
	UnitDefId selectedPowerPlant;

	float     bestRating(0.0f);

	const AAIUnitStatistics& unitStatistics  = ai->s_buildTree.GetUnitStatistics(side);
	const StatisticalData&   generatedPowers = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::POWER_PLANT);
	const StatisticalData&   buildtimes      = unitStatistics.GetUnitBuildtimeStatistics(EUnitCategory::POWER_PLANT);
	const StatisticalData&   costs           = unitStatistics.GetUnitCostStatistics(EUnitCategory::POWER_PLANT);

	for(auto powerPlant = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::POWER_PLANT, side).begin(); powerPlant != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::POWER_PLANT, side).end(); ++powerPlant)
	{
				// check if under water or ground || water = true and building under water
		if( IsBuildingSelectable(*powerPlant, water, mustBeConstructable) )
		{
			const float generatedPower = ai->s_buildTree.GetMaxRange( *powerPlant );

			float myRating =   powerGeneration * generatedPowers.GetNormalizedDeviationFromMin(generatedPower)
						     + cost            * costs.GetNormalizedDeviationFromMax(ai->s_buildTree.GetTotalCost(*powerPlant))
							 + buildtime       * buildtimes.GetNormalizedDeviationFromMax(ai->s_buildTree.GetBuildtime(*powerPlant));

			if(myRating > bestRating)
			{
				bestRating = myRating;
				selectedPowerPlant = *powerPlant;
			}
		}
	}

	return selectedPowerPlant;
}

UnitDefId AAIBuildTable::SelectExtractor(int side, float cost, float extractedMetal, bool armed, bool water)
{
	UnitDefId extractor = SelectExtractor(side, cost, extractedMetal, armed, water, false);

	if(extractor.isValid() && (units_dynamic[extractor.id].constructorsAvailable <= 0) && (units_dynamic[extractor.id].constructorsRequested <= 0) )
	{
		ai->Getbt()->BuildBuilderFor(extractor);
		extractor = SelectExtractor(side, cost, extractedMetal, armed, water, true);
	}

	return extractor;
}

UnitDefId AAIBuildTable::SelectExtractor(int side, float cost, float extractedMetal, bool armed, bool water, bool mustBeConstructable) const
{
	UnitDefId selectedExtractorDefId;
	float     bestRating(0.0f);

	const AAIUnitStatistics& unitStatistics           = ai->s_buildTree.GetUnitStatistics(side);
	const StatisticalData&   extractedMetalStatistics = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::METAL_EXTRACTOR);
	const StatisticalData&   costStatistics           = unitStatistics.GetUnitCostStatistics(EUnitCategory::METAL_EXTRACTOR);

	for(auto extractorDefId = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).begin(); extractorDefId != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).end(); ++extractorDefId)
	{
		// check if under water or ground || water = true and building under water
		if( IsBuildingSelectable(*extractorDefId, water, mustBeConstructable) )
		{
			const float metalExtraction = ai->s_buildTree.GetMaxRange( *extractorDefId );

			float myRating =   extractedMetal * extractedMetalStatistics.GetNormalizedDeviationFromMin(metalExtraction)
						     + cost           * costStatistics.GetNormalizedDeviationFromMax(ai->s_buildTree.GetTotalCost(*extractorDefId));

			if(armed && !GetUnitDef(extractorDefId->id).weapons.empty())
				myRating += 0.2f;

			if(myRating > bestRating)
			{
				bestRating = myRating;
				selectedExtractorDefId = *extractorDefId;
			}
		}
	}

	// 0 if no unit found (list was probably empty)
	return selectedExtractorDefId.id;
}

UnitDefId AAIBuildTable::GetLargestExtractor() const
{
	UnitDefId largestExtractor;
	int largestYardMap(0);

	for(int side = 1; side <= cfg->SIDES; ++side)
	{
		for(auto extractor = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).begin(); extractor != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).end(); ++extractor)
		{
			const int yardMap = GetUnitDef(extractor->id).xsize * GetUnitDef(extractor->id).zsize;
			
			if(yardMap > largestYardMap)
			{
				largestYardMap   = yardMap;
				largestExtractor = *extractor;
			}
		}
	}

	return largestExtractor;
}

UnitDefId AAIBuildTable::SelectStorage(int side, float cost, float buildtime, float metal, float energy, bool water)
{
	UnitDefId selectedStorage = SelectStorage(side, cost, buildtime, metal, energy, water, false);

	if(selectedStorage.isValid() && (units_dynamic[selectedStorage.id].constructorsAvailable <= 0))
	{
		if(units_dynamic[selectedStorage.id].constructorsRequested <= 0)
			BuildBuilderFor(selectedStorage);

		selectedStorage = SelectStorage(side, cost, buildtime, metal, energy, water, true);
	}

	return selectedStorage;
}

UnitDefId AAIBuildTable::SelectStorage(int side, float cost, float buildtime, float metal, float energy, bool water, bool mustBeConstructable) const
{
	const AAIUnitStatistics& unitStatistics  = ai->s_buildTree.GetUnitStatistics(side);
	const StatisticalData&   costs           = unitStatistics.GetUnitCostStatistics(EUnitCategory::STORAGE);
	const StatisticalData&   buildtimes      = unitStatistics.GetUnitBuildtimeStatistics(EUnitCategory::STORAGE);
	const StatisticalData&   metalStored     = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::STORAGE);
	const StatisticalData&   energyStored    = unitStatistics.GetUnitSecondaryAbilityStatistics(EUnitCategory::STORAGE);

	UnitDefId selectedStorage;
	float bestRating(0.0f);

	for(auto storage = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STORAGE, side).begin(); storage != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STORAGE, side).end(); ++storage)
	{
		
		if( IsBuildingSelectable(storage->id, water, mustBeConstructable) )
		{
			const float myRating =    cost * costs.GetNormalizedDeviationFromMax( ai->s_buildTree.GetTotalCost(*storage) )
									+ buildtime * buildtimes.GetNormalizedDeviationFromMax( ai->s_buildTree.GetBuildtime(*storage) )
									+ metal * metalStored.GetNormalizedDeviationFromMin( ai->s_buildTree.GetMaxRange(*storage) )
									+ energy * energyStored.GetNormalizedDeviationFromMin( ai->s_buildTree.GetMaxSpeed(*storage) );

			if(myRating > bestRating)
			{
				bestRating = myRating;
				selectedStorage = *storage;
			}
		}
	}

	return selectedStorage;
}

int AAIBuildTable::GetMetalMaker(int side, float cost, float efficiency, float metal, float urgency, bool water, bool canBuild)
{
	int best_maker = 0;
	float best_rating = 0, my_rating;

	for(auto maker = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_MAKER, side).begin(); maker != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_MAKER, side).end(); ++maker)
	{

		//ai->LogConsole("MakesMetal: %f", GetUnitDef(*maker).makesMetal);
		//this somehow got broken in spring... :(
		float makesMetal = GetUnitDef(maker->id).makesMetal;
		if (makesMetal <= 0.1f) {
			makesMetal = 12.0f/600.0f;
		}

		if(canBuild && units_dynamic[maker->id].constructorsAvailable <= 0)
			my_rating = 0;
		else if(!water && GetUnitDef(maker->id).minWaterDepth <= 0)
		{
			my_rating = (pow((long double) efficiency * units_static[maker->id].efficiency[0], (long double) 1.4) + pow((long double) metal * makesMetal, (long double) 1.6))
				/(pow((long double) cost * ai->s_buildTree.GetTotalCost(*maker),(long double) 1.4) + pow((long double) urgency * ai->s_buildTree.GetBuildtime(*maker),(long double) 1.4));
		}
		else if(water && GetUnitDef(maker->id).minWaterDepth > 0)
		{
			my_rating = (pow((long double) efficiency * units_static[maker->id].efficiency[0], (long double) 1.4) + pow((long double) metal * makesMetal, (long double) 1.6))
				/(pow((long double) cost * ai->s_buildTree.GetTotalCost(*maker),(long double) 1.4) + pow((long double) urgency * ai->s_buildTree.GetBuildtime(*maker),(long double) 1.4));
		}
		else
			my_rating = 0;


		if(my_rating > best_rating)
		{
			best_rating = my_rating;
			best_maker = maker->id;
		}
	}

	return best_maker;
}

UnitDefId AAIBuildTable::RequestInitialFactory(int side, MapType mapType)
{
	//-----------------------------------------------------------------------------------------------------------------
	// create list with all factories that can be built (i.e. can be constructed by the start unit)
	//-----------------------------------------------------------------------------------------------------------------

	std::list<FactoryRatingInputData> factoryList;
	CombatPower combatPowerWeights(0.0f);
	DetermineCombatPowerWeights(combatPowerWeights, mapType);

	StatisticalData combatPowerRatingStatistics;

	for(auto factory = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, side).begin(); factory != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, side).end(); ++factory)
	{
		if(units_dynamic[factory->id].constructorsAvailable > 0)
		{
			FactoryRatingInputData data;
			CalculateFactoryRating(data, *factory, combatPowerWeights, mapType);
			factoryList.push_back(data);

			combatPowerRatingStatistics.AddValue(data.combatPowerRating);
		}
	}

	combatPowerRatingStatistics.Finalize();

	//-----------------------------------------------------------------------------------------------------------------
	// calculate final ratings and select highest rated factory
	//-----------------------------------------------------------------------------------------------------------------
	
	float bestRating(0.0f);
	UnitDefId selectedFactoryDefId;

	const StatisticalData& costStatistics = ai->s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(EUnitCategory::STATIC_CONSTRUCTOR);

	//ai->Log("Combat power weights: ground %f   air %f   hover %f   sea %f   submarine %f\n", combatPowerWeights.vsGround, combatPowerWeights.vsAir, combatPowerWeights.vsHover, combatPowerWeights.vsSea, combatPowerWeights.vsSubmarine);
	//ai->Log("Factory ratings (max combat power rating %f):", combatPowerRatingStatistics.GetMaxValue());

	for(auto factory = factoryList.begin(); factory != factoryList.end(); ++factory)
	{
		float myRating =  0.5f * costStatistics.GetNormalizedDeviationFromMax(ai->s_buildTree.GetTotalCost(factory->factoryDefId))
		                + 1.0f * combatPowerRatingStatistics.GetNormalizedDeviationFromMin(factory->combatPowerRating);  

		if(factory->canConstructBuilder)
			myRating += 0.2f;

		if(factory->canConstructScout)
			myRating += 0.4f;

		//ai->Log(" %s %f %f", ai->s_buildTree.GetUnitTypeProperties(factory->factoryDefId).m_name.c_str(), myRating, factory->combatPowerRating);
	
		if(myRating > bestRating)
		{
			bestRating = myRating;
			selectedFactoryDefId = factory->factoryDefId;
		}
	}

	//ai->Log("\n");

	//-----------------------------------------------------------------------------------------------------------------
	// order construction
	//-----------------------------------------------------------------------------------------------------------------

	if(selectedFactoryDefId.isValid())
	{
		units_dynamic[selectedFactoryDefId.id].requested += 1;
		m_factoryBuildqueue.push_front(selectedFactoryDefId);
		ConstructorRequested(selectedFactoryDefId);
	}

	return selectedFactoryDefId;
}

void AAIBuildTable::DetermineCombatPowerWeights(CombatPower& combatPowerWeights, const MapType mapType) const
{
	combatPowerWeights.vsAir       = 0.5f + (attacked_by_category_learned[mapType][0][1] + attacked_by_category_learned[mapType][1][1]);
	combatPowerWeights.vsHover     = 0.5f + (attacked_by_category_learned[mapType][0][2] + attacked_by_category_learned[mapType][1][2]);

	switch(mapType)
	{
		case LAND_MAP:
			combatPowerWeights.vsGround    = 0.5f + (attacked_by_category_learned[mapType][0][0] + attacked_by_category_learned[mapType][1][0]);
			break;
		case LAND_WATER_MAP:
			combatPowerWeights.vsGround    = 0.5f + (attacked_by_category_learned[mapType][0][0] + attacked_by_category_learned[mapType][1][0]);
			combatPowerWeights.vsSea       = 0.5f + (attacked_by_category_learned[mapType][0][3] + attacked_by_category_learned[mapType][1][3]);
			combatPowerWeights.vsSubmarine = 0.5f + (attacked_by_category_learned[mapType][0][4] + attacked_by_category_learned[mapType][1][4]);
			break;
		case WATER_MAP:
			combatPowerWeights.vsSea       = 0.5f + (attacked_by_category_learned[mapType][0][3] + attacked_by_category_learned[mapType][1][3]);
			combatPowerWeights.vsSubmarine = 0.5f + (attacked_by_category_learned[mapType][0][4] + attacked_by_category_learned[mapType][1][4]);
			break;
	}
}

void AAIBuildTable::CalculateFactoryRating(FactoryRatingInputData& ratingData, const UnitDefId factoryDefId, const CombatPower& combatPowerWeights, const MapType mapType) const
{
	ratingData.canConstructBuilder = false;
	ratingData.canConstructScout   = false;
	ratingData.factoryDefId        = factoryDefId;

	CombatPower combatPowerOfConstructedUnits(0.0f);
	int         combatUnits(0);

	const bool considerLand  = (mapType == LAND_WATER_MAP) || (mapType == LAND_MAP);
	const bool considerWater = (mapType == LAND_WATER_MAP) || (mapType == WATER_MAP);

	//-----------------------------------------------------------------------------------------------------------------
	// go through buildoptions to determine input values for calculation of factory rating
	//-----------------------------------------------------------------------------------------------------------------

	for(auto unit = ai->s_buildTree.GetCanConstructList(factoryDefId).begin(); unit != ai->s_buildTree.GetCanConstructList(factoryDefId).end(); ++unit)
	{
		switch(ai->s_buildTree.GetUnitCategory(*unit).getUnitCategory())
		{
			case EUnitCategory::GROUND_COMBAT:
				combatPowerOfConstructedUnits.vsGround += units_static[(*unit).id].efficiency[0];
				combatPowerOfConstructedUnits.vsAir    += units_static[(*unit).id].efficiency[1];
				combatPowerOfConstructedUnits.vsHover  += units_static[(*unit).id].efficiency[2];
				++combatUnits;
				break;
			case EUnitCategory::AIR_COMBAT:     // same calculation as for hover
			case EUnitCategory::HOVER_COMBAT:
				combatPowerOfConstructedUnits.vsGround += units_static[(*unit).id].efficiency[0];
				combatPowerOfConstructedUnits.vsAir    += units_static[(*unit).id].efficiency[1];
				combatPowerOfConstructedUnits.vsHover  += units_static[(*unit).id].efficiency[2];
				combatPowerOfConstructedUnits.vsSea    += units_static[(*unit).id].efficiency[3];
				++combatUnits;
				break;
			case EUnitCategory::SEA_COMBAT:
				combatPowerOfConstructedUnits.vsAir       += units_static[(*unit).id].efficiency[1];
				combatPowerOfConstructedUnits.vsHover     += units_static[(*unit).id].efficiency[2];
				combatPowerOfConstructedUnits.vsSea       += units_static[(*unit).id].efficiency[3];
				combatPowerOfConstructedUnits.vsSubmarine += units_static[(*unit).id].efficiency[4];
				++combatUnits;
				break;
			case EUnitCategory::SUBMARINE_COMBAT:
				combatPowerOfConstructedUnits.vsSea       += units_static[(*unit).id].efficiency[3];
				combatPowerOfConstructedUnits.vsSubmarine += units_static[(*unit).id].efficiency[4];
				++combatUnits;
				break;
			case EUnitCategory::MOBILE_CONSTRUCTOR:
				if( ai->s_buildTree.GetMovementType(*unit).isSeaUnit() )
				{
					if(considerWater)
						ratingData.canConstructBuilder = true;
				}
				else if(ai->s_buildTree.GetMovementType(*unit).isGround() )
				{
					if(considerLand)
						ratingData.canConstructBuilder = true;
				}
				else // always consider hover, air, or amphibious
				{
					ratingData.canConstructBuilder = true;
				}
				break;
			case EUnitCategory::SCOUT:
				if( ai->s_buildTree.GetMovementType(*unit).isSeaUnit() )
				{
					if(considerWater)
						ratingData.canConstructScout = true;
				}
				else if(ai->s_buildTree.GetMovementType(*unit).isGround() )
				{
					if(considerLand)
						ratingData.canConstructScout = true;
				}
				else // always consider hover, air, or amphibious
				{
					ratingData.canConstructScout = true;
				}
				break;
		}
	}

	//-----------------------------------------------------------------------------------------------------------------
	// calculate rating
	//-----------------------------------------------------------------------------------------------------------------
	if(combatUnits > 0)
	{
		ratingData.combatPowerRating = combatPowerOfConstructedUnits.CalculateWeightedSum(combatPowerWeights);
		ratingData.combatPowerRating /= static_cast<float>(combatUnits);
	}
}

UnitDefId AAIBuildTable::SelectStaticDefence(int side, float cost, float buildtime, float combatPower, const CombatPower& combatCriteria, float range, int randomness, bool water)
{
	UnitDefId selectedDefence = SelectStaticDefence(side, cost, buildtime, combatPower, combatCriteria, range, randomness, false, false);

	if(selectedDefence.isValid() && (units_dynamic[selectedDefence.id].constructorsAvailable <= 0))
	{
		if(units_dynamic[selectedDefence.id].constructorsRequested <= 0)
			BuildBuilderFor(selectedDefence);

		selectedDefence = SelectStaticDefence(side, cost, buildtime, combatPower, combatCriteria, range, randomness, false, true);
	}

	return selectedDefence;
}

UnitDefId AAIBuildTable::SelectStaticDefence(int side, float cost, float buildtime, float combatPower, const CombatPower& combatCriteria, float range, int randomness, bool water, bool mustBeConstructable) const
{
	// get data needed for selection
	AAIUnitCategory category(EUnitCategory::STATIC_DEFENCE);
	const std::list<UnitDefId> unitList = ai->s_buildTree.GetUnitsInCategory(category, side);

	const StatisticalData& costs      = ai->s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(category);
	const StatisticalData& ranges     = ai->s_buildTree.GetUnitStatistics(side).GetUnitPrimaryAbilityStatistics(category);
	const StatisticalData& buildtimes = ai->s_buildTree.GetUnitStatistics(side).GetUnitBuildtimeStatistics(category);

	// calculate combat power
	StatisticalData combatPowerStat;		

	for(auto defence = unitList.begin(); defence != unitList.end(); ++defence)
	{
		const UnitTypeStatic *unit = &units_static[defence->id];

		float combatPower = combatCriteria.vsGround    * unit->efficiency[0] + combatCriteria.vsAir * unit->efficiency[1] 
						  + combatCriteria.vsHover     * unit->efficiency[2] + combatCriteria.vsSea * unit->efficiency[3] 
						  + combatCriteria.vsSubmarine * unit->efficiency[4];
		combatPowerStat.AddValue(combatPower);
	}

	combatPowerStat.Finalize();

	// start with selection
	UnitDefId selectedDefence;
	float bestRating(0.0f);

	for(auto defence = unitList.begin(); defence != unitList.end(); ++defence)
	{
		if( IsBuildingSelectable(*defence, water, mustBeConstructable) )
		{
			const UnitTypeStatic *unit = &units_static[defence->id];
			const UnitTypeProperties& unitData = ai->s_buildTree.GetUnitTypeProperties(*defence);

			float myCombatPower =   combatCriteria.vsGround * unit->efficiency[0] + combatCriteria.vsAir * unit->efficiency[1] 
								  + combatCriteria.vsHover  * unit->efficiency[2] + combatCriteria.vsSea * unit->efficiency[3] 
						  		  + combatCriteria.vsSubmarine * unit->efficiency[4];

			float myRating =  cost        * costs.GetNormalizedDeviationFromMax( unitData.m_totalCost )
							+ buildtime   * buildtimes.GetNormalizedDeviationFromMax( unitData.m_buildtime )
							+ range       * ranges.GetNormalizedDeviationFromMin( unitData.m_range )
							+ combatPower * combatPowerStat.GetNormalizedDeviationFromMin( myCombatPower )
							+ 0.05f * ((float)(rand()%randomness));

			if(myRating > bestRating)
			{
				bestRating = myRating;
				selectedDefence = *defence;
			}
		}
	}

	return selectedDefence;
}

int AAIBuildTable::GetAirBase(int side, float /*cost*/, bool water, bool canBuild)
{
	float best_ranking = 0, my_ranking;
	int best_airbase = 0;

	// @todo Reactivate when detection of air bases is resolved
	/*for(auto airbase = ai->s_buildTree.GetU.begin(); airbase != units_of_category[AIR_BASE][side-1].end(); ++airbase)
	{
		// check if water
		if(canBuild && units_dynamic[*airbase].constructorsAvailable <= 0)
			my_ranking = 0;
		else if(!water && GetUnitDef(*airbase).minWaterDepth <= 0)
		{
			my_ranking = 100.f / (units_dynamic[*airbase].active + 1);
		}
		else if(water && GetUnitDef(*airbase).minWaterDepth > 0)
		{
			//my_ranking =  100 / (cost * units_static[*airbase].cost);
			my_ranking = 100.f / (units_dynamic[*airbase].active + 1);
		}
		else
			my_ranking = 0;

		if(my_ranking > best_ranking)
		{
			best_ranking = my_ranking;
			best_airbase = *airbase;
		}
	}*/
	return best_airbase;
}

UnitDefId AAIBuildTable::SelectStaticArtillery(int side, float cost, float range, bool water) const
{
	const StatisticalData& costs      = ai->s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(EUnitCategory::STATIC_ARTILLERY);
	const StatisticalData& ranges     = ai->s_buildTree.GetUnitStatistics(side).GetUnitPrimaryAbilityStatistics(EUnitCategory::STATIC_ARTILLERY);

	float bestRating(0.0f);
	UnitDefId selectedArtillery;

	for(auto artillery = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_ARTILLERY, side).begin(); artillery != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_ARTILLERY, side).end(); ++artillery)
	{
		// check if water
		if( IsBuildingSelectable(*artillery, water, false) )
		{
			const float myRating =   cost  * costs.GetNormalizedDeviationFromMax(ai->s_buildTree.GetTotalCost(*artillery))
			                       + range * ranges.GetNormalizedDeviationFromMin(ai->s_buildTree.GetMaxRange(*artillery));

			if(myRating > bestRating)
			{
				bestRating        = myRating;
				selectedArtillery = *artillery;
			}
		}
	}

	return selectedArtillery;
}

UnitDefId AAIBuildTable::SelectRadar(int side, float cost, float range, bool water)
{
	UnitDefId radar = SelectRadar(side, cost, range, water, false);

	if(radar.isValid() && (units_dynamic[radar.id].constructorsAvailable <= 0) )
	{
		if(units_dynamic[radar.id].constructorsRequested <= 0)
			BuildBuilderFor(radar);

		radar = SelectRadar(side, cost, range, water, true);
	}

	return radar;
}
	
UnitDefId AAIBuildTable::SelectRadar(int side, float cost, float range, bool water, bool mustBeConstructable) const
{
	UnitDefId selectedRadar;
	float bestRating(0.0f);

	const StatisticalData& costs  = ai->s_buildTree.GetUnitStatistics(side).GetSensorStatistics().m_radarCosts;
	const StatisticalData& ranges = ai->s_buildTree.GetUnitStatistics(side).GetSensorStatistics().m_radarRanges;

	for(auto sensor = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SENSOR, side).begin(); sensor != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SENSOR, side).end(); ++sensor)
	{
		//! @todo replace by checking unit type for radar when implemented.
		if( ai->s_buildTree.GetUnitType(sensor->id).IsRadar() )
		{
			if(IsBuildingSelectable(*sensor, water, mustBeConstructable))
			{
				const float myRating =   cost * costs.GetNormalizedDeviationFromMax(ai->s_buildTree.GetTotalCost(sensor->id))
				                       + range * ranges.GetNormalizedDeviationFromMin(ai->s_buildTree.GetMaxRange(sensor->id));

				if(myRating > bestRating)
				{
					selectedRadar = *sensor;
					bestRating    = myRating;
				}
			}
		}
	}

	return selectedRadar;
}

int AAIBuildTable::GetJammer(int side, float cost, float range, bool water, bool canBuild)
{
	int best_jammer = 0;
	float my_rating, best_rating = -10000;

	for(auto jammer = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SUPPORT, side).begin(); jammer != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SUPPORT, side).end(); ++jammer)
	{
		//! @todo Check unit type for jammer
		/*


		if(my_rating > best_rating)
		{
			if(GetUnitDef(*i).metalCost < cfg->MAX_METAL_COST)
			{
				best_jammer = *i;
				best_rating = my_rating;
			}
		}*/
	}

	return best_jammer;
}

UnitDefId AAIBuildTable::selectScout(int side, float sightRange, float cost, uint32_t movementType, int randomness, bool cloakable, bool factoryAvailable)
{
	float highestRating = 0.0f;
	UnitDefId selectedScout;

	const StatisticalData& costs       = ai->s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(EUnitCategory::SCOUT);
	const StatisticalData& sightRanges = ai->s_buildTree.GetUnitStatistics(side).GetUnitPrimaryAbilityStatistics(EUnitCategory::SCOUT);

	for(auto scout = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::SCOUT, side).begin(); scout != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::SCOUT, side).end(); ++scout)
	{
		bool movementTypeAllowed     = ai->s_buildTree.GetMovementType(scout->id).isIncludedIn(movementType);
		bool factoryPrerequisitesMet = !factoryAvailable || (units_dynamic[scout->id].constructorsAvailable > 0);

		if( movementTypeAllowed && factoryPrerequisitesMet )
		{
			float myRating =     sightRange * sightRanges.GetNormalizedDeviationFromMin(ai->s_buildTree.GetMaxRange(*scout))
							   +       cost * costs.GetNormalizedDeviationFromMax( ai->s_buildTree.GetTotalCost(*scout) );

			if(cloakable && GetUnitDef(scout->id).canCloak)
				myRating += 2.0f;

			myRating += (0.1f * ((float)(rand()%randomness)));

			if(myRating > highestRating)
			{
				highestRating = myRating;
				selectedScout = *scout;
			}
		}
	}
	
	return selectedScout;
}

void AAIBuildTable::CalculateCombatPowerForUnits(const std::list<int>& unitList, const AAICombatCategory& category, const CombatPower& combatCriteria, std::vector<float>& combatPowerValues, StatisticalData& combatPowerStat, StatisticalData& combatEfficiencyStat)
{
	int i = 0;
	for(std::list<int>::const_iterator id = unitList.begin(); id != unitList.end(); ++id)
	{
		const UnitTypeStatic *unit = &units_static[*id];
		const UnitTypeProperties& unitData = ai->s_buildTree.GetUnitTypeProperties(UnitDefId(*id));

		float combatPower =	  combatCriteria.vsGround    * unit->efficiency[0] 
							+ combatCriteria.vsAir       * unit->efficiency[1] 
							+ combatCriteria.vsHover     * unit->efficiency[2]
							+ combatCriteria.vsSea       * unit->efficiency[3] 
							+ combatCriteria.vsSubmarine * unit->efficiency[4] 
							+ combatCriteria.vsBuildings * unit->efficiency[5];
		float combatEff = combatPower / unitData.m_totalCost;

		combatPowerStat.AddValue(combatPower);
		combatEfficiencyStat.AddValue(combatEff);
		combatPowerValues[i] = combatPower;

		++i;
	}

	combatPowerStat.Finalize();
	combatEfficiencyStat.Finalize();
}

UnitDefId AAIBuildTable::SelectCombatUnit(int side, const AAICombatCategory& category, const CombatPower& combatCriteria, const UnitSelectionCriteria& unitCriteria, int randomness, bool canBuild)
{
	//-----------------------------------------------------------------------------------------------------------------
	// get data needed for selection
	//-----------------------------------------------------------------------------------------------------------------
	const std::list<int> unitList = ai->s_buildTree.GetUnitsInCombatCategory(category, side);

	const StatisticalData& costStatistics  = ai->s_buildTree.GetUnitStatistics(side).GetCombatCostStatistics(category);
	const StatisticalData& rangeStatistics = ai->s_buildTree.GetUnitStatistics(side).GetCombatRangeStatistics(category);
	const StatisticalData& speedStatistics = ai->s_buildTree.GetUnitStatistics(side).GetCombatSpeedStatistics(category);

	StatisticalData combatPowerStat;		               // absolute combat power
	StatisticalData combatEfficiencyStat;	               // combat power related to unit cost
	std::vector<float> combatPowerValues(unitList.size()); // values for individual units (in order of appearance in unitList)

	CalculateCombatPowerForUnits(unitList, category, combatCriteria, combatPowerValues, combatPowerStat, combatEfficiencyStat);

	//-----------------------------------------------------------------------------------------------------------------
	// begin with selection
	//-----------------------------------------------------------------------------------------------------------------
	UnitDefId selectedUnitType;
	float bestRating = 0.0f;

	//ai->Log("Selecting ground unit:\n");

	int i = 0;
	for(std::list<int>::const_iterator id = unitList.begin(); id != unitList.end(); ++id)
	{
		if(    (canBuild == false)
			|| ((canBuild == true) && (units_dynamic[*id].constructorsAvailable > 0)) )
		{
			const UnitTypeStatic *unit = &units_static[*id];
			const UnitTypeProperties& unitData = ai->s_buildTree.GetUnitTypeProperties(UnitDefId(*id));

			float combatEff = combatPowerValues[i] / unitData.m_totalCost;

			float myRating =  unitCriteria.cost  * costStatistics.GetNormalizedDeviationFromMax( unitData.m_totalCost )
							+ unitCriteria.range * rangeStatistics.GetNormalizedDeviationFromMin( unitData.m_range )
							+ unitCriteria.speed * speedStatistics.GetNormalizedDeviationFromMin( unitData.m_maxSpeed )
							+ unitCriteria.power * combatPowerStat.GetNormalizedDeviationFromMin( combatPowerValues[i] )
							+ unitCriteria.efficiency * combatEfficiencyStat.GetNormalizedDeviationFromMin( combatEff )
							+ 0.05f * ((float)(rand()%randomness));

			/*ai->Log("%s %f %f %f %f %f %i %i\n", unitData.m_name.c_str(), 
			costStatistics.GetNormalizedDeviationFromMax( unitData.m_totalCost ), combatPowerStat.GetNormalizedDeviationFromMin( combatPower ),
			speedStatistics.GetNormalizedDeviationFromMin( unitData.m_maxSpeed ), combatPowerStat.GetNormalizedDeviationFromMin( combatPower ),
			combatEfficiencyStat.GetNormalizedDeviationFromMin( combatEff ), units_dynamic[*id].constructorsRequested, units_dynamic[*id].constructorsAvailable
			);*/

			if(myRating > bestRating)
			{
				bestRating          = myRating;
				selectedUnitType.id = *id;
			}
		}

		++i;
	}
	
	return selectedUnitType;
}

void AAIBuildTable::UpdateTable(const UnitDef* def_killer, int killer, const UnitDef *def_killed, int killed)
{
	// buidling killed
	if(killed == 5)
	{
		// stationary defence killed
		if(ai->s_buildTree.GetUnitCategory(UnitDefId(def_killed->id)).isStaticDefence() == true)
		{
			float change = cfg->LEARN_SPEED * units_static[def_killed->id].efficiency[killer] / units_static[def_killer->id].efficiency[killed];

			if(change > 0.5)
				change = 0.5;
			else if(change < cfg->MIN_EFFICIENCY/2.0f)
				change = cfg->MIN_EFFICIENCY/2.0f;

			units_static[def_killer->id].efficiency[killed] += change;
			units_static[def_killed->id].efficiency[killer] -= change;

			if(units_static[def_killed->id].efficiency[killer] < cfg->MIN_EFFICIENCY)
				units_static[def_killed->id].efficiency[killer] = cfg->MIN_EFFICIENCY;
		}
		// other building killed
		else
		{
			if(units_static[def_killer->id].efficiency[5] < 8)
			{
				if(killer == 1)	// aircraft
					units_static[def_killer->id].efficiency[5] += cfg->LEARN_SPEED / 3.0f;
				else			// other assault units
					units_static[def_killer->id].efficiency[5] += cfg->LEARN_SPEED / 9.0f;
			}
		}
	}
	// unit killed
	else
	{
		float change = cfg->LEARN_SPEED * units_static[def_killed->id].efficiency[killer] / units_static[def_killer->id].efficiency[killed];

		if(change > 0.5)
			change = 0.5;
		else if(change < cfg->MIN_EFFICIENCY/2.0f)
			change = cfg->MIN_EFFICIENCY/2.0f;

		units_static[def_killer->id].efficiency[killed] += change;
		units_static[def_killed->id].efficiency[killer] -= change;

		if(units_static[def_killed->id].efficiency[killer] < cfg->MIN_EFFICIENCY)
			units_static[def_killed->id].efficiency[killer] = cfg->MIN_EFFICIENCY;
	}
}

void AAIBuildTable::UpdateMinMaxAvgEfficiency()
{
	int counter;
	float min, max, sum;

	for(int side = 0; side < numOfSides; ++side)
	{
		for(int i = 0; i < combat_categories; ++i)
		{
			for(int j = 0; j < combat_categories; ++j)
			{
				const AAIUnitCategory& killerUnitCategory    = GetUnitCategoryOfCombatUnitIndex(i);
				const AAIUnitCategory& destroyedUnitCategory = GetUnitCategoryOfCombatUnitIndex(j);
				counter = 0;

				// update max and avg efficiency of i versus j
				max = 0;
				min = 100000;
				sum = 0;

				for(auto unit = ai->s_buildTree.GetUnitsInCategory(killerUnitCategory, side+1).begin(); unit != ai->s_buildTree.GetUnitsInCategory(killerUnitCategory, side+1).end(); ++unit)
				{
					// only count anti air units vs air and assault units vs non air
					if(    (destroyedUnitCategory.isAirCombat()  && ai->s_buildTree.GetUnitType(*unit).IsAntiAir()) 
					    || (!destroyedUnitCategory.isAirCombat() && !ai->s_buildTree.GetUnitType(*unit).IsAntiAir()) )
					{
						sum += units_static[unit->id].efficiency[j];

						if(units_static[unit->id].efficiency[j] > max)
							max = units_static[unit->id].efficiency[j];

						if(units_static[unit->id].efficiency[j] < min)
							min = units_static[unit->id].efficiency[j];

						++counter;
					}
				}

				if(counter > 0)
				{
					avg_eff[side][i][j] = sum / counter;
					max_eff[side][i][j] = max;
					min_eff[side][i][j] = min;

					total_eff[side][i][j] = max - min;

					if(total_eff[side][i][j] <= 0)
						total_eff[side][i][j] = 1;

					if(max_eff[side][i][j] <= 0)
						max_eff[side][i][j] = 1;

					if(avg_eff[side][i][j] <= 0)
						avg_eff[side][i][j] = 1;

					if(min_eff[side][i][j] <= 0)
						min_eff[side][i][j] = 1;
				}
				else
				{
					// set to 1 to prevent division by zero crashes
					max_eff[side][i][j] = 1;
					min_eff[side][i][j] = 1;
					avg_eff[side][i][j] = 1;
					total_eff[side][i][j] = 1;
				}

				//ai->Log("min_eff[%i][%i] %f;  max_eff[%i][%i] %f\n", i, j, this->min_eff[i][j], i, j, this->max_eff[i][j]);
			}
		}
	}
}

std::string AAIBuildTable::GetBuildCacheFileName()
{
	return cfg->GetFileName(ai->GetAICallback(), cfg->getUniqueName(ai->GetAICallback(), true, true, false, false), MOD_LEARN_PATH, "_buildcache.txt", true);
}


// returns true if cache found
bool AAIBuildTable::LoadBuildTable()
{
	// stop further loading if already done
	if(units_static.empty() == false)
	{
		return true;
	}
	else 
	{
		// load data
		FILE *load_file;

		int tmp = 0, cat = 0;
		size_t bo = 0, bb = 0;
		const std::string filename = GetBuildCacheFileName();
		// load units if file exists
		if((load_file = fopen(filename.c_str(), "r")))
		{
			char buffer[1024];
			// check if correct version
			fscanf(load_file, "%s", buffer);

			if(strcmp(buffer, MOD_LEARN_VERSION))
			{
				ai->LogConsole("Buildtable version out of date - creating new one");
				return false;
			}

			int counter = 0;

			// load attacked_by table
			for(int map = 0; map <= (int)WATER_MAP; ++map)
			{
				for(int t = 0; t < 4; ++t)
				{
					for(int category = 0; category < combat_categories; ++category)
					{
						++counter;
						fscanf(load_file, "%f ", &attacked_by_category_learned[map][t][category]);
					}
				}
			}

			units_static.resize(unitList.size());
			fixed_eff.resize(unitList.size(), vector<float>(combat_categories));

			for(int i = 1; i < unitList.size(); ++i)
			{
				// get memory for eff
				units_static[i].efficiency.resize(combat_categories);

				// load eff
				for(int k = 0; k < combat_categories; ++k)
				{
					fscanf(load_file, "%f ", &units_static[i].efficiency[k]);
					fixed_eff[i][k] = units_static[i].efficiency[k];
				}
			}

			fclose(load_file);
			return true;
		}
	}



	return false;
}

void AAIBuildTable::SaveBuildTable(const GamePhase& gamePhase, const AttackedByFrequency& atackedByFrequencies, const MapType& mapType)
{
	// reset factory ratings
	for(int s = 0; s < cfg->SIDES; ++s)
	{
		for(auto fac = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, s+1).begin(); fac != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, s+1).end(); ++fac)
		{
			units_static[fac->id].efficiency[5] = -1;
			units_static[fac->id].efficiency[4] = 0;
		}
	}
	// reset builder ratings
	for(int s = 0; s < cfg->SIDES; ++s)
	{
		for(auto builder = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, s+1).begin(); builder != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, s+1).end(); ++builder)
			units_static[builder->id].efficiency[5] = -1;
	}

	const std::string filename = GetBuildCacheFileName();
	FILE *save_file = fopen(filename.c_str(), "w+");

	// file version
	fprintf(save_file, "%s \n", MOD_LEARN_VERSION);

	// update attacked_by values
	for(GamePhase updateGamePhase(0); updateGamePhase <= gamePhase; updateGamePhase.EnterNextGamePhase())
	{
		for(AAICombatUnitCategory category(AAICombatUnitCategory::firstCombatUnitCategory); category.End() == false; category.Next())
		{
				attacked_by_category_learned[mapType][updateGamePhase.GetArrayIndex()][category.GetArrayIndex()] =
						0.75f * attacked_by_category_learned[mapType][updateGamePhase.GetArrayIndex()][category.GetArrayIndex()] +
						0.25f * atackedByFrequencies.GetAttackFrequency(updateGamePhase, category);
		}
	}

	// save attacked_by table
	for(int map = 0; map <= WATER_MAP; ++map)
	{
		for(int t = 0; t < GamePhase::numberOfGamePhases; ++t)
		{
			for(int cat = 0; cat < combat_categories; ++cat)
			{
				fprintf(save_file, "%f ", attacked_by_category_learned[map][t][cat]);
				fprintf(save_file, "\n");
			}
		}
	}

	for(int i = 1; i < unitList.size(); ++i)
	{
		// save combat eff
		for(int k = 0; k < combat_categories; ++k)
			fprintf(save_file, "%f ", units_static[i].efficiency[k]);

		fprintf(save_file, "\n");
	}

	fclose(save_file);
}

void AAIBuildTable::BuildFactoryFor(int unit_def_id)
{
	int constructor = 0;
	float best_rating = -100000.0f, my_rating;

	float cost = 1.0f;
	float buildspeed = 1.0f;

	// determine max values
	float max_buildtime = 0;
	float max_buildspeed = 0;
	float max_cost = 0;

	for(std::list<UnitDefId>::const_iterator factory = ai->s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).begin();  factory != ai->s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).end(); ++factory)
	{
		if(ai->s_buildTree.GetTotalCost(*factory) > max_cost)
			max_cost = ai->s_buildTree.GetTotalCost(*factory);

		if(GetUnitDef((*factory).id).buildTime > max_buildtime)
			max_buildtime = GetUnitDef((*factory).id).buildTime;

		if(GetUnitDef((*factory).id).buildSpeed > max_buildspeed)
			max_buildspeed = GetUnitDef((*factory).id).buildSpeed;
	}

	// look for best builder to do the job
	for(std::list<UnitDefId>::const_iterator factory = ai->s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).begin();  factory != ai->s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).end(); ++factory)
	{
		if(units_dynamic[(*factory).id].active + units_dynamic[(*factory).id].requested + units_dynamic[(*factory).id].under_construction < cfg->MAX_FACTORIES_PER_TYPE)
		{
			my_rating = buildspeed * (GetUnitDef((*factory).id).buildSpeed / max_buildspeed)
				- (GetUnitDef((*factory).id).buildTime / max_buildtime)
				- cost * (ai->s_buildTree.GetTotalCost(*factory) / max_cost);

			// prefer builders that can be built atm
			if(units_dynamic[(*factory).id].constructorsAvailable > 0)
				my_rating += 2.0f;

			// prevent AAI from requesting factories that cannot be built within the current base
			if(ai->s_buildTree.GetMovementType(*factory).isStaticLand() == true)
			{
				if(ai->Getbrain()->GetBaseFlatLandRatio() > 0.1f)
					my_rating *= ai->Getbrain()->GetBaseFlatLandRatio();
				else
					my_rating = -100000.0f;
			}
			else if(ai->s_buildTree.GetMovementType(*factory).isStaticSea() == true)
			{
				if(ai->Getbrain()->GetBaseWaterRatio() > 0.1f)
					my_rating *= ai->Getbrain()->GetBaseWaterRatio();
				else
					my_rating = -100000.0f;
			}

			if(my_rating > best_rating)
			{
				best_rating = my_rating;
				constructor = (*factory).id;
			}
		}
	}

	if(constructor && units_dynamic[constructor].requested + units_dynamic[constructor].under_construction <= 0)
	{
		ConstructorRequested(UnitDefId(constructor));

		units_dynamic[constructor].requested += 1;
		m_factoryBuildqueue.push_back(UnitDefId(constructor));

		// factory requested
		if(ai->s_buildTree.GetMovementType(UnitDefId(constructor)).isStatic() == true)
		{
			if(units_dynamic[constructor].constructorsAvailable + units_dynamic[constructor].constructorsRequested <= 0)
			{
				ai->Log("BuildFactoryFor(%s) is requesting builder for %s\n", ai->s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
				BuildBuilderFor(UnitDefId(constructor));
			}

			// debug
			ai->Log("BuildFactoryFor(%s) requested %s\n", ai->s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
		}
		// mobile constructor requested
		else
		{
			// only mark as urgent (unit gets added to front of buildqueue) if no constructor of that type already exists
			bool urgent = (units_dynamic[constructor].active > 0) ? false : true;

			if(ai->Getexecute()->AddUnitToBuildqueue(UnitDefId(constructor), 1, urgent))
			{
				// increase counter if mobile factory is a builder as well
				if(ai->s_buildTree.GetUnitType(UnitDefId(constructor)).IsBuilder())
					ai->Getut()->futureBuilders += 1;

				if(units_dynamic[constructor].constructorsAvailable + units_dynamic[constructor].constructorsRequested <= 0)
				{
					ai->Log("BuildFactoryFor(%s) is requesting factory for %s\n", ai->s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
					BuildFactoryFor(constructor);
				}

				// debug
				ai->Log("BuildFactoryFor(%s) requested %s\n", ai->s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
			}
			else
			{
				//something went wrong -> decrease values
				units_dynamic[constructor].requested -= 1;

				// decrease "contructor requested" counters of buildoptions 
				UnfinishedConstructorKilled(UnitDefId(constructor));
			}
		}
	}
}

// tries to build another builder for a certain building
void AAIBuildTable::BuildBuilderFor(UnitDefId building, float cost, float buildtime, float buildpower, float constructableBuilderBonus)
{
	StatisticalData costStatistics;
	StatisticalData buildtimeStatistics;
	StatisticalData buildpowerStatistics;

	for(std::list<UnitDefId>::const_iterator builder = ai->s_buildTree.GetConstructedByList(building).begin();  builder != ai->s_buildTree.GetConstructedByList(building).end(); ++builder)
	{
		costStatistics.AddValue( ai->s_buildTree.GetTotalCost(*builder) );
		buildtimeStatistics.AddValue( ai->s_buildTree.GetBuildtime(*builder) );
		buildpowerStatistics.AddValue( ai->s_buildTree.GetBuildspeed(*builder) );
	}

	costStatistics.Finalize();
	buildtimeStatistics.Finalize();
	buildpowerStatistics.Finalize();

	float bestRating = 0.0f;
	UnitDefId selectedBuilder;


	// look for best builder to do the job
	for(std::list<UnitDefId>::const_iterator builder = ai->s_buildTree.GetConstructedByList(building).begin();  builder != ai->s_buildTree.GetConstructedByList(building).end(); ++builder)
	{
		// prevent ai from ordering too many builders of the same type/commanders/builders that cant be built atm
		if(units_dynamic[(*builder).id].active + units_dynamic[(*builder).id].under_construction + units_dynamic[(*builder).id].requested < cfg->MAX_BUILDERS_PER_TYPE)
		{
			float myRating = cost       * costStatistics.GetNormalizedDeviationFromMax( ai->s_buildTree.GetTotalCost(*builder) )
			               + buildtime  * buildtimeStatistics.GetNormalizedDeviationFromMax( ai->s_buildTree.GetBuildtime(*builder) )
				           + buildpower * buildpowerStatistics.GetNormalizedDeviationFromMin( ai->s_buildTree.GetBuildspeed(*builder) );

			if(units_dynamic[(*builder).id].constructorsAvailable > 0)
				myRating += constructableBuilderBonus;

			if(myRating > bestRating)
			{
				bestRating      = myRating;
				selectedBuilder = *builder;
			}
		}
	}

	if( (selectedBuilder.isValid() == true) && (units_dynamic[selectedBuilder.id].under_construction + units_dynamic[selectedBuilder.id].requested <= 0) )
	{
		// build factory if necessary
		if(units_dynamic[selectedBuilder.id].constructorsAvailable + units_dynamic[selectedBuilder.id].constructorsRequested <= 0)
		{
			ai->Log("BuildBuilderFor(%s) is requesting factory for %s\n", ai->s_buildTree.GetUnitTypeProperties(building).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(selectedBuilder).m_name.c_str());

			BuildFactoryFor(selectedBuilder.id);
		}

		// only mark as urgent (unit gets added to front of buildqueue) if no constructor of that type already exists
		bool urgent = (units_dynamic[selectedBuilder.id].active > 0) ? false : true;

		if(ai->Getexecute()->AddUnitToBuildqueue(selectedBuilder, 1, urgent))
		{
			units_dynamic[selectedBuilder.id].requested += 1;
			ai->Getut()->futureBuilders += 1;
			ai->Getut()->UnitRequested(AAIUnitCategory(EUnitCategory::MOBILE_CONSTRUCTOR));

			// set all its buildoptions buildable
			ConstructorRequested(selectedBuilder);

			// debug
			ai->Log("BuildBuilderFor(%s) requested %s\n", ai->s_buildTree.GetUnitTypeProperties(building).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(selectedBuilder).m_name.c_str());
		}
	}
}


/*void AAIBuildTable::AddAssistant(uint32_t allowedMovementTypes, bool canBuild)
{
	int builder = 0;
	float best_rating = -10000, my_rating;

	int side = ai->GetSide();

	float cost = 1.0f;
	float buildspeed = 2.0f;
	float urgency = 1.0f;

	for(auto unit = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, side).begin();  unit != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, side).end(); ++unit)
	{
		if(ai->s_buildTree.GetMovementType(UnitDefId(*unit)).isIncludedIn(allowedMovementTypes) == true)
		{
			if( (!canBuild || units_dynamic[unit->id].constructorsAvailable > 0)
				&& units_dynamic[unit->id].active + units_dynamic[unit->id].under_construction + units_dynamic[unit->id].requested < cfg->MAX_BUILDERS_PER_TYPE)
			{
				if( GetUnitDef(unit->id).buildSpeed >= (float)cfg->MIN_ASSISTANCE_BUILDTIME && GetUnitDef(unit->id).canAssist)
				{
					my_rating = cost * (ai->s_buildTree.GetTotalCost(UnitDefId(unit->id)) / max_cost[MOBILE_CONSTRUCTOR][ai->GetSide()-1])
								+ buildspeed * (GetUnitDef(unit->id).buildSpeed / max_value[MOBILE_CONSTRUCTOR][ai->GetSide()-1])
								- urgency * (GetUnitDef(unit->id).buildTime / max_buildtime[MOBILE_CONSTRUCTOR][ai->GetSide()-1]);

					if(my_rating > best_rating)
					{
						best_rating = my_rating;
						builder = unit->id;
					}
				}
			}
		}
	}

	if(builder && units_dynamic[builder].under_construction + units_dynamic[builder].requested < 1)
	{
		// build factory if necessary
		if(units_dynamic[builder].constructorsAvailable <= 0)
			BuildFactoryFor(builder);

		if(ai->Getexecute()->AddUnitToBuildqueue(UnitDefId(builder), 1, true))
		{
			units_dynamic[builder].requested += 1;
			ai->Getut()->futureBuilders += 1;
			ai->Getut()->UnitRequested(AAIUnitCategory(EUnitCategory::MOBILE_CONSTRUCTOR));

			// increase number of requested builders of all buildoptions
			ConstructorRequested(UnitDefId(builder));

			//ai->Log("AddAssister() requested: %s %i \n", GetUnitDef(builder).humanName.c_str(), units_dynamic[builder].requested);
		}
	}
}*/


bool AAIBuildTable::IsArty(int id)
{
	if(!GetUnitDef(id).weapons.empty())
	{
		float max_range = 0;
//		const WeaponDef *longest = 0;

		for(vector<UnitDef::UnitDefWeapon>::const_iterator weapon = GetUnitDef(id).weapons.begin(); weapon != GetUnitDef(id).weapons.end(); ++weapon)
		{
			if(weapon->def->range > max_range)
			{
				max_range = weapon->def->range;
//				longest = weapon->def;
			}
		}

		// veh, kbot, hover or ship
		if(GetUnitDef(id).movedata)
		{
			if(GetUnitDef(id).movedata->moveFamily == MoveData::Tank || GetUnitDef(id).movedata->moveFamily == MoveData::KBot)
			{
				if(max_range > cfg->GROUND_ARTY_RANGE)
					return true;
			}
			else if(GetUnitDef(id).movedata->moveFamily == MoveData::Ship)
			{
				if(max_range > cfg->SEA_ARTY_RANGE)
					return true;
			}
			else if(GetUnitDef(id).movedata->moveFamily == MoveData::Hover)
			{
				if(max_range > cfg->HOVER_ARTY_RANGE)
					return true;
			}
		}
		else // aircraft
		{
			if(cfg->AIR_ONLY_MOD)
			{
				if(max_range > cfg->GROUND_ARTY_RANGE)
					return true;
			}
		}

		if(GetUnitDef(id).highTrajectoryType == 1)
			return true;
	}

	return false;
}

bool AAIBuildTable::IsAttacker(int id)
{
	for(list<int>::iterator i = cfg->ATTACKERS.begin(); i != cfg->ATTACKERS.end(); ++i)
	{
		if(*i == id)
			return true;
	}

	return false;
}


bool AAIBuildTable::IsTransporter(int id)
{
	for(list<int>::iterator i = cfg->TRANSPORTERS.begin(); i != cfg->TRANSPORTERS.end(); ++i)
	{
		if(*i == id)
			return true;
	}

	return false;
}

bool AAIBuildTable::AllowedToBuild(int id)
{
	for(list<int>::iterator i = cfg->DONT_BUILD.begin(); i != cfg->DONT_BUILD.end(); ++i)
	{
		if(*i == id)
			return false;
	}

	return true;
}

bool AAIBuildTable::IsMetalMaker(int id)
{
	for(list<int>::iterator i = cfg->METAL_MAKERS.begin(); i != cfg->METAL_MAKERS.end(); ++i)
	{
		if(*i == id)
			return true;
	}

	return false;
}

bool AAIBuildTable::IsMissileLauncher(int def_id)
{
	for(vector<UnitDef::UnitDefWeapon>::const_iterator weapon = GetUnitDef(def_id).weapons.begin(); weapon != GetUnitDef(def_id).weapons.end(); ++weapon)
	{
		if(weapon->def->stockpile)
			return true;
	}

	return false;
}

bool AAIBuildTable::IsDeflectionShieldEmitter(int def_id)
{
	for(vector<UnitDef::UnitDefWeapon>::const_iterator weapon = GetUnitDef(def_id).weapons.begin(); weapon != GetUnitDef(def_id).weapons.end(); ++weapon)
	{
		if(weapon->def->isShield)
			return true;
	}

	return false;
}

int AAIBuildTable::GetIDOfAssaultCategory(const AAIUnitCategory& category) const
{
	if(category.isCombatUnit() == true)
	{
		AAICombatUnitCategory cat(category);
		return cat.GetArrayIndex();
	}
	else if(category.isBuilding() == true)
		return 5;
	else
		return -1;
}

AAIUnitCategory AAIBuildTable::GetUnitCategoryOfCombatUnitIndex(int index) const
{
	//! @todo Use array instead of switch (is only called during initialization, thus not performance critical)
	switch(index)
	{
		case 0:
			return AAIUnitCategory(EUnitCategory::GROUND_COMBAT);
		case 1:
			return AAIUnitCategory(EUnitCategory::AIR_COMBAT);
		case 2:
			return AAIUnitCategory(EUnitCategory::HOVER_COMBAT);
		case 3:
			return AAIUnitCategory(EUnitCategory::SEA_COMBAT);
		case 4:
			return AAIUnitCategory(EUnitCategory::SUBMARINE_COMBAT);
		case 5:
			return AAIUnitCategory(EUnitCategory::STATIC_DEFENCE);
		default:
			return AAIUnitCategory(EUnitCategory::UNKNOWN);
	}
}
