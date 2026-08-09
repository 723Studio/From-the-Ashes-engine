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
#include "RuleContainedMonster.h"

#include <cmath>
#include <set>
#include "../Engine/Exception.h"
#include "Mod.h"
#include "RuleItem.h"
#include "RuleResearch.h"
#include "RuleSoldier.h"

namespace OpenXcom
{

void RuleContainedMonsterProject::loadProject(const YAML::YamlNodeReader& reader)
{
	reader.tryRead("cost", _cost);
	_stats.merge(reader["stats"].readVal(_stats));
}

void RuleContainedMonsterProject::validateProject(const std::string& context) const
{
	if (_cost <= 0)
	{
		throw Exception(context + ": cost must be greater than 0");
	}
	if (_stats.empty())
	{
		throw Exception(context + ": stats must define at least one scientist stat");
	}
}

void RuleContainedMonsterStudy::load(const YAML::YamlNodeReader& reader)
{
	loadProject(reader);
}

void RuleContainedMonsterStudy::afterLoad(const std::string& context) const
{
	validateProject(context);
}

void RuleContainedMonsterVivisection::load(const YAML::YamlNodeReader& reader, Mod* mod,
	const std::string& parent)
{
	loadProject(reader);
	mod->loadUnorderedNames(parent, _requiredResearchNames, reader["requiredResearch"]);
	mod->loadUnorderedNamesToInt(parent, _producedItemNames, reader["producedItems"]);
}

void RuleContainedMonsterVivisection::afterLoad(const Mod* mod, const std::string& context)
{
	validateProject(context);
	_requiredResearch = mod->getResearch(_requiredResearchNames);
	_producedItems.clear();
	if (_producedItemNames.empty())
	{
		throw Exception(context + ": producedItems must define at least one item");
	}
	for (const auto& produced : _producedItemNames)
	{
		if (produced.second <= 0)
		{
			throw Exception(context + ": producedItems['" + produced.first +
				"'] must be greater than 0");
		}
		_producedItems.emplace(mod->getItem(produced.first, true), produced.second);
	}
}

void RuleContainedMonsterTaming::load(const YAML::YamlNodeReader& reader, Mod* mod,
	const std::string& parent)
{
	loadProject(reader);
	mod->loadUnorderedNames(parent, _requiredResearchNames, reader["requiredResearch"]);
	reader.tryRead("soldier", _soldierName);
}

void RuleContainedMonsterTaming::afterLoad(const Mod* mod, const std::string& context)
{
	validateProject(context);
	_requiredResearch = mod->getResearch(_requiredResearchNames);
	if (Mod::isEmptyRuleName(_soldierName))
	{
		throw Exception(context + ": soldier must name an existing RuleSoldier");
	}
	_soldier = mod->getSoldier(_soldierName, false);
	if (!_soldier)
	{
		throw Exception(context + ": soldier '" + _soldierName + "' does not exist");
	}
}

RuleContainedMonster::RuleContainedMonster(const std::string& type) : _type(type)
{
}

void RuleContainedMonster::load(const YAML::YamlNodeReader& node, Mod* mod)
{
	const auto& reader = node.useIndex();
	if (const auto& parent = reader["refNode"])
	{
		load(parent, mod);
	}

	reader.tryRead("type", _type);
	mod->loadNameNull(_type, _corpseItemName, reader["corpseItem"]);
	mod->loadUnorderedNames(_type, _unlockedResearchNames, reader["unlockedResearches"]);
	reader.tryRead("healthDegradationPercent", _healthDegradationPercent);
	if (reader["study"])
	{
		_study.load(reader["study"]);
	}
	if (reader["vivisection"])
	{
		_hasVivisection = true;
		_vivisection.load(reader["vivisection"], mod, _type + ".vivisection");
	}
	if (reader["taming"])
	{
		_hasTaming = true;
		_taming.load(reader["taming"], mod, _type + ".taming");
	}
}

void RuleContainedMonster::afterLoad(const Mod* mod)
{
	const std::string context = "Contained monster '" + _type + "'";
	if (_type.empty())
	{
		throw Exception("Contained monster type must not be empty");
	}
	if (!Mod::isEmptyRuleName(_corpseItemName))
	{
		_corpseItem = mod->getItem(_corpseItemName, false);
		if (!_corpseItem)
		{
			throw Exception(context + ": corpseItem '" + _corpseItemName + "' does not exist");
		}
	}
	else
	{
		_corpseItem = nullptr;
	}
	if (!std::isfinite(_healthDegradationPercent) || _healthDegradationPercent < 0.0)
	{
		throw Exception(context + ": healthDegradationPercent must be finite and non-negative");
	}

	_unlockedResearches = mod->getResearch(_unlockedResearchNames);
	std::set<const RuleResearch*> uniqueResearch;
	for (const auto* research : _unlockedResearches)
	{
		if (!uniqueResearch.insert(research).second)
		{
			throw Exception(context + ": unlockedResearches contains duplicate '" +
				research->getName() + "'");
		}
	}

	_study.afterLoad(context + ".study");
	if (_hasVivisection)
	{
		_vivisection.afterLoad(mod, context + ".vivisection");
	}
	if (_hasTaming)
	{
		_taming.afterLoad(mod, context + ".taming");
	}
}

}
