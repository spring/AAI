// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "System/SafeUtil.h"
#include "AAIBuildTree.h"
#include "AAIConfig.h"

#include "LegacyCpp/IGlobalAICallback.h"

#include "LegacyCpp/UnitDef.h"
#include "LegacyCpp/MoveData.h"
#include "LegacyCpp/WeaponDef.h"

#include <string>

using namespace springLegacyAI;

AAIBuildTree::AAIBuildTree() :
    m_initialized(false),
    m_numberOfSides(0)
{
}

AAIBuildTree::~AAIBuildTree(void)
{
    m_initialized = false;
    m_unitTypeCanBeConstructedtByLists.clear();
    m_unitTypeCanConstructLists.clear();
    m_unitTypeProperties.clear();
    m_sideOfUnitType.clear();
    m_startUnitsOfSide.clear();
}

bool AAIBuildTree::generate(springLegacyAI::IAICallback* cb)
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

            m_unitTypeCanConstructLists[id].push_back(canConstructId);
            m_unitTypeCanBeConstructedtByLists[canConstructId].push_back(id);
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
        assignSideToUnitType(m_numberOfSides, UnitDefId(*id) );
        m_startUnitsOfSide[m_numberOfSides] = *id;
    }

    //-----------------------------------------------------------------------------------------------------------------
    // set further unit type properties
    //-----------------------------------------------------------------------------------------------------------------

    for(int id = 1; id <= numberOfUnitTypes; ++id)
    {
        m_unitTypeProperties[id].m_totalCost = unitDefs[id]->metalCost + (unitDefs[id]->energyCost / energyToMetalConversionFactor);
        m_unitTypeProperties[id].m_buildtime = unitDefs[id]->buildTime;
        m_unitTypeProperties[id].m_maxSpeed  = unitDefs[id]->speed;
        m_unitTypeProperties[id].m_name      = unitDefs[id]->humanName;
        
        m_unitTypeProperties[id].m_maxRange = 0.0f;
        for(std::vector<springLegacyAI::UnitDef::UnitDefWeapon>::const_iterator w = unitDefs[id]->weapons.begin(); w != unitDefs[id]->weapons.end(); ++w)
        {
            if((*w).def->range > m_unitTypeProperties[id].m_maxRange)
                m_unitTypeProperties[id].m_maxRange = (*w).def->range;
        }

        m_unitTypeProperties[id].m_movementType.setMovementType( determineMovementType(unitDefs[id]) );

		m_unitTypeProperties[id].m_unitCategory.setUnitCategory( determineUnitCategory(unitDefs[id]) );
    }
    
    //-----------------------------------------------------------------------------------------------------------------
    // print info to file for debug purposes
    //-----------------------------------------------------------------------------------------------------------------
    FILE* file = fopen("buildtree.txt", "w+");
    if(file != nullptr)
    {
        fprintf(file, "Number of unit types: %i\n", numberOfUnitTypes);

        fprintf(file, "Detected start units (aka commanders):\n");
        for(int side = 1; side <= m_numberOfSides; ++side)
        {
            fprintf(file, " %s\n", unitDefs[ m_startUnitsOfSide[side] ]->name.c_str());
        }

        fprintf(file, "\nUnit Side\n");
        for(int id = 1; id <= numberOfUnitTypes; ++id)
        {
            /*fprintf(file, "%s %s %i %f %u %i\n", m_unitTypeProperties[id].m_name.c_str(), 
                                                 unitDefs[id]->name.c_str(), 
                                                 m_sideOfUnitType[id], 
                                                 m_unitTypeProperties[id].m_maxRange,
                                                 static_cast<uint32_t>(m_unitTypeProperties[id].m_movementType.getMovementType()),
                                                 static_cast<int>(m_unitTypeProperties[id].m_movementType.cannotMoveToOtherContinents()) );*/
			fprintf(file, "%s %s %u\n", m_unitTypeProperties[id].m_name.c_str(), 
										unitDefs[id]->name.c_str(), 
										static_cast<uint32_t>(m_unitTypeProperties[id].m_unitCategory.getUnitCategory()));
        }
        fclose(file);
    }
    

    return true;
}

void AAIBuildTree::assignSideToUnitType(int side, UnitDefId unitDefId)
{
    // avoid "visiting" unit types multiple times (if units can be constructed by more than one other unit)
    if( m_sideOfUnitType[unitDefId.id] == 0)
    {
        // set side of given unit type
        m_sideOfUnitType[unitDefId.id] = side;

        // continue with unit types constructed by given unit type
        for( std::list<int>::iterator id = m_unitTypeCanConstructLists[unitDefId.id].begin(); id != m_unitTypeCanConstructLists[unitDefId.id].end(); ++id)
        {
            assignSideToUnitType(side, UnitDefId(*id) );
        }
    }
}

EMovementType AAIBuildTree::determineMovementType(const springLegacyAI::UnitDef* unitDef) const
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

EUnitCategory AAIBuildTree::determineUnitCategory(const springLegacyAI::UnitDef* unitDef) const
{
	if(m_sideOfUnitType[unitDef->id] == 0)
		return EUnitCategory::UNIT_CATEGORY_UNKNOWN;

	// --------------- buildings --------------------------------------------------------------------------------------
	if(m_unitTypeProperties[unitDef->id].m_movementType.isStatic() == true)
	{
		if(m_unitTypeCanConstructLists[unitDef->id].size() > 0)
		{
			return EUnitCategory::UNIT_CATEGORY_STATIC_CONSTRUCTOR;
		}
		else if(unitDef->extractsMetal > 0.0f)
		{
			return EUnitCategory::UNIT_CATEGORY_METAL_EXTRACTOR;
		}
		else if(unitDef->isAirBase == true)
		{
			return EUnitCategory::UNIT_CATEGORY_STATIC_SUPPORT;
		}
		else if(   (unitDef->energyMake > cfg->MIN_ENERGY)
				|| (unitDef->tidalGenerator > 0.0f)
				|| (unitDef->windGenerator  > 0.0f) 
				|| (unitDef->energyUpkeep < -cfg->MIN_ENERGY) )
		{
			if(unitDef->radarRadius == 0 && unitDef->sonarRadius == 0) // prevent radar/sonar who make some energy to be classified as power plant
			{
				return EUnitCategory::UNIT_CATEGORY_POWER_PLANT;
			}
		}
		// --------------- armed buildings --------------------------------------------------------------------------------
		else if( (unitDef->weapons.empty() == false) && (GetMaxDamage(unitDef) > 1) )
		{
			// filter out nuke silos, antinukes and stuff like that
			if(IsMissileLauncher(unitDef) == true)
			{
				return EUnitCategory::UNIT_CATEGORY_STATIC_SUPPORT;
			}
			else if(IsDeflectionShieldEmitter(unitDef) == true)
			{
				return EUnitCategory::UNIT_CATEGORY_STATIC_SUPPORT;
			}
			//else
			{
				if( getMaxRange( UnitDefId(unitDef->id) ) < cfg->STATIONARY_ARTY_RANGE)
				{
					return EUnitCategory::UNIT_CATEGORY_STATIC_DEFENCE;
				}
				else
				{
					return EUnitCategory::UNIT_CATEGORY_STATIC_ARTILLERY;
				}
			}
		}
		else if((unitDef->sonarJamRadius > 0) || (unitDef->sonarRadius > 0) || (unitDef->jammerRadius > 0) || (unitDef->radarRadius > 0))
		{
			return EUnitCategory::UNIT_CATEGORY_STATIC_SUPPORT;
		}
		else if(unitDef->metalMake > 0.0f) //! @todo Does not work - investigate later
		{
			return EUnitCategory::UNIT_CATEGORY_METAL_MAKER;
		}
		else if( (unitDef->metalStorage > static_cast<float>(cfg->MIN_METAL_STORAGE)) || (unitDef->energyStorage > static_cast<float>(cfg->MIN_ENERGY_STORAGE)) )
		{
			return EUnitCategory::UNIT_CATEGORY_STORAGE;
		}
	}
	// --------------- units ------------------------------------------------------------------------------------------
	else
	{
		if( isStartingUnit(unitDef->id) == true )
		{
			return EUnitCategory::UNIT_CATEGORY_COMMANDER;
		}
		else if(IsScout(unitDef) == true)
		{
			return EUnitCategory::UNIT_CATEGORY_SCOUT;
		}
		else if(IsMobileTransport(unitDef) == true)
		{
			return EUnitCategory::UNIT_CATEGORY_TRANSPORT;
		}

		// --------------- armed units --------------------------------------------------------------------------------
		if( (unitDef->weapons.empty() == false) && (GetMaxDamage(unitDef) > 1))
		{
			if(unitDef->weapons.begin()->def->stockpile)
			{
				return EUnitCategory::UNIT_CATEGORY_MOBILE_SUPPORT;
			}
			else
			{
				if(    (m_unitTypeProperties[unitDef->id].m_movementType.isGround()     == true) 
				    || (m_unitTypeProperties[unitDef->id].m_movementType.isAmphibious() == true) )
				{
					if( IsArtillery(unitDef, cfg->GROUND_ARTY_RANGE) == true)
						return EUnitCategory::UNIT_CATEGORY_MOBILE_ARTILLERY;
					else
						return EUnitCategory::UNIT_CATEGORY_GROUND_COMBAT;
				}
				else if(m_unitTypeProperties[unitDef->id].m_movementType.isHover() == true)
				{
					if( IsArtillery(unitDef, cfg->HOVER_ARTY_RANGE) == true)
						return EUnitCategory::UNIT_CATEGORY_MOBILE_ARTILLERY;
					else
						return EUnitCategory::UNIT_CATEGORY_HOVER_COMBAT;
				}
				else if(m_unitTypeProperties[unitDef->id].m_movementType.isAir() == true)
				{
					return EUnitCategory::UNIT_CATEGORY_AIR_COMBAT;
				}
				else if(m_unitTypeProperties[unitDef->id].m_movementType.isSeaUnit() == true)
				{
					//! @todo: Sea artillery is skipped on prupose - handling of sea artillery not implemented at the moment.
					return EUnitCategory::UNIT_CATEGORY_SEA_COMBAT;
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
				return EUnitCategory::UNIT_CATEGORY_MOBILE_CONSTRUCTOR;
			}
			else if( (unitDef->sonarJamRadius > 0) || (unitDef->sonarRadius > 0) || (unitDef->jammerRadius > 0) || (unitDef->radarRadius > 0) )
			{
				return EUnitCategory::UNIT_CATEGORY_MOBILE_SUPPORT;
			}
		}
	}
	
	return EUnitCategory::UNIT_CATEGORY_UNKNOWN;
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

	if(    (m_unitTypeProperties[unitDef->id].m_maxRange > artilleryRangeThreshold)
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

bool AAIBuildTree::canBuildUnitType(UnitDefId unitDefIdBuilder, UnitDefId unitDefId) const
{
    // look in build options of builder for unit type
    for(std::list<int>::const_iterator id = m_unitTypeCanConstructLists[unitDefIdBuilder.id].begin(); id != m_unitTypeCanConstructLists[unitDefIdBuilder.id].end(); ++id)
    {
        if(*id == unitDefId.id)
            return true;
    }

    // unit type not found in build options
    return false;
}

bool AAIBuildTree::isStartingUnit(UnitDefId unitDefId) const
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