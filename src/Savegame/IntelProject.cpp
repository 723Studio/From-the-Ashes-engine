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
#include "IntelProject.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include "../Engine/Exception.h"
#include "../Engine/Logger.h"
#include "../Engine/RNG.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleIntelProject.h"
#include "Craft.h"
#include "SavedGame.h"
#include "Soldier.h"

namespace OpenXcom
{

namespace
{

constexpr float PROGRESS_LIMIT_POOR = 0.1f;
constexpr float PROGRESS_LIMIT_AVERAGE = 0.2f;
constexpr float PROGRESS_LIMIT_GOOD = 0.3f;
constexpr float PROGRESS_LIMIT_GREAT = 0.4f;
constexpr float PROGRESS_LIMIT_SUPERIOR = 0.5f;
constexpr int64_t MAX_SAVED_PROGRESS = std::numeric_limits<int64_t>::max() -
	static_cast<int64_t>(std::numeric_limits<int>::max());
constexpr int64_t MAX_SAVED_EVENT_CYCLES = std::numeric_limits<int64_t>::max() - 1;

void checkedAdd(int64_t& value, int increment, const std::string& project, const char* field)
{
	if (increment < 0 || value > MAX_SAVED_PROGRESS - increment)
	{
		throw Exception("Intel project '" + project + "': " + field + " overflow");
	}
	value += increment;
}

bool hasDisabledResearch(const SavedGame* save, const std::vector<const RuleResearch*>& research)
{
	for (const auto* rule : research)
	{
		if (save->isResearched(rule, false))
		{
			return true;
		}
	}
	return false;
}

}

IntelProject::IntelProject(const RuleIntelProject* rule) : _rules(rule)
{
	if (!_rules)
	{
		throw Exception("Cannot create an intelligence project without rules");
	}
	if (_rules->getFirstMilestone())
	{
		_currentMilestone = _rules->getFirstMilestone()->getId();
	}
}

int IntelProject::getStepProgress(std::map<Soldier*, int>& assignedAgents, const Mod* mod, int rating,
	std::string& description, bool estimate, std::map<Soldier*, UnitStats>* dailyExperience,
	std::vector<Soldier*>* contributors)
{
	description.clear();
	if (assignedAgents.empty() || _completed)
	{
		return 0;
	}

	double effort = 0.0;
	size_t contributingAgents = 0;
	const UnitStats projectStats = _rules->getStats();
	const int trainingFactor = mod->getIntelTrainingFactor();
	const double speedFactor = static_cast<double>(mod->getIntelSpeedFactor()) / 100.0;

	for (const auto& assigned : assignedAgents)
	{
		Soldier* agent = assigned.first;
		if (!agent || agent->getRoleRank(ROLE_AGENT) < 1)
		{
			continue;
		}
		if (agent->getCraft() && agent->getCraft()->getStatus() == "STR_OUT")
		{
			continue;
		}
		if (contributors)
		{
			contributors->push_back(agent);
		}
		const bool canTrain = !estimate && agent->canGainExperience();

		const UnitStats* stats = agent->getCurrentStats();
		const UnitStats caps = agent->getRules()->getStatCaps();
		unsigned int statsCount = 0;
		double soldierEffort = 0.0;

#define INTEL_STAT(field) \
		if (projectStats.field > 0) \
		{ \
			soldierEffort += static_cast<double>(stats->field) / projectStats.field; \
			if (canTrain && dailyExperience && stats->field < caps.field && \
				RNG::generate(0, caps.field) > stats->field && RNG::percent(trainingFactor) && \
				RNG::percent(assigned.second)) \
			{ \
				auto inserted = dailyExperience->emplace(agent, UnitStats::scalar(0)); \
				inserted.first->second.field++; \
			} \
			statsCount++; \
		}

		INTEL_STAT(data)
		INTEL_STAT(computers)
		INTEL_STAT(xenolinguistics)
		INTEL_STAT(hacking)
		INTEL_STAT(alienTech)
		INTEL_STAT(investigation)

#undef INTEL_STAT

		double insightBonus = estimate ? stats->insight / 4.0 : RNG::generate(0, stats->insight / 2);
		soldierEffort += insightBonus / 10.0;
		soldierEffort /= statsCount + 1;
		effort += soldierEffort;
		contributingAgents++;
	}

	if (contributingAgents == 0)
	{
		return 0;
	}
	if (contributingAgents > 1)
	{
		effort *= std::max(0.0, (100.0 - 19.0 * std::log(static_cast<double>(contributingAgents))) / 100.0);
	}
	effort *= static_cast<double>(rating) / 100.0;
	effort *= speedFactor * 24.0;

	const double roundedEffort = std::ceil(std::max(0.0, effort));
	if (roundedEffort > std::numeric_limits<int>::max())
	{
		throw Exception("Intel project '" + getName() + "': daily effort exceeds integer range");
	}
	return static_cast<int>(roundedEffort);
}

bool IntelProject::isCurrentMilestoneBlocked(const SavedGame* save) const
{
	if (_currentMilestone.empty())
	{
		return false;
	}
	const RuleIntelMilestone* milestone = _rules->getMilestone(_currentMilestone);
	if (!milestone)
	{
		throw Exception("Intel project '" + getName() + "': current milestone '" +
			_currentMilestone + "' does not exist");
	}
	return !save->isResearched(milestone->getRequiredResearch(), false);
}

std::vector<const RuleIntelEventOutput*> IntelProject::getAvailableEventOutputs(const SavedGame* save) const
{
	std::vector<const RuleIntelEventOutput*> result;
	const RuleIntelEventCycle* cycle = _rules->getEventCycle();
	if (!cycle)
	{
		return result;
	}

	if (_eventCycles >= MAX_SAVED_EVENT_CYCLES)
	{
		throw Exception("Intel project '" + getName() + "': eventCycles overflow");
	}
	const int64_t upcomingCycle = _eventCycles + 1;
	for (const auto* output : cycle->getOutputs())
	{
		if (output->getMinCycles() > upcomingCycle)
		{
			continue;
		}
		if (!save->isResearched(output->getRequiredResearch(), false))
		{
			continue;
		}
		if (hasDisabledResearch(save, output->getDisabledByResearch()))
		{
			continue;
		}
		result.push_back(output);
	}
	return result;
}

IntelProjectDailyResult IntelProject::processDay(SavedGame* save, int progress)
{
	IntelProjectDailyResult result;
	if (_completed || progress <= 0 || isCurrentMilestoneBlocked(save))
	{
		return result;
	}

	if (!_currentMilestone.empty() && !isCurrentMilestoneBlocked(save))
	{
		const RuleIntelMilestone* milestone = _rules->getMilestone(_currentMilestone);
		checkedAdd(_milestoneProgress, progress, getName(), "milestoneProgress");
		if (_milestoneProgress >= milestone->getCost())
		{
			_milestoneProgress -= milestone->getCost();
			result.completedMilestone = milestone;
			result.eventScripts.insert(result.eventScripts.end(),
				milestone->getEventScripts().begin(), milestone->getEventScripts().end());
			if (milestone->getSpawnMission())
			{
				result.spawnMissions.push_back(milestone->getSpawnMission());
			}

			if (milestone->completesProject())
			{
				result.completed = true;
				return result;
			}

			const RuleIntelMilestone* next = _rules->getNextMilestone(milestone->getId());
			_currentMilestone = next ? next->getId() : std::string();
			if (!next)
			{
				_milestoneProgress = 0;
			}
			else if (isCurrentMilestoneBlocked(save))
			{
				// A newly reached gate stops the whole mixed project immediately,
				// including its event cycle, until the required evidence exists.
				return result;
			}
		}
	}

	const RuleIntelEventCycle* cycle = _rules->getEventCycle();
	if (!cycle)
	{
		return result;
	}

	checkedAdd(_eventProgress, progress, getName(), "eventProgress");
	if (_eventProgress < cycle->getCost())
	{
		return result;
	}

	const auto outputs = getAvailableEventOutputs(save);
	if (outputs.empty())
	{
		_eventProgress = cycle->getCost();
		return result;
	}

	int totalWeight = 0;
	for (const auto* output : outputs)
	{
		totalWeight += output->getWeight();
	}
	int roll = RNG::generate(1, totalWeight);
	const RuleIntelEventOutput* selected = outputs.back();
	for (const auto* output : outputs)
	{
		roll -= output->getWeight();
		if (roll <= 0)
		{
			selected = output;
			break;
		}
	}

	_eventProgress -= cycle->getCost();
	if (_eventCycles >= MAX_SAVED_EVENT_CYCLES)
	{
		throw Exception("Intel project '" + getName() + "': eventCycles overflow");
	}
	++_eventCycles;
	if (!selected->getEventScript().empty())
	{
		result.eventScripts.push_back(selected->getEventScript());
	}
	else
	{
		result.spawnMissions.push_back(selected->getSpawnMission());
	}
	return result;
}

std::string IntelProject::getProgressState(int dailyProgress, const SavedGame* save) const
{
	if (_completed || dailyProgress <= 0)
	{
		return "STR_NONE";
	}
	if (!_currentMilestone.empty())
	{
		if (isCurrentMilestoneBlocked(save))
		{
			return "STR_NONE";
		}
		return getState(dailyProgress, _rules->getMilestone(_currentMilestone)->getCost());
	}
	if (_rules->getEventCycle())
	{
		if (getAvailableEventOutputs(save).empty())
		{
			return "STR_NONE";
		}
		return getState(dailyProgress, _rules->getEventCycle()->getCost());
	}
	return "STR_NONE";
}

std::string IntelProject::getState(int progress, int cost) const
{
	if (progress <= 0 || cost <= 0)
	{
		return "STR_NONE";
	}

	const float rating = static_cast<float>(progress) / static_cast<float>(cost);
	if (rating <= PROGRESS_LIMIT_POOR)
		return "STR_POOR";
	if (rating <= PROGRESS_LIMIT_AVERAGE)
		return "STR_AVERAGE";
	if (rating <= PROGRESS_LIMIT_GOOD)
		return "STR_GOOD";
	if (rating <= PROGRESS_LIMIT_GREAT)
		return "STR_GREAT";
	if (rating <= PROGRESS_LIMIT_SUPERIOR)
		return "STR_SUPERIOR";
	return "STR_EXCELLENT";
}

const std::string& IntelProject::getName() const
{
	return _rules->getName();
}

void IntelProject::complete()
{
	_completed = true;
	_currentMilestone.clear();
	_blockedMilestone.clear();
	_milestoneProgress = 0;
	_eventProgress = 0;
	_eventCycles = 0;
}

void IntelProject::markCurrentMilestoneBlocked()
{
	if (_currentMilestone.empty())
	{
		throw Exception("Intel project '" + getName() +
			"': cannot mark a project without a current milestone as blocked");
	}
	_blockedMilestone = _currentMilestone;
}

void IntelProject::load(const YAML::YamlNodeReader& reader)
{
	reader.tryRead("completed", _completed);
	reader.tryRead("currentMilestone", _currentMilestone);
	reader.tryRead("blockedMilestone", _blockedMilestone);
	reader.tryRead("milestoneProgress", _milestoneProgress);
	reader.tryRead("eventProgress", _eventProgress);
	reader.tryRead("eventCycles", _eventCycles);

	if (_milestoneProgress < 0)
	{
		throw Exception("Intel project '" + getName() + "': milestoneProgress must be non-negative");
	}
	if (_milestoneProgress > MAX_SAVED_PROGRESS)
	{
		throw Exception("Intel project '" + getName() +
			"': milestoneProgress is too large to safely advance");
	}
	if (_eventProgress < 0)
	{
		throw Exception("Intel project '" + getName() + "': eventProgress must be non-negative");
	}
	if (_eventProgress > MAX_SAVED_PROGRESS)
	{
		throw Exception("Intel project '" + getName() +
			"': eventProgress is too large to safely advance");
	}
	if (_eventCycles < 0)
	{
		throw Exception("Intel project '" + getName() + "': eventCycles must be non-negative");
	}
	if (_eventCycles > MAX_SAVED_EVENT_CYCLES)
	{
		throw Exception("Intel project '" + getName() +
			"': eventCycles is too large to safely advance");
	}
	if (!_currentMilestone.empty() && !_rules->getMilestone(_currentMilestone))
	{
		throw Exception("Intel project '" + getName() + "': saved currentMilestone '" +
			_currentMilestone + "' does not exist");
	}
	if (_currentMilestone.empty() && _milestoneProgress != 0)
	{
		throw Exception("Intel project '" + getName() +
			"': save contains milestoneProgress without a currentMilestone");
	}
	if (!_blockedMilestone.empty() && _blockedMilestone != _currentMilestone)
	{
		throw Exception("Intel project '" + getName() + "': saved blockedMilestone '" +
			_blockedMilestone + "' does not match currentMilestone '" + _currentMilestone + "'");
	}
	if (!_rules->getEventCycle() && (_eventProgress != 0 || _eventCycles != 0))
	{
		throw Exception("Intel project '" + getName() +
			"': save contains event cycle state but the project has no eventCycle");
	}
	if (_completed && (!_currentMilestone.empty() || !_blockedMilestone.empty() ||
		_milestoneProgress != 0 || _eventProgress != 0 || _eventCycles != 0))
	{
		throw Exception("Intel project '" + getName() +
			"': completed project contains active progress state");
	}
}

void IntelProject::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("name", getName());
	if (_completed)
	{
		writer.write("completed", true);
	}
	writer.write("currentMilestone", _currentMilestone);
	if (!_blockedMilestone.empty())
	{
		writer.write("blockedMilestone", _blockedMilestone);
	}
	if (_milestoneProgress != 0)
	{
		writer.write("milestoneProgress", _milestoneProgress);
	}
	if (_eventProgress != 0)
	{
		writer.write("eventProgress", _eventProgress);
	}
	if (_eventCycles != 0)
	{
		writer.write("eventCycles", _eventCycles);
	}
}

}
