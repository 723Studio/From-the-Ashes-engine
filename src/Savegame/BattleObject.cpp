/*
 * Copyright 2010-2021 OpenXcom Developers.
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

#include "BattleObject.h"
#include "BattleItem.h"
#include "BattleUnit.h"
#include "Soldier.h"
#include "Tile.h"
#include "SavedGame.h"
#include "SavedBattleGame.h"
#include "../Battlescape/BattlescapeGame.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleObject.h"
#include "../Engine/RNG.h"
#include "../Engine/Game.h"
#include <climits>

namespace OpenXcom
{
/**
* Initializes BattleObject of the specified type.
* @param rules Pointer to ruleset.
* @param id The id of the object.
*/
BattleObject::BattleObject(const RuleObject* rules) : _rules(rules), _tile(0), _hackingDefence(0), _failedAttempts(0), _wasUsed(false)
{
	_hackingDefence = rules->getHackingDefence(); // needs as it can be increased later with failed hacking attempts
}

/**
*
*/
BattleObject::~BattleObject()
{
}

/**
* Loads the BattleObject from a YAML file.
* @param node YAML node.
* @param mod Mod for the item.
*/
void BattleObject::load(const YAML::YamlNodeReader& reader, Mod* mod)
{
	reader.tryRead("hackingDefence", _hackingDefence);
	reader.tryRead("failedAttempts", _failedAttempts);
	reader.tryRead("wasUsed", _wasUsed);
	reader.tryRead("position", _position);
}

/**
* Saves the BattleObject to a YAML file.
* @return YAML node.
*/
void BattleObject::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("type", _rules->getName());
	writer.write("hackingDefence", _hackingDefence);
	writer.write("failedAttempts", _failedAttempts);
	writer.write("wasUsed", _wasUsed);
	if (_tile)
		writer.write("position", _tile->getPosition());
}

void BattleObject::setTile(Tile *tile)
{
	_tile = tile;
}

/**
 * Process battleobject value updates on hacking
 * @param result - result of hacking, true if success
 */
void BattleObject::hackingPostProcess(bool result, Game* game)
{
	// Opening the minigame is an attempt: the target hardens itself even if the player cancels.
	const int currentDefence = getHackingDefence() > 0 ? getHackingDefence() : 1;
	const int defenceIncrease = static_cast<int>(ceil(currentDefence * RNG::generate(0.3, 0.7)));
	setHackingDefence(currentDefence > INT_MAX - defenceIncrease ? INT_MAX : currentDefence + defenceIncrease);

	if (result)
	{
		// Hacking is wireless: strategic rewards are transmitted immediately and intentionally
		// survive the hacker's death, mission failure, or a later tactical rollback.
		for (const std::string& eventId : _rules->getSpawnedEvents())
		{
			game->getSavedGame()->spawnEvent(game->getMod()->getEvent(eventId));
		}

		const std::string weightedEvent = _rules->getWeightedEvent(game->getSavedGame()->getMonthsPassed());
		if (!weightedEvent.empty())
		{
			game->getSavedGame()->spawnEvent(game->getMod()->getEvent(weightedEvent));
		}

		if (_rules->isMissionObjective())
		{
			game->getSavedGame()->getSavedBattle()->setHackingObjectiveGained(true);
		}
	}
	else
	{
		_failedAttempts++;
	}
}

/**
 * Resolves a sampling attempt.
 *
 * Failed rolls still spend TUs, but the object is consumed only after all
 * configured rewards have been delivered successfully.
 */
void BattleObject::resolveSamplingAttempt(BattleAction& action, Game* game)
{
	if (_wasUsed || !_rules)
	{
		action.result = "STR_NOTHING_TO_GATHER";
		return;
	}

	const BattleObjectType useType = _rules->getUseType();
	if ((useType != BATTLE_OBJECT_BIOLOGY_SAMPLING && useType != BATTLE_OBJECT_ANOMALY_SAMPLING) ||
		_rules->getSamplingDefence() <= 0 || !action.actor || !action.weapon ||
		!action.actor->getGeoscapeSoldier())
	{
		action.result = "STR_NOTHING_TO_GATHER";
		return;
	}
	if (!action.spendTU(&action.result))
	{
		return;
	}
	if (BattlescapeGame *battleGame = game->getSavedGame()->getSavedBattle()->getBattleGame())
	{
		battleGame->processUndercoverAction(action);
	}

	BattleUnit* unit = action.actor;
	Soldier* soldier = unit->getGeoscapeSoldier();
	auto stats = unit->getBaseStats();
	int power = action.weapon->getRules()->getSamplingPower();
	if (useType == BATTLE_OBJECT_BIOLOGY_SAMPLING)
	{
		power += stats->biology;
	}
	else if (useType == BATTLE_OBJECT_ANOMALY_SAMPLING)
	{
		power += stats->physics;
	}

	if (soldier->getRoleRank(ROLE_SCIENTIST) < 1)
	{
		power /= 2;
	}
	if (RNG::generate(0, power) < _rules->getSamplingDefence())
	{
		action.result = "STR_SAMPLES_GATHERING_FAILED";
		return;
	}

	for (const std::string& eventId : _rules->getSpawnedEvents())
	{
		game->getSavedGame()->spawnEvent(game->getMod()->getEvent(eventId, true));
	}
	if (!_rules->getSpawnedItem().empty())
	{
		const RuleItem* itemRule = game->getMod()->getItem(_rules->getSpawnedItem(), true);
		game->getSavedGame()->getSavedBattle()->createItemForTile(itemRule, unit->getTile());
	}

	if (useType == BATTLE_OBJECT_BIOLOGY_SAMPLING)
	{
		unit->addBiologyExp();
	}
	else if (useType == BATTLE_OBJECT_ANOMALY_SAMPLING)
	{
		unit->addPhysicsExp();
	}

	action.result = "STR_SAMPLES_GATHERED";
	setWasUsed(true);
}

/**
 * Create new battle object from rules for tile;
 */
BattleObject* SavedBattleGame::createObjectForTile(const RuleObject* rule, Tile* tile)
{
	BattleObject* object = new BattleObject(rule);
	if (tile)
	{
		tile->setBattleObject(object);
		object->setTile(tile);
		object->setPosition(tile->getPosition());
		return object;
	}
	else
	{
		delete object;
		return 0;
	}
}
}
