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
#include "AAIUnitTypes.h"

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

	m_combatUnitCategories = {  AAIUnitCategory(EUnitCategory::GROUND_COMBAT),
								AAIUnitCategory(EUnitCategory::AIR_COMBAT),
								AAIUnitCategory(EUnitCategory::HOVER_COMBAT),
								AAIUnitCategory(EUnitCategory::SEA_COMBAT),
								AAIUnitCategory(EUnitCategory::SUBMARINE_COMBAT) };
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
		fprintf(saveFile, "%f %f %f %f %f\n", m_combatPowerOfUnits[id].GetValue(ETargetType::SURFACE),
											  m_combatPowerOfUnits[id].GetValue(ETargetType::AIR),
											  m_combatPowerOfUnits[id].GetValue(ETargetType::FLOATER),
											  m_combatPowerOfUnits[id].GetValue(ETargetType::SUBMERGED),
											  m_combatPowerOfUnits[id].GetValue(ETargetType::STATIC));
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
	
		m_combatPowerOfUnits[id].SetValue(ETargetType::SURFACE,   inputValues[0]);
		m_combatPowerOfUnits[id].SetValue(ETargetType::AIR,       inputValues[1]);
		m_combatPowerOfUnits[id].SetValue(ETargetType::FLOATER,   inputValues[2]);
		m_combatPowerOfUnits[id].SetValue(ETargetType::SUBMERGED, inputValues[3]);
		m_combatPowerOfUnits[id].SetValue(ETargetType::STATIC,    inputValues[4]);
	}

	UpdateUnitTypesOfCombatUnits();

	return true;
}

const std::list<UnitDefId>& AAIBuildTree::GetUnitsOfTargetType(const AAITargetType& targetType, int side) const
{
	if(targetType.IsSurface())
		return GetUnitsInCombatUnitCategory(ECombatUnitCategory::SURFACE, side);
	else if(targetType.IsAir())
		return GetUnitsInCategory(EUnitCategory::AIR_COMBAT, side);
	else if(targetType.IsFloater())
		return GetUnitsInCategory(EUnitCategory::SEA_COMBAT, side);
	else if(targetType.IsSubmerged())
		return GetUnitsInCategory(EUnitCategory::SUBMARINE_COMBAT, side);
	else
		return GetUnitsInCategory(EUnitCategory::STATIC_DEFENCE, side);
}

void AAIBuildTree::InitCombatPowerOfUnits(springLegacyAI::IAICallback* cb)
{
	// calculate statistics of max costs of all combat units
	const std::vector<AAIUnitCategory> combatCategories = { AAIUnitCategory(EUnitCategory::GROUND_COMBAT), 
															AAIUnitCategory(EUnitCategory::AIR_COMBAT), 
															AAIUnitCategory(EUnitCategory::HOVER_COMBAT), 
															AAIUnitCategory(EUnitCategory::SEA_COMBAT),
															AAIUnitCategory(EUnitCategory::SUBMARINE_COMBAT),
															AAIUnitCategory(EUnitCategory::STATIC_DEFENCE) };

	StatisticalData unitCosts;
	for(int side = 1; side <= m_numberOfSides; ++side)
	{
		const AAIUnitStatistics& unitStatistics = GetUnitStatistics(side);
		
		for(auto category : combatCategories)
		{
			unitCosts.AddValue( unitStatistics.GetUnitCostStatistics(category).GetMinValue() );
			unitCosts.AddValue( unitStatistics.GetUnitCostStatistics(category).GetMaxValue() );
		}

		unitCosts.AddValue( unitStatistics.GetUnitCostStatistics(EUnitCategory::STATIC_DEFENCE).GetMinValue() );
		unitCosts.AddValue( unitStatistics.GetUnitCostStatistics(EUnitCategory::STATIC_DEFENCE).GetMaxValue() );
	}
	unitCosts.Finalize();

	const float baseCombatPower = AAIConstants::minInitialCombatPower - AAIConstants::noValidTargetInitialCombatPower;
	const float costBasedCombarPower = 0.5f * AAIConstants::maxCombatPower - AAIConstants::minInitialCombatPower;

	const int numberOfUnitTypes = cb->GetNumUnitDefs();
	//spring first unitdef id is 1, we remap it so id = is position in array
	std::vector<const springLegacyAI::UnitDef*> unitDefs(numberOfUnitTypes+1);
	cb->GetUnitDefList(&unitDefs[1]);

	for(int id = 1; id < m_combatPowerOfUnits.size(); ++id)
	{
		const UnitDefId unitDefId(id);
		if( (GetSideOfUnitType(unitDefId) > 0) && (GetUnitCategory(unitDefId).IsCombatUnit() || GetUnitCategory(unitDefId).IsStaticDefence()))
		{
			unsigned int allowedTargetCategories(0u);
			for(const auto& weapon : unitDefs[id]->weapons)
			{
				allowedTargetCategories |= weapon.onlyTargetCat;
			}

			// initial combat power ranges from AAIConstants::noValidTargetInitialCombarPower to 0.5f * AAIConstants::maxCombatPower, 
			// depending on total cost of the unit and its allowed target categories
			const float power = baseCombatPower + costBasedCombarPower * unitCosts.GetNormalizedDeviationFromMin( GetTotalCost(unitDefId) );

			TargetTypeValues combatPower;		
			for(auto targetType : AAITargetType::m_targetTypes)
			{
				int numberOfTargetableUnits(0);
				int totalNumberOfUnits(0);

				for(int side = 1; side <= m_numberOfSides; ++side)
				{
					auto unitList = GetUnitsOfTargetType(targetType, side);
					totalNumberOfUnits += unitList.size();

					for(const auto& unitDefId : unitList)
					{
						if( (allowedTargetCategories & unitDefs[unitDefId.id]->category) != 0u)
							++numberOfTargetableUnits;
					}
				}

				const float targetableUnitsRatio = (totalNumberOfUnits > 0) ? static_cast<float>(numberOfTargetableUnits) / static_cast<float>(totalNumberOfUnits) : 1.0f;
				combatPower.SetValue(targetType, AAIConstants::noValidTargetInitialCombatPower + power * targetableUnitsRatio);	
			}

			m_combatPowerOfUnits[id].SetValues(combatPower);
		}
	}

	UpdateUnitTypesOfCombatUnits();
}

void AAIBuildTree::UpdateUnitTypesOfCombatUnits()
{
	for(int id = 1; id < m_unitTypeProperties.size(); ++id)
	{
		if(m_unitTypeProperties[id].m_unitCategory.IsCombatUnit() || m_unitTypeProperties[id].m_unitCategory.IsStaticDefence() )
		{
			if(m_combatPowerOfUnits[id].GetValue(ETargetType::SURFACE) > AAIConstants::minAntiTargetTypeCombatPower)
				m_unitTypeProperties[id].m_unitType.AddUnitType(EUnitType::ANTI_SURFACE);

			if(m_combatPowerOfUnits[id].GetValue(ETargetType::AIR) > AAIConstants::minAntiTargetTypeCombatPower)
				m_unitTypeProperties[id].m_unitType.AddUnitType(EUnitType::ANTI_AIR);

			if(m_combatPowerOfUnits[id].GetValue(ETargetType::FLOATER) > AAIConstants::minAntiTargetTypeCombatPower)
				m_unitTypeProperties[id].m_unitType.AddUnitType(EUnitType::ANTI_SHIP);

			if(m_combatPowerOfUnits[id].GetValue(ETargetType::SUBMERGED) > AAIConstants::minAntiTargetTypeCombatPower)
				m_unitTypeProperties[id].m_unitType.AddUnitType(EUnitType::ANTI_SUBMERGED);
		}
	} 
}

float AAIBuildTree::CalculateCombatPowerChange(UnitDefId attackerUnitDefId, UnitDefId killedUnitDefId) const
{
	const AAIUnitCategory& killedCategory = GetUnitCategory(killedUnitDefId);

	const AAITargetType& attackerTargetType = GetTargetType(attackerUnitDefId);
	const AAITargetType& killedTargetType   = GetTargetType(killedUnitDefId);

	const float change = AAIConstants::combatPowerLearningFactor * m_combatPowerOfUnits[killedUnitDefId.id].GetValue(attackerTargetType) /  m_combatPowerOfUnits[attackerUnitDefId.id].GetValue(killedTargetType);

	if(change < AAIConstants::maxCombatPowerChangeAfterSingleCombat)
		return change;
	else
		return AAIConstants::maxCombatPowerChangeAfterSingleCombat;
}

void AAIBuildTree::UpdateCombatPowerStatistics(UnitDefId attackerUnitDefId, UnitDefId killedUnitDefId)
{
	const AAIUnitCategory& attackerCategory = GetUnitCategory(attackerUnitDefId);
	const AAIUnitCategory& killedCategory   = GetUnitCategory(killedUnitDefId);

	if(    (attackerCategory.IsCombatUnit() || attackerCategory.IsStaticDefence())
		&& (killedCategory.IsCombatUnit()   || killedCategory.IsStaticDefence()) )
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

	// workaround for AAI to work with mod "beyond all repair"
	if(rootUnits.size() != cfg->numberOfSides)
	{
		rootUnits.clear();
		rootUnits = cfg->m_startUnits;
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
		m_unitsInCombatCategory[side].resize( AAICombatUnitCategory::numberOfCombatUnitCategories );
	}

	//-----------------------------------------------------------------------------------------------------------------
	// set further unit type properties
	//-----------------------------------------------------------------------------------------------------------------

	for(int id = 1; id <= numberOfUnitTypes; ++id)
	{
		m_unitTypeProperties[id].m_totalCost = unitDefs[id]->metalCost + (unitDefs[id]->energyCost / AAIConstants::energyToMetalConversionFactor);
		m_unitTypeProperties[id].m_buildtime = unitDefs[id]->buildTime;
		m_unitTypeProperties[id].m_health    = unitDefs[id]->health;
		m_unitTypeProperties[id].m_name      = unitDefs[id]->humanName;
		m_unitTypeProperties[id].m_footprint = UnitFootprint(unitDefs[id]->xsize, unitDefs[id]->zsize);
		
		m_unitTypeProperties[id].m_movementType.SetMovementType( DetermineMovementType(unitDefs[id]) );
		m_unitTypeProperties[id].m_targetType.SetType( DetermineTargetType(m_unitTypeProperties[id].m_movementType) );
	}

	// second loop because movement type information for all units is needed to determine unit type
	int numberOfFactories(0);

	for(int id = 1; id <= numberOfUnitTypes; ++id)
	{
		// set unit category and add to corresponding unit list (if unit is not neutral)
		const AAIUnitCategory unitCategory( DetermineUnitCategory(unitDefs[id]) );
		m_unitTypeProperties[id].m_unitCategory = unitCategory;

		const UnitDefId unitDefId(id);

		if(m_sideOfUnitType[id] > 0)
		{
			m_unitsInCategory[ m_sideOfUnitType[id]-1 ][ unitCategory.GetArrayIndex() ].push_back(unitDefId);

			UpdateUnitTypes(id, unitDefs[id]);

			if(GetUnitType(unitDefId).IsFactory())
				++numberOfFactories;
		}

		// add combat units to combat category lists
		if(unitCategory.IsGroundCombat())
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][AAICombatUnitCategory::surfaceIndex].push_back(unitDefId);
		else if(unitCategory.IsAirCombat())
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][AAICombatUnitCategory::airIndex].push_back(unitDefId);
		else if(unitCategory.IsHoverCombat())
		{
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][AAICombatUnitCategory::surfaceIndex].push_back(unitDefId);
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][AAICombatUnitCategory::seaIndex].push_back(unitDefId);
		}
		else if(unitCategory.IsSeaCombat())
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][AAICombatUnitCategory::seaIndex].push_back(unitDefId);
		else if(unitCategory.IsSubmarineCombat())
			m_unitsInCombatCategory[ m_sideOfUnitType[id]-1 ][AAICombatUnitCategory::seaIndex].push_back(unitDefId);

		// set primary and secondary abilities
		m_unitTypeProperties[id].m_primaryAbility   = DeterminePrimaryAbility(unitDefs[id], unitCategory, cb);
		m_unitTypeProperties[id].m_secondaryAbility = DetermineSecondaryAbility(unitDefs[id], unitCategory);
	}

	InitFactoryDefIdLookUpTable(numberOfFactories);

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
		fprintf(file, "Number of different unit types: %i\n", static_cast<int>(unitDefs.size())-1);
		fprintf(file, "Number of factories: %i\n", static_cast<int>(m_factoryIdsTable.size()));
		fprintf(file, "Number of sides: %i\n", m_numberOfSides);
		fprintf(file, "Detected start units (aka commanders):\n");
		for(int side = 1; side <= m_numberOfSides; ++side)
		{
			fprintf(file, "%s (%s)  ", unitDefs[ m_startUnitsOfSide[side] ]->humanName.c_str(), unitDefs[ m_startUnitsOfSide[side] ]->name.c_str());
		}
		fprintf(file, "\n");

		fprintf(file, "\nUnit List (human/internal name, internal category, side, category, cost, primary ability, secondary ability) - \n");
		fprintf(file, "  Primary ability:   weapon range for combat units, artillery, or static defences, los for scout, radar(jammer) range, buildtime for constructors, metal extraction for extractors, metal storage capacity for storages), generated power for power plants\n");
		fprintf(file, "  Secondary ability: movement speed for combat units, artillery, scouts, or mobile constructors, sonar(jammer) range, energy storage capacity for storages\n");
		for(int id = 1; id < unitDefs.size(); ++id)
		{
			fprintf(file, "ID: %-3i %-40s %-16s %-8u %-1i %-18s %-6f %-6f %-6f", 
								id, m_unitTypeProperties[id].m_name.c_str(), unitDefs[id]->name.c_str(), unitDefs[id]->category, 
								GetSideOfUnitType(UnitDefId(id)), GetCategoryName(GetUnitCategory(UnitDefId(id))).c_str(),
								GetTotalCost(UnitDefId(id)), GetPrimaryAbility(UnitDefId(id)), GetSecondaryAbility(UnitDefId(id)) );

			for(auto unitType = unitTypes.begin(); unitType != unitTypes.end(); ++unitType)
			{
				if( m_unitTypeProperties[id].m_unitType.IsUnitTypeSet(unitType->first) )
				{
					fprintf(file, "  %s", unitType->second.c_str());
				}
			}

			fprintf(file, "\n");
		}

		fprintf(file, "\nCombat power of combat units & static defences (vs. surface, air, ship, submarine, buildings)\n");
		for(auto category : AAICombatUnitCategory::m_combatUnitCategories )
		{
			fprintf(file, "\n%s units:\n", AAICombatUnitCategory::m_combatCategoryNames[AAICombatUnitCategory(category).GetArrayIndex()].c_str() );
			for(int side = 1; side <= m_numberOfSides; ++side)
			{
				for(auto unitDefId : GetUnitsInCombatUnitCategory(category, side) )
				{
					fprintf(file, "%-30s %-2.3f %-2.3f %-2.3f %-2.3f %-2.3f\n",  m_unitTypeProperties[unitDefId.id].m_name.c_str(),
														m_combatPowerOfUnits[unitDefId.id].GetValue(ETargetType::SURFACE),
														m_combatPowerOfUnits[unitDefId.id].GetValue(ETargetType::AIR),
														m_combatPowerOfUnits[unitDefId.id].GetValue(ETargetType::FLOATER),
														m_combatPowerOfUnits[unitDefId.id].GetValue(ETargetType::SUBMERGED),
														m_combatPowerOfUnits[unitDefId.id].GetValue(ETargetType::STATIC));
				}
			}
		}

		fprintf(file, "\nStatic defences:\n");
		for(int side = 1; side <= m_numberOfSides; ++side)
		{
			for(auto unitDefId : m_unitsInCategory[side-1][AAIUnitCategory(EUnitCategory::STATIC_DEFENCE).GetArrayIndex()] )
			{
				fprintf(file, "%-30s %-2.3f %-2.3f %-2.3f %-2.3f %-2.3f\n",  m_unitTypeProperties[unitDefId.id].m_name.c_str(),
													m_combatPowerOfUnits[unitDefId.id].GetValue(ETargetType::SURFACE),
													m_combatPowerOfUnits[unitDefId.id].GetValue(ETargetType::AIR),
													m_combatPowerOfUnits[unitDefId.id].GetValue(ETargetType::FLOATER),
													m_combatPowerOfUnits[unitDefId.id].GetValue(ETargetType::SUBMERGED),
													m_combatPowerOfUnits[unitDefId.id].GetValue(ETargetType::STATIC));
			}
		}

		for(int side = 0; side < m_numberOfSides; ++side)
		{
			// abort if too many side have been detected (to avoid crash as no name is available from config)
			if(side >= cfg->numberOfSides)
				break;

			fprintf(file, "\n\n####### Side %i (%s) #######", side+1, cfg->sideNames[side].c_str() );
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

	if(   unitCategory.IsCombatUnit()
	   || unitCategory.IsMobileArtillery()
	   || unitCategory.IsStaticArtillery() 
	   || unitCategory.IsStaticDefence() )
	{
		for(std::vector<springLegacyAI::UnitDef::UnitDefWeapon>::const_iterator w = unitDef->weapons.begin(); w != unitDef->weapons.end(); ++w)
		{
			if((*w).def->range > primaryAbility)
				primaryAbility = (*w).def->range;
		}
	}
	else if(unitCategory.IsScout())
		primaryAbility = unitDef->losRadius;
	else if(unitCategory.IsStaticSensor())
		primaryAbility = static_cast<float>(unitDef->radarRadius);
	else if(unitCategory.IsStaticConstructor() || unitCategory.IsMobileConstructor() || unitCategory.IsCommander())
		primaryAbility = unitDef->buildSpeed;
	else if(unitCategory.IsMetalExtractor())
		primaryAbility = unitDef->extractsMetal;
	else if(unitCategory.IsPowerPlant())
		primaryAbility = DetermineGeneratedPower(unitDef, cb);
	else if(unitCategory.IsStorage())
		primaryAbility = unitDef->metalStorage;

	return primaryAbility;
}

float AAIBuildTree::DetermineSecondaryAbility(const springLegacyAI::UnitDef* unitDef, const AAIUnitCategory& unitCategory) const
{
	float secondaryAbility(0.0f);

	if(   unitCategory.IsCombatUnit()
	   || unitCategory.IsMobileArtillery()
	   || unitCategory.IsScout()
	   || unitCategory.IsMobileConstructor()
	   || unitCategory.IsCommander() )
		secondaryAbility = unitDef->speed;
	else if(unitCategory.IsStaticSensor())
		secondaryAbility = unitDef->sonarRadius;
	else if(unitCategory.IsStorage())
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
            if(unitDef->movedata->subMarine)
                moveType = EMovementType::MOVEMENT_TYPE_SEA_SUBMERGED;
            else 
                moveType = EMovementType::MOVEMENT_TYPE_SEA_FLOATER;  
        }
    }
    // aircraft
    else if(unitDef->canfly)
        moveType = EMovementType::MOVEMENT_TYPE_AIR;
    // stationary
    else
    {
        if(unitDef->minWaterDepth <= 0.0f)
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

void AAIBuildTree::InitFactoryDefIdLookUpTable(int numberOfFactories)
{
	m_factoryIdsTable.resize(numberOfFactories);

	int nextFactoryId(0);

	for(int id = 1; id < m_unitTypeProperties.size(); ++id)
	{
		if( (m_sideOfUnitType[id] > 0) && m_unitTypeProperties[id].m_unitType.IsFactory() )
		{
			m_unitTypeProperties[id].m_factoryId.Set(nextFactoryId);
			m_factoryIdsTable[nextFactoryId] = UnitDefId(id);
			++nextFactoryId;
		}
	}
}

void AAIBuildTree::UpdateUnitTypes(UnitDefId unitDefId, const springLegacyAI::UnitDef* unitDef)
{
	if( GetMovementType(unitDefId).IsStatic() )
		m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::BUILDING);
	else
		m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::MOBILE_UNIT);

	if(m_unitTypeProperties[unitDefId.id].m_unitCategory.IsStaticSensor())
	{
		if(unitDef->radarRadius > 0)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::RADAR);

		if(unitDef->sonarRadius > 0)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::SONAR);

		if(unitDef->seismicRadius > 0)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::SEISMIC);
	}
	else if(m_unitTypeProperties[unitDefId.id].m_unitCategory.IsStaticSupport())
	{
		if(unitDef->jammerRadius > 0)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::RADAR_JAMMER);

		if(unitDef->sonarJamRadius > 0)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::SONAR_JAMMER);
		
		if(unitDef->canAssist)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::CONSTRUCTION_ASSIST);
	}
	else if(   m_unitTypeProperties[unitDefId.id].m_unitCategory.IsMobileConstructor() 
	        || m_unitTypeProperties[unitDefId.id].m_unitCategory.IsStaticConstructor()
			|| m_unitTypeProperties[unitDefId.id].m_unitCategory.IsCommander() )
	{
		if(unitDef->canAssist)
			m_unitTypeProperties[unitDefId.id].m_unitType.AddUnitType(EUnitType::CONSTRUCTION_ASSIST);

		bool builder(false), factory(false);
		for(auto constructedUnitDefId : GetCanConstructList(unitDefId))
		{
			if(GetMovementType(constructedUnitDefId).IsStatic())
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
	if(std::find(cfg->m_ignoredUnits.begin(), cfg->m_ignoredUnits.end(), unitDef->id) != cfg->m_ignoredUnits.end())
		return EUnitCategory::UNKNOWN;

	// --------------- buildings --------------------------------------------------------------------------------------
	if(m_unitTypeProperties[unitDef->id].m_movementType.IsStatic())
	{
		if( IsNanoTurret(unitDef) )
		{
			return EUnitCategory::STATIC_ASSISTANCE;
		}
		else if(m_unitTypeCanConstructLists[unitDef->id].size() > 0)
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
		else if( (unitDef->metalMake > 0.0f) || (std::find(cfg->m_metalMakers.begin(), cfg->m_metalMakers.end(), unitDef->id) != cfg->m_metalMakers.end()) ) //! @todo Does not work - investigate later
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
		if(IsStartingUnit(unitDef->id))
		{
			return EUnitCategory::COMMANDER;
		}
		else if(IsScout(unitDef))
		{
			return EUnitCategory::SCOUT;
		}
		else if(IsMobileTransport(unitDef))
		{
			return EUnitCategory::TRANSPORT;
		}

		// --------------- armed units --------------------------------------------------------------------------------
		if(    (m_unitTypeCanConstructLists[unitDef->id].size() > 0)
			|| (unitDef->canResurrect == true)
			|| (unitDef->canAssist    == true)  )
		{
			return EUnitCategory::MOBILE_CONSTRUCTOR;
		}
		else if( (unitDef->weapons.empty() == false) && (GetMaxDamage(unitDef) > 1))
		{
			if(unitDef->weapons.begin()->def->stockpile)
			{
				return EUnitCategory::MOBILE_SUPPORT;
			}
			else
			{
				if(    (m_unitTypeProperties[unitDef->id].m_movementType.IsGround()) 
				    || (m_unitTypeProperties[unitDef->id].m_movementType.IsAmphibious()) )
				{
					if( IsArtillery(unitDef, cfg->GROUND_ARTY_RANGE) == true)
						return EUnitCategory::MOBILE_ARTILLERY;
					else
						return EUnitCategory::GROUND_COMBAT;
				}
				else if(m_unitTypeProperties[unitDef->id].m_movementType.IsHover())
				{
					if( IsArtillery(unitDef, cfg->HOVER_ARTY_RANGE) == true)
						return EUnitCategory::MOBILE_ARTILLERY;
					else
						return EUnitCategory::HOVER_COMBAT;
				}
				else if(m_unitTypeProperties[unitDef->id].m_movementType.IsAir())
				{
					return EUnitCategory::AIR_COMBAT;
				}
				else if(m_unitTypeProperties[unitDef->id].m_movementType.IsShip())
				{
					//! @todo: Sea artillery is skipped on purpose - handling of sea artillery not implemented at the moment.
					return EUnitCategory::SEA_COMBAT;
				}
				else if(m_unitTypeProperties[unitDef->id].m_movementType.IsSubmarine())
				{
					return EUnitCategory::SUBMARINE_COMBAT;
				}
			}
		}
		// --------------- unarmed units ------------------------------------------------------------------------------
		else
		{
			if( (unitDef->sonarJamRadius > 0) || (unitDef->sonarRadius > 0) || (unitDef->jammerRadius > 0) || (unitDef->radarRadius > 0) )
			{
				return EUnitCategory::MOBILE_SUPPORT;
			}
		}
	}
	
	return EUnitCategory::UNKNOWN;
}

bool AAIBuildTree::IsNanoTurret(const springLegacyAI::UnitDef* unitDef) const
{
	return unitDef->canAssist && (unitDef->buildOptions.size() == 0);
}

bool AAIBuildTree::IsScout(const springLegacyAI::UnitDef* unitDef) const
{
	if( (unitDef->speed > cfg->SCOUT_SPEED) && (unitDef->canfly == false) )
		return true;
	else
	{
		for(auto i = cfg->m_scouts.begin(); i != cfg->m_scouts.end(); ++i)
		{
			if(*i == unitDef->id)
				return true;
		}
	}

	return false;
}

bool AAIBuildTree::IsMobileTransport(const springLegacyAI::UnitDef* unitDef) const
{
	for(auto i = cfg->m_transporters.begin(); i != cfg->m_transporters.end(); ++i)
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
	for(auto weapon = unitDef->weapons.begin(); weapon != unitDef->weapons.end(); ++weapon)
	{
		if( (weapon->def->stockpile == true) && (weapon->def->noAutoTarget == true) )
			return true;
	}

	return false;
}

bool AAIBuildTree::IsDeflectionShieldEmitter(const springLegacyAI::UnitDef* unitDef) const
{
	for(auto weapon = unitDef->weapons.begin(); weapon != unitDef->weapons.end(); ++weapon)
	{
		if(weapon->def->isShield)
			return true;
	}

	return false;
}


float AAIBuildTree::GetMaxDamage(const springLegacyAI::UnitDef* unitDef) const
{
	float maxDamage = 0.0f;

	for(auto w = unitDef->weapons.begin(); w != unitDef->weapons.end(); ++w)
	{
		for(int d = 0; d < (*w).def->damages.GetNumTypes(); ++d)
		{
			if((*w).def->damages[d] > maxDamage)
				maxDamage = (*w).def->damages[d];
		}
	}

	return maxDamage;
}

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