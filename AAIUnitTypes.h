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
enum class EUnitCategory : int
{
	UNKNOWN              =  0, //! Unknown unit category, i.e. not set
	STATIC_DEFENCE       =  1, //! 
	STATIC_ARTILLERY     =  2, //! 
	STORAGE              =  3, //! 
	STATIC_CONSTRUCTOR   =  4, //! factories
	STATIC_SUPPORT       =  5, //! jammer, air base, missile launcher, shields
	STATIC_SENSOR        =  6, //! radar, sonar, seismic
	POWER_PLANT          =  7, //! 
	METAL_EXTRACTOR      =  8, //!
	METAL_MAKER          =  9, //!
	COMMANDER            = 10, //!
	GROUND_COMBAT        = 11, //! 
	AIR_COMBAT           = 12, //!
	HOVER_COMBAT         = 13, //!
	SEA_COMBAT           = 14, //!
	SUBMARINE_COMBAT     = 15,    
	MOBILE_ARTILLERY     = 16, //!
	SCOUT                = 17, //!
	TRANSPORT            = 18, //!
	MOBILE_CONSTRUCTOR   = 19, //!
	MOBILE_SUPPORT       = 20, //! mobile radar, jammer, anti-nukes
	NUMBER_OF_CATEGORIES = 21
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

	bool isSubmarineCombat()   const { return (m_unitCategory == EUnitCategory::SUBMARINE_COMBAT) ? true : false; };

	bool isMobileArtillery()   const { return (m_unitCategory == EUnitCategory::MOBILE_ARTILLERY) ? true : false; };
	
	bool isScout()             const { return (m_unitCategory == EUnitCategory::SCOUT) ? true : false; };
	
	bool isTransport()         const { return (m_unitCategory == EUnitCategory::TRANSPORT) ? true : false; };
	
	bool isMobileConstructor() const { return (m_unitCategory == EUnitCategory::MOBILE_CONSTRUCTOR) ? true : false; };
	
	bool isMobileSupport()     const { return (m_unitCategory == EUnitCategory::MOBILE_SUPPORT) ? true : false; };

	bool isBuilding()          const { return      (static_cast<int>(m_unitCategory) >= static_cast<int>(EUnitCategory::STATIC_DEFENCE) )
												&& (static_cast<int>(m_unitCategory) <= static_cast<int>(EUnitCategory::METAL_MAKER) ); };

	bool isCombatUnit()        const { return      (static_cast<int>(m_unitCategory) >= static_cast<int>(EUnitCategory::GROUND_COMBAT) )
												&& (static_cast<int>(m_unitCategory) <= static_cast<int>(EUnitCategory::SEA_COMBAT) ); };

	static const int numberOfUnitCategories = static_cast<int>(EUnitCategory::NUMBER_OF_CATEGORIES);

	static EUnitCategory GetFirst() { return EUnitCategory::UNKNOWN; };

	void Next() { m_unitCategory = static_cast<EUnitCategory>( static_cast<int>(m_unitCategory) + 1 ); };

	bool End() const { return (m_unitCategory == EUnitCategory::NUMBER_OF_CATEGORIES); };

	//! Returns unit category as index (to access arrays)
	int GetArrayIndex() const {return static_cast<int>(m_unitCategory); };

private:
	//! The unit category
	EUnitCategory m_unitCategory;
};

//! Different categories of combat units (equal to unit categories, mainly used for array access)
enum class ECombatUnitCategory : int
{
	GROUND_COMBAT        = 0,
	AIR_COMBAT           = 1,
	HOVER_COMBAT         = 2,
	SEA_COMBAT           = 3,
	SUBMARINE_COMBAT     = 4,
	NUMBER_OF_CATEGORIES = 5,
	UNKNOWN              = 6
};

//! The combat unit category contains a subset of the unit category (only combat units) and is mainly used
//! for handling arrays storing combat unit related data.
class AAICombatUnitCategory
{
public:
	AAICombatUnitCategory() : m_combatUnitCategory(ECombatUnitCategory::UNKNOWN) {}

	AAICombatUnitCategory(ECombatUnitCategory category) : m_combatUnitCategory(category) {}

	AAICombatUnitCategory(const AAIUnitCategory& category) : m_combatUnitCategory( static_cast<ECombatUnitCategory>(category.GetArrayIndex() - static_cast<int>(EUnitCategory::GROUND_COMBAT)) ) {}

	static const int numberOfCombatUnitCategories = static_cast<int>(ECombatUnitCategory::NUMBER_OF_CATEGORIES);

	static const ECombatUnitCategory firstCombatUnitCategory = ECombatUnitCategory::GROUND_COMBAT;

	void Next() { m_combatUnitCategory = static_cast<ECombatUnitCategory>( static_cast<int>(m_combatUnitCategory) + 1 ); }

	bool End() const { return (m_combatUnitCategory == ECombatUnitCategory::NUMBER_OF_CATEGORIES); }

	//! Returns index to access arrays storing combat unit data, i.e. ranging from 0 to numberOfCombatUnitCategories-1
	int GetArrayIndex() const {return static_cast<int>(m_combatUnitCategory); }

private:
	//! The unit category
	ECombatUnitCategory m_combatUnitCategory;
};


//! The type of the unit (may further specifiy the purpose of a unit, e.g. anti ground vs anti air for combat units)
enum class EUnitType : int
{
	UNKNOWN              = 0x0000, //! Unknown unit type, i.e. not set
	BUILDING             = 0x0001, //! Static unit aka building
	MOBILE_UNIT          = 0x0002, //! Mobile unit
	ANTI_SURFACE         = 0x0004, //! Used for combat units/static defences that can fight land/hover/floating units
	ANTI_AIR             = 0x0008, //! Anti air combat units/static defences
	ANTI_SUBMERGED       = 0x0010, //! Anti submarine combat units/static defences
	ANTI_STATIC          = 0x0020, //! Anti building (bombers)
	RADAR                = 0x0040, //! Radar
	SONAR                = 0x0080, //! Sonar
	SEISMIC              = 0x0100, //! Seismic detector
	RADAR_JAMMER         = 0x0200, //! Radar jammer
	SONAR_JAMMER         = 0x0400, //! Sonar jammer
	BUILDER              = 0x0800, //! Can construct buildings
	FACTORY              = 0x1000, //! Can construct units
	CONSTRUCTION_ASSIST  = 0x2000, //! Can assists with construction of units/buildings
};

//! @brief Unit type with convenience functions (works as bitmask)
class AAIUnitType
{
public:
	AAIUnitType() : m_unitType(static_cast<int>(EUnitType::UNKNOWN)) {}

	//! @brief Sets the given unit type
	void SetUnitType(EUnitType unitType) { m_unitType = static_cast<int>(unitType);  }

	//! @brief Adds the given unit type 
	void AddUnitType(EUnitType unitType) { m_unitType |= static_cast<int>(unitType); }

	//! Returns whether given unit type is set
	bool IsUnitTypeSet(EUnitType unitType) const { return static_cast<bool>(m_unitType & static_cast<int>(unitType)); }

	//! Returns whether unit is a building (i.e. static)
	bool IsBuilding()        const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::BUILDING)); }

	//! Returns whether unit is mobile
	bool IsMobileUnit()      const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::MOBILE_UNIT)); }

	//! Returns whether unit is considered to be able to fight against surface units (ground, hover, ships)
	bool IsAntiSurface()     const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::ANTI_SURFACE)); }

	//! Returns whether unit is considered to be an anti air unit
	bool IsAntiAir()         const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::ANTI_AIR)); }

	//! Returns whether unit is considered to be able to fight submerged units (submarines)
	bool IsAntiSubmerged()   const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::ANTI_SUBMERGED)); }

	//! Returns whether unit is considered to be able to fight static units more efficiently (e.g. bombers, missile launchers)
	bool IsAntiStatic()      const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::ANTI_STATIC)); }

	//! Returns true if radar flag is set
	bool IsRadar()           const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::RADAR)); }

	//! Returns true if sonar flag is set
	bool IsSonar()           const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::SONAR)); }

	//! Returns true if seismic detector flag is set
	bool IsSeismicDetector() const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::SEISMIC)); }

	//! Returns true if radar jammer flag is set
	bool IsRadarJammer()     const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::RADAR_JAMMER)); }

	//! Returns true if radar jammer flag is set
	bool IsSonarJammer()     const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::SONAR_JAMMER)); }

	//! Returns true if unit can construct at least one building
	bool IsBuilder()         const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::BUILDER)); }

	//! Returns true if unit can construct at least one mobile unit
	bool IsFactory()         const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::FACTORY)); }

	//! Returns true if unit can help with costruction of other units/buildings
	bool IsConstructionAssist()     const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::CONSTRUCTION_ASSIST)); }

	//! Returns whether unit is considered to be able to fight against surface or submerged units (not anti air)
	bool IsAssaultUnit()     const { return static_cast<bool>(m_unitType & (static_cast<int>(EUnitType::ANTI_SURFACE) + static_cast<int>(EUnitType::ANTI_SUBMERGED) )); }

private:
	//! Unit type
	int m_unitType;
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
};

//! The combat category describes what kind of target class a unit belongs to
enum class ETargetTypeCategory : int
{
	SURFACE              = 0, //! Units on ground (move type ground, amphibious, hover, land buildings)
	AIR                  = 1, //! Air units
	FLOATER              = 2, //! Units moving above water (ships, hover) or floating buildings
	SUBMERGED            = 3, //! Units moving below water (submarines) or submerged buildings
	NUMBER_OF_CATEGORIES = 4, //! The number of combat categories (unknown/invalid not used)
	UNKNOWN              = 5, //! This value will be treated as invalid
};

//! This class handles the target category which describes
class AAICombatCategory
{
public:
	AAICombatCategory(ETargetTypeCategory combatUnitCategory) : m_combatUnitCategory(combatUnitCategory) {};

	AAICombatCategory() : AAICombatCategory(ETargetTypeCategory::UNKNOWN) {};

	void setCategory(ETargetTypeCategory category) { m_combatUnitCategory = category; };

	bool IsValid()     const { return (m_combatUnitCategory != ETargetTypeCategory::UNKNOWN); };

	bool IsGround()    const { return (m_combatUnitCategory == ETargetTypeCategory::SURFACE); };

	bool IsAir()       const { return (m_combatUnitCategory == ETargetTypeCategory::AIR); };

	bool IsFloater()   const { return (m_combatUnitCategory == ETargetTypeCategory::FLOATER); };

	bool IsSubmerged() const { return (m_combatUnitCategory == ETargetTypeCategory::SUBMERGED); };

	int GetArrayIndex() const {return static_cast<int>(m_combatUnitCategory); };

	static int GetArrayIndex(ETargetTypeCategory category) { return static_cast<int>(category); };

	static const int numberOfCombatCategories = static_cast<int>(ETargetTypeCategory::NUMBER_OF_CATEGORIES);

private:
	ETargetTypeCategory m_combatUnitCategory;
};

#endif