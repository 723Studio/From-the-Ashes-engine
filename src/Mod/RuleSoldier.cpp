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
#include <set>
#include "RuleSoldier.h"
#include "RuleSkill.h"
#include "Mod.h"
#include "ModScript.h"
#include "RuleItem.h"
#include "Armor.h"
#include "SoldierNamePool.h"
#include "StatString.h"
#include "../Engine/FileMap.h"
#include "../Engine/ScriptBind.h"
#include "../Engine/Unicode.h"

namespace OpenXcom
{

/**
 * Creates a blank RuleSoldier for a certain
 * type of soldier.
 * @param type String defining the type.
 */
RuleSoldier::RuleSoldier(const std::string &type, int listOrder) : _type(type), _group(0), _listOrder(listOrder), _armor(nullptr), _specWeapon(nullptr),
	_monthlyBuyLimit(0), _costBuy(0), _costSalary(0),
	_standHeight(0), _kneelHeight(0), _floatHeight(0), _femaleFrequency(50), _value(20), _transferTime(0), _moraleLossWhenKilled(100), _livingSpace(1),
	_totalSoldierNamePoolWeight(0),
	_avatarOffsetX(67), _avatarOffsetY(48), _flagOffset(0),
	_allowPiloting(true), _showTypeInInventory(false),
	_rankSprite(42), _rankSpriteBattlescape(20), _rankSpriteTiny(0),
	_pilotRankSprite(42), _pilotRankSpriteBattlescape(20), _pilotRankSpriteTiny(0),
	_agentRankSprite(42), _agentRankSpriteBattlescape(20), _agentRankSpriteTiny(0),
	_scientistRankSprite(42), _scientistRankSpriteBattlescape(20), _scientistRankSpriteTiny(0),
	_engineerRankSprite(42), _engineerRankSpriteBattlescape(20), _engineerRankSpriteTiny(0),
	_skillIconSprite(1)
{
}

/**
 *
 */
RuleSoldier::~RuleSoldier()
{
	for (auto* namepool : _names)
	{
		delete namepool;
	}
	for (auto* statString : _statStrings)
	{
		delete statString;
	}
	for (auto *requirements : _roleExpRequirements)
	{
		delete requirements;
	}
	for (auto *strings : _roleRankStrings)
	{
		delete strings;
	}
}

/**
 * Loads the soldier from a YAML file.
 * @param node YAML node.
 * @param mod Mod for the unit.
 */
void RuleSoldier::load(const YAML::YamlNodeReader& node, Mod *mod, const ModScript &parsers)
{
	const auto& reader = node.useIndex();
	if (const auto& parent = reader["refNode"])
	{
		load(parent, mod, parsers);
	}
	reader.tryRead("type", _type);
	if (reader["roles"])
	{
		std::vector<int> roles;
		reader.tryRead("roles", roles);
		loadRoles(roles);
	}
	// Just in case
	if (_type == "XCOM")
		_type = "STR_SOLDIER";

	//requires
	mod->loadUnorderedNames(_type, _requires, reader["requires"]);
	mod->loadBaseFunction(_type, _requiresBuyBaseFunc, reader["requiresBuyBaseFunc"]);

	// IMPORTANT: UnitStats contains many valid 0 values.
	// UnitStats::merge() treats 0 as "not specified", which breaks rule inheritance for fields that
	// are explicitly set to 0 (e.g. deception min 0 overriding a non-zero parent).
	// Read into a copy of the inherited stats so missing fields stay inherited, while explicit 0
	// overrides are applied correctly.
	if (reader["minStats"])
	{
		UnitStats tmp = _minStats;
		reader["minStats"].tryReadVal(tmp);
		_minStats = tmp;
	}
	if (reader["maxStats"])
	{
		UnitStats tmp = _maxStats;
		reader["maxStats"].tryReadVal(tmp);
		_maxStats = tmp;
	}
	if (reader["statCaps"])
	{
		UnitStats tmp = _statCaps;
		reader["statCaps"].tryReadVal(tmp);
		_statCaps = tmp;
	}
	if (reader["trainingStatCaps"])
	{
		UnitStats tmp = _trainingStatCaps;
		reader["trainingStatCaps"].tryReadVal(tmp);
		_trainingStatCaps = tmp;
	}
	else if (reader["statCaps"])
	{
		UnitStats tmp = _trainingStatCaps;
		reader["statCaps"].tryReadVal(tmp);
		_trainingStatCaps = tmp;
	}
	if (reader["dogfightExperience"])
	{
		UnitStats tmp = _dogfightExperience;
		reader["dogfightExperience"].tryReadVal(tmp);
		_dogfightExperience = tmp;
	}
	if (reader["roleExpRequirements"])
	{
		for (auto *requirements : _roleExpRequirements)
		{
			delete requirements;
		}
		_roleExpRequirements.clear();
		for (const auto& reqReader : reader["roleExpRequirements"].children())
		{
			SoldierRoleRanksRequirements *r = new SoldierRoleRanksRequirements;
			r->load(reqReader);
			_roleExpRequirements.push_back(r);
		}
	}
	if (reader["roleRankStrings"])
	{
		for (auto *strings : _roleRankStrings)
		{
			delete strings;
		}
		_roleRankStrings.clear();
		for (const auto& sReader : reader["roleRankStrings"].children())
		{
			SoldierRoleRanksStrings *s = new SoldierRoleRanksStrings;
			s->load(sReader);
			_roleRankStrings.push_back(s);
		}
	}
	mod->loadName(_type, _armorName, reader["armor"]);
	reader.tryRead("specialWeapon", _specWeaponName);
	reader.tryRead("armorForAvatar", _armorForAvatar);
	reader.tryRead("avatarOffsetX", _avatarOffsetX);
	reader.tryRead("avatarOffsetY", _avatarOffsetY);
	reader.tryRead("flagOffset", _flagOffset);
	reader.tryRead("allowPiloting", _allowPiloting);
	reader.tryRead("monthlyBuyLimit", _monthlyBuyLimit);
	reader.tryRead("costBuy", _costBuy);
	reader.tryRead("costSalary", _costSalary);
	reader.tryRead("standHeight", _standHeight);
	reader.tryRead("kneelHeight", _kneelHeight);
	reader.tryRead("floatHeight", _floatHeight);
	reader.tryRead("femaleFrequency", _femaleFrequency);
	reader.tryRead("value", _value);
	reader.tryRead("livingSpace", _livingSpace);
	if (_livingSpace < 0)
		throw Exception("Soldier rules " + _type + " has a negative value of livingSpace property, this is not allowed");
	reader.tryRead("transferTime", _transferTime);
	reader.tryRead("moraleLossWhenKilled", _moraleLossWhenKilled);
	reader.tryRead("showTypeInInventory", _showTypeInInventory);

	mod->loadSoundOffset(_type, _deathSoundMale, reader["deathMale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _deathSoundFemale, reader["deathFemale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _panicSoundMale, reader["panicMale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _panicSoundFemale, reader["panicFemale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _berserkSoundMale, reader["berserkMale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _berserkSoundFemale, reader["berserkFemale"], "BATTLE.CAT");

	mod->loadSoundOffset(_type, _selectUnitSoundMale, reader["selectUnitMale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _selectUnitSoundFemale, reader["selectUnitFemale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _startMovingSoundMale, reader["startMovingMale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _startMovingSoundFemale, reader["startMovingFemale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _selectWeaponSoundMale, reader["selectWeaponMale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _selectWeaponSoundFemale, reader["selectWeaponFemale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _annoyedSoundMale, reader["annoyedMale"], "BATTLE.CAT");
	mod->loadSoundOffset(_type, _annoyedSoundFemale, reader["annoyedFemale"], "BATTLE.CAT");

	for (const auto& fileNameOrCommand : reader["soldierNames"].children())
	{
		std::string fileName = fileNameOrCommand.readVal<std::string>();
		if (fileName == "delete")
		{
			for (auto* namepool : _names)
			{
				delete namepool;
			}
			_names.clear();
		}
		else
		{
			if (fileName[fileName.length() - 1] == '/')
			{
				// load all *.nam files in given directory
				std::vector<std::string> names;
				for (const auto& f: FileMap::filterFiles(FileMap::getVFolderContents(fileName), "nam")) { names.push_back(f); }
				std::sort(names.begin(), names.end(), Unicode::naturalCompare);
				for (const auto& name : names)
				{
					addSoldierNamePool(fileName + name);
				}
			}
			else
			{
				// load given file
				addSoldierNamePool(fileName);
			}
		}
	}

	for (const auto& statStringReader : reader["statStrings"].children())
	{
		StatString *statString = new StatString();
		statString->load(statStringReader);
		_statStrings.push_back(statString);
	}

	mod->loadSpriteOffset(_type, _rankSprite, reader["rankSprite"], "BASEBITS.PCK");
	mod->loadSpriteOffset(_type, _rankSpriteBattlescape, reader["rankBattleSprite"], "SMOKE.PCK");
	mod->loadSpriteOffset(_type, _rankSpriteTiny, reader["rankTinySprite"], "TinyRanks");

	mod->loadSpriteOffset(_type, _pilotRankSprite, reader["pilotRankSprite"], "BASEBITS.PCK");
	mod->loadSpriteOffset(_type, _pilotRankSpriteBattlescape, reader["pilotRankSpriteBattlescape"], "SMOKE.PCK");
	mod->loadSpriteOffset(_type, _pilotRankSpriteTiny, reader["pilotRankSpriteTiny"], "TinyRanks");

	mod->loadSpriteOffset(_type, _agentRankSprite, reader["agentRankSprite"], "BASEBITS.PCK");
	mod->loadSpriteOffset(_type, _agentRankSpriteBattlescape, reader["agentRankSpriteBattlescape"], "SMOKE.PCK");
	mod->loadSpriteOffset(_type, _agentRankSpriteTiny, reader["agentRankSpriteTiny"], "TinyRanks");
	mod->loadSpriteOffset(_type, _scientistRankSprite, reader["scientistRankSprite"], "BASEBITS.PCK");
	mod->loadSpriteOffset(_type, _scientistRankSpriteBattlescape, reader["scientistRankSpriteBattlescape"], "SMOKE.PCK");
	mod->loadSpriteOffset(_type, _scientistRankSpriteTiny, reader["scientistRankSpriteTiny"], "TinyRanks");

	mod->loadSpriteOffset(_type, _engineerRankSprite, reader["engineerRankSprite"], "BASEBITS.PCK");
	mod->loadSpriteOffset(_type, _engineerRankSpriteBattlescape, reader["engineerRankSpriteBattlescape"], "SMOKE.PCK");
	mod->loadSpriteOffset(_type, _engineerRankSpriteTiny, reader["engineerRankSpriteTiny"], "TinyRanks");

	mod->loadSpriteOffset(_type, _skillIconSprite, reader["skillIconSprite"], "SPICONS.DAT");
	mod->loadNames(_type, _skillNames, reader["skills"]);

	if (reader["spawnedSoldier"])
	{
		_spawnedSoldier = reader["spawnedSoldier"].emitDescendants(YAML::YamlRootNodeReader(_spawnedSoldier, "(spawned soldier template)"));
	}
	reader.tryRead("group", _group);
	reader.tryRead("listOrder", _listOrder);

	_scriptValues.load(reader, parsers.getShared());
}

/**
 * Cross link with other Rules.
 */
void RuleSoldier::afterLoad(const Mod* mod)
{
	std::set<SoldierRole> requirementRoles;
	for (const auto *roleRequirements : _roleExpRequirements)
	{
		if (!roleRequirements || roleRequirements->role <= ROLE_NONE || roleRequirements->role > ROLE_ENGINEER || roleRequirements->role == ROLE_ROBOT)
		{
			throw Exception("Soldier type '" + _type + "' has experience requirements for an invalid or non-trainable role.");
		}
		if (!requirementRoles.insert(roleRequirements->role).second)
		{
			throw Exception("Soldier type '" + _type + "' defines duplicate role experience requirements.");
		}
		if (roleRequirements->requirements.empty() || roleRequirements->requirements.count(1) == 0)
		{
			throw Exception("Soldier type '" + _type + "' must define the rank-1 qualification cost for each trainable role.");
		}
		int expectedRank = 1;
		for (const auto &requirement : roleRequirements->requirements)
		{
			if (requirement.first != expectedRank++ || requirement.second < 0)
			{
				throw Exception("Soldier type '" + _type + "' has invalid or non-contiguous role experience requirements.");
			}
		}
	}

	std::set<SoldierRole> stringRoles;
	for (const auto *roleStrings : _roleRankStrings)
	{
		if (!roleStrings || roleStrings->role <= ROLE_NONE || roleStrings->role > ROLE_ENGINEER || !stringRoles.insert(roleStrings->role).second)
		{
			throw Exception("Soldier type '" + _type + "' has invalid or duplicate role rank strings.");
		}
		int expectedRank = 1;
		for (const auto &rankString : roleStrings->strings)
		{
			if (rankString.first != expectedRank++ || rankString.second.empty())
			{
				throw Exception("Soldier type '" + _type + "' has invalid or non-contiguous role rank strings.");
			}
		}
		if (roleStrings->role == ROLE_ROBOT && (roleStrings->strings.size() != 1 || roleStrings->strings.count(1) == 0))
		{
			throw Exception("Soldier type '" + _type + "' may define only rank 1 for non-sapient robots.");
		}
	}

	for (SoldierRole role : requirementRoles)
	{
		if (stringRoles.count(role) == 0)
		{
			throw Exception("Soldier type '" + _type + "' is missing rank strings for a trainable role.");
		}
		const auto *requirements = *std::find_if(_roleExpRequirements.begin(), _roleExpRequirements.end(),
			[role](const SoldierRoleRanksRequirements *entry) { return entry->role == role; });
		const auto *strings = *std::find_if(_roleRankStrings.begin(), _roleRankStrings.end(),
			[role](const SoldierRoleRanksStrings *entry) { return entry->role == role; });
		if (strings->strings.size() != requirements->requirements.size() + 1)
		{
			throw Exception("Soldier type '" + _type + "' must define one more rank string than experience thresholds for each trainable role.");
		}
	}
	for (SoldierRole role : stringRoles)
	{
		if (role != ROLE_ROBOT && requirementRoles.count(role) == 0)
		{
			throw Exception("Soldier type '" + _type + "' defines rank strings without experience requirements for a trainable role.");
		}
	}
	for (SoldierRole role : _roles)
	{
		if (role == ROLE_NONE)
		{
			continue;
		}
		if (stringRoles.count(role) == 0)
		{
			throw Exception("Soldier type '" + _type + "' has an initial role without rank strings.");
		}
		if (role != ROLE_ROBOT && requirementRoles.count(role) == 0)
		{
			throw Exception("Soldier type '" + _type + "' has a trainable initial role without experience requirements.");
		}
	}

	_totalSoldierNamePoolWeight = 0;
	for (auto* namepool : _names)
	{
		_totalSoldierNamePoolWeight += namepool->getGlobalWeight();
	}
	if (_totalSoldierNamePoolWeight < 1)
	{
		Log(LOG_ERROR) << _type << ": total soldier name pool weight is invalid. Forgotten 'soldierNames:' ?";
	}

	mod->linkRule(_armor, _armorName);
	mod->linkRule(_prisoner, _prisonerName);
	mod->checkForSoftError(_armor == nullptr, _type, "Soldier type is missing the default armor", LOG_ERROR);

	mod->verifySoundOffset(_type, _deathSoundMale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _deathSoundFemale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _panicSoundMale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _panicSoundFemale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _berserkSoundMale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _berserkSoundFemale, "BATTLE.CAT");

	mod->verifySoundOffset(_type, _selectUnitSoundMale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _selectUnitSoundFemale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _startMovingSoundMale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _startMovingSoundFemale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _selectWeaponSoundMale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _selectWeaponSoundFemale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _annoyedSoundMale, "BATTLE.CAT");
	mod->verifySoundOffset(_type, _annoyedSoundFemale, "BATTLE.CAT");

	mod->verifySpriteOffset(_type, _rankSprite, "BASEBITS.PCK");
	mod->verifySpriteOffset(_type, _rankSpriteBattlescape, "SMOKE.PCK");
	mod->verifySpriteOffset(_type, _rankSpriteTiny, "TinyRanks");
	mod->verifySpriteOffset(_type, _pilotRankSprite, "BASEBITS.PCK");
	mod->verifySpriteOffset(_type, _pilotRankSpriteBattlescape, "SMOKE.PCK");
	mod->verifySpriteOffset(_type, _pilotRankSpriteTiny, "TinyRanks");
	mod->verifySpriteOffset(_type, _agentRankSprite, "BASEBITS.PCK");
	mod->verifySpriteOffset(_type, _agentRankSpriteBattlescape, "SMOKE.PCK");
	mod->verifySpriteOffset(_type, _agentRankSpriteTiny, "TinyRanks");
	mod->verifySpriteOffset(_type, _scientistRankSprite, "BASEBITS.PCK");
	mod->verifySpriteOffset(_type, _scientistRankSpriteBattlescape, "SMOKE.PCK");
	mod->verifySpriteOffset(_type, _scientistRankSpriteTiny, "TinyRanks");
	mod->verifySpriteOffset(_type, _engineerRankSprite, "BASEBITS.PCK");
	mod->verifySpriteOffset(_type, _engineerRankSpriteBattlescape, "SMOKE.PCK");
	mod->verifySpriteOffset(_type, _engineerRankSpriteTiny, "TinyRanks");
	mod->verifySpriteOffset(_type, _skillIconSprite, "SPICONS.DAT");

	if (!_specWeaponName.empty())
	{
		mod->linkRule(_specWeapon, _specWeaponName);

		if ((_specWeapon->getBattleType() == BT_FIREARM || _specWeapon->getBattleType() == BT_MELEE) && !_specWeapon->getClipSize())
		{
			throw Exception("Weapon " + _specWeaponName + " is used as a special weapon, but doesn't have its own ammo - give it a clipSize!");
		}
	}
	mod->linkRule(_skills, _skillNames);

	_manaMissingWoundThreshold = mod->getManaWoundThreshold();
	_healthMissingWoundThreshold = mod->getHealthWoundThreshold();
}

void RuleSoldier::loadRoles(const std::vector<int> &r)
{
	_roles.clear();
	for (auto i : r)
	{
		if (i < ROLE_NONE || i > ROLE_ENGINEER)
		{
			throw Exception("Soldier type '" + _type + "' contains an invalid role value.");
		}
		SoldierRole role = static_cast<SoldierRole>(i);
		if (std::find(_roles.begin(), _roles.end(), role) != _roles.end())
		{
			throw Exception("Soldier type '" + _type + "' contains a duplicate role.");
		}
		_roles.push_back(role);
	}
	if (_roles.size() > 1 && std::find(_roles.begin(), _roles.end(), ROLE_NONE) != _roles.end())
	{
		throw Exception("Soldier type '" + _type + "' cannot combine ROLE_NONE with a real role.");
	}
	if (_roles.size() > 1 && std::find(_roles.begin(), _roles.end(), ROLE_ROBOT) != _roles.end())
	{
		throw Exception("Soldier type '" + _type + "' cannot combine a non-sapient ROLE_ROBOT with another role.");
	}
}

void RuleSoldier::addSoldierNamePool(const std::string &namFile)
{
	SoldierNamePool *pool = new SoldierNamePool();
	pool->load(namFile);
	_names.push_back(pool);
}

/**
 * Returns the language string that names
 * this soldier. Each soldier type has a unique name.
 * @return Soldier name.
 */
const std::string& RuleSoldier::getType() const
{
	return _type;
}

/**
 * Gets the list/sort order of the soldier's type.
 * @return The list/sort order.
 */
int RuleSoldier::getListOrder() const
{
	return _listOrder;
}

/**
 * Gets the list of research required to
 * acquire this soldier.
 * @return The list of research IDs.
*/
const std::vector<std::string> &RuleSoldier::getRequirements() const
{
	return _requires;
}

/**
 * Gets the minimum stats for the random stats generator.
 * @return The minimum stats.
 */
UnitStats RuleSoldier::getMinStats() const
{
	return _minStats;
}

/**
 * Gets the maximum stats for the random stats generator.
 * @return The maximum stats.
 */
UnitStats RuleSoldier::getMaxStats() const
{
	return _maxStats;
}

/**
 * Gets the stat caps.
 * @return The stat caps.
 */
UnitStats RuleSoldier::getStatCaps() const
{
	return _statCaps;
}

/**
* Gets the training stat caps.
* @return The training stat caps.
*/
UnitStats RuleSoldier::getTrainingStatCaps() const
{
	return _trainingStatCaps;
}

/**
* Gets the improvement chances for pilots (after dogfight).
* @return The improvement changes.
*/
UnitStats RuleSoldier::getDogfightExperience() const
{
	return _dogfightExperience;
}

/**
 * Gets the cost of hiring this soldier.
 * @return The cost.
 */
int RuleSoldier::getBuyCost() const
{
	return _costBuy;
}

/**
 * Gets the list of defined skills.
 * @return The list of defined skills.
 */
const std::vector<const RuleSkill*> &RuleSoldier::getSkills() const
{
	return _skills;
}

/**
 * Gets the sprite index into SPICONS for the skill icon sprite.
 * @return The sprite index.
 */
int RuleSoldier::getSkillIconSprite() const
{
	return _skillIconSprite;
}

/**
 * Gets the height of the soldier when it's standing.
 * @return The standing height.
 */
int RuleSoldier::getStandHeight() const
{
	return _standHeight;
}

/**
 * Gets the height of the soldier when it's kneeling.
 * @return The kneeling height.
 */
int RuleSoldier::getKneelHeight() const
{
	return _kneelHeight;
}

/**
 * Gets the elevation of the soldier when it's flying.
 * @return The floating height.
 */
int RuleSoldier::getFloatHeight() const
{
	return _floatHeight;
}

/**
 * Gets the default armor name.
 * @return The armor name.
 */
Armor* RuleSoldier::getDefaultArmor() const
{
	return const_cast<Armor*>(_armor); //TODO: fix this function usage to remove const cast
}

/**
* Gets the armor for avatar.
* @return The armor name.
*/
const std::string& RuleSoldier::getArmorForAvatar() const
{
	return _armorForAvatar;
}

/**
* Gets the avatar's X offset.
* @return The X offset.
*/
int RuleSoldier::getAvatarOffsetX() const
{
	return _avatarOffsetX;
}

/**
* Gets the avatar's Y offset.
* @return The Y offset.
*/
int RuleSoldier::getAvatarOffsetY() const
{
	return _avatarOffsetY;
}

/**
* Gets the flag offset.
* @return The flag offset.
*/
int RuleSoldier::getFlagOffset() const
{
	return _flagOffset;
}

/**
* Gets the allow piloting flag.
* @return True if piloting is allowed.
*/
bool RuleSoldier::getAllowPiloting() const
{
	return _allowPiloting;
}

/**
 * Gets the female appearance ratio.
 * @return The percentage ratio.
 */
int RuleSoldier::getFemaleFrequency() const
{
	return _femaleFrequency;
}

/**
 * Gets the death sounds for male soldiers.
 * @return List of sound IDs.
 */
const std::vector<int> &RuleSoldier::getMaleDeathSounds() const
{
	return _deathSoundMale;
}

/**
 * Gets the death sounds for female soldiers.
 * @return List of sound IDs.
 */
const std::vector<int> &RuleSoldier::getFemaleDeathSounds() const
{
	return _deathSoundFemale;
}

/**
 * Gets the panic sounds for male soldiers.
 * @return List of sound IDs.
 */
const std::vector<int> &RuleSoldier::getMalePanicSounds() const
{
	return _panicSoundMale;
}

/**
 * Gets the panic sounds for female soldiers.
 * @return List of sound IDs.
 */
const std::vector<int> &RuleSoldier::getFemalePanicSounds() const
{
	return _panicSoundFemale;
}

/**
 * Gets the berserk sounds for male soldiers.
 * @return List of sound IDs.
 */
const std::vector<int> &RuleSoldier::getMaleBerserkSounds() const
{
	return _berserkSoundMale;
}

/**
 * Gets the berserk sounds for female soldiers.
 * @return List of sound IDs.
 */
const std::vector<int> &RuleSoldier::getFemaleBerserkSounds() const
{
	return _berserkSoundFemale;
}

/**
 * Returns the list of soldier name pools.
 * @return Pointer to soldier name pool list.
 */
const std::vector<SoldierNamePool*> &RuleSoldier::getNames() const
{
	return _names;
}

/*
 * Gets the soldier's base value, without experience modifiers.
 * @return The soldier's value.
 */
int RuleSoldier::getValue() const
{
	return _value;
}

/*
 * Gets the amount of time this item
 * takes to arrive at a base.
 * @return The time in hours.
 */
int RuleSoldier::getTransferTime() const
{
	return _transferTime;
}

/**
* Gets the list of StatStrings.
* @return The list of StatStrings.
*/
const std::vector<StatString *> &RuleSoldier::getStatStrings() const
{
	return _statStrings;
}

/**
 * Gets the index of the sprites to use to represent this soldier's rank in BASEBITS.PCK
 * @return The sprite index.
 */
int RuleSoldier::getRankSprite() const
{
	return _rankSprite;
}

/**
 * Gets the index of the sprites to use to represent this soldier's rank in SMOKE.PCK
 * @return The sprite index.
 */
int RuleSoldier::getRankSpriteBattlescape() const
{
	return _rankSpriteBattlescape;
}

/**
 * Gets the index of the sprites to use to represent this soldier's rank in TinyRanks
 * @return The sprite index.
 */
int RuleSoldier::getRankSpriteTiny() const
{
	return _rankSpriteTiny;
}

int RuleSoldier::getRequiredExperience(SoldierRole role, int rank) const
{
    for (const auto& req : _roleExpRequirements)
    {
        if (req->role == role)
        {
            auto it = req->requirements.find(rank);
            if (it != req->requirements.end())
            {
                return it->second;
            }
        }
    }
    return -1; // or some default value indicating no requirement found
}


////////////////////////////////////////////////////////////
//					Script binding
////////////////////////////////////////////////////////////


namespace
{

void getTypeScript(const RuleSoldier* r, ScriptText& txt)
{
	if (r)
	{
		txt = { r->getType().c_str() };
		return;
	}
	else
	{
		txt = ScriptText::empty;
	}
}

std::string debugDisplayScript(const RuleSoldier* rs)
{
	if (rs)
	{
		std::string s;
		s += RuleSoldier::ScriptName;
		s += "(name: \"";
		s += rs->getType();
		s += "\")";
		return s;
	}
	else
	{
		return "null";
	}
}

}

/**
 * Converts SoldierRole enum to string.
 * @param role The SoldierRole enum value.
 * @return The string representation.
 */
std::string RuleSoldier::getRoleString(SoldierRole role)
{
	switch (role)
	{
	case ROLE_NONE:
		return "NONE";
	case ROLE_SOLDIER:
		return "SOLDIER";
	case ROLE_ROBOT:
		return "ROBOT";
	case ROLE_PILOT:
		return "PILOT";
	case ROLE_AGENT:
		return "AGENT";
	case ROLE_SCIENTIST:
		return "SCIENTIST";
	case ROLE_ENGINEER:
		return "ENGINEER";
	default:
		return "UNKNOWN";
	}
}

/**
 * Register Armor in script parser.
 * @param parser Script parser.
 */
void RuleSoldier::ScriptRegister(ScriptParserBase* parser)
{
	Bind<RuleSoldier> ra = { parser };

	UnitStats::addGetStatsScript<&RuleSoldier::_statCaps>(ra, "StatsCap.");
	UnitStats::addGetStatsScript<&RuleSoldier::_minStats>(ra, "StatsMin.");
	UnitStats::addGetStatsScript<&RuleSoldier::_maxStats>(ra, "StatsMax.");

	ra.add<&getTypeScript>("getType");

	ra.addScriptValue<BindBase::OnlyGet, &RuleSoldier::_scriptValues>();
	ra.addDebugDisplay<&debugDisplayScript>();
}

}
