/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SpawnLocationResolver.h"

#include <algorithm>
#include "Pathfinding.h"
#include "TileEngine.h"
#include "../Engine/Logger.h"
#include "../Engine/RNG.h"
#include "../Mod/Armor.h"
#include "../Mod/MapBlock.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleTerrain.h"
#include "../Mod/RuleUfo.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/Node.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/Tile.h"

namespace OpenXcom
{

SpawnLocationResolver::SpawnLocationResolver(SavedBattleGame *save, const Mod *mod) :
	_save(save), _mod(mod)
{
}

std::vector<std::vector<int> > SpawnLocationResolver::buildBlockMask(
	int sizeX,
	int sizeY,
	SpawnBlockSelection selection,
	const std::vector<std::string> &directions,
	const std::vector<std::vector<int> > *markedBlocks)
{
	if (sizeX <= 0 || sizeY <= 0)
	{
		return {};
	}

	std::vector<std::vector<int> > directionMask(sizeX, std::vector<int>(sizeY, directions.empty() ? 1 : 0));
	if (!directions.empty())
	{
		for (const auto &direction : directions)
		{
			if (direction == "EDGES")
			{
				for (int x = 0; x < sizeX; ++x)
				{
					directionMask[x][0] = 1;
					directionMask[x][sizeY - 1] = 1;
				}
				for (int y = 0; y < sizeY; ++y)
				{
					directionMask[0][y] = 1;
					directionMask[sizeX - 1][y] = 1;
				}
				continue;
			}
			if (direction == "NORTH")
			{
				for (int x = 0; x < sizeX; ++x) directionMask[x][0] = 1;
			}
			else if (direction == "WEST")
			{
				for (int y = 0; y < sizeY; ++y) directionMask[0][y] = 1;
			}
			else if (direction == "SOUTH")
			{
				for (int x = 0; x < sizeX; ++x) directionMask[x][sizeY - 1] = 1;
			}
			else if (direction == "EAST")
			{
				for (int y = 0; y < sizeY; ++y) directionMask[sizeX - 1][y] = 1;
			}
			else if (direction == "NW") directionMask[0][0] = 1;
			else if (direction == "NE") directionMask[sizeX - 1][0] = 1;
			else if (direction == "SW") directionMask[0][sizeY - 1] = 1;
			else if (direction == "SE") directionMask[sizeX - 1][sizeY - 1] = 1;
		}
	}

	std::vector<std::vector<int> > markedMask(sizeX, std::vector<int>(sizeY, 0));
	if (markedBlocks && static_cast<int>(markedBlocks->size()) == sizeX)
	{
		for (int x = 0; x < sizeX; ++x)
		{
			if (static_cast<int>((*markedBlocks)[x].size()) != sizeY)
			{
				continue;
			}
			for (int y = 0; y < sizeY; ++y)
			{
				markedMask[x][y] = (*markedBlocks)[x][y] > 0 ? 1 : 0;
			}
		}
	}

	std::vector<std::vector<int> > result(sizeX, std::vector<int>(sizeY, 0));
	for (int x = 0; x < sizeX; ++x)
	{
		for (int y = 0; y < sizeY; ++y)
		{
			switch (selection)
			{
			case SBS_ALL:
				result[x][y] = 1;
				break;
			case SBS_DIRECTIONS:
				result[x][y] = directionMask[x][y];
				break;
			case SBS_MARKED:
				result[x][y] = markedMask[x][y];
				break;
			case SBS_MARKED_OR_DIRECTIONS:
				result[x][y] = markedMask[x][y] || directionMask[x][y];
				break;
			case SBS_MARKED_AND_DIRECTIONS:
				result[x][y] = markedMask[x][y] && directionMask[x][y];
				break;
			}
		}
	}
	return result;
}

bool SpawnLocationResolver::matchesGroups(int blockX, int blockY, const std::vector<int> &groups) const
{
	if (groups.empty())
	{
		return true;
	}

	const auto &terrainNames = _save->getFlattenedMapTerrainNames();
	const auto &blockNames = _save->getFlattenedMapBlockNames();
	if (blockX < 0 || blockY < 0 || blockX >= static_cast<int>(terrainNames.size()) || blockX >= static_cast<int>(blockNames.size()) ||
		blockY >= static_cast<int>(terrainNames[blockX].size()) || blockY >= static_cast<int>(blockNames[blockX].size()))
	{
		return false;
	}

	RuleTerrain *terrain = _mod->getTerrain(terrainNames[blockX][blockY], false);
	if (!terrain)
	{
		if (RuleCraft *craft = _mod->getCraft(terrainNames[blockX][blockY], false))
		{
			terrain = craft->getBattlescapeTerrainData();
		}
	}
	if (!terrain)
	{
		if (RuleUfo *ufo = _mod->getUfo(terrainNames[blockX][blockY], false))
		{
			terrain = ufo->getBattlescapeTerrainData();
		}
	}
	if (!terrain)
	{
		Log(LOG_WARNING) << "SpawnLocationResolver rejected block (" << blockX << ", " << blockY
			<< ") because flattened terrain '" << terrainNames[blockX][blockY]
			<< "' could not be resolved while applying groups.";
		return false;
	}

	MapBlock *mapBlock = terrain->getMapBlock(blockNames[blockX][blockY]);
	if (!mapBlock)
	{
		Log(LOG_WARNING) << "SpawnLocationResolver rejected block (" << blockX << ", " << blockY
			<< ") because flattened map block '" << blockNames[blockX][blockY]
			<< "' could not be resolved while applying groups.";
		return false;
	}
	for (int group : groups)
	{
		if (mapBlock->isInGroup(group))
		{
			return true;
		}
	}
	return false;
}

bool SpawnLocationResolver::isFarEnoughFromPlayerUnits(const Position &position, int minimumDistance) const
{
	if (minimumDistance <= 0)
	{
		return true;
	}
	const int minimumDistanceSquared = minimumDistance * minimumDistance;
	for (const auto *unit : *_save->getUnits())
	{
		if (unit->getOriginalFaction() == FACTION_PLAYER && !unit->isOut() &&
			Position::distanceSq(unit->getPosition(), position) < minimumDistanceSquared)
		{
			return false;
		}
	}
	return true;
}

bool SpawnLocationResolver::isNearMapEdge(const Position &position, int maximumDistance) const
{
	if (maximumDistance <= 0)
	{
		return true;
	}
	if (position.x < 0 || position.y < 0 || position.x >= _save->getMapSizeX() || position.y >= _save->getMapSizeY())
	{
		return false;
	}
	return position.x < maximumDistance || position.y < maximumDistance ||
		position.x >= _save->getMapSizeX() - maximumDistance ||
		position.y >= _save->getMapSizeY() - maximumDistance;
}

bool SpawnLocationResolver::blockIntersectsMapEdgeBand(int blockX, int blockY, int maximumDistance) const
{
	if (maximumDistance <= 0)
	{
		return true;
	}
	const int minimumX = blockX * 10;
	const int minimumY = blockY * 10;
	if (minimumX < 0 || minimumY < 0 || minimumX >= _save->getMapSizeX() || minimumY >= _save->getMapSizeY())
	{
		return false;
	}
	const int maximumX = std::min(minimumX + 9, _save->getMapSizeX() - 1);
	const int maximumY = std::min(minimumY + 9, _save->getMapSizeY() - 1);
	return minimumX < maximumDistance || minimumY < maximumDistance ||
		maximumX >= _save->getMapSizeX() - maximumDistance ||
		maximumY >= _save->getMapSizeY() - maximumDistance;
}

bool SpawnLocationResolver::isAllowedZLevel(int z, const SpawnLocationOptions &options) const
{
	if (z < 0 || z >= _save->getMapSizeZ())
	{
		return false;
	}
	if (!options.zLevels.empty())
	{
		return std::find(options.zLevels.begin(), options.zLevels.end(), z) != options.zLevels.end();
	}
	return !options.useZLevelRange || (z >= options.minZLevel && z <= options.maxZLevel);
}

bool SpawnLocationResolver::isAllowedBlock(const Position &position, const std::vector<std::vector<int> > &blockMask) const
{
	if (position.x < 0 || position.y < 0)
	{
		return false;
	}
	const int blockX = position.x / 10;
	const int blockY = position.y / 10;
	return blockX >= 0 && blockY >= 0 && blockX < static_cast<int>(blockMask.size()) &&
		blockY < static_cast<int>(blockMask[blockX].size()) && blockMask[blockX][blockY] != 0;
}

bool SpawnLocationResolver::isAllowedFinalPosition(const BattleUnit *unit, const Position &position, const SpawnLocationOptions &options, const std::vector<std::vector<int> > &blockMask, bool enforceBlockMask) const
{
	if (!isAllowedZLevel(position.z, options))
	{
		return false;
	}
	if (enforceBlockMask)
	{
		const int unitSize = unit->getArmor()->getSize();
		for (int x = 0; x < unitSize; ++x)
		{
			for (int y = 0; y < unitSize; ++y)
			{
				if (!isAllowedBlock(position + Position(x, y, 0), blockMask))
				{
					return false;
				}
			}
		}
	}
	return isFarEnoughFromPlayerUnits(position, options.minDistanceFromPlayerUnits) &&
		isNearMapEdge(position, options.maxDistanceFromMapEdge);
}

void SpawnLocationResolver::clearTemporaryPosition(BattleUnit *unit) const
{
	unit->setTile(nullptr, _save);
	unit->setPosition(TileEngine::invalid, false);
}

bool SpawnLocationResolver::tryPosition(BattleUnit *unit, const Position &position, const SpawnLocationOptions &options, const std::vector<std::vector<int> > &blockMask, bool enforceBlockMask, Position &resolvedPosition) const
{
	Position validatedPosition = position;
	// Candidate iteration belongs to the resolver; substituting a neighbor would break node, block, and budget identity.
	if (!_save->getTileEngine()->isPositionValidForUnit(validatedPosition, unit, false, 0) ||
		!isAllowedFinalPosition(unit, validatedPosition, options, blockMask, enforceBlockMask) ||
		!_save->setUnitPosition(unit, validatedPosition))
	{
		return false;
	}
	resolvedPosition = unit->getPosition();
	if (!isAllowedFinalPosition(unit, resolvedPosition, options, blockMask, enforceBlockMask))
	{
		clearTemporaryPosition(unit);
		return false;
	}
	return true;
}

SpawnLocationCandidates SpawnLocationResolver::buildCandidates(const SpawnLocationOptions &options) const
{
	SpawnLocationCandidates candidates;
	const int sizeX = _save->getMapSizeX() / 10;
	const int sizeY = _save->getMapSizeY() / 10;
	candidates.blockMask = buildBlockMask(sizeX, sizeY, options.blockSelection, options.directions, options.markedBlocks);
	if (candidates.blockMask.empty())
	{
		return candidates;
	}

	for (int x = 0; x < sizeX; ++x)
	{
		for (int y = 0; y < sizeY; ++y)
		{
			if (candidates.blockMask[x][y] &&
				blockIntersectsMapEdgeBand(x, y, options.maxDistanceFromMapEdge) &&
				matchesGroups(x, y, options.groups))
			{
				candidates.blocks.push_back(Position(x, y, 0));
			}
			else
			{
				candidates.blockMask[x][y] = 0;
			}
		}
	}

	if (!options.useNodes)
	{
		return candidates;
	}
	for (Node *node : *_save->getNodes())
	{
		if (!node || node->isDummy())
		{
			continue;
		}
		const Position &position = node->getPosition();
		if (!options.nodeRanks.empty() && std::find(options.nodeRanks.begin(), options.nodeRanks.end(), static_cast<int>(node->getRank())) == options.nodeRanks.end())
		{
			continue;
		}
		if (!isAllowedZLevel(position.z, options))
		{
			continue;
		}
		const int blockX = position.x / 10;
		const int blockY = position.y / 10;
		if (options.filterNodesByBlocks &&
			(blockX < 0 || blockY < 0 || blockX >= sizeX || blockY >= sizeY || candidates.blockMask[blockX][blockY] == 0))
		{
			continue;
		}
		Tile *tile = _save->getTile(position);
		if (!tile || tile->getUnit())
		{
			continue;
		}
		if (!isFarEnoughFromPlayerUnits(position, options.minDistanceFromPlayerUnits) || !isNearMapEdge(position, options.maxDistanceFromMapEdge))
		{
			continue;
		}
		candidates.nodes.push_back(node);
	}
	RNG::shuffle(candidates.nodes);
	return candidates;
}

bool SpawnLocationResolver::findPosition(BattleUnit *unit, const SpawnLocationOptions &options, const SpawnLocationCandidates &candidates, int &remainingPositionChecks, SpawnLocationResult &result) const
{
	for (Node *node : candidates.nodes)
	{
		// Static node flags are compatibility filters, not position checks.
		if ((node->getType() & Node::TYPE_SMALL) && !unit->isSmallUnit())
		{
			continue;
		}
		if ((node->getType() & Node::TYPE_FLYING) && unit->getMovementType() != MT_FLY)
		{
			continue;
		}
		if (remainingPositionChecks <= 0)
		{
			return false;
		}
		--remainingPositionChecks;
		Position resolved;
		if (tryPosition(unit, node->getPosition(), options, candidates.blockMask, options.filterNodesByBlocks, resolved))
		{
			result.position = resolved;
			result.node = node;
			return true;
		}
	}

	if (candidates.blocks.empty())
	{
		return false;
	}

	std::vector<int> zLevels;
	if (!options.zLevels.empty())
	{
		for (int z : options.zLevels)
		{
			if (z >= 0 && z < _save->getMapSizeZ())
			{
				zLevels.push_back(z);
			}
		}
	}
	else
	{
		const int firstLevel = options.useZLevelRange ? options.minZLevel : 0;
		const int lastLevel = options.useZLevelRange ? options.maxZLevel : _save->getMapSizeZ() - 1;
		for (int z = firstLevel; z <= lastLevel; ++z)
		{
			if (z >= 0 && z < _save->getMapSizeZ())
			{
				zLevels.push_back(z);
			}
		}
	}
	if (zLevels.empty())
	{
		return false;
	}
	if (options.randomizeZLevels)
	{
		RNG::shuffle(zLevels);
	}

	// Generate valid X/Y anchors once so edge filtering cannot waste the bounded search budget.
	std::vector<Position> groundPositions;
	for (const Position &block : candidates.blocks)
	{
		for (int localX = 0; localX < 10; ++localX)
		{
			for (int localY = 0; localY < 10; ++localY)
			{
				const Position position(block.x * 10 + localX, block.y * 10 + localY, 0);
				if (position.x >= 0 && position.y >= 0 &&
					position.x < _save->getMapSizeX() && position.y < _save->getMapSizeY() &&
					isNearMapEdge(position, options.maxDistanceFromMapEdge))
				{
					groundPositions.push_back(position);
				}
			}
		}
	}
	RNG::shuffle(groundPositions);

	while (remainingPositionChecks > 0 && !groundPositions.empty())
	{
		const Position groundPosition = groundPositions.back();
		groundPositions.pop_back();

		for (int z : zLevels)
		{
			if (remainingPositionChecks <= 0)
			{
				return false;
			}
			--remainingPositionChecks;
			Position position(groundPosition.x, groundPosition.y, z);
			if (!isFarEnoughFromPlayerUnits(position, options.minDistanceFromPlayerUnits))
			{
				continue;
			}
			if (unit->getMovementType() != MT_FLY)
			{
				Tile *tile = _save->getTile(position);
				if (!tile || tile->getTUCost(O_FLOOR, unit->getMovementType()) == Pathfinding::INVALID_MOVE_COST)
				{
					continue;
				}
			}
			Position resolved;
			if (tryPosition(unit, position, options, candidates.blockMask, true, resolved))
			{
				result.position = resolved;
				result.node = nullptr;
				return true;
			}
		}
	}
	return false;
}

}
