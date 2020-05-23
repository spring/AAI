// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_TYPES_H
#define AAI_TYPES_H

typedef unsigned int   uint32_t;

//! Movement types that are used to describe the movement type of every unit
enum class EMovementType : uint32_t
{
    MOVEMENT_TYPE_UNKNOWN              = 0x0000u, //! Unknown move type, i.e. not set
    MOVEMENT_TYPE_GROUND               = 0x0001u, //! can move on land only
    MOVEMENT_TYPE_AMPHIBIOUS           = 0x0002u, //! can move on land and underwater
    MOVEMENT_TYPE_HOVER                = 0x0004u, //! can move on land and above water
    MOVEMENT_TYPE_SEA_FLOATER          = 0x0010u, //! can move above water (e.g. ships)
    MOVEMENT_TYPE_SEA_SUBMERGED        = 0x0020u, //! can move below water (e.g. submarines)
    MOVEMENT_TYPE_AIR                  = 0x0040u, //! can fly
    MOVEMENT_TYPE_STATIC_LAND          = 0x0080u, //! building on solid ground
    MOVEMENT_TYPE_STATIC_SEA_FLOATER   = 0x0100u, //! building floating on water
    MOVEMENT_TYPE_STATIC_SEA_SUBMERGED = 0x0200u  //! building on sea floor
};

//! @brief A bitmask describing the movement type of a unit type with several helper functions
class AAIMovementType
{
public:
    AAIMovementType() : m_movementType(EMovementType::MOVEMENT_TYPE_UNKNOWN) {};

    //! @brief Sets the given elementary movement type to the movement type bitmask
    void setMovementType(EMovementType moveType) { m_movementType = moveType; };

private:
    //! Movement type
    EMovementType m_movementType;
};

#endif