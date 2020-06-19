// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_UNIT_CATEGORY_H
#define AAI_UNIT_CATEGORY_H

#include <string>
#include "AAITypes.h"

typedef unsigned int   uint32_t;

//! Different categories that are used to group units with similar/same purpose
enum class EUnitCategory : uint32_t
{
	UNKNOWN              =  0u, //! Unknown unit category, i.e. not set
	STATIC_DEFENCE       =  1u, //! 
	STATIC_ARTILLERY     =  2u, //! 
	STORAGE              =  3u, //! 
	STATIC_CONSTRUCTOR   =  4u, //! factories
	STATIC_SUPPORT       =  5u, //! jammer, air base, missile launcher, shields
	STATIC_SENSOR        =  6u, //! radar, sonar, seismic
	POWER_PLANT          =  7u, //! 
	METAL_EXTRACTOR      =  8u, //!
	METAL_MAKER          =  9u, //!
	COMMANDER            = 10u, //!
	GROUND_COMBAT        = 11u, //! 
	AIR_COMBAT           = 12u, //!
	HOVER_COMBAT         = 13u, //!
	SEA_COMBAT           = 14u, //! Ships & submarines
	MOBILE_ARTILLERY     = 15u, //!
	SCOUT                = 16u, //!
	TRANSPORT            = 17u, //!
	MOBILE_CONSTRUCTOR   = 18u, //!
	MOBILE_SUPPORT       = 19u, //! mobile radar, jammer, anti-nukes
	NUMBER_OF_CATEGORIES = 20u
};

//! The unit category is a coarse classification used to differentiate between different types of units.
//! Statistical data (e.g. buildcost) is calculated for each category. Further differentiation 
//! (e.g. combat vs. anti air units) is given by AAIUnitType.
class AAIUnitCategory
{
public:
	AAIUnitCategory() : m_unitCategory(EUnitCategory::UNKNOWN) {};

	AAIUnitCategory(EUnitCategory unitCategory) : m_unitCategory(unitCategory) {};

    bool operator==(const AAIUnitCategory& rhs) const { return (m_unitCategory == rhs.getUnitCategory()); };

	void setUnitCategory(EUnitCategory unitCategory) { m_unitCategory = unitCategory; };

	EUnitCategory getUnitCategory() const { return m_unitCategory; };

	bool isValid()             const { return (m_unitCategory != EUnitCategory::UNKNOWN) ? true : false; };

	bool isStaticDefence()     const { return (m_unitCategory == EUnitCategory::STATIC_DEFENCE) ? true : false; };

	bool isStaticArtillery()   const { return (m_unitCategory == EUnitCategory::STATIC_ARTILLERY) ? true : false; };

	bool isStorage()           const { return (m_unitCategory == EUnitCategory::STORAGE) ? true : false; };

	bool isStaticConstructor() const { return (m_unitCategory == EUnitCategory::STATIC_CONSTRUCTOR) ? true : false; };

	bool isStaticSupport()     const { return (m_unitCategory == EUnitCategory::STATIC_SUPPORT) ? true : false; };

	bool isStaticSensor()      const { return (m_unitCategory == EUnitCategory::STATIC_SENSOR) ? true : false; };

	bool isPowerPlant()        const { return (m_unitCategory == EUnitCategory::POWER_PLANT) ? true : false; };

	bool isMetalExtractor()    const { return (m_unitCategory == EUnitCategory::METAL_EXTRACTOR) ? true : false; };

	bool isMetalMaker()        const { return (m_unitCategory == EUnitCategory::METAL_MAKER) ? true : false; };

	bool isCommander()         const { return (m_unitCategory == EUnitCategory::COMMANDER) ? true : false; };

	bool isGroundCombat()      const { return (m_unitCategory == EUnitCategory::GROUND_COMBAT) ? true : false; };

	bool isAirCombat()         const { return (m_unitCategory == EUnitCategory::AIR_COMBAT) ? true : false; };

	bool isHoverCombat()       const { return (m_unitCategory == EUnitCategory::HOVER_COMBAT) ? true : false; };

	bool isSeaCombat()         const { return (m_unitCategory == EUnitCategory::SEA_COMBAT) ? true : false; };

	bool isMobileArtillery()   const { return (m_unitCategory == EUnitCategory::MOBILE_ARTILLERY) ? true : false; };
	
	bool isScout()             const { return (m_unitCategory == EUnitCategory::SCOUT) ? true : false; };
	
	bool isTransport()         const { return (m_unitCategory == EUnitCategory::TRANSPORT) ? true : false; };
	
	bool isMobileConstructor() const { return (m_unitCategory == EUnitCategory::MOBILE_CONSTRUCTOR) ? true : false; };
	
	bool isMobileSupport()     const { return (m_unitCategory == EUnitCategory::MOBILE_SUPPORT) ? true : false; };

	bool isCombatUnit()        const { return      (static_cast<uint32_t>(m_unitCategory) >= static_cast<uint32_t>(EUnitCategory::GROUND_COMBAT) )
												&& (static_cast<uint32_t>(m_unitCategory) <= static_cast<uint32_t>(EUnitCategory::SEA_COMBAT) ); };

	//! Returns unit category as index (to access arrays)
	uint32_t GetArrayIndex() const {return static_cast<uint32_t>(m_unitCategory); };

	void Next() { m_unitCategory = static_cast<EUnitCategory>( static_cast<uint32_t>(m_unitCategory) + 1u ); };

	bool IsLast() const { return (m_unitCategory == EUnitCategory::NUMBER_OF_CATEGORIES); };

	static EUnitCategory GetFirst() { return EUnitCategory::UNKNOWN; };

	static uint32_t getNumberOfUnitCategories() { return static_cast<uint32_t>(EUnitCategory::NUMBER_OF_CATEGORIES); };

private:
	//! The unit category
	EUnitCategory m_unitCategory;
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

	//unsigned int unitType;
};

//! The combat category describes what kind of target class a unit belongs to
enum class ECombatUnitCategory : uint32_t
{
	COMBAT_CATEGORY_GROUND               = 0u, //! Units on ground (move type ground, amphibious, hover, land buildings)
	COMBAT_CATEGORY_AIR                  = 1u, //! Air units
	COMBAT_CATEGORY_FLOATER              = 2u, //! Units moving above water (ships, hover) or floating buildings
	COMBAT_CATEGORY_SUBMERGED            = 3u, //! Units moving below water (submarines) or submerged buildings
	COMBAT_CATEGORY_NUMBER_OF_CATEGORIES = 4u, //! The number of combat categories (unknown/invalid not used)
	COMBAT_CATEGORY_UNKNOWN              = 5u, //! This value will be treated as invalid
};

//! This class handles the target category which describes
class AAICombatCategory
{
public:
	AAICombatCategory(ECombatUnitCategory combatUnitCategory) : m_combatUnitCategory(combatUnitCategory) {};

	AAICombatCategory() : AAICombatCategory(ECombatUnitCategory::COMBAT_CATEGORY_UNKNOWN) {};

	void setCategory(ECombatUnitCategory category) { m_combatUnitCategory = category; };

	bool IsValid()     const { return (m_combatUnitCategory != ECombatUnitCategory::COMBAT_CATEGORY_UNKNOWN); };

	bool IsGround()    const { return (m_combatUnitCategory == ECombatUnitCategory::COMBAT_CATEGORY_GROUND); };

	bool IsAir()       const { return (m_combatUnitCategory == ECombatUnitCategory::COMBAT_CATEGORY_AIR); };

	bool IsFloater()   const { return (m_combatUnitCategory == ECombatUnitCategory::COMBAT_CATEGORY_FLOATER); };

	bool IsSubmerged() const { return (m_combatUnitCategory == ECombatUnitCategory::COMBAT_CATEGORY_SUBMERGED); };

	uint32_t GetCategoryIndex() const {return static_cast<uint32_t>(m_combatUnitCategory); };

	static uint32_t GetCategoryIndex(ECombatUnitCategory category) { return static_cast<uint32_t>(category); };

	static uint32_t GetNumberOfCombatCategories() { return static_cast<uint32_t>(ECombatUnitCategory::COMBAT_CATEGORY_NUMBER_OF_CATEGORIES); };

private:
	ECombatUnitCategory m_combatUnitCategory;
};

#endif