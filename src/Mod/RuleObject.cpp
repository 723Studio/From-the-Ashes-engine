/*
 * Copyright 2010-2021 OpenXcom Developers.
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
#include "RuleObject.h"
#include <algorithm>
#include "../Engine/Exception.h"
#include "Mod.h"

namespace OpenXcom
{

RuleObject::RuleObject(const std::string& type)
	: _type(type), _hackingDefence(0), _samplingDefence(0), _targetAltMCD(-1), _alterationRadius(0), _isMissionObjective(false), _useType(BATTLE_OBJECT_HACKING_TERMINAL)
{
}

RuleObject::~RuleObject()
{
	for (std::vector<std::pair<size_t, WeightedOptions*> >::iterator i = _eventWeights.begin(); i != _eventWeights.end(); ++i)
	{
		delete i->second;
	}
}

/**
	* Loads the event definition from YAML.
	* @param node YAML node.
	*/
void RuleObject::load(const YAML::YamlNodeReader& node)
{
	loadInternal(node);
}

void RuleObject::afterLoad(const Mod* mod)
{
	std::sort(
		_eventWeights.begin(),
		_eventWeights.end(),
		[](const std::pair<size_t, WeightedOptions*>& lhs, const std::pair<size_t, WeightedOptions*>& rhs)
		{
			return lhs.first < rhs.first;
		});
	validate(mod);
}

void RuleObject::loadInternal(const YAML::YamlNodeReader& node)
{
	const auto& reader = node.useIndex();
	if (const YAML::YamlNodeReader& parent = reader["refNode"])
	{
		loadInternal(parent);
	}
	reader.tryRead("type", _type);
	reader.tryRead("useType", _useType);
	reader.tryRead("hackingDefence", _hackingDefence);
	reader.tryRead("samplingDefence", _samplingDefence);
	reader.tryRead("isMissionObjective", _isMissionObjective);
	reader.tryRead("spawnedEvents", _spawnedEvents);
	reader.tryRead("spawnedItem", _spawnedItem);
	reader.tryRead("targetAltMCD", _targetAltMCD);
	reader.tryRead("alterationRadius", _alterationRadius);
	if (reader["eventWeights"])
	{
		for (const auto& child : reader["eventWeights"].children())
		{
			WeightedOptions* options = new WeightedOptions();
			options->load(child);
			size_t month = 0;
			child.tryReadKey(month);

			const auto existing = std::find_if(
				_eventWeights.begin(),
				_eventWeights.end(),
				[month](const std::pair<size_t, WeightedOptions*>& entry)
				{
					return entry.first == month;
				});
			if (existing != _eventWeights.end())
			{
				delete existing->second;
				existing->second = options;
			}
			else
			{
				_eventWeights.push_back(std::make_pair(month, options));
			}
		}
	}
}

void RuleObject::validate(const Mod* mod) const
{
	if (_type.empty())
	{
		throw Exception("Battle object type must not be empty.");
	}
	if (_useType < BATTLE_OBJECT_HACKING_TERMINAL || _useType > BATTLE_OBJECT_ANOMALY_SAMPLING)
	{
		throw Exception("Battle object '" + _type + "': useType must be 0, 1, or 2.");
	}
	if (_useType == BATTLE_OBJECT_HACKING_TERMINAL)
	{
		if (_hackingDefence <= 0)
		{
			throw Exception("Hacking object '" + _type + "': hackingDefence must be greater than 0.");
		}
		if (_samplingDefence != 0)
		{
			throw Exception("Hacking object '" + _type +
				"': samplingDefence is only valid for biology or anomaly sampling objects.");
		}
	}
	else
	{
		if (_samplingDefence <= 0)
		{
			throw Exception("Sampling object '" + _type + "': samplingDefence must be greater than 0.");
		}
		if (_hackingDefence != 0)
		{
			throw Exception("Sampling object '" + _type +
				"': hackingDefence is only valid for hacking objects.");
		}
	}
	if (_useType != BATTLE_OBJECT_HACKING_TERMINAL)
	{
		if (_spawnedItem.empty() && _spawnedEvents.empty())
		{
			throw Exception("Sampling object '" + _type +
				"': define at least one reward in spawnedItem or spawnedEvents.");
		}
		if (_isMissionObjective)
		{
			throw Exception("Sampling object '" + _type +
				"': isMissionObjective is only supported for hacking objectives; " +
				"mark the spawned sample item as missionObjective instead.");
		}
	}
	if (_alterationRadius < 0)
	{
		throw Exception("Battle object '" + _type + "': alterationRadius must not be negative.");
	}
	if (_targetAltMCD < -1)
	{
		throw Exception("Battle object '" + _type + "': targetAltMCD must not be negative.");
	}
	if (_alterationRadius > 0 && _targetAltMCD == -1)
	{
		throw Exception("Battle object '" + _type + "' has alterationRadius " + std::to_string(_alterationRadius) +
			" but does not define targetAltMCD. Define targetAltMCD explicitly (0 is valid).");
	}
	for (size_t i = 0; i < _spawnedEvents.size(); ++i)
	{
		const std::string& eventId = _spawnedEvents[i];
		if (eventId.empty())
		{
			throw Exception("Battle object '" + _type + "': spawnedEvents must not contain an empty event ID.");
		}
		if (std::find(_spawnedEvents.begin(), _spawnedEvents.begin() + i, eventId) != _spawnedEvents.begin() + i)
		{
			throw Exception("Battle object '" + _type + "': spawnedEvents contains duplicate event ID '" + eventId + "'.");
		}
		if (!mod->getEvent(eventId))
		{
			throw Exception("Battle object '" + _type + "': spawnedEvents references unknown event '" + eventId + "'.");
		}
	}
	if (!_spawnedItem.empty() && !mod->getItem(_spawnedItem))
	{
		throw Exception("Battle object '" + _type + "': spawnedItem references unknown item '" + _spawnedItem + "'.");
	}
	if (!_eventWeights.empty() && _eventWeights.front().first != 0)
	{
		throw Exception("Battle object '" + _type + "': eventWeights must define a month 0 fallback.");
	}
	for (const auto& eventPool : _eventWeights)
	{
		for (const std::string& eventId : eventPool.second->getNames())
		{
			if (!mod->getEvent(eventId))
			{
				throw Exception("Battle object '" + _type + "': eventWeights month " +
					std::to_string(eventPool.first) + " references unknown event '" + eventId + "'.");
			}
		}
	}
}

std::string RuleObject::getWeightedEvent(const size_t monthsPassed) const
{
	if (_eventWeights.empty())
		return std::string();

	auto selected = std::upper_bound(
		_eventWeights.begin(),
		_eventWeights.end(),
		monthsPassed,
		[](size_t month, const std::pair<size_t, WeightedOptions*>& entry)
		{
			return month < entry.first;
		});
	if (selected == _eventWeights.begin())
	{
		return std::string();
	}
	--selected;
	return selected->second->choose();
}

}
