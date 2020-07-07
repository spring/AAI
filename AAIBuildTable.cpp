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
vector<vector<list<int>>> AAIBuildTable::units_of_category;
char AAIBuildTable::buildtable_filename[500];
vector<vector<float>> AAIBuildTable::avg_cost;
vector<vector<float>> AAIBuildTable::avg_buildtime;
vector<vector<float>> AAIBuildTable::avg_value;
vector<vector<float>> AAIBuildTable::max_cost;
vector<vector<float>> AAIBuildTable::max_buildtime;
vector<vector<float>> AAIBuildTable::max_value;
vector<vector<float>> AAIBuildTable::min_cost;
vector<vector<float>> AAIBuildTable::min_buildtime;
vector<vector<float>> AAIBuildTable::min_value;
vector< vector< vector<float> > > AAIBuildTable::attacked_by_category_learned;
vector< vector<float> > AAIBuildTable::attacked_by_category_current;
vector<UnitTypeStatic> AAIBuildTable::units_static;
vector<vector<double> >AAIBuildTable::def_power;
vector<double>AAIBuildTable::max_pplant_eff;
/*float* AAIBuildTable::max_builder_buildtime;
float* AAIBuildTable::max_builder_cost;
float* AAIBuildTable::max_builder_buildspeed;*/
vector< vector< vector<float> > > AAIBuildTable::avg_eff;
vector< vector< vector<float> > > AAIBuildTable::max_eff;
vector< vector< vector<float> > > AAIBuildTable::min_eff;
vector< vector< vector<float> > > AAIBuildTable::total_eff;
vector< vector<float> > AAIBuildTable::fixed_eff;

AAIBuildTree AAIBuildTable::s_buildTree;


AAIBuildTable::AAIBuildTable(AAI* ai) :
	initialized(false)
{
	this->ai = ai;

	numOfSides = cfg->SIDES;
	sideNames.resize(numOfSides+1);
	sideNames[0] = "Neutral";

	const UnitDef *temp;

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
	if(ai->getAAIInstance() == 1)
	{
		avg_cost.resize(MOBILE_CONSTRUCTOR+1);
		avg_buildtime.resize(MOBILE_CONSTRUCTOR+1);
		avg_value.resize(MOBILE_CONSTRUCTOR+1);
		max_cost.resize(MOBILE_CONSTRUCTOR+1);
		max_buildtime.resize(MOBILE_CONSTRUCTOR+1);
		max_value.resize(MOBILE_CONSTRUCTOR+1);
		min_cost.resize(MOBILE_CONSTRUCTOR+1);
		min_buildtime.resize(MOBILE_CONSTRUCTOR+1);
		min_value.resize(MOBILE_CONSTRUCTOR+1);
		units_of_category.resize(MOBILE_CONSTRUCTOR+1);

		for(int i = 0; i <= MOBILE_CONSTRUCTOR; ++i)
		{
			// set up the unit lists
			units_of_category[i].resize(numOfSides);

			// statistical values (mod sepcific)
			avg_cost[i].resize(numOfSides);
			avg_buildtime[i].resize(numOfSides);
			avg_value[i].resize(numOfSides);
			max_cost[i].resize(numOfSides);
			max_buildtime[i].resize(numOfSides);
			max_value[i].resize(numOfSides);
			min_cost[i].resize(numOfSides);
			min_buildtime[i].resize(numOfSides);
			min_value[i].resize(numOfSides);

			for(int s = 0; s < numOfSides; ++s)
			{
				avg_cost[i][s] = -1;
				avg_buildtime[i][s] = -1;
				avg_value[i][s] = -1;
				max_cost[i][s] = -1;
				max_buildtime[i][s] = -1;
				max_value[i][s] = -1;
				min_cost[i][s] = -1;
				min_buildtime[i][s] = -1;
				min_value[i][s] = -1;
			}
		}

		// statistical values for builders (map specific)
		/*max_builder_buildtime = new float[numOfSides];
		max_builder_cost = new float[numOfSides];
		max_builder_buildspeed = new float[numOfSides];

		for(int s = 0; s < numOfSides; ++s)
		{
			max_builder_buildtime[s] = -1;
			max_builder_cost[s] = -1;
			max_builder_buildspeed[s] = -1;
		}*/

		// set up attacked_by table
		attacked_by_category_current.resize(cfg->GAME_PERIODS, vector<float>(combat_categories, 0));
		attacked_by_category_learned.resize(3,  vector< vector<float> >(cfg->GAME_PERIODS, vector<float>(combat_categories, 0)));

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
	if(ai->getNumberOfAAIInstances() == 0)
	{
		units_of_category.clear();

		avg_cost.clear();
		avg_buildtime.clear();
		avg_value.clear();
		max_cost.clear();
		max_buildtime.clear();
		max_value.clear();
		min_cost.clear();
		min_buildtime.clear();
		min_value.clear();

		/*spring::SafeDeleteArray(max_builder_buildtime);
		spring::SafeDeleteArray(max_builder_cost);
		spring::SafeDeleteArray(max_builder_buildspeed);*/

		attacked_by_category_learned.clear();
		attacked_by_category_current.clear();

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
	const int numOfUnits = ai->Getcb()->GetNumUnitDefs();

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
		ai->Getcb()->GetUnitDefList(&unitList[1]);
		UnitDef* tmp = new UnitDef();
		tmp->id=0;
		unitList[0] = tmp;
		#ifndef NDEBUG
		for(int i=0; i<numOfUnits; i++) {
			assert(i == GetUnitDef(i).id);
		}
		#endif
	}

	// generate buildtree (if not already done by other instance)
	s_buildTree.Generate(ai->Getcb());

	// Try to load buildtable; if not possible, create a new one
	if(LoadBuildTable() == false)
	{
		// one more than needed because 0 is dummy object
		// (so UnitDef->id can be used to address that unit in the array)
		units_static.resize(numOfUnits+1);
		fixed_eff.resize(numOfUnits+1, vector<float>(combat_categories));

		// temporary list to sort air unit in air only mods
		list<int> *temp_list;
		temp_list = new list<int>[numOfSides];

		// init with 
		for(int i = 0; i <= numOfUnits; ++i)
		{
			units_static[i].unit_type = 0;
		}

		// now calculate efficiency of combat units and get max range
		for(int i = 1; i <= numOfUnits; i++)
		{
			const AAIUnitCategory& category = s_buildTree.GetUnitCategory(UnitDefId(i));
			if( (category.isCombatUnit() == true) || (category.isStaticDefence() == true) )
			{
				const int side                  = s_buildTree.GetSideOfUnitType(UnitDefId(i));
				const AAIUnitCategory& category = s_buildTree.GetUnitCategory(UnitDefId(i));
				const float cost                = s_buildTree.GetTotalCost(UnitDefId(i));

				if(side > 0)
				{
					units_static[i].efficiency.resize(combat_categories, 0.2f);

					const float eff = 1.0f + 5.0f * s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(category).GetNormalizedDeviationFromMin(cost);

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
						if(s_buildTree.GetMovementType(UnitDefId(i)).isStaticLand() == true)
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

		//
		// put units into the different categories
		//
		for(int i = 0; i <= numOfUnits; ++i)
		{
			int side = s_buildTree.GetSideOfUnitType(UnitDefId(i));

			if( (side == 0) || !AllowedToBuild(i))
			{
			}
			// get scouts
			else if(IsScout(i))
			{
				units_of_category[SCOUT][side-1].push_back(GetUnitDef(i).id);
			}
			// get mobile transport
			else if(IsTransporter(i))
			{
				units_of_category[MOBILE_TRANSPORT][side-1].push_back(GetUnitDef(i).id);
			}
			// check if builder or factory
			else if(GetUnitDef(i).buildOptions.size() > 0 && !IsAttacker(i))
			{
				// stationary constructors
				if( s_buildTree.GetMovementType(UnitDefId(i)).isStatic() == true)
				{
					// ground factory or sea factory
					units_of_category[STATIONARY_CONSTRUCTOR][side-1].push_back(GetUnitDef(i).id);
				}
				// mobile constructors
				else
				{
					units_of_category[MOBILE_CONSTRUCTOR][side-1].push_back(GetUnitDef(i).id);
				}
			}
			// no builder or factory
			// check if other building
			else if(s_buildTree.GetMovementType(UnitDefId(i)).isStatic() == true)
			{
				// check if extractor
				if(GetUnitDef(i).extractsMetal)
				{
					units_of_category[EXTRACTOR][side-1].push_back(GetUnitDef(i).id);
				}
				// check if repair pad
				else if(GetUnitDef(i).isAirBase)
				{
					units_of_category[AIR_BASE][side-1].push_back(GetUnitDef(i).id);
				}
				// check if powerplant
				else if(GetUnitDef(i).energyMake > cfg->MIN_ENERGY || GetUnitDef(i).tidalGenerator || GetUnitDef(i).windGenerator || GetUnitDef(i).energyUpkeep < -cfg->MIN_ENERGY)
				{
					if(!GetUnitDef(i).isAirBase && GetUnitDef(i).radarRadius == 0 && GetUnitDef(i).sonarRadius == 0)
					{
						units_of_category[POWER_PLANT][side-1].push_back(GetUnitDef(i).id);
					}
				}
				// check if defence building
				else if(!GetUnitDef(i).weapons.empty() && GetMaxDamage(i) > 1)
				{
					// filter out nuke silos, antinukes and stuff like that
					if(IsMissileLauncher(i))
					{
						units_of_category[STATIONARY_LAUNCHER][side-1].push_back(GetUnitDef(i).id);
					}
					else if(IsDeflectionShieldEmitter(i))
					{
						units_of_category[DEFLECTION_SHIELD][side-1].push_back(GetUnitDef(i).id);
					}
					else
					{
						if( s_buildTree.GetUnitTypeProperties( UnitDefId(i) ).m_range  < cfg->STATIONARY_ARTY_RANGE)
						{
							units_of_category[STATIONARY_DEF][side-1].push_back(GetUnitDef(i).id);
						}
						else
						{
							units_of_category[STATIONARY_ARTY][side-1].push_back(GetUnitDef(i).id);
						}
					}

				}
				// check if radar or sonar
				else if(GetUnitDef(i).radarRadius > 0 || GetUnitDef(i).sonarRadius > 0)
				{
					units_of_category[STATIONARY_RECON][side-1].push_back(GetUnitDef(i).id);
				}
				// check if jammer
				else if(GetUnitDef(i).jammerRadius > 0 || GetUnitDef(i).sonarJamRadius > 0)
				{
					units_of_category[STATIONARY_JAMMER][side-1].push_back(GetUnitDef(i).id);
				}
				// check storage or converter
				else if( GetUnitDef(i).energyStorage > cfg->MIN_ENERGY_STORAGE && !GetUnitDef(i).energyMake)
				{
					units_of_category[STORAGE][side-1].push_back(GetUnitDef(i).id);
				}
				else if(GetUnitDef(i).metalStorage > cfg->MIN_METAL_STORAGE && !GetUnitDef(i).extractsMetal)
				{
					units_of_category[STORAGE][side-1].push_back(GetUnitDef(i).id);
				}
				else if(GetUnitDef(i).makesMetal > 0 || GetUnitDef(i).metalMake > 0 || IsMetalMaker(i))
				{
					units_of_category[METAL_MAKER][side-1].push_back(GetUnitDef(i).id);
				}
			}
			// units that are not builders
			else if(GetUnitDef(i).movedata)
			{
				// ground units
				if(GetUnitDef(i).movedata->moveFamily == MoveData::Tank ||
					GetUnitDef(i).movedata->moveFamily == MoveData::KBot ||
					GetUnitDef(i).movedata->moveFamily == MoveData::Hover)
				{
					// units with weapons
					if((!GetUnitDef(i).weapons.empty() && GetMaxDamage(i) > 1) || IsAttacker(i))
					{
						if(IsMissileLauncher(i))
						{
							units_of_category[MOBILE_LAUNCHER][side-1].push_back(GetUnitDef(i).id);
						}
						else if(GetMaxDamage(GetUnitDef(i).id) > 1)
						{
							// switch between arty and assault
							if(IsArty(i))
							{
								if(GetUnitDef(i).movedata->moveFamily == MoveData::Tank || GetUnitDef(i).movedata->moveFamily == MoveData::KBot)
								{
									units_of_category[GROUND_ARTY][side-1].push_back(GetUnitDef(i).id);
								}
								else
								{
									units_of_category[HOVER_ARTY][side-1].push_back(GetUnitDef(i).id);
								}
							}
							else if(GetUnitDef(i).speed > 0)
							{
								if(GetUnitDef(i).movedata->moveFamily == MoveData::Tank || GetUnitDef(i).movedata->moveFamily == MoveData::KBot)
								{
									units_of_category[GROUND_ASSAULT][side-1].push_back(GetUnitDef(i).id);
								}
								else
								{
									units_of_category[HOVER_ASSAULT][side-1].push_back(GetUnitDef(i).id);
								}
							}
						}

						else if(GetUnitDef(i).sonarJamRadius > 0 || GetUnitDef(i).jammerRadius > 0)
						{
							units_of_category[MOBILE_JAMMER][side-1].push_back(GetUnitDef(i).id);
						}
					}
					// units without weapons
					else
					{
						if(GetUnitDef(i).sonarJamRadius > 0 || GetUnitDef(i).jammerRadius > 0)
						{
							units_of_category[MOBILE_JAMMER][side-1].push_back(GetUnitDef(i).id);
						}
					}
				}
				else if(GetUnitDef(i).movedata->moveFamily == MoveData::Ship)
				{
					// ship
					if(!GetUnitDef(i).weapons.empty())
					{
						if(IsMissileLauncher(i))
						{
							units_of_category[MOBILE_LAUNCHER][side-1].push_back(GetUnitDef(i).id);
						}
						else if(GetMaxDamage(GetUnitDef(i).id) > 1 || IsAttacker(i))
						{
							if(GetUnitDef(i).categoryString.find("UNDERWATER") != string::npos)
							{
								units_of_category[SUBMARINE_ASSAULT][side-1].push_back(GetUnitDef(i).id);
							}
							else
							{
								// switch between arty and assault
								if(IsArty(i))
								{	
									units_of_category[SEA_ARTY][side-1].push_back(GetUnitDef(i).id);
								}
								else
								{
									units_of_category[SEA_ASSAULT][side-1].push_back(GetUnitDef(i).id);
								}
							}
						}
						else if(GetUnitDef(i).sonarJamRadius > 0 || GetUnitDef(i).jammerRadius > 0)
						{
							units_of_category[MOBILE_JAMMER][side-1].push_back(GetUnitDef(i).id);
						}
					}
					else
					{
						if(GetUnitDef(i).sonarJamRadius > 0 || GetUnitDef(i).jammerRadius > 0)
						{
							units_of_category[MOBILE_JAMMER][side-1].push_back(GetUnitDef(i).id);
						}
					}
				}
			}
			// aircraft
			else if(GetUnitDef(i).canfly)
			{
				// units with weapons
				if((!GetUnitDef(i).weapons.empty() && GetMaxDamage(GetUnitDef(i).id) > 1) || IsAttacker(i))
				{
					if(GetUnitDef(i).weapons.begin()->def->stockpile)
					{
						units_of_category[MOBILE_LAUNCHER][side-1].push_back(GetUnitDef(i).id);
					}
					else
					{
						// to apply different sorting rules later
						if(cfg->AIR_ONLY_MOD)
							temp_list[side-1].push_back(GetUnitDef(i).id);

						units_of_category[AIR_ASSAULT][side-1].push_back(GetUnitDef(i).id);
					}
				}
			}

			// get commander
			if( s_buildTree.IsStartingUnit( UnitDefId(i) ) == true )
			{
				units_of_category[COMMANDER][side-1].push_back(GetUnitDef(i).id);
			}
		}

		//
		// determine unit type
		//
		for(int i = 1; i <= numOfUnits; i++)
		{
			// check for factories and builders
			if(s_buildTree.GetCanConstructList(UnitDefId(i)).size() > 0)
			{
				for(std::list<UnitDefId>::const_iterator unit = s_buildTree.GetCanConstructList(UnitDefId(i)).begin(); unit != s_buildTree.GetCanConstructList(UnitDefId(i)).end(); ++unit)
				{
					// filter out neutral and unknown units
					if( (s_buildTree.GetSideOfUnitType(*unit) > 0) && (s_buildTree.GetUnitCategory(*unit).isValid() == true) )
					{
						if(s_buildTree.GetMovementType(*unit).isStatic() == true)
							units_static[i].unit_type |= UNIT_TYPE_BUILDER;
						else
							units_static[i].unit_type |= UNIT_TYPE_FACTORY;
					}
				}

				if(    !(s_buildTree.GetMovementType(UnitDefId(i)).isStatic() == true) 
				    &&  (GetUnitDef(i).canAssist == true) )
					units_static[i].unit_type |= UNIT_TYPE_ASSISTER;
			}

			if(GetUnitDef(i).canResurrect)
				units_static[i].unit_type |= UNIT_TYPE_RESURRECTOR;

			if( s_buildTree.IsStartingUnit( UnitDefId(i) ))
				units_static[i].unit_type |= UNIT_TYPE_COMMANDER;
		}

		// precache stats
		PrecacheStats();

		// save to cache file
		SaveBuildTable(0, LAND_MAP);

		ai->LogConsole("New BuildTable has been created");
	}

	// only once
	if(ai->getAAIInstance() == 1)
	{
		UpdateMinMaxAvgEfficiency();

		float temp;

		def_power.resize(numOfSides);
		max_pplant_eff.resize(numOfSides);

		for(int s = 0; s < numOfSides; ++s)
		{
			def_power[s].resize(s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_DEFENCE, s+1).size());

			// power plant max eff
			max_pplant_eff[s] = 0;

			for(auto plant = s_buildTree.GetUnitsInCategory(EUnitCategory::POWER_PLANT, s+1).begin(); plant != s_buildTree.GetUnitsInCategory(EUnitCategory::POWER_PLANT, s+1).end(); ++plant)
			{
				temp = units_static[plant->id].efficiency[1];

				// eff. of tidal generators have not been calculated yet (depend on map)
				if(temp == 0)
				{
					temp = ai->Getcb()->GetTidalStrength() / s_buildTree.GetTotalCost(UnitDefId(plant->id));

					units_static[plant->id].efficiency[0] = ai->Getcb()->GetTidalStrength();
					units_static[plant->id].efficiency[1] = temp;
				} else if (temp < 0) {
					temp = (ai->Getcb()->GetMaxWind() + ai->Getcb()->GetMinWind()) * 0.5f / s_buildTree.GetTotalCost(UnitDefId(plant->id));

					units_static[plant->id].efficiency[0] = (ai->Getcb()->GetMaxWind() + ai->Getcb()->GetMinWind()) * 0.5f;
					units_static[plant->id].efficiency[1] = temp;
				}

				if(temp > max_pplant_eff[s])
					max_pplant_eff[s] = temp;
			}
		}
	}

	// buildtable is initialized
	initialized = true;
}

void AAIBuildTable::InitCombatEffCache(int side)
{
	side--;

	size_t max_size = 0;

	UnitCategory category;

	for(int cat = 0; cat < combat_categories; ++cat)
	{
		category = GetAssaultCategoryOfID(cat);

		if(units_of_category[(int)category][side].size() > max_size)
			max_size = units_of_category[(int)category][side].size();
	}

	combat_eff.resize(max_size, 0);
}

void AAIBuildTable::ConstructorRequested(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = s_buildTree.GetCanConstructList(constructor).begin();  id != s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		++units_dynamic[(*id).id].constructorsRequested;
	}
}

void AAIBuildTable::ConstructorFinished(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = s_buildTree.GetCanConstructList(constructor).begin();  id != s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		++units_dynamic[(*id).id].constructorsAvailable;
		--units_dynamic[(*id).id].constructorsRequested;
	}
}

void AAIBuildTable::ConstructorKilled(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = s_buildTree.GetCanConstructList(constructor).begin();  id != s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		--units_dynamic[(*id).id].constructorsAvailable;
	}
}

void AAIBuildTable::UnfinishedConstructorKilled(UnitDefId constructor)
{
	for(std::list<UnitDefId>::const_iterator id = s_buildTree.GetCanConstructList(constructor).begin();  id != s_buildTree.GetCanConstructList(constructor).end(); ++id)
	{
		--units_dynamic[(*id).id].constructorsRequested;
	}
}

void AAIBuildTable::PrecacheStats()
{
	for(int side = 1; side <= numOfSides; ++side)
	{
		// precache efficiency of power plants
		for(auto plant = s_buildTree.GetUnitsInCategory(EUnitCategory::POWER_PLANT, side).begin(); plant != s_buildTree.GetUnitsInCategory(EUnitCategory::POWER_PLANT, side).end(); ++plant)
		{
			if(GetUnitDef(plant->id).tidalGenerator)
				units_static[plant->id].efficiency[0] = 0;
			else if (GetUnitDef(plant->id).windGenerator)
				units_static[plant->id].efficiency[0] = -1;
			else if(GetUnitDef(plant->id).energyMake >= cfg->MIN_ENERGY)
				units_static[plant->id].efficiency[0] = GetUnitDef(plant->id).energyMake;
			else if(GetUnitDef(plant->id).energyUpkeep <= -cfg->MIN_ENERGY)
				units_static[plant->id].efficiency[0] = - GetUnitDef(plant->id).energyUpkeep;

			units_static[plant->id].efficiency[1] = units_static[plant->id].efficiency[0] / s_buildTree.GetTotalCost(UnitDefId(plant->id));
		}

		// precache efficiency of extractors
		for(auto extractor = s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).begin(); extractor != s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).end(); ++extractor)
			units_static[extractor->id].efficiency[0] = GetUnitDef(extractor->id).extractsMetal;

		// precache efficiency of metalmakers
		for(auto metalMaker = s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_MAKER, side).begin(); metalMaker != s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_MAKER, side).end(); ++metalMaker)
		{
			if (GetUnitDef(metalMaker->id).makesMetal <= 0.1f) {
				units_static[metalMaker->id].efficiency[0] = 12.0f/600.0f; //FIXME: this somehow is broken...
			} else {
				units_static[metalMaker->id].efficiency[0] = GetUnitDef(metalMaker->id).makesMetal/(GetUnitDef(metalMaker->id).energyUpkeep+1);
			}
		}

		// precache average metal and energy consumption of factories
		float average_metal, average_energy;
		for(auto factory = s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, side).begin(); factory != s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, side).end(); ++factory)
		{
			average_metal = average_energy = 0;

			for(auto unit = s_buildTree.GetCanConstructList(UnitDefId(factory->id)).begin(); unit != s_buildTree.GetCanConstructList(UnitDefId(factory->id)).end(); ++unit)
			{
				average_metal += ( GetUnitDef((*unit).id).metalCost * GetUnitDef(factory->id).buildSpeed ) / GetUnitDef((*unit).id).buildTime;
				average_energy += ( GetUnitDef((*unit).id).energyCost * GetUnitDef(factory->id).buildSpeed ) / GetUnitDef((*unit).id).buildTime;
			}

			units_static[factory->id].efficiency[0] = average_metal  / s_buildTree.GetCanConstructList(UnitDefId(factory->id)).size();
			units_static[factory->id].efficiency[1] = average_energy / s_buildTree.GetCanConstructList(UnitDefId(factory->id)).size();
		}

		// precache range of arty
		for(auto artillery = s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_ARTILLERY, side).begin(); artillery != s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_ARTILLERY, side).end(); ++artillery)
		{
			units_static[artillery->id].efficiency[1] = s_buildTree.GetUnitTypeProperties( UnitDefId(artillery->id) ).m_range;
			units_static[artillery->id].efficiency[0] = 1 + s_buildTree.GetTotalCost(UnitDefId(artillery->id))/100.0;
		}
	}

	for(int s = 0; s < numOfSides; ++s)
	{
		// precache costs and buildtime
		float buildtime;

		for(int i = 1; i <= MOBILE_CONSTRUCTOR; ++i)
		{
			// precache costs
			avg_cost[i][s] = 0;
			this->min_cost[i][s] = 10000;
			this->max_cost[i][s] = 0;

			for(list<int>::iterator unit = units_of_category[i][s].begin(); unit != units_of_category[i][s].end(); ++unit)
			{
				avg_cost[i][s] += s_buildTree.GetTotalCost(UnitDefId(*unit));

				if(s_buildTree.GetTotalCost(UnitDefId(*unit)) > this->max_cost[i][s])
					this->max_cost[i][s] = s_buildTree.GetTotalCost(UnitDefId(*unit));

				if(s_buildTree.GetTotalCost(UnitDefId(*unit)) < this->min_cost[i][s] )
					this->min_cost[i][s] = s_buildTree.GetTotalCost(UnitDefId(*unit));
			}

			if(units_of_category[i][s].size() > 0)
				avg_cost[i][s] /= units_of_category[i][s].size();
			else
			{
				avg_cost[i][s] = -1;
				this->min_cost[i][s] = -1;
				this->max_cost[i][s] = -1;
			}

			// precache buildtime
			min_buildtime[i][s] = 10000;
			avg_buildtime[i][s] = 0;
			max_buildtime[i][s] = 0;

			for(list<int>::iterator unit = units_of_category[i][s].begin(); unit != units_of_category[i][s].end(); ++unit)
			{
				buildtime = GetUnitDef(*unit).buildTime;

				avg_buildtime[i][s] += buildtime;

				if(buildtime > max_buildtime[i][s])
					max_buildtime[i][s] = buildtime;

				if(buildtime < min_buildtime[i][s])
					min_buildtime[i][s] = buildtime;
			}

			if(units_of_category[i][s].size() > 0)
				avg_buildtime[i][s] /= units_of_category[i][s].size();
			else
			{
				avg_buildtime[i][s] = -1;
				min_buildtime[i][s] = -1;
				max_buildtime[i][s] = -1;
			}
		}

		// precache radar ranges
		min_value[STATIONARY_RECON][s] = 10000;
		avg_value[STATIONARY_RECON][s] = 0;
		max_value[STATIONARY_RECON][s] = 0;

		for(list<int>::iterator unit = units_of_category[STATIONARY_RECON][s].begin(); unit != units_of_category[STATIONARY_RECON][s].end(); ++unit)
		{
			avg_value[STATIONARY_RECON][s] += GetUnitDef(*unit).radarRadius;

			if(GetUnitDef(*unit).radarRadius > max_value[STATIONARY_RECON][s])
				max_value[STATIONARY_RECON][s] = GetUnitDef(*unit).radarRadius;

			if(GetUnitDef(*unit).radarRadius < min_value[STATIONARY_RECON][s])
				min_value[STATIONARY_RECON][s] = GetUnitDef(*unit).radarRadius;
		}

		if(units_of_category[STATIONARY_RECON][s].size() > 0)
			avg_value[STATIONARY_RECON][s] /= units_of_category[STATIONARY_RECON][s].size();
		else
		{
			min_value[STATIONARY_RECON][s] = -1;
			avg_value[STATIONARY_RECON][s] = -1;
			max_value[STATIONARY_RECON][s] = -1;
		}

		// precache jammer ranges
		min_value[STATIONARY_JAMMER][s] = 10000;
		avg_value[STATIONARY_JAMMER][s] = 0;
		max_value[STATIONARY_JAMMER][s] = 0;

		for(list<int>::iterator unit = units_of_category[STATIONARY_JAMMER][s].begin(); unit != units_of_category[STATIONARY_JAMMER][s].end(); ++unit)
		{
			avg_value[STATIONARY_JAMMER][s] += GetUnitDef(*unit).jammerRadius;

			if(GetUnitDef(*unit).jammerRadius > max_value[STATIONARY_JAMMER][s])
				max_value[STATIONARY_JAMMER][s] = GetUnitDef(*unit).jammerRadius;

			if(GetUnitDef(*unit).jammerRadius < min_value[STATIONARY_JAMMER][s])
				min_value[STATIONARY_JAMMER][s] = GetUnitDef(*unit).jammerRadius;
		}

		if(units_of_category[STATIONARY_JAMMER][s].size() > 0)
			avg_value[STATIONARY_JAMMER][s] /= units_of_category[STATIONARY_JAMMER][s].size();
		else
		{
			min_value[STATIONARY_JAMMER][s] = -1;
			avg_value[STATIONARY_JAMMER][s] = -1;
			max_value[STATIONARY_JAMMER][s] = -1;
		}

		// precache usage of jammers
		for(list<int>::iterator i = units_of_category[STATIONARY_JAMMER][s].begin(); i != units_of_category[STATIONARY_JAMMER][s].end(); ++i)
		{
			if(GetUnitDef(*i).energyUpkeep - GetUnitDef(*i).energyMake > 0)
				units_static[*i].efficiency[0] = GetUnitDef(*i).energyUpkeep - GetUnitDef(*i).energyMake;
		}

		// precache usage of radar
		for(list<int>::iterator i = units_of_category[STATIONARY_RECON][s].begin(); i != units_of_category[STATIONARY_RECON][s].end(); ++i)
		{
			if(GetUnitDef(*i).energyUpkeep - GetUnitDef(*i).energyMake > 0)
				units_static[*i].efficiency[0] = GetUnitDef(*i).energyUpkeep - GetUnitDef(*i).energyMake;
		}

		// precache extractor efficiency
		min_value[EXTRACTOR][s] = 10000;
		avg_value[EXTRACTOR][s] = 0;
		max_value[EXTRACTOR][s] = 0;

		for(list<int>::iterator unit = units_of_category[EXTRACTOR][s].begin(); unit != units_of_category[EXTRACTOR][s].end(); ++unit)
		{
			avg_value[EXTRACTOR][s] += GetUnitDef(*unit).extractsMetal;

			if(GetUnitDef(*unit).extractsMetal > max_value[EXTRACTOR][s])
				max_value[EXTRACTOR][s] = GetUnitDef(*unit).extractsMetal;

			if(GetUnitDef(*unit).extractsMetal < min_value[EXTRACTOR][s])
				min_value[EXTRACTOR][s] = GetUnitDef(*unit).extractsMetal;
		}

		if(units_of_category[EXTRACTOR][s].size() > 0)
			avg_value[EXTRACTOR][s] /= units_of_category[EXTRACTOR][s].size();
		else
		{
			min_value[EXTRACTOR][s] = -1;
			avg_value[EXTRACTOR][s] = -1;
			max_value[EXTRACTOR][s] = -1;
		}

		// precache power plant energy production
		min_value[POWER_PLANT][s] = 10000;
		avg_value[POWER_PLANT][s] = 0;
		max_value[POWER_PLANT][s] = 0;

		for(list<int>::iterator unit = units_of_category[POWER_PLANT][s].begin(); unit != units_of_category[POWER_PLANT][s].end(); ++unit)
		{
			avg_value[POWER_PLANT][s] += units_static[*unit].efficiency[0];

			if(units_static[*unit].efficiency[0] > max_value[POWER_PLANT][s])
				max_value[POWER_PLANT][s] = units_static[*unit].efficiency[0];

			if(units_static[*unit].efficiency[0] < min_value[POWER_PLANT][s])
				min_value[POWER_PLANT][s] = units_static[*unit].efficiency[0];
		}

		if(units_of_category[POWER_PLANT][s].size() > 0)
			avg_value[POWER_PLANT][s] /= units_of_category[POWER_PLANT][s].size();
		else
		{
			min_value[POWER_PLANT][s] = -1;
			avg_value[POWER_PLANT][s] = -1;
			max_value[POWER_PLANT][s] = -1;
		}

		// precache stationary arty range
		min_value[STATIONARY_ARTY][s] = 100000;
		avg_value[STATIONARY_ARTY][s] = 0;
		max_value[STATIONARY_ARTY][s] = 0;

		for(list<int>::iterator unit = units_of_category[STATIONARY_ARTY][s].begin(); unit != units_of_category[STATIONARY_ARTY][s].end(); ++unit)
		{
			avg_value[STATIONARY_ARTY][s] += units_static[*unit].efficiency[1];

			if(units_static[*unit].efficiency[1] > max_value[STATIONARY_ARTY][s])
				max_value[STATIONARY_ARTY][s] = units_static[*unit].efficiency[1];

			if(units_static[*unit].efficiency[1] < min_value[STATIONARY_ARTY][s])
				min_value[STATIONARY_ARTY][s] = units_static[*unit].efficiency[1];
		}

		if(units_of_category[STATIONARY_ARTY][s].size() > 0)
			avg_value[STATIONARY_ARTY][s] /= units_of_category[STATIONARY_ARTY][s].size();
		else
		{
			min_value[STATIONARY_ARTY][s] = -1;
			avg_value[STATIONARY_ARTY][s] = -1;
			max_value[STATIONARY_ARTY][s] = -1;
		}

		// precache scout los
		min_value[SCOUT][s] = 100000;
		avg_value[SCOUT][s] = 0;
		max_value[SCOUT][s] = 0;

		for(list<int>::iterator unit = units_of_category[SCOUT][s].begin(); unit != units_of_category[SCOUT][s].end(); ++unit)
		{
			avg_value[SCOUT][s] += GetUnitDef(*unit).losRadius;

			if(GetUnitDef(*unit).losRadius > max_value[SCOUT][s])
				max_value[SCOUT][s] = GetUnitDef(*unit).losRadius;

			if(GetUnitDef(*unit).losRadius < min_value[SCOUT][s])
				min_value[SCOUT][s] = GetUnitDef(*unit).losRadius;
		}

		if(units_of_category[SCOUT][s].size() > 0)
			avg_value[SCOUT][s] /= units_of_category[SCOUT][s].size();
		else
		{
			min_value[SCOUT][s] = -1;
			avg_value[SCOUT][s] = -1;
			max_value[SCOUT][s] = -1;
		}

		// precache stationary defences weapon range
		min_value[STATIONARY_DEF][s] = 100000;
		avg_value[STATIONARY_DEF][s] = 0;
		max_value[STATIONARY_DEF][s] = 0;

		float range;

		if(units_of_category[STATIONARY_DEF][s].size() > 0)
		{
			for(list<int>::iterator unit = units_of_category[STATIONARY_DEF][s].begin(); unit != units_of_category[STATIONARY_DEF][s].end(); ++unit)
			{
				range = s_buildTree.GetMaxRange(UnitDefId(*unit));

				avg_value[STATIONARY_DEF][s] += range;

				if(range > max_value[STATIONARY_DEF][s])
					max_value[STATIONARY_DEF][s] = range;

				if(range < min_value[STATIONARY_DEF][s])
					min_value[STATIONARY_DEF][s] = range;
			}

			avg_value[STATIONARY_DEF][s] /= (float)units_of_category[STATIONARY_DEF][s].size();
		}
		else
		{
			min_value[STATIONARY_DEF][s] = -1;
			avg_value[STATIONARY_DEF][s] = -1;
			max_value[STATIONARY_DEF][s] = -1;
		}

		// precache builders' buildspeed
		float buildspeed;

		if(units_of_category[MOBILE_CONSTRUCTOR][s].size() > 0)
		{
			min_value[MOBILE_CONSTRUCTOR][s] = 100000;
			avg_value[MOBILE_CONSTRUCTOR][s] = 0;
			max_value[MOBILE_CONSTRUCTOR][s] = 0;

			for(list<int>::iterator unit = units_of_category[MOBILE_CONSTRUCTOR][s].begin(); unit != units_of_category[MOBILE_CONSTRUCTOR][s].end(); ++unit)
			{
				buildspeed = GetUnitDef(*unit).buildSpeed;

				avg_value[MOBILE_CONSTRUCTOR][s] += buildspeed;

				if(buildspeed > max_value[MOBILE_CONSTRUCTOR][s])
					max_value[MOBILE_CONSTRUCTOR][s] = buildspeed;

				if(buildspeed < min_value[MOBILE_CONSTRUCTOR][s])
					min_value[MOBILE_CONSTRUCTOR][s] = buildspeed;
			}

			avg_value[MOBILE_CONSTRUCTOR][s] /= (float)units_of_category[MOBILE_CONSTRUCTOR][s].size();
		}
		else
		{
			min_value[MOBILE_CONSTRUCTOR][s] = -1;
			avg_value[MOBILE_CONSTRUCTOR][s] = -1;
			max_value[MOBILE_CONSTRUCTOR][s] = -1;
		}

		// precache unit speed and weapons range
		for(list<UnitCategory>::iterator category = assault_categories.begin(); category != assault_categories.end(); ++category)
		{
			// precache range
			min_value[*category][s] = 10000;
			avg_value[*category][s] = 0;
			max_value[*category][s] = 0;

			if(units_of_category[*category][s].size() > 0)
			{
				for(list<int>::iterator unit = units_of_category[*category][s].begin(); unit != units_of_category[*category][s].end(); ++unit)
				{
					range = s_buildTree.GetUnitTypeProperties( UnitDefId(*unit) ).m_range;

					avg_value[*category][s] += range;

					if(range > max_value[*category][s])
						max_value[*category][s] = range;

					if(range < min_value[*category][s])
						min_value[*category][s] = range;
				}

				avg_value[*category][s] /= (float)units_of_category[*category][s].size();
			}
			else
			{
				min_value[*category][s] = -1;
				avg_value[*category][s] = -1;
				max_value[*category][s] = -1;
			}
		}
	}
}

void AAIBuildTable::PrecacheCosts()
{
	for(int s = 0; s < numOfSides; ++s)
	{
		for(int i = 1; i <= MOBILE_CONSTRUCTOR; ++i)
		{
			// precache costs
			avg_cost[i][s] = 0;
			this->min_cost[i][s] = 10000;
			this->max_cost[i][s] = 0;

			for(list<int>::iterator unit = units_of_category[i][s].begin(); unit != units_of_category[i][s].end(); ++unit)
			{
				avg_cost[i][s] += s_buildTree.GetTotalCost(UnitDefId(*unit));

				if(s_buildTree.GetTotalCost(UnitDefId(*unit)) > this->max_cost[i][s])
					this->max_cost[i][s] = s_buildTree.GetTotalCost(UnitDefId(*unit));

				if(s_buildTree.GetTotalCost(UnitDefId(*unit)) < this->min_cost[i][s] )
					this->min_cost[i][s] = s_buildTree.GetTotalCost(UnitDefId(*unit));
			}

			if(units_of_category[i][s].size() > 0)
				avg_cost[i][s] /= units_of_category[i][s].size();
			else
			{
				avg_cost[i][s] = -1;
				this->min_cost[i][s] = -1;
				this->max_cost[i][s] = -1;
			}
		}
	}
}


UnitType AAIBuildTable::GetUnitType(int def_id)
{
	if(cfg->AIR_ONLY_MOD)
	{
		return ASSAULT_UNIT;
	}
	else
	{
		if (units_static.empty()) return UNKNOWN_UNIT;
		const AAIUnitCategory& category = s_buildTree.GetUnitCategory(UnitDefId(def_id));

		int side = s_buildTree.GetSideOfUnitType(UnitDefId(def_id))-1;

		if(side < 0)
			return UnitType::UNKNOWN_UNIT;

		if(category.isGroundCombat() == true)
		{
			if( units_static[def_id].efficiency[1] / max_eff[side][0][1]  > 6.0f * units_static[def_id].efficiency[0] / max_eff[side][0][0] )
				return ANTI_AIR_UNIT;
			else
				return ASSAULT_UNIT;
		}
		else if(category.isAirCombat() == true)
		{
			float vs_building = units_static[def_id].efficiency[5] / max_eff[side][1][5];

			float vs_units = (units_static[def_id].efficiency[0] / max_eff[side][1][0]
							+ units_static[def_id].efficiency[3] / max_eff[side][1][3]) / 2.0f;

			if( units_static[def_id].efficiency[1]  / max_eff[side][1][1] > 2 * (vs_building + vs_units) )
				return ANTI_AIR_UNIT;
			else
			{
				if(vs_building > 4 * vs_units || GetUnitDef(def_id).type == string("Bomber"))
					return BOMBER_UNIT;
				else
					return ASSAULT_UNIT;
			}
		}
		else if(category.isHoverCombat() == true)
		{
			if( units_static[def_id].efficiency[1] / max_eff[side][2][1] > 6.0f * units_static[def_id].efficiency[0] / max_eff[side][2][0] )
				return ANTI_AIR_UNIT;
			else
				return ASSAULT_UNIT;
		}
		else if(category.isSeaCombat() == true)
		{
			if( units_static[def_id].efficiency[1] / max_eff[side][3][1] > 6.0f * units_static[def_id].efficiency[3] / max_eff[side][3][3] )
				return ANTI_AIR_UNIT;
			else
				return ASSAULT_UNIT;
		}
		else if(category.isSubmarineCombat() == SUBMARINE_ASSAULT)
		{
			if( units_static[def_id].efficiency[1] / max_eff[side][4][1] > 6 * units_static[def_id].efficiency[3] / max_eff[side][4][3] )
				return ANTI_AIR_UNIT;
			else
				return ASSAULT_UNIT;
		}
		else if(category.isMobileArtillery() == true)
		{
			return ARTY_UNIT;
		} else //throw "AAIBuildTable::GetUnitType: invalid unit category";
			return UNKNOWN_UNIT;
	}
}

bool AAIBuildTable::MemberOf(int unit_id, list<int> unit_list)
{
	// test all units in list
	for(list<int>::iterator i = unit_list.begin(); i != unit_list.end(); ++i)
	{
		if(*i == unit_id)
			return true;
	}

	// unitid not found
	return false;
}

int AAIBuildTable::GetPowerPlant(int side, float cost, float urgency, float power, float /*current_energy*/, bool water, bool geo, bool canBuild)
{
	UnitTypeStatic *unit;

	int best_unit = 0;

	float best_ranking = -10000, my_ranking;

	//debug
	//ai->Log("Selecting power plant:     power %f    cost %f    urgency %f   energy %f \n", power, cost, urgency, current_energy);

	for(list<int>::iterator pplant = units_of_category[POWER_PLANT][side-1].begin(); pplant != units_of_category[POWER_PLANT][side-1].end(); ++pplant)
	{
		unit = &units_static[*pplant];

		if(canBuild && units_dynamic[*pplant].constructorsAvailable <= 0)
			my_ranking = -10000;
		else if(!geo && GetUnitDef(*pplant).needGeo)
			my_ranking = -10000;
		else if( (!water && GetUnitDef(*pplant).minWaterDepth <= 0) || (water && GetUnitDef(*pplant).minWaterDepth > 0) )
		{
			my_ranking = cost * unit->efficiency[1] / max_pplant_eff[side-1] + power * unit->efficiency[0] / max_value[POWER_PLANT][side-1]
						- urgency * (GetUnitDef(*pplant).buildTime / max_buildtime[POWER_PLANT][side-1]);

			//
			if(s_buildTree.GetTotalCost(UnitDefId(*pplant)) >= max_cost[POWER_PLANT][side-1])
				my_ranking -= (cost + urgency + power)/2.0f;

			//ai->Log("%-20s: %f\n", GetUnitDef(*pplant)->humanName.c_str(), my_ranking);
		}
		else
			my_ranking = -10000;

		if(my_ranking > best_ranking)
		{
				best_ranking = my_ranking;
				best_unit = *pplant;
		}
	}

	// 0 if no unit found (list was probably empty)
	return best_unit;
}

bool AAIBuildTable::IsBuildingSelectable(UnitDefId building, bool water, bool mustBeConstructable) const
{
	const bool constructablePassed = !mustBeConstructable || (units_dynamic[building.id].constructorsAvailable > 0);
	const bool landCheckPassed     = !water    && s_buildTree.GetMovementType(building.id).isStaticLand();
	const bool seaCheckPassed      =  water    && s_buildTree.GetMovementType(building.id).isStaticSea();

	return constructablePassed && (landCheckPassed || seaCheckPassed );
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

	const AAIUnitStatistics& unitStatistics           = s_buildTree.GetUnitStatistics(side);
	const StatisticalData&   extractedMetalStatistics = unitStatistics.GetUnitPrimaryAbilityStatistics(EUnitCategory::METAL_EXTRACTOR);
	const StatisticalData&   costStatistics           = unitStatistics.GetUnitCostStatistics(EUnitCategory::METAL_EXTRACTOR);

	for(auto extractorDefId = s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).begin(); extractorDefId != s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).end(); ++extractorDefId)
	{
		// check if under water or ground || water = true and building under water
		if( IsBuildingSelectable(*extractorDefId, water, mustBeConstructable) )
		{
			const float metalExtraction = s_buildTree.GetMaxRange( *extractorDefId );

			float myRating =   extractedMetal * extractedMetalStatistics.GetNormalizedDeviationFromMin(metalExtraction)
						     + cost           * costStatistics.GetNormalizedDeviationFromMax(s_buildTree.GetTotalCost(*extractorDefId));

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
		for(auto extractor = s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).begin(); extractor != s_buildTree.GetUnitsInCategory(EUnitCategory::METAL_EXTRACTOR, side).end(); ++extractor)
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

int AAIBuildTable::GetStorage(int side, float cost, float metal, float energy, float urgency, bool water, bool canBuild)
{
	int best_storage = 0;
	float best_rating = 0, my_rating;

	for(list<int>::iterator storage = units_of_category[STORAGE][side-1].begin(); storage != units_of_category[STORAGE][side-1].end(); ++storage)
	{
		if(canBuild && units_dynamic[*storage].constructorsAvailable <= 0)
			my_rating = 0;
		else if(!water && GetUnitDef(*storage).minWaterDepth <= 0)
		{
			my_rating = (metal * GetUnitDef(*storage).metalStorage + energy * GetUnitDef(*storage).energyStorage)
				/(cost * s_buildTree.GetTotalCost(UnitDefId(*storage)) + urgency * GetUnitDef(*storage).buildTime);
		}
		else if(water && GetUnitDef(*storage).minWaterDepth > 0)
		{
			my_rating = (metal * GetUnitDef(*storage).metalStorage + energy * GetUnitDef(*storage).energyStorage)
				/(cost * s_buildTree.GetTotalCost(UnitDefId(*storage)) + urgency * GetUnitDef(*storage).buildTime);
		}
		else
			my_rating = 0;


		if(my_rating > best_rating)
		{
			best_rating = my_rating;
			best_storage = *storage;
		}
	}

	return best_storage;
}

int AAIBuildTable::GetMetalMaker(int side, float cost, float efficiency, float metal, float urgency, bool water, bool canBuild)
{
	int best_maker = 0;
	float best_rating = 0, my_rating;

	for(list<int>::iterator maker = units_of_category[METAL_MAKER][side-1].begin(); maker != units_of_category[METAL_MAKER][side-1].end(); ++maker)
	{

		//ai->LogConsole("MakesMetal: %f", GetUnitDef(*maker).makesMetal);
		//this somehow got broken in spring... :(
		float makesMetal = GetUnitDef(*maker).makesMetal;
		if (makesMetal <= 0.1f) {
			makesMetal = 12.0f/600.0f;
		}

		if(canBuild && units_dynamic[*maker].constructorsAvailable <= 0)
			my_rating = 0;
		else if(!water && GetUnitDef(*maker).minWaterDepth <= 0)
		{
			my_rating = (pow((long double) efficiency * units_static[*maker].efficiency[0], (long double) 1.4) + pow((long double) metal * makesMetal, (long double) 1.6))
				/(pow((long double) cost * s_buildTree.GetTotalCost(UnitDefId(*maker)),(long double) 1.4) + pow((long double) urgency * GetUnitDef(*maker).buildTime,(long double) 1.4));
		}
		else if(water && GetUnitDef(*maker).minWaterDepth > 0)
		{
			my_rating = (pow((long double) efficiency * units_static[*maker].efficiency[0], (long double) 1.4) + pow((long double) metal * makesMetal, (long double) 1.6))
				/(pow((long double) cost * s_buildTree.GetTotalCost(UnitDefId(*maker)),(long double) 1.4) + pow((long double) urgency * GetUnitDef(*maker).buildTime,(long double) 1.4));
		}
		else
			my_rating = 0;


		if(my_rating > best_rating)
		{
			best_rating = my_rating;
			best_maker = *maker;
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

	for(auto factory = units_of_category[STATIONARY_CONSTRUCTOR][side-1].begin(); factory != units_of_category[STATIONARY_CONSTRUCTOR][side-1].end(); ++factory)
	{
		if(units_dynamic[*factory].constructorsAvailable > 0)
		{
			FactoryRatingInputData data;
			CalculateFactoryRating(data, UnitDefId(*factory), combatPowerWeights, mapType);
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

	const StatisticalData& costStatistics = s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(EUnitCategory::STATIC_CONSTRUCTOR);

	//ai->Log("Combat power weights: ground %f   air %f   hover %f   sea %f   submarine %f\n", combatPowerWeights.vsGround, combatPowerWeights.vsAir, combatPowerWeights.vsHover, combatPowerWeights.vsSea, combatPowerWeights.vsSubmarine);
	//ai->Log("Factory ratings (max combat power rating %f):", combatPowerRatingStatistics.GetMaxValue());

	for(auto factory = factoryList.begin(); factory != factoryList.end(); ++factory)
	{
		float myRating =  0.5f * costStatistics.GetNormalizedDeviationFromMax(s_buildTree.GetTotalCost(factory->factoryDefId))
		                + 1.0f * combatPowerRatingStatistics.GetNormalizedDeviationFromMin(factory->combatPowerRating);  

		if(factory->canConstructBuilder)
			myRating += 0.2f;

		if(factory->canConstructScout)
			myRating += 0.4f;

		//ai->Log(" %s %f %f", s_buildTree.GetUnitTypeProperties(factory->factoryDefId).m_name.c_str(), myRating, factory->combatPowerRating);
	
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

	for(auto unit = s_buildTree.GetCanConstructList(factoryDefId).begin(); unit != s_buildTree.GetCanConstructList(factoryDefId).end(); ++unit)
	{
		switch(s_buildTree.GetUnitCategory(*unit).getUnitCategory())
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
				if( s_buildTree.GetMovementType(*unit).isSeaUnit() )
				{
					if(considerWater)
						ratingData.canConstructBuilder = true;
				}
				else if(s_buildTree.GetMovementType(*unit).isGround() )
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
				if( s_buildTree.GetMovementType(*unit).isSeaUnit() )
				{
					if(considerWater)
						ratingData.canConstructScout = true;
				}
				else if(s_buildTree.GetMovementType(*unit).isGround() )
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
	const std::list<UnitDefId> unitList = s_buildTree.GetUnitsInCategory(category, side);

	const StatisticalData& costs      = s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(category);
	const StatisticalData& ranges     = s_buildTree.GetUnitStatistics(side).GetUnitPrimaryAbilityStatistics(category);
	const StatisticalData& buildtimes = s_buildTree.GetUnitStatistics(side).GetUnitBuildtimeStatistics(category);

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
			const UnitTypeProperties& unitData = s_buildTree.GetUnitTypeProperties(*defence);

			float combatPower =   combatCriteria.vsGround * unit->efficiency[0] + combatCriteria.vsAir * unit->efficiency[1] 
								+ combatCriteria.vsHover * unit->efficiency[2] + combatCriteria.vsSea * unit->efficiency[3] 
						  		+ combatCriteria.vsSubmarine * unit->efficiency[4];

			float myRating =  cost        * costs.GetNormalizedDeviationFromMax( unitData.m_totalCost )
							+ buildtime   * buildtimes.GetNormalizedDeviationFromMax( unitData.m_buildtime )
							+ range       * ranges.GetNormalizedDeviationFromMin( unitData.m_range )
							+ combatPower * combatPowerStat.GetNormalizedDeviationFromMin( combatPower )
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

int AAIBuildTable::GetCheapDefenceBuilding(int side, double efficiency, double combat_power, double cost, double urgency, double ground_eff, double air_eff, double hover_eff, double sea_eff, double submarine_eff, bool water)
{
	--side;

	double best_ranking = -100000, my_ranking;
	int best_defence = 0;

	UnitTypeStatic *unit;

	double my_power;

	double total_eff = ground_eff + air_eff + hover_eff + sea_eff + submarine_eff;
	double max_eff_selection = 0;
	double max_power = 0;

	uint32_t buildingTypeBitmask = 0;

	if(water)
		buildingTypeBitmask =   static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_STATIC_SEA_FLOATER)
						      + static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_STATIC_SEA_SUBMERGED);
	else
		buildingTypeBitmask =   static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_STATIC_LAND);

	int k = 0;

	// use my_power as temp var
	for(list<int>::iterator defence = units_of_category[STATIONARY_DEF][side].begin(); defence != units_of_category[STATIONARY_DEF][side].end(); ++defence)
	{
		if(    (units_dynamic[*defence].constructorsAvailable > 0) 
			&& (s_buildTree.GetMovementType(UnitDefId(*defence)).isIncludedIn(buildingTypeBitmask) == true) )
		{
			unit = &units_static[*defence];

			// calculate eff.
			my_power = ground_eff * unit->efficiency[0] / avg_eff[side][5][0] + air_eff * unit->efficiency[1] / avg_eff[side][5][1]
					+ hover_eff * unit->efficiency[2] / avg_eff[side][5][2] + sea_eff * unit->efficiency[3] / avg_eff[side][5][3]
					+ submarine_eff * unit->efficiency[4] / avg_eff[side][5][4];
			my_power /= total_eff;

			// store result
			def_power[side][k] = my_power;

			if(my_power > max_power)
				max_power = my_power;

			// calculate eff
			my_power /= s_buildTree.GetTotalCost(UnitDefId(*defence));

			if(my_power > max_eff_selection)
				max_eff_selection = my_power;

			++k;
		}
	}

	// something went wrong
	if(max_eff_selection <= 0)
		return 0;

	// reset counter
	k = 0;

	// calculate rating
	for(list<int>::iterator defence = units_of_category[STATIONARY_DEF][side].begin(); defence != units_of_category[STATIONARY_DEF][side].end(); ++defence)
	{
		if(    (units_dynamic[*defence].constructorsAvailable > 0)
		    && (s_buildTree.GetMovementType(UnitDefId(*defence)).isIncludedIn(buildingTypeBitmask) == true) )
		{
			unit = &units_static[*defence];

			my_ranking = efficiency * (def_power[side][k] / s_buildTree.GetTotalCost(UnitDefId(*defence))) / max_eff_selection
						+ combat_power * def_power[side][k] / max_power
						- cost * s_buildTree.GetTotalCost(UnitDefId(*defence)) / avg_cost[STATIONARY_DEF][side]
						- urgency * GetUnitDef(*defence).buildTime / max_buildtime[STATIONARY_DEF][side];

			if(my_ranking > best_ranking)
			{
				best_ranking = my_ranking;
				best_defence = *defence;
			}

			++k;
			//ai->Log("%-20s: %f %f %f %f %f\n", GetUnitDef(unit->id).humanName.c_str(), t1, t2, t3, t4, my_ranking);
		}
	}

	return best_defence;
}

int AAIBuildTable::GetAirBase(int side, float /*cost*/, bool water, bool canBuild)
{
	float best_ranking = 0, my_ranking;
	int best_airbase = 0;

	for(list<int>::iterator airbase = units_of_category[AIR_BASE][side-1].begin(); airbase != units_of_category[AIR_BASE][side-1].end(); ++airbase)
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
	}
	return best_airbase;
}

int AAIBuildTable::GetStationaryArty(int side, float cost, float range, float efficiency, bool water, bool canBuild)
{
	float best_ranking = 0, my_ranking;
	int best_arty = 0;

	for(list<int>::iterator arty = units_of_category[STATIONARY_ARTY][side-1].begin(); arty != units_of_category[STATIONARY_ARTY][side-1].end(); ++arty)
	{
		// check if water
		if(canBuild && units_dynamic[*arty].constructorsAvailable <= 0)
			my_ranking = 0;
		else if(!water && GetUnitDef(*arty).minWaterDepth <= 0)
		{
			my_ranking =  (range * units_static[*arty].efficiency[1] + efficiency * units_static[*arty].efficiency[0]) / (cost * s_buildTree.GetTotalCost(UnitDefId(*arty)));
		}
		else if(water && GetUnitDef(*arty).minWaterDepth > 0)
		{
			my_ranking =  (range * units_static[*arty].efficiency[1] + efficiency * units_static[*arty].efficiency[0]) / (cost * s_buildTree.GetTotalCost(UnitDefId(*arty)));
		}
		else
			my_ranking = 0;

		if(my_ranking > best_ranking)
		{
			best_ranking = my_ranking;
			best_arty = *arty;
		}
	}
	return best_arty;
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

	const StatisticalData& costs  = s_buildTree.GetUnitStatistics(side).GetSensorStatistics().m_radarCosts;
	const StatisticalData& ranges = s_buildTree.GetUnitStatistics(side).GetSensorStatistics().m_radarRanges;

	for(auto sensor = s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SENSOR, side).begin(); sensor != s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SENSOR, side).end(); ++sensor)
	{
		//! @todo replace by checking unit type for radar when implemented.
		if( s_buildTree.GetUnitType(sensor->id).IsRadar() )
		{
			if(IsBuildingSelectable(*sensor, water, mustBeConstructable))
			{
				const float myRating =   cost * costs.GetNormalizedDeviationFromMax(s_buildTree.GetTotalCost(sensor->id))
				                       + range * ranges.GetNormalizedDeviationFromMin(s_buildTree.GetMaxRange(sensor->id));

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
	side -= 1;

	for(auto i = s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SUPPORT, side+1).begin(); i != s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_SUPPORT, side+1).end(); ++i)
	{
		//! @todo Check unit type for jammer
		/*if(canBuild && units_dynamic[*i].constructorsAvailable <= 0)
			my_rating = -10000;
		else if(water && GetUnitDef(*i).minWaterDepth > 0)
			my_rating = cost * (avg_cost[STATIONARY_JAMMER][side] - s_buildTree.GetTotalCost(UnitDefId(*i)))/max_cost[STATIONARY_JAMMER][side]
						+ range * (GetUnitDef(*i).jammerRadius - avg_value[STATIONARY_JAMMER][side])/max_value[STATIONARY_JAMMER][side];
		else if (!water &&  GetUnitDef(*i).minWaterDepth <= 0)
			my_rating = cost * (avg_cost[STATIONARY_JAMMER][side] - s_buildTree.GetTotalCost(UnitDefId(*i)))/max_cost[STATIONARY_JAMMER][side]
						+ range * (GetUnitDef(*i).jammerRadius - avg_value[STATIONARY_JAMMER][side])/max_value[STATIONARY_JAMMER][side];
		else
			my_rating = -10000;


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

	const StatisticalData& costs       = s_buildTree.GetUnitStatistics(side).GetUnitCostStatistics(EUnitCategory::SCOUT);
	const StatisticalData& sightRanges = s_buildTree.GetUnitStatistics(side).GetUnitPrimaryAbilityStatistics(EUnitCategory::SCOUT);

	for(auto scout = s_buildTree.GetUnitsInCategory(EUnitCategory::SCOUT, side).begin(); scout != s_buildTree.GetUnitsInCategory(EUnitCategory::SCOUT, side).end(); ++scout)
	{
		bool movementTypeAllowed     = s_buildTree.GetMovementType(scout->id).isIncludedIn(movementType);
		bool factoryPrerequisitesMet = !factoryAvailable || (units_dynamic[scout->id].constructorsAvailable > 0);

		if( movementTypeAllowed && factoryPrerequisitesMet )
		{
			float myRating =     sightRange * sightRanges.GetNormalizedDeviationFromMin(s_buildTree.GetMaxRange(*scout))
							   +       cost * costs.GetNormalizedDeviationFromMax( s_buildTree.GetTotalCost(*scout) );

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

int AAIBuildTable::GetRandomUnit(list<int> unit_list)
{
	float best_rating = 0, my_rating;

	int best_unit = 0;

	for(list<int>::iterator i = unit_list.begin(); i != unit_list.end(); ++i)
	{
		my_rating = rand()%512;

		if(my_rating >best_rating)
		{
			if(GetUnitDef(*i).metalCost < cfg->MAX_METAL_COST)
			{
				best_unit = *i;
				best_rating = my_rating;
			}
		}
	}
	return best_unit;
}

void AAIBuildTable::CalculateCombatPowerForUnits(const std::list<int>& unitList, const AAICombatCategory& category, const CombatPower& combatCriteria, std::vector<float>& combatPowerValues, StatisticalData& combatPowerStat, StatisticalData& combatEfficiencyStat)
{
	int i = 0;
	for(std::list<int>::const_iterator id = unitList.begin(); id != unitList.end(); ++id)
	{
		const UnitTypeStatic *unit = &units_static[*id];
		const UnitTypeProperties& unitData = s_buildTree.GetUnitTypeProperties(UnitDefId(*id));

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
	const std::list<int> unitList = s_buildTree.GetUnitsInCombatCategory(category, side);

	const StatisticalData& costStatistics  = s_buildTree.GetUnitStatistics(side).GetCombatCostStatistics(category);
	const StatisticalData& rangeStatistics = s_buildTree.GetUnitStatistics(side).GetCombatRangeStatistics(category);
	const StatisticalData& speedStatistics = s_buildTree.GetUnitStatistics(side).GetCombatSpeedStatistics(category);

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
			const UnitTypeProperties& unitData = s_buildTree.GetUnitTypeProperties(UnitDefId(*id));

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
		if(s_buildTree.GetUnitCategory(UnitDefId(def_killed->id)).isStaticDefence() == true)
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
	UnitCategory killer, killed;

	for(int side = 0; side < numOfSides; ++side)
	{
		for(int i = 0; i < combat_categories; ++i)
		{
			for(int j = 0; j < combat_categories; ++j)
			{
				killer = GetAssaultCategoryOfID(i);
				killed = GetAssaultCategoryOfID(j);
				counter = 0;

				// update max and avg efficiency of i versus j
				max = 0;
				min = 100000;
				sum = 0;

				for(list<int>::iterator unit = units_of_category[killer][side].begin(); unit != units_of_category[killer][side].end(); ++unit)
				{
					// only count anti air units vs air and assault units vs non air
					if( (killed == AIR_ASSAULT && units_static[*unit].unit_type == ANTI_AIR_UNIT) || (killed != AIR_ASSAULT && units_static[*unit].unit_type != ANTI_AIR_UNIT))
					{
						sum += units_static[*unit].efficiency[j];

						if(units_static[*unit].efficiency[j] > max)
							max = units_static[*unit].efficiency[j];

						if(units_static[*unit].efficiency[j] < min)
							min = units_static[*unit].efficiency[j];

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
	return cfg->GetFileName(ai->Getcb(), cfg->getUniqueName(ai->Getcb(), true, true, false, false), MOD_LEARN_PATH, "_buildcache.txt", true);
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
				// to be deleted when unit static fully moved to AAIBuildtree
				fscanf(load_file, "%u ", &units_static[i].unit_type);

				// get memory for eff
				units_static[i].efficiency.resize(combat_categories);

				// load eff
				for(int k = 0; k < combat_categories; ++k)
				{
					fscanf(load_file, "%f ", &units_static[i].efficiency[k]);
					fixed_eff[i][k] = units_static[i].efficiency[k];
				}
			}

			// now load unit lists
			for(int s = 0; s < numOfSides; ++s)
			{
				for(int cat = 0; cat <= MOBILE_CONSTRUCTOR; ++cat)
				{
					// load number of buildoptions
					fscanf(load_file,  _STPF_ " ", &bo);

					for(size_t i = 0; i < bo; ++i)
					{
						fscanf(load_file, "%i ", &tmp);
						units_of_category[cat][s].push_back(tmp);
					}

					// load pre cached values
					fscanf(load_file, "%f %f %f %f %f %f %f %f %f \n",
						&max_cost[cat][s], &min_cost[cat][s], &avg_cost[cat][s],
						&max_buildtime[cat][s], &min_buildtime[cat][s], &avg_buildtime[cat][s],
						&max_value[cat][s], &min_value[cat][s], &avg_value[cat][s]);
				}
			}

			fclose(load_file);
			return true;
		}
	}



	return false;
}

void AAIBuildTable::SaveBuildTable(int game_period, MapType map_type)
{
	// reset factory ratings
	for(int s = 0; s < cfg->SIDES; ++s)
	{
		for(auto fac = s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, s+1).begin(); fac != s_buildTree.GetUnitsInCategory(EUnitCategory::STATIC_CONSTRUCTOR, s+1).end(); ++fac)
		{
			units_static[fac->id].efficiency[5] = -1;
			units_static[fac->id].efficiency[4] = 0;
		}
	}
	// reset builder ratings
	for(int s = 0; s < cfg->SIDES; ++s)
	{
		for(auto builder = s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, s+1).begin(); builder != s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, s+1).end(); ++builder)
			units_static[builder->id].efficiency[5] = -1;
	}

	const std::string filename = GetBuildCacheFileName();
	FILE *save_file = fopen(filename.c_str(), "w+");

	// file version
	fprintf(save_file, "%s \n", MOD_LEARN_VERSION);

	// update attacked_by values
	// FIXME: using t two times as the for-loop-var?
	for(int t = 0; t < 4; ++t)
	{
		for(int cat = 0; cat < combat_categories; ++cat)
		{
			for(int t = 0; t < game_period; ++t)
			{
				attacked_by_category_learned[map_type][t][cat] =
						0.75f * attacked_by_category_learned[map_type][t][cat] +
						0.25f * attacked_by_category_current[t][cat];
			}
		}
	}

	// save attacked_by table
	for(int map = 0; map <= WATER_MAP; ++map)
	{
		for(int t = 0; t < 4; ++t)
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
		fprintf(save_file, "%u ", units_static[i].unit_type);

		// save combat eff
		for(int k = 0; k < combat_categories; ++k)
			fprintf(save_file, "%f ", units_static[i].efficiency[k]);

		fprintf(save_file, "\n");
	}

	for(int s = 0; s < numOfSides; ++s)
	{
		// now save unit lists
		for(int cat = 0; cat <= MOBILE_CONSTRUCTOR; ++cat)
		{
			// save number of units
			fprintf(save_file,  _STPF_ " ", units_of_category[cat][s].size());

			for(list<int>::iterator unit = units_of_category[cat][s].begin(); unit != units_of_category[cat][s].end(); ++unit)
				fprintf(save_file, "%i ", *unit);

			fprintf(save_file, "\n");

			// save pre cached values
			fprintf(save_file, "%f %f %f %f %f %f %f %f %f \n",
						max_cost[cat][s], min_cost[cat][s], avg_cost[cat][s],
						max_buildtime[cat][s], min_buildtime[cat][s], avg_buildtime[cat][s],
						max_value[cat][s], min_value[cat][s], avg_value[cat][s]);

			fprintf(save_file, "\n");
		}
	}

	fclose(save_file);
}

float AAIBuildTable::GetMaxDamage(int unit_id)
{
	float max_damage = 0;

	int armor_types;
	ai->Getcb()->GetValue(AIVAL_NUMDAMAGETYPES,&armor_types);

	for(vector<UnitDef::UnitDefWeapon>::const_iterator i = GetUnitDef(unit_id).weapons.begin(); i != GetUnitDef(unit_id).weapons.end(); ++i)
	{
		for(int k = 0; k < armor_types; ++k)
		{
			if((*i).def->damages[k] > max_damage)
				max_damage = (*i).def->damages[k];
		}
	}

	return max_damage;
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

	for(std::list<UnitDefId>::const_iterator factory = s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).begin();  factory != s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).end(); ++factory)
	{
		if(s_buildTree.GetTotalCost(*factory) > max_cost)
			max_cost = s_buildTree.GetTotalCost(*factory);

		if(GetUnitDef((*factory).id).buildTime > max_buildtime)
			max_buildtime = GetUnitDef((*factory).id).buildTime;

		if(GetUnitDef((*factory).id).buildSpeed > max_buildspeed)
			max_buildspeed = GetUnitDef((*factory).id).buildSpeed;
	}

	// look for best builder to do the job
	for(std::list<UnitDefId>::const_iterator factory = s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).begin();  factory != s_buildTree.GetConstructedByList(UnitDefId(unit_def_id)).end(); ++factory)
	{
		if(units_dynamic[(*factory).id].active + units_dynamic[(*factory).id].requested + units_dynamic[(*factory).id].under_construction < cfg->MAX_FACTORIES_PER_TYPE)
		{
			my_rating = buildspeed * (GetUnitDef((*factory).id).buildSpeed / max_buildspeed)
				- (GetUnitDef((*factory).id).buildTime / max_buildtime)
				- cost * (s_buildTree.GetTotalCost(*factory) / max_cost);

			// prefer builders that can be built atm
			if(units_dynamic[(*factory).id].constructorsAvailable > 0)
				my_rating += 2.0f;

			// prevent AAI from requesting factories that cannot be built within the current base
			if(s_buildTree.GetMovementType(*factory).isStaticLand() == true)
			{
				if(ai->Getbrain()->baseLandRatio > 0.1f)
					my_rating *= ai->Getbrain()->baseLandRatio;
				else
					my_rating = -100000.0f;
			}
			else if(s_buildTree.GetMovementType(*factory).isStaticSea() == true)
			{
				if(ai->Getbrain()->baseWaterRatio > 0.1f)
					my_rating *= ai->Getbrain()->baseWaterRatio;
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
		if(s_buildTree.GetMovementType(UnitDefId(constructor)).isStatic() == true)
		{
			if(units_dynamic[constructor].constructorsAvailable + units_dynamic[constructor].constructorsRequested <= 0)
			{
				ai->Log("BuildFactoryFor(%s) is requesting builder for %s\n", s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
				BuildBuilderFor(UnitDefId(constructor));
			}

			// debug
			ai->Log("BuildFactoryFor(%s) requested %s\n", s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
		}
		// mobile constructor requested
		else
		{
			// only mark as urgent (unit gets added to front of buildqueue) if no constructor of that type already exists
			bool urgent = (units_dynamic[constructor].active > 0) ? false : true;

			if(ai->Getexecute()->AddUnitToBuildqueue(UnitDefId(constructor), 1, urgent))
			{
				// increase counter if mobile factory is a builder as well
				if(units_static[constructor].unit_type & UNIT_TYPE_BUILDER)
					ai->Getut()->futureBuilders += 1;

				if(units_dynamic[constructor].constructorsAvailable + units_dynamic[constructor].constructorsRequested <= 0)
				{
					ai->Log("BuildFactoryFor(%s) is requesting factory for %s\n", s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
					BuildFactoryFor(constructor);
				}

				// debug
				ai->Log("BuildFactoryFor(%s) requested %s\n", s_buildTree.GetUnitTypeProperties(UnitDefId(unit_def_id)).m_name.c_str(), s_buildTree.GetUnitTypeProperties(UnitDefId(constructor)).m_name.c_str());
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

	for(std::list<UnitDefId>::const_iterator builder = s_buildTree.GetConstructedByList(building).begin();  builder != s_buildTree.GetConstructedByList(building).end(); ++builder)
	{
		costStatistics.AddValue( s_buildTree.GetTotalCost(*builder) );
		buildtimeStatistics.AddValue( s_buildTree.GetBuildtime(*builder) );
		buildpowerStatistics.AddValue( s_buildTree.GetBuildspeed(*builder) );
	}

	costStatistics.Finalize();
	buildtimeStatistics.Finalize();
	buildpowerStatistics.Finalize();

	float bestRating = 0.0f;
	UnitDefId selectedBuilder;


	// look for best builder to do the job
	for(std::list<UnitDefId>::const_iterator builder = s_buildTree.GetConstructedByList(building).begin();  builder != s_buildTree.GetConstructedByList(building).end(); ++builder)
	{
		// prevent ai from ordering too many builders of the same type/commanders/builders that cant be built atm
		if(units_dynamic[(*builder).id].active + units_dynamic[(*builder).id].under_construction + units_dynamic[(*builder).id].requested < cfg->MAX_BUILDERS_PER_TYPE)
		{
			float myRating = cost       * costStatistics.GetNormalizedDeviationFromMax( s_buildTree.GetTotalCost(*builder) )
			               + buildtime  * buildtimeStatistics.GetNormalizedDeviationFromMax( s_buildTree.GetBuildtime(*builder) )
				           + buildpower * buildpowerStatistics.GetNormalizedDeviationFromMin( s_buildTree.GetBuildspeed(*builder) );

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
			ai->Log("BuildBuilderFor(%s) is requesting factory for %s\n", s_buildTree.GetUnitTypeProperties(building).m_name.c_str(), s_buildTree.GetUnitTypeProperties(selectedBuilder).m_name.c_str());

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
			ai->Log("BuildBuilderFor(%s) requested %s\n", s_buildTree.GetUnitTypeProperties(building).m_name.c_str(), s_buildTree.GetUnitTypeProperties(selectedBuilder).m_name.c_str());
		}
	}
}


void AAIBuildTable::AddAssistant(uint32_t allowedMovementTypes, bool canBuild)
{
	int builder = 0;
	float best_rating = -10000, my_rating;

	int side = ai->GetSide();

	float cost = 1.0f;
	float buildspeed = 2.0f;
	float urgency = 1.0f;

	for(auto unit = s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, side).begin();  unit != s_buildTree.GetUnitsInCategory(EUnitCategory::MOBILE_CONSTRUCTOR, side).end(); ++unit)
	{
		if(s_buildTree.GetMovementType(UnitDefId(*unit)).isIncludedIn(allowedMovementTypes) == true)
		{
			if( (!canBuild || units_dynamic[unit->id].constructorsAvailable > 0)
				&& units_dynamic[unit->id].active + units_dynamic[unit->id].under_construction + units_dynamic[unit->id].requested < cfg->MAX_BUILDERS_PER_TYPE)
			{
				if( GetUnitDef(unit->id).buildSpeed >= (float)cfg->MIN_ASSISTANCE_BUILDTIME && GetUnitDef(unit->id).canAssist)
				{
					my_rating = cost * (s_buildTree.GetTotalCost(UnitDefId(unit->id)) / max_cost[MOBILE_CONSTRUCTOR][ai->GetSide()-1])
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
}


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

bool AAIBuildTable::IsScout(int id)
{
	if(GetUnitDef(id).speed > cfg->SCOUT_SPEED && !GetUnitDef(id).canfly)
		return true;
	else
	{
		for(list<int>::iterator i = cfg->SCOUTS.begin(); i != cfg->SCOUTS.end(); ++i)
		{
			if(*i == id)
				return true;
		}
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

bool AAIBuildTable::IsCommander(int def_id)
{
	if(units_static[def_id].unit_type & UNIT_TYPE_COMMANDER)
		return true;
	else
		return false;
}

float AAIBuildTable::GetEfficiencyAgainst(int unit_def_id, UnitCategory category)
{
	if(category == GROUND_ASSAULT)
		return units_static[unit_def_id].efficiency[0];
	else if(category == AIR_ASSAULT)
		return units_static[unit_def_id].efficiency[1];
	else if(category == HOVER_ASSAULT)
		return units_static[unit_def_id].efficiency[2];
	else if(category == SEA_ASSAULT)
		return units_static[unit_def_id].efficiency[3];
	else if(category == SUBMARINE_ASSAULT)
		return units_static[unit_def_id].efficiency[4];
	else if(category >= STATIONARY_DEF && category <= METAL_MAKER)
		return units_static[unit_def_id].efficiency[5];
	else
		return 0;
}

const char* AAIBuildTable::GetCategoryString2(UnitCategory category)
{
	if(category == UNKNOWN)
		return "unknown";
	else if(category == GROUND_ASSAULT)
	{
		if(cfg->AIR_ONLY_MOD)
			return "light air assault";
		else
			return "ground assault";
	}
	else if(category == AIR_ASSAULT)
		return "air assault";
	else if(category == HOVER_ASSAULT)
	{
		if(cfg->AIR_ONLY_MOD)
			return "heavy air assault";
		else
			return "hover assault";
	}
	else if(category == SEA_ASSAULT)
	{
		if(cfg->AIR_ONLY_MOD)
			return "super heavy air assault";
		else
			return "sea assault";
	}
	else if(category == SUBMARINE_ASSAULT)
		return "submarine assault";
	else if(category == MOBILE_CONSTRUCTOR)
		return "builder";
	else if(category == SCOUT)
		return "scout";
	else if(category == MOBILE_TRANSPORT)
		return "transport";
	else if(category == GROUND_ARTY)
	{
		if(cfg->AIR_ONLY_MOD)
			return "mobile artillery";
		else
			return "ground artillery";
	}
	else if(category == SEA_ARTY)
		return "naval artillery";
	else if(category == HOVER_ARTY)
		return "hover artillery";
	else if(category == STATIONARY_DEF)
		return "defence building";
	else if(category == STATIONARY_ARTY)
		return "stationary arty";
	else if(category == EXTRACTOR)
		return "metal extractor";
	else if(category == POWER_PLANT)
		return "power plant";
	else if(category == STORAGE)
		return "storage";
	else if(category == METAL_MAKER)
		return "metal maker";
	else if(category == STATIONARY_CONSTRUCTOR)
		return "stationary constructor";
	else if(category == AIR_BASE)
		return "air base";
	else if(category == DEFLECTION_SHIELD)
		return "deflection shield";
	else if(category == STATIONARY_JAMMER)
		return "stationary jammer";
	else if(category == STATIONARY_RECON)
		return "stationary radar/sonar";
	else if(category == STATIONARY_LAUNCHER)
		return "stationary launcher";
	else if(category == MOBILE_JAMMER)
		return "mobile jammer";
	else if(category == MOBILE_LAUNCHER)
		return "mobile launcher";
	else if(category == COMMANDER)
		return "commander";

	return "unknown";
}

bool AAIBuildTable::IsBuilder(int def_id)
{
	if(units_static[def_id].unit_type & UNIT_TYPE_BUILDER)
		return true;
	else
		return false;
}

bool AAIBuildTable::IsFactory(int def_id)
{
	assert(def_id >= 0);
	assert(def_id < units_static.size());
	if(units_static[def_id].unit_type & UNIT_TYPE_FACTORY)
		return true;
	else
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

UnitCategory AAIBuildTable::GetAssaultCategoryOfID(int id)
{
	if(id == 0)
		return GROUND_ASSAULT;
	else if(id == 1)
		return AIR_ASSAULT;
	else if(id == 2)
		return HOVER_ASSAULT;
	else if(id == 3)
		return SEA_ASSAULT;
	else if(id == 4)
		return SUBMARINE_ASSAULT;
	else if(id == 5)
		return STATIONARY_DEF;
	else
		return UNKNOWN;
}
