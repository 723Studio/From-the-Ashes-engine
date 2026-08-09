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
#include "RuleContainedAlien.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include "../Engine/Exception.h"
#include "Mod.h"
#include "RuleItem.h"
#include "RuleResearch.h"
#include "RuleXenoSpecies.h"

namespace OpenXcom
{

namespace
{

ContainedAlienStudyCategory parseCategory(const std::string& value,
	const std::string& containedAlien, const std::string& node)
{
	if (value == "LIVE_BIOLOGY") return CONTAINED_STUDY_LIVE_BIOLOGY;
	if (value == "COMMUNICATION") return CONTAINED_STUDY_COMMUNICATION;
	if (value == "TACTICAL_INTELLIGENCE") return CONTAINED_STUDY_TACTICAL_INTELLIGENCE;
	if (value == "TECHNICAL_INTELLIGENCE") return CONTAINED_STUDY_TECHNICAL_INTELLIGENCE;
	if (value == "STRATEGIC_INTELLIGENCE") return CONTAINED_STUDY_STRATEGIC_INTELLIGENCE;
	throw Exception("Contained alien '" + containedAlien + "', study node '" + node +
		"': unknown category '" + value + "'");
}

void validateFiniteNonNegative(double value, const std::string& context, const std::string& field)
{
	if (!std::isfinite(value) || value < 0.0)
	{
		throw Exception(context + ": " + field + " must be finite and non-negative");
	}
}

#ifndef NDEBUG
void runRuleContainedAlienStudyNodeDebugSelfTests()
{
	static const bool tested = []
	{
		const std::string baseNode =
			"type: STR_TEST_STUDY\n"
			"category: LIVE_BIOLOGY\n"
			"cost: 1\n"
			"stabilityCost: 0\n";
		{
			const YAML::YamlRootNodeReader reader(
				YAML::YamlString{baseNode}, "(contained alien study node self-test)");
			RuleContainedAlienStudyNode node;
			node.load(reader, "STR_TEST_CONTAINED_ALIEN");
			assert(!node.hasExplicitRepeatStudyCostPercent());
			assert(node.getRepeatStudyCostPercent() == 100);
		}
		{
			const YAML::YamlRootNodeReader reader(
				YAML::YamlString{baseNode + "repeatStudyCostPercent: 100\n"},
				"(contained alien study node self-test)");
			RuleContainedAlienStudyNode node;
			node.load(reader, "STR_TEST_CONTAINED_ALIEN");
			assert(node.hasExplicitRepeatStudyCostPercent());
			assert(node.getRepeatStudyCostPercent() == 100);
		}
		{
			bool rejected = false;
			try
			{
				const YAML::YamlRootNodeReader reader(
					YAML::YamlString{baseNode + "requiresResearch: []\n"},
					"(contained alien study node self-test)");
				RuleContainedAlienStudyNode node;
				node.load(reader, "STR_TEST_CONTAINED_ALIEN");
			}
			catch (const Exception&)
			{
				rejected = true;
			}
			assert(rejected);
		}
		return true;
	}();
	(void)tested;
}
#endif

}

void RuleContainedAlienStudyNode::load(const YAML::YamlNodeReader& reader,
	const std::string& containedAlienType)
{
	reader.tryRead("type", _type);
	std::string category;
	reader.tryRead("category", category);
	reader.tryRead("cost", _cost);
	reader.tryRead("stabilityCost", _stabilityCost);
	reader.tryRead("speciesProgress", _speciesProgress);
	const bool requiresResearchAuthored =
		reader.tryRead("requiresResearch", _requiredResearchNames);
	_repeatStudyCostPercentAuthored =
		reader.tryRead("repeatStudyCostPercent", _repeatStudyCostPercent);

	const std::string context = "Contained alien '" + containedAlienType +
		"', study node '" + _type + "'";
	if (_type.empty())
	{
		throw Exception("Contained alien '" + containedAlienType +
			"': study node type must not be empty");
	}
	_category = parseCategory(category, containedAlienType, _type);
	if (!std::isfinite(_cost) || _cost <= 0.0)
	{
		throw Exception(context + ": cost must be finite and greater than 0");
	}
	validateFiniteNonNegative(_stabilityCost, context, "stabilityCost");
	if (_repeatStudyCostPercent < 1 || _repeatStudyCostPercent > 100)
	{
		throw Exception(context + ": repeatStudyCostPercent must be within 1..100");
	}
	if (requiresResearchAuthored && _requiredResearchNames.empty())
	{
		throw Exception(context + ": requiresResearch must contain at least one research id");
	}
	std::set<std::string> uniqueResearch;
	for (const auto& research : _requiredResearchNames)
	{
		if (research.empty())
		{
			throw Exception(context + ": requiresResearch must not contain an empty id");
		}
		if (!uniqueResearch.insert(research).second)
		{
			throw Exception(context + ": requiresResearch contains duplicate research '" +
				research + "'");
		}
	}
	for (const auto& progress : _speciesProgress)
	{
		if (progress.first.empty())
		{
			throw Exception(context + ": speciesProgress must not contain an empty milestone key");
		}
		validateFiniteNonNegative(progress.second, context,
			"speciesProgress['" + progress.first + "']");
	}
}

void RuleContainedAlienStudyNode::afterLoad(const Mod* mod,
	const std::string& containedAlienType)
{
	_requiredResearch.clear();
	_requiredResearch.reserve(_requiredResearchNames.size());
	const std::string context = "Contained alien '" + containedAlienType +
		"', study node '" + _type + "'";
	for (const auto& researchName : _requiredResearchNames)
	{
		const RuleResearch* research = mod->getResearch(researchName, false);
		if (!research)
		{
			throw Exception(context + ": requiresResearch research '" + researchName +
				"' does not exist");
		}
		_requiredResearch.push_back(research);
	}
}

RuleContainedAlien::RuleContainedAlien(const std::string& type) : _type(type)
{
#ifndef NDEBUG
	runRuleContainedAlienStudyNodeDebugSelfTests();
#endif
}

RuleContainedAlien::~RuleContainedAlien()
{
	for (auto* node : _studyNodes)
	{
		delete node;
	}
}

void RuleContainedAlien::load(const YAML::YamlNodeReader& node, Mod* mod)
{
	const auto& reader = node.useIndex();
	if (const auto& parent = reader["refNode"])
	{
		load(parent, mod);
	}
	reader.tryRead("type", _type);
	reader.tryRead("xenoSpecies", _xenoSpeciesName);
	reader.tryRead("corpseItem", _corpseItemName);
	reader.tryRead("initialStability", _initialStability);
	reader.tryRead("healthDecayPerDay", _healthDecayPerDay);
	reader.tryRead("stabilityDecayPerDay", _stabilityDecayPerDay);
	reader.tryRead("unstableHealthDamagePerDay", _unstableHealthDamagePerDay);
	reader.tryRead("passiveRecoveryPerDay", _passiveRecoveryPerDay);
	reader.tryRead("passiveRecoveryCap", _passiveRecoveryCap);
	reader.tryRead("fatalWoundStabilityPenalty", _fatalWoundStabilityPenalty);

	if (reader["studyNodes"])
	{
		for (auto* studyNode : _studyNodes)
		{
			delete studyNode;
		}
		_studyNodes.clear();
		for (const auto& studyNodeReader : reader["studyNodes"].children())
		{
			auto* studyNode = new RuleContainedAlienStudyNode();
			studyNode->load(studyNodeReader, _type);
			_studyNodes.push_back(studyNode);
		}
	}
}

void RuleContainedAlien::afterLoad(const Mod* mod)
{
	const std::string context = "Contained alien '" + _type + "'";
	if (_type.empty())
	{
		throw Exception("Contained alien type must not be empty");
	}
	_xenoSpecies = mod->getXenoSpecies(_xenoSpeciesName, false);
	if (!_xenoSpecies)
	{
		throw Exception(context + ": xenoSpecies '" + _xenoSpeciesName + "' does not exist");
	}
	_corpseItem = mod->getItem(_corpseItemName, false);
	if (!_corpseItem)
	{
		throw Exception(context + ": corpseItem '" + _corpseItemName + "' does not exist");
	}
	if (!std::isfinite(_initialStability) || _initialStability < 0.0 || _initialStability > 100.0)
	{
		throw Exception(context + ": initialStability must be within 0..100");
	}
	validateFiniteNonNegative(_healthDecayPerDay, context, "healthDecayPerDay");
	validateFiniteNonNegative(_stabilityDecayPerDay, context, "stabilityDecayPerDay");
	validateFiniteNonNegative(_unstableHealthDamagePerDay, context, "unstableHealthDamagePerDay");
	validateFiniteNonNegative(_passiveRecoveryPerDay, context, "passiveRecoveryPerDay");
	if (!std::isfinite(_passiveRecoveryCap) || _passiveRecoveryCap < 0.0 || _passiveRecoveryCap > 100.0)
	{
		throw Exception(context + ": passiveRecoveryCap must be within 0..100");
	}
	if (_fatalWoundStabilityPenalty < 0 || _fatalWoundStabilityPenalty > 100)
	{
		throw Exception(context + ": fatalWoundStabilityPenalty must be within 0..100");
	}
	if (_studyNodes.empty())
	{
		throw Exception(context + ": studyNodes must contain at least one node");
	}

	_studyNodesByType.clear();
	for (auto* studyNode : _studyNodes)
	{
		studyNode->afterLoad(mod, _type);
		if (!_studyNodesByType.emplace(studyNode->getType(), studyNode).second)
		{
			throw Exception(context + ": duplicate study node type '" + studyNode->getType() + "'");
		}
		for (const auto& progress : studyNode->getSpeciesProgress())
		{
			if (!_xenoSpecies->getMilestone(progress.first))
			{
				throw Exception(context + ", study node '" + studyNode->getType() +
					"': speciesProgress milestone '" + progress.first +
					"' does not exist in xenoSpecies '" + _xenoSpecies->getType() + "'");
			}
		}
	}
}

const RuleContainedAlienStudyNode* RuleContainedAlien::getStudyNode(const std::string& type) const
{
	auto found = _studyNodesByType.find(type);
	return found == _studyNodesByType.end() ? nullptr : found->second;
}

const RuleContainedAlienStudyNode* RuleContainedAlien::getNextStudyNode(const std::string& type) const
{
	for (size_t i = 0; i < _studyNodes.size(); ++i)
	{
		if (_studyNodes[i]->getType() == type)
		{
			return i + 1 < _studyNodes.size() ? _studyNodes[i + 1] : nullptr;
		}
	}
	return nullptr;
}

}
