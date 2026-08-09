#pragma once
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
#include "../Engine/Yaml.h"
#include <cstdint>

namespace OpenXcom
{
class RuleManufacture;
class Base;
class SavedGame;
class Language;
class Mod;
class Soldier;
enum productionProgress_e { PROGRESS_NOT_COMPLETE, PROGRESS_COMPLETE, PROGRESS_NOT_ENOUGH_MONEY, PROGRESS_NOT_ENOUGH_MATERIALS, PROGRESS_NOT_ENOUGH_LIVING_SPACE, PROGRESS_NOT_ENOUGH_BASE_FUNCTIONS, PROGRESS_MAX, PROGRESS_CONSTRUCTION };

class Production
{
public:
	Production (const RuleManufacture * rules, int amount);
	int getAmountTotal() const{ return _amount; }
	void setAmountTotal(int amount);
	bool getInfiniteAmount() const{ return _infinite; }
	void setInfiniteAmount(bool infinite);
	int64_t getTimeSpent() const{ return _timeSpent; }
	void setTimeSpent(int64_t timeSpent){ _timeSpent = timeSpent; }
	int getAmountProduced() const;
	int getEfficiency() const { return _efficiency; }
	void setEfficiency(int efficiency) { _efficiency = efficiency; }
	static int64_t calculateUnitCost(int64_t baseCost, int efficiency);
	int64_t getUnitCost() const;
	/// Cost actually committed for the in-progress unit, or the next-unit estimate.
	int64_t getDisplayedUnitCost() const { return _currentItemStarted ? _currentUnitCost : getUnitCost(); }
	bool hasCurrentItemStarted() const { return _currentItemStarted; }
	std::vector<Soldier*> getAssignedSoldiers(Base* b);
	int getProgress(Base* b, SavedGame* g, const Mod* m, int loyalty, bool prediction = false);
	
	productionProgress_e step(Base * b, SavedGame * g, const Mod *m, Language *lang, int rating);
	const RuleManufacture * getRules() const;
	productionProgress_e startItem(Base * b, SavedGame * g, const Mod *m);
	void refundItem(Base * b, SavedGame * g, const Mod *m);
	void save(YAML::YamlNodeWriter writer) const;
	void load(const YAML::YamlNodeReader& reader);
	const std::map<std::string, int> &getRandomProductionInfo() const { return _randomProductionInfo; }
private:
	const RuleManufacture * _rules;
	int _amount;
	bool _infinite;
	int64_t _timeSpent;
	int _amountProduced;
	int _efficiency;
	bool _currentItemStarted;
	int64_t _currentUnitCost;
	bool _baseFunctionsMissingReported;
	std::map<std::string, int> _randomProductionInfo;
	bool haveEnoughMoneyForOneMoreUnit(SavedGame * g) const;
	bool haveRequiredBaseFunctions(Base * b) const;
	bool haveEnoughLivingSpaceForOneMoreUnit(Base * b);
	bool haveEnoughMaterialsForOneMoreUnit(Base * b, const Mod *m) const;
	void produceItem(Base *b, SavedGame *g, const Mod *m, Language *lang);
};

}
