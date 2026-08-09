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
#include "Production.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "../Mod/RuleManufacture.h"
#include "../Mod/RuleSoldier.h"
#include "Base.h"
#include "SavedGame.h"
#include "Transfer.h"
#include "ItemContainer.h"
#include "Soldier.h"
#include "Craft.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleCraft.h"
#include "../Engine/Language.h"
#include "../Engine/RNG.h"
#include "../Engine/Exception.h"

namespace OpenXcom
{
Production::Production(const RuleManufacture * rules, int amount) :
	_rules(rules), _amount(amount), _infinite(false), _timeSpent(0), _amountProduced(0), _efficiency(0),
	_currentItemStarted(false), _currentUnitCost(0), _baseFunctionsMissingReported(false)
{
	if (_rules->getProducedCraft() && amount > 0 && amount != 1)
	{
		throw Exception("Craft production must contain exactly one craft.");
	}
}

bool Production::haveEnoughMoneyForOneMoreUnit(SavedGame * g) const
{
	const int64_t cost = getUnitCost();
	return cost <= 0 || g->getFunds() >= cost;
}

void Production::setAmountTotal(int amount)
{
	if (amount < 1 || (_rules->getProducedCraft() && amount != 1))
	{
		throw Exception("Invalid production amount for '" + _rules->getName() + "'.");
	}
	_amount = amount;
}

void Production::setInfiniteAmount(bool infinite)
{
	if (infinite && _rules->getProducedCraft())
	{
		throw Exception("Craft production cannot be infinite.");
	}
	_infinite = infinite;
}

int64_t Production::calculateUnitCost(int64_t baseCost, int efficiencyValue)
{
	if (baseCost <= 0)
	{
		return 0;
	}

	const double efficiency = std::max(0, efficiencyValue);
	double discount;
	if (efficiency <= 100.0)
	{
		discount = efficiency / 4.0;
	}
	else
	{
		discount = 50.0 - 25.0 * std::exp(-(efficiency - 100.0) / 100.0);
	}
	return static_cast<int64_t>(std::ceil(static_cast<long double>(baseCost) * (1.0L - discount / 100.0L)));
}

int64_t Production::getUnitCost() const
{
	return calculateUnitCost(_rules->getManufactureCost(), _efficiency);
}

bool Production::haveRequiredBaseFunctions(Base * b) const
{
	const RuleBaseFacilityFunctions provided = b->getProvidedBaseFunc({});
	return !(~provided & _rules->getRequireBaseFunc()).any();
}

bool Production::haveEnoughLivingSpaceForOneMoreUnit(Base * b)
{
	if (_rules->getSpawnedPersonType() != "")
	{
		// Note: if the production is running then the space we need is already counted by getUsedQuarters
		if (b->getAvailableQuarters() < b->getUsedQuarters())
		{
			return false;
		}
	}
	return true;
}

bool Production::haveEnoughMaterialsForOneMoreUnit(Base * b, const Mod *m) const
{
	(void)m;
	for (const auto& i : _rules->getRequiredItems())
	{
		if (b->getStorageItems()->getItem(i.first) < i.second)
			return false;
	}
	for (const auto& i : _rules->getRequiredCrafts())
	{
		if (b->getCraftCountForProduction(i.first) < i.second)
			return false;
	}
	return true;
}

std::vector<Soldier*> Production::getAssignedSoldiers(Base* b)
{
	std::vector<Soldier*> assignedEngineers;
	for (auto s : *b->getSoldiers())
	{
		if (s->getProductionProject() == this && s->hasRole(ROLE_ENGINEER))
		{
			assignedEngineers.push_back(s);
		}
	}
	return assignedEngineers;
}

int Production::getProgress(Base* b, SavedGame* g, const Mod* m, int loyaltyRating, bool prediction)
{
	(void)g;
	int progress = 0;
	std::vector<Soldier*> assignedEngineers = getAssignedSoldiers(b);
	if (!assignedEngineers.empty())
	{
		double effort = 0;
		auto projStats = _rules->getStats();
		int trainingFactor = m->getEngineerTrainingFactor();
		double speedFactor = (double)m->getEngineeringSpeedFactor() / 100;
		int64_t summedEfficiency = 0;
		std::size_t contributingEngineers = 0;
		for (auto s : assignedEngineers)
		{
			if (s->getCraft() && s->getCraft()->getStatus() == "STR_OUT")
			{
				continue;
			}
			++contributingEngineers;
			const bool canTrain = !prediction && s->canGainExperience();

			s->prepareStatsWithBonuses(m);
			auto stats = s->getStatsWithAllBonuses();
			auto caps = s->getRules()->getStatCaps();
			unsigned int statsN = 0;
			double soldierEffort = 0, statEffort = 0;
			if (projStats.weaponry > 0)
			{
				statEffort = stats->weaponry;
				soldierEffort += statEffort / projStats.weaponry;
				if (canTrain && stats->weaponry < caps.weaponry && RNG::generate(0, caps.weaponry) > stats->weaponry && RNG::percent(trainingFactor))
					s->getEngineerExperience()->weaponry++;
				statsN++;
			}
			if (projStats.explosives > 0)
			{
				statEffort = stats->explosives;
				soldierEffort += statEffort / projStats.explosives;
				if (canTrain && stats->explosives < caps.explosives && RNG::generate(0, caps.explosives) > stats->explosives && RNG::percent(trainingFactor))
					s->getEngineerExperience()->explosives++;
				statsN++;
			}
			if (projStats.microelectronics > 0)
			{
				statEffort = stats->microelectronics;
				soldierEffort += statEffort / projStats.microelectronics;
				if (canTrain && stats->microelectronics < caps.microelectronics && RNG::generate(0, caps.microelectronics) > stats->microelectronics && RNG::percent(trainingFactor))
					s->getEngineerExperience()->microelectronics++;
				statsN++;
			}
			if (projStats.metallurgy > 0)
			{
				statEffort = stats->metallurgy;
				soldierEffort += statEffort / projStats.metallurgy;
				if (canTrain && stats->metallurgy < caps.metallurgy && RNG::generate(0, caps.metallurgy) > stats->metallurgy && RNG::percent(trainingFactor))
					s->getEngineerExperience()->metallurgy++;
				statsN++;
			}
			if (projStats.processing > 0)
			{
				statEffort = stats->processing;
				soldierEffort += statEffort / projStats.processing;
				if (canTrain && stats->processing < caps.processing && RNG::generate(0, caps.processing) > stats->processing && RNG::percent(trainingFactor))
					s->getEngineerExperience()->processing++;
				statsN++;
			}
			if (projStats.hacking > 0)
			{
				statEffort = stats->hacking;
				soldierEffort += statEffort / projStats.hacking;
				if (canTrain && stats->hacking < caps.hacking && RNG::generate(0, caps.hacking) > stats->hacking && RNG::percent(trainingFactor))
					s->getEngineerExperience()->hacking++;
				statsN++;
			}
			if (projStats.robotics > 0)
			{
				statEffort = stats->robotics;
				soldierEffort += statEffort / projStats.robotics;
				if (canTrain && stats->robotics < caps.robotics && RNG::generate(0, caps.robotics) > stats->robotics && RNG::percent(trainingFactor))
					s->getEngineerExperience()->robotics++;
				statsN++;
			}
			if (projStats.alienTech > 0)
			{
				statEffort = stats->alienTech;
				soldierEffort += statEffort / projStats.alienTech;
				if (canTrain && stats->alienTech < caps.alienTech && RNG::generate(0, caps.alienTech) > stats->alienTech && RNG::percent(trainingFactor))
					s->getEngineerExperience()->alienTech++;
				statsN++;
			}
			if (projStats.reverseEngineering > 0)
			{
				statEffort = stats->reverseEngineering;
				soldierEffort += statEffort / projStats.reverseEngineering;
				if (canTrain && stats->reverseEngineering < caps.reverseEngineering && RNG::generate(0, caps.reverseEngineering) > stats->reverseEngineering && RNG::percent(trainingFactor))
					s->getEngineerExperience()->reverseEngineering++;
				statsN++;
			}

			int diligence = stats->diligence;
			double diligenceFactor = 0.5;
			if (diligence > 10)
				diligenceFactor = -0.5 + 0.434 * std::log(std::fabs(diligence));

			soldierEffort *= diligenceFactor;
			if (statsN > 0)
				soldierEffort /= statsN;
			effort += soldierEffort;
			summedEfficiency += stats->efficiency;
		}
		_efficiency = contributingEngineers > 0 ? static_cast<int>(summedEfficiency / contributingEngineers) : 0;

		effort *= loyaltyRating; //not normalizing by 100 to fit small hourly values into integer later
		effort *= speedFactor;
		progress = std::max(0, static_cast<int>(effort));
	}
	else
	{
		_efficiency = 0;
	}
	return progress;
}

productionProgress_e Production::step(Base * b, SavedGame * g, const Mod *m, Language *lang, int rating)
{
	if (!_infinite && _amountProduced >= _amount)
	{
		return PROGRESS_COMPLETE;
	}

	if (!haveRequiredBaseFunctions(b))
	{
		if (!_baseFunctionsMissingReported)
		{
			_baseFunctionsMissingReported = true;
			return PROGRESS_NOT_ENOUGH_BASE_FUNCTIONS;
		}
		return PROGRESS_NOT_COMPLETE;
	}
	_baseFunctionsMissingReported = false;

	if (!_currentItemStarted)
	{
		getProgress(b, g, m, rating, true);
		const productionProgress_e status = startItem(b, g, m);
		if (status != PROGRESS_NOT_COMPLETE)
		{
			return status;
		}
	}

	_timeSpent += getProgress(b, g, m, rating);
	const int64_t timePerUnit = _rules->getManufactureTime();
	while (_currentItemStarted && _timeSpent >= (static_cast<int64_t>(_amountProduced) + 1) * timePerUnit)
	{
		produceItem(b, g, m, lang);
		++_amountProduced;
		_currentItemStarted = false;
		_currentUnitCost = 0;

		if (!_infinite && _amountProduced >= _amount)
		{
			_timeSpent = static_cast<int64_t>(_amountProduced) * timePerUnit;
			return PROGRESS_COMPLETE;
		}

		const productionProgress_e status = startItem(b, g, m);
		if (status != PROGRESS_NOT_COMPLETE)
		{
			// Work cannot be credited to an item whose resources were never committed.
			_timeSpent = static_cast<int64_t>(_amountProduced) * timePerUnit;
			return status;
		}
	}
	return PROGRESS_NOT_COMPLETE;
}

int Production::getAmountProduced() const
{
	return _amountProduced;
}

void Production::produceItem(Base *b, SavedGame *g, const Mod *m, Language *lang)
{
	const int transferTimeItems = std::max(0, _rules->getTransferTimes().empty() ? 0 : _rules->getTransferTimes().front());
	auto deliverItem = [&](const RuleItem *item, int quantity)
	{
		if (transferTimeItems > 0)
		{
			Transfer *transfer = new Transfer(transferTimeItems);
			transfer->setItems(item, quantity);
			b->getTransfers()->push_back(transfer);
		}
		else
		{
			b->getStorageItems()->addItem(item, quantity);
			if (item->getBattleType() == BT_NONE)
			{
				for (auto *craft : *b->getCrafts())
				{
					craft->reuseItem(item);
				}
			}
		}
	};

	if (const RuleCraft *ruleCraft = _rules->getProducedCraft())
	{
		Craft *craft = new Craft(ruleCraft, b, g->getId(ruleCraft->getType()));
		craft->initFixedWeapons(m);
		craft->checkup();
		const int transferTimeCraft = std::max(0, _rules->getTransferTimes().size() < 3 ? 0 : _rules->getTransferTimes().at(2));
		if (transferTimeCraft > 0)
		{
			Transfer *transfer = new Transfer(transferTimeCraft);
			transfer->setCraft(craft);
			b->getTransfers()->push_back(transfer);
		}
		else
		{
			b->getCrafts()->push_back(craft);
		}
	}
	else
	{
		for (const auto &item : _rules->getProducedItems())
		{
			deliverItem(item.first, item.second);
			if (!_rules->getRandomProducedItems().empty())
			{
				_randomProductionInfo[item.first->getType()] += item.second;
			}
		}
	}

	if (!_rules->getRandomProducedItems().empty())
	{
		int totalWeight = 0;
		for (const auto &itemSet : _rules->getRandomProducedItems())
		{
			totalWeight += itemSet.first;
		}
		const int roll = RNG::generate(1, totalWeight);
		int runningTotal = 0;
		for (const auto &itemSet : _rules->getRandomProducedItems())
		{
			runningTotal += itemSet.first;
			if (runningTotal >= roll)
			{
				for (const auto &item : itemSet.second)
				{
					deliverItem(item.first, item.second);
					_randomProductionInfo[item.first->getType()] += item.second;
				}
				break;
			}
		}
	}

	const std::string &spawnedPersonType = _rules->getSpawnedPersonType();
	if (!spawnedPersonType.empty())
	{
		if (const RuleSoldier *rule = m->getSoldier(spawnedPersonType))
		{
			Soldier *soldier = m->genSoldier(g, rule);
			YAML::YamlRootNodeReader reader(_rules->getSpawnedSoldierTemplate(), "(spawned soldier template)");
			soldier->load(reader, m, g, m->getScriptGlobal(), true);
			if (!_rules->getSpawnedPersonName().empty())
			{
				soldier->setName(lang->getString(_rules->getSpawnedPersonName()));
			}
			else
			{
				soldier->genName();
			}
			b->getSoldiers()->push_back(soldier);
		}
	}
	if (_rules->getFundsGain() > 0)
	{
		g->setFunds(g->getFunds() + _rules->getFundsGain());
	}
	if (_rules->getPoints() != 0)
	{
		g->addResearchScore(_rules->getPoints());
	}
}

const RuleManufacture * Production::getRules() const
{
	return _rules;
}

productionProgress_e Production::startItem(Base * b, SavedGame * g, const Mod *m)
{
	if (_currentItemStarted)
	{
		return PROGRESS_NOT_COMPLETE;
	}
	if (!haveRequiredBaseFunctions(b))
	{
		return PROGRESS_NOT_ENOUGH_BASE_FUNCTIONS;
	}
	if (!haveEnoughMoneyForOneMoreUnit(g))
	{
		return PROGRESS_NOT_ENOUGH_MONEY;
	}
	if (!haveEnoughLivingSpaceForOneMoreUnit(b))
	{
		return PROGRESS_NOT_ENOUGH_LIVING_SPACE;
	}
	if (!haveEnoughMaterialsForOneMoreUnit(b, m))
	{
		return PROGRESS_NOT_ENOUGH_MATERIALS;
	}

	for (const auto& i : _rules->getRequiredCrafts())
	{
		for (int count = 0; count < i.second; ++count)
		{
				auto craft = std::find_if(b->getCrafts()->begin(), b->getCrafts()->end(),
					[&](const Craft *candidate)
					{
						return candidate->getRules() == i.first && candidate->getStatus() != "STR_OUT";
					});
			if (craft == b->getCrafts()->end())
			{
				throw Exception("Production materials changed during an atomic craft transaction.");
			}
			Craft *consumedCraft = *craft;
			b->removeCraft(consumedCraft, true);
			delete consumedCraft;
		}
	}
	_currentUnitCost = getUnitCost();
	g->setFunds(g->getFunds() - _currentUnitCost);
	for (const auto& i : _rules->getRequiredItems())
	{
		b->getStorageItems()->removeItem(i.first, i.second);
	}
	_currentItemStarted = true;
	return PROGRESS_NOT_COMPLETE;
}

void Production::refundItem(Base * b, SavedGame * g, const Mod *m)
{
	(void)m;
	if (!_currentItemStarted)
	{
		return;
	}
	g->setFunds(g->getFunds() + _currentUnitCost);
	for (const auto& pair : _rules->getRequiredItems())
	{
		b->getStorageItems()->addItem(pair.first, pair.second);
	}
	_currentItemStarted = false;
	_currentUnitCost = 0;
}

void Production::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("item", getRules()->getName());
	writer.write("spent", getTimeSpent());
	writer.write("amount", getAmountTotal());
	writer.write("infinite", getInfiniteAmount());
	writer.write("efficiency", _efficiency);
	writer.write("amountProduced", _amountProduced);
	writer.write("currentItemStarted", _currentItemStarted);
	writer.write("currentUnitCost", _currentUnitCost);
	if (_baseFunctionsMissingReported)
		writer.write("baseFunctionsMissingReported", _baseFunctionsMissingReported);
	if (!_rules->getRandomProducedItems().empty())
		writer.write("randomProductionInfo", _randomProductionInfo);
}

void Production::load(const YAML::YamlNodeReader& reader)
{
	if (!reader["amountProduced"] || !reader["currentItemStarted"] || !reader["currentUnitCost"])
	{
		throw Exception("Unsupported production save format for '" + _rules->getName() + "'.");
	}
	setTimeSpent(reader["spent"].readVal(getTimeSpent()));
	setAmountTotal(reader["amount"].readVal(getAmountTotal()));
	setInfiniteAmount(reader["infinite"].readVal(getInfiniteAmount()));
	setEfficiency(reader["efficiency"].readVal(getEfficiency()));
	reader.tryRead("amountProduced", _amountProduced);
	reader.tryRead("currentItemStarted", _currentItemStarted);
	reader.tryRead("currentUnitCost", _currentUnitCost);
	reader.tryRead("baseFunctionsMissingReported", _baseFunctionsMissingReported);
	const int64_t completedWork = static_cast<int64_t>(_amountProduced) * _rules->getManufactureTime();
	if (_amountProduced < 0 || _currentUnitCost < 0 || (!_infinite && _amountProduced > _amount) || _timeSpent < 0
		|| (!_currentItemStarted && _currentUnitCost != 0)
		|| (!_infinite && _amountProduced >= _amount && _currentItemStarted)
		|| _timeSpent < completedWork
		|| (_currentItemStarted && _timeSpent >= completedWork + _rules->getManufactureTime())
		|| (!_currentItemStarted && _timeSpent != completedWork))
	{
		throw Exception("Invalid production state for '" + _rules->getName() + "'.");
	}
	if (!_rules->getRandomProducedItems().empty())
	{
		_randomProductionInfo = reader["randomProductionInfo"].readVal(_randomProductionInfo);
		for (const auto &entry : _randomProductionInfo)
		{
			if (entry.second < 0)
			{
				throw Exception("Invalid random production history for '" + _rules->getName() + "'.");
			}
		}
	}
}

}
