/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "BasePrisoner.h"
#include "Base.h"
#include "SavedGame.h"
#include "../Mod/Mod.h"
#include "../Mod/Armor.h"
#include "../Mod/RulePrisoner.h"
#include "../Engine/Game.h"
#include "../Engine/RNG.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Engine/Language.h"
#include "../Engine/Exception.h"
#include <sstream>
#include <algorithm>
#include <set>

namespace OpenXcom
{
/**
 * Initializes a BattleUnit from a Soldier
 * @param mod Mod
 * @param type prisoner type
 * @param id technical captive instance id
 * @param designation player-facing captive designation
 */
BasePrisoner::BasePrisoner(const RulePrisoner* rule, Base* base, const std::string& type,
	int id, std::string designation) :
	_rule(rule), _id(id), _designation(std::move(designation)), _type(type), _state(PRISONER_STATE_NONE),
	_health(1), _intelligence(0), _aggression(0), _morale(100), _cooperation(0), _interrogationProgress(0), _recruitingProgress(0), _base(base)
{
}

void BasePrisoner::loadRoles(const std::vector<int>& r)
{
	_roles.clear();
	for (auto i : r)
	{
		if (i < ROLE_NONE || i > ROLE_ENGINEER)
		{
			throw Exception("Prisoner '" + _type + "' contains an invalid role value.");
		}
		SoldierRole role = static_cast<SoldierRole>(i);
		if (std::find(_roles.begin(), _roles.end(), role) != _roles.end())
		{
			throw Exception("Prisoner '" + _type + "' contains a duplicate role.");
		}
		_roles.push_back(role);
	}
	if (_roles.size() > 1 && std::find(_roles.begin(), _roles.end(), ROLE_NONE) != _roles.end())
	{
		throw Exception("Prisoner '" + _type + "' cannot combine ROLE_NONE with a real role.");
	}
	if (_roles.size() > 1 && std::find(_roles.begin(), _roles.end(), ROLE_ROBOT) != _roles.end())
	{
		throw Exception("Prisoner '" + _type + "' cannot combine a non-sapient ROLE_ROBOT with another role.");
	}
}

void BasePrisoner::setRoles(const std::vector<SoldierRole>& roles)
{
	std::vector<int> rawRoles;
	rawRoles.reserve(roles.size());
	for (SoldierRole role : roles)
	{
		rawRoles.push_back(static_cast<int>(role));
	}
	loadRoles(rawRoles);
}

///**
// * Loads the unit from a YAML file.
// * @param node YAML node.
// */
void BasePrisoner::load(const YAML::YamlNodeReader& reader, const Mod* mod)
{
	if (_id <= 0)
	{
		throw Exception("Prisoner '" + _type + "': id must be a positive integer");
	}
	if (_designation.empty())
	{
		throw Exception("Prisoner '" + _type + "' with id " + std::to_string(_id) +
			": designation must not be empty");
	}
	reader.tryRead("name", _name);
	reader.tryRead("type", _type);
	reader.tryRead("state", _state);
	if (reader["roles"])
	{
		std::vector<int> roles;
		reader.tryRead("roles", roles);
		loadRoles(roles);
	}
	reader.tryRead("health", _health);
	reader.tryRead("faction", _faction);
	reader.tryRead("stats", _stats);
	reader.tryRead("intelligence", _intelligence);
	reader.tryRead("aggression", _aggression);
	reader.tryRead("morale", _morale);
	reader.tryRead("cooperation", _cooperation);
	setMorale(_morale);
	setCooperation(_cooperation);
	reader.tryRead("spawnedTortureEvent", _spawnedTortureEvent);
	reader.tryRead("interrogationProgress", _interrogationProgress);
	reader.tryRead("recruitingProgress", _recruitingProgress);
	reader.tryRead("interrogationDone", _interrogationDone);
	if (reader["armor"].isValid())
	{
		std::string armor;
		reader.tryRead("armor", armor);
		_armor = mod->getArmor(armor);
	}
	//in case
	if (!_armor)
	{
		_armor = mod->getArmor(mod->getArmorsList().at(0));
		Log(LOG_ERROR) << "Base Prisoner named: " << _name << " fails to load correct ruleset for armor, default armor type: " << _armor->getType() << " was assigned. Please, report this case!";
	}


}

///**
// * Saves the soldier to a YAML file.
// * @return YAML node.
// */
void BasePrisoner::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("id", _id);
	writer.write("designation", _designation);
	writer.write("type", _type);
	writer.write("name", _name);
	writer.write("state", (int)_state);
	{
		std::vector<int> roles;
		for (auto r : _roles)
		{
			roles.push_back(r);
		}
		writer.write("roles", roles);
	}
	if (_spawnedTortureEvent)
		writer.write("spawnedTortureEvent", _spawnedTortureEvent);
	writer.write("health", _health);
	writer.write("faction", (int)_faction);
	writer.write("stats", _stats);
	writer.write("intelligence", _intelligence);
	writer.write("aggression", _aggression);
	writer.write("morale", _morale);
	writer.write("cooperation", _cooperation);
	writer.write("interrogationProgress", _interrogationProgress);
	writer.write("recruitingProgress", _recruitingProgress);
	writer.write("interrogationDone", _interrogationDone);
	writer.write("armor", _armor->getType());
}

void BasePrisoner::setMorale(int morale)
{
	if (morale < 1)
		morale = 1;
	else if (morale > 100)
		morale = 100;

	_morale = morale;
}

void BasePrisoner::setCooperation(int cooperation)
{
	_cooperation = std::max(-300, std::min(cooperation, 100));
}

/**
 * Applies the signed daily containment upkeep or income.
 * @param save Current campaign.
 */
void BasePrisoner::applyDailyFunds(SavedGame& save) const
{
	save.setFunds(save.getFunds() + _rule->getContainingRules().getFunds());
}

/**
 * Geoscape logic
 * @param engine - game pointer
 */
PrisonerProcessResult BasePrisoner::think(Game &engine)
{
	const Mod& mod = *engine.getMod();
	SavedGame& save = *engine.getSavedGame();
	PrisonerProcessResult result;

	// Containment upkeep/income is a daily per-prisoner rule and applies
	// regardless of the prisoner's currently selected activity.
	applyDailyFunds(save);

	//populate data
	PrisonerState prisonerState = getPrisonerState();
	_agents.clear();
	for (auto s: *_base->getSoldiers())
	{
		if (s->getActivePrisoner() == this)
			_agents.push_back(s);
	}

	//first, let's process physical conditions first
	auto containReq = _rule->getContainingRules().getReuiredResearch();
	if (containReq != nullptr && !save.isResearched(containReq))
	{
		setHealth(getHealth() - RNG::generate(0, _rule->getDamageOverTime()));
	}

	if (getHealth() <= 0) //prisoner dies
	{
		finalizeAgentExperience(false);
		die();
		result.event = PRISONER_PROCESS_DEATH;
	}
	else
	{
		//process different stats
		double speedFactor = (double)engine.getMod()->getPrisonerActionsSpeedFactor() / 100;
		int trainingFactor = mod.getIntelTrainingFactor();
		double loyaltyFactor = (double)engine.getGeoscapeState()->getLoyaltyPerformanceBonus() / 100;

		if (prisonerState == PRISONER_STATE_INTERROGATION)
		{
			const auto& rules = _rule->getInterrogationRules();
			int breakpoint = rules.getBaseResistance()
				+ getMorale() / 4
				- getCooperation() / 4
				+ getAggression() * 2
				+ getIntelligence() * 2;
			breakpoint = std::max(1, breakpoint);
			int progress = 0;
			int contributingAgents = 0;
			double effort = 0;
			for (auto s : _agents)
			{
				if (s->getCraft() != nullptr && s->getCraft()->getStatus() == "STR_OUT")
				{
					continue;
				}
				++contributingAgents;

				const bool canTrain = s->canGainExperience();
				auto stats = s->getCurrentStats();
				auto caps = s->getRules()->getStatCaps();
				double soldierEffort = 0, statEffort = 0;
				int interrogationCoef = 10;
				int charismaCoef = 20;
				int deceptionCoef = 40;
				int psiCoef = 5;

				statEffort = stats->interrogation;
				soldierEffort += (statEffort / interrogationCoef);
				if (canTrain && stats->interrogation < caps.interrogation
					&& RNG::generate(0, caps.interrogation) > stats->interrogation
					&& RNG::percent(trainingFactor))
				{
					s->getIntelExperience()->interrogation++;
				}

				statEffort = stats->charisma;
				soldierEffort += (statEffort / charismaCoef);
				if (canTrain && stats->charisma < caps.charisma
					&& RNG::generate(0, caps.charisma) > stats->charisma
					&& RNG::percent(trainingFactor))
				{
					s->getIntelExperience()->charisma++;
				}

				statEffort = stats->deception;
				soldierEffort += (statEffort / deceptionCoef);
				if (canTrain && stats->deception < caps.deception
					&& RNG::generate(0, caps.deception) > stats->deception
					&& RNG::percent(trainingFactor))
				{
					s->getIntelExperience()->deception++;
				}

				double insightBonus = RNG::generate(0, stats->insight);
				soldierEffort += insightBonus / 100;

				//extra handle for psi
				int statsN = 4;
				if (stats->psiSkill > 0)
				{
					statsN++;
					statEffort = stats->psiSkill;
					soldierEffort += (statEffort / psiCoef);

					if (canTrain && stats->psiSkill < caps.psiSkill
						&& RNG::generate(0, caps.psiSkill) > stats->psiSkill
						&& RNG::percent(trainingFactor / 2))
					{
						s->getIntelExperience()->psiSkill++;
					}
				}

				soldierEffort /= statsN;
				effort += soldierEffort;
			}
			// If one woman can carry a baby in nine months, nine women can't do it in a month...
			if (contributingAgents > 1)
			{
				effort *= std::max(0.0, (100.0 - 25.0 * log(contributingAgents)) / 100.0);
			}
			effort *= loyaltyFactor;
			effort *= speedFactor;
			progress = std::max(0, static_cast<int>(effort));
			_interrogationProgress += progress;
			if (_interrogationProgress >= breakpoint)
			{
				result.event = PRISONER_PROCESS_INTERROGATION;
				PromotionBatch promotions = finalizeAgentExperience(true);
				_interrogationProgress = 0;
				// give research if any
				std::string researchName;
				std::string bonusResearchName;
				std::vector<const RuleResearch*> possibilities;
				auto unlockResearches = rules.getUnlockedResearches();
				const bool hasResearchReward = std::any_of(unlockResearches.begin(), unlockResearches.end(),
					[&](const RuleResearch* research)
					{
						return !save.isResearched(research, false) ||
							save.hasUndiscoveredGetOneFree(research, true);
					});
				if (hasResearchReward)
				{
					std::vector<RuleResearch*> researchBefore;
					save.getAvailableResearchProjects(researchBefore, &mod, _base);
					const std::set<const RuleResearch*> discoveredBefore(
						save.getDiscoveredResearch().begin(), save.getDiscoveredResearch().end());
					engine.getSavedGame()->helpResearchDiscovery(unlockResearches, possibilities, engine.getMod(), _base, researchName, bonusResearchName);
					result.researchNotifications = save.getNewlyAvailableResearchIds(
						researchBefore, &mod, _base);
					for (const auto* discovered : save.getDiscoveredResearch())
					{
						if (discoveredBefore.find(discovered) == discoveredBefore.end())
						{
							result.newlyDiscovered.push_back(discovered);
						}
					}
				}

				if (rules.isDiesAfterInterrogation()) //prisoner dies
				{
					die();
				}
				else if (possibilities.empty()) //there is no point interrogating further
				{
					auto containReq = _rule->getContainingRules().getReuiredResearch();
					if (containReq == nullptr || save.isResearched(containReq))
					{
						setPrisonerState(PRISONER_STATE_CONTAINING);
					}
					else
					{
						setPrisonerState(PRISONER_STATE_NONE);
					}
				}

				_interrogationDone = rules.isDiesAfterInterrogation() || possibilities.empty();
				for (auto s : _agents)
				{
					s->clearBaseDuty();
				}

				RuleResearch* research;
				RuleResearch* bonus;
				if (!researchName.empty())
					research = mod.getResearch(researchName);
				else
					research = nullptr;

				if (!bonusResearchName.empty())
					bonus = mod.getResearch(bonusResearchName);
				else
					bonus = nullptr;

				result.research = research;
				result.bonus = bonus;
				result.promotions = std::move(promotions);
			}
		}
		else if (prisonerState == PRISONER_STATE_TORTURE)
		{
			const auto& rules = _rule->getTortureRules();
			// let's calculate power of our team
			int psionics = 0, torturePower = 0;
			for (auto agent: _agents)
			{
				torturePower += agent->getCurrentStats()->bravery;
				if (agent->getCurrentStats()->psiSkill > 0)
				{
					if (agent->getCurrentStats()->psiStrength > 50)
						psionics += 2;
					else
						psionics++;
				}
			}

			torturePower *= psionics + 1;
			if (torturePower > 0)
			{
				if (RNG::percent(-10 * save.getDifficultyCoefficient() + 80))
				{
					int tortureDifficulty = RNG::generate(rules.getDifficulty() / 2, rules.getDifficulty() * 2);
					tortureDifficulty = std::max(1, tortureDifficulty * (50 + getMorale()) / 100);
					//calculate and apply torture effects
					int maxDmg = 4 + floor(save.getDifficultyCoefficient() / 2);
					int loyaty = rules.getLoyalty() * (1 + floor(save.getDifficultyCoefficient() / 2));
					int moraleDmg = rules.getMorale();
					int cooperationChange = rules.getCooperation();
					int eventChance = rules.getEventChance();
					if (tortureDifficulty > torturePower * 2) // min torture
					{
						moraleDmg = 0;
						cooperationChange /= 10;
						maxDmg = ceil(maxDmg / 2);
						loyaty = ceil(loyaty / 5);
						eventChance = ceil(eventChance / 3);
					}
					else if (tortureDifficulty > torturePower)
					{
						maxDmg = ceil(maxDmg / 2);
						moraleDmg = ceil(moraleDmg / 3);
						cooperationChange /= 3;
						loyaty = ceil(loyaty / 4);
						eventChance = ceil(eventChance / 2);
					}
					setHealth(getHealth() - RNG::generate(0, maxDmg));
					setMorale(getMorale() - moraleDmg);
					setCooperation(getCooperation() + cooperationChange);
					engine.getGeoscapeState()->updateLoyalty(loyaty);

					if (_spawnedTortureEvent && !rules.isMultipleEventsPossible())
					{
						//no event, sorry.
					}
					else if (RNG::percent(eventChance))
					{
						auto events = rules.getSpawnedEvents();
						auto weightedEvent = rules.getWeightedEvent(save.getMonthsPassed());
						if (!weightedEvent.empty())
						{
							events.push_back(weightedEvent);
						}
						if (!events.empty() && save.spawnEvent(events, &mod))
						{
							_spawnedTortureEvent = true;
						}
					}
				}
			}
		}
		else if (prisonerState == PRISONER_STATE_REQRUITING)
		{
			const auto& rules = _rule->getRecruitingRules();
			int breakpoint = rules.getDifficulty()
				- getCooperation()
				+ (100 - getMorale()) / 2
				+ getAggression() * 10;
			breakpoint = std::max(1, breakpoint);
			int progress = 0;
			int contributingAgents = 0;
			double effort = 0;

			for (auto s : _agents)
			{
				if (s->getCraft() != nullptr && s->getCraft()->getStatus() == "STR_OUT")
				{
					continue;
				}
				++contributingAgents;

				double soldierEffort = 0, statEffort = 0;
				const bool canTrain = s->canGainExperience();
				auto stats = s->getCurrentStats();
				auto caps = s->getRules()->getStatCaps();

				statEffort = stats->charisma;
				soldierEffort += (statEffort);
				if (canTrain && stats->charisma < caps.charisma
					&& RNG::generate(0, caps.charisma) > stats->charisma
					&& RNG::percent(trainingFactor))
				{
					s->getIntelExperience()->charisma++;
				}

				statEffort = stats->deception;
				soldierEffort += (statEffort);
				if (canTrain && stats->deception < caps.deception
					&& RNG::generate(0, caps.deception) > stats->deception
					&& RNG::percent(trainingFactor))
				{
					s->getIntelExperience()->deception++;
				}

				soldierEffort /= 2;
				effort += soldierEffort;
			}

			if (contributingAgents > 1)
			{
				effort *= std::max(0.0, (100.0 - 25.0 * log(contributingAgents)) / 100.0);
			}

			effort *= loyaltyFactor;
			effort *= speedFactor;
			progress = std::max(0, static_cast<int>(effort));
			_recruitingProgress += progress;
			if (_recruitingProgress >= breakpoint)
			{
				result.event = PRISONER_PROCESS_RECRUITING;
				finalizeAgentExperience(false);
				_recruitingProgress = 0;
				auto events = rules.getSpawnedEvents();
				if (!events.empty() && RNG::percent(rules.getEventChance()))
				{
					save.spawnEvent(events, &mod);
				}

				const RuleSoldier *soldierRule = mod.getSoldier(rules.getSpawnedSoldier());
				if (soldierRule != nullptr) // we now create a new soldier from prisoner
				{
					Soldier* soldier = new Soldier(soldierRule, _armor, save.getId("STR_SOLDIER"));
					soldier->setBothStats(&_stats);
					for (auto role : _roles)
					{
						if (role != ROLE_NONE && !soldier->hasRole(role) && !soldier->addRole(role))
						{
							delete soldier;
							throw Exception("Recruited prisoner '" + _type + "' could not receive configured role "
								+ std::to_string(static_cast<int>(role)) + ".");
						}
					}
					_base->getSoldiers()->push_back(soldier);
					result.recruitedSoldier = soldier;
				}

				die();
			}
		}
		else if (prisonerState == PRISONER_STATE_CONTAINING)
		{
			const auto& rules = _rule->getContainingRules();
			int effort = 0;
			for (auto s : _agents)
			{
				int statEffort = 0;
				s->prepareStatsWithBonuses(&mod);
				auto stats = s->getStatsWithAllBonuses();

				statEffort = stats->charisma;

				if (stats->psiSkill > 0)
				{
					statEffort += ceil(stats->psiSkill * 0.5);
				}

				effort += statEffort;
			}

			const int braveryBias = (static_cast<int>(getStats()->bravery) - 35) / 5;
			int moraleChange = RNG::generate(-4 + braveryBias, 4 + braveryBias);
			int hpRegen = RNG::generate(0, 2);
			if (effort > 20)
			{
				hpRegen += RNG::generate(0, 1);
			}
			int authoredCooperationChange = rules.getCooperation();
			int cooperationChange = RNG::generate(
				std::min(0, authoredCooperationChange),
				std::max(0, authoredCooperationChange));
			setMorale(getMorale() + moraleChange);
			setHealth(std::min(getHealth() + hpRegen, static_cast<int>(getStats()->health)));
			setCooperation(getCooperation() + cooperationChange);
		}

		//almost done...
		if (_interrogationProgress > 0 && prisonerState != PRISONER_STATE_INTERROGATION)
		{
			_interrogationProgress -= floor(_interrogationProgress * 0.1);
		}

		if (_recruitingProgress > 0 && prisonerState != PRISONER_STATE_REQRUITING)
		{
			_recruitingProgress -= floor(_recruitingProgress * 0.15);
		}
	}
	return result;
}

void BasePrisoner::die()
{
	for (auto &s : _agents)
	{
		s->clearBaseDuty();
	}
	_base->removePrisoner(this);
	_interrogationProgress = 0;
	_recruitingProgress = 0;
}

PromotionBatch BasePrisoner::finalizeAgentExperience(bool checkPromotions)
{
	PromotionBatch promotions;
	for (auto* s : _agents)
	{
		s->improvePrimaryStats(s->getIntelExperience(), ROLE_AGENT);
		s->clearIntelExperience();
		if (checkPromotions)
		{
			RolePromotion promotion;
			if (s->rolePromoteSoldier(ROLE_AGENT, &promotion))
			{
				promotions.push_back(promotion);
			}
		}
	}
	return promotions;
}

std::string BasePrisoner::getNameAndDesignation(Language* lang) const
{
	std::ostringstream nameId;
	nameId << lang->getString(getName());
	nameId << " / ";
	nameId << getDesignation();
	return nameId.str();
}

}
