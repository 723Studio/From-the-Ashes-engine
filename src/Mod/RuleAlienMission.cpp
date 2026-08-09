/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "RuleAlienMission.h"
#include "Mod.h"
#include "../Engine/Exception.h"
#include "../Savegame/WeightedOptions.h"

namespace OpenXcom
{

RuleAlienMission::RuleAlienMission(const std::string &type) :
	_type(type), _skipScoutingPhase(false), _points(0), _objective(OBJECTIVE_SCORE), _spawnZone(-1),
	_retaliationOdds(-1), _multiUfoRetaliation(false), _multiUfoRetaliationExtra(false),
	_ignoreBaseDefenses(false), _instaHyper(false),
	_despawnEvenIfTargeted(false), _respawnUfoAfterSiteDespawn(false), _showAlienBase(false),
	_operationType(AMOT_SPACE), _operationSpawnZone(-1),
	_targetBaseOdds(0)
{
}

/**
 * Ensures the allocated memory is released.
 */
RuleAlienMission::~RuleAlienMission()
{
	for (auto& pair : _raceDistribution)
	{
		delete pair.second;
	}
	for (auto& pair : _regionWeights)
	{
		delete pair.second;
	}
}

/**
 * Loads the mission data from a YAML node.
 * @param node YAML node.
 */
void RuleAlienMission::load(const YAML::YamlNodeReader& node)
{
	const auto& reader = node.useIndex();
	if (const auto& parent = reader["refNode"])
	{
		load(parent);
	}

	reader.tryRead("points", _points);
	reader.tryRead("waves", _waves);
	reader.tryRead("objective", _objective);
	reader.tryRead("spawnUfo", _spawnUfo);
	reader.tryRead("skipScoutingPhase", _skipScoutingPhase);
	reader.tryRead("spawnZone", _spawnZone);
	reader.tryRead("missionWeights", _weights);
	reader.tryRead("retaliationOdds", _retaliationOdds);
	reader.tryRead("multiUfoRetaliation", _multiUfoRetaliation);
	reader.tryRead("multiUfoRetaliationExtra", _multiUfoRetaliationExtra);
	if (_multiUfoRetaliationExtra)
	{
		// yes, I should have changed _multiUfoRetaliation to int instead, but I'm lazy
		// I also don't want to break existing mods or handle it in a complicated way
		_multiUfoRetaliation = true;
	}
	reader.tryRead("ignoreBaseDefenses", _ignoreBaseDefenses);
	reader.tryRead("instaHyper", _instaHyper);
	reader.tryRead("despawnEvenIfTargeted", _despawnEvenIfTargeted);
	reader.tryRead("respawnUfoAfterSiteDespawn", _respawnUfoAfterSiteDespawn);
	reader.tryRead("showAlienBase", _showAlienBase);
	reader.tryRead("interruptResearch", _interruptResearch);
	reader.tryRead("siteType", _siteType);
	reader.tryRead("operationType", _operationType);
	reader.tryRead("operationSpawnZone", _operationSpawnZone);
	reader.tryRead("operationBaseType", _operationBaseType);
	reader.tryRead("targetBaseOdds", _targetBaseOdds);

	typedef std::map<size_t, WeightedOptions*> Associative;
	typedef std::vector< std::pair<size_t, WeightedOptions*> > Linear;
	auto mergeDistribution = [](Linear& distribution, const YAML::YamlNodeReader& weights)
	{
		Associative assoc;
		//Place in the associative container so we can index by month and keep entries sorted.
		for (Linear::iterator ii = distribution.begin(); ii != distribution.end(); ++ii)
		{
			assoc.insert(*ii);
		}

		// Now go through the node contents and merge with existing data.
		for (const auto& nn : weights.children())
		{
			size_t month = nn.readKey<size_t>();
			Associative::iterator existing = assoc.find(month);
			if (assoc.end() == existing)
			{
				// New entry, load and add it.
				WeightedOptions *nw = new WeightedOptions();
				nw->load(nn);
				assoc.insert(std::make_pair(month, nw));
			}
			else
			{
				// Existing entry, update it.
				existing->second->load(nn);
			}
		}

		// Now replace values in our actual member variable!
		distribution.clear();
		distribution.reserve(assoc.size());
		for (Associative::iterator ii = assoc.begin(); ii != assoc.end(); ++ii)
		{
			if (ii->second->empty())
			{
				// Don't keep empty lists.
				delete ii->second;
			}
			else
			{
				// Place it
				distribution.push_back(*ii);
			}
		}
	};

	if (const auto& weights = reader["regionWeights"])
	{
		mergeDistribution(_regionWeights, weights);
	}
	if (const auto& weights = reader["raceWeights"])
	{
		mergeDistribution(_raceDistribution, weights);
	}
}

/**
 * Validates race references after all ruleset merges are complete.
 */
void RuleAlienMission::afterLoad(const Mod* mod)
{
	if (_spawnZone < -1)
	{
		throw Exception("Alien mission '" + _type + "' has invalid spawnZone: expected -1 or a non-negative zone index");
	}

	for (const auto& distribution : _raceDistribution)
	{
		for (const auto& race : distribution.second->getNames())
		{
			if (!mod->getAlienRace(race))
			{
				throw Exception("Alien mission '" + _type + "' has undefined race '" + race + "' in raceWeights");
			}
		}
	}
	for (const auto& distribution : _regionWeights)
	{
		for (const auto& region : distribution.second->getNames())
		{
			if (!mod->getRegion(region))
			{
				throw Exception("Alien mission '" + _type + "' has undefined region '" + region + "' in regionWeights");
			}
		}
	}
}

/**
 * Ensures that a mission spawned without a RuleMissionScript or alien base
 * can select a race at every point in the campaign.
 */
void RuleAlienMission::validateDirectSpawn() const
{
	if (_raceDistribution.empty() || _raceDistribution.front().first != 0)
	{
		throw Exception("Alien mission '" + _type + "' is used for direct spawning and must define raceWeights starting at month 0");
	}
	if (!_regionWeights.empty() && _regionWeights.front().first != 0)
	{
		throw Exception("Alien mission '" + _type + "' is used for direct spawning and its regionWeights must start at month 0");
	}
}

/**
 * @return if this mission uses a weighted distribution to pick a race.
 */
bool RuleAlienMission::hasRaceWeights() const
{
	return !_raceDistribution.empty();
}

/**
 * Chooses one of the available races for this mission.
 * The racial distribution may vary based on the current game date.
 * @param monthsPassed The number of months that have passed in the game world.
 * @return The string id of the race.
 */
std::string RuleAlienMission::generateRace(const size_t monthsPassed) const
{
	std::vector<std::pair<size_t, WeightedOptions*> >::const_reverse_iterator rc;
	for (rc = _raceDistribution.rbegin(); rc != _raceDistribution.rend() && monthsPassed < rc->first; ++rc);
	if (rc == _raceDistribution.rend())
		return "";
	return rc->second->choose();
}

/**
 * Returns the Alien score for this mission.
 * @return Amount of points.
 */
int RuleAlienMission::getPoints() const
{
	return _points;
}

/**
 * Returns the chances of this mission being generated based on the current game date.
 * @param monthsPassed The number of months that have passed in the game world.
 * @return The weight.
 */
int RuleAlienMission::getWeight(const size_t monthsPassed) const
{
	if (_weights.empty())
	{
		return 1;
	}
	int weight = 0;
	for (auto& pair : _weights)
	{
		if (pair.first > monthsPassed)
		{
			break;
		}
		weight = pair.second;
	}
	return weight;
}

/**
 * Returns the Alien score for this mission.
 * @return Amount of points.
 */
int RuleAlienMission::getRetaliationOdds() const
{
	return _retaliationOdds;
}

/**
 * Does this mission have region weights?
 * @return if this mission uses a weighted distribution to pick a region.
 */
bool RuleAlienMission::hasRegionWeights() const
{
	return !_regionWeights.empty();
}

/**
 * Chooses one of the available regions for this mission.
 * The region distribution may vary based on the current game date.
 * @param monthsPassed The number of months that have passed in the game world.
 * @return The string id of the region.
 */
std::string RuleAlienMission::generateRegion(const size_t monthsPassed) const
{
	std::vector<std::pair<size_t, WeightedOptions*> >::const_reverse_iterator rc = _regionWeights.rbegin();
	while (monthsPassed < rc->first)
		++rc;
	return rc->second->choose();
}

// helper overloads for deserialization-only
bool read(ryml::ConstNodeRef const& n, MissionWave* val)
{
	YAML::YamlNodeReader reader(n);
	reader.tryRead("ufo", val->ufoType);
	reader.tryRead("count", val->ufoCount);
	reader.tryRead("trajectory", val->trajectory);
	reader.tryRead("timer", val->spawnTimer);
	reader.readNode("objective", val->objective, false);
	reader.readNode("objectiveOnTheLandingSite", val->objectiveOnTheLandingSite, false);
	reader.readNode("objectiveOnXcomBase", val->objectiveOnXcomBase, false);
	reader.readNode("hunterKillerPercentage", val->hunterKillerPercentage, -1);
	reader.readNode("huntMode", val->huntMode, -1);
	reader.readNode("huntBehavior", val->huntBehavior, -1);
	reader.readNode("escort", val->escort, false);
	reader.readNode("interruptPercentage", val->interruptPercentage, 0);

	return true;
}

}
