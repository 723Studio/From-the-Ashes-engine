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
#include "XenoSpecies.h"

#include <cmath>
#include <limits>
#include "../Engine/Exception.h"
#include "../Mod/RuleXenoSpecies.h"

namespace OpenXcom
{

XenoSpecies::XenoSpecies(const RuleXenoSpecies* rules) : _rules(rules)
{
	if (!_rules)
	{
		throw Exception("Cannot create xeno-species campaign state without rules");
	}
}

const std::string& XenoSpecies::getType() const
{
	return _rules->getType();
}

double XenoSpecies::getMilestoneProgress(const std::string& milestone) const
{
	auto found = _milestoneProgress.find(milestone);
	return found == _milestoneProgress.end() ? 0.0 : found->second;
}

XenoSpeciesProgressResult XenoSpecies::addProgress(const std::string& milestone, double amount)
{
	XenoSpeciesProgressResult result;
	const RuleXenoSpeciesMilestone* rules = _rules->getMilestone(milestone);
	if (!rules)
	{
		throw Exception("Xeno species '" + getType() + "': milestone '" + milestone +
			"' does not exist");
	}
	if (!std::isfinite(amount) || amount < 0.0)
	{
		throw Exception("Xeno species '" + getType() + "', milestone '" + milestone +
			"': progress increment must be finite and non-negative");
	}
	const double oldProgress = getMilestoneProgress(milestone);
	if (amount > std::numeric_limits<double>::max() - oldProgress)
	{
		throw Exception("Xeno species '" + getType() + "', milestone '" + milestone +
			"': progress overflow");
	}
	const double newProgress = oldProgress + amount;
	_milestoneProgress[milestone] = newProgress;
	if (oldProgress < rules->getCost() && newProgress >= rules->getCost())
	{
		result.completedMilestones.push_back(rules);
	}
	return result;
}

void XenoSpecies::load(const YAML::YamlNodeReader& reader)
{
	reader.tryRead("milestoneProgress", _milestoneProgress);
	for (const auto& progress : _milestoneProgress)
	{
		if (!_rules->getMilestone(progress.first))
		{
			throw Exception("Saved xeno species '" + getType() + "': milestoneProgress key '" +
				progress.first + "' does not exist in the ruleset");
		}
		if (!std::isfinite(progress.second) || progress.second < 0.0)
		{
			throw Exception("Saved xeno species '" + getType() + "', milestone '" +
				progress.first + "': progress must be finite and non-negative");
		}
	}
}

void XenoSpecies::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("type", getType());
	if (!_milestoneProgress.empty())
	{
		writer.write("milestoneProgress", _milestoneProgress);
	}
}

}
