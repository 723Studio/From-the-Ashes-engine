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
#include <map>
#include <string>
#include <vector>
#include "../Engine/Yaml.h"

namespace OpenXcom
{

class RuleXenoSpecies;
class RuleXenoSpeciesMilestone;

struct XenoSpeciesProgressResult
{
	std::vector<const RuleXenoSpeciesMilestone*> completedMilestones;
};

/**
 * Campaign-global organizational knowledge about one alien species.
 */
class XenoSpecies
{
private:
	const RuleXenoSpecies* _rules;
	std::map<std::string, double> _milestoneProgress;

public:
	explicit XenoSpecies(const RuleXenoSpecies* rules);

	const RuleXenoSpecies* getRules() const { return _rules; }
	const std::string& getType() const;
	double getMilestoneProgress(const std::string& milestone) const;
	XenoSpeciesProgressResult addProgress(const std::string& milestone, double amount);

	void load(const YAML::YamlNodeReader& reader);
	void save(YAML::YamlNodeWriter writer) const;
};

}
