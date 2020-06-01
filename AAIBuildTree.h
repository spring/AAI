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

#include "AAITypes.h"
#include "LegacyCpp/IAICallback.h"

#include <list>
#include <vector>

//! @todo Make this changeable via optinal mod config file
const float energyToMetalConversionFactor = 60.0f;

//! @brief Unit Type properties needed by AAI for internal decision making (i.e. unit type selection)
struct UnitTypeProperties
{
    //! Name of the unit
    std::string m_name;

    //! Cost of unit (metal + energy / conversion_factor)
    float m_totalCost;

    //! max weapon range (0.f for unarmed units)
    float m_maxRange;

    //! Movement type (land, sea, air, hover, submarine, ...)
    AAIMovementType m_movementType;

    //! Maximum movement speed
    float m_maxSpeed;

    //UnitCategory category;

    //unsigned int unitType;
};

//! @brief This class stores the build-tree, this includes which unit builds another, to which side each unit belongs
class AAIBuildTree
{
public:
    AAIBuildTree();

    ~AAIBuildTree(void);

    //! @brief Generates buildtree for current game/mod
    bool generate(springLegacyAI::IAICallback* cb);

    //! @brief Returns whether given the given unit type can be constructed by the given constructor unit type
    bool canBuildUnitType(UnitDefId unitDefIdBuilder, UnitDefId unitDefId) const;

    //! @brief Return side of given unit type (0 if not initialized)
    int getSideOfUnitType(UnitDefId unitDefId) const { return m_initialized ? m_sideOfUnitType[unitDefId.id] : 0; };

    //! @brief Returns the number of sides
    int getNumberOfSides() const { return m_numberOfSides; };

    //! @brief Returns whether a given unit type is a starting unit for one side
    bool isStartingUnit(UnitDefId unitDefId) const;

    //! @brief Returns start units (probably not needed anymore when refactoring AAIBuildTable is finished)
    UnitDefId getStartUnit(int side) const { return m_initialized ? UnitDefId(m_startUnitsOfSide[side]) : UnitDefId(0); };

    //! @brief Returns the unit type properties of the given unit type
    const UnitTypeProperties& getUnitTypeProperties(UnitDefId unitDefId) const { return m_unitTypeProperties[unitDefId.id]; };

    //! @brief Returns movement type of given unit type
    const AAIMovementType& getMovementType(UnitDefId unitDefId) const  { return m_unitTypeProperties[unitDefId.id].m_movementType; };

private:
    //! @brief Sets side for given unit type, and recursively calls itself for all unit types that can be constructed by it.
    void assignSideToUnitType(int side, UnitDefId unitDefId);

    //! @brief Returns movement type of given unit definition
    EMovementType determineMovementType(const springLegacyAI::UnitDef* unitDef) const;

    //! Flag if build tree is initialized
    bool                            m_initialized;

    //! For every unit type, a list of unit types (unit type id) that may contsruct it 
    std::vector< std::list<int> >   m_unitTypeCanBeConstructedtByLists;

    //! For every unit type, a list of unit types (unit type id) that it may contsruct (e.g. empty if it cannot construct any units) 
    std::vector< std::list<int> >   m_unitTypeCanConstructLists;

    //! Properties of every unit type needed by other parts of AAI for decision making
    std::vector< UnitTypeProperties > m_unitTypeProperties;

    //! For every unit type, the side/faction it belongs to
    std::vector< int >              m_sideOfUnitType;

    //! For every side, the start unit, i.e. root of the buildtree (commander for original TA like mods)
    std::vector< int >              m_startUnitsOfSide;

    //! The number of sides (i.e. groups of units with disjunct buildtree)
    int                             m_numberOfSides;
};

#endif