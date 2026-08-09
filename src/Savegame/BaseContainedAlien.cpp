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
#include "BaseContainedAlien.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>
#include "../Engine/Exception.h"
#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleContainedAlien.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/RuleXenoSpecies.h"
#include "Base.h"
#include "ItemContainer.h"
#include "SavedGame.h"
#include "Soldier.h"
#include "XenoSpecies.h"

namespace OpenXcom
{

namespace
{

template<typename T>
void appendUnique(std::vector<T>& values, const T& value)
{
	if (std::find(values.begin(), values.end(), value) == values.end())
	{
		values.push_back(value);
	}
}

}

int64_t BaseContainedAlien::toFixed(double value, const std::string& context)
{
	if (!std::isfinite(value) || value > static_cast<double>(std::numeric_limits<int64_t>::max()) / FIXED_SCALE ||
		value < static_cast<double>(std::numeric_limits<int64_t>::min()) / FIXED_SCALE)
	{
		throw Exception(context + ": fixed-point value is outside the supported range");
	}
	return static_cast<int64_t>(std::llround(value * FIXED_SCALE));
}

BaseContainedAlien::BaseContainedAlien(const RuleContainedAlien* rule, Base* base, int id,
	std::string designation) :
	_id(id), _designation(std::move(designation)), _rule(rule), _base(base)
{
	if (!_rule)
	{
		throw Exception("Cannot create a contained alien without rules");
	}
	runContainedAlienStudyDebugSelfTests();
	_stabilityFixed = toFixed(_rule->getInitialStability(), "Contained alien initial stability");
}

void BaseContainedAlien::initialize(double health, double maxHealth, int fatalWounds,
	SavedGame* save)
{
	if (!save || _id <= 0 || _designation.empty())
	{
		throw Exception("Cannot initialize contained alien '" + _rule->getType() +
			"' without save, positive id and designation");
	}
	_save = save;
	maxHealth = std::max(1.0, maxHealth);
	health = std::clamp(health, 1.0, maxHealth);
	_maxHealthFixed = toFixed(maxHealth, "Contained alien maxHealth");
	_healthFixed = toFixed(health, "Contained alien health");
	_stabilityFixed = toFixed(calculateContainedAlienCaptureStability(
		_rule->getInitialStability(), fatalWounds, _rule->getFatalWoundStabilityPenalty()),
		"Contained alien stability");
	_completedStudyNodes.clear();
	_currentStudyNode = _rule->getStudyNodes().front()->getType();
	_studyProgressFixed = 0;
	XenoSpecies* species = save->addXenoSpecies(_rule->getXenoSpecies());
	_effectiveStudyCostFixed = toFixed(
		calculateContainedAlienEffectiveCost(getCurrentStudyNode(), species),
		"Contained alien effectiveStudyCost");
	normalizeProtocolForCurrentNode();
}

void BaseContainedAlien::load(const YAML::YamlNodeReader& reader, const Mod* mod, SavedGame* save)
{
	_save = save;
	reader.tryRead("id", _id);
	reader.tryRead("designation", _designation);
	double health = getHealth();
	double maxHealth = getMaxHealth();
	double stability = getStability();
	double studyProgress = getStudyProgress();
	double effectiveStudyCost = getEffectiveStudyCost();
	reader.tryRead("health", health);
	reader.tryRead("maxHealth", maxHealth);
	reader.tryRead("stability", stability);
	int protocol = static_cast<int>(_protocol);
	reader.tryRead("protocol", protocol);
	reader.tryRead("currentStudyNode", _currentStudyNode);
	reader.tryRead("completedStudyNodes", _completedStudyNodes);
	reader.tryRead("studyProgress", studyProgress);
	reader.tryRead("effectiveStudyCost", effectiveStudyCost);
	reader.tryRead("containedHours", _containedHours);
	reader.tryRead("pendingCompletedMilestones", _pendingCompletedMilestones);
	reader.tryRead("pendingResearchRewards", _pendingResearchRewards);
	reader.tryRead("pendingArticleIds", _pendingArticleIds);
	reader.tryRead("pendingResearchNotifications", _pendingResearchNotifications);
	reader.tryRead("pendingSpeciesProgress", _pendingSpeciesProgress);
	reader.tryRead("pendingStudyUnblockNotification", _pendingStudyUnblockNotification);

	const std::string context = "Contained alien '" + _rule->getType() + "'";
	if (_id <= 0)
	{
		throw Exception(context + ": id must be a positive integer");
	}
	if (_designation.empty())
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": designation must not be empty");
	}
	if (!std::isfinite(maxHealth) || maxHealth <= 0.0 || !std::isfinite(health) ||
		health <= 0.0 || health > maxHealth)
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": health must be within 0 < health <= maxHealth");
	}
	if (!std::isfinite(stability) || stability < 0.0 || stability > 100.0)
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": stability must be within 0..100");
	}
	if (protocol < CONTAINED_PROTOCOL_PASSIVE || protocol > CONTAINED_PROTOCOL_AGGRESSIVE)
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": protocol is invalid");
	}
	if (!std::isfinite(studyProgress) || studyProgress < 0.0 ||
		!std::isfinite(effectiveStudyCost) || effectiveStudyCost < 0.0)
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": study progress and effective cost must be finite and non-negative");
	}
	if (_containedHours < 0)
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": containedHours must be non-negative");
	}

	_protocol = static_cast<ContainedAlienStudyProtocol>(protocol);
	_healthFixed = toFixed(health, context + " health");
	_maxHealthFixed = toFixed(maxHealth, context + " maxHealth");
	_stabilityFixed = toFixed(stability, context + " stability");
	_studyProgressFixed = toFixed(studyProgress, context + " studyProgress");
	_effectiveStudyCostFixed = toFixed(effectiveStudyCost, context + " effectiveStudyCost");
	validateStudyChain();
	if (!_currentStudyNode.empty() &&
		(_effectiveStudyCostFixed <= 0 || _studyProgressFixed > _effectiveStudyCostFixed))
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": active node requires 0 <= studyProgress <= effectiveStudyCost");
	}
	if (_currentStudyNode.empty() && (_studyProgressFixed != 0 || _effectiveStudyCostFixed != 0))
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": completed chain contains active study progress");
	}

	XenoSpecies* species = save->addXenoSpecies(_rule->getXenoSpecies());
	for (const auto& milestone : _pendingCompletedMilestones)
	{
		if (!species->getRules()->getMilestone(milestone))
		{
			throw Exception(context + " with id " + std::to_string(_id) +
				": pending milestone '" + milestone + "' does not exist");
		}
	}
	for (const auto& research : _pendingResearchRewards)
	{
		if (!mod->getResearch(research, false))
		{
			throw Exception(context + " with id " + std::to_string(_id) +
				": pending research reward '" + research + "' does not exist");
		}
	}
	normalizeProtocolForCurrentNode();
}

void BaseContainedAlien::validateStudyChain() const
{
	const auto& nodes = _rule->getStudyNodes();
	if (_completedStudyNodes.size() > nodes.size())
	{
		throw Exception("Contained alien id " + std::to_string(_id) +
			": completedStudyNodes is longer than the ruleset chain");
	}
	std::set<std::string> unique;
	for (size_t i = 0; i < _completedStudyNodes.size(); ++i)
	{
		if (!unique.insert(_completedStudyNodes[i]).second)
		{
			throw Exception("Contained alien id " + std::to_string(_id) +
				": completedStudyNodes contains duplicate node '" + _completedStudyNodes[i] + "'");
		}
		if (nodes[i]->getType() != _completedStudyNodes[i])
		{
			throw Exception("Contained alien id " + std::to_string(_id) +
				": completedStudyNodes must be the ordered prefix of the ruleset chain");
		}
	}
	const std::string expected = _completedStudyNodes.size() < nodes.size()
		? nodes[_completedStudyNodes.size()]->getType()
		: std::string();
	if (_currentStudyNode != expected)
	{
		throw Exception("Contained alien id " + std::to_string(_id) +
			": currentStudyNode '" + _currentStudyNode + "' does not follow completedStudyNodes");
	}
}

void BaseContainedAlien::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("id", _id);
	writer.write("designation", _designation);
	writer.write("rule", _rule->getType());
	writer.write("health", getHealth());
	writer.write("maxHealth", getMaxHealth());
	writer.write("stability", getStability());
	writer.write("protocol", static_cast<int>(_protocol));
	writer.write("currentStudyNode", _currentStudyNode);
	writer.write("completedStudyNodes", _completedStudyNodes);
	writer.write("studyProgress", getStudyProgress());
	writer.write("effectiveStudyCost", getEffectiveStudyCost());
	writer.write("containedHours", _containedHours);
	if (!_pendingCompletedMilestones.empty())
		writer.write("pendingCompletedMilestones", _pendingCompletedMilestones);
	if (!_pendingResearchRewards.empty())
		writer.write("pendingResearchRewards", _pendingResearchRewards);
	if (!_pendingArticleIds.empty())
		writer.write("pendingArticleIds", _pendingArticleIds);
	if (!_pendingResearchNotifications.empty())
		writer.write("pendingResearchNotifications", _pendingResearchNotifications);
	if (_pendingSpeciesProgress)
		writer.write("pendingSpeciesProgress", _pendingSpeciesProgress);
	if (_pendingStudyUnblockNotification)
		writer.write("pendingStudyUnblockNotification", _pendingStudyUnblockNotification);
}

std::vector<Soldier*> BaseContainedAlien::getAssignedScientists() const
{
	std::vector<Soldier*> result;
	if (!_base)
	{
		return result;
	}
	for (auto* soldier : *_base->getSoldiers())
	{
		if (soldier->getActiveContainedAlien() == this)
		{
			result.push_back(soldier);
		}
	}
	return result;
}

ContainedAlienHourlyInput BaseContainedAlien::makeHourlyInput(ContainedAlienStudyProtocol protocol,
	const std::vector<Soldier*>& scientists) const
{
	ContainedAlienHourlyInput input;
	input.health = getHealth();
	input.stability = getStability();
	input.progress = getStudyProgress();
	input.effectiveCost = getEffectiveStudyCost();
	const auto* node = getCurrentStudyNode();
	input.studyBlocked = node && isCurrentStudyNodeBlocked();
	input.protocol = normalizeContainedAlienProtocol(
		protocol, node != nullptr, input.studyBlocked);
	input.healthDecayPerDay = _rule->getHealthDecayPerDay();
	input.stabilityDecayPerDay = _rule->getStabilityDecayPerDay();
	input.unstableHealthDamagePerDay = _rule->getUnstableHealthDamagePerDay();
	input.passiveRecoveryPerDay = _rule->getPassiveRecoveryPerDay();
	input.passiveRecoveryCap = _rule->getPassiveRecoveryCap();
	// Assigned, on-base scientists keep providing care after the study chain is
	// complete even though there is no node progress or category effort to earn.
	input.hasContributors = hasContainedAlienContributors(scientists);
	if (node)
	{
		input.baseCost = node->getCost();
		input.stabilityCost = node->getStabilityCost();
		input.teamEffort = getContainedAlienTeamEffort(scientists, node->getCategory(), _base->getMod());
	}
	return input;
}

ContainedAlienProcessResult BaseContainedAlien::processHour(Game& engine)
{
	ContainedAlienProcessResult result;
	normalizeProtocolForCurrentNode();
	SavedGame* save = engine.getSavedGame();
	const Mod* mod = engine.getMod();
	XenoSpecies* species = save->addXenoSpecies(_rule->getXenoSpecies());
	auto scientists = getAssignedScientists();
	const auto* node = getCurrentStudyNode();
	const double oldProgress = getStudyProgress();
	std::vector<RuleResearch*> researchBefore;
	bool capturedResearchAvailability = false;
	const auto hourlyInput = makeHourlyInput(_protocol, scientists);
	const auto output = processContainedAlienHour(hourlyInput);
	_healthFixed = toFixed(output.health, "Contained alien hourly health");
	_stabilityFixed = toFixed(output.stability, "Contained alien hourly stability");
	++_containedHours;

	// Physical collapse is resolved before scientific work from the same hour.
	// If death and completion would coincide, the node does not complete and no
	// progress, species knowledge or training from this fatal hour is awarded.
	if (_healthFixed <= 0)
	{
		return finishTerminal(false);
	}

	_studyProgressFixed = toFixed(output.progress, "Contained alien hourly study progress");
	const double actualProgressGain = output.progress - oldProgress;
	if (node && shouldApplyContainedAlienScientificProgress(hourlyInput, output))
	{
		trainContainedAlienScientists(scientists, node->getCategory(), mod);
		for (const auto& configuredProgress : node->getSpeciesProgress())
		{
			const double increment = configuredProgress.second * actualProgressGain /
				getEffectiveStudyCost();
			const auto* milestoneRules = species->getRules()->getMilestone(configuredProgress.first);
			if (increment > 0.0 &&
				species->getMilestoneProgress(configuredProgress.first) < milestoneRules->getCost())
			{
				_pendingSpeciesProgress = true;
			}
			const auto progressResult = species->addProgress(configuredProgress.first, increment);
			for (const auto* milestone : progressResult.completedMilestones)
			{
				appendUnique(_pendingCompletedMilestones, milestone->getType());
				// Research graph snapshots are event-owned: ordinary hourly care and
				// progress never pay for them unless a reward is actually issued.
				if (!milestone->getUnlockResearches().empty() && !capturedResearchAvailability)
				{
					save->getAvailableResearchProjects(researchBefore, mod, _base);
					capturedResearchAvailability = true;
				}
				for (const auto* research : milestone->getUnlockResearches())
				{
					ResearchDiscoveryResult discovery = save->completeResearchDiscovery(
						research, mod, _base, true, false, true);
					for (const auto* discovered : discovery.newlyDiscovered)
					{
						appendUnique(_pendingResearchRewards, discovered->getName());
						appendUnique(result.newlyDiscoveredResearchIds, discovered->getName());
					}
				}
			}
		}
	}
	if (capturedResearchAvailability)
	{
		for (const auto& researchId : save->getNewlyAvailableResearchIds(
			researchBefore, mod, _base))
		{
			appendUnique(result.researchNotifications, researchId);
		}
	}

	if (!node || _studyProgressFixed < _effectiveStudyCostFixed)
	{
		return result;
	}

	result.nodeCompleted = true;
	result.completedNode = node->getType();
	result.strategicImpact = !_pendingCompletedMilestones.empty()
		? CONTAINED_IMPACT_BREAKTHROUGH
		: (_pendingSpeciesProgress ? CONTAINED_IMPACT_SPECIES_PROGRESS : CONTAINED_IMPACT_ROUTINE);
	result.completedMilestones = _pendingCompletedMilestones;
	result.researchRewards = _pendingResearchRewards;
	result.promotions = finalizeScientistExperience(true);
	_pendingCompletedMilestones.clear();
	_pendingResearchRewards.clear();
	_pendingSpeciesProgress = false;
	_completedStudyNodes.push_back(node->getType());
	_studyProgressFixed = 0;
	if (const auto* next = _rule->getNextStudyNode(node->getType()))
	{
		_currentStudyNode = next->getType();
		_effectiveStudyCostFixed = toFixed(
			calculateContainedAlienEffectiveCost(next, species),
			"Contained alien next effectiveStudyCost");
		normalizeProtocolForCurrentNode();
	}
	else
	{
		_currentStudyNode.clear();
		_effectiveStudyCostFixed = 0;
		normalizeProtocolForCurrentNode();
		result.chainCompleted = true;
	}
	return result;
}

PromotionBatch BaseContainedAlien::finalizeScientistExperience(bool checkPromotions)
{
	PromotionBatch promotions;
	for (auto* scientist : getAssignedScientists())
	{
		scientist->improvePrimaryStats(scientist->getResearchExperience(), ROLE_SCIENTIST);
		scientist->clearResearchExperience();
		if (checkPromotions)
		{
			RolePromotion promotion;
			if (scientist->rolePromoteSoldier(ROLE_SCIENTIST, &promotion))
			{
				promotions.push_back(promotion);
			}
		}
	}
	return promotions;
}

void BaseContainedAlien::clearAssignments()
{
	if (!_base)
	{
		return;
	}
	for (auto* soldier : *_base->getSoldiers())
	{
		if (soldier->getActiveContainedAlien() == this)
		{
			soldier->setActiveContainedAlien(nullptr);
		}
	}
}

ContainedAlienProcessResult BaseContainedAlien::terminate()
{
	return finishTerminal(true);
}

ContainedAlienProcessResult BaseContainedAlien::finishTerminal(bool manualTermination)
{
	ContainedAlienProcessResult result;
	result.died = !manualTermination;
	result.terminated = manualTermination;
	result.strategicImpact = !_pendingCompletedMilestones.empty()
		? CONTAINED_IMPACT_BREAKTHROUGH
		: (_pendingSpeciesProgress ? CONTAINED_IMPACT_SPECIES_PROGRESS : CONTAINED_IMPACT_ROUTINE);
	result.completedMilestones = _pendingCompletedMilestones;
	result.researchRewards = _pendingResearchRewards;
	result.articleIds.swap(_pendingArticleIds);
	result.researchNotifications.swap(_pendingResearchNotifications);
	finalizeScientistExperience(false);
	_pendingCompletedMilestones.clear();
	_pendingResearchRewards.clear();
	_pendingSpeciesProgress = false;
	if (!_base)
	{
		return result;
	}
	clearAssignments();
	_base->getStorageItems()->addItem(_rule->getCorpseItem());
	_base->removeContainedAlien(this);
	return result;
}

bool BaseContainedAlien::setProtocol(ContainedAlienStudyProtocol protocol)
{
	if (!canUseProtocol(protocol))
	{
		return false;
	}
	_protocol = protocol;
	return true;
}

const RuleContainedAlienStudyNode* BaseContainedAlien::getCurrentStudyNode() const
{
	return _currentStudyNode.empty() ? nullptr : _rule->getStudyNode(_currentStudyNode);
}

bool BaseContainedAlien::isCurrentStudyNodeBlocked() const
{
	const auto* node = getCurrentStudyNode();
	return node && _save && !_save->isResearched(node->getRequiredResearch(), false);
}

std::vector<const RuleResearch*> BaseContainedAlien::getUnmetStudyResearch() const
{
	std::vector<const RuleResearch*> result;
	const auto* node = getCurrentStudyNode();
	if (!node)
	{
		return result;
	}
	for (const auto* research : node->getRequiredResearch())
	{
		if (!_save || !_save->isResearched(research, false))
		{
			result.push_back(research);
		}
	}
	return result;
}

bool BaseContainedAlien::canUseProtocol(ContainedAlienStudyProtocol protocol) const
{
	const bool hasCurrentStudyNode = getCurrentStudyNode() != nullptr;
	return canUseContainedAlienProtocol(
		protocol, hasCurrentStudyNode,
		hasCurrentStudyNode && isCurrentStudyNodeBlocked());
}

void BaseContainedAlien::normalizeProtocolForCurrentNode()
{
	const bool hasCurrentStudyNode = getCurrentStudyNode() != nullptr;
	_protocol = normalizeContainedAlienProtocol(
		_protocol, hasCurrentStudyNode,
		hasCurrentStudyNode && isCurrentStudyNodeBlocked());
}

std::string BaseContainedAlien::getNameAndDesignation(Language* lang) const
{
	std::ostringstream result;
	result << lang->getString(_rule->getType()) << " / " << _designation;
	return result.str();
}

ContainedAlienStudyForecast BaseContainedAlien::getForecast(ContainedAlienStudyProtocol protocol,
	const std::vector<Soldier*>* scientists) const
{
	auto assigned = scientists ? *scientists : getAssignedScientists();
	return forecastContainedAlienStudy(makeHourlyInput(protocol, assigned));
}

ContainedAlienStrategicImpact BaseContainedAlien::getStrategicImpact() const
{
	if (!_base || !getCurrentStudyNode())
	{
		return CONTAINED_IMPACT_ROUTINE;
	}
	if (!_pendingCompletedMilestones.empty())
	{
		return CONTAINED_IMPACT_BREAKTHROUGH;
	}
	if (_pendingSpeciesProgress)
	{
		return CONTAINED_IMPACT_SPECIES_PROGRESS;
	}
	if (!_save)
	{
		return CONTAINED_IMPACT_ROUTINE;
	}
	XenoSpecies* species = _save->addXenoSpecies(_rule->getXenoSpecies());
	return calculateContainedAlienStrategicImpact(getCurrentStudyNode(), getStudyProgress(),
		getEffectiveStudyCost(), species);
}

void BaseContainedAlien::appendPendingResearchNotifications(const std::vector<std::string>& ids)
{
	for (const auto& id : ids)
	{
		appendUnique(_pendingResearchNotifications, id);
	}
}

void BaseContainedAlien::appendPendingArticleIds(const std::vector<std::string>& ids)
{
	for (const auto& id : ids)
	{
		appendUnique(_pendingArticleIds, id);
	}
}

std::vector<std::string> BaseContainedAlien::takePendingArticleIds()
{
	std::vector<std::string> result;
	result.swap(_pendingArticleIds);
	return result;
}

std::vector<std::string> BaseContainedAlien::takePendingResearchNotifications()
{
	std::vector<std::string> result;
	result.swap(_pendingResearchNotifications);
	return result;
}

bool BaseContainedAlien::takePendingStudyUnblockNotification()
{
	const bool result = _pendingStudyUnblockNotification;
	_pendingStudyUnblockNotification = false;
	return result;
}

}
