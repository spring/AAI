// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_BUILDTREE_H
#define AAI_BUILDTREE_H


namespace springLegacyAI {
	struct UnitDef;
}
using namespace springLegacyAI;

#include "aidef.h"
#include "LegacyCpp/IAICallback.h"
#include <list>
#include <vector>

//! @brief This class stores the build-tree, this includes which unit builds another, to which side each uhnit belongs
class AAIBuildTree
{
public:
	AAIBuildTree();

	~AAIBuildTree(void);

    //! @brief Generates buildtree for current game/mod
    bool generate(IAICallback* cb);

    //! @brief Returns whether given the given unit type can be constructed by the given constructor unit type
    bool canBuildUnitType(int unitDefIdBuilder, int unitDefId);

private:
    //! @brief Sets side for given unit type, and recursively calls itself for all unit types that can be constructed by it.
    void assignSideToUnitType(int side, int unitDefId);

    //! Flag if build tree is initialized
    bool                            m_initialized;

    //! For every unit type, a list of unit types (unit type id) that may contsruct it 
    std::vector< std::list<int> >   m_unitTypeCanBeConstructedtByLists;

    //! For every unit type, a list of unit types (unit type id) that it may contsruct (e.g. empty if it cannot construct any units) 
    std::vector< std::list<int> >   m_unitTypeCanConstructLists;

    //! For every unit type, the side/faction it belongs to
    std::vector< int >              m_sideOfUnitType;

    //! The number of sides (i.e. groups of units with disjunct buildtree)
    int                             m_numberOfSides;
};

#endif