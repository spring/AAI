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

AttackedByRatesPerGamePhaseAndMapType AAIBuildTable::s_attackedByRates;

AAIBuildTable::AAIBuildTable(AAI* ai)
{
	this->ai = ai;
}

AAIBuildTable::~AAIBuildTable(void)
{
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

	// If first instance of AAI: Try to load combat power&attacked by rates; if no stored data availble init with default values
	// (combat power and attacked by rates are both static)
	if(ai->GetAAIInstance() == 1)
	{
		if(LoadModLearnData() == false)
		{
			ai->s_buildTree.InitCombatPowerOfUnits(ai->GetAICallback());
			ai->LogConsole("New BuildTable has been created");
		}
	}
}

void AAIBuildTable::ConstructorRequested(UnitDefId constructor)
{
	for(const auto unitDefId : ai->s_buildTree.GetCanConstructList(constructor))
	{
		++units_dynamic[unitDefId.id].constructorsRequested;
	}
}

void AAIBuildTable::ConstructorFinished(UnitDefId constructor)
{
	for(const auto unitDefId : ai->s_buildTree.GetCanConstructList(constructor))
	{
		++units_dynamic[unitDefId.id].constructorsAvailable;
		--units_dynamic[unitDefId.id].constructorsRequested;
	}
}

void AAIBuildTable::ConstructorKilled(UnitDefId constructor)
{
	for(const auto unitDefId : ai->s_buildTree.GetCanConstructList(constructor))
	{
		--units_dynamic[unitDefId.id].constructorsAvailable;
	}
}

void AAIBuildTable::UnfinishedConstructorKilled(UnitDefId constructor)
{
	for(const auto unitDefId : ai->s_buildTree.GetCanConstructList(constructor))
	{
		--units_dynamic[unitDefId.id].constructorsRequested;
	}
}

bool AAIBuildTable::IsBuildingSelectable(UnitDefId building, bool water, bool mustBeConstructable) const
{
	const bool constructablePassed = !mustBeConstructable || (units_dynamic[building.id].constructorsAvailable > 0);
	const bool landCheckPassed     = !water    && ai->s_buildTree.GetMovementType(building.id).IsStaticLand();
	const bool seaCheckPassed      =  water    && ai->s_buildTree.GetMovementType(building.id).IsStaticSea();

	return constructablePassed && (landCheckPassed || seaCheckPassed );
}

UnitDefId AAIBuildTable::SelectPowerPlant(int side, const PowerPlantSelectionCriteria& selectionCriteria, bool water)
{
	UnitDefId powerPlant = SelectPowerPlant(side, selectionCriteria, water, false);

	if(powerPlant.IsValid() && (units_dynamic[powerPlant.id].constructorsAvailable + units_dynamic[powerPlant.id].constructorsRequested <= 0) )
	{
		RequestBuilderFor(powerPlant);
		powerPlant = SelectPowerPlant(side, selectionCriteria, water, true);
	}

	return powerPlant;
}

UnitDefId AAIBuildTable::SelectPowerPlant(int side, const PowerPlantSelectionCriteria& selectionCriteria, bool water, bool mustBeConstructable) const
{
	//-----------------------------------------------------------------------------------------------------------------
	// determine the power plant whose generated energy exceeds the current total energy generation the least (-> to
	// discard more advanced plants in the beginning)
	//-----------------------------------------------------------------------------------------------------------------
	const float energyGenerationLimit = selectionCriteria.currentEnergyIncome+1.0f;

	const AAIUnitStatistics& unitStatistics = ai->s_buildTree.GetUnitStatistics(ai->GetSide());
	float maxPower = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::POWER_PLANT).GetMaxValue();

	for(auto powerPlant : ai->s_buildTree.GetUnitsInCategory(EUnitCategory::POWER_PLANT, side))
	{
		if( IsBuildingSelectable(powerPlant, water, false) )
		{
			const float generatedEnergy = ai->s_buildTree.GetPrimaryAbility(powerPlant);

			if( (generatedEnergy > energyGenerationLimit) && (generatedEnergy < maxPower) )
				maxPower = generatedEnergy;
		}
	}

	//-----------------------------------------------------------------------------------------------------------------
	// calculate statistics for remaining plants
	//-----------------------------------------------------------------------------------------------------------------

	StatisticalData generatedEnergies;
	StatisticalData buildtimes;
	StatisticalData costs;
	std::list<UnitDefId> powerPlants;

	for(auto powerPlant : ai->s_buildTree.GetUnitsInCategory(EUnitCategory::POWER_PLANT, side))
	{
		if( IsBuildingSelectable(powerPlant, water, false))
		{
			if(ai->s_buildTree.GetPrimaryAbility(powerPlant) < (maxPower+1.0f))
			{
				powerPlants.push_back(powerPlant);

				// cap energy at current energy production (to avoid jumping to very advanced power plants to fast)
				const float cappedEnergy = std::min(ai->s_buildTree.GetPrimaryAbility(powerPlant), energyGenerationLimit);

				generatedEnergies.AddValue(cappedEnergy);
				buildtimes.AddValue( ai->s_buildTree.GetBuildtime(powerPlant) );
				costs.AddValue(      ai->s_buildTree.GetTotalCost(powerPlant) );
			}
		}
	}

	generatedEnergies.Finalize();
	buildtimes.Finalize();
	costs.Finalize();

	//-----------------------------------------------------------------------------------------------------------------
	// select power plant
	//-----------------------------------------------------------------------------------------------------------------

	UnitDefId selectedPowerPlant;
	float     bestRating(0.0f);

	for(auto powerPlant : powerPlants)
	{
		if(!mustBeConstructable || (units_dynamic[powerPlant.id].constructorsAvailable > 0) )
		{
			const float cappedEnergy = std::min(ai->s_buildTree.GetPrimaryAbility(powerPlant), energyGenerationLimit);

			const float rating =  selectionCriteria.powerProduction * generatedEnergies.GetDeviationFromZero(cappedEnergy)
								+ selectionCriteria.cost            * costs.GetDeviationFromMax(ai->s_buildTree.GetTotalCost(powerPlant))
								+ selectionCriteria.buildtime       * buildtimes.GetDeviationFromMax(ai->s_buildTree.GetBuildtime(powerPlant));

			if(rating > bestRating)
			{
				bestRating = rating;
				selectedPowerPlant = powerPlant;
			}
		}
	}

	return selectedPowerPlant;
}

UnitDefId AAIBuildTable::SelectExtractor(int side, const ExtractorSelectionCriteria& selectionCriteria, bool water)
{
	UnitDefId extractor = SelectExtractor(side, selectionCriteria, water, false);

	if(extractor.IsValid() && (units_dynamic[extractor.id].constructorsAvailable <= 0) && (units_dynamic[extractor.id].constructorsRequested <= 0) )
	{
		RequestBuilderFor(extractor);
		extractor = SelectExtractor(side, selectionCriteria, water, true);
	}

	return extractor;
}

UnitDefId AAIBuildTable::SelectExtractor(int side, const ExtractorSelectionCriteria& selectionCriteria, bool water, bool mustBeConstructable) const
{
	UnitDefId selectedExtractorDefId;
	float     bestRating(0.0f);

	const AAIUnitStatistics& unitStatistics           = ai->s_buildTree.GetUnitStatistics(side);
	const StatisticalData&   extractedMetalStatistics = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::METAL_EXTRACTOR);
	const StatisticalData&   costStatistics           = unitStatistics.GetUnitCostStatistics(EUnitCategory::METAL_EXTRACTOR);

	for(auto extractorDefId : ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side))
	{
		// check if under water or ground || water = true and building under water
		if( IsBuildingSelectable(extractorDefId, water, mustBeConstructable) )
		{
			const float metalExtraction = ai->s_buildTree.GetMaxRange(extractorDefId);
			const float isArmed         = GetUnitDef(extractorDefId.id).weapons.empty() ? 0.0f : 1.0f;

			const float myRating =   selectionCriteria.extractedMetal * extractedMetalStatistics.GetDeviationFromZero(metalExtraction)
						           + selectionCriteria.cost           * costStatistics.GetDeviationFromMax(ai->s_buildTree.GetTotalCost(extractorDefId));
								   + selectionCriteria.armed          * isArmed;

			if(myRating > bestRating)
			{
				bestRating             = myRating;
				selectedExtractorDefId = extractorDefId;
			}
		}
	}

	return selectedExtractorDefId;
}

UnitDefId AAIBuildTable::SelectStorage(int side, const StorageSelectionCriteria& selectionCriteria, bool water)
{
	UnitDefId selectedStorage = SelectStorage(side, selectionCriteria, water, false);

	if(selectedStorage.IsValid() && (units_dynamic[selectedStorage.id].constructorsAvailable <= 0))
	{
		if(units_dynamic[selectedStorage.id].constructorsRequested <= 0)
			RequestBuilderFor(selectedStorage);

		selectedStorage = SelectStorage(side, selectionCriteria, water, true);
	}

	return selectedStorage;
}

UnitDefId AAIBuildTable::SelectStorage(int side, const StorageSelectionCriteria& selectionCriteria, bool water, bool mustBeConstructable) const
{
	const AAIUnitStatistics& unitStatistics  = ai->s_buildTree.GetUnitStatistics(side);
	const StatisticalData&   costs           = unitStatistics.GetUnitCostStatistics(EUnitCategory::STORAGE);
	const StatisticalData&   buildtimes      = unitStatistics.GetUnitBuildtimeStatistics(EUnitCategory::STORAGE);
	const StatisticalData&   metalStored     = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::STORAGE);
	const StatisticalData&   energyStored    = unitStatistics.GetUnitSecondaryAbilityStatistics(EUnitCategory::STORAGE);

	UnitDefId selectedStorage;
	float highestRating(0.0f);

	for(auto storage : ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STORAGE, side))
	{
		
		if( IsBuildingSelectable(storage.id, water, mustBeConstructable) )
		{
			const float rating =      selectionCriteria.cost         * costs.GetDeviationFromMax( ai->s_buildTree.GetTotalCost(storage) )
									+ selectionCriteria.buildtime    * buildtimes.GetDeviationFromMax( ai->s_buildTree.GetBuildtime(storage) )
									+ selectionCriteria.storedMetal  * metalStored.GetDeviationFromZero( ai->s_buildTree.GetMaxRange(storage) )
									+ selectionCriteria.storedEnergy * energyStored.GetDeviationFromZero( ai->s_buildTree.GetMaxSpeed(storage) );

			if(rating > highestRating)
			{
				highestRating   = rating;
				selectedStorage = storage;
			}
		}
	}

	return selectedStorage;
}

UnitDefId AAIBuildTable::GetMetalMaker(int side, float cost, float efficiency, float metal, float urgency, bool water, bool canBuild) const
{
	UnitDefId selectedMetalMaker;

	for(auto maker = ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_MAKER, side).begin(); maker != ai->s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_MAKER, side).end(); ++maker)
	{
		//! @todo reimplement selection of metal makers
	}

	return selectedMetalMaker;
}

void AAIBuildTable::DetermineCombatPowerWeights(MobileTargetTypeValues& combatPowerWeights, const AAIMapType& mapType) const
{
	combatPowerWeights.SetValueForTargetType(ETargetType::AIR,     0.1f + s_attackedByRates.GetAttackedByRateUntilEarlyPhase(mapType, ETargetType::AIR));
	combatPowerWeights.SetValueForTargetType(ETargetType::SURFACE, 1.0f + s_attackedByRates.GetAttackedByRateUntilEarlyPhase(mapType, ETargetType::SURFACE));
	
	if(!mapType.IsLand())
	{
		combatPowerWeights.SetValueForTargetType(ETargetType::FLOATER,   1.0f + s_attackedByRates.GetAttackedByRateUntilEarlyPhase(mapType, ETargetType::FLOATER));
		combatPowerWeights.SetValueForTargetType(ETargetType::SUBMERGED, 0.75f + s_attackedByRates.GetAttackedByRateUntilEarlyPhase(mapType, ETargetType::SUBMERGED));
	}
}

float AAIBuildTable::DetermineFactoryRating(UnitDefId factoryDefId) const
{
	float rating(0.0f);
	int numberOfUnits(0);

	for(auto unit : ai->s_buildTree.GetCanConstructList(factoryDefId))
	{
		++numberOfUnits;

		const AAIMovementType& moveType = ai->s_buildTree.GetMovementType(unit);
	
		if(moveType.IsMobileSea())
			rating += AAIMap::s_waterTilesRatio;
		else if(moveType.IsGround())
			rating += AAIMap::s_landTilesRatio;
		else
			rating += 1.0f;
	}

	return rating;
	//return (numberOfUnits > 0) ? rating / static_cast<float>(numberOfUnits) : 0.0f;
}

void AAIBuildTable::CalculateFactoryRating(FactoryRatingInputData& ratingData, const UnitDefId factoryDefId, const MobileTargetTypeValues& combatPowerWeights, const AAIMapType& mapType) const
{
	ratingData.canConstructBuilder = false;
	ratingData.canConstructScout   = false;
	ratingData.factoryDefId        = factoryDefId;

	MobileTargetTypeValues combatPowerOfConstructedUnits;
	int         combatUnits(0);

	const bool considerLand  = !mapType.IsWater();
	const bool considerWater = !mapType.IsLand();

	//-----------------------------------------------------------------------------------------------------------------
	// go through buildoptions to determine input values for calculation of factory rating
	//-----------------------------------------------------------------------------------------------------------------

	for(auto unit = ai->s_buildTree.GetCanConstructList(factoryDefId).begin(); unit != ai->s_buildTree.GetCanConstructList(factoryDefId).end(); ++unit)
	{
		const TargetTypeValues& combatPowerOfUnit = ai->s_buildTree.GetCombatPower(*unit);

		switch(ai->s_buildTree.GetUnitCategory(*unit).GetUnitCategory())
		{
			case EUnitCategory::GROUND_COMBAT:
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::SURFACE, combatPowerOfUnit.GetValue(ETargetType::SURFACE));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::AIR,     combatPowerOfUnit.GetValue(ETargetType::AIR));
				++combatUnits;
				break;
			case EUnitCategory::AIR_COMBAT:     // same calculation as for hover
			case EUnitCategory::HOVER_COMBAT:
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::SURFACE, combatPowerOfUnit.GetValue(ETargetType::SURFACE));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::AIR,     combatPowerOfUnit.GetValue(ETargetType::AIR));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::FLOATER, combatPowerOfUnit.GetValue(ETargetType::FLOATER));
				++combatUnits;
				break;
			case EUnitCategory::SEA_COMBAT:
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::SURFACE,   combatPowerOfUnit.GetValue(ETargetType::SURFACE));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::AIR,       combatPowerOfUnit.GetValue(ETargetType::AIR));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::FLOATER,   combatPowerOfUnit.GetValue(ETargetType::FLOATER));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::SUBMERGED, combatPowerOfUnit.GetValue(ETargetType::SUBMERGED));
				++combatUnits;
				break;
			case EUnitCategory::SUBMARINE_COMBAT:
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::FLOATER,   combatPowerOfUnit.GetValue(ETargetType::FLOATER));
				combatPowerOfConstructedUnits.AddValueForTargetType(ETargetType::SUBMERGED, combatPowerOfUnit.GetValue(ETargetType::SUBMERGED));
				++combatUnits;
				break;
			case EUnitCategory::MOBILE_CONSTRUCTOR:
				if( ai->s_buildTree.GetMovementType(*unit).IsMobileSea() )
				{
					if(considerWater)
						ratingData.canConstructBuilder = true;
				}
				else if(ai->s_buildTree.GetMovementType(*unit).IsGround() )
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
				if( ai->s_buildTree.GetMovementType(*unit).IsMobileSea() )
				{
					if(considerWater)
						ratingData.canConstructScout = true;
				}
				else if(ai->s_buildTree.GetMovementType(*unit).IsGround() )
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

UnitDefId AAIBuildTable::SelectStaticDefence(int side, const StaticDefenceSelectionCriteria& selectionCriteria, bool water)
{
	UnitDefId selectedDefence = SelectStaticDefence(side, selectionCriteria, water, false);

	if(selectedDefence.IsValid() && (units_dynamic[selectedDefence.id].constructorsAvailable <= 0))
	{
		if(units_dynamic[selectedDefence.id].constructorsRequested <= 0)
			RequestBuilderFor(selectedDefence);

		selectedDefence = SelectStaticDefence(side, selectionCriteria, false, true);
	}

	return selectedDefence;
}

UnitDefId AAIBuildTable::SelectStaticDefence(int side, const StaticDefenceSelectionCriteria& selectionCriteria, bool water, bool mustBeConstructable) const
{
	// get data needed for selection
	AAIUnitCategory category(EUnitCategory::STATIC_DEFENCE);
	const std::list<UnitDefId> unitList = ai->s_buildTree.GetUnitsInCategory(category, side);

	const StatisticalData& costs      = ai->s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(category);
	const StatisticalData& ranges     = ai->s_buildTree.GetUnitStatistics(side).GetUnitPrimaryAbilityStatistics(category);
	const StatisticalData& buildtimes = ai->s_buildTree.GetUnitStatistics(side).GetUnitBuildtimeStatistics(category);

	// calculate combat power
	StatisticalData combatPowerStat;		

	for(auto defence : unitList)
	{
		const float defenceCombatPower = ai->s_buildTree.GetCombatPower(defence).GetValue(selectionCriteria.targetType);
		combatPowerStat.AddValue(defenceCombatPower);
	}

	combatPowerStat.Finalize();

	// start with selection
	UnitDefId selectedDefence;
	float bestRating(0.0f);

	for(auto defence : unitList)
	{
		if( IsBuildingSelectable(defence, water, mustBeConstructable) )
		{
			const UnitTypeProperties& unitData = ai->s_buildTree.GetUnitTypeProperties(defence);

			const float myCombatPower = ai->s_buildTree.GetCombatPower(defence).GetValue(selectionCriteria.targetType);

			float myRating =  selectionCriteria.cost        * costs.GetDeviationFromMax( unitData.m_totalCost )
							+ selectionCriteria.buildtime   * buildtimes.GetDeviationFromMax( unitData.m_buildtime )
							+ selectionCriteria.range       * ranges.GetDeviationFromZero( unitData.m_primaryAbility )
							+ selectionCriteria.combatPower * combatPowerStat.GetDeviationFromZero( myCombatPower )
							+ 0.05f * ((float)(rand()%(selectionCriteria.randomness+1)));

			if(myRating > bestRating)
			{
				bestRating = myRating;
				selectedDefence = defence;
			}
		}
	}

	return selectedDefence;
}

UnitDefId AAIBuildTable::SelectNanoTurret(int side, bool water) const
{
	UnitDefId selectedTurret;

	for(auto nanoTurret : ai->s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_ASSISTANCE, side) )
	{
		if(IsBuildingSelectable(nanoTurret, water, true) )
		{
			selectedTurret = nanoTurret;
		}
	}

	return selectedTurret;
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

	if(radar.IsValid() && (units_dynamic[radar.id].constructorsAvailable <= 0) )
	{
		if(units_dynamic[radar.id].constructorsRequested <= 0)
			RequestBuilderFor(radar);

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
			best_jammer = *i;
			best_rating = my_rating;
		}*/
	}

	return best_jammer;
}

UnitDefId AAIBuildTable::SelectScout(int side, float sightRange, float cost, float cloakable, uint32_t movementType, int randomness, bool factoryAvailable)
{
	float highestRating(0.0f);
	UnitDefId selectedScout;

	const StatisticalData& costs       = ai->s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(EUnitCategory::SCOUT);
	const StatisticalData& sightRanges = ai->s_buildTree.GetUnitStatistics(side).GetUnitPrimaryAbilityStatistics(EUnitCategory::SCOUT);

	for(auto scoutUnitDefId : ai->s_buildTree.GetUnitsInCategory(EUnitCategory::SCOUT, side))
	{
		const AAIMovementType& moveType    = ai->s_buildTree.GetMovementType(scoutUnitDefId);
		const bool factoryPrerequisitesMet = !factoryAvailable || (units_dynamic[scoutUnitDefId.id].constructorsAvailable > 0);

		if( moveType.IsIncludedIn(movementType) && factoryPrerequisitesMet )
		{
			float rating =     sightRange * sightRanges.GetDeviationFromZero(ai->s_buildTree.GetMaxRange(scoutUnitDefId))
							+  cost       * costs.GetDeviationFromMax( ai->s_buildTree.GetTotalCost(scoutUnitDefId) )
							+ (0.1f * ((float)(rand()%randomness)));

			if(GetUnitDef(scoutUnitDefId.id).canCloak)
				rating += cloakable;

			if(moveType.IsMobileSea())
				rating *= (0.2f + 0.8f * AAIMap::s_waterTilesRatio);
			else if(moveType.IsGround())
				rating *= (0.2f + 0.8f * AAIMap::s_landTilesRatio);

			if(rating > highestRating)
			{
				highestRating = rating;
				selectedScout = scoutUnitDefId;
			}
		}
	}
	
	return selectedScout;
}

void AAIBuildTable::SelectCombatUnits(std::list<UnitDefId>& unitList, int side, const AAIMovementType& allowedMoveTypes, bool constructorAvailable) const
{
	const auto& combatUnitCategories = ai->s_buildTree.GetCombatUnitCatgegories();

	std::vector<bool> checkCategory(combatUnitCategories.size(), false);

	if(allowedMoveTypes.IsAir())
		checkCategory[1] = true;

	if(allowedMoveTypes.Includes(EMovementType::MOVEMENT_TYPE_GROUND) || allowedMoveTypes.Includes(EMovementType::MOVEMENT_TYPE_AMPHIBIOUS) )
	{
		checkCategory[0] = true;
		checkCategory[2] = true;
	}

	if(allowedMoveTypes.Includes(EMovementType::MOVEMENT_TYPE_HOVER))
		checkCategory[2] = true;

	if(allowedMoveTypes.Includes(EMovementType::MOVEMENT_TYPE_SEA_FLOATER))
	{
		checkCategory[2] = true;
		checkCategory[3] = true;
	}

	if(allowedMoveTypes.Includes(EMovementType::MOVEMENT_TYPE_SEA_SUBMERGED))
		checkCategory[4] = true;

	int i(0);
	for(auto unitCategory : combatUnitCategories)
	{
		if(checkCategory[i])
		{
			for(auto unitDefId : ai->s_buildTree.GetUnitsInCategory(unitCategory, side) )
			{
				const bool constructorAvailabilityCheckPassed = (constructorAvailable == false) || (units_dynamic[unitDefId.id].constructorsAvailable > 0);
				
				if(constructorAvailabilityCheckPassed && ai->s_buildTree.GetMovementType(unitDefId).IsIncludedIn(allowedMoveTypes))
					unitList.push_back(unitDefId);
			}
		}

		++i;
	}
}

UnitDefId AAIBuildTable::SelectCombatUnit(int side, const AAIMovementType& allowedMoveTypes, const TargetTypeValues& combatPowerCriteria, const UnitSelectionCriteria& unitCriteria, const std::vector<float>& factoryUtilization, int randomness, bool constructorAvailable) const
{
	//-----------------------------------------------------------------------------------------------------------------
	// get data needed for selection
	//-----------------------------------------------------------------------------------------------------------------

	std::list<UnitDefId> unitList;
	SelectCombatUnits(unitList, side, allowedMoveTypes, constructorAvailable);

	StatisticalData costStatistics;
	StatisticalData rangeStatistics;
	StatisticalData speedStatistics;
	StatisticalData combatPowerStat;
	StatisticalData combatEfficiencyStat;
	std::vector<float> combatPowerValues(unitList.size()); // values for individual units (in order of appearance in unitList)

	int i = 0;
	for(auto unitDefId : unitList)
	{
		const UnitTypeProperties& unitData = ai->s_buildTree.GetUnitTypeProperties(unitDefId);

		const float combatPower = combatPowerCriteria.CalculateWeightedSum(ai->s_buildTree.GetCombatPower(unitDefId)); 
		const float combatEff   = combatPower / unitData.m_totalCost;

		costStatistics.AddValue(unitData.m_totalCost);
		rangeStatistics.AddValue(unitData.m_primaryAbility);
		speedStatistics.AddValue(unitData.m_secondaryAbility);
		combatPowerStat.AddValue(combatPower);
		combatEfficiencyStat.AddValue(combatEff);
		combatPowerValues[i] = combatPower;

		++i;
	}

	costStatistics.Finalize();
	rangeStatistics.Finalize();
	speedStatistics.Finalize();
	combatPowerStat.Finalize();
	combatEfficiencyStat.Finalize();

	//-----------------------------------------------------------------------------------------------------------------
	// begin with selection
	//-----------------------------------------------------------------------------------------------------------------

	//ai->Log("Selected combat unit (move type %u): cost %f  range %f   speed %f   power %f   efficiency %f   fact. ut. %f   combat: %f %f %f %f %f\n", static_cast<uint32_t>(allowedMoveTypes.GetMovementType()),
	//unitCriteria.cost, unitCriteria.range, unitCriteria.speed, unitCriteria.power, unitCriteria.efficiency, unitCriteria.factoryUtilization,
	//combatPowerCriteria.m_values[0], combatPowerCriteria.m_values[1], combatPowerCriteria.m_values[2], combatPowerCriteria.m_values[3], combatPowerCriteria.m_values[4]);

	UnitDefId selectedUnitType;
	float highestRating(0.0f);
	i = 0;
	for(const auto& unitDefId : unitList)
	{
		const UnitTypeProperties& unitData = ai->s_buildTree.GetUnitTypeProperties(unitDefId);

		float minFactoryUtilization(0.0f);
		for(const auto& factory : ai->s_buildTree.GetConstructedByList(unitDefId))
		{
			const float utilization = factoryUtilization[ai->s_buildTree.GetUnitTypeProperties(factory).m_factoryId.id];

			if(utilization > minFactoryUtilization)
				minFactoryUtilization = utilization;
		}
			
		const float combatEff = combatPowerValues[i] / unitData.m_totalCost;

		const float rating =  unitCriteria.cost  * costStatistics.GetDeviationFromMax( unitData.m_totalCost )
							+ unitCriteria.range * rangeStatistics.GetDeviationFromZero( unitData.m_primaryAbility )
							+ unitCriteria.speed * speedStatistics.GetDeviationFromZero( unitData.m_secondaryAbility )
							+ unitCriteria.power * combatPowerStat.GetDeviationFromZero( combatPowerValues[i] )
							+ unitCriteria.efficiency * combatEfficiencyStat.GetDeviationFromZero( combatEff )
							+ unitCriteria.factoryUtilization * minFactoryUtilization
							+ 0.1f * ((float)(rand()%randomness));

		/*ai->Log("%s: %f  -  %f %f %f %f %f %f\n", unitData.m_name.c_str(), rating, 
												unitCriteria.cost * costStatistics.GetDeviationFromMax(unitData.m_totalCost), 
												unitCriteria.range * rangeStatistics.GetDeviationFromZero(unitData.m_primaryAbility),
												unitCriteria.speed * speedStatistics.GetDeviationFromZero(unitData.m_secondaryAbility),
												unitCriteria.power * combatPowerStat.GetDeviationFromZero(combatPowerValues[i]),
												unitCriteria.efficiency * combatEfficiencyStat.GetDeviationFromZero(combatEff),
												minFactoryUtilization);*/

		if(rating > highestRating)
		{
			highestRating       = rating;
			selectedUnitType.id = unitDefId.id;
		}
		
		++i;
	}

	//ai->Log("Selected: %s\n", ai->s_buildTree.GetUnitTypeProperties(selectedUnitType).m_name.c_str() );

	return selectedUnitType;
}

std::string AAIBuildTable::GetBuildCacheFileName() const
{
	return cfg->GetFileName(ai->GetAICallback(), cfg->GetUniqueName(ai->GetAICallback(), true, true, false, false), MOD_LEARN_PATH, "_buildcache.txt", true);
}

bool AAIBuildTable::LoadModLearnData()
{
	// load data
	const std::string filename = GetBuildCacheFileName();
	// load units if file exists
	FILE *inputFile = fopen(filename.c_str(), "r");

	if(inputFile)
	{
		char buffer[1024];
		// check if correct version
		fscanf(inputFile, "%s", buffer);

		if(strcmp(buffer, MOD_LEARN_VERSION))
		{
			ai->LogConsole("Buildtable version out of date - creating new one");
			return false;
		}

		// load attacked_by table
		for(AAIMapType mapType(AAIMapType::first); mapType.End() == false; mapType.Next())
		{
			for(GamePhase gamePhase(0); gamePhase.End() == false; gamePhase.Next())
			{
				for(const auto& targetType : AAITargetType::m_mobileTargetTypes)
				{
					float atackedByRate;
					fscanf(inputFile, "%f ", &atackedByRate);
					s_attackedByRates.SetAttackedByRate(mapType, gamePhase, targetType, atackedByRate);
				}
			}
		}

		const bool combatPowerLoaded = ai->s_buildTree.LoadCombatPowerOfUnits(inputFile);
		fclose(inputFile);
		return combatPowerLoaded;
	}
	
	return false;
}

void AAIBuildTable::SaveModLearnData(const GamePhase& gamePhase, const AttackedByRatesPerGamePhase& attackedByRates, const AAIMapType& mapType) const
{
	const std::string filename = GetBuildCacheFileName();
	FILE *saveFile = fopen(filename.c_str(), "w+");

	// file version
	fprintf(saveFile, "%s \n", MOD_LEARN_VERSION);

	// update attacked_by values
	AttackedByRatesPerGamePhase& updateRates = s_attackedByRates.GetAttackedByRates(mapType);
	updateRates = attackedByRates;
	updateRates.DecreaseByFactor(gamePhase, 0.7f);

	// save attacked_by table
	for(AAIMapType mapTypeIterator(AAIMapType::first); mapTypeIterator.End() == false; mapTypeIterator.Next())
	{
		for(GamePhase gamePhaseIterator(0); gamePhaseIterator.End() == false; gamePhaseIterator.Next())
		{
			for(const auto& targetType : AAITargetType::m_mobileTargetTypes)
			{
				fprintf(saveFile, "%f ", s_attackedByRates.GetAttackedByRate(mapTypeIterator, gamePhaseIterator, targetType));
			}
			fprintf(saveFile, "\n");
		}
	}

	ai->s_buildTree.SaveCombatPowerOfUnits(saveFile);

	fclose(saveFile);
}

UnitDefId AAIBuildTable::SelectConstructorFor(UnitDefId unitDefId) const
{
	float cost = 1.0f;
	float buildtime = 0.5f;
	float buildpower = 1.0f;
	float constructorAvailableBonus = 2.0f;

	if(units_dynamic[unitDefId.id].constructorsAvailable == 0)
	{
		buildtime = 2.0f;
		cost = 1.5f;
	}
	else if(units_dynamic[unitDefId.id].constructorsAvailable < 2)
	{
		buildtime  = 1.0f;
	}

	//-----------------------------------------------------------------------------------------------------------------
	// determine statistical data needed for selection
	//-----------------------------------------------------------------------------------------------------------------
	StatisticalData costStatistics;
	StatisticalData buildtimeStatistics;
	StatisticalData buildpowerStatistics;

	for(auto constructor : ai->s_buildTree.GetConstructedByList(unitDefId))
	{
		costStatistics.AddValue( ai->s_buildTree.GetTotalCost(constructor) );
		buildtimeStatistics.AddValue( ai->s_buildTree.GetBuildtime(constructor) );
		buildpowerStatistics.AddValue( ai->s_buildTree.GetBuildspeed(constructor) );
	}

	costStatistics.Finalize();
	buildtimeStatistics.Finalize();
	buildpowerStatistics.Finalize();

	//-----------------------------------------------------------------------------------------------------------------
	// select constructor according to determined criteria
	//-----------------------------------------------------------------------------------------------------------------

	UnitDefId selectedConstructor;
	float highestRating(0.0f);

	for(const auto constructor : ai->s_buildTree.GetConstructedByList(unitDefId))
	{
		const int maxNumberOfConstructors = ai->s_buildTree.GetMovementType(constructor).IsStatic() ? cfg->MAX_FACTORIES_PER_TYPE : cfg->MAX_BUILDERS_PER_TYPE;

		if(GetTotalNumberOfUnits(constructor) < maxNumberOfConstructors)
		{
			float rating =   cost       * costStatistics.GetDeviationFromMax( ai->s_buildTree.GetTotalCost(constructor) )
			               + buildtime  * buildtimeStatistics.GetDeviationFromMax( ai->s_buildTree.GetBuildtime(constructor) )
				           + buildpower * buildpowerStatistics.GetDeviationFromZero( ai->s_buildTree.GetBuildspeed(constructor) )
						   + 0.1f;

			if(units_dynamic[constructor.id].constructorsAvailable > 0)
				rating += constructorAvailableBonus;

			// take movement type into consideration (dont build ground based construction units on water maps and water bound construction units on land maps)
			const AAIMovementType& moveType = ai->s_buildTree.GetMovementType(constructor);

			if(moveType.IsSea())
				rating *= (0.2f + 0.8f * AAIMap::s_waterTilesRatio);
			else if(moveType.IsGround() || moveType.IsStaticLand())
				rating *= (0.2f + 0.8f * AAIMap::s_landTilesRatio);

			if(rating > highestRating)
			{
				highestRating       = rating;
				selectedConstructor = constructor;
			}
		}
	}

	return selectedConstructor;
}

bool AAIBuildTable::RequestConstructionOfConstructor(UnitDefId constructor)
{
	if(GetTotalNumberOfConstructorsForUnit(constructor) <= 0)
	{
		//ai->Log("BuildFactoryFor(%s) is requesting factory for %s\n", ai->s_buildTree.GetUnitTypeProperties(unitDefId).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(constructor).m_name.c_str());
		RequestFactoryFor(constructor);
	}
	else
	{	
		// only mark as urgent (unit gets added to front of buildqueue) if no constructor of that type already exists
		const BuildQueuePosition queuePosition = (units_dynamic[constructor.id].active > 1) ? BuildQueuePosition::SECOND : BuildQueuePosition::FRONT;

		if(ai->Getexecute()->AddUnitToBuildqueue(constructor, 1, queuePosition, true))
		{
			ConstructorRequested(constructor);
			return true;
		}
	}
	
	return false;
}

void AAIBuildTable::RequestFactoryFor(UnitDefId unitDefId)
{
	const UnitDefId selectedConstructor = SelectConstructorFor(unitDefId);
	
	//-----------------------------------------------------------------------------------------------------------------
	// order construction if valid factory/constructor selected and check if constuctor for selected factory is available
	//-----------------------------------------------------------------------------------------------------------------
	if(selectedConstructor.IsValid() && (GetNumberOfFutureUnits(selectedConstructor) <= 0) )
	{
		ConstructorRequested(selectedConstructor);

		// factory requested
		if( ai->s_buildTree.GetUnitCategory(selectedConstructor).IsStaticConstructor() )
		{			
			m_factoryBuildqueue.push_back(selectedConstructor);
			units_dynamic[selectedConstructor.id].requested += 1;

			if(GetTotalNumberOfConstructorsForUnit(selectedConstructor) <= 0)
			{
				//ai->Log("RequestFactoryFor(%s) is requesting builder for %s\n", ai->s_buildTree.GetUnitTypeProperties(unitDefId).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(selectedConstructor).m_name.c_str());
				RequestBuilderFor(selectedConstructor);
			}

			// debug
			ai->Log("RequestFactoryFor(%s) requested %s\n", ai->s_buildTree.GetUnitTypeProperties(unitDefId).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(selectedConstructor).m_name.c_str());
		}
		// mobile constructor requested
		else
		{
			const bool successful = RequestConstructionOfConstructor(selectedConstructor);

			if(successful)
				ai->Log("RequestFactoryFor(%s) requested %s\n", ai->s_buildTree.GetUnitTypeProperties(unitDefId).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(selectedConstructor).m_name.c_str());
		}
	}
}

void AAIBuildTable::RequestBuilderFor(UnitDefId building)
{
	const UnitDefId selectedBuilder = SelectConstructorFor(building);

	if( selectedBuilder.IsValid() && (GetNumberOfFutureUnits(selectedBuilder) <= 0) )
	{
		const bool successful = RequestConstructionOfConstructor(selectedBuilder);

		if(successful)
			ai->Log("RequestBuilderFor(%s) requested %s\n", ai->s_buildTree.GetUnitTypeProperties(building).m_name.c_str(), ai->s_buildTree.GetUnitTypeProperties(selectedBuilder).m_name.c_str());
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
			ai->Getut()->UnitRequested(AAIUnitCategory(EUnitCategory::MOBILE_CONSTRUCTOR));

			// increase number of requested builders of all buildoptions
			ConstructorRequested(UnitDefId(builder));

			//ai->Log("AddAssister() requested: %s %i \n", GetUnitDef(builder).humanName.c_str(), units_dynamic[builder].requested);
		}
	}
}*/
