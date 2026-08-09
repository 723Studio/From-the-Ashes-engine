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

class Mod;
class RuleResearch;

class RuleXenoSpeciesMilestone
{
private:
	std::string _type;
	double _cost = 0.0;
	std::vector<std::string> _unlockResearchNames;
	std::vector<const RuleResearch*> _unlockResearches;

public:
	void load(const YAML::YamlNodeReader& reader, const std::string& speciesType);
	void afterLoad(const Mod* mod, const std::string& speciesType);

	const std::string& getType() const { return _type; }
	double getCost() const { return _cost; }
	const std::vector<const RuleResearch*>& getUnlockResearches() const { return _unlockResearches; }
};

/**
 * Ruleset constants shared by every contained specimen of one alien species.
 */
class RuleXenoSpecies
{
private:
	std::string _type;
	std::vector<RuleXenoSpeciesMilestone*> _speciesMilestones;
	std::map<std::string, RuleXenoSpeciesMilestone*> _milestonesByType;

public:
	explicit RuleXenoSpecies(const std::string& type);
	~RuleXenoSpecies();

	void load(const YAML::YamlNodeReader& reader, Mod* mod);
	void afterLoad(const Mod* mod);

	const std::string& getType() const { return _type; }
	const std::vector<RuleXenoSpeciesMilestone*>& getSpeciesMilestones() const { return _speciesMilestones; }
	const RuleXenoSpeciesMilestone* getMilestone(const std::string& type) const;
};

}
