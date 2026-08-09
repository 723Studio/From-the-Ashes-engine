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
#include "RuleCovertOperation.h"
#include "RuleAlienMission.h"
#include "../Engine/RNG.h"
#include "../Engine/Exception.h"
#include "Mod.h"

namespace OpenXcom
{

/**
* Creates a blank ruleset for a certain
* type of RuleCovertOperation.
* @param type String defining the type.
*/
RuleCovertOperation::RuleCovertOperation(const std::string& name) : _name(name), _soldiersMin(1), _soldiersMax(1), _optionalSoldierEffect(0),
	_armorEffect(20), _baseChances(50), _costs(0), _successScore(0), _failureScore(0),
	_successLoyalty(0), _failureLoyalty(0), _successFunds(0), _failureFunds(0), _counterValue(1), _danger(0), _trapChance(0),
	_progressEventChance(0), _concealedItemsBonus(20), _bonusItemsEffect(5), _repeatProgressEvent(false), _allowAllEquipment(false),
	_removeRequiredItemsOnSuccess(true), _removeRequiredItemsOnFailure(false), _listOrder(0) 
{
}

RuleCovertOperation::~RuleCovertOperation()
{
}

/**
* Loads the craft from a YAML file.
* @param node YAML node.
* @param mod Mod for the CovertOperation.
* @param modIndex A value that offsets the sounds and sprite values to avoid conflicts.
* @param listOrder The list weight for this CovertOperation.
*/
void RuleCovertOperation::load(const YAML::YamlNodeReader& node, Mod* mod, int listOrder)
{
	const auto& reader = node.useIndex();
	if (const YAML::YamlNodeReader& parent = reader["refNode"])
	{
		load(reader["refNode"], mod, listOrder);
	}
	reader.tryRead("name", _name);
	reader.tryRead("description", _description);
	reader.tryRead("categories", _categories);
	reader.tryRead("successBackground", _successBackground);
	reader.tryRead("failureBackground", _failureBackground);
	reader.tryRead("successDescription", _successDescription);
	reader.tryRead("failureDescription", _failureDescription);
	reader.tryRead("cancelledDescription", _cancelledDescription);
	reader.tryRead("successMusic", _successMusic);
	reader.tryRead("failureMusic", _failureMusic);
	reader.tryRead("successEvent", _successEvent);
	reader.tryRead("failureEvent", _failureEvent);
	if (reader["progressEvent"])
	{
		_progressEvent.load(reader["progressEvent"]);
	}
	reader.tryRead("repeatProgressEvent", _repeatProgressEvent);
	reader.tryRead("requires", _requires);
	reader.tryRead("disabledByResearch", _disabledByResearch);
	reader.tryRead("allowedRoles", _allowedRoles);
	mod->loadBaseFunction(_name, _requiresBaseFunc, reader["requiresBaseFunc"]);
	reader.tryRead("soldiersMin", _soldiersMin);
	if (_soldiersMin < 1)
	{
		throw Exception("Error in loading operation '" + _name + "'! It must have at least 1 soldier!");
	}
	reader.tryRead("soldiersMax", _soldiersMax);
	if (_soldiersMax == 0)
	{
		_soldiersMax = _soldiersMin;
	}
	else if (_soldiersMax < _soldiersMin)
	{
		throw Exception("Error in loading operation '" + _name + "'! soldiersMax < _soldiersMin!");
	}
	reader.tryRead("optionalSoldierEffect", _optionalSoldierEffect);
	if (_optionalSoldierEffect < 0)
	{
		throw Exception("Error in loading operation '" + _name + "'! optionalSoldierEffect must be greater than or equal to 0.");
	}
	reader.tryRead("baseChances", _baseChances);
	if (_baseChances < 0 || _baseChances > 200)
	{
		throw Exception("Error in loading operation '" + _name + "'! baseChances must be between 0 and 200.");
	}
	reader.tryRead("costs", _costs);
	if (_costs <= 0)
	{
		throw Exception("Error in loading operation '" + _name + "'! costs must be greater than 0.");
	}
	reader.tryRead("progressEventChance", _progressEventChance);
	if (_progressEventChance < 0 || _progressEventChance > 100)
	{
		throw Exception("Error in loading operation '" + _name + "'! progressEventChance must be between 0 and 100.");
	}
	reader.tryRead("trapChance", _trapChance);
	if (_trapChance < 0 || _trapChance > 100)
	{
		throw Exception("Error in loading operation '" + _name + "'! trapChance must be between 0 and 100.");
	}
	reader.tryRead("danger", _danger);
	if (_danger < 0)
	{
		throw Exception("Error in loading operation '" + _name + "'! danger must be greater than or equal to 0.");
	}
	reader.tryRead("successScore", _successScore);
	reader.tryRead("failureScore", _failureScore);
	reader.tryRead("successLoyalty", _successLoyalty);
	reader.tryRead("failureLoyalty", _failureLoyalty);
	reader.tryRead("successFunds", _successFunds);
	reader.tryRead("failureFunds", _failureFunds);
	reader.tryRead("successItemNames", _successItemNames);
	reader.tryRead("failureItemNames", _failureItemNames);
	reader.tryRead("addSoldiers", _addSoldiersStr);
	reader.tryRead("successResearchList", _successResearchList);
	reader.tryRead("failureResearchList", _failureResearchList);
	reader.tryRead("decreaseCounter", _decreaseCounter);
	reader.tryRead("increaseCounter", _increaseCounter);
	reader.tryRead("counterValue", _counterValue);
	if (_counterValue < 1)
	{
		throw Exception("Error in loading operation '" + _name + "'! counterValue must be greater than 0.");
	}
	if (reader["successMissions"])
	{
		_successMissions.load(reader["successMissions"]);
	}
	if (reader["failureMissions"])
	{
		_failureMissions.load(reader["failureMissions"]);
	}
	if (reader["instantTrapDeployment"])
	{
		_instantTrapDeployment.load(reader["instantTrapDeployment"]);
	}
	if (reader["instantSuccessDeployment"])
	{
		_instantSuccessDeployment.load(reader["instantSuccessDeployment"]);
	}
	reader.tryRead("requiredReputationLvl", _requiredReputationLvl);
	reader.tryRead("successReputationScore", _successReputationScore);
	reader.tryRead("failureReputationScore", _failureReputationScore);
	reader.tryRead("requiredItems", _requiredItemsStr);
	reader.tryRead("bonusItems", _bonusItemsStr);
	reader.tryRead("bonusItemsEffect", _bonusItemsEffect);
	if (_bonusItemsEffect < 0)
	{
		throw Exception("Error in loading operation '" + _name + "'! bonusItemsEffect must be greater than or equal to 0.");
	}
	reader.tryRead("allowAllEquipment", _allowAllEquipment);
	reader.tryRead("removeRequiredItemsOnSuccess", _removeRequiredItemsOnSuccess);
	reader.tryRead("removeRequiredItemsOnFailure", _removeRequiredItemsOnFailure);
	reader.tryRead("concealedItemsBonus", _concealedItemsBonus);
	if (_concealedItemsBonus < 0)
	{
		throw Exception("Error in loading operation '" + _name + "'! concealedItemsBonus must be greater than or equal to 0.");
	}
	reader.tryRead("allowedArmor", _allowedArmor);
	reader.tryRead("armorEffect", _armorEffect);
	if (_armorEffect < 0)
	{
		throw Exception("Error in loading operation '" + _name + "'! armorEffect must be greater than or equal to 0.");
	}
	reader.tryRead("soldierTypeEffectiveness", _soldierTypeEffectiveness);
	reader.tryRead("specialRule", _specialRule);
	if (!_listOrder)
	{
		_listOrder = listOrder;
	}
}

/**
 * Cross link with other rules
 */
void RuleCovertOperation::afterLoad(const Mod* mod)
{
	if (_categories.empty())
	{
		throw Exception("Covert operation named: '" + this->getName() + "' must define at least one category");
	}
	for (size_t i = 0; i < _categories.size(); ++i)
	{
		const auto& category = _categories[i];
		if (category != "STR_INVESTIGATION" && category != "STR_INFILTRATION" &&
			category != "STR_NEGOTIATION" && category != "STR_DECEPTION")
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has unsupported category: '" + category + "'");
		}
		for (size_t j = i + 1; j < _categories.size(); ++j)
		{
			if (category == _categories[j])
			{
				throw Exception("Covert operation named: '" + this->getName() + "' has duplicate category: '" + category + "'");
			}
		}
	}
	auto validateUniqueStrings = [this](const std::vector<std::string>& values, const std::string& field)
	{
		for (size_t i = 0; i < values.size(); ++i)
		{
			for (size_t j = i + 1; j < values.size(); ++j)
			{
				if (values[i] == values[j])
				{
					throw Exception("Covert operation named: '" + this->getName() + "' has duplicate " + field + " entry: '" + values[i] + "'");
				}
			}
		}
	};
	validateUniqueStrings(_requires, "requires");
	validateUniqueStrings(_disabledByResearch, "disabledByResearch");
	validateUniqueStrings(_successResearchList, "successResearchList");
	validateUniqueStrings(_failureResearchList, "failureResearchList");
	validateUniqueStrings(_decreaseCounter, "decreaseCounter");
	validateUniqueStrings(_increaseCounter, "increaseCounter");
	validateUniqueStrings(_allowedArmor, "allowedArmor");
	for (const auto& research : _requires)
	{
		if (!mod->getResearch(research))
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid requires entry: research '" + research + "' is not found");
		}
	}
	for (const auto& research : _disabledByResearch)
	{
		for (const auto& requirement : _requires)
		{
			if (research == requirement)
			{
				throw Exception("Covert operation named: '" + this->getName() + "' has research '" + research + "' in both requires and disabledByResearch");
			}
		}
		if (!mod->getResearch(research))
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid disabledByResearch entry: research '" + research + "' is not found");
		}
	}
	if (!_disabledByResearch.empty() && _cancelledDescription.empty())
	{
		throw Exception("Covert operation named: '" + this->getName() + "' defines disabledByResearch without cancelledDescription");
	}
	for (const auto& research : _successResearchList)
	{
		if (!mod->getResearch(research))
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid successResearchList entry: research '" + research + "' is not found");
		}
	}
	for (const auto& research : _failureResearchList)
	{
		if (!mod->getResearch(research))
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid failureResearchList entry: research '" + research + "' is not found");
		}
	}
	for (const auto& armor : _allowedArmor)
	{
		if (!mod->getArmor(armor))
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid allowedArmor entry: armor '" + armor + "' is not found");
		}
	}
	for (const auto& role : _allowedRoles)
	{
		if (role < ROLE_NONE || role > ROLE_ENGINEER)
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid allowedRoles value: '" + std::to_string(role) + "'");
		}
	}
	for (size_t i = 0; i < _allowedRoles.size(); ++i)
	{
		for (size_t j = i + 1; j < _allowedRoles.size(); ++j)
		{
			if (_allowedRoles[i] == _allowedRoles[j])
			{
				throw Exception("Covert operation named: '" + this->getName() + "' has duplicate allowedRoles value: '" + std::to_string(_allowedRoles[i]) + "'");
			}
		}
	}
	for (const auto& effectiveness : _soldierTypeEffectiveness)
	{
		if (!mod->getSoldier(effectiveness.first))
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid soldierTypeEffectiveness entry: soldier type '" + effectiveness.first + "' is not found");
		}
		if (effectiveness.second < 0)
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid soldierTypeEffectiveness for '" + effectiveness.first + "': value must be greater than or equal to 0");
		}
	}
	auto validateFactionMap = [this, mod](const std::map<std::string, int>& values, const std::string& field)
	{
		for (const auto& value : values)
		{
			if (!mod->getDiplomacyFaction(value.first))
			{
				throw Exception("Covert operation named: '" + this->getName() + "' has invalid " + field + " entry: faction '" + value.first + "' is not found");
			}
		}
	};
	validateFactionMap(_requiredReputationLvl, "requiredReputationLvl");
	validateFactionMap(_successReputationScore, "successReputationScore");
	validateFactionMap(_failureReputationScore, "failureReputationScore");
	for (const auto& event : _progressEvent.getNames())
	{
		if (!mod->getEvent(event))
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid progressEvent entry: event '" + event + "' is not found");
		}
	}
	for (const auto& mission : _successMissions.getNames())
	{
		const RuleAlienMission* rule = mod->getAlienMission(mission, true);
		rule->validateDirectSpawn();
		_linkedAlienMissions[mission] = rule;
	}
	for (const auto& mission : _failureMissions.getNames())
	{
		const RuleAlienMission* rule = mod->getAlienMission(mission, true);
		rule->validateDirectSpawn();
		_linkedAlienMissions[mission] = rule;
	}
	for (const auto& deployment : _instantTrapDeployment.getNames())
	{
		if (!mod->getDeployment(deployment))
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid instantTrapDeployment entry: deployment '" + deployment + "' is not found");
		}
	}
	for (const auto& deployment : _instantSuccessDeployment.getNames())
	{
		if (!mod->getDeployment(deployment))
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid instantSuccessDeployment entry: deployment '" + deployment + "' is not found");
		}
	}
	if (!_specialRule.empty() && _specialRule != "STR_REGIONAL_HQ_DISCOVERY")
	{
		throw Exception("Covert operation named: '" + this->getName() + "' has unsupported specialRule: '" + _specialRule + "'");
	}
	if (!_successEvent.empty() && !mod->getEvent(_successEvent))
	{
		throw Exception("Cover operation named: '" + this->getName() + "' has broken link in successEvent: '" + _successEvent +"' is not found!");
	}
	if (!_failureEvent.empty() && !mod->getEvent(_failureEvent))
	{
		throw Exception("Cover operation named: '" + this->getName() + "' has broken link in failureEvent: '" + _failureEvent + "' is not found!");
	}
	if (!_addSoldiersStr.empty())
	{
		for (auto &addSoldier: _addSoldiersStr)
		{
			const RuleSoldier* soldier = mod->getSoldier(addSoldier.first);
			if (soldier != 0)
			{
				if (addSoldier.second < 1)
				{
					throw Exception("Cover operation named: '" + this->getName() + " has invalid addSoldiers property - second value of the map must be greater than 0.");
				}
				_addSoldiers.insert_or_assign(soldier, addSoldier.second);
			}
			else
			{
				throw Exception("Cover operation named: '" + this->getName() + " has invalid addSoldiers property - no rule for soldier: '" + addSoldier.first + "'");
			}
			
		}
	}
	if (!_requiredItemsStr.empty())
	{
		for (auto& item : _requiredItemsStr)
		{
			const RuleItem* rule = mod->getItem(item.first);
			if (rule)
			{
				if (item.second < 1)
				{
					throw Exception("Cover operation named: '" + this->getName() + " has invalid requiredItems property - second value of the map must be greater than 0.");
				}
				_requiredItems.insert_or_assign(rule, item.second);
			}
			else
			{
				throw Exception("Cover operation named: '" + this->getName() + " has invalid requiredItems property - no rule for item: '" + item.first + "'");
			}
		}
	}
	if (!_bonusItemsStr.empty())
	{
		for (auto& item : _bonusItemsStr)
		{
			const RuleItem* rule = mod->getItem(item.first);
			if (rule)
			{
				if (item.second < 1)
				{
					throw Exception("Cover operation named: '" + this->getName() + " has invalid bonusItems property - second value of the map must be greater than 0.");
				}
				_bonusItems.insert_or_assign(rule, item.second);
			}
			else
			{
				throw Exception("Cover operation named: '" + this->getName() + " has invalid bonusItems property - no rule for item: '" + item.first + "'");
			}
		}
	}

	for (auto& itemSet : _successItemNames)
	{
		if (itemSet.first < 1)
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid successItemNames weight: value must be greater than 0");
		}
		std::map<const RuleItem*, int> tmp;
		for (auto& i : itemSet.second)
		{
			if (i.second < 1)
			{
				throw Exception("Covert operation named: '" + this->getName() + "' has invalid successItemNames quantity for '" + i.first + "': value must be greater than 0");
			}
			auto item = mod->getItem(i.first, true);
			if (!item)
			{
				throw Exception("Cover operation named: '" + this->getName() + " has invalid successItemNames property - no rule for item: '" + i.first + "'");
			}
			else
			{
				tmp[item] = i.second;
			}
		}
		_successItems.push_back(std::make_pair(itemSet.first, tmp));
	}

	for (auto& itemSet : _failureItemNames)
	{
		if (itemSet.first < 1)
		{
			throw Exception("Covert operation named: '" + this->getName() + "' has invalid failureItemNames weight: value must be greater than 0");
		}
		std::map<const RuleItem*, int> tmp;
		for (auto& i : itemSet.second)
		{
			if (i.second < 1)
			{
				throw Exception("Covert operation named: '" + this->getName() + "' has invalid failureItemNames quantity for '" + i.first + "': value must be greater than 0");
			}
			auto item = mod->getItem(i.first, true);
			if (!item)
			{
				throw Exception("Cover operation named: '" + this->getName() + " has invalid failureItemNames property - no rule for item: '" + i.first + "'");
			}
			else
			{
				tmp[item] = i.second;
			}
		}
		_failureItems.push_back(std::make_pair(itemSet.first, tmp));
	}

	//remove not needed data
	Collections::removeAll(_addSoldiersStr);
	Collections::removeAll(_requiredItemsStr);
	Collections::removeAll(_bonusItemsStr);
	Collections::removeAll(_successItemNames);
	Collections::removeAll(_failureItemNames);
}

const RuleAlienMission* RuleCovertOperation::chooseGenSuccessMission() const
{
	const std::string mission = _successMissions.choose();
	return mission.empty() ? nullptr : _linkedAlienMissions.at(mission);
}

const RuleAlienMission* RuleCovertOperation::chooseGenFailureMission() const
{
	const std::string mission = _failureMissions.choose();
	return mission.empty() ? nullptr : _linkedAlienMissions.at(mission);
}

std::string RuleCovertOperation::chooseGenInstantTrapDeploymentType() const
{
	return _instantTrapDeployment.choose();
}

std::string RuleCovertOperation::chooseGenInstantSuccessDeploymentType() const
{
	return _instantSuccessDeployment.choose();
}

std::string RuleCovertOperation::chooseProgressEvent() const
{
	return _progressEvent.choose();
}

}

