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
#include "BaseContainedMonster.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <sstream>
#include <utility>
#include "../Engine/Exception.h"
#include "../Engine/Game.h"
#include "../Engine/Language.h"
#include "../Engine/RNG.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleContainedMonster.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/RuleSoldier.h"
#include "Base.h"
#include "Craft.h"
#include "ItemContainer.h"
#include "ResearchProject.h"
#include "SavedGame.h"
#include "Soldier.h"

namespace OpenXcom
{

namespace
{

void appendUnique(std::vector<std::string>& values, const std::string& value)
{
	if (std::find(values.begin(), values.end(), value) == values.end())
	{
		values.push_back(value);
	}
}

ContainedMonsterAction parseAction(const std::string& action)
{
	if (action == "none") return CONTAINED_MONSTER_ACTION_NONE;
	if (action == "study") return CONTAINED_MONSTER_ACTION_STUDY;
	if (action == "quarantine") return CONTAINED_MONSTER_ACTION_QUARANTINE;
	if (action == "vivisection") return CONTAINED_MONSTER_ACTION_VIVISECTION;
	if (action == "taming") return CONTAINED_MONSTER_ACTION_TAMING;
	throw Exception("Unknown contained-monster action '" + action + "'");
}

#ifndef NDEBUG
void runDegradationDebugSelfTests()
{
	static const bool tested = []
	{
		assert(BaseContainedMonster::calculateDailyHealthDegradation(200, 5.0, {}) == 10);
		assert(BaseContainedMonster::calculateDailyHealthDegradation(200, 5.0, {0.75}) == 5);
		assert(BaseContainedMonster::calculateDailyHealthDegradation(200, 5.0, {1.0}) == 4);
		assert(BaseContainedMonster::calculateDailyHealthDegradation(200, 5.0, {0.75, 0.75}) == 2);
		assert(BaseContainedMonster::calculateDailyHealthDegradation(200, 5.0, {1.0, 1.0}) == 0);
		return true;
	}();
	(void)tested;
}
#endif

}

BaseContainedMonster::BaseContainedMonster(const RuleContainedMonster* rule, Base* base,
	int id, std::string designation) :
	_id(id), _designation(std::move(designation)), _rule(rule), _base(base)
{
	if (!_rule)
	{
		throw Exception("Cannot create a contained monster without rules");
	}
#ifndef NDEBUG
	runDegradationDebugSelfTests();
#endif
}

void BaseContainedMonster::initialize(int currentHP, int maxHP)
{
	if (_id <= 0 || _designation.empty())
	{
		throw Exception("Cannot initialize contained monster '" + _rule->getType() +
			"' without a positive id and designation");
	}
	if (maxHP <= 0 || currentHP <= 0 || currentHP > maxHP)
	{
		throw Exception("Cannot initialize contained monster '" + _rule->getType() +
			"': health must satisfy 0 < currentHP <= maxHP");
	}
	_currentHP = currentHP;
	_maxHP = maxHP;
	_action = CONTAINED_MONSTER_ACTION_NONE;
	_studyProgress = 0;
	_vivisectionProgress = 0;
	_tamingProgress = 0;
	_studied = false;
}

void BaseContainedMonster::load(const YAML::YamlNodeReader& reader, const Mod*,
	SavedGame* save)
{
	reader.tryRead("id", _id);
	reader.tryRead("designation", _designation);
	reader.tryRead("currentHP", _currentHP);
	reader.tryRead("maxHP", _maxHP);
	std::string action = "none";
	reader.tryRead("action", action);
	_action = parseAction(action);
	reader.tryRead("studyProgress", _studyProgress);
	reader.tryRead("vivisectionProgress", _vivisectionProgress);
	reader.tryRead("tamingProgress", _tamingProgress);
	reader.tryRead("studied", _studied);

	const std::string context = "Contained monster '" + _rule->getType() + "'";
	if (_id <= 0)
	{
		throw Exception(context + ": id must be a positive integer");
	}
	if (_designation.empty())
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": designation must not be empty");
	}
	if (_maxHP <= 0 || _currentHP <= 0 || _currentHP > _maxHP)
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": health must satisfy 0 < currentHP <= maxHP");
	}
	if (_studyProgress < 0 || _studyProgress > _rule->getStudy().getCost())
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": studyProgress is outside 0..study.cost");
	}
	if (_vivisectionProgress < 0 ||
		(_rule->getVivisection() && _vivisectionProgress > _rule->getVivisection()->getCost()) ||
		(!_rule->getVivisection() && _vivisectionProgress != 0))
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": vivisectionProgress is invalid for this rule");
	}
	if (_tamingProgress < 0 ||
		(_rule->getTaming() && _tamingProgress > _rule->getTaming()->getCost()) ||
		(!_rule->getTaming() && _tamingProgress != 0))
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": tamingProgress is invalid for this rule");
	}
	if (_studied && _studyProgress != _rule->getStudy().getCost())
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": studied specimen must retain completed studyProgress");
	}
	if (_action == CONTAINED_MONSTER_ACTION_VIVISECTION && !_rule->getVivisection())
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": saved vivisection action has no ruleset block");
	}
	if (_action == CONTAINED_MONSTER_ACTION_TAMING && !_rule->getTaming())
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": saved taming action has no ruleset block");
	}
	if (!save)
	{
		throw Exception(context + " with id " + std::to_string(_id) +
			": SavedGame is required while loading");
	}
}

void BaseContainedMonster::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("id", _id);
	writer.write("designation", _designation);
	writer.write("rule", _rule->getType());
	writer.write("currentHP", _currentHP);
	writer.write("maxHP", _maxHP);
	writer.write("action", getActionId());
	writer.write("studyProgress", _studyProgress);
	writer.write("vivisectionProgress", _vivisectionProgress);
	writer.write("tamingProgress", _tamingProgress);
	writer.write("studied", _studied);
}

bool BaseContainedMonster::isUsefulResearch(const RuleResearch* research,
	const SavedGame& save) const
{
	return research && !save.isResearchRuleStatusDisabled(research->getName()) &&
		(!save.isResearched(research, false) ||
			save.hasUndiscoveredGetOneFree(research, true) ||
			save.hasUndiscoveredProtectedUnlock(research));
}

bool BaseContainedMonster::isStudyAvailable(const SavedGame& save) const
{
	if (_studied || _rule->getUnlockedResearches().empty())
	{
		return false;
	}
	return std::any_of(_rule->getUnlockedResearches().begin(),
		_rule->getUnlockedResearches().end(),
		[&](const RuleResearch* research) { return isUsefulResearch(research, save); });
}

bool BaseContainedMonster::isVivisectionAvailable(const SavedGame& save) const
{
	const auto* rules = _rule->getVivisection();
	return rules && save.isResearched(rules->getRequiredResearch());
}

bool BaseContainedMonster::isTamingAvailable(const SavedGame& save) const
{
	const auto* rules = _rule->getTaming();
	return rules && _vivisectionProgress == 0 &&
		save.isResearched(rules->getRequiredResearch());
}

bool BaseContainedMonster::isQuarantineAvailable(const SavedGame& save) const
{
	return !isTamingAvailable(save);
}

bool BaseContainedMonster::setAction(ContainedMonsterAction action, const SavedGame& save)
{
	bool available = false;
	switch (action)
	{
	case CONTAINED_MONSTER_ACTION_NONE:
		cancelAction();
		return true;
	case CONTAINED_MONSTER_ACTION_STUDY:
		available = isStudyAvailable(save);
		break;
	case CONTAINED_MONSTER_ACTION_QUARANTINE:
		available = isQuarantineAvailable(save);
		break;
	case CONTAINED_MONSTER_ACTION_VIVISECTION:
		available = isVivisectionAvailable(save);
		break;
	case CONTAINED_MONSTER_ACTION_TAMING:
		available = isTamingAvailable(save);
		break;
	default:
		throw Exception("Invalid contained-monster action " +
			std::to_string(static_cast<int>(action)));
	}
	if (available)
	{
		_action = action;
	}
	return available;
}

void BaseContainedMonster::cancelAction()
{
	_action = CONTAINED_MONSTER_ACTION_NONE;
	clearAssignments();
}

bool BaseContainedMonster::normalizeActionAvailability(const SavedGame& save)
{
	bool available = true;
	switch (_action)
	{
	case CONTAINED_MONSTER_ACTION_NONE:
		return false;
	case CONTAINED_MONSTER_ACTION_STUDY:
		available = isStudyAvailable(save);
		break;
	case CONTAINED_MONSTER_ACTION_QUARANTINE:
		available = isQuarantineAvailable(save);
		break;
	case CONTAINED_MONSTER_ACTION_VIVISECTION:
		available = isVivisectionAvailable(save);
		break;
	case CONTAINED_MONSTER_ACTION_TAMING:
		available = isTamingAvailable(save);
		break;
	default:
		available = false;
		break;
	}
	if (!available)
	{
		cancelAction();
		return true;
	}
	return false;
}

std::vector<Soldier*> BaseContainedMonster::getAssignedScientists() const
{
	std::vector<Soldier*> result;
	if (!_base)
	{
		return result;
	}
	for (auto* soldier : *_base->getSoldiers())
	{
		if (soldier->getActiveContainedMonster() == this)
		{
			result.push_back(soldier);
		}
	}
	return result;
}

std::vector<Soldier*> BaseContainedMonster::getContributingScientists() const
{
	std::vector<Soldier*> result;
	for (auto* scientist : getAssignedScientists())
	{
		if (scientist->getRoleRank(ROLE_SCIENTIST) > 0 &&
			(!scientist->getCraft() || scientist->getCraft()->getStatus() != "STR_OUT"))
		{
			result.push_back(scientist);
		}
	}
	return result;
}

std::map<Soldier*, int> BaseContainedMonster::getScientistAssignments() const
{
	std::map<Soldier*, int> result;
	for (auto* scientist : getContributingScientists())
	{
		result.emplace(scientist, 100);
	}
	return result;
}

PromotionBatch BaseContainedMonster::finalizeScientistExperience(bool checkPromotions)
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

void BaseContainedMonster::clearAssignments()
{
	if (!_base)
	{
		return;
	}
	for (auto* soldier : *_base->getSoldiers())
	{
		if (soldier->getActiveContainedMonster() == this)
		{
			soldier->setActiveContainedMonster(nullptr);
		}
	}
}

void BaseContainedMonster::removeFromBase()
{
	if (_base)
	{
		_base->removeContainedMonster(this);
	}
}

ContainedMonsterProcessResult BaseContainedMonster::finishWithCorpse(bool terminated)
{
	ContainedMonsterProcessResult result;
	result.died = !terminated;
	result.terminated = terminated;
	result.promotions = finalizeScientistExperience(false);
	_action = CONTAINED_MONSTER_ACTION_NONE;
	clearAssignments();
	if (_base && _rule->getCorpseItem())
	{
		_base->getStorageItems()->addItem(_rule->getCorpseItem());
		result.corpseProduced = true;
	}
	removeFromBase();
	return result;
}

ContainedMonsterProcessResult BaseContainedMonster::terminate()
{
	return finishWithCorpse(true);
}

ContainedMonsterProcessResult BaseContainedMonster::processHour(Game& engine)
{
	ContainedMonsterProcessResult result;
	SavedGame& save = *engine.getSavedGame();
	Mod* mod = engine.getMod();
	result.actionCancelled = normalizeActionAvailability(save);
	if (_action == CONTAINED_MONSTER_ACTION_NONE ||
		_action == CONTAINED_MONSTER_ACTION_QUARANTINE)
	{
		return result;
	}

	auto assignments = getScientistAssignments();
	const int rating = save.getLoyaltyPerformanceBonus();
	if (_action == CONTAINED_MONSTER_ACTION_STUDY)
	{
		const int gain = getScientistProjectProgress(
			_rule->getStudy().getStats(), assignments, mod, rating, true);
		if (gain <= 0)
		{
			return result;
		}
		_studyProgress = std::min(_rule->getStudy().getCost(), _studyProgress + gain);
		if (_studyProgress < _rule->getStudy().getCost())
		{
			return result;
		}

		std::vector<RuleResearch*> researchBefore;
		if (_base)
		{
			save.getAvailableResearchProjects(researchBefore, mod, _base);
		}
		for (const auto* research : _rule->getUnlockedResearches())
		{
			if (!isUsefulResearch(research, save))
			{
				continue;
			}
			const ResearchDiscoveryResult discovery = save.completeResearchDiscovery(
				research, mod, _base, true, false, true);
			for (const auto* discovered : discovery.newlyDiscovered)
			{
				appendUnique(result.newlyDiscoveredResearchIds, discovered->getName());
			}
		}
		if (_base)
		{
			result.researchNotifications =
				save.getNewlyAvailableResearchIds(researchBefore, mod, _base);
		}
		_studied = true;
		result.studyCompleted = true;
		result.promotions = finalizeScientistExperience(true);
		_action = CONTAINED_MONSTER_ACTION_NONE;
		clearAssignments();
		return result;
	}

	if (_action == CONTAINED_MONSTER_ACTION_VIVISECTION)
	{
		const auto* rules = _rule->getVivisection();
		const int gain = getScientistProjectProgress(
			rules->getStats(), assignments, mod, rating, true);
		if (gain <= 0)
		{
			return result;
		}
		_vivisectionProgress = std::min(rules->getCost(), _vivisectionProgress + gain);
		if (_vivisectionProgress < rules->getCost())
		{
			return result;
		}
		result.promotions = finalizeScientistExperience(true);
		result.vivisectionCompleted = true;
		result.producedItems = rules->getProducedItems();
		_action = CONTAINED_MONSTER_ACTION_NONE;
		clearAssignments();
		if (_base)
		{
			for (const auto& produced : result.producedItems)
			{
				_base->getStorageItems()->addItem(produced.first, produced.second);
			}
		}
		removeFromBase();
		return result;
	}

	const auto* tamingRules = _rule->getTaming();
	if (_currentHP < _maxHP)
	{
		const int oldHealth = _currentHP;
		for (const auto* scientist : getContributingScientists())
		{
			const int chance = std::clamp<int>(scientist->getCurrentStats()->biology, 0, 100);
			if (RNG::percent(chance))
			{
				_currentHP = std::min(_maxHP, _currentHP + RNG::generate(1, 3));
			}
		}
		result.healthRestored = _currentHP - oldHealth;
		return result;
	}

	const int gain = getScientistProjectProgress(
		tamingRules->getStats(), assignments, mod, rating, true);
	if (gain <= 0)
	{
		return result;
	}
	_tamingProgress = std::min(tamingRules->getCost(), _tamingProgress + gain);
	if (_tamingProgress < tamingRules->getCost())
	{
		return result;
	}
	result.promotions = finalizeScientistExperience(true);
	result.tamedSoldier = mod->genSoldier(&save, tamingRules->getSoldier());
	if (_base)
	{
		_base->getSoldiers()->push_back(result.tamedSoldier);
	}
	result.tamingCompleted = true;
	_action = CONTAINED_MONSTER_ACTION_NONE;
	clearAssignments();
	removeFromBase();
	return result;
}

int BaseContainedMonster::calculateDailyHealthDegradation(int maxHP,
	double degradationPercent, const std::vector<double>& biologyRatios)
{
	if (maxHP <= 0 || !std::isfinite(degradationPercent) || degradationPercent < 0.0)
	{
		return 0;
	}
	double effectiveness = 0.0;
	for (double ratio : biologyRatios)
	{
		if (std::isfinite(ratio))
		{
			effectiveness += std::clamp(ratio, 0.0, 1.0);
		}
	}
	double reduction = 0.0;
	if (effectiveness <= 0.75)
	{
		reduction = 0.5 * effectiveness / 0.75;
	}
	else if (effectiveness < 2.0)
	{
		reduction = 0.5 + 0.5 * (effectiveness - 0.75) / 1.25;
	}
	else
	{
		reduction = 1.0;
	}
	const double rawDamage =
		static_cast<double>(maxHP) * degradationPercent / 100.0 * (1.0 - reduction);
	return static_cast<int>(std::lround(std::clamp(rawDamage, 0.0,
		static_cast<double>(maxHP))));
}

ContainedMonsterProcessResult BaseContainedMonster::processDay(Game& engine)
{
	ContainedMonsterProcessResult result;
	SavedGame& save = *engine.getSavedGame();
	result.actionCancelled = normalizeActionAvailability(save);
	if (isTamingAvailable(save))
	{
		return result;
	}

	std::vector<double> biologyRatios;
	if (_action == CONTAINED_MONSTER_ACTION_QUARANTINE)
	{
		for (const auto* scientist : getContributingScientists())
		{
			const int cap = scientist->getRules()->getStatCaps().biology;
			if (cap > 0)
			{
				biologyRatios.push_back(
					static_cast<double>(scientist->getCurrentStats()->biology) / cap);
			}
		}
	}
	const int damage = calculateDailyHealthDegradation(
		_maxHP, _rule->getHealthDegradationPercent(), biologyRatios);
	const int healthLost = std::min(_currentHP, damage);
	result.healthLost = healthLost;
	_currentHP -= damage;
	if (_currentHP <= 0)
	{
		result = finishWithCorpse(false);
		result.healthLost = healthLost;
	}
	return result;
}

ContainedMonsterProcessResult BaseContainedMonster::processTransferDay(Game& engine)
{
	ContainedMonsterProcessResult result;
	SavedGame& save = *engine.getSavedGame();
	result.actionCancelled = normalizeActionAvailability(save);
	if (isTamingAvailable(save))
	{
		return result;
	}

	const int damage = calculateDailyHealthDegradation(
		_maxHP, _rule->getHealthDegradationPercent(), {});
	const int healthLost = std::min(_currentHP, damage);
	result.healthLost = healthLost;
	_currentHP -= damage;
	if (_currentHP <= 0)
	{
		_action = CONTAINED_MONSTER_ACTION_NONE;
		clearAssignments();
		result.died = true;
	}
	return result;
}

void BaseContainedMonster::setCurrentHP(int health)
{
	_currentHP = std::clamp(health, 0, _maxHP);
}

std::string BaseContainedMonster::getActionId() const
{
	switch (_action)
	{
	case CONTAINED_MONSTER_ACTION_NONE: return "none";
	case CONTAINED_MONSTER_ACTION_STUDY: return "study";
	case CONTAINED_MONSTER_ACTION_QUARANTINE: return "quarantine";
	case CONTAINED_MONSTER_ACTION_VIVISECTION: return "vivisection";
	case CONTAINED_MONSTER_ACTION_TAMING: return "taming";
	default:
		throw Exception("Invalid contained-monster action " +
			std::to_string(static_cast<int>(_action)));
	}
}

int BaseContainedMonster::getCurrentProgress() const
{
	switch (_action)
	{
	case CONTAINED_MONSTER_ACTION_STUDY: return _studyProgress;
	case CONTAINED_MONSTER_ACTION_VIVISECTION: return _vivisectionProgress;
	case CONTAINED_MONSTER_ACTION_TAMING: return _tamingProgress;
	default: return 0;
	}
}

int BaseContainedMonster::getCurrentCost() const
{
	switch (_action)
	{
	case CONTAINED_MONSTER_ACTION_STUDY:
		return _rule->getStudy().getCost();
	case CONTAINED_MONSTER_ACTION_VIVISECTION:
		return _rule->getVivisection() ? _rule->getVivisection()->getCost() : 0;
	case CONTAINED_MONSTER_ACTION_TAMING:
		return _rule->getTaming() ? _rule->getTaming()->getCost() : 0;
	default:
		return 0;
	}
}

std::string BaseContainedMonster::getNameAndDesignation(Language* lang) const
{
	std::ostringstream result;
	result << lang->getString(_rule->getType()) << " / " << _designation;
	return result.str();
}

}
