#pragma once
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

#include <string>
#include <vector>
#include "Position.h"

namespace OpenXcom
{

class BattleUnit;
class Mod;
class Node;
class SavedBattleGame;

enum SpawnBlockSelection
{
	SBS_ALL,
	SBS_DIRECTIONS,
	SBS_MARKED,
	SBS_MARKED_OR_DIRECTIONS,
	SBS_MARKED_AND_DIRECTIONS
};

struct SpawnLocationOptions
{
	SpawnBlockSelection blockSelection = SBS_ALL;
	std::vector<std::string> directions;
	std::vector<int> groups;
	std::vector<int> nodeRanks;
	std::vector<int> zLevels;
	bool useZLevelRange = false;
	int minZLevel = 0;
	int maxZLevel = 0;
	const std::vector<std::vector<int> > *markedBlocks = nullptr;
	bool useNodes = false;
	bool filterNodesByBlocks = true;
	bool randomizeZLevels = true;
	int minDistanceFromPlayerUnits = 0;
	int maxDistanceFromMapEdge = 0;
};

struct SpawnLocationCandidates
{
	std::vector<std::vector<int> > blockMask;
	std::vector<Position> blocks;
	std::vector<Node*> nodes;
};

struct SpawnLocationResult
{
	Position position = Position(-1, -1, -1);
	Node *node = nullptr;
};

/**
 * Builds and validates neutral Battlescape spawn-location candidates.
 */
class SpawnLocationResolver
{
private:
	SavedBattleGame *_save;
	const Mod *_mod;

	bool matchesGroups(int blockX, int blockY, const std::vector<int> &groups) const;
	bool isFarEnoughFromPlayerUnits(const Position &position, int minimumDistance) const;
	bool isNearMapEdge(const Position &position, int maximumDistance) const;
	bool blockIntersectsMapEdgeBand(int blockX, int blockY, int maximumDistance) const;
	bool isAllowedZLevel(int z, const SpawnLocationOptions &options) const;
	bool isAllowedBlock(const Position &position, const std::vector<std::vector<int> > &blockMask) const;
	bool isAllowedFinalPosition(const BattleUnit *unit, const Position &position, const SpawnLocationOptions &options, const std::vector<std::vector<int> > &blockMask, bool enforceBlockMask) const;
	void clearTemporaryPosition(BattleUnit *unit) const;
	bool tryPosition(BattleUnit *unit, const Position &position, const SpawnLocationOptions &options, const std::vector<std::vector<int> > &blockMask, bool enforceBlockMask, Position &resolvedPosition) const;

public:
	SpawnLocationResolver(SavedBattleGame *save, const Mod *mod);

	static std::vector<std::vector<int> > buildBlockMask(
		int sizeX,
		int sizeY,
		SpawnBlockSelection selection,
		const std::vector<std::string> &directions,
		const std::vector<std::vector<int> > *markedBlocks);

	SpawnLocationCandidates buildCandidates(const SpawnLocationOptions &options) const;
	bool findPosition(BattleUnit *unit, const SpawnLocationOptions &options, const SpawnLocationCandidates &candidates, int &remainingPositionChecks, SpawnLocationResult &result) const;
};

}
