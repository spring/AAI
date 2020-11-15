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
#include <vector>
#include <array>

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

    bool operator==(const AAIUnitCategory& rhs) const { return (m_unitCategory == rhs.GetUnitCategory()); };

	void SetUnitCategory(EUnitCategory unitCategory) { m_unitCategory = unitCategory; };

	EUnitCategory GetUnitCategory() const { return m_unitCategory; };

	bool IsValid()             const { return (m_unitCategory != EUnitCategory::UNKNOWN) ? true : false; };

	bool IsStaticDefence()     const { return (m_unitCategory == EUnitCategory::STATIC_DEFENCE) ? true : false; };

	bool IsStaticArtillery()   const { return (m_unitCategory == EUnitCategory::STATIC_ARTILLERY) ? true : false; };

	bool IsStorage()           const { return (m_unitCategory == EUnitCategory::STORAGE) ? true : false; };

	bool IsStaticConstructor() const { return (m_unitCategory == EUnitCategory::STATIC_CONSTRUCTOR) ? true : false; };

	bool IsStaticSupport()     const { return (m_unitCategory == EUnitCategory::STATIC_SUPPORT) ? true : false; };

	bool IsStaticSensor()      const { return (m_unitCategory == EUnitCategory::STATIC_SENSOR) ? true : false; };

	bool IsPowerPlant()        const { return (m_unitCategory == EUnitCategory::POWER_PLANT) ? true : false; };

	bool IsMetalExtractor()    const { return (m_unitCategory == EUnitCategory::METAL_EXTRACTOR) ? true : false; };

	bool IsMetalMaker()        const { return (m_unitCategory == EUnitCategory::METAL_MAKER) ? true : false; };

	bool IsCommander()         const { return (m_unitCategory == EUnitCategory::COMMANDER) ? true : false; };

	bool IsGroundCombat()      const { return (m_unitCategory == EUnitCategory::GROUND_COMBAT) ? true : false; };

	bool IsAirCombat()         const { return (m_unitCategory == EUnitCategory::AIR_COMBAT) ? true : false; };

	bool IsHoverCombat()       const { return (m_unitCategory == EUnitCategory::HOVER_COMBAT) ? true : false; };

	bool IsSeaCombat()         const { return (m_unitCategory == EUnitCategory::SEA_COMBAT) ? true : false; };

	bool IsSubmarineCombat()   const { return (m_unitCategory == EUnitCategory::SUBMARINE_COMBAT) ? true : false; };

	bool IsMobileArtillery()   const { return (m_unitCategory == EUnitCategory::MOBILE_ARTILLERY) ? true : false; };
	
	bool IsScout()             const { return (m_unitCategory == EUnitCategory::SCOUT) ? true : false; };
	
	bool IsTransport()         const { return (m_unitCategory == EUnitCategory::TRANSPORT) ? true : false; };
	
	bool IsMobileConstructor() const { return (m_unitCategory == EUnitCategory::MOBILE_CONSTRUCTOR) ? true : false; };
	
	bool IsMobileSupport()     const { return (m_unitCategory == EUnitCategory::MOBILE_SUPPORT) ? true : false; };

	bool IsBuilding()          const { return      (static_cast<int>(m_unitCategory) >= static_cast<int>(EUnitCategory::STATIC_DEFENCE) )
												&& (static_cast<int>(m_unitCategory) <= static_cast<int>(EUnitCategory::METAL_MAKER) ); };

	bool IsCombatUnit()        const { return      (static_cast<int>(m_unitCategory) >= static_cast<int>(EUnitCategory::GROUND_COMBAT) )
												&& (static_cast<int>(m_unitCategory) <= static_cast<int>(EUnitCategory::SEA_COMBAT) ); };

	static constexpr int numberOfUnitCategories = static_cast<int>(EUnitCategory::NUMBER_OF_CATEGORIES);

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

	ECombatUnitCategory GetCombatUnitCategory() const { return m_combatUnitCategory; }
	
	static const int numberOfCombatUnitCategories = static_cast<int>(ECombatUnitCategory::NUMBER_OF_CATEGORIES);

	//! Returns index to access arrays storing combat unit data, i.e. ranging from 0 to numberOfCombatUnitCategories-1
	int GetArrayIndex() const {return static_cast<int>(m_combatUnitCategory); }

private:
	//! The unit category
	ECombatUnitCategory m_combatUnitCategory;
};

//! The target category describes what kind of target class a unit belongs to
enum class ETargetType : int
{
	SURFACE              = 0, //! Units on ground (move type ground, amphibious, hover, land buildings)
	AIR                  = 1, //! Air units
	FLOATER              = 2, //! Units moving above water (ships, hover) or floating buildings
	SUBMERGED            = 3, //! Units moving below water (submarines) or submerged buildings
	STATIC               = 4, //! Static units (= buildings)
	NUMBER_OF_CATEGORIES = 5, //! The number of combat categories (unknown/invalid not used)
	UNKNOWN              = 6, //! This value will be treated as invalid
};

//! This class handles the target category which describes
class AAITargetType
{
public:
	AAITargetType(ETargetType targetType) : m_targetType(targetType) {}

	AAITargetType() : AAITargetType(ETargetType::UNKNOWN) {}

	AAITargetType(const AAICombatUnitCategory& combatCategory)
	{
		switch(combatCategory.GetCombatUnitCategory())
		{
			case ECombatUnitCategory::GROUND_COMBAT:
			case ECombatUnitCategory::HOVER_COMBAT:
				m_targetType = ETargetType::SURFACE;
				break;
			case ECombatUnitCategory::AIR_COMBAT:
				m_targetType = ETargetType::AIR;
				break;
			case ECombatUnitCategory::SEA_COMBAT:
				m_targetType = ETargetType::FLOATER;
				break;
			case ECombatUnitCategory::SUBMARINE_COMBAT:
				m_targetType = ETargetType::SUBMERGED;
				break;
			default:
				m_targetType = ETargetType::UNKNOWN;
		}
	}

	void SetType(ETargetType targetType) { m_targetType = targetType; }

	bool IsValid()      const { return (m_targetType != ETargetType::UNKNOWN); }

	bool IsSurface()    const { return (m_targetType == ETargetType::SURFACE); }

	bool IsAir()        const { return (m_targetType == ETargetType::AIR); }

	bool IsFloater()    const { return (m_targetType == ETargetType::FLOATER); }

	bool IsSubmerged()  const { return (m_targetType == ETargetType::SUBMERGED); }

	bool IsStatic()     const { return (m_targetType == ETargetType::STATIC); }

	int GetArrayIndex() const {return static_cast<int>(m_targetType); }

	static int GetArrayIndex(ETargetType targetType) { return static_cast<int>(targetType); }

	static constexpr int surfaceIndex   = static_cast<int>(ETargetType::SURFACE);
	static constexpr int airIndex       = static_cast<int>(ETargetType::AIR);
	static constexpr int floaterIndex   = static_cast<int>(ETargetType::FLOATER);
	static constexpr int submergedIndex = static_cast<int>(ETargetType::SUBMERGED);
	static constexpr int staticIndex    = static_cast<int>(ETargetType::STATIC);

	static constexpr int numberOfMobileTargetTypes = static_cast<int>(ETargetType::NUMBER_OF_CATEGORIES)-1;

	static constexpr int numberOfTargetTypes = static_cast<int>(ETargetType::NUMBER_OF_CATEGORIES);

	static constexpr std::array<ETargetType, 4> m_mobileTargetTypes = {ETargetType::SURFACE, ETargetType::AIR, ETargetType::FLOATER, ETargetType::SUBMERGED};

	const std::string& GetName() const { return m_targetTypeNames[GetArrayIndex()]; }

	static const std::vector<std::string> m_targetTypeNames;
	//const static std::vector<std::string> m_targetTypeNames = {"surface", "air", "floater", "submerged"};
private:
	ETargetType m_targetType;
};

//! The type of the unit (may further specifiy the purpose of a unit, e.g. anti ground vs anti air for combat units)
enum class EUnitType : int
{
	UNKNOWN              = 0x0000, //! Unknown unit type, i.e. not set
	BUILDING             = 0x0001, //! Static unit aka building
	MOBILE_UNIT          = 0x0002, //! Mobile unit
	ANTI_SURFACE         = 0x0004, //! Used for combat units/static defences that can fight land/hover/floating units
	ANTI_AIR             = 0x0008, //! Anti air combat units/static defences
	ANTI_SHIP            = 0x0010, //! Anti ship combat units/static defences
	ANTI_SUBMERGED       = 0x0020, //! Anti submarine combat units/static defences
	ANTI_STATIC          = 0x0040, //! Anti building (bombers)
	RADAR                = 0x0080, //! Radar
	SONAR                = 0x0100, //! Sonar
	SEISMIC              = 0x0200, //! Seismic detector
	RADAR_JAMMER         = 0x0400, //! Radar jammer
	SONAR_JAMMER         = 0x0800, //! Sonar jammer
	BUILDER              = 0x1000, //! Can construct buildings
	FACTORY              = 0x2000, //! Can construct units
	CONSTRUCTION_ASSIST  = 0x4000, //! Can assists with construction of units/buildings
};

//! @brief Unit type with convenience functions (works as bitmask)
class AAIUnitType
{
public:
	AAIUnitType() : m_unitType(static_cast<int>(EUnitType::UNKNOWN)) {}

	AAIUnitType(EUnitType unitType) : m_unitType(static_cast<int>(unitType)) {}

	//! @brief Sets the given unit type
	void SetUnitType(EUnitType unitType) { m_unitType = static_cast<int>(unitType);  }

	//! @brief Adds the given unit type 
	void AddUnitType(EUnitType unitType) { m_unitType |= static_cast<int>(unitType); }

	//! @brief Returns whether given unit type is set
	bool IsUnitTypeSet(EUnitType unitType) const { return static_cast<bool>(m_unitType & static_cast<int>(unitType)); }

	//! @brief Returns whether unit is a building (i.e. static)
	bool IsBuilding()        const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::BUILDING)); }

	//! @brief Returns whether unit is mobile
	bool IsMobileUnit()      const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::MOBILE_UNIT)); }

	//! @brief Returns whether unit is considered to be able to fight against surface units (ground, hover, ships)
	bool IsAntiSurface()     const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::ANTI_SURFACE)); }

	//! @brief Returns whether unit is considered to be an anti air unit
	bool IsAntiAir()         const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::ANTI_AIR)); }

	//! @brief Returns whether unit is considered to be able to fight submerged units (submarines)
	bool IsAntiShip()        const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::ANTI_SHIP)); }

	//! @brief Returns whether unit is considered to be able to fight submerged units (submarines)
	bool IsAntiSubmerged()   const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::ANTI_SUBMERGED)); }

	//! @brief Returns whether unit is considered to be able to fight static units more efficiently (e.g. bombers, missile launchers)
	bool IsAntiStatic()      const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::ANTI_STATIC)); }

	//! @brief Returns true if radar flag is set
	bool IsRadar()           const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::RADAR)); }

	//! @brief Returns true if sonar flag is set
	bool IsSonar()           const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::SONAR)); }

	//! @brief Returns true if seismic detector flag is set
	bool IsSeismicDetector() const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::SEISMIC)); }

	//! @brief Returns true if radar jammer flag is set
	bool IsRadarJammer()     const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::RADAR_JAMMER)); }

	//! @brief Returns true if radar jammer flag is set
	bool IsSonarJammer()     const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::SONAR_JAMMER)); }

	//! @brief Returns true if unit can construct at least one building
	bool IsBuilder()         const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::BUILDER)); }

	//! @brief Returns true if unit can construct at least one mobile unit
	bool IsFactory()         const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::FACTORY)); }

	//! @brief Returns true if unit can help with costruction of other units/buildings
	bool IsConstructionAssist()     const { return static_cast<bool>(m_unitType & static_cast<int>(EUnitType::CONSTRUCTION_ASSIST)); }

	//! @brief Returns whether unit is considered to be able to fight against surface or submerged units (not anti air)
	bool IsAssaultUnit()     const { return static_cast<bool>(m_unitType & (static_cast<int>(EUnitType::ANTI_SURFACE) + static_cast<int>(EUnitType::ANTI_SHIP) + static_cast<int>(EUnitType::ANTI_SUBMERGED) )); }

	//! @brief Returns whether unit type is suitable to fight given target type
	bool CanFightTargetType(const AAITargetType& targetType) const
	{
		return (targetType.IsSurface()   && IsAntiSurface())
			|| (targetType.IsAir()       && IsAntiAir())
			|| (targetType.IsFloater()   && IsAntiShip())
			|| (targetType.IsSubmerged() && IsAntiSubmerged())
			|| (targetType.IsStatic()    && IsAntiStatic());
	}

private:
	//! Unit type
	int m_unitType;
};

#endif