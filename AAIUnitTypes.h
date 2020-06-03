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

typedef unsigned int   uint32_t;

//! Movement types that are used to describe the movement type of every unit
enum class EUnitCategory : uint32_t
{
	UNIT_CATEGORY_UNKNOWN              = 0x000000u, //! Unknown unit category, i.e. not set
	UNIT_CATEGORY_STATIC_DEFENCE       = 0x000001u, //! 
	UNIT_CATEGORY_STATIC_ARTILLERY     = 0x000002u, //! 
	UNIT_CATEGORY_STORAGE              = 0x000004u, //! 
	UNIT_CATEGORY_STATIC_CONSTRUCTOR   = 0x000008u, //! factories
	UNIT_CATEGORY_STATIC_SUPPORT       = 0x000010u, //! static radar, jammer, air base
	UNIT_CATEGORY_POWER_PLANT          = 0x000020u, //! 
	UNIT_CATEGORY_METAL_EXTRACTOR      = 0x000040u, //!
	UNIT_CATEGORY_METAL_MAKER          = 0x000080u, //!
	UNIT_CATEGORY_COMMANDER            = 0x000100u, //!
	UNIT_CATEGORY_GROUND_COMBAT        = 0x000200u, //! 
	UNIT_CATEGORY_AIR_COMBAT           = 0x000400u, //!
	UNIT_CATEGORY_HOVER_COMBAT         = 0x000800u, //!
	UNIT_CATEGORY_SEA_COMBAT           = 0x001000u, //! Ships & submarines
	UNIT_CATEGORY_MOBILE_ARTILLERY     = 0x002000u, //!
	UNIT_CATEGORY_SCOUT                = 0x004000u, //!
	UNIT_CATEGORY_TRANSPORT            = 0x008000u, //!
	UNIT_CATEGORY_MOBILE_CONSTRUCTOR   = 0x010000u, //!
	UNIT_CATEGORY_MOBILE_SUPPORT       = 0x020000u  //! mobile radar, jammer, anti-nukes
};

//! The unit category is a coarse classification used to differentiate between different types of units.
//! Statistical data (e.g. buildcost) is calculated for each category. Further differentiation 
//! (e.g. combat vs. anti air units) is given by AAIUnitType.
class AAIUnitCategory
{
public:
	AAIUnitCategory() : m_unitCategory(EUnitCategory::UNIT_CATEGORY_UNKNOWN) {};

	AAIUnitCategory(EUnitCategory unitCategory) : m_unitCategory(unitCategory) {};

	void setUnitCategory(EUnitCategory unitCategory) { m_unitCategory = unitCategory; };

	EUnitCategory getUnitCategory() const { return m_unitCategory; };

	bool isValid()             const { return (m_unitCategory != EUnitCategory::UNIT_CATEGORY_UNKNOWN) ? true : false; };

	bool isStaticDefence()     const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_STATIC_DEFENCE) ? true : false; };

	bool isStaticArtillery()   const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_STATIC_ARTILLERY) ? true : false; };

	bool isStorage()           const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_STORAGE) ? true : false; };

	bool isStaticConstructor() const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_STATIC_CONSTRUCTOR) ? true : false; };

	bool isStaticSupport()     const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_STATIC_SUPPORT) ? true : false; };

	bool isSPowerPlant()       const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_POWER_PLANT) ? true : false; };

	bool isMetalExtractor()    const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_METAL_EXTRACTOR) ? true : false; };

	bool isMetalMaker()        const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_METAL_MAKER) ? true : false; };

	bool isCommander()         const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_COMMANDER) ? true : false; };

	bool isGroundCombat()      const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_GROUND_COMBAT) ? true : false; };

	bool isAirCombat()         const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_AIR_COMBAT) ? true : false; };

	bool isHoverCombat()       const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_HOVER_COMBAT) ? true : false; };

	bool isSeaCombat()         const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_SEA_COMBAT) ? true : false; };

	bool isMobileArtillery()   const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_MOBILE_ARTILLERY) ? true : false; };
	
	bool isScout()             const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_SCOUT) ? true : false; };
	
	bool isTransport()         const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_TRANSPORT) ? true : false; };
	
	bool isMobileConstructor() const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_MOBILE_CONSTRUCTOR) ? true : false; };
	
	bool isMobileSupport()     const { return (m_unitCategory == EUnitCategory::UNIT_CATEGORY_MOBILE_SUPPORT) ? true : false; };

private:
	//! The unit category
	EUnitCategory m_unitCategory;
};

#endif