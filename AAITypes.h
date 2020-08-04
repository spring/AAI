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

#include <vector>
#include <string>
#include "AAIUnitTypes.h"

//! @brief An id identifying a unit type - used to prevent mixing ids referring to units and unit definitions
class UnitDefId
{
public:
	UnitDefId() : id(0) { }

	UnitDefId(int unitDefId) : id(unitDefId) { }

	bool operator==(const UnitDefId& rhs) const { return (id == rhs.id); }

	bool isValid() const { return (id > 0) ? true : false; }

	void invalidate() { id = 0; }

	int id;
};

//! Movement types that are used to describe the movement type of every unit
enum class EMovementType : uint32_t
{
	MOVEMENT_TYPE_UNKNOWN              = 0x0000u, //! Unknown move type, i.e. not set
	MOVEMENT_TYPE_GROUND               = 0x0001u, //! can move on land only
	MOVEMENT_TYPE_AMPHIBIOUS           = 0x0002u, //! can move on land and underwater
	MOVEMENT_TYPE_HOVER                = 0x0004u, //! can move on land and above water
	MOVEMENT_TYPE_SEA_FLOATER          = 0x0008u, //! can move above water (e.g. ships)
	MOVEMENT_TYPE_SEA_SUBMERGED        = 0x0010u, //! can move below water (e.g. submarines)
	MOVEMENT_TYPE_AIR                  = 0x0020u, //! can fly
	MOVEMENT_TYPE_STATIC_LAND          = 0x0040u, //! building on solid ground
	MOVEMENT_TYPE_STATIC_SEA_FLOATER   = 0x0080u, //! building floating on water
	MOVEMENT_TYPE_STATIC_SEA_SUBMERGED = 0x0100u  //! building on sea floor
};

//! @brief A bitmask describing the movement type of a unit type with several helper functions
class AAIMovementType
{
public:
	AAIMovementType() : m_movementType(EMovementType::MOVEMENT_TYPE_UNKNOWN) {}

	//! @brief Sets the given elementary movement type to the movement type bitmask
	void setMovementType(EMovementType moveType) { m_movementType = moveType; }

	//! @brief Getter function to access unit type.
	EMovementType getMovementType() const { return m_movementType; }

	//! @brief Returns whether unit movement is limited to its continent (e.g. ground or sea units vs.
	//!        amphibious, hover, or air units - see AAIMap for more info on continents)
	bool cannotMoveToOtherContinents() const
	{
		const uint32_t continentBitMask =  static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_GROUND) 
											+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_SEA_FLOATER)
											+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_SEA_SUBMERGED);

		return static_cast<bool>( static_cast<uint32_t>(m_movementType) & continentBitMask );
	}

	//! @brief Returns whether unit type is capable to move on land tiles (ground, amphibious or hover)
	bool canMoveOnLand() const 
	{
		const uint32_t canMoveLandBitmask =  static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_GROUND) 
											+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_AMPHIBIOUS)
											+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_HOVER);

		return static_cast<bool>( static_cast<uint32_t>(m_movementType) & canMoveLandBitmask );
	};

	//! @brief Returns whether unit type is capable to move on sea tiles (floaters, submerged, amphibious or hover)
	bool canMoveOnSea() const 
	{
		const uint32_t canMoveSeaBitmask =   static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_SEA_FLOATER)
											+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_SEA_SUBMERGED)
											+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_AMPHIBIOUS)
											+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_HOVER);

		return static_cast<bool>( static_cast<uint32_t>(m_movementType) & canMoveSeaBitmask );
	}

	//! @brief Returns whether unit type is static (i.e. a building)
	bool isStatic() const 
	{
		const uint32_t staticBitmask =    static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_STATIC_LAND)
										+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_STATIC_SEA_FLOATER)
										+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_STATIC_SEA_SUBMERGED);

		return static_cast<bool>( static_cast<uint32_t>(m_movementType) & staticBitmask );
	}

	//! @brief Returns whether unit type is static on ground (i.e. a land based building)
	bool isStaticLand() const { return (m_movementType == EMovementType::MOVEMENT_TYPE_STATIC_LAND); };

	//! @brief Returns whether unit type is static on sea (i.e. a floating or submerged building)
	bool isStaticSea() const 
	{ 
		const uint32_t staticSeaBitmask =     static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_STATIC_SEA_FLOATER)
											+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_STATIC_SEA_SUBMERGED);
		return static_cast<bool>( static_cast<uint32_t>(m_movementType) & staticSeaBitmask ); 
	}

	bool isGround() const { return (m_movementType == EMovementType::MOVEMENT_TYPE_GROUND); }

	bool isHover() const { return (m_movementType == EMovementType::MOVEMENT_TYPE_HOVER); }

	bool isAir() const { return (m_movementType == EMovementType::MOVEMENT_TYPE_AIR); }

	bool isAmphibious() const { return (m_movementType == EMovementType::MOVEMENT_TYPE_AMPHIBIOUS); }

	bool isShip() const { return (m_movementType == EMovementType::MOVEMENT_TYPE_SEA_FLOATER); }

	bool isSubmarine() const { return (m_movementType == EMovementType::MOVEMENT_TYPE_SEA_SUBMERGED); }

	//! @brief Returns whether unit type can only move on sea (i.e. a floating or submerged unit)
	bool isSeaUnit() const 
	{ 
		const uint32_t seaUnitBitmask =   static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_SEA_FLOATER)
										+ static_cast<uint32_t>(EMovementType::MOVEMENT_TYPE_SEA_SUBMERGED);
		return static_cast<bool>( static_cast<uint32_t>(m_movementType) & seaUnitBitmask ); 
	}

	//! @brief Returns whether this movement type is included in the given movement type bitmask.
	bool isIncludedIn(uint32_t moveTypesBitmask) const { return static_cast<bool>( static_cast<uint32_t>(m_movementType) & moveTypesBitmask); }

private:
	//! Movement type
	EMovementType m_movementType;
};

//! Unit Type properties needed by AAI for internal decision making (i.e. unit type selection)
struct UnitTypeProperties
{
	//! Name of the unit
	std::string m_name;

	//! Cost of unit (metal + energy / conversion_factor)
	float m_totalCost;

	//! Buildtime
	float m_buildtime;

	//! Range of unit category relevant ability: 
	//! max range of weapons (Combat units, artillery and static defences), line of sight (scouts), radar/sonar/jammer range
	//! buildspeed for mobile/static constructors
	float m_range;

	//! Movement type (land, sea, air, hover, submarine, ...)
	AAIMovementType m_movementType;

	//! Maximum movement speed
	float m_maxSpeed;

	//! The category of the unit
	AAIUnitCategory m_unitCategory;

	//! The type of the unit (may further specifiy the purpose of a unit, e.g. anti ground vs anti air for combat units)
	AAIUnitType     m_unitType;

	//! The target type - ground&hover=surface, air=air, ... 
	AAITargetType   m_targetType;
};

//! Enum for the different types of maps
enum class EMapType : int
{
	LAND_MAP              = 0, //!< Map primarily/only consists of land
	LAND_WATER_MAP        = 1, //!< Mixed land & water map
    WATER_MAP             = 2, //!< Pure water map (may contain small islands)
	NUMBER_OF_MAP_TYPES   = 3,
	UNKNOWN_MAP           = 4
} ;

//! @brief Map type (allows distinction of of behaviour based on map type) + helper functions
class AAIMapType
{
public:
	AAIMapType(EMapType mapType) : m_mapType(mapType) {}

	AAIMapType() : AAIMapType(EMapType::UNKNOWN_MAP) {}

	bool IsLandMap() const { return m_mapType == EMapType::LAND_MAP; }

	bool IsLandWaterMap() const { return m_mapType == EMapType::LAND_WATER_MAP; }

	bool IsWaterMap() const { return m_mapType == EMapType::WATER_MAP; }

	int GetArrayIndex() const { return static_cast<int>(m_mapType); }

	static const int numberOfMapTypes = static_cast<int>(EMapType::NUMBER_OF_MAP_TYPES);

	static const EMapType first = EMapType::LAND_MAP;

	void Next() { m_mapType = static_cast<EMapType>( static_cast<int>(m_mapType) + 1 ); }

	bool End() const { return (m_mapType == EMapType::NUMBER_OF_MAP_TYPES); }

	bool operator==(const AAIMapType& rhs) const { return (m_mapType == rhs.m_mapType); }

private:
	EMapType m_mapType;
};

#endif