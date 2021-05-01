// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "AAIThreatMap.h"
#include "AAIMap.h"

AAIThreatMap::AAIThreatMap(int xSectors, int ySectors) :
	m_estimatedEnemyCombatPowerForSector( xSectors, std::vector<MobileTargetTypeValues>(ySectors) )
{
}

AAIThreatMap::~AAIThreatMap(void)
{
}

void AAIThreatMap::UpdateLocalEnemyCombatPower(const AAITargetType& targetType, const SectorMap& sectors)
{
	for(size_t x = 0; x < sectors.size(); ++x)
	{
		for(size_t y = 0; y < sectors[x].size(); ++y)
		{
			m_estimatedEnemyCombatPowerForSector[x][y].SetValueForTargetType(targetType, sectors[x][y].GetEnemyCombatPower(targetType) );
		}
	}
}

const AAISector* AAIThreatMap::DetermineSectorToAttack(const AAITargetType& attackerTargetType, const MapPos& mapPosition, const SectorMap& sectors) const
{
	const float3 position( static_cast<float>(mapPosition.x * SQUARE_SIZE), 0.0f, static_cast<float>(mapPosition.y * SQUARE_SIZE));
	const SectorIndex startSectorIndex = AAIMap::GetSectorIndex(position);

	float highestRating(0.0f);
	const AAISector* selectedSector = nullptr;

	for(size_t x = 0; x < sectors.size(); ++x)
	{
		for(size_t y = 0; y < sectors[x].size(); ++y)
		{
			const int enemyBuildings = sectors[x][y].GetNumberOfEnemyBuildings();

			if(enemyBuildings > 0)
			{
				const float3 sectorCenter = sectors[x][y].GetCenter();

				const float dx = sectorCenter.x - position.x;
				const float dz = sectorCenter.z - position.z;
				const float distSquared = dx * dx + dz * dz;

				// distance rating between 0 (close to given position) and 0.9 (~ 0.7 of map size away)
				const float distRating = std::min( distSquared / (0.5f * AAIMap::s_maxSquaredMapDist), 0.9f);

				// value between 0.1 (15 or more recently lost units) and 1 (no lost units)
				const float lostUnitsRating = std::max(1.0f - sectors[x][y].GetTotalLostUnits() / 15.0f, 0.1f);

				const float enemyCombatPower = CalculateThreat<EThreatType::COMBAT_POWER>(attackerTargetType, startSectorIndex, SectorIndex(x, y), sectors);

				const float rating =  static_cast<float>(enemyBuildings) / (0.1f + enemyCombatPower) * (1.0 - distRating) * lostUnitsRating;

				if(rating > highestRating)
				{
					selectedSector = &sectors[x][y];
					highestRating  = rating;
				}
			}
		}
	}

	return selectedSector;
}

float AAIThreatMap::CalculateEnemyDefencePower(const AAITargetType& targetType, const float3& startPosition, const float3& targetPosition, const SectorMap& sectors) const
{
	const SectorIndex startSectorIndex  = AAIMap::GetSectorIndex(startPosition);
	const SectorIndex targetSectorIndex = AAIMap::GetSectorIndex(targetPosition);

	return CalculateThreat<EThreatType::ALL>(targetType, startSectorIndex, targetSectorIndex, sectors);
}

template<EThreatType threatTypeToConsider>
float AAIThreatMap::CalculateThreat(const AAITargetType& targetType, const SectorIndex& startSectorIndex, const SectorIndex& targetSectorIndex, const SectorMap& sectors) const
{
	float totalThreat(0.0f);

	const float dx = static_cast<float>( targetSectorIndex.x - startSectorIndex.x );
	const float dy = static_cast<float>( targetSectorIndex.y - startSectorIndex.y );

	const float invDist = fastmath::isqrt2_nosse(dx * dx + dy * dy);

	SectorIndex lastSector(startSectorIndex);
	bool targetSectorReached(false);
	float step(1);

	/*FILE* file = fopen("AAIAttackDebug.txt", "w+");
	fprintf(file, "Start sector:  (%i, %i)\n", startSector.x, startSector.y);
	fprintf(file, "Target sector: (%i, %i)\n", targetSector.x, targetSector.y);
	fprintf(file, "Distance: %f\n", 1.0f / invDist);*/

	while(targetSectorReached == false)
	{
		const int x = startSectorIndex.x + static_cast<int>( step * dx * invDist );
		const int y = startSectorIndex.y + static_cast<int>( step * dy * invDist );

		if( (x !=lastSector.x) || (y != lastSector.y) ) // avoid counting the same sector twice if step size is too low because of rounding errors
		{
			if( static_cast<int>(threatTypeToConsider) & static_cast<int>(EThreatType::COMBAT_POWER) )
				totalThreat += m_estimatedEnemyCombatPowerForSector[x][y].GetValueOfTargetType(targetType);

			if( static_cast<int>(threatTypeToConsider) & static_cast<int>(EThreatType::LOST_UNITS) )
				totalThreat += sectors[x][y].GetLostUnits(targetType);
		}

		//fprintf(file, "Step: %f, sector (%i, %i), combat power: %f\n", step, x, y, m_estimatedEnemyCombatPowerForSector[x][y].GetValueOfTargetType(targetType));

		if((SectorIndex(x, y) == targetSectorIndex) || (step > dx+dy))
			targetSectorReached = true;

		++step;
	}

	//fprintf(file, "Final combat power: %f\n", combatPower);
	//fclose(file);

	return totalThreat;
}