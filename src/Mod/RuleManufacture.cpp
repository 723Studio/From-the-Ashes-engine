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
#include <algorithm>
#include <climits>
#include <cstdint>
#include "RuleManufacture.h"
#include "RuleResearch.h"
#include "RuleCraft.h"
#include "RuleItem.h"
#include "Mod.h"
#include "../Engine/Collections.h"
#include "../Engine/Exception.h"

namespace OpenXcom
{
/**
 * Creates a new Manufacture.
 * @param name The unique manufacture name.
 */
RuleManufacture::RuleManufacture(const std::string& name, int listOrder)
	: _name(name), _space(0), _time(0), _cost(0), _fundsGain(0), _points(0), _refund(false), _producedCraft(0), _listOrder(listOrder)
{
	_producedItemsNames[name] = 1;
}

/**
 * Loads the manufacture project from a YAML file.
 * @param node YAML node.
 * @param listOrder The list weight for this manufacture.
 */
void RuleManufacture::load(const YAML::YamlNodeReader& node, Mod* mod)
{
	const auto& reader = node.useIndex();
	if (const auto& parent = reader["refNode"])
	{
		load(parent, mod);
	}

	reader.tryRead("category", _category);
	mod->loadUnorderedNames(_name, _requiresName, reader["requires"]);
	mod->loadBaseFunction(_name, _requiresBaseFunc, reader["requiresBaseFunc"]);
	reader.tryRead("space", _space);
	reader.tryRead("time", _time);
	reader.tryRead("cost", _cost);
	reader.tryRead("fundsGain", _fundsGain);
	_stats.merge(reader["stats"].readVal(_stats));
	reader.tryRead("points", _points);
	reader.tryRead("refund", _refund);
	mod->loadUnorderedNamesToInt(_name, _requiredItemsNames, reader["requiredItems"]);
	mod->loadUnorderedNamesToInt(_name, _producedItemsNames, reader["producedItems"]);
	reader.tryRead("randomProducedItems", _randomProducedItemsNames);
	reader.tryRead("spawnedPersonType", _spawnedPersonType);
	reader.tryRead("spawnedPersonName", _spawnedPersonName);
	if (reader["spawnedSoldier"])
	{
		_spawnedSoldier = reader["spawnedSoldier"].emitDescendants(YAML::YamlRootNodeReader(_spawnedSoldier, "(spawned soldier template)"));
	}
	reader.tryRead("transferTimes", _transferTimes);
	reader.tryRead("listOrder", _listOrder);
}

/**
 * Cross link with other Rules.
 */
void RuleManufacture::afterLoad(const Mod* mod)
{
	if (_space < 0)
	{
		throw Exception("Manufacture '" + _name + "' has negative workshop space.");
	}
	if (_time <= 0)
	{
		throw Exception("Manufacture '" + _name + "' time must be greater than zero.");
	}
	if (_cost < 0)
	{
		throw Exception("Manufacture '" + _name + "' has negative cost; use fundsGain for completion income.");
	}
	if (_fundsGain < 0)
	{
		throw Exception("Manufacture '" + _name + "' has negative fundsGain.");
	}
	if (_cost > 0 && _fundsGain > 0)
	{
		throw Exception("Manufacture '" + _name + "' cannot define both positive cost and fundsGain.");
	}

	_requires = mod->getResearch(_requiresName);
	if (_category == "STR_CRAFT")
	{
		if (_producedItemsNames.size() != 1)
		{
			throw Exception("Craft manufacture '" + _name + "' must define exactly one produced craft.");
		}
		auto item = _producedItemsNames.begin();
		if (item == _producedItemsNames.end())
		{
			throw Exception("No craft defined for production");
		}
		else if (item->second != 1)
		{
			throw Exception("Craft manufacture '" + _name + "' must produce exactly one craft per project.");
		}
		else
		{
			_producedCraft = mod->getCraft(item->first, true);
		}
	}
	else
	{
		for (auto& i : _producedItemsNames)
		{
			if (i.second <= 0)
			{
				throw Exception("Manufacture '" + _name + "' has a non-positive produced item quantity.");
			}
			_producedItems[mod->getItem(i.first, true)] = i.second;
		}
	}

	const int taskStats[] =
	{
		_stats.weaponry, _stats.explosives, _stats.microelectronics,
		_stats.metallurgy, _stats.processing, _stats.hacking, _stats.robotics,
		_stats.alienTech, _stats.reverseEngineering
	};
	bool hasPositiveTaskStat = false;
	for (const int value : taskStats)
	{
		if (value < 0)
		{
			throw Exception("Manufacture '" + _name + "' has a negative engineering task stat.");
		}
		hasPositiveTaskStat = hasPositiveTaskStat || value > 0;
	}
	if (!hasPositiveTaskStat)
	{
		throw Exception("Manufacture '" + _name + "' must define at least one positive supported engineering task stat.");
	}

	for (auto& i : _requiredItemsNames)
	{
		if (i.second <= 0)
		{
			throw Exception("Manufacture '" + _name + "' has a non-positive required item quantity.");
		}
		auto* itemRule = mod->getItem(i.first, false);
		auto* craftRule = mod->getCraft(i.first, false);
		if (itemRule)
		{
			_requiredItems[itemRule] = i.second;
		}
		else if (craftRule)
		{
			_requiredCrafts[craftRule] = i.second;
		}
		else
		{
			throw Exception("Unknown required item '" + i.first + "'");
		}
	}

	int64_t totalRandomWeight = 0;
	for (auto& itemSet : _randomProducedItemsNames)
	{
		if (itemSet.first <= 0 || itemSet.second.empty())
		{
			throw Exception("Manufacture '" + _name + "' has an invalid random production entry.");
		}
		totalRandomWeight += itemSet.first;
		if (totalRandomWeight > INT_MAX)
		{
			throw Exception("Manufacture '" + _name + "' has excessive random production weight.");
		}
		std::map<const RuleItem*, int> tmp;
		for (auto& i : itemSet.second)
		{
			if (i.second <= 0)
			{
				throw Exception("Manufacture '" + _name + "' has a non-positive random output quantity.");
			}
			tmp[mod->getItem(i.first, true)] = i.second;
		}
		_randomProducedItems.push_back(std::make_pair(itemSet.first, tmp));
	}

	if (!_spawnedPersonType.empty())
	{
		mod->getSoldier(_spawnedPersonType, true);
	}
	if (_refund && !_requiredCrafts.empty())
	{
		throw Exception("Manufacture '" + _name + "' cannot refund consumed crafts safely; disable refund for this project.");
	}

	//remove not needed data
	Collections::removeAll(_requiresName);
	Collections::removeAll(_producedItemsNames);
	Collections::removeAll(_requiredItemsNames);
	Collections::removeAll(_randomProducedItemsNames);
}


/**
 * Gets the unique name of the manufacture.
 * @return The name.
 */
const std::string &RuleManufacture::getName() const
{
	return _name;
}

/**
 * Gets the category shown in the manufacture list.
 * @return The category.
 */
const std::string &RuleManufacture::getCategory() const
{
	return _category;
}

/**
 * Gets the list of research required to
 * manufacture this object.
 * @return A list of research IDs.
 */
const std::vector<const RuleResearch*> &RuleManufacture::getRequirements() const
{
	return _requires;
}

/**
 * Gets the required workspace to start production.
 * @return The required workspace.
 */
int RuleManufacture::getRequiredSpace() const
{
	return _space;
}

/**
 * Gets the time needed to manufacture one object.
 * @return The time needed to manufacture one object (in man/hour).
 */
int RuleManufacture::getManufactureTime() const
{
	return _time;
}


/**
 * Gets the cost of manufacturing one object.
 * @return The cost of manufacturing one object.
 */
int RuleManufacture::getManufactureCost() const
{
	return _cost;
}

/**
 * Should all resources of a cancelled project be refunded?
 * @return True, if all resources should be refunded. False otherwise.
 */
bool RuleManufacture::getRefund() const
{
	return _refund;
}

/**
 * Gets the list of items required to manufacture one object.
 * @return The list of items required to manufacture one object.
 */
const std::map<const RuleItem*, int> &RuleManufacture::getRequiredItems() const
{
	return _requiredItems;
}

/**
 * Gets the list of crafts required to manufacture one object.
 */
const std::map<const RuleCraft*, int> &RuleManufacture::getRequiredCrafts() const
{
	return _requiredCrafts;
}

/**
 * Gets the list of items produced by completing "one object" of this project.
 * @return The list of items produced by completing "one object" of this project.
 */
const std::map<const RuleItem*, int> &RuleManufacture::getProducedItems() const
{
	return _producedItems;
}

/*
 * Gets craft build by this project. null if is not craft production.
 * @return Craft rule set.
 */
const RuleCraft* RuleManufacture::getProducedCraft() const
{
	return _producedCraft;
}

/**
 * Gets the random manufacture rules.
 * @return The random manufacture rules.
 */
const std::vector<std::pair<int, std::map<const RuleItem*, int> > > &RuleManufacture::getRandomProducedItems() const
{
	return _randomProducedItems;
}

/**
* Gets the "manufactured person", i.e. person spawned when manufacturing project ends.
* @return The person type ID.
*/
const std::string &RuleManufacture::getSpawnedPersonType() const
{
	return _spawnedPersonType;
}

/**
* Gets the custom name of the "manufactured person".
* @return The person name translation code.
*/
const std::string &RuleManufacture::getSpawnedPersonName() const
{
	return _spawnedPersonName;
}


/**
 * Gets the list weight for this manufacture item.
 * @return The list weight for this manufacture item.
 */
int RuleManufacture::getListOrder() const
{
	return _listOrder;
}

}
