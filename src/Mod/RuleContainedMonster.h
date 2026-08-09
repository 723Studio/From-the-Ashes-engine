#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include <map>
#include <string>
#include <vector>
#include "../Engine/Yaml.h"
#include "Unit.h"

namespace OpenXcom
{

class Mod;
class RuleItem;
class RuleResearch;
class RuleSoldier;

/**
 * Cost and scientist-stat requirements shared by contained-monster projects.
 */
class RuleContainedMonsterProject
{
protected:
	int _cost = 0;
	UnitStats _stats;

	void loadProject(const YAML::YamlNodeReader& reader);
	void validateProject(const std::string& context) const;

public:
	int getCost() const { return _cost; }
	const UnitStats& getStats() const { return _stats; }
};

class RuleContainedMonsterStudy final : public RuleContainedMonsterProject
{
public:
	void load(const YAML::YamlNodeReader& reader);
	void afterLoad(const std::string& context) const;
};

class RuleContainedMonsterVivisection final : public RuleContainedMonsterProject
{
private:
	std::vector<std::string> _requiredResearchNames;
	std::vector<const RuleResearch*> _requiredResearch;
	std::map<std::string, int> _producedItemNames;
	std::map<const RuleItem*, int> _producedItems;

public:
	void load(const YAML::YamlNodeReader& reader, Mod* mod, const std::string& parent);
	void afterLoad(const Mod* mod, const std::string& context);

	const std::vector<const RuleResearch*>& getRequiredResearch() const { return _requiredResearch; }
	const std::map<const RuleItem*, int>& getProducedItems() const { return _producedItems; }
};

class RuleContainedMonsterTaming final : public RuleContainedMonsterProject
{
private:
	std::vector<std::string> _requiredResearchNames;
	std::vector<const RuleResearch*> _requiredResearch;
	std::string _soldierName;
	const RuleSoldier* _soldier = nullptr;

public:
	void load(const YAML::YamlNodeReader& reader, Mod* mod, const std::string& parent);
	void afterLoad(const Mod* mod, const std::string& context);

	const std::vector<const RuleResearch*>& getRequiredResearch() const { return _requiredResearch; }
	const RuleSoldier* getSoldier() const { return _soldier; }
};

/**
 * Ruleset capture profile for one kind of living monster specimen.
 */
class RuleContainedMonster
{
private:
	std::string _type;
	std::string _corpseItemName;
	const RuleItem* _corpseItem = nullptr;
	std::vector<std::string> _unlockedResearchNames;
	std::vector<const RuleResearch*> _unlockedResearches;
	double _healthDegradationPercent = 5.0;
	RuleContainedMonsterStudy _study;
	bool _hasVivisection = false;
	RuleContainedMonsterVivisection _vivisection;
	bool _hasTaming = false;
	RuleContainedMonsterTaming _taming;

public:
	explicit RuleContainedMonster(const std::string& type);

	void load(const YAML::YamlNodeReader& reader, Mod* mod);
	void afterLoad(const Mod* mod);

	const std::string& getType() const { return _type; }
	const RuleItem* getCorpseItem() const { return _corpseItem; }
	const std::vector<const RuleResearch*>& getUnlockedResearches() const { return _unlockedResearches; }
	double getHealthDegradationPercent() const { return _healthDegradationPercent; }
	const RuleContainedMonsterStudy& getStudy() const { return _study; }
	const RuleContainedMonsterVivisection* getVivisection() const
	{
		return _hasVivisection ? &_vivisection : nullptr;
	}
	const RuleContainedMonsterTaming* getTaming() const
	{
		return _hasTaming ? &_taming : nullptr;
	}
};

}
