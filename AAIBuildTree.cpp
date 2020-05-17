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
    m_sideOfUnitType.clear();
}

bool AAIBuildTree::generate(IAICallback* cb)
{
    // prevent buildtree from beeing initialized several times
    if(m_initialized )
        return false;

    //-----------------------------------------------------------------------------------------------------------------
    // get number of unit types and set up arrays
    //-----------------------------------------------------------------------------------------------------------------
    const int numberOfUnitTypes = cb->GetNumUnitDefs();

    m_unitTypeCanBeConstructedtByLists.resize(numberOfUnitTypes);
    m_unitTypeCanConstructLists.resize(numberOfUnitTypes);
    m_sideOfUnitType.resize(numberOfUnitTypes, 0);

    //-----------------------------------------------------------------------------------------------------------------
    // get list all of unit definitions for further analysis
    //-----------------------------------------------------------------------------------------------------------------

    //spring first unitdef id is 1, we remap it so id = is position in array
	std::vector<const UnitDef*> unitDefs(numberOfUnitTypes+1);

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

    for(std::list<int>::iterator id = rootUnits.begin(); id != rootUnits.end(); ++id)
    {
        ++m_numberOfSides;
        assignSideToUnitType(m_numberOfSides, *id);
    }
    
    //-----------------------------------------------------------------------------------------------------------------
    // print info to file for debug purposes
    //-----------------------------------------------------------------------------------------------------------------
    FILE* file = fopen("buildtree.txt", "w+");
    if(file != nullptr)
    {
        fprintf(file, "Number of unit types: %i\n", numberOfUnitTypes);

        fprintf(file, "Detected start units (aka commanders):\n");
        for(std::list<int>::iterator id = rootUnits.begin(); id != rootUnits.end(); ++id)
        {
            fprintf(file, " %s\n", unitDefs[*id]->name.c_str());
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

void AAIBuildTree::assignSideToUnitType(int side, int unitDefId)
{
    // avoid "visiting" unit types multiple times (if units can be constructed by more than one other unit)
    if( m_sideOfUnitType[unitDefId] == 0)
    {
        // set side of given unit type
        m_sideOfUnitType[unitDefId] = side;

        // continue with unit types constructed by given unit type
        for( std::list<int>::iterator id = m_unitTypeCanConstructLists[unitDefId].begin(); id != m_unitTypeCanConstructLists[unitDefId].end(); ++id)
        {
            assignSideToUnitType(side, *id);
        }
    }
}

bool AAIBuildTree::canBuildUnitType(int unitDefIdBuilder, int unitDefId)
{
	// look in build options of builder for unit type
	for(std::list<int>::iterator id = m_unitTypeCanConstructLists[unitDefIdBuilder].begin(); id != m_unitTypeCanConstructLists[unitDefIdBuilder].end(); ++id)
	{
		if(*id == unitDefId)
			return true;
	}

	// unit type not found buildoptions
	return false;
}
