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

class AAIThreatMap
{
public:
	//friend AAIMap;

	AAIThreatMap(int xSectors, int ySectors);

	~AAIThreatMap(void);

	//! @brief Calculates the combat power values for each sector assuming given position of own units
	void UpdateLocalEnemyCombatPower(const AAITargetType& targetType, const std::vector< std::vector<AAISector> >& sectors);

	//! @brief Determines sector to attack (nullptr if none found)
	const AAISector* DetermineSectorToAttack(const AAITargetType& attackerTargetType, const MapPos& position, const std::vector< std::vector<AAISector> >& sectors) const;

private:
	float CalculateCombatPower(const AAITargetType& targetType, const MapPos& origin, const MapPos& target) const;

	//! Buffer to store the estimated enemy combat power available to defend each sector
	std::vector< std::vector<MobileTargetTypeValues> > m_estimatedEnemyCombatPowerForSector;
};

#endif