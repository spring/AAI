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

void AAIThreatMap::UpdateLocalEnemyCombatPower(const AAITargetType& targetType, const std::vector< std::vector<AAISector> >& sectors)
{
	for(size_t x = 0; x < sectors.size(); ++x)
	{
		for(size_t y = 0; y < sectors[x].size(); ++y)
		{
			//for(auto targetType : AAITargetType::m_targetTypes)
				m_estimatedEnemyCombatPowerForSector[x][y].SetValueForTargetType(targetType, sectors[x][y].GetEnemyCombatPower(targetType) );
		}
	}
}

const AAISector* AAIThreatMap::DetermineSectorToAttack(const AAITargetType& attackerTargetType, const MapPos& mapPosition, const std::vector< std::vector<AAISector> >& sectors) const
{
	const float3 position( static_cast<float>(mapPosition.x * SQUARE_SIZE), 0.0f, static_cast<float>(mapPosition.y * SQUARE_SIZE));
	const MapPos startSector( position.x/AAIMap::xSectorSize, position.z/AAIMap::ySectorSize);

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
				const float lostUnitsRating = std::max(1.0f - sectors[x][y].GetLostUnits() / 15.0f, 0.1f);

				const float enemyCombatPower = CalculateCombatPower(attackerTargetType, startSector, MapPos(x, y));

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

float AAIThreatMap::CalculateCombatPower(const AAITargetType& targetType, const MapPos& startSector, const MapPos& targetSector) const
{
	float combatPower(0.0f);

	const float dx = static_cast<float>( targetSector.x - startSector.x );
	const float dy = static_cast<float>( targetSector.y - startSector.y );

	const float invDist = fastmath::isqrt2_nosse(dx * dx + dy * dy);

	MapPos lastSector(startSector);
	bool targetSectorReached(false);
	float step(1);

	/*FILE* file = fopen("AAIAttackDebug.txt", "w+");
	fprintf(file, "Start sector:  (%i, %i)\n", startSector.x, startSector.y);
	fprintf(file, "Target sector: (%i, %i)\n", targetSector.x, targetSector.y);
	fprintf(file, "Distance: %f\n", 1.0f / invDist);*/

	while(targetSectorReached == false)
	{
		const int x = startSector.x + static_cast<int>( step * dx * invDist );
		const int y = startSector.y + static_cast<int>( step * dy * invDist );

		if( (x !=lastSector.x) || (y != lastSector.y) ) // avoid counting the same sector twice if step size is too low because of rounding errors
			combatPower += m_estimatedEnemyCombatPowerForSector[x][y].GetValueOfTargetType(targetType);

		//fprintf(file, "Step: %f, sector (%i, %i), combat power: %f\n", step, x, y, m_estimatedEnemyCombatPowerForSector[x][y].GetValueOfTargetType(targetType));

		if((MapPos(x, y) == targetSector) || (step > dx+dy))
			targetSectorReached = true;

		++step;
	}

	//fprintf(file, "Final combat power: %f\n", combatPower);
	//fclose(file);

	return combatPower;
}