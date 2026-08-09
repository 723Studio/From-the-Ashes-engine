/*
 * Copyright 2010-2022 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "RuleIntelProject.h"

#include <algorithm>
#include <limits>
#include <set>
#include <sstream>
#include "../Engine/Exception.h"
#include "Mod.h"
#include "RuleAlienMission.h"
#include "RuleEventScript.h"
#include "RuleResearch.h"

namespace OpenXcom
{

namespace
{

std::string milestoneContext(const std::string& project, const std::string& milestone)
{
	return "Intel project '" + project + "', milestone '" + milestone + "'";
}

std::string eventCycleContext(const std::string& project)
{
	return "Intel project '" + project + "', eventCycle";
}

void validateRuleNames(const std::vector<std::string>& names, const std::string& context,
	const std::string& field)
{
	std::set<std::string> unique;
	for (const auto& name : names)
	{
		if (name.empty())
		{
			throw Exception(context + ": " + field + " must not contain empty ids");
		}
		if (!unique.insert(name).second)
		{
			throw Exception(context + ": " + field + " contains duplicate id '" + name + "'");
		}
	}
}

void validateEventScript(const Mod* mod, const std::string& project, const std::string& source,
	const std::string& name, const RuleEventScript*& result)
{
	result = mod->getEventScript(name, false);
	if (!result)
	{
		throw Exception("Intel project '" + project + "', " + source +
			": eventScript '" + name + "' does not exist");
	}
	if (result->getAllowedProcessor() != 3)
	{
		throw Exception("Intel project '" + project + "', " + source +
			": eventScript '" + name + "' must allow processor 3");
	}
}

void linkResearchRules(const Mod* mod, const std::vector<std::string>& names,
	std::vector<const RuleResearch*>& result, const std::string& context, const std::string& field)
{
	result.clear();
	result.reserve(names.size());
	for (const auto& name : names)
	{
		const RuleResearch* research = mod->getResearch(name, false);
		if (!research)
		{
			throw Exception(context + ": " + field + " research '" + name + "' does not exist");
		}
		result.push_back(research);
	}
}

const RuleAlienMission* linkDirectMission(const Mod* mod, const std::string& project,
	const std::string& source, const std::string& name)
{
	const RuleAlienMission* mission = mod->getAlienMission(name, false);
	if (!mission)
	{
		throw Exception("Intel project '" + project + "', " + source +
			": spawnMission '" + name + "' does not exist");
	}
	mission->validateDirectSpawn();
	return mission;
}

}

void RuleIntelMilestone::load(const YAML::YamlNodeReader& reader, const std::string& projectName)
{
	reader.tryRead("id", _id);
	reader.tryRead("previousMilestone", _previousMilestone);
	reader.tryRead("cost", _cost);
	reader.tryRead("requiredResearch", _requiredResearchNames);
	reader.tryRead("eventScripts", _eventScriptNames);
	reader.tryRead("spawnMission", _spawnMissionName);
	reader.tryRead("unlockResearches", _unlockResearchNames);
	reader.tryRead("completesProject", _completesProject);
	validateRuleNames(_requiredResearchNames, milestoneContext(projectName, _id), "requiredResearch");
	validateRuleNames(_eventScriptNames, milestoneContext(projectName, _id), "eventScripts");
	validateRuleNames(_unlockResearchNames, milestoneContext(projectName, _id), "unlockResearches");

	if (_id.empty())
	{
		throw Exception("Intel project '" + projectName + "': milestone id must not be empty");
	}
	if (_cost <= 0)
	{
		throw Exception(milestoneContext(projectName, _id) +
			": cost must be greater than 0, got " + std::to_string(_cost));
	}
	if (_previousMilestone == _id)
	{
		throw Exception(milestoneContext(projectName, _id) + ": previousMilestone must not reference itself");
	}
}

void RuleIntelMilestone::afterLoad(const Mod* mod, const std::string& projectName)
{
	const std::string context = milestoneContext(projectName, _id);
	linkResearchRules(mod, _requiredResearchNames, _requiredResearch, context, "requiredResearch");
	linkResearchRules(mod, _unlockResearchNames, _unlockResearches, context, "unlockResearches");
	_eventScripts.clear();
	_eventScripts.reserve(_eventScriptNames.size());
	for (const auto& name : _eventScriptNames)
	{
		const RuleEventScript* script = nullptr;
		validateEventScript(mod, projectName, "milestone '" + _id + "'", name, script);
		_eventScripts.push_back(script);
	}
	if (!_spawnMissionName.empty())
	{
		_spawnMission = linkDirectMission(mod, projectName, "milestone '" + _id + "'", _spawnMissionName);
	}
}

void RuleIntelEventOutput::load(const YAML::YamlNodeReader& reader, const std::string& projectName)
{
	reader.tryRead("eventScript", _eventScriptName);
	reader.tryRead("spawnMission", _spawnMissionName);
	reader.tryRead("weight", _weight);
	reader.tryRead("minCycles", _minCycles);
	reader.tryRead("requiredResearch", _requiredResearchNames);
	reader.tryRead("disabledByResearch", _disabledByResearchNames);
	const std::string outputName = !_eventScriptName.empty() ? _eventScriptName : _spawnMissionName;
	const std::string outputType = !_eventScriptName.empty() ? "eventScript" : "spawnMission";
	const std::string context = eventCycleContext(projectName) + ", " + outputType + " '" + outputName + "'";
	validateRuleNames(_requiredResearchNames, context, "requiredResearch");
	validateRuleNames(_disabledByResearchNames, context, "disabledByResearch");
	for (const auto& research : _requiredResearchNames)
	{
		if (std::find(_disabledByResearchNames.begin(), _disabledByResearchNames.end(), research) !=
			_disabledByResearchNames.end())
		{
			throw Exception(context + ": research '" + research +
				"' cannot be both required and disabled");
		}
	}

	if (_eventScriptName.empty() == _spawnMissionName.empty())
	{
		throw Exception(eventCycleContext(projectName) +
			": each output must define exactly one of eventScript or spawnMission");
	}
	if (_weight <= 0)
	{
		throw Exception(context +
			": weight must be greater than 0, got " + std::to_string(_weight));
	}
	if (_minCycles < 0)
	{
		throw Exception(context +
			": minCycles must be non-negative, got " + std::to_string(_minCycles));
	}
}

void RuleIntelEventOutput::afterLoad(const Mod* mod, const std::string& projectName)
{
	const std::string outputName = !_eventScriptName.empty() ? _eventScriptName : _spawnMissionName;
	const std::string outputType = !_eventScriptName.empty() ? "eventScript" : "spawnMission";
	const std::string context = eventCycleContext(projectName) +
		", " + outputType + " '" + outputName + "'";
	linkResearchRules(mod, _requiredResearchNames, _requiredResearch, context, "requiredResearch");
	linkResearchRules(mod, _disabledByResearchNames, _disabledByResearch, context, "disabledByResearch");
	if (!_eventScriptName.empty())
	{
		validateEventScript(mod, projectName, "eventCycle output", _eventScriptName, _eventScript);
	}
	else
	{
		_spawnMission = linkDirectMission(mod, projectName, "eventCycle output", _spawnMissionName);
	}
}

RuleIntelEventCycle::~RuleIntelEventCycle()
{
	for (auto* output : _outputs)
	{
		delete output;
	}
}

void RuleIntelEventCycle::load(const YAML::YamlNodeReader& reader, const std::string& projectName)
{
	reader.tryRead("cost", _cost);
	if (_cost <= 0)
	{
		throw Exception(eventCycleContext(projectName) +
			": cost must be greater than 0, got " + std::to_string(_cost));
	}

	for (auto* output : _outputs)
	{
		delete output;
	}
	_outputs.clear();

	for (const auto& outputReader : reader["outputs"].children())
	{
		auto* output = new RuleIntelEventOutput();
		output->load(outputReader, projectName);
		_outputs.push_back(output);
	}
	if (_outputs.empty())
	{
		throw Exception(eventCycleContext(projectName) + ": outputs must contain at least one entry");
	}
	int64_t totalWeight = 0;
	for (const auto* output : _outputs)
	{
		totalWeight += output->getWeight();
	}
	if (totalWeight > std::numeric_limits<int>::max())
	{
		throw Exception(eventCycleContext(projectName) + ": total output weight exceeds integer range");
	}
}

void RuleIntelEventCycle::afterLoad(const Mod* mod, const std::string& projectName)
{
	std::set<std::string> results;
	for (auto* output : _outputs)
	{
		output->afterLoad(mod, projectName);
		const bool isScript = !output->getEventScript().empty();
		const std::string& name = isScript ? output->getEventScript() : output->getSpawnMissionName();
		const std::string key = (isScript ? "eventScript:" : "spawnMission:") + name;
		if (!results.insert(key).second)
		{
			throw Exception(eventCycleContext(projectName) + ": duplicate " +
				(isScript ? "eventScript" : "spawnMission") + " '" + name + "'");
		}
	}
}

RuleIntelProject::RuleIntelProject(const std::string& name, int listOrder) :
	_name(name), _listOrder(listOrder)
{
}

RuleIntelProject::~RuleIntelProject()
{
	for (auto* milestone : _milestones)
	{
		delete milestone;
	}
	delete _eventCycle;
}

void RuleIntelProject::load(const YAML::YamlNodeReader& node, Mod* mod)
{
	const auto& reader = node.useIndex();
	if (const YAML::YamlNodeReader& parent = reader["refNode"])
	{
		load(parent, mod);
	}

	reader.tryRead("name", _name);
	reader.tryRead("description", _description);
	reader.tryRead("requiredResearch", _requiredResearchNames);
	validateRuleNames(_requiredResearchNames, "Intel project '" + _name + "'", "requiredResearch");

	int effectType = static_cast<int>(_effectType);
	reader.tryRead("effectType", effectType);
	if (effectType < INTEL_EFFECT_NONE || effectType > INTEL_EFFECT_DEPLOYMENT_HINTS)
	{
		throw Exception("Intel project '" + _name + "': unknown effectType " + std::to_string(effectType));
	}
	_effectType = static_cast<IntelProjectEffectType>(effectType);

	reader.tryRead("stats", _stats);
	reader.tryRead("listOrder", _listOrder);

	if (reader["milestones"])
	{
		for (auto* milestone : _milestones)
		{
			delete milestone;
		}
		_milestones.clear();

		for (const auto& milestoneReader : reader["milestones"].children())
		{
			auto* milestone = new RuleIntelMilestone();
			milestone->load(milestoneReader, _name);
			_milestones.push_back(milestone);
		}
		if (_milestones.empty())
		{
			throw Exception("Intel project '" + _name + "': milestones must not be an empty list");
		}
	}

	if (reader["eventCycle"])
	{
		delete _eventCycle;
		_eventCycle = new RuleIntelEventCycle();
		_eventCycle->load(reader["eventCycle"], _name);
	}

}

void RuleIntelProject::validateStats() const
{
	const UnitStats::Ptr supported[] =
	{
		&UnitStats::data,
		&UnitStats::computers,
		&UnitStats::xenolinguistics,
		&UnitStats::hacking,
		&UnitStats::alienTech,
		&UnitStats::investigation
	};

	bool hasPositive = false;
	UnitStats::fieldLoop([&](UnitStats::Ptr stat)
	{
		const int value = _stats.*stat;
		if (value < 0)
		{
			throw Exception("Intel project '" + _name + "': stat '" +
				UnitStats::getStatString(stat) + "' must be non-negative");
		}
		const bool isSupported = std::find(std::begin(supported), std::end(supported), stat) != std::end(supported);
		if (value > 0 && !isSupported)
		{
			throw Exception("Intel project '" + _name + "': stat '" +
				UnitStats::getStatString(stat) + "' is not supported by intelligence projects");
		}
		if (value > 0 && isSupported)
		{
			hasPositive = true;
		}
	});
	if (!hasPositive)
	{
		throw Exception("Intel project '" + _name +
			"': stats must contain at least one positive supported value");
	}
}

void RuleIntelProject::afterLoad(const Mod* mod)
{
	if (_name.empty())
	{
		throw Exception("Intel project name must not be empty");
	}
	validateStats();
	if (_effectType == INTEL_EFFECT_NONE && _milestones.empty() && !_eventCycle)
	{
		throw Exception("Intel project '" + _name +
			"': at least one of effectType, milestones or eventCycle must be defined");
	}
	linkResearchRules(mod, _requiredResearchNames, _requiredResearch,
		"Intel project '" + _name + "'", "requiredResearch");
	if (_stats.alienTech > 0)
	{
		const std::string& unlock = mod->getAlienTechUnlockResearch();
		if (unlock.empty() || !mod->getResearch(unlock, false))
		{
			throw Exception("Intel project '" + _name +
				"': alienTech is used but alienTechUnlockResearch is empty or invalid");
		}
	}
	if (_stats.xenolinguistics > 0)
	{
		const std::string& unlock = mod->getXenolinguisticsUnlockResearch();
		if (unlock.empty() || !mod->getResearch(unlock, false))
		{
			throw Exception("Intel project '" + _name +
				"': xenolinguistics is used but xenolinguisticsUnlockResearch is empty or invalid");
		}
	}

	for (auto* milestone : _milestones)
	{
		milestone->afterLoad(mod, _name);
	}
	if (_eventCycle)
	{
		_eventCycle->afterLoad(mod, _name);
	}

	buildMilestoneChain();
}

void RuleIntelProject::buildMilestoneChain()
{
	_milestonesById.clear();
	_nextMilestones.clear();
	_firstMilestone = nullptr;

	for (auto* milestone : _milestones)
	{
		if (!_milestonesById.emplace(milestone->getId(), milestone).second)
		{
			throw Exception("Intel project '" + _name + "': duplicate milestone id '" +
				milestone->getId() + "'");
		}
	}

	for (auto* milestone : _milestones)
	{
		const std::string& previous = milestone->getPreviousMilestone();
		if (previous.empty())
		{
			if (_firstMilestone)
			{
				throw Exception("Intel project '" + _name +
					"': milestone chain must contain exactly one first milestone; found '" +
					_firstMilestone->getId() + "' and '" + milestone->getId() + "'");
			}
			_firstMilestone = milestone;
			continue;
		}

		if (_milestonesById.find(previous) == _milestonesById.end())
		{
			throw Exception(milestoneContext(_name, milestone->getId()) +
				": previousMilestone '" + previous + "' does not exist");
		}
		auto inserted = _nextMilestones.emplace(previous, milestone);
		if (!inserted.second)
		{
			throw Exception("Intel project '" + _name + "': milestones '" +
				inserted.first->second->getId() + "' and '" + milestone->getId() +
				"' both follow milestone '" + previous + "'; branching is not supported");
		}
	}

	if (_milestones.empty())
	{
		return;
	}
	if (!_firstMilestone)
	{
		throw Exception("Intel project '" + _name +
			"': milestone chain has no first milestone and contains a cycle");
	}

	std::set<std::string> visited;
	const RuleIntelMilestone* current = _firstMilestone;
	const RuleIntelMilestone* terminal = nullptr;
	while (current)
	{
		if (!visited.insert(current->getId()).second)
		{
			throw Exception("Intel project '" + _name +
				"': milestone chain contains a cycle involving '" + current->getId() + "'");
		}
		terminal = current;
		current = getNextMilestone(current->getId());
	}

	if (visited.size() != _milestones.size())
	{
		for (const auto& entry : _milestonesById)
		{
			if (visited.find(entry.first) == visited.end())
			{
				throw Exception("Intel project '" + _name + "': milestone '" + entry.first +
					"' is not reachable from first milestone '" + _firstMilestone->getId() + "'");
			}
		}
	}

	for (auto* milestone : _milestones)
	{
		if (milestone->completesProject() && milestone != terminal)
		{
			throw Exception(milestoneContext(_name, milestone->getId()) +
				": completesProject is only valid on the final milestone");
		}
	}

	if (_effectType == INTEL_EFFECT_NONE && !_eventCycle && !terminal->completesProject())
	{
		throw Exception("Intel project '" + _name +
			"': the final milestone must complete a project without effectType or eventCycle");
	}
}

const RuleIntelMilestone* RuleIntelProject::getMilestone(const std::string& id) const
{
	auto found = _milestonesById.find(id);
	return found == _milestonesById.end() ? nullptr : found->second;
}

const RuleIntelMilestone* RuleIntelProject::getNextMilestone(const std::string& id) const
{
	auto found = _nextMilestones.find(id);
	return found == _nextMilestones.end() ? nullptr : found->second;
}

}
