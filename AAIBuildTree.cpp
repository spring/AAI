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

#include "LegacyCpp/IGlobalAICallback.h"

#include "LegacyCpp/UnitDef.h"
#include "LegacyCpp/MoveData.h"
#include "LegacyCpp/WeaponDef.h"

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
        m_unitTypeProperties[id].totalCost = unitDefs[id]->metalCost + (unitDefs[id]->energyCost / energyToMetalConversionFactor);
        
        m_unitTypeProperties[id].maxRange = 0.0f;
        for(std::vector<springLegacyAI::UnitDef::UnitDefWeapon>::const_iterator w = unitDefs[id]->weapons.begin(); w != unitDefs[id]->weapons.end(); ++w)
        {
            if((*w).def->range > m_unitTypeProperties[id].maxRange)
                m_unitTypeProperties[id].maxRange = (*w).def->range;
        }
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
            fprintf(file, "%s %s %i\n", unitDefs[id]->humanName.c_str(), unitDefs[id]->name.c_str(), m_sideOfUnitType[id]);
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