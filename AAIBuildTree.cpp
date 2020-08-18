// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "System/SafeUtil.h"
#include "aidef.h"
#include "AAIBuildTree.h"
#include "AAIConfig.h"

#include "LegacyCpp/IGlobalAICallback.h"

#include "LegacyCpp/UnitDef.h"
#include "LegacyCpp/MoveData.h"
#include "LegacyCpp/WeaponDef.h"

#include <string>
#include <unordered_map>
#include <map>

using namespace springLegacyAI;

AAIBuildTree::AAIBuildTree() :
	m_initialized(false),
	m_numberOfSides(0)
{
	m_unitCategoryNames.resize(AAIUnitCategory::numberOfUnitCategories);
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::UNKNOWN).GetArrayIndex()].append("Unknown");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::STATIC_DEFENCE).GetArrayIndex()].append("Static Defence");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::STATIC_ARTILLERY).GetArrayIndex()].append("Static Artillery");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::STORAGE).GetArrayIndex()].append("Storage");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::STATIC_CONSTRUCTOR).GetArrayIndex()].append("Static Constructor");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::STATIC_SUPPORT).GetArrayIndex()].append("Static Support");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::STATIC_SENSOR).GetArrayIndex()].append("Static Sensor");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::POWER_PLANT).GetArrayIndex()].append("Power Plant");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::METAL_EXTRACTOR).GetArrayIndex()].append("Metal Extractor");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::METAL_MAKER).GetArrayIndex()].append("Metal Maker");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::COMMANDER).GetArrayIndex()].append("Commander");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::GROUND_COMBAT).GetArrayIndex()].append("Ground Combat");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::AIR_COMBAT).GetArrayIndex()].append("Air Combat");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::HOVER_COMBAT).GetArrayIndex()].append("Hover Combat");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::SEA_COMBAT).GetArrayIndex()].append("Sea Combat");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::SUBMARINE_COMBAT).GetArrayIndex()].append("Submarine Combat");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::MOBILE_ARTILLERY).GetArrayIndex()].append("Mobile Artillery");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::SCOUT).GetArrayIndex()].append("Scout");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::TRANSPORT).GetArrayIndex()].append("Transport");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::MOBILE_CONSTRUCTOR).GetArrayIndex()].append("Mobile Constructor");
	m_unitCategoryNames[AAIUnitCategory(EUnitCategory::MOBILE_SUPPORT).GetArrayIndex()].append("Mobile Support");

	m_combatUnitCategories.push_back(EUnitCategory::GROUND_COMBAT);
	m_combatUnitCategories.push_back(EUnitCategory::AIR_COMBAT);
	m_combatUnitCategories.push_back(EUnitCategory::HOVER_COMBAT);
	m_combatUnitCategories.push_back(EUnitCategory::SEA_COMBAT);
	m_combatUnitCategories.push_back(EUnitCategory::SUBMARINE_COMBAT);
}

AAIBuildTree::~AAIBuildTree(void)
{
	m_initialized = false;
	m_unitTypeCanBeConstructedtByLists.clear();
	m_unitTypeCanConstructLists.clear();
	m_unitTypeProperties.clear();
	m_sideOfUnitType.clear();
	m_startUnitsOfSide.clear();
	m_unitCategoryNames.clear();
}

void AAIBuildTree::SaveCombatPowerOfUnits(FILE* saveFile) const
{
	fprintf(saveFile, "%i\n", static_cast<int>(m_combatPowerOfUnits.size()));

	for(int id = 1; id < m_combatPowerOfUnits.size(); ++id)
	{
		fprintf(saveFile, "%f %f %f %f %f\n", m_combatPowerOfUnits[id].GetCombatPowerVsTargetType(ETargetType::SURFACE),
											  m_combatPowerOfUnits[id].GetCombatPowerVsTargetType(ETargetType::AIR),
											  m_combatPowerOfUnits[id].GetCombatPowerVsTargetType(ETargetType::FLOATER),
											  m_combatPowerOfUnits[id].GetCombatPowerVsTargetType(ETargetType::SUBMERGED),
											  m_combatPowerOfUnits[id].GetCombatPowerVsTargetType(ETargetType::STATIC));
	}
}

bool AAIBuildTree::LoadCombatPowerOfUnits(FILE* inputFile)
{
	// abort loading if number of stored combat power data does not match number of units
	int numOfData;
	fscanf(inputFile, "%i", &numOfData);

	if(numOfData != static_cast<int>(m_combatPowerOfUnits.size()) )
		return false;

	float inputValues[5];

	for(int id = 1; id < m_combatPowerOfUnits.size(); ++id)
	{
		fscanf(inputFile, "%f %f %f %f %f", &inputValues[0], &inputValues[1], &inputValues[2], &inputValues[3], &inputValues[4]);
	
		m_combatPowerOfUnits[id].SetCombatPower(ETargetType::SURFACE,   inputValues[0]);
		m_combatPowerOfUnits[id].SetCombatPower(ETargetType::AIR,       inputValues[1]);
		m_combatPowerOfUnits[id].SetCombatPower(ETargetType::FLOATER,   inputValues[2]);
		m_combatPowerOfUnits[id].SetCombatPower(ETargetType::SUBMERGED, inputValues[3]);
		m_combatPowerOfUnits[id].SetCombatPower(ETargetType::STATIC,    inputValues[4]);
	}

	UpdateUnitTypesOfCombatUnits();

	return true;
}

void AAIBuildTree::InitCombatPowerOfUnits()
{
	for(int id = 1; id < m_combatPowerOfUnits.size(); ++id)
	{
		const int side = GetSideOfUnitType(UnitDefId(id));
		
		if(side > 0)
		{
			const AAIUnitCategory& category = GetUnitCategory(UnitDefId(id));

			const float cost = GetTotalCost(UnitDefId(id));
			const float eff = 1.0f + 9.0f * GetUnitStatistics(side).GetUnitCostStatistics(category).GetNormalizedDeviationFromMin(cost);

			AAICombatPower combatPower;

			if(category.isGroundCombat())
			{
				combatPower.SetCombatPower(ETargetType::SURFACE,   eff);
				combatPower.SetCombatPower(ETargetType::AIR,       0.2f);
				combatPower.SetCombatPower(ETargetType::FLOATER,   0.2f);
				combatPower.SetCombatPower(ETargetType::SUBMERGED, 0.2f);
				combatPower.SetCombatPower(ETargetType::STATIC,    eff);
			}
			else if(category.isAirCombat())
			{
				combatPower.SetCombatPower(ETargetType::SURFACE,   0.5f * eff);
				combatPower.SetCombatPower(ETargetType::AIR,       eff);
				combatPower.SetCombatPower(ETargetType::FLOATER,   0.5f * eff);
				combatPower.SetCombatPower(ETargetType::SUBMERGED, 0.2f);
				combatPower.SetCombatPower(ETargetType::STATIC,    0.5f * eff);
			}
			else if(category.isHoverCombat())
			{
				combatPower.SetCombatPower(ETargetType::SURFACE,   eff);
				combatPower.SetCombatPower(ETargetType::AIR,       0.2f);
				combatPower.SetCombatPower(ETargetType::FLOATER,   eff);
				combatPower.SetCombatPower(ETargetType::SUBMERGED, 0.2f);
				combatPower.SetCombatPower(ETargetType::STATIC,    eff);
			}
			else if(category.isSeaCombat())
			{
				combatPower.SetCombatPower(ETargetType::SURFACE,   eff);
				combatPower.SetCombatPower(ETargetType::AIR,       0.2f);
				combatPower.SetCombatPower(ETargetType::FLOATER,   eff);
				combatPower.SetCombatPower(ETargetType::SUBMERGED, eff);
				combatPower.SetCombatPower(ETargetType::STATIC,    eff);
			}
			else if(category.isSubmarineCombat())
			{
				combatPower.SetCombatPower(ETargetType::SURFACE,   0.2f);
				combatPower.SetCombatPower(ETargetType::AIR,       0.2f);
				combatPower.SetCombatPower(ETargetType::FLOATER,   eff);
				combatPower.SetCombatPower(ETargetType::SUBMERGED, eff);
				combatPower.SetCombatPower(ETargetType::STATIC,    eff);
			}
			else if(category.isStaticDefence())
			{
				if(GetMovementType(UnitDefId(id)).IsStaticLand())
				{
					combatPower.SetCombatPower(ETargetType::SURFACE,   eff);
					combatPower.SetCombatPower(ETargetType::FLOATER,   0.5f * eff);
					combatPower.SetCombatPower(ETargetType::SUBMERGED, 0.2f);
				}
				else
				{
					combatPower.SetCombatPower(ETargetType::SURFACE,   eff);
					combatPower.SetCombatPower(ETargetType::FLOATER,   eff);
					combatPower.SetCombatPower(ETargetType::SUBMERGED, eff);
				}

				combatPower.SetCombatPower(ETargetType::AIR,    0.2f);
				combatPower.SetCombatPower(ETargetType::STATIC, 0.2f);
			}

			m_combatPowerOfUnits[id].SetCombatPower(combatPower);
		}
	}

	UpdateUnitTypesOfCombatUnits();
}

void AAIBuildTree::UpdateUnitTypesOfCombatUnits()
{
	for(int id = 1; id < m_unitTypeProperties.size(); ++id)
	{
		if(m_unitTypeProperties[id].m_unitCategory.isCombatUnit() || m_unitTypeProperties[id].m_unitCategory.isStaticDefence() )
		{
			if(m_combatPowerOfUnits[id].GetCombatPowerVsTargetType(ETargetType::SURFACE) > AAIConstants::minAntiTargetTypeCombatPower)
				m_unitTypeProperties[id].m_unitType.AddUnitType(EUnitType::ANTI_SURFACE);

			if(m_combatPowerOfUnits[id].GetCombatPowerVsTargetType(ETargetType::AIR) > AAIConstants::minAntiTargetTypeCombatPower)
				m_unitTypeProperties[id].m_unitType.AddUnitType(EUnitType::ANTI_AIR);

			if(m_combatPowerOfUnits[id].GetCombatPowerVsTargetType(ETargetType::FLOATER) > AAIConstants::minAntiTargetTypeCombatPower)
				m_unitTypeProperties[id].m_unitType.AddUnitType(EUnitType::ANTI_SHIP);

			if(m_combatPowerOfUnits[id].GetCombatPowerVsTargetType(ETargetType::SUBMERGED) > AAIConstants::minAntiTargetTypeCombatPower)
				m_unitTypeProperties[id].m_unitType.AddUnitType(EUnitType::ANTI_SUBMERGED);
		}
	} 
}

float AAIBuildTree::CalculateCombatPowerChange(UnitDefId attackerUnitDefId, UnitDefId killedUnitDefId) const
{
	const AAIUnitCategory& killedCategory = GetUnitCategory(killedUnitDefId);

	const AAITargetType& attackerTargetType = GetTargetType(attackerUnitDefId);
	const AAITargetType& killedTargetType   = GetTargetType(killedUnitDefId);

	const float change = cfg->LEARN_SPEED * m_combatPowerOfUnits[killedUnitDefId.id].GetCombatPowerVsTargetType(attackerTargetType) /  m_combatPowerOfUnits[attackerUnitDefId.id].GetCombatPowerVsTargetType(killedTargetType);

	if(change < AAIConstants::maxCombatPowerChangeAfterSingleCombat)
		return change;
	else
		return AAIConstants::maxCombatPowerChangeAfterSingleCombat;
}

void AAIBuildTree::UpdateCombatPowerStatistics(UnitDefId attackerUnitDefId, UnitDefId killedUnitDefId)
{
	const AAIUnitCategory& attackerCategory = GetUnitCategory(attackerUnitDefId);
	const AAIUnitCategory& killedCategory   = GetUnitCategory(killedUnitDefId);

	if(    (attackerCategory.isCombatUnit() || attackerCategory.isStaticDefence())
		&& (killedCategory.isCombatUnit()   || killedCategory.isStaticDefence()) )
	{
		const float combatPowerChange = CalculateCombatPowerChange(attackerUnitDefId, killedUnitDefId);

		m_combatPowerOfUnits[attackerUnitDefId.id].IncreaseCombatPower(GetTargetType(killedUnitDefId), combatPowerChange);
		m_combatPowerOfUnits[killedUnitDefId.id].DecreaseCombatPower(GetTargetType(attackerUnitDefId), combatPowerChange);
	}
}

bool AAIBuildTree::Generate(springLegacyAI::IAICallback* cb)
{
	// prevent buildtree from beeing initialized several times
	if(m_initialized == true)
		return false;

	m_initialized = true;

	//-----------------------------------------------------------------------------------------------------------------
	// get number of unit types and set up arrays
	//-----------------------------------------------------------------------------------------------------------------
	const int numberOfUnitTypes = cb->GetNumUnitDefs();

	// unit ids start with 1 -> add one additional element to arrays to be able to directly access unit def with corresponding id
	m_unitTypeCanBeConstructedtByLists.resize(numberOfUnitTypes+1);
	m_unitTypeCanConstructLists.resize(numberOfUnitTypes+1);
	m_unitTypeProperties.resize(numberOfUnitTypes+1);
	m_sideOfUnitType.resize(numberOfUnitTypes+1, 0);
	m_combatPowerOfUnits.resize(numberOfUnitTypes+1);

	//-----------------------------------------------------------------------------------------------------------------
	// get list all of unit definitions for further analysis
	//-----------------------------------------------------------------------------------------------------------------

	//spring first unitdef id is 1, we remap it so id = is position in array
	std::vector<const springLegacyAI::UnitDef*> unitDefs(numberOfUnitTypes+1);

	cb->GetUnitDefList(&unitDefs[1]);

	//-----------------------------------------------------------------------------------------------------------------
	// determine build tree
	//-----------------------------------------------------------------------------------------------------------------
	for(int id = 1; id <= numberOfUnitTypes; ++id)
	{
		// determine which unit types can be constructed by the current unit type
		for(std::map<int, std::string>::const_iterator j = unitDefs[id]->buildOptions.begin(); j != unitDefs[id]->buildOptions.end(); ++j)
		{
			int canConstructId = cb->GetUnitDef(j->second.c_str())->id;

			m_unitTypeCanConstructLists[id].push_back( UnitDefId(canConstructId) );
			m_unitTypeCanBeConstructedtByLists[canConstructId].push_back( UnitDefId(id) );
		}
	}

	//-----------------------------------------------------------------------------------------------------------------
	// determine "roots" of buildtrees
	//-----------------------------------------------------------------------------------------------------------------
	std::list<int> rootUnits;

	for(int id = 1; id <= numberOfUnitTypes; ++id)
	{
		if(    (m_unitTypeCanConstructLists[id].size() > 0) 
			&& (m_unitTypeCanBeConstructedtByLists[id].size() == 0) )
		{
			rootUnits.push_back(id);
		}
	}

	//-----------------------------------------------------------------------------------------------------------------
	// assign sides to units
	//-----------------------------------------------------------------------------------------------------------------
	m_numberOfSides = 0;
	m_startUnitsOfSide.resize( rootUnits.size()+1, 0);  // +1 because of neutral (side = 0) units

	for(std::list<int>::iterator id = rootUnits.begin(); id != rootUnits.end(); ++id)
	{
		++m_numberOfSides;
		AssignSideToUnitType(m_numberOfSides, UnitDefId(*id) );
		m_startUnitsOfSide[m_numberOfSides] = *id;
	}

	m_unitsInCategory.resize(m_numberOfSides); // no need to create statistics for neutral units
	m_unitsInCombatCategory.resize(m_numberOfSides);

	for(int side = 0; side < m_numberOfSides; ++side)
	{
		m_unitsInCategory[side].resize( AAIUnitCategory::numberOfUnitCategories ); 
		m_unitsInCombatCategory[side].resize( AAICombatCategory::numberOfCombatCategories );
	}

	//-----------------------------------------------------------------------------------------------------------------
	// set further unit type properties
	//-----------------------------------------------------------------------------------------------------------------

	for(int id = 1; id <= numberOfUnitTypes; ++id)
	{
		m_unitTypeProperties[id].m_totalCost = unitDefs[id]->metalCost + (unitDefs[id]->energyCost / AAIConstants::energyToMetalConversionFactor);
		m_unitTypeProperties[id].m_buildtime = unitDefs[id]->buildTime;
		m_unitTypeProperties[id].m_name      = unitDefs[id]->humanName;
		
		m_unitTypeProperties[id].m_movementType.SetMovementType( DetermineMovementType(unitDefs[id]) );
		m_unitTypeProperties[id].m_targetType.SetType( DetermineTargetType(m_unitTypeProperties[id].m_movementType) );
	}

	// second loop because movement type information for all units is needed to determine unit type
	for(int id = 1; id <= numberOfUnitTypes; ++id)
	{
		// set unit category and add to corresponding unit list (if unit is not neutral)
		AAIUnitCategory unitCategory( DetermineUnitCategory(unitDefs[id]) );
		m_unitTypeProperties[id].m_unitCategory = unitCategory;

		if(m_sideOfUnitType[id] > 0)
		{
			m_unitsInCategory[ m_sideOfUnitType[id]-1 ][ unitCategory.GetArrayIndex() ].push_back(UnitDefId(id));

			UpdateUnitTypes(id ,unitDefs[id]);
		}

		// add combat units to combat category lists
		if(unitCategory.isGroundCombat() == true)
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][ AAICombatCategory::GetArrayIndex(EMobileTargetType::SURFACE) ].push_back(id);
		else if(unitCategory.isAirCombat() == true)
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][ AAICombatCategory::GetArrayIndex(EMobileTargetType::AIR) ].push_back(id);
		else if(unitCategory.isHoverCombat() == true)
		{
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][ AAICombatCategory::GetArrayIndex(EMobileTargetType::SURFACE) ].push_back(id);
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][ AAICombatCategory::GetArrayIndex(EMobileTargetType::FLOATER) ].push_back(id);
		}
		else if(unitCategory.isSeaCombat() == true)
		{
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][ AAICombatCategory::GetArrayIndex(EMobileTargetType::FLOATER) ].push_back(id);
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][ AAICombatCategory::GetArrayIndex(EMobileTargetType::SUBMERGED) ].push_back(id);
		}

		// set primary and secondary abilities
		m_unitTypeProperties[id].m_primaryAbility     = DeterminePrimaryAbility(unitDefs[id], unitCategory, cb);
		m_unitTypeProperties[id].m_maxSpeed  = DetermineSecondaryAbility(unitDefs[id], unitCategory);
    }

	//-----------------------------------------------------------------------------------------------------------------
	// calculate unit category statistics
	//-----------------------------------------------------------------------------------------------------------------

	m_unitCategoryStatisticsOfSide.resize(m_numberOfSides);

	for(int side = 0; side < m_numberOfSides; ++side)
	{
		m_unitCategoryStatisticsOfSide[side].Init(unitDefs, m_unitTypeProperties, m_unitsInCategory[side], m_unitsInCombatCategory[side]);
	}

    return true;
}

void AAIBuildTree::PrintSummaryToFile(const std::string& filename, springLegacyAI::IAICallback* cb) const
{
	//spring first unitdef id is 1, we remap it so id = is position in array
	const int numberOfUnitTypes = cb->GetNumUnitDefs();
	std::vector<const springLegacyAI::UnitDef*> unitDefs(numberOfUnitTypes+1);
	cb->GetUnitDefList(&unitDefs[1]);

	const std::map<EUnitType, std::string> unitTypes {
			{EUnitType::BUILDING, "building"},
			{EUnitType::MOBILE_UNIT, "mobile unit"},
			{EUnitType::ANTI_SURFACE, "anti surface"},
			{EUnitType::ANTI_AIR, "anti air"},
			{EUnitType::ANTI_SHIP, "anti ship"},
			{EUnitType::ANTI_SUBMERGED, "anti submerged"},
			{EUnitType::ANTI_STATIC, "anti building"},
			{EUnitType::RADAR, "radar"},
			{EUnitType::SONAR, "sonar"},
			{EUnitType::SEISMIC, "seismic detector"},
			{EUnitType::RADAR_JAMMER, "radar jammer"},
			{EUnitType::SONAR_JAMMER, "sonar jammer"},
			{EUnitType::BUILDER, "builder"},
			{EUnitType::FACTORY, "factory"},
			{EUnitType::CONSTRUCTION_ASSIST, "construction assist"}};

	FILE* file = fopen(filename.c_str(), "w+");

	if(file != nullptr)
	{
		fprintf(file, "Number of different unit types: %i\n", unitDefs.size()-1);

		fprintf(file, "Detected start units (aka commanders):\n");
		for(int side = 1; side <= m_numberOfSides; ++side)
		{
			fprintf(file, "%s (%s)  ", unitDefs[ m_startUnitsOfSide[side] ]->humanName.c_str(), unitDefs[ m_startUnitsOfSide[side] ]->name.c_str());
		}
		fprintf(file, "\n");

		fprintf(file, "\nUnit List (human/internal name, side, category)\n");
		for(int id = 1; id < unitDefs.size(); ++id)
		{
			fprintf(file, "ID: %-3i %-40s %-16s %-1i %-18s", id, m_unitTypeProperties[id].m_name.c_str(), unitDefs[id]->name.c_str(), GetSideOfUnitType(UnitDefId(id)), GetCategoryName(GetUnitCategory(UnitDefId(id))).c_str() );

			for(auto unitType = unitTypes.begin(); unitType != unitTypes.end(); ++unitType)
			{
				if( m_unitTypeProperties[id].m_unitType.IsUnitTypeSet(unitType->first) )
				{
					fprintf(file, "  %s", unitType->second.c_str());
				}
			}

			fprintf(file, "\n");
		}

		for(int side = 0; side < m_numberOfSides; ++side)
		{
			fprintf(file, "\n\n####### Side %i (%s) #######", side+1, cfg->SIDE_NAMES[side].c_str() );
			for(AAIUnitCategory category(AAIUnitCategory::GetFirst()); category.End() == false; category.Next())
			{
				fprintf(file, "\n%s:\n", GetCategoryName(category).c_str() );
		
				const StatisticalData& cost      = m_unitCategoryStatisticsOfSide[side].GetUnitCostStatistics(category);
				const StatisticalData& buildtime = m_unitCategoryStatisticsOfSide[side].GetUnitBuildtimeStatistics(category);
				const StatisticalData& range     = m_unitCategoryStatisticsOfSide[side].GetUnitPrimaryAbilityStatistics(category);

				fprintf(file, "Min/max/avg cost: %f/%f/%f, Min/max/avg buildtime: %f/%f/%f Min/max/avg range/buildspeed: %f/%f/%f\n",
								cost.GetMinValue(), cost.GetMaxValue(), cost.GetAvgValue(), 
								buildtime.GetMinValue(), buildtime.GetMaxValue(), buildtime.GetAvgValue(),
								range.GetMinValue(), range.GetMaxValue(), range.GetAvgValue()); 
				fprintf(file, "Units:");
				for(auto defId = m_unitsInCategory[side][category.GetArrayIndex()].begin(); defId != m_unitsInCategory[side][category.GetArrayIndex()].end(); ++defId)
				{
					fprintf(file, "  %s", m_unitTypeProperties[defId->id].m_name.c_str());
				}
				fprintf(file, "\n");
			}

			{
				fprintf(file, "\nRadar:\n");
				const StatisticalData& cost      = m_unitCategoryStatisticsOfSide[side].GetSensorStatistics().m_radarCosts;
				const StatisticalData& range     = m_unitCategoryStatisticsOfSide[side].GetSensorStatistics().m_radarRanges;
				fprintf(file, "Min/max/avg cost: %f/%f/%f,   Min/max/avg range: %f/%f/%f\n",
									cost.GetMinValue(), cost.GetMaxValue(), cost.GetAvgValue(), 
									range.GetMinValue(), range.GetMaxValue(), range.GetAvgValue());
			}

			{
				fprintf(file, "\nSonar:\n");
				const StatisticalData& cost      = m_unitCategoryStatisticsOfSide[side].GetSensorStatistics().m_sonarCosts;
				const StatisticalData& range     = m_unitCategoryStatisticsOfSide[side].GetSensorStatistics().m_sonarRanges;
				fprintf(file, "Min/max/avg cost: %f/%f/%f,   Min/max/avg range: %f/%f/%f\n",
									cost.GetMinValue(), cost.GetMaxValue(), cost.GetAvgValue(), 
									range.GetMinValue(), range.GetMaxValue(), range.GetAvgValue());
			}

			/*for(auto powerPlant = m_unitsInCategory[side][AAIUnitCategory(EUnitCategory::POWER_PLANT).GetArrayIndex()].begin(); powerPlant != m_unitsInCategory[side][AAIUnitCategory(EUnitCategory::POWER_PLANT).GetArrayIndex()].end(); ++powerPlant)
			{
				fprintf(file, "%s: %f %f %f %f\n", unitDefs[powerPlant->id]->humanName.c_str(), 
				 unitDefs[powerPlant->id]->energyMake,
				 unitDefs[powerPlant->id]->tidalGenerator,
				 unitDefs[powerPlant->id]->windGenerator, 
				 unitDefs[powerPlant->id]->energyUpkeep);	
			}*/
		}

		fclose(file);
	}
}

void AAIBuildTree::AssignSideToUnitType(int side, UnitDefId unitDefId)
{
	// avoid "visiting" unit types multiple times (if units can be constructed by more than one other unit)
	if( m_sideOfUnitType[unitDefId.id] == 0)
	{
		// set side of given unit type
		m_sideOfUnitType[unitDefId.id] = side;

		// continue with unit types constructed by given unit type
		for( std::list<UnitDefId>::iterator id = m_unitTypeCanConstructLists[unitDefId.id].begin(); id != m_unitTypeCanConstructLists[unitDefId.id].end(); ++id)
		{
			AssignSideToUnitType(side, *id);
		}
	}
}

float DetermineGeneratedPower(const springLegacyAI::UnitDef* unitDef, springLegacyAI::IAICallback* cb)
{
	if(unitDef->tidalGenerator > 0.0f)
		return cb->GetTidalStrength();
	else if(unitDef->windGenerator  > 0.0f)
		return 0.5f * (cb->GetMinWind() + cb->GetMaxWind());
	else if(unitDef->energyUpkeep < static_cast<float>(-cfg->MIN_ENERGY)) // solar plants
		return - unitDef->energyUpkeep;
	else if(unitDef->energyMake > static_cast<float>(cfg->MIN_ENERGY))
		return unitDef->energyMake;	
	else
		return 0.0f;			
}

float AAIBuildTree::DeterminePrimaryAbility(const springLegacyAI::UnitDef* unitDef, const AAIUnitCategory& unitCategory, springLegacyAI::IAICallback* cb) const
{
	float primaryAbility(0.0f);

	if(   unitCategory.isCombatUnit()
	   || unitCategory.isMobileArtillery()
	   || unitCategory.isStaticArtillery() 
	   || unitCategory.isStaticDefence() )
	{
		for(std::vector<springLegacyAI::UnitDef::UnitDefWeapon>::const_iterator w = unitDef->weapons.begin(); w != unitDef->weapons.end(); ++w)
		{
			if((*w).def->range > primaryAbility)
				primaryAbility = (*w).def->range;
		}
	}
	else if(unitCategory.isScout())
		primaryAbility = unitDef->losRadius;
	else if(unitCategory.isStaticSensor())
		primaryAbility = static_cast<float>(unitDef->radarRadius);
	else if(unitCategory.isStaticConstructor() || unitCategory.isMobileConstructor() || unitCategory.isCommander())
		primaryAbility = unitDef->buildSpeed;
	else if(unitCategory.isMetalExtractor())
		primaryAbility = unitDef->extractsMetal;
	else if(unitCategory.isPowerPlant())
		primaryAbility = DetermineGeneratedPower(unitDef, cb);
	else if(unitCategory.isStorage())
		primaryAbility = unitDef->metalStorage;

	return primaryAbility;
}

float AAIBuildTree::DetermineSecondaryAbility(const springLegacyAI::UnitDef* unitDef, const AAIUnitCategory& unitCategory) const
{
	float secondaryAbility(0.0f);

	if(   unitCategory.isCombatUnit()
	   || unitCategory.isMobileArtillery()
	   || unitCategory.isScout()
	   || unitCategory.isMobileConstructor()
	   || unitCategory.isCommander() )
		secondaryAbility = unitDef->speed;
	else if(unitCategory.isStaticSensor())
		secondaryAbility = unitDef->sonarRadius;
	else if(unitCategory.isStorage())
		secondaryAbility = unitDef->energyStorage;
	
	return secondaryAbility;
}


EMovementType AAIBuildTree::DetermineMovementType(const springLegacyAI::UnitDef* unitDef) const
{
    EMovementType moveType = EMovementType::MOVEMENT_TYPE_UNKNOWN;

    if(unitDef->movedata)
    {
        if(    (unitDef->movedata->moveFamily == MoveData::Tank) 
            || (unitDef->movedata->moveFamily == MoveData::KBot) )
        {
            // check for amphibious units
            if(unitDef->movedata->depth > 250) //! @todo Get magic number from config
                moveType = EMovementType::MOVEMENT_TYPE_AMPHIBIOUS;
            else
                moveType = EMovementType::MOVEMENT_TYPE_GROUND;
        }
        else if(unitDef->movedata->moveFamily == MoveData::Hover) 
        {
            moveType = EMovementType::MOVEMENT_TYPE_HOVER;
        }
        // ship
        else if(unitDef->movedata->moveFamily == MoveData::Ship)
        {
            if(unitDef->categoryString.find("UNDERWATER") != std::string::npos) {
                moveType = EMovementType::MOVEMENT_TYPE_SEA_SUBMERGED;
            } else {
                moveType = EMovementType::MOVEMENT_TYPE_SEA_FLOATER;
            }
        }
    }
    // aircraft
    else if(unitDef->canfly)
        moveType = EMovementType::MOVEMENT_TYPE_AIR;
    // stationary
    else
    {
        if(unitDef->minWaterDepth <= 0)
        {
            moveType = EMovementType::MOVEMENT_TYPE_STATIC_LAND;
        }
        else
        {
            if(unitDef->floater)
                moveType = EMovementType::MOVEMENT_TYPE_STATIC_SEA_FLOATER;
            else
                moveType = EMovementType::MOVEMENT_TYPE_STATIC_SEA_SUBMERGED;
        }
    }

    return moveType;
}

ETargetType AAIBuildTree::DetermineTargetType(const AAIMovementType& moveType) const
{
	if(moveType.IsGround() || moveType.IsHover() || moveType.IsAmphibious())
		return ETargetType::SURFACE;
	else if(moveType.IsAir())
		return ETargetType::AIR;
	else if(moveType.IsShip())
		return ETargetType::FLOATER;
	else if(moveType.IsSubmarine())
		return ETargetType::SUBMERGED;
	else
		return ETargetType::STATIC;
}

void AAIBuildTree::UpdateUnitTypes(UnitDefId unitDefId, const springLegacyAI::UnitDef* unitDef)
{
	//! @todo Add detection of bassault unit types when combat efficiency is available.
	if(m_unitTypeProperties[unitDefId.id].m_unitCategory.isAirCombat())
	{
		m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::MOBILE_UNIT);
		//m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::ANTI_SURFACE);
		//m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::ANTI_AIR);
	}
	else if(   m_unitTypeProperties[unitDefId.id].m_unitCategory.isGroundCombat()
	        || m_unitTypeProperties[unitDefId.id].m_unitCategory.isHoverCombat())
	{
		m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::MOBILE_UNIT);
		//m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::ANTI_SURFACE);
	}
	else if(    m_unitTypeProperties[unitDefId.id].m_unitCategory.isSeaCombat()
	         || m_unitTypeProperties[unitDefId.id].m_unitCategory.isSubmarineCombat())
	{
		m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::MOBILE_UNIT);
		//m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::ANTI_SURFACE);
		//m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::ANTI_SUBMERGED);
	}
	else if(m_unitTypeProperties[unitDefId.id].m_unitCategory.isStaticSensor())
	{
		m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::BUILDING);

		if(unitDef->radarRadius > 0)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::RADAR);

		if(unitDef->sonarRadius > 0)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::SONAR);

		if(unitDef->seismicRadius > 0)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::SEISMIC);
	}
	else if(m_unitTypeProperties[unitDefId.id].m_unitCategory.isStaticSupport())
	{
		m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::BUILDING);

		if(unitDef->jammerRadius > 0)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::RADAR_JAMMER);

		if(unitDef->sonarJamRadius > 0)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::SONAR_JAMMER);
		
		if(unitDef->canAssist)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::CONSTRUCTION_ASSIST);
	}
	else if(   m_unitTypeProperties[unitDefId.id].m_unitCategory.isMobileConstructor() 
	        || m_unitTypeProperties[unitDefId.id].m_unitCategory.isStaticConstructor()
			|| m_unitTypeProperties[unitDefId.id].m_unitCategory.isCommander() )
	{
		if( GetMovementType(unitDefId).IsStatic() )
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::BUILDING);
		else
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::MOBILE_UNIT);

		if(unitDef->canAssist)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::CONSTRUCTION_ASSIST);

		bool builder(false), factory(false);
		for(auto unit = GetCanConstructList(unitDefId).begin(); unit != GetCanConstructList(unitDefId).end(); ++unit)
		{
			if(GetMovementType(*unit).IsStatic())
				builder = true;
			else
				factory = true;
		}

		if(builder)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::BUILDER);

		if(factory)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::FACTORY);
	}
}

EUnitCategory AAIBuildTree::DetermineUnitCategory(const springLegacyAI::UnitDef* unitDef) const
{
	if(m_sideOfUnitType[unitDef->id] == 0)
		return EUnitCategory::UNKNOWN;

	// discard units that are on ignore list
	if(std::find(cfg->DONT_BUILD.begin(), cfg->DONT_BUILD.end(), unitDef->id) != cfg->DONT_BUILD.end())
		return EUnitCategory::UNKNOWN;

	// --------------- buildings --------------------------------------------------------------------------------------
	if(m_unitTypeProperties[unitDef->id].m_movementType.IsStatic() == true)
	{
		if(m_unitTypeCanConstructLists[unitDef->id].size() > 0)
		{
			return EUnitCategory::STATIC_CONSTRUCTOR;
		}
		else if(unitDef->extractsMetal > 0.0f)
		{
			return EUnitCategory::METAL_EXTRACTOR;
		}
		else if(unitDef->isAirBase == true)
		{
			return EUnitCategory::STATIC_SUPPORT;
		}
		else if(   ( (unitDef->energyMake > static_cast<float>(cfg->MIN_ENERGY)) && !unitDef->needGeo )
				|| (unitDef->tidalGenerator > 0.0f)
				|| (unitDef->windGenerator  > 0.0f) 
				|| (unitDef->energyUpkeep < static_cast<float>(-cfg->MIN_ENERGY)) )
		{
			//if(unitDef->radarRadius == 0 && unitDef->sonarRadius == 0) // prevent radar/sonar who make some energy to be classified as power plant
			{
				return EUnitCategory::POWER_PLANT;
			}
		}
		// --------------- armed buildings --------------------------------------------------------------------------------
		else if( (unitDef->weapons.empty() == false) && (GetMaxDamage(unitDef) > 1) )
		{
			// filter out nuke silos, antinukes and stuff like that
			if(IsMissileLauncher(unitDef) == true)
			{
				return EUnitCategory::STATIC_SUPPORT;
			}
			else if(IsDeflectionShieldEmitter(unitDef) == true)
			{
				return EUnitCategory::STATIC_SUPPORT;
			}
			else
			{
				float range(0.0f);
				
				for(std::vector<springLegacyAI::UnitDef::UnitDefWeapon>::const_iterator w = unitDef->weapons.begin(); w != unitDef->weapons.end(); ++w)
				{
					if((*w).def->range > range)
						range = (*w).def->range;
				}

				if( range < cfg->STATIONARY_ARTY_RANGE)
				{
					return EUnitCategory::STATIC_DEFENCE;
				}
				else
				{
					return EUnitCategory::STATIC_ARTILLERY;
				}
			}
		}
		else if((unitDef->radarRadius > 0) || (unitDef->sonarRadius > 0) ) // ignore seismic for now || (unitDef->seismicRadius > 0))
		{
			return EUnitCategory::STATIC_SENSOR;
		}
		else if((unitDef->sonarJamRadius > 0) || (unitDef->jammerRadius > 0))
		{
			return EUnitCategory::STATIC_SUPPORT;
		}
		else if( (unitDef->metalMake > 0.0f) || (std::find(cfg->METAL_MAKERS.begin(), cfg->METAL_MAKERS.end(), unitDef->id) != cfg->METAL_MAKERS.end()) ) //! @todo Does not work - investigate later
		{
			return EUnitCategory::METAL_MAKER;
		}
		else if( (unitDef->metalStorage > static_cast<float>(cfg->MIN_METAL_STORAGE)) || (unitDef->energyStorage > static_cast<float>(cfg->MIN_ENERGY_STORAGE)) )
		{
			return EUnitCategory::STORAGE;
		}
	}
	// --------------- units ------------------------------------------------------------------------------------------
	else
	{
		if( IsStartingUnit(unitDef->id) == true )
		{
			return EUnitCategory::COMMANDER;
		}
		else if(IsScout(unitDef) == true)
		{
			return EUnitCategory::SCOUT;
		}
		else if(IsMobileTransport(unitDef) == true)
		{
			return EUnitCategory::TRANSPORT;
		}

		// --------------- armed units --------------------------------------------------------------------------------
		if( (unitDef->weapons.empty() == false) && (GetMaxDamage(unitDef) > 1))
		{
			if(unitDef->weapons.begin()->def->stockpile)
			{
				return EUnitCategory::MOBILE_SUPPORT;
			}
			else
			{
				if(    (m_unitTypeProperties[unitDef->id].m_movementType.IsGround()     == true) 
				    || (m_unitTypeProperties[unitDef->id].m_movementType.IsAmphibious() == true) )
				{
					if( IsArtillery(unitDef, cfg->GROUND_ARTY_RANGE) == true)
						return EUnitCategory::MOBILE_ARTILLERY;
					else
						return EUnitCategory::GROUND_COMBAT;
				}
				else if(m_unitTypeProperties[unitDef->id].m_movementType.IsHover() == true)
				{
					if( IsArtillery(unitDef, cfg->HOVER_ARTY_RANGE) == true)
						return EUnitCategory::MOBILE_ARTILLERY;
					else
						return EUnitCategory::HOVER_COMBAT;
				}
				else if(m_unitTypeProperties[unitDef->id].m_movementType.IsAir() == true)
				{
					return EUnitCategory::AIR_COMBAT;
				}
				else if(m_unitTypeProperties[unitDef->id].m_movementType.IsSeaUnit() == true)
				{
					//! @todo: Sea artillery is skipped on prupose - handling of sea artillery not implemented at the moment.
					return EUnitCategory::SEA_COMBAT;
				}
			}
		}
		// --------------- unarmed units ------------------------------------------------------------------------------
		else
		{
			if(   (m_unitTypeCanConstructLists[unitDef->id].size() > 0)
					|| (unitDef->canResurrect == true)
					|| (unitDef->canAssist    == true)  )
			{
				return EUnitCategory::MOBILE_CONSTRUCTOR;
			}
			else if( (unitDef->sonarJamRadius > 0) || (unitDef->sonarRadius > 0) || (unitDef->jammerRadius > 0) || (unitDef->radarRadius > 0) )
			{
				return EUnitCategory::MOBILE_SUPPORT;
			}
		}
	}
	
	return EUnitCategory::UNKNOWN;
}

bool AAIBuildTree::IsScout(const springLegacyAI::UnitDef* unitDef) const
{
	if( (unitDef->speed > cfg->SCOUT_SPEED) && (unitDef->canfly == false) )
		return true;
	else
	{
		for(list<int>::iterator i = cfg->SCOUTS.begin(); i != cfg->SCOUTS.end(); ++i)
		{
			if(*i == unitDef->id)
				return true;
		}
	}

	return false;
}

bool AAIBuildTree::IsMobileTransport(const springLegacyAI::UnitDef* unitDef) const
{
	for(list<int>::iterator i = cfg->TRANSPORTERS.begin(); i != cfg->TRANSPORTERS.end(); ++i)
	{
		if(*i == unitDef->id)
			return true;
	}

	return false;
}

bool AAIBuildTree::IsArtillery(const springLegacyAI::UnitDef* unitDef, float artilleryRangeThreshold) const
{
	if(unitDef->weapons.empty() == true)
		return false;

	if(    (m_unitTypeProperties[unitDef->id].m_primaryAbility > artilleryRangeThreshold)
	    || (unitDef->highTrajectoryType == 1) )
		return true;
	else
		return false;
}

bool AAIBuildTree::IsMissileLauncher(const springLegacyAI::UnitDef* unitDef) const
{
	for(vector<springLegacyAI::UnitDef::UnitDefWeapon>::const_iterator weapon = unitDef->weapons.begin(); weapon != unitDef->weapons.end(); ++weapon)
	{
		if( (weapon->def->stockpile == true) && (weapon->def->noAutoTarget == true) )
			return true;
	}

	return false;
}

bool AAIBuildTree::IsDeflectionShieldEmitter(const springLegacyAI::UnitDef* unitDef) const
{
	for(vector<springLegacyAI::UnitDef::UnitDefWeapon>::const_iterator weapon = unitDef->weapons.begin(); weapon != unitDef->weapons.end(); ++weapon)
	{
		if(weapon->def->isShield)
			return true;
	}

	return false;
}


float AAIBuildTree::GetMaxDamage(const springLegacyAI::UnitDef* unitDef) const
{
	float maxDamage = 0.0f;

	for(vector<springLegacyAI::UnitDef::UnitDefWeapon>::const_iterator w = unitDef->weapons.begin(); w != unitDef->weapons.end(); ++w)
	{
		for(int d = 0; d < (*w).def->damages.GetNumTypes(); ++d)
		{
			if((*w).def->damages[d] > maxDamage)
				maxDamage = (*w).def->damages[d];
		}
	}

	return maxDamage;
}

/*bool AAIBuildTree::IsCombatUnit(const springLegacyAI::UnitDef* unitDef) const
{
	for(list<int>::iterator i = cfg->ATTACKERS.begin(); i != cfg->ATTACKERS.end(); ++i)
	{
		if(*i == id)
			return true;
	}

	return false;
}*/

bool AAIBuildTree::CanBuildUnitType(UnitDefId unitDefIdBuilder, UnitDefId unitDefId) const
{
    // look in build options of builder for unit type
    for(std::list<UnitDefId>::const_iterator id = m_unitTypeCanConstructLists[unitDefIdBuilder.id].begin(); id != m_unitTypeCanConstructLists[unitDefIdBuilder.id].end(); ++id)
    {
        if((*id).id == unitDefId.id)
            return true;
    }

    // unit type not found in build options
    return false;
}

bool AAIBuildTree::IsStartingUnit(UnitDefId unitDefId) const
{
    if(m_initialized == false)
        return false;
        
    for(int side = 1; side <= m_numberOfSides; ++side)
    {
        if(m_startUnitsOfSide[side] == unitDefId.id)
            return true;
    }

    return false;
}