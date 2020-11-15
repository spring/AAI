// -------------------------------------------------------------------------
// AAI
//
// A skirmish AI for the Spring engine.
// Copyright Alexander Seizinger
//
// Released under GPL license: see LICENSE.html for more information.
// -------------------------------------------------------------------------

#include "AAIMapTypes.h"
#include "AAIConfig.h"
#include "AAIMap.h"

void AAIScoutedUnitsMap::Init(int xMapSize, int yMapSize, int losMapResolution)
{
	m_losToScoutMapResolution = losMapResolution / scoutMapResolution;
	m_xScoutMapSize           = xMapSize/scoutMapResolution;
	m_yScoutMapSize           = yMapSize/scoutMapResolution;

	m_scoutedUnitsMap.resize(m_xScoutMapSize*m_yScoutMapSize, 0);
	m_lastUpdateInFrameMap.resize(m_xScoutMapSize*m_yScoutMapSize, 0);
}

void AAIScoutedUnitsMap::ResetTiles(int xLosMap, int yLosMap, int frame)
{
	int tileIndex = xLosMap*m_losToScoutMapResolution + yLosMap*m_losToScoutMapResolution * m_xScoutMapSize;

	for(int y = 0; y < m_losToScoutMapResolution; ++y)
	{
		for(int x = 0; x < m_losToScoutMapResolution; ++x)
		{
			m_scoutedUnitsMap[tileIndex]      = 0;
			m_lastUpdateInFrameMap[tileIndex] = frame;

			++tileIndex;
		}

		tileIndex += (m_xScoutMapSize-m_losToScoutMapResolution);
	}
}

void AAIScoutedUnitsMap::UpdateSectorWithScoutedUnits(AAISector *sector, std::vector<int>& buildingsOnContinent)
{
	const int xStart = (sector->x * AAIMap::xSectorSizeMap) / scoutMapResolution;
	const int yStart = (sector->y * AAIMap::ySectorSizeMap) / scoutMapResolution;
	int tileIndex = xStart + yStart * m_xScoutMapSize;

	const int xCells = AAIMap::xSectorSizeMap/scoutMapResolution;
	const int yCells = AAIMap::ySectorSizeMap/scoutMapResolution;

	for(int y = 0; y < yCells; ++y)
	{
		for(int x = 0; x < xCells; ++x)
		{
			const UnitDefId unitDefId(m_scoutedUnitsMap[tileIndex]);

			if(unitDefId.IsValid())
			{
				sector->AddScoutedEnemyUnit(unitDefId, m_lastUpdateInFrameMap[tileIndex]);

				
				const int continentId = AAIMap::s_continentMap.GetContinentID( MapPos((xStart+x)*scoutMapResolution, (yStart+y)*scoutMapResolution) );
				
				++buildingsOnContinent[continentId];
			}
			
			++tileIndex;
		}

		tileIndex += (m_xScoutMapSize-xCells);
	}
}

void AAIContinentMap::Init(int xMapSize, int yMapSize)
{ 
	m_xContMapSize = xMapSize / continentMapResolution;
	m_yContMapSize = yMapSize / continentMapResolution;

	m_continentMap.resize(m_xContMapSize*m_yContMapSize, -1);
}

void AAIContinentMap::LoadFromFile(FILE* file)
{
	for(int y = 0; y < m_yContMapSize; ++y)
	{
		for(int x = 0; x < m_xContMapSize; ++x)
		{
			fscanf(file, "%i ", &m_continentMap[y * m_xContMapSize + x]);
		}
	}
}

void AAIContinentMap::SaveToFile(FILE* file)
{
	for(int y = 0; y < m_yContMapSize; ++y)
	{
		for(int x = 0; x < m_xContMapSize; ++x)
			fprintf(file, "%i ", m_continentMap[y * m_xContMapSize + x]);

		fprintf(file, "\n");
	}
}

int AAIContinentMap::GetContinentID(const float3& pos) const
{
	int x = static_cast<int>(pos.x) / (SQUARE_SIZE * continentMapResolution);
	int y = static_cast<int>(pos.z) / (SQUARE_SIZE * continentMapResolution);

	// check if pos inside of the map
	if(x < 0)
		x = 0;
	else if(x >= m_xContMapSize)
		x = m_xContMapSize - 1;

	if(y < 0)
		y = 0;
	else if(y >= m_yContMapSize)
		y = m_yContMapSize - 1;

	return m_continentMap[x + y * m_xContMapSize];
}

void AAIContinentMap::CheckIfTileBelongsToLandContinent(const int continentMapTileIndex, const float tileHeight, std::vector<AAIContinent>& continents, const int continentId, std::vector<int>* nextEdgeCells)
{
	if(m_continentMap[continentMapTileIndex] == -1)
	{
		// if height is above sea level, the tile belongs to the same continent
		// -> set it on list whose neighbours shall be checked in the next iteration
		if(tileHeight >= 0.0f)
		{
			m_continentMap[continentMapTileIndex] = continentId;
			continents[continentId].size += 1;
			nextEdgeCells->push_back( continentMapTileIndex );
		}
		// if height is below sea level but not below maximum water depth for non amphibious land units
		// -> tile does not belong to land continent but check its neighbours in the next iteration
		// -> ensures that connected land masses are detected as one continent
		else if(tileHeight >= - cfg->NON_AMPHIB_MAX_WATERDEPTH)
		{
			m_continentMap[continentMapTileIndex] = -2;
			nextEdgeCells->push_back( continentMapTileIndex );
		}
	}
}

void AAIContinentMap::CheckIfTileBelongsToSeaContinent(const int continentMapTileIndex, const float tileHeight, std::vector<AAIContinent>& continents, const int continentId, std::vector<int>* nextEdgeCells)
{
	if( (m_continentMap[continentMapTileIndex] < 0) && (tileHeight < 0.0f) )
	{
		m_continentMap[continentMapTileIndex] = continentId;
		continents[continentId].size += 1;
		nextEdgeCells->push_back(continentMapTileIndex);	
	}
}
				
void AAIContinentMap::DetectContinents(std::vector<AAIContinent>& continents, const float *heightMap, const int xMapSize, const int yMapSize)
{
	// In every iteration, one of those two containers will be used to store the newly identified edge tiles
	// After each iteration, the containers will be flipped  to check for further neighbours.
	// When no new edge cells are found the detection of the current continent has been completed.
	std::vector<int> edgeTiles1, edgeTiles2;

	std::vector<int>* new_edge_cells(&edgeTiles1);
	std::vector<int>* old_edge_cells(&edgeTiles2);

	int continentId(0);

	for(int i = 0; i < m_xContMapSize; i += 1)
	{
		for(int j = 0; j < m_yContMapSize; j += 1)
		{
			// add new continent if cell has not been visited yet
			if( (m_continentMap[j * m_xContMapSize + i] < 0) && (heightMap[4 * (j * xMapSize + i)] >= 0.0f) )
			{
				AAIContinent newContinent(continentId, 1, false);
				continents.push_back(newContinent);

				m_continentMap[j * m_xContMapSize + i] = continentId;

				old_edge_cells->push_back(j * m_xContMapSize + i);

				// check edges of the continent as long as new cells have been added to the continent during the last loop
				while(old_edge_cells->size() > 0)
				{
					for(auto cell : *old_edge_cells)
					{
						// get cell indizes
						const int x = cell%m_xContMapSize;
						const int y = (cell - x) / m_xContMapSize;

						const int continentMapTileIndex = y * m_xContMapSize + x;
						const int heightMapTileIndex    = 4 * (y * xMapSize + x);

						if(x > 0)
							CheckIfTileBelongsToLandContinent(continentMapTileIndex-1, heightMap[heightMapTileIndex-continentMapResolution], continents, continentId, new_edge_cells);

						if(x < m_xContMapSize-1)
							CheckIfTileBelongsToLandContinent(continentMapTileIndex+1, heightMap[heightMapTileIndex+continentMapResolution], continents, continentId, new_edge_cells);

						if(y > 0)
							CheckIfTileBelongsToLandContinent(continentMapTileIndex-m_xContMapSize, heightMap[heightMapTileIndex-continentMapResolution*xMapSize], continents, continentId, new_edge_cells);					

						if(y < m_yContMapSize-1)
							CheckIfTileBelongsToLandContinent(continentMapTileIndex+m_xContMapSize, heightMap[heightMapTileIndex+continentMapResolution*xMapSize], continents, continentId, new_edge_cells);
					}

					old_edge_cells->clear();

					// invert pointers to new/old edge cells
					if(new_edge_cells == &edgeTiles1)
					{
						new_edge_cells = &edgeTiles2;
						old_edge_cells = &edgeTiles1;
					}
					else
					{
						new_edge_cells = &edgeTiles1;
						old_edge_cells = &edgeTiles2;
					}
				}

				// finished adding continent
				++continentId;
				old_edge_cells->clear();
				new_edge_cells->clear();
			}
		}
	}

	// water continents
	for(int i = 0; i < m_xContMapSize; i += 1)
	{
		for(int j = 0; j < m_yContMapSize; j += 1)
		{
			// add new continent if cell has not been visited yet
			if(m_continentMap[j * m_xContMapSize + i] < 0)
			{
				AAIContinent newContinent(continentId, 1, true);
				continents.push_back(newContinent);

				m_continentMap[j * m_xContMapSize + i] = continentId;

				old_edge_cells->push_back(j * m_xContMapSize + i);

				// check edges of the continent as long as new cells have been added to the continent during the last loop
				while(old_edge_cells->size() > 0)
				{
					for(auto cell : *old_edge_cells)
					{
						// get cell indizes
						const int x = cell%m_xContMapSize;
						const int y = (cell - x) / m_xContMapSize;

						const int continentMapTileIndex = y * m_xContMapSize + x;
						const int heightMapTileIndex    = 4 * (y * xMapSize + x);

						// check edges
						if(x > 0)
							CheckIfTileBelongsToSeaContinent(continentMapTileIndex-1, heightMap[heightMapTileIndex-continentMapResolution], continents, continentId, new_edge_cells);

						if(x < m_xContMapSize-1)
							CheckIfTileBelongsToSeaContinent(continentMapTileIndex+1, heightMap[heightMapTileIndex+continentMapResolution], continents, continentId, new_edge_cells);

						if(y > 0)
							CheckIfTileBelongsToSeaContinent(continentMapTileIndex-m_xContMapSize, heightMap[heightMapTileIndex-continentMapResolution*xMapSize], continents, continentId, new_edge_cells);					

						if(y < m_yContMapSize-1)
							CheckIfTileBelongsToSeaContinent(continentMapTileIndex+m_xContMapSize, heightMap[heightMapTileIndex+continentMapResolution*xMapSize], continents, continentId, new_edge_cells);
					}

					old_edge_cells->clear();

					// invert pointers to new/old edge cells
					if(new_edge_cells == &edgeTiles1)
					{
						new_edge_cells = &edgeTiles2;
						old_edge_cells = &edgeTiles1;
					}
					else
					{
						new_edge_cells = &edgeTiles1;
						old_edge_cells = &edgeTiles2;
					}
				}

				// finished adding continent
				++continentId;
				old_edge_cells->clear();
				new_edge_cells->clear();
			}
		}
	}
}