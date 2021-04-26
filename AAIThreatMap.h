// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#ifndef AAI_THREATMAP_H
#define AAI_THREATMAP_H

#include "aidef.h"
#include "AAITypes.h"
#include "AAISector.h"

enum class EThreatType : int
{
	UNKNOWN        = 0x00, //! Not set
	COMBAT_POWER   = 0x01, //! Consider enemy combat power
	LOST_UNITS     = 0x02, //! Consider own lost units
	ALL            = 0x03  //! Conider enemy combat power and own lost units
};


class AAIThreatMap
{
public:
	AAIThreatMap(int xSectors, int ySectors);

	~AAIThreatMap(void);

	//! @brief Calculates the combat power values for each sector assuming given position of own units
	void UpdateLocalEnemyCombatPower(const AAITargetType& targetType, const std::vector< std::vector<AAISector> >& sectors);

	//! @brief Determines sector to attack (nullptr if none found)
	const AAISector* DetermineSectorToAttack(const AAITargetType& attackerTargetType, const MapPos& position, const std::vector< std::vector<AAISector> >& sectors) const;

	//! @brief Determines the total enemy defence power of the sector in a line from start to target position
	float CalculateEnemyDefencePower(const AAITargetType& targetType, const float3& startPosition, const float3& targetPosition) const;

private:
	template<EThreatType threatTypeToConsider>
	float CalculateThreat(const AAITargetType& targetType, const SectorIndex& startSectorIndex, const SectorIndex& targetSectorIndex) const;

	//! Buffer to store the estimated enemy combat power available to defend each sector
	std::vector< std::vector<MobileTargetTypeValues> > m_estimatedEnemyCombatPowerForSector;

	//! Buffer to store the number of lost units per sector
	std::vector< std::vector<MobileTargetTypeValues> > m_lostUnitsInSector;
};

#endif