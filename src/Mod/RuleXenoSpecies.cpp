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
#include "RuleXenoSpecies.h"

#include <cmath>
#include <set>
#include "../Engine/Exception.h"
#include "Mod.h"
#include "RuleResearch.h"

namespace OpenXcom
{

namespace
{

std::string milestoneContext(const std::string& species, const std::string& milestone)
{
	return "Xeno species '" + species + "', milestone '" + milestone + "'";
}

}

void RuleXenoSpeciesMilestone::load(const YAML::YamlNodeReader& reader, const std::string& speciesType)
{
	reader.tryRead("type", _type);
	reader.tryRead("cost", _cost);
	reader.tryRead("unlockResearches", _unlockResearchNames);

	if (_type.empty())
	{
		throw Exception("Xeno species '" + speciesType + "': milestone type must not be empty");
	}
	if (!std::isfinite(_cost) || _cost <= 0.0)
	{
		throw Exception(milestoneContext(speciesType, _type) +
			": cost must be finite and greater than 0");
	}
	std::set<std::string> unique;
	for (const auto& research : _unlockResearchNames)
	{
		if (research.empty())
		{
			throw Exception(milestoneContext(speciesType, _type) +
				": unlockResearches must not contain an empty id");
		}
		if (!unique.insert(research).second)
		{
			throw Exception(milestoneContext(speciesType, _type) +
				": unlockResearches contains duplicate research '" + research + "'");
		}
	}
}

void RuleXenoSpeciesMilestone::afterLoad(const Mod* mod, const std::string& speciesType)
{
	_unlockResearches.clear();
	_unlockResearches.reserve(_unlockResearchNames.size());
	for (const auto& researchName : _unlockResearchNames)
	{
		const RuleResearch* research = mod->getResearch(researchName, false);
		if (!research)
		{
			throw Exception(milestoneContext(speciesType, _type) +
				": unlockResearches research '" + researchName + "' does not exist");
		}
		_unlockResearches.push_back(research);
	}
}

RuleXenoSpecies::RuleXenoSpecies(const std::string& type) : _type(type)
{
}

RuleXenoSpecies::~RuleXenoSpecies()
{
	for (auto* milestone : _speciesMilestones)
	{
		delete milestone;
	}
}

void RuleXenoSpecies::load(const YAML::YamlNodeReader& node, Mod* mod)
{
	const auto& reader = node.useIndex();
	if (const auto& parent = reader["refNode"])
	{
		load(parent, mod);
	}
	reader.tryRead("type", _type);
	if (reader["speciesMilestones"])
	{
		for (auto* milestone : _speciesMilestones)
		{
			delete milestone;
		}
		_speciesMilestones.clear();
		for (const auto& milestoneReader : reader["speciesMilestones"].children())
		{
			auto* milestone = new RuleXenoSpeciesMilestone();
			milestone->load(milestoneReader, _type);
			_speciesMilestones.push_back(milestone);
		}
	}
}

void RuleXenoSpecies::afterLoad(const Mod* mod)
{
	if (_type.empty())
	{
		throw Exception("Xeno species type must not be empty");
	}
	if (_speciesMilestones.empty())
	{
		throw Exception("Xeno species '" + _type + "': speciesMilestones must contain at least one milestone");
	}
	_milestonesByType.clear();
	for (auto* milestone : _speciesMilestones)
	{
		if (!_milestonesByType.emplace(milestone->getType(), milestone).second)
		{
			throw Exception("Xeno species '" + _type + "': duplicate milestone type '" +
				milestone->getType() + "'");
		}
		milestone->afterLoad(mod, _type);
	}
}

const RuleXenoSpeciesMilestone* RuleXenoSpecies::getMilestone(const std::string& type) const
{
	auto found = _milestonesByType.find(type);
	return found == _milestonesByType.end() ? nullptr : found->second;
}

}
