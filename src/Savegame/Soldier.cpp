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
#include <cmath>
#include <algorithm>
#include <climits>
#include "Soldier.h"
#include "../Engine/Exception.h"
#include "../Engine/RNG.h"
#include "../Engine/Language.h"
#include "../Engine/Options.h"
#include "../Engine/ScriptBind.h"
#include "Craft.h"
#include "../Savegame/CovertOperation.h"
#include "../Savegame/ResearchProject.h"
#include "../Savegame/Production.h"
#include "../Savegame/IntelProject.h"
#include "../Savegame/BattleUnit.h"
#include "EquipmentLayoutItem.h"
#include "SoldierDeath.h"
#include "SoldierDiary.h"
#include "../Mod/SoldierNamePool.h"
#include "../Mod/RuleSoldier.h"
#include "../Mod/RuleSoldierBonus.h"
#include "../Mod/Armor.h"
#include "../Mod/Mod.h"
#include "../Mod/StatString.h"
#include "../Engine/Unicode.h"
#include "../Mod/RuleSoldierTransformation.h"
#include "../Mod/RuleCommendations.h"
#include "Base.h"
#include "BasePrisoner.h"
#include "BaseContainedAlien.h"
#include "BaseContainedMonster.h"
#include "ItemContainer.h"
#include "../Mod/RuleSkill.h"

namespace OpenXcom
{
namespace
{

/**
 * Generates a standard normally distributed value using the Box-Muller transform.
 */
double generateStandardNormal()
{
	constexpr double TwoPi = 6.28318530717958647692;
	const double u1 = std::max(1.0 - RNG::generate(0.0, 1.0), 1.0e-12);
	const double u2 = RNG::generate(0.0, 1.0);
	return std::sqrt(-2.0 * std::log(u1)) * std::cos(TwoPi * u2);
}

}

/**
 * Generates a normally distributed stat centered in the configured range.
 * @param min Minimum value.
 * @param max Maximum value.
 * @return Generated stat.
 */
int Soldier::generateNormalStat(int min, int max)
{
	if (max < min)
	{
		std::swap(min, max);
	}
	if (min == max)
	{
		return min;
	}

	const double normalized = std::max(0.0, std::min(1.0, 0.5 + generateStandardNormal() / 6.0));
	return static_cast<int>(std::lround(min + (max - min) * normalized));
}

/**
 * Generates a positively skewed science stat with a long upper tail.
 * @param min Minimum value.
 * @param max Maximum value.
 * @return Generated stat.
 */
int Soldier::generateScienceStat(int min, int max)
{
	if (max < min)
	{
		std::swap(min, max);
	}
	if (max <= 0)
	{
		return 0;
	}

	min = std::max(min, 1);
	if (min >= max)
	{
		return max;
	}

	constexpr double LogNormalSigma = 0.9;
	constexpr double LogNormalScale = 6.35;
	const double normalized = std::min(1.0, std::exp(LogNormalSigma * generateStandardNormal()) / LogNormalScale);
	return static_cast<int>(std::lround(min + (max - min) * normalized));
}

/**
 * Gets possible stat improvement
 * @param exp - stats experience points
 * @param rate - pointer for role rank experience calculations
 * @param bravery - if this is a special calculation for bravery increase
 * @return - value of stat improvement
 */
int Soldier::improveStat(int exp, int &rate, bool bravery)
{
	rate = 0;
	if (bravery && exp > RNG::generate(0, 10))
	{
		rate = 10;
		return 10;
	}

	if (exp > 10)
	{
		rate = 3;
		return RNG::generate(2, 6);
	}
	else if (exp > 5)
	{
		rate = 2;
		return RNG::generate(1, 4);
	}
	else if (exp > 2)
	{
		rate = 1;
		return RNG::generate(1, 3);
	}
	else if (exp > 0)
	{
		int improve = RNG::generate(0, 1);
		if (improve > 0)
			rate = 1;
		return improve;
	}
	else
		return 0;
}

/**
 * Selects a technical identity/name pool using its global recruitment weight.
 */
int Soldier::selectNamePoolIndex() const
{
	const auto &names = _rules->getNames();
	if (names.empty())
	{
		return 0;
	}

	int totalWeight = 0;
	for (const auto *namePool : names)
	{
		totalWeight += std::max(0, namePool->getGlobalWeight());
	}
	if (totalWeight <= 0)
	{
		return RNG::generate(0, static_cast<int>(names.size()) - 1);
	}

	int roll = RNG::generate(1, totalWeight);
	for (size_t i = 0; i < names.size(); ++i)
	{
		roll -= std::max(0, names[i]->getGlobalWeight());
		if (roll <= 0)
		{
			return static_cast<int>(i);
		}
	}
	return static_cast<int>(names.size()) - 1;
}

/**
 * Initializes a new soldier, either blank or randomly generated.
 * @param rules Soldier ruleset.
 * @param armor Soldier armor.
 * @param id Unique soldier id for soldier generation.
 * @param nationality Optional technical name-pool index; negative selects one randomly.
 */
Soldier::Soldier(const RuleSoldier *rules, Armor *armor, int id, int nationality) :
	_id(id), _nationality(0),
	_improvement(0), _rules(rules), _craft(0), _covertOperation(0), _researchProject(0), _production(0), _intelProject(0), _prisoner(0), _containedAlien(0), _containedMonster(0),
	_gender(GENDER_MALE), _look(LOOK_BLONDE), _lookVariant(0), _missions(0), _kills(0), _stuns(0),
	_psiTraining(false), _training(false), _returnToTrainingWhenHealed(false), _justSaved(false), _returnToTrainingsWhenOperationOver(NONE),
	_armor(armor), _replacedArmor(0), _transformedArmor(0), _death(0), _diary(new SoldierDiary()),
	_corpseRecovered(false), _isRookieSoldier(true), _isRookieAgent(true), _isRookiePilot(true)
{
	if (id != 0)
	{
		UnitStats minStats = rules->getMinStats();
		UnitStats maxStats = rules->getMaxStats();

		const std::vector<SoldierRole> roles = rules->getRoles();
		const bool isScientist = std::find(roles.begin(), roles.end(), ROLE_SCIENTIST) != roles.end();

		// Soldier
		_initialStats.tu = generateNormalStat(minStats.tu, maxStats.tu);
		_initialStats.stamina = generateNormalStat(minStats.stamina, maxStats.stamina);
		_initialStats.health = generateNormalStat(minStats.health, maxStats.health);
		_initialStats.mana = generateNormalStat(minStats.mana, maxStats.mana);
		_initialStats.bravery = generateNormalStat(minStats.bravery / 10, maxStats.bravery / 10) * 10;
		_initialStats.reactions = generateNormalStat(minStats.reactions, maxStats.reactions);
		_initialStats.firing = generateNormalStat(minStats.firing, maxStats.firing);
		_initialStats.throwing = generateNormalStat(minStats.throwing, maxStats.throwing);
		_initialStats.strength = generateNormalStat(minStats.strength, maxStats.strength);
		_initialStats.psiStrength = generateNormalStat(minStats.psiStrength, maxStats.psiStrength);
		_initialStats.melee = generateNormalStat(minStats.melee, maxStats.melee);
		_initialStats.psiSkill = generateNormalStat(minStats.psiSkill, maxStats.psiSkill);

		// Pilot
		_initialStats.maneuvering = generateNormalStat(minStats.maneuvering, maxStats.maneuvering);
		_initialStats.missiles = generateNormalStat(minStats.missiles, maxStats.missiles);
		_initialStats.dogfight = generateNormalStat(minStats.dogfight, maxStats.dogfight);
		_initialStats.tracking = generateNormalStat(minStats.tracking, maxStats.tracking);
		_initialStats.cooperation = generateNormalStat(minStats.cooperation, maxStats.cooperation);
		_initialStats.beams = generateNormalStat(minStats.beams, maxStats.beams);
		_initialStats.synaptic = generateNormalStat(minStats.synaptic, maxStats.synaptic);
		_initialStats.gravity = generateNormalStat(minStats.gravity, maxStats.gravity);

		// Agent
		_initialStats.stealth = generateNormalStat(minStats.stealth, maxStats.stealth);
		_initialStats.perception = generateNormalStat(minStats.perception, maxStats.perception);
		_initialStats.charisma = generateNormalStat(minStats.charisma, maxStats.charisma);
		_initialStats.investigation = generateNormalStat(minStats.investigation, maxStats.investigation);
		_initialStats.deception = generateNormalStat(minStats.deception, maxStats.deception);
		_initialStats.interrogation = generateNormalStat(minStats.interrogation, maxStats.interrogation);

		// Scientist
		_initialStats.insight = generateNormalStat(minStats.insight, maxStats.insight);
		const UnitStats::Ptr scienceStats[] =
		{
			&UnitStats::physics,
			&UnitStats::chemistry,
			&UnitStats::biology,
			&UnitStats::data,
			&UnitStats::computers,
			&UnitStats::tactics,
			&UnitStats::materials,
			&UnitStats::designing,
			&UnitStats::alienTech,
			&UnitStats::psionics,
			&UnitStats::xenolinguistics
		};
		bool hasAvailableScienceStat = false;
		bool hasScienceSpecialization = false;
		for (const UnitStats::Ptr stat : scienceStats)
		{
			const int statMin = minStats.*stat;
			const int statMax = maxStats.*stat;
			_initialStats.*stat = generateScienceStat(statMin, statMax);

			if (statMax > 0)
			{
				hasAvailableScienceStat = true;
				const int midpoint = statMin + (statMax - statMin + 1) / 2;
				if (_initialStats.*stat >= midpoint)
				{
					hasScienceSpecialization = true;
				}
			}
		}
		if (isScientist && hasAvailableScienceStat && !hasScienceSpecialization)
		{
			const int insightBonus = (maxStats.insight - minStats.insight) / 2;
			_initialStats.insight = std::min<int>(maxStats.insight, _initialStats.insight + insightBonus);
		}

		// Engineer
		_initialStats.weaponry = generateNormalStat(minStats.weaponry, maxStats.weaponry);
		_initialStats.explosives = generateNormalStat(minStats.explosives, maxStats.explosives);
		_initialStats.efficiency = generateNormalStat(minStats.efficiency, maxStats.efficiency);
		_initialStats.microelectronics = generateNormalStat(minStats.microelectronics, maxStats.microelectronics);
		_initialStats.metallurgy = generateNormalStat(minStats.metallurgy, maxStats.metallurgy);
		_initialStats.processing = generateNormalStat(minStats.processing, maxStats.processing);
		_initialStats.hacking = generateNormalStat(minStats.hacking, maxStats.hacking);
		_initialStats.robotics = generateNormalStat(minStats.robotics, maxStats.robotics);
		_initialStats.diligence = generateNormalStat(minStats.diligence, maxStats.diligence);
		_initialStats.reverseEngineering = generateNormalStat(minStats.reverseEngineering, maxStats.reverseEngineering);

		_currentStats = _initialStats;

		const std::vector<SoldierNamePool*> &names = rules->getNames();
		if (!names.empty())
		{
			_nationality = nationality >= 0 && static_cast<size_t>(nationality) < names.size()
				? nationality
				: selectNamePoolIndex();
			_name = names.at(_nationality)->genName(&_gender, rules->getFemaleFrequency());
			_callsign = generateCallsign(rules->getNames());
			_look = (SoldierLook)names.at(_nationality)->genLook(4); // Once we add the ability to mod in extra looks, this will need to reference the ruleset for the maximum amount of looks.
		}
		else
		{
			// No possible names, just wing it
			_gender = (RNG::percent(rules->getFemaleFrequency()) ? GENDER_FEMALE : GENDER_MALE);
			_look = (SoldierLook)RNG::generate(0,3);
			_name = (_gender == GENDER_FEMALE) ? "Jane" : "John";
			_name += " Doe";
			_callsign = "";
		}
		auto role = rules->getRoles();
		if (!role.empty())
		{
			for (auto r : role)
			{
				initializeRole(r);
			}
		}
		else
		{
			initializeRole(ROLE_NONE);
		}
		normalizeRoles();
	}

	_lookVariant = RNG::seedless(0, RuleSoldier::LookVariantMax - 1);
}

/**
 *
 */
Soldier::~Soldier()
{
	for (auto* entry : _equipmentLayout)
	{
		delete entry;
	}
	for (std::vector<SoldierRoleRanks *>::iterator i = _roles.begin(); i != _roles.end(); ++i)
	{
		delete *i;
	}
	delete _death;
	delete _diary;
}

/**
 * Loads the soldier from a YAML file.
 * @param node YAML node.
 * @param mod Game mod.
 * @param save Pointer to savegame.
 */
void Soldier::load(const YAML::YamlNodeReader& reader, const Mod *mod, SavedGame *save, const ScriptGlobal *shared, bool soldierTemplate)
{
	if (!soldierTemplate)
		reader.tryRead("id", _id);
	reader.tryRead("name", _name);
	reader.tryRead("callsign", _callsign);
	reader.tryRead("nationality", _nationality);
	const std::vector<SoldierNamePool*> &namePools = _rules->getNames();
	if (namePools.empty())
	{
		_nationality = 0;
	}
	else if (_nationality < 0 || static_cast<size_t>(_nationality) >= namePools.size())
	{
		// `nationality` is temporarily only a technical identity-pool index.
		_nationality = selectNamePoolIndex();
	}
	if (soldierTemplate)
	{
		UnitStats ii, cc;
		reader.tryRead("initialStats", ii);
		reader.tryRead("currentStats", cc);
		_initialStats = UnitStats::templateMerge(_initialStats, ii);
		_currentStats = UnitStats::templateMerge(_currentStats, cc);
	}
	else
	{
		reader.tryRead("initialStats", _initialStats);
		reader.tryRead("currentStats", _currentStats);
	}

	reader.tryRead("monthlyExperienceCache", _monthlyExperienceCache);
	reader.tryRead("dogfightExperience", _dogfightExperience);
	reader.tryRead("researchExperience", _researchExperience);
	reader.tryRead("engineerExperience", _engineerExperience);
	reader.tryRead("intelExperience", _intelExperience);
	reader.tryRead("isRookieSoldier", _isRookieSoldier);
	reader.tryRead("isRookieAgent", _isRookieAgent);
	reader.tryRead("isRookiePilot", _isRookiePilot);

	// re-roll mana stats when upgrading saves
	if (_currentStats.mana == 0 && _rules->getMaxStats().mana > 0)
	{
		int reroll = RNG::generate(_rules->getMinStats().mana, _rules->getMaxStats().mana);
		_currentStats.mana = reroll;
		_initialStats.mana = reroll;
	}

	if (reader["roles"])
	{
		clearRoles();
		for (const auto& roleNode : reader["roles"].children())
		{
			SoldierRoleRanks *r = new SoldierRoleRanks;
			r->load(roleNode);
			_roles.push_back(r);
		}
		normalizeRoles(true);
	}
	else if (!soldierTemplate)
	{
		throw Exception("Soldier '" + _name + "' has no roles; this save format is not supported.");
	}
	reader.tryRead("gender", _gender);
	reader.tryRead("look", _look);
	reader.tryRead("lookVariant", _lookVariant);
	reader.tryRead("missions", _missions);
	reader.tryRead("kills", _kills);
	reader.tryRead("stuns", _stuns);
	reader.tryRead("manaMissing", _manaMissing);
	reader.tryRead("healthMissing", _healthMissing);
	reader.tryRead("recovery", _recovery);
	Armor *armor = _armor;
	if (reader["armor"])
		armor = mod->getArmor(reader["armor"].readVal<std::string>());
	if (armor == 0)
		armor = mod->getSoldier(mod->getSoldiersList().front())->getDefaultArmor();
	_armor = armor;
	if (reader["replacedArmor"])
		_replacedArmor = mod->getArmor(reader["replacedArmor"].readVal<std::string>());
	if (reader["transformedArmor"])
		_transformedArmor = mod->getArmor(reader["transformedArmor"].readVal<std::string>());
	reader.tryRead("psiTraining", _psiTraining);
	reader.tryRead("training", _training);
	reader.tryRead("returnToTrainingWhenHealed", _returnToTrainingWhenHealed);
	reader.tryRead("returnToTrainingsWhenOperationOver", _returnToTrainingsWhenOperationOver);
	if (!canGainExperience())
	{
		// Non-sapient robots must not resume any training or retain deferred
		// stat-growth work loaded from a soldier template/save.
		_psiTraining = false;
		_training = false;
		_returnToTrainingWhenHealed = false;
		_returnToTrainingsWhenOperationOver = NONE;
		_monthlyExperienceCache = UnitStats::scalar(0);
		_dogfightExperience = UnitStats::scalar(0);
		_researchExperience = UnitStats::scalar(0);
		_engineerExperience = UnitStats::scalar(0);
		_intelExperience = UnitStats::scalar(0);
	}
	reader.tryRead("justSaved", _justSaved);
	reader.tryRead("improvement", _improvement);
	for (const auto& layoutItem : reader["equipmentLayout"].children())
	{
		try
		{
			_equipmentLayout.push_back(new EquipmentLayoutItem(layoutItem, mod));
		}
		catch (Exception& ex)
		{
			Log(LOG_ERROR) << "Error loading Layout: " << ex.what();
		}
	}
	if (reader["death"])
	{
		_death = new SoldierDeath();
		_death->load(reader["death"]);
	}
	if (reader["diary"])
	{
		_diary = new SoldierDiary();
		_diary->load(reader["diary"], mod);
	}
	calcStatString(mod->getStatStrings());
	reader.tryRead("corpseRecovered", _corpseRecovered);
	reader.tryRead("previousTransformations", _previousTransformations);
	reader.tryRead("transformationBonuses", _transformationBonuses);
	reader.tryRead("pendingTransformations", _pendingTransformations);

	if (const auto& spawnInfo = reader["randomTransformationBonuses"])
	{
		WeightedOptions randomTransformationBonuses;
		randomTransformationBonuses.load(spawnInfo);
		int transformationBonusesCount = reader["transformationBonusesCount"].readVal(1); // if not provided, default is 1
		while (transformationBonusesCount > 0 && !randomTransformationBonuses.empty())
		{
			transformationBonusesCount--;
			std::string chosen = randomTransformationBonuses.choose();
			randomTransformationBonuses.set(chosen, 0);

			// Award a soldier bonus, if defined
			if (!Mod::isEmptyRuleName(chosen))
			{
				auto it2 = _transformationBonuses.find(chosen);
				if (it2 != _transformationBonuses.end())
				{
					it2->second += 1;
				}
				else
				{
					_transformationBonuses[chosen] = 1;
				}
			}
		}
	}

	_scriptValues.load(reader, shared);
}

/**
 * Saves the soldier to a YAML file.
 * @return YAML node.
 */
void Soldier::save(YAML::YamlNodeWriter writer, const ScriptGlobal *shared) const
{
	writer.setAsMap();

	writer.write("type", _rules->getType());
	writer.write("id", _id);
	writer.write("name", _name);
	if (!_callsign.empty())
		writer.write("callsign", _callsign);
	writer.write("nationality", _nationality);
	writer.write("initialStats", _initialStats);
	writer.write("currentStats", _currentStats);
	if (!_monthlyExperienceCache.empty())
		writer.write("monthlyExperienceCache", _monthlyExperienceCache);
	if (!_dogfightExperience.empty())
		writer.write("dogfightExperience", _dogfightExperience);
	if (!_researchExperience.empty())
		writer.write("researchExperience", _researchExperience);
	if (!_engineerExperience.empty())
		writer.write("engineerExperience", _engineerExperience);
	if (!_intelExperience.empty())
		writer.write("intelExperience", _intelExperience);
	if (_craft)
		_craft->saveId(writer["craft"]);
	if (_covertOperation)
		writer.write("covertOperation", _covertOperation->getOperationName());
	if (_researchProject)
		writer.write("researchProject", _researchProject->getRules()->getName());
	if (_production)
		writer.write("production", _production->getRules()->getName());
	if (_intelProject)
		writer.write("intelProject", _intelProject->getName());
	if (_prisoner)
		writer.write("activePrisoner", _prisoner->getId());
	if (_containedAlien)
		writer.write("activeContainedAlien", _containedAlien->getId());
	if (_containedMonster)
		writer.write("activeContainedMonster", _containedMonster->getId());
	writer.write("gender", _gender);
	writer.write("look", _look);
	writer.write("lookVariant", _lookVariant);
	writer.write("missions", _missions);
	writer.write("kills", _kills);
	writer.write("stuns", _stuns);
	if (_manaMissing > 0)
		writer.write("manaMissing", _manaMissing);
	if (_healthMissing > 0)
		writer.write("healthMissing", _healthMissing);
	if (_recovery > 0.0f)
		writer.write("recovery", _recovery);
	writer.write("armor", _armor->getType());
	if (_replacedArmor != 0)
		writer.write("replacedArmor", _replacedArmor->getType());
	if (_transformedArmor != 0)
		writer.write("transformedArmor", _transformedArmor->getType());
	if (_psiTraining)
		writer.write("psiTraining", _psiTraining);
	if (_training)
		writer.write("training", _training);
	if (_returnToTrainingWhenHealed)
		writer.write("returnToTrainingWhenHealed", _returnToTrainingWhenHealed);
	if (_returnToTrainingsWhenOperationOver != NONE)
		writer.write("returnToTrainingsWhenOperationOver", _returnToTrainingsWhenOperationOver);
	if (_justSaved)
		writer.write("justSaved", _justSaved);
	writer.write("isRookieSoldier", _isRookieSoldier);
	writer.write("isRookieAgent", _isRookieAgent);
	writer.write("isRookiePilot", _isRookiePilot);
	writer.write("improvement", _improvement);
	if (!_roles.empty())
	{
		auto rolesWriter = writer["roles"];
		rolesWriter.setAsSeq();
		for (auto* r : _roles)
		{
			// Rank zero is still meaningful: it preserves an unqualified role's
			// latent XP state even when monthly decay has brought that XP to zero.
			r->save(rolesWriter.write());
		}
	}
	writer.write("equipmentLayout", _equipmentLayout,
		[](YAML::YamlNodeWriter& w, EquipmentLayoutItem* i)
		{ i->save(w.write()); });
	if (_death != 0)
		 _death->save(writer["death"]);
	if (Options::soldierDiaries && (!_diary->getMissionIdList().empty() || !_diary->getSoldierCommendations()->empty() || _diary->getMonthsService() > 0))
		_diary->save(writer["diary"]);
	if (_corpseRecovered)
		writer.write("corpseRecovered", _corpseRecovered);
	if (!_previousTransformations.empty())
		writer.write("previousTransformations", _previousTransformations);
	if (!_transformationBonuses.empty())
		writer.write("transformationBonuses", _transformationBonuses);
	if (!_pendingTransformations.empty())
		writer.write("pendingTransformations", _pendingTransformations);

	_scriptValues.save(writer, shared);
}

/**
 * Returns the soldier's full name (and, optionally, statString).
 * @param statstring Add stat string?
 * @param maxLength Restrict length to a certain value.
 * @return Soldier name.
 */
std::string Soldier::getName(bool statstring, unsigned int maxLength) const
{
	if (statstring && !_statString.empty())
	{
		auto nameCodePointLength = Unicode::codePointLengthUTF8(_name);
		auto statCodePointLength = Unicode::codePointLengthUTF8(_statString);
		if (nameCodePointLength + statCodePointLength > maxLength)
		{
			return Unicode::codePointSubstrUTF8(_name, 0, maxLength - statCodePointLength) + "/" + _statString;
		}
		else
		{
			return _name + "/" + _statString;
		}
	}
	else
	{
		return _name;
	}
}

/**
 * Changes the soldier's full name.
 * @param name Soldier name.
 */
void Soldier::setName(const std::string &name)
{
	_name = name;
}

/**
 * Generates a new name based on nationality.
 */
void Soldier::genName()
{
	const std::vector<SoldierNamePool*>& names = _rules->getNames();
	if (!names.empty())
	{
		// Keep name, appearance and callsign tied to one valid identity pool.
		if (_nationality < 0 || static_cast<size_t>(_nationality) >= names.size())
		{
			_nationality = selectNamePoolIndex();
		}
		_name = names.at(_nationality)->genName(&_gender, _rules->getFemaleFrequency());
		_callsign = generateCallsign(_rules->getNames());
		_look = (SoldierLook)names.at(_nationality)->genLook(4); // Once we add the ability to mod in extra looks, this will need to reference the ruleset for the maximum amount of looks.
	}
	else
	{
		_nationality = 0;
	}
}

/**
 * Returns the soldier's callsign.
 * @param maxLength Restrict length to a certain value.
 * @return Soldier callsign.
 */
std::string Soldier::getCallsign(unsigned int maxLength) const
{
	std::ostringstream ss;
	ss << "\"";
	ss << Unicode::codePointSubstrUTF8(_callsign, 0, maxLength);
	ss << "\"";

	return ss.str();
}

/**
 * Changes the soldier's callsign.
 * @param callsign Soldier callsign.
 */
void Soldier::setCallsign(const std::string &callsign)
{
	_callsign = callsign;
}

/**
 * Check whether the soldier has a callsign assigned.
 * @return true, if the soldier has a callsign, false, if he has not.
 */
bool Soldier::hasCallsign() const
{
	return !_callsign.empty();
}

/**
 * Generate a random callsign from the same identity pool as the name and appearance.
 * @return generated callsign.
 */
std::string Soldier::generateCallsign(const std::vector<SoldierNamePool*> &names)
{
	std::string callsign = names.at(_nationality)->genCallsign(_gender);
	return callsign;
}

/**
* Returns the soldier's nationality.
* @return Nationality ID.
*/
int Soldier::getNationality() const
{
	return _nationality;
}

/**
* Changes the soldier's nationality.
* @param nationality Nationality ID.
*/
void Soldier::setNationality(int nationality)
{
	_nationality = nationality;
}

/**
 * Returns the craft the soldier is assigned to.
 * @return Pointer to craft.
 */
Craft *Soldier::getCraft() const
{
	return _craft;
}

/**
 * Assigns the soldier to a new craft.
 * @param craft Pointer to craft.
 */
void Soldier::setCraft(Craft *craft, bool resetCustomDeployment)
{
	_craft = craft;

	if (resetCustomDeployment && _craft)
	{
		// adding a soldier into a craft invalidates a custom craft deployment
		_craft->resetCustomDeployment();
	}
}
/**
 * Returns the soldier's craft string, which
 * is either the soldier's wounded status,
 * the assigned craft name, or none.
 * @param lang Language to get strings from.
 * @return Full name.
 */
std::string Soldier::getCurrentDuty(Language *lang, const BaseSumDailyRecovery &recovery, bool &isBusy, bool &isFree, DutyMode mode) const
{
	isBusy = false;
	isFree = false;
	bool facility = (mode == LAB || mode == ASSIGN || mode == WORK || mode == INTEL);
	if (_death)
	{
		isBusy = true;
		if (_death->getCause())
		{
			return lang->getString("STR_KILLED_IN_ACTION", _gender); //#FINNIKTODO: case we torture it to death?
		}
		else
		{
			return lang->getString("STR_MISSING_IN_ACTION", _gender);
		}
	}

	if (isWounded())
	{
		std::ostringstream ss;
		ss << lang->getString("STR_WOUNDED");
		ss << ">";
		int days = getNeededRecoveryTime(recovery);
		if (days < 0)
		{
			ss << "∞";
		}
		else
		{
			ss << days;
		}
		isBusy = true;
		return ss.str();
	}

	if (_covertOperation)
	{
		isBusy = true;
		return lang->getString("STR_COVERT_OPERATION_UC");
	}

	if (_researchProject)
	{
		if (mode == LAB)
		{
			return lang->getString(_researchProject->getRules()->getName());
		}
		else
		{
			return lang->getString("STR_IN_LAB");
		}
	}


	if (_production)
	{
		if (mode == WORK)
		{
			return lang->getString(_production->getRules()->getName());
		}
		else
		{
			return lang->getString("STR_IN_WORKSHOP");
		}
	}

	if (_intelProject)
	{
		if (mode == INTEL)
		{
			return lang->getString(_intelProject->getName());
		}
		else
		{
			return lang->getString("STR_INTEL");
		}
	}

	if (_prisoner)
	{
		if (mode == INTEL)
		{
			return _prisoner->getNameAndDesignation(lang);
		}
		else
		{
			return lang->getString("STR_INTEL"); //#FINNIKTODO a better string
		}
	}

	if (_containedAlien)
	{
		if (mode == LAB || mode == INTEL)
		{
			return _containedAlien->getNameAndDesignation(lang);
		}
		return lang->getString("STR_CONTAINED_ALIEN_STUDY");
	}

	if (_containedMonster)
	{
		return _containedMonster->getNameAndDesignation(lang);
	}

	if (hasPendingTransformation())
	{
		isBusy = true;
		std::ostringstream ss;
		ss << lang->getString("STR_IN_TRANSFORMATION_UC");
		ss << ">";
		int days = 0;
		isBusy = true;
		for (auto it = _pendingTransformations.cbegin(); it != _pendingTransformations.cend();)
		{
			if ((*it).second > 0)
			{
				days += (*it).second;
				++it;
			}
		}
		days = (int)ceil(days / 24);
		ss << days;
		return ss.str();
	}
	if (facility)
	{
		if (_psiTraining)
		{
			return lang->getString("STR_IN_PSI_TRAINING_UC");
		}
		if (_training)
		{
			return lang->getString("STR_COMBAT_TRAINING");
		}
	}


	if (_craft)
	{
		if (_craft->getStatus() == "STR_OUT")
		{
			isBusy = true;
			if (mode != INFO)
			{
				return lang->getString("STR_OUT");
			}
		}
		return _craft->getName(lang);
	}

	isFree = true;
	return lang->getString("STR_NONE_UC");
}

bool Soldier::isAvailableForBaseDuty() const
{
	return !_death
		&& !isWounded()
		&& !_covertOperation
		&& !_researchProject
		&& !_production
		&& !_intelProject
		&& !_prisoner
		&& !_containedAlien
		&& !_containedMonster
		&& _pendingTransformations.empty()
		&& !_psiTraining
		&& !_training
		&& !_craft;
}

/**
 * Clear all soldier tasks to prepare for some base duty (research project, manufacture, etc)
 */
void Soldier::clearBaseDuty()
{
	setResearchProject(nullptr);
	setProductionProject(nullptr);
	setIntelProject(nullptr);
	setActivePrisoner(nullptr);
	setActiveContainedAlien(nullptr);
	setActiveContainedMonster(nullptr);
	setPsiTraining(false);
	setTraining(false);
	setCraft(0);
	setReturnToTrainingWhenHealed(false);
}

/**
 * Returns a localizable-string representation of
 * the soldier's military rank.
 * @return String ID for rank.
 */
const std::string Soldier::getRankString() const
{
	const auto bestRoleRank = getBestRoleRank();
	if (bestRoleRank.first == ROLE_NONE)
	{
		return "STR_CIVILIAN";
	}
	return getRoleRankString(bestRoleRank.first, bestRoleRank.second);
}

const std::string Soldier::getRoleRankString(SoldierRole role, int rank) const
{
	std::string rankString = "UNDEFINED";
	for (const auto *ruleRole : _rules->getRoleRankStrings())
	{
		if (ruleRole->role == role)
		{
			for (const auto &rankStringEntry : ruleRole->strings)
			{
				if (rankStringEntry.first == rank)
				{
					rankString = rankStringEntry.second;
					break;
				}
			}
			break;
		}
	}
	return rankString;
}

/**
 * Returns the soldier's amount of missions.
 * @return Missions.
 */
int Soldier::getMissions() const
{
	return _missions;
}

/**
 * Returns the soldier's amount of kills.
 * @return Kills.
 */
int Soldier::getKills() const
{
	return _kills;
}

/**
 * Returns the soldier's amount of stuns.
 * @return Stuns.
 */
int Soldier::getStuns() const
{
	return _stuns;
}

/**
 * Returns the soldier's gender.
 * @return Gender.
 */
SoldierGender Soldier::getGender() const
{
	return _gender;
}

/**
 * Changes the soldier's gender (1/3 of avatar).
 * @param gender Gender.
 */
void Soldier::setGender(SoldierGender gender)
{
	_gender = gender;
}

/**
 * Returns the soldier's look.
 * @return Look.
 */
SoldierLook Soldier::getLook() const
{
	return _look;
}

/**
 * Changes the soldier's look (2/3 of avatar).
 * @param look Look.
 */
void Soldier::setLook(SoldierLook look)
{
	_look = look;
}

/**
 * Returns the soldier's look sub type.
 * @return Look.
 */
int Soldier::getLookVariant() const
{
	return _lookVariant;
}

/**
 * Changes the soldier's look variant (3/3 of avatar).
 * @param lookVariant Look sub type.
 */
void Soldier::setLookVariant(int lookVariant)
{
	_lookVariant = lookVariant;
}

/**
 * Returns the soldier's rules.
 * @return rule soldier
 */
const RuleSoldier *Soldier::getRules() const
{
	return _rules;
}

/**
 * Returns the soldier's unique ID. Each soldier
 * can be identified by its ID. (not it's name)
 * @return Unique ID.
 */
int Soldier::getId() const
{
	return _id;
}

/**
 * Add a mission to the counter.
 */
void Soldier::addMissionCount()
{
	_missions++;
}

/**
 * Add a kill to the counter.
 */
void Soldier::addKillCount(int count)
{
	_kills += count;
}

/**
 * Add a stun to the counter.
 */
void Soldier::addStunCount(int count)
{
	_stuns += count;
}

int Soldier::getHireValue() const
{
	// Base cost comes from rules and may be adjusted by role rank.
	const int baseCost = _rules->getBuyCost();
	const auto bestRoleRank = getBestRoleRank();
	// A rank-zero latent role must not make an otherwise valid recruit free.
	const int roleRank = std::max(1, bestRoleRank.second);
	const double baseValue = static_cast<double>(baseCost) * static_cast<double>(roleRank);
	double value = baseValue;

	// Scale the cost based on current stats relative to the *average* initial stats.
	// - If all stats are at min => -30%
	// - If all stats are at max => +30%
	// - Stats above max can increase cost further, but asymptotically capped at +60%
	const UnitStats cur = _currentStats;
	const UnitStats min = _rules->getMinStats();
	const UnitStats max = _rules->getMaxStats();

	// Normalized deviation from average:
	// -1 at min, +1 at max.
	// For stats above max, we allow values > 1 but with soft cap to avoid huge outliers.
	auto addStat = [](double &sumT, int &count, double &maxT, std::string &maxTName,
		const std::string &name, int curV, int minV, int maxV)
	{
		if (maxV == minV)
			return;
		if (maxV < minV)
			std::swap(maxV, minV);
		const double avgV = (static_cast<double>(minV) + static_cast<double>(maxV)) * 0.5;
		const double halfRange = (static_cast<double>(maxV) - static_cast<double>(minV)) * 0.5;
		if (halfRange == 0.0)
			return;

		double t = 0.0;
		if (curV <= minV)
		{
			t = -1.0;
		}
		else if (curV <= maxV)
		{
			// Inside the initial range: clamp to [-1, +1].
			t = (static_cast<double>(curV) - avgV) / halfRange;
			t = std::clamp(t, -1.0, 1.0);
		}
		else
		{
			// Above max: soft-cap the excess so a single stat can't explode the average.
			// excessNorm = 0 at max, grows with distance above max.
			const double excessNorm = (static_cast<double>(curV) - static_cast<double>(maxV)) / std::max(1.0, (static_cast<double>(maxV) - static_cast<double>(minV)));
			constexpr double kExcess = 1.0;
			// t goes from 1.0 at max towards 2.0 asymptotically.
			t = 1.0 + (1.0 - std::exp(-kExcess * excessNorm));
		}

		sumT += t;
		++count;
		if (t > maxT)
		{
			maxT = t;
			maxTName = name;
		}
	};

	double sumT = 0.0;
	int count = 0;
	double maxT = -1e100;
	std::string maxTName;
	UnitStats::fieldLoop(
		[&](UnitStats::Ptr p)
		{
			// Use stat string key for diagnostics; avoids hardcoding each field name here.
			const std::string statName = UnitStats::getStatString(p, UnitStats::STATSTR_UC);
			addStat(
				sumT, count, maxT, maxTName,
				statName,
				static_cast<int>(cur.*p),
				static_cast<int>(min.*p),
				static_cast<int>(max.*p)
			);
		}
	);

	const double avgT = (count > 0) ? (sumT / static_cast<double>(count)) : 0.0;

	double coeff = 1.0;
	if (avgT <= 1.0)
	{
		coeff = 1.0 + 0.3 * std::clamp(avgT, -1.0, 1.0);
	}
	else
	{
		// Asymptotically approach +60% total multiplier.
		constexpr double k = 1.0;
		coeff = 1.3 + 0.3 * (1.0 - std::exp(-k * (avgT - 1.0)));
	}
	coeff = std::clamp(coeff, 0.7, 1.6);

	value *= coeff;

	int result = static_cast<int>(value);
	result /= 100;
	result *= 100;
	return result;
}

/**
 * Get pointer to initial stats.
 */
const UnitStats* Soldier::getInitStats() const
{
	return &_initialStats;
}

/**
 * Get pointer to current stats.
 */
UnitStats *Soldier::getCurrentStatsEditable()
{
	return &_currentStats;
}
const UnitStats* Soldier::getCurrentStats() const
{
	return &_currentStats;
}

void Soldier::setBothStats(UnitStats *stats)
{
	_currentStats = *stats;
	_initialStats = *stats;
}

/**
 * Returns the unit's current armor.
 * @return Pointer to armor data.
 */
Armor *Soldier::getArmor() const
{
	return _armor;
}

/**
 * Changes the unit's current armor.
 * @param armor Pointer to armor data.
 */
void Soldier::setArmor(Armor *armor, bool resetCustomDeployment)
{
	if (resetCustomDeployment && _craft && _armor && armor && _armor->getSize() < armor->getSize())
	{
		// increasing the size of a soldier's armor invalidates a custom craft deployment
		_craft->resetCustomDeployment();
	}

	_armor = armor;
}

/**
 * Returns a list of armor layers (sprite names).
 */
const std::vector<std::string>& Soldier::getArmorLayers(Armor *customArmor) const
{
	std::stringstream ss;

	const Armor *armor = customArmor ? customArmor : _armor;

	const std::string gender = _gender == GENDER_MALE ? "M" : "F";
	const auto& layoutDefinition = armor->getLayersDefinition();

	// find relevant layer
	for (int i = 0; i <= RuleSoldier::LookVariantBits; ++i)
	{
		ss.str("");
		ss << gender;
		ss << (int)_look + (_lookVariant & (RuleSoldier::LookVariantMask >> i)) * 4;
		auto it = layoutDefinition.find(ss.str());
		if (it != layoutDefinition.end())
		{
			return it->second;
		}
	}

	{
		// try also gender + hardcoded look 0
		ss.str("");
		ss << gender << "0";
		auto it = layoutDefinition.find(ss.str());
		if (it != layoutDefinition.end())
		{
			return it->second;
		}
	}

	throw Exception("Layered armor sprite definition (" + armor->getType() + ") not found!");
}

/**
* Gets the soldier's original armor (before replacement).
* @return Pointer to armor data.
*/
Armor *Soldier::getReplacedArmor() const
{
	return _replacedArmor;
}

/**
* Backs up the soldier's original armor (before replacement).
* @param armor Pointer to armor data.
*/
void Soldier::setReplacedArmor(Armor *armor)
{
	_replacedArmor = armor;
}

/**
* Gets the soldier's original armor (before transformation).
* @return Pointer to armor data.
*/
Armor *Soldier::getTransformedArmor() const
{
	return _transformedArmor;
}

/**
* Backs up the soldier's original armor (before transformation).
* @param armor Pointer to armor data.
*/
void Soldier::setTransformedArmor(Armor *armor)
{
	_transformedArmor = armor;
}

namespace
{

/**
 * Calculates absolute threshold based on percentage threshold.
 * @param base base value
 * @param threshold threshold in percent
 * @return threshold in absolute value
 */
int valueThreshold(int base, int threshold)
{
	return base * threshold / 100;
}

/**
 * Calculates the amount that exceeds the threshold of the base value.
 * @param value value to check
 * @param base base value
 * @param threshold threshold of the base value in percent
 * @return absolute value over the threshold
 */
int valueOverThreshold(int value, int base, int threshold)
{
	return std::max(0, value - valueThreshold(base, threshold));
}

/**
 * Calculates how long will it take to recover.
 * @param current Total amount of days.
 * @param recovery Recovery per day.
 * @return How many days will it take to recover, can return -1 meaning infinity.
 */
int recoveryTime(int current, int recovery)
{
	if (current <= 0)
	{
		return 0;
	}

	if (recovery <= 0)
		return -1; // represents infinity

	int days = current / recovery;
	if (current % recovery > 0)
		++days;

	return days;
}


}


/**
* Is the soldier wounded or not?.
* @return True if wounded.
*/
bool Soldier::isWounded() const
{
	if (_manaMissing > 0)
	{
		if (valueOverThreshold(_manaMissing, _currentStats.mana, _rules->getManaWoundThreshold()))
		{
			return true;
		}
	}
	if (_healthMissing > 0)
	{
		if (valueOverThreshold(_healthMissing, _currentStats.health, _rules->getHealthWoundThreshold()))
		{
			return true;
		}
	}
	return _recovery > 0.0f;
}

/**
* Is the soldier wounded or not?.
* @return False if wounded.
*/
bool Soldier::hasFullHealth() const
{
	return !isWounded();
}

/**
 * Is the soldier capable of defending a base?.
 * @return False if wounded too much.
 */
bool Soldier::canDefendBase() const
{
	int currentHealthPercentage = std::max(0, _currentStats.health - getWoundRecoveryInt() - getHealthMissing()) * 100 / _currentStats.health;
	return currentHealthPercentage >= Options::oxceWoundedDefendBaseIf;
}


/**
 * Returns the amount of missing mana.
 * @return Missing mana.
 */
int Soldier::getManaMissing() const
{
	return _manaMissing;
}

/**
 * Sets the amount of missing mana.
 * @param manaMissing Missing mana.
 */
void Soldier::setManaMissing(int manaMissing)
{
	_manaMissing = std::max(manaMissing, 0);
}

/**
 * Returns the amount of time until the soldier's mana is fully replenished.
 * @return Number of days. -1 represents infinity.
 */
int Soldier::getManaRecovery(int manaRecoveryPerDay) const
{
	return recoveryTime(_manaMissing, manaRecoveryPerDay);
}


/**
 * Returns the amount of missing health.
 * @return Missing health.
 */
int Soldier::getHealthMissing() const
{
	return _healthMissing;
}

/**
 * Sets the amount of missing health.
 * @param healthMissing Missing health.
 */
void Soldier::setHealthMissing(int healthMissing)
{
	_healthMissing = std::max(healthMissing, 0);
}

/**
 * Returns the amount of time until the soldier's health is fully replenished.
 * @return Number of days. -1 represents infinity.
 */
int Soldier::getHealthRecovery(int healthRecoveryPerDay) const
{
	return recoveryTime(_healthMissing, healthRecoveryPerDay);
}


/**
 * Returns the amount of time until the soldier is healed.
 * @return Number of days.
 */
int Soldier::getWoundRecoveryInt() const
{
	// Note: only for use in Yankes scripts! ...and in base defense HP calculations :/
	return (int)(std::ceil(_recovery));
}

int Soldier::getWoundRecovery(float absBonus, float relBonus) const
{
	float hpPerDay = 1.0f + absBonus + (relBonus * _currentStats.health * 0.01f);
	return (int)(std::ceil(_recovery / hpPerDay));
}

/**
 * Changes the amount of time until the soldier is healed.
 * @param recovery Number of days.
 */
void Soldier::setWoundRecovery(int recovery)
{
	_recovery = std::max(recovery, 0);
}


/**
 * Heals soldier wounds.
 */
void Soldier::healWound(float absBonus, float relBonus)
{
	// 1 hp per day as minimum
	_recovery -= 1.0f;

	// absolute bonus from sick bay facilities
	_recovery -= absBonus;

	// relative bonus from sick bay facilities
	_recovery -= (relBonus * _currentStats.health * 0.01f);

	if (_recovery < 0.0f)
		_recovery = 0.0f;
}

/**
 * Replenishes the soldier's mana.
 */
void Soldier::replenishMana(int manaRecoveryPerDay)
{
	_manaMissing -= manaRecoveryPerDay;

	if (_manaMissing < 0)
		_manaMissing = 0;

	// maximum amount of mana missing can be up to 2x the current mana pool (WITHOUT armor and bonuses!); at least 100
	int maxThreshold = std::max(100, _currentStats.mana * 2);
	if (_manaMissing > maxThreshold)
		_manaMissing = maxThreshold;
}

/**
 * Replenishes the soldier's health.
 */
void Soldier::replenishHealth(int healthRecoveryPerDay)
{
	_healthMissing -= healthRecoveryPerDay;

	if (_healthMissing < 0)
		_healthMissing = 0;
}

/**
 * Daily stat replenish and healing of the soldier based on the facilities available in the base.
 * @param recovery Recovery values provided by the base.
 */
void Soldier::replenishStats(const BaseSumDailyRecovery& recovery)
{
	if (_recovery > 0.0f)
	{
		healWound(recovery.SickBayAbsoluteBonus, recovery.SickBayRelativeBonus);
	}
	else
	{
		if (getManaMissing() > 0 && recovery.ManaRecovery > 0)
		{
			// positive mana recovery only when NOT wounded
			replenishMana(recovery.ManaRecovery);
		}

		if (getHealthMissing() > 0 && recovery.HealthRecovery > 0)
		{
			// health recovery only when NOT wounded
			replenishHealth(recovery.HealthRecovery);
		}
	}

	if (recovery.ManaRecovery  < 0)
	{
		// negative mana recovery always
		replenishMana(recovery.ManaRecovery);
	}
}

/**
 * Gets number of days until the soldier is ready for action again.
 * @return Number of days. -1 represents infinity.
 */
int Soldier::getNeededRecoveryTime(const BaseSumDailyRecovery& recovery) const
{
	int time = getWoundRecovery(recovery.SickBayAbsoluteBonus, recovery.SickBayRelativeBonus);

	int bonusTime = 0;
	if (_healthMissing > 0)
	{
		int t = recoveryTime(
			valueOverThreshold(_healthMissing, _currentStats.health, _rules->getHealthWoundThreshold()),
			recovery.HealthRecovery
		);

		if (t < 0)
		{
			return t;
		}

		bonusTime = std::max(bonusTime, t);
	}
	if (_manaMissing > 0)
	{
		int t = recoveryTime(
			valueOverThreshold(_manaMissing, _currentStats.mana, _rules->getManaWoundThreshold()),
			recovery.ManaRecovery
		);

		if (t < 0)
		{
			return t;
		}

		bonusTime = std::max(bonusTime, t);
	}

	return time + bonusTime;
}



/**
 * Returns the list of EquipmentLayoutItems of a soldier.
 * @return Pointer to the EquipmentLayoutItem list.
 */
std::vector<EquipmentLayoutItem*> *Soldier::getEquipmentLayout()
{
	return &_equipmentLayout;
}

/**
 * Trains a soldier's Psychic abilities after 1 day.
 */
void Soldier::trainPsi1Day()
{
	if (!canGainExperience())
	{
		_improvement = 0;
		return;
	}
	if (!_psiTraining)
	{
		_improvement = 0;
		return;
	}

	if (_currentStats.psiSkill > 0) // yes, 0. _rules->getMinStats().psiSkill was wrong.
	{
		if (8 * 100 >= _currentStats.psiSkill * RNG::generate(1, 100) && _currentStats.psiSkill < _rules->getStatCaps().psiSkill)
		{
			++_improvement;
			++_currentStats.psiSkill;
		}
	}
	else if (_currentStats.psiSkill < _rules->getMinStats().psiSkill)
	{
		if (++_currentStats.psiSkill == _rules->getMinStats().psiSkill)	// initial training is over
		{
			_improvement = _rules->getMaxStats().psiSkill + RNG::generate(0, _rules->getMaxStats().psiSkill / 2);
			_currentStats.psiSkill = _improvement;
		}
	}
	else // minStats.psiSkill <= 0 && _currentStats.psiSkill == minStats.psiSkill
		_currentStats.psiSkill -= RNG::generate(30, 60);	// set initial training from 30 to 60 days
}

/**
 * Is the soldier already fully psi-trained?
 * @return True, if the soldier cannot gain any more stats in the psi-training facility.
 */
bool Soldier::isFullyPsiTrained()
{
	if (!canGainExperience())
	{
		return true;
	}
	return _currentStats.psiSkill >= _rules->getStatCaps().psiSkill;
}

/**
 * returns whether or not the unit is in psi training
 * @return true/false
 */
bool Soldier::isInPsiTraining() const
{
	return _psiTraining;
}

/**
 * changes whether or not the unit is in psi training
 */
void Soldier::setPsiTraining(bool psi)
{
	_psiTraining = psi && canGainExperience();
}

/**
 * returns this soldier's psionic skill improvement score for this month.
 * @return score
 */
int Soldier::getImprovement() const
{
	return _improvement;
}

/**
 * Returns the soldier's death details.
 * @return Pointer to death data. NULL if no death has occurred.
 */
SoldierDeath *Soldier::getDeath() const
{
	return _death;
}

/**
 * Kills the soldier in the Geoscape.
 * @param death Pointer to death data.
 */
void Soldier::die(SoldierDeath *death)
{
	delete _death;
	_death = death;

	// Clean up associations
	_craft = 0;
	_covertOperation = 0;
	_researchProject = 0;
	_production = 0;
	_intelProject = 0;
	_psiTraining = false;
	_training = false;
	_returnToTrainingWhenHealed = false;
	_returnToTrainingsWhenOperationOver = NONE;
	_manaMissing = 0;
	_healthMissing = 0;
	_recovery = 0.0f;
	clearEquipmentLayout();
}

/**
 * Clears the equipment layout.
 */
void Soldier::clearEquipmentLayout()
{
	for (auto* entry : _equipmentLayout)
	{
		delete entry;
	}
	_equipmentLayout.clear();
}

/**
 * Returns the soldier's diary.
 * @return Diary.
 */
SoldierDiary *Soldier::getDiary()
{
	return _diary;
}
const SoldierDiary* Soldier::getDiary() const
{
	return _diary;
}

/**
* Resets the soldier's diary.
*/
void Soldier::resetDiary()
{
	delete _diary;
	_diary = new SoldierDiary();
}

/**
 * Calculates the soldier's statString
 * Calculates the soldier's statString.
 * @param statStrings List of statString rules.
 */
void Soldier::calcStatString(const std::vector<StatString *> &statStrings)
{
	if (_rules->getStatStrings().empty())
	{
		_statString = StatString::calcStatString(_currentStats, statStrings, _psiTraining);
	}
	else
	{
		_statString = StatString::calcStatString(_currentStats, _rules->getStatStrings(), _psiTraining);
	}
}

/**
 * Trains a soldier's Physical abilities
 */
void Soldier::trainPhys(int customTrainingFactor)
{
	if (!canGainExperience())
	{
		return;
	}
	UnitStats caps1 = _rules->getStatCaps();
	UnitStats caps2 = _rules->getTrainingStatCaps();

	// no P.T. for the wounded
	int roleFactor = 1;

	const int soldierRank = getRoleRank(ROLE_SOLDIER);
	if (soldierRank < 1)
	{
		roleFactor = 4;
	}
	else if (soldierRank >= 4)
	{
		roleFactor = 2;
	}
	// no P.T. for the wounded

	if (hasFullHealth())
	{
		if (_currentStats.firing < caps1.firing && RNG::generate(0, caps2.firing) > _currentStats.firing && RNG::percent(customTrainingFactor / roleFactor))
		{
			_currentStats.firing++;
		}

		if (_currentStats.health < caps1.health && RNG::generate(0, caps2.health) > _currentStats.health && RNG::percent(customTrainingFactor / roleFactor))
		{
			_currentStats.health++;
		}

		if (_currentStats.melee < caps1.melee && RNG::generate(0, caps2.melee) > _currentStats.melee && RNG::percent(customTrainingFactor / roleFactor))
		{
			_currentStats.melee++;
		}

		if (_currentStats.throwing < caps1.throwing && RNG::generate(0, caps2.throwing) > _currentStats.throwing && RNG::percent(customTrainingFactor / roleFactor))
		{
			_currentStats.throwing++;
		}

		if (_currentStats.strength < caps1.strength && RNG::generate(0, caps2.strength) > _currentStats.strength && RNG::percent(customTrainingFactor / roleFactor))
		{
			_currentStats.strength++;
		}

		if (_currentStats.tu < caps1.tu && RNG::generate(0, caps2.tu) > _currentStats.tu && RNG::percent(customTrainingFactor / roleFactor))

		{
			_currentStats.tu++;
		}

		if (_currentStats.stamina < caps1.stamina && RNG::generate(0, caps2.stamina) > _currentStats.stamina && RNG::percent(customTrainingFactor / roleFactor))
		{
			_currentStats.stamina++;
		}
	}
}

/**
 * Is the soldier already fully trained?
 * @return True, if the soldier cannot gain any more stats in the training facility.
 */
bool Soldier::isFullyTrained() const
{
	if (!canGainExperience())
	{
		return true;
	}
	UnitStats trainingCaps = _rules->getTrainingStatCaps();

	if (_currentStats.firing < trainingCaps.firing
		|| _currentStats.health < trainingCaps.health
		|| _currentStats.melee < trainingCaps.melee
		|| _currentStats.throwing < trainingCaps.throwing
		|| _currentStats.strength < trainingCaps.strength
		|| _currentStats.tu < trainingCaps.tu
		|| _currentStats.stamina < trainingCaps.stamina)
	{
		return false;
	}
	return true;
}

/**
 * returns whether or not the unit is in physical training
 */
bool Soldier::isInTraining() const
{
	return _training;
}

/**
 * changes whether or not the unit is in physical training
 */
void Soldier::setTraining(bool training)
{
	_training = training && canGainExperience();
}

/**
 * Should the soldier return to martial training automatically when fully healed?
 */
bool Soldier::getReturnToTrainingWhenHealed() const
{
	return _returnToTrainingWhenHealed;
}

/**
 * Sets whether the soldier should return to martial training automatically when fully healed.
 */
void Soldier::setReturnToTrainingWhenHealed(bool returnToTrainingWhenHealed)
{
	_returnToTrainingWhenHealed = returnToTrainingWhenHealed && canGainExperience();
}

/**
 * Sets whether or not the unit's corpse was recovered from a battle
 */
void Soldier::setCorpseRecovered(bool corpseRecovered)
{
	_corpseRecovered = corpseRecovered;
}

/**
 * Gets the previous transformations performed on this soldier
 * @return The map of previous transformations
 */
std::map<std::string, int> &Soldier::getPreviousTransformations()
{
	return _previousTransformations;
}

bool Soldier::hasCompatibleRolesForTransformation(const RuleSoldierTransformation *transformationRule) const
{
	const RuleSoldier* destinationType = transformationRule->getProducedSoldier();
	if (!destinationType)
	{
		destinationType = _rules;
	}
	const auto destinationDefaultRoles = destinationType->getRoles();
	const bool destinationIsRobot = std::find(
		destinationDefaultRoles.begin(), destinationDefaultRoles.end(), ROLE_ROBOT) != destinationDefaultRoles.end();

	auto supportsRoleRank = [destinationType](SoldierRole role, int rank)
	{
		if (role == ROLE_NONE)
		{
			return true;
		}
		if (role == ROLE_ROBOT)
		{
			const auto destinationRoles = destinationType->getRoles();
			return std::find(destinationRoles.begin(), destinationRoles.end(), ROLE_ROBOT) != destinationRoles.end();
		}
		for (const auto* requirements : destinationType->getRoleExpRequirements())
		{
			if (requirements && requirements->role == role && !requirements->requirements.empty())
			{
				const int maximumRank = requirements->requirements.rbegin()->first + 1;
				return rank <= maximumRank;
			}
		}
		return false;
	};

	std::vector<SoldierRole> resultingRoles;
	if (transformationRule->isCreatingClone())
	{
		resultingRoles = destinationDefaultRoles;
		for (SoldierRole role : resultingRoles)
		{
			if (!supportsRoleRank(role, 1))
			{
				return false;
			}
		}
	}
	else
	{
		for (const auto* entry : _roles)
		{
			if (!entry || !supportsRoleRank(entry->role, entry->rank))
			{
				return false;
			}
			resultingRoles.push_back(entry->role);
		}
	}
	if (destinationIsRobot &&
		(resultingRoles.size() != 1 || resultingRoles.front() != ROLE_ROBOT))
	{
		return false;
	}

	if (transformationRule->hasRoleToAdd())
	{
		if (!supportsRoleRank(transformationRule->getRoleToAdd(), 1) ||
			std::find(resultingRoles.begin(), resultingRoles.end(), ROLE_ROBOT) != resultingRoles.end())
		{
			return false;
		}
	}
	return true;
}

/**
 * Checks whether or not the soldier is eligible for a certain transformation
 */
bool Soldier::isEligibleForTransformation(const RuleSoldierTransformation *transformationRule) const
{
	// alive and well
	if (!_death && !isWounded() && !transformationRule->isAllowingAliveSoldiers())
		return false;

	// alive and wounded
	if (!_death && isWounded() && !transformationRule->isAllowingWoundedSoldiers())
		return false;

	// dead
	if (_death && !transformationRule->isAllowingDeadSoldiers())
		return false;

	// dead and vaporized, or missing in action
	if (_death && !_corpseRecovered && transformationRule->needsCorpseRecovered())
		return false;

	// Is the soldier of the correct type?
	const auto& allowedTypes = transformationRule->getAllowedSoldierTypes();
	auto it = std::find(allowedTypes.begin(), allowedTypes.end(), _rules->getType());
	if (it == allowedTypes.end())
		return false;
	if (!hasCompatibleRolesForTransformation(transformationRule))
		return false;

	// Does this soldier's transformation history preclude this new project?
	for (const auto& reqd_trans : transformationRule->getRequiredPreviousTransformations())
	{
		if (_previousTransformations.find(reqd_trans) == _previousTransformations.end())
			return false;
	}

	for (const auto& forb_trans : transformationRule->getForbiddenPreviousTransformations())
	{
		if (_previousTransformations.find(forb_trans) != _previousTransformations.end())
			return false;
	}

	if(!_pendingTransformations.empty())
		return false;

	// Does this soldier meet the minimum stat requirements for the project?
	UnitStats currentStats = transformationRule->getIncludeBonusesForMinStats() ? _tmpStatsWithSoldierBonuses: _currentStats;
	UnitStats minStats = transformationRule->getRequiredMinStats();
	bool minStatReq = true;
	currentStats.fieldLoop(
		[&](UnitStats::Ptr p)
		{
			if (currentStats.*p < minStats.*p)
				minStatReq = false;
		}
	);

	if (!minStatReq)
		return false;

	// Does the soldier met role rank requirements?
	if (!transformationRule->getRoleRankRequirements().empty())
	{
		for (const auto &req : transformationRule->getRoleRankRequirements())
		{
			if (getRoleRank(req.first) < req.second)
				return false;
		}
	}

	if (transformationRule->getForbiddenRole() != ROLE_NONE)
	{
		if (getRoleRank(transformationRule->getForbiddenRole()) > 0)
		{
			return false;
		}
	}
	// Does this soldier meet the maximum stat requirements for the project?
	currentStats = transformationRule->getIncludeBonusesForMaxStats() ? _tmpStatsWithSoldierBonuses : _currentStats;
	UnitStats maxStats = transformationRule->getRequiredMaxStats();
	if (currentStats.tu > maxStats.tu ||
		currentStats.stamina > maxStats.stamina ||
		currentStats.health > maxStats.health ||
		currentStats.bravery > maxStats.bravery ||
		currentStats.reactions > maxStats.reactions ||
		currentStats.firing > maxStats.firing ||
		currentStats.throwing > maxStats.throwing ||
		currentStats.melee > maxStats.melee ||
		currentStats.mana > maxStats.mana ||
		currentStats.strength > maxStats.strength ||
		currentStats.psiStrength > maxStats.psiStrength ||
		currentStats.psiSkill > maxStats.psiSkill ||
		currentStats.maneuvering > maxStats.maneuvering ||
		currentStats.missiles > maxStats.missiles ||
		currentStats.dogfight > maxStats.dogfight ||
		currentStats.tracking > maxStats.tracking ||
		currentStats.cooperation > maxStats.cooperation ||
		currentStats.beams > maxStats.beams ||
		currentStats.synaptic > maxStats.synaptic ||
		currentStats.gravity > maxStats.gravity ||
		currentStats.physics > maxStats.physics ||
		currentStats.chemistry > maxStats.chemistry ||
		currentStats.biology > maxStats.biology ||
		currentStats.insight > maxStats.insight ||
		currentStats.data > maxStats.data ||
		currentStats.computers > maxStats.computers ||
		currentStats.tactics > maxStats.tactics ||
		currentStats.materials > maxStats.materials ||
		currentStats.designing > maxStats.designing ||
		currentStats.psionics > maxStats.psionics ||
		currentStats.xenolinguistics > maxStats.xenolinguistics ||
		currentStats.weaponry > maxStats.weaponry ||
		currentStats.explosives > maxStats.explosives ||
		currentStats.efficiency > maxStats.efficiency ||
		currentStats.microelectronics > maxStats.microelectronics ||
		currentStats.metallurgy > maxStats.metallurgy ||
		currentStats.processing > maxStats.processing ||
		currentStats.hacking > maxStats.hacking ||
		currentStats.robotics > maxStats.robotics ||
		currentStats.diligence > maxStats.diligence ||
		currentStats.alienTech > maxStats.alienTech ||
		currentStats.reverseEngineering > maxStats.reverseEngineering ||
		currentStats.stealth > maxStats.stealth ||
		currentStats.perception > maxStats.perception ||
		currentStats.charisma > maxStats.charisma ||
		currentStats.investigation > maxStats.investigation ||
		currentStats.deception > maxStats.deception ||
		currentStats.interrogation > maxStats.interrogation)
		return false;

	// Does the soldier have the required commendations?
	for (const auto& reqd_comm : transformationRule->getRequiredCommendations())
	{
		bool found = false;
		for (const auto* comm : *_diary->getSoldierCommendations())
		{
			if (comm->getDecorationLevelInt() >= reqd_comm.second && comm->getType() == reqd_comm.first)
			{
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	return true;
}

/**
 * Performs a transformation on this unit
 */
void Soldier::transform(const Mod *mod, RuleSoldierTransformation *transformationRule, Soldier *sourceSoldier, Base *base)
{
	if (!hasCompatibleRolesForTransformation(transformationRule))
	{
		throw Exception("Transformation '" + transformationRule->getName() +
			"' is incompatible with the roles of soldier '" + _name + "'.");
	}

	if (_death)
	{
		_corpseRecovered = false; // They're not a corpse anymore!
		delete _death;
		_death = 0;
	}

	if (transformationRule->getRecoveryTime() > 0)
	{
		_recovery = transformationRule->getRecoveryTime();
	}

	// needed, because the armor size may change (also, it just makes sense)
	sourceSoldier->setCraft(0);

	if (transformationRule->isCreatingClone())
	{
		// a clone already has the correct soldier type, but random stats
		// if we don't want random stats, let's copy them from the source soldier
		UnitStats sourceStats = *sourceSoldier->getCurrentStats() + calculateStatChanges(mod, transformationRule, sourceSoldier, 0, sourceSoldier->getRules());
		UnitStats mergedStats = UnitStats::combine(transformationRule->getRerollStats(), sourceStats, _currentStats);
		setBothStats(&mergedStats);
	}
	else
	{
		// backup original soldier type, it will still be needed later for stat change calculations
		const RuleSoldier* sourceSoldierType = _rules;

		// change soldier type if needed
		if (!Mod::isEmptyRuleName(transformationRule->getProducedSoldierType()) && _rules->getType() != transformationRule->getProducedSoldierType())
		{
			_rules = mod->getSoldier(transformationRule->getProducedSoldierType());

			// clamp (and randomize) nationality if needed
			{
				const std::vector<SoldierNamePool *> &names = _rules->getNames();
				if (!names.empty())
				{
					if (_nationality < 0 || static_cast<size_t>(_nationality) >= names.size())
					{
						_nationality = selectNamePoolIndex();
					}
				}
				else
				{
					_nationality = 0;
				}
			}
		}

		// handle training (soldier type change rules)
		if (sourceSoldierType != _rules && _rules->getTrainingStatCaps().psiSkill <= 0)
		{
			// transformed into a new soldier type, which doesn't support psi training
			_psiTraining = false;
		}
		// handle training (recovery rules)
		if (_training && isWounded())
		{
			_training = false;
			setReturnToTrainingWhenHealed(true);
		}

		// change stats
		_currentStats += calculateStatChanges(mod, transformationRule, sourceSoldier, 0, sourceSoldierType);

		// and randomize stats where needed
		{
			Soldier *tmpSoldier = new Soldier(_rules, nullptr, _id);
			_currentStats = UnitStats::combine(transformationRule->getRerollStats(), _currentStats, *tmpSoldier->getCurrentStats());
			delete tmpSoldier;
			tmpSoldier = 0;
		}
	}

	// Qualification is explicit. ROLE_NONE is a civilian marker and can never
	// be acquired through a transformation. Clone transformations use the same
	// contract as in-place transformations.
	if (transformationRule->hasRoleToAdd())
	{
		const SoldierRole roleToAdd = transformationRule->getRoleToAdd();
		if (!hasRole(roleToAdd) && !addRole(roleToAdd))
		{
			throw Exception("Transformation '" + transformationRule->getName() + "' could not qualify soldier '" + _name + "' for its configured role.");
		}
	}
	normalizeRoles();

	if (!transformationRule->isKeepingSoldierArmor())
	{
		auto* oldArmor = _armor;
		if (Mod::isEmptyRuleName(transformationRule->getProducedSoldierArmor()))
		{
			// default armor of the soldier's type
			_armor = _rules->getDefaultArmor();
		}
		else
		{
			// explicitly defined armor
			_armor = mod->getArmor(transformationRule->getProducedSoldierArmor());
		}
		if (oldArmor != _armor && !transformationRule->isCreatingClone())
		{
			if (oldArmor->getStoreItem())
			{
				base->getStorageItems()->addItem(oldArmor->getStoreItem());
			}
		}
	}

	// Reset performed transformations (on the destination soldier), if needed
	if (transformationRule->getReset())
	{
		_previousTransformations.clear();
	}
	else if (!transformationRule->getRemoveTransformations().empty())
	{
		// Remove specific transformations and their related bonuses
		for (const auto& remove_transf : transformationRule->getRemoveTransformations())
		{
			int count = 0;
			auto it1 = _previousTransformations.find(remove_transf);
			if (it1 != _previousTransformations.end())
			{
				count = it1->second;
				_previousTransformations.erase(remove_transf);
			}
			if (count > 0)
			{
				const auto* rtRule = mod->getSoldierTransformation(remove_transf, false);
				if (rtRule)
				{
					if (!Mod::isEmptyRuleName(rtRule->getSoldierBonusType()))
					{
						auto it2 = _transformationBonuses.find(rtRule->getSoldierBonusType());
						if (it2 != _transformationBonuses.end())
						{
							if (it2->second > count)
							{
								it2->second -= count;
							}
							else
							{
								_transformationBonuses.erase(rtRule->getSoldierBonusType());
							}
						}
					}
				}
			}
		}
	}

	// Remember the performed transformation (on the source soldier)
	auto& history = sourceSoldier->getPreviousTransformations();
	auto it = history.find(transformationRule->getName());
	if (it != history.end())
	{
		it->second += 1;
	}
	else
	{
		history[transformationRule->getName()] = 1;
	}

	// Reset soldier bonuses, if needed
	if (transformationRule->getReset())
	{
		_transformationBonuses.clear();
	}

	// Award a soldier bonus, if defined
	if (!Mod::isEmptyRuleName(transformationRule->getSoldierBonusType()))
	{
		auto it2 = _transformationBonuses.find(transformationRule->getSoldierBonusType());
		if (it2 != _transformationBonuses.end())
		{
			it2->second += 1;
		}
		else
		{
			_transformationBonuses[transformationRule->getSoldierBonusType()] = 1;
		}
	}
}

/**
 * Saves info about pending transformation for this soldier.
 * @param transformationRule RuleSoldierTransformation pointer.
 */
void Soldier::postponeTransformation(RuleSoldierTransformation* transformationRule)
{
	clearBaseDuty();
	_training = false;
	_returnToTrainingWhenHealed = false;
	_psiTraining = false;
	_craft = 0;

	int time = transformationRule->getTransformationTime();
	time += RNG::generate((int)(time * -0.2), int(time * 0.2));
	_pendingTransformations[transformationRule->getName()] = time;
}

/**
 * Handles pending transformation - reducing timer, performing transformation once finished.
 * @param transformationRule RuleSoldierTransformation pointer
 */
bool Soldier::handlePendingTransformation()
{
	bool finished = false;
	for (auto it = _pendingTransformations.cbegin(); it != _pendingTransformations.cend();)
	{
		_pendingTransformations.at((*it).first) -= 1;
		if ((*it).second < 1)
		{
			it = _pendingTransformations.erase(it);
			finished = true;
		}
		else
		{
			++it;
		}
	}
	return finished;
}

SoldierRoleRanks *Soldier::findRole(SoldierRole role) const
{
	for (auto *entry : _roles)
	{
		if (entry && entry->role == role)
		{
			return entry;
		}
	}
	return nullptr;
}

void Soldier::clearRoles()
{
	for (auto *entry : _roles)
	{
		delete entry;
	}
	_roles.clear();
}

void Soldier::initializeRole(SoldierRole role, int rank, int experience)
{
	auto *entry = findRole(role);
	if (!entry)
	{
		entry = new SoldierRoleRanks;
		entry->role = role;
		_roles.push_back(entry);
	}
	entry->rank = rank;
	entry->experience = experience;
}

void Soldier::normalizeRoles(bool strict)
{
	bool hasRealEntry = false;
	bool hasRobotEntry = false;
	std::vector<SoldierRole> seen;
	for (auto *entry : _roles)
	{
		if (!entry || entry->role < ROLE_NONE || entry->role > ROLE_ENGINEER)
		{
			throw Exception("Soldier '" + _name + "' contains an invalid role.");
		}
		if (std::find(seen.begin(), seen.end(), entry->role) != seen.end())
		{
			throw Exception("Soldier '" + _name + "' contains a duplicate role.");
		}
		seen.push_back(entry->role);
		if (entry->rank < 0)
		{
			throw Exception("Soldier '" + _name + "' contains a negative role rank.");
		}
		if (entry->role == ROLE_NONE)
		{
			if (entry->rank != 1 || entry->experience != 0)
			{
				throw Exception("Soldier '" + _name + "' has invalid ROLE_NONE rank or experience.");
			}
		}
		else
		{
			hasRealEntry = true;
			if (entry->rank == 0 && entry->experience < 0)
			{
				throw Exception("Soldier '" + _name + "' has negative experience in an unqualified role.");
			}
			if (entry->role == ROLE_ROBOT)
			{
				hasRobotEntry = true;
				if (entry->rank != 1 || entry->experience != 0)
				{
					throw Exception("Soldier '" + _name + "' has an invalid non-sapient robot rank or experience value.");
				}
			}
			else
			{
				const auto &allRequirements = _rules->getRoleExpRequirements();
				const auto requirements = std::find_if(allRequirements.begin(), allRequirements.end(),
					[entry](const SoldierRoleRanksRequirements *rules) { return rules && rules->role == entry->role; });
				if (requirements == allRequirements.end() || (*requirements)->requirements.empty())
				{
					throw Exception("Soldier '" + _name + "' has a role unsupported by its soldier type.");
				}
				const int maximumRank = (*requirements)->requirements.rbegin()->first + 1;
				if (entry->rank > maximumRank)
				{
					throw Exception("Soldier '" + _name + "' has a role rank above the configured maximum.");
				}
			}
		}
	}

	if (hasRobotEntry && _roles.size() != 1)
	{
		throw Exception("Soldier '" + _name + "' cannot combine a non-sapient ROLE_ROBOT with another role.");
	}

	if (hasRealEntry)
	{
		if (strict && std::find(seen.begin(), seen.end(), ROLE_NONE) != seen.end())
		{
			throw Exception("Soldier '" + _name + "' cannot combine ROLE_NONE with another role entry.");
		}
		for (auto it = _roles.begin(); it != _roles.end();)
		{
			if ((*it)->role == ROLE_NONE)
			{
				delete *it;
				it = _roles.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
	else if (_roles.empty())
	{
		if (strict)
		{
			throw Exception("Soldier '" + _name + "' has an empty role list; use ROLE_NONE for an unqualified civilian.");
		}
		initializeRole(ROLE_NONE);
	}
}

bool Soldier::addRole(SoldierRole newRole)
{
	if (newRole <= ROLE_NONE || newRole > ROLE_ENGINEER || newRole == ROLE_ROBOT)
	{
		return false;
	}
	if (hasRole(ROLE_ROBOT) || hasRole(newRole))
	{
		return false;
	}

	const int qualificationCost = _rules->getRequiredExperience(newRole, 1);
	if (qualificationCost < 0)
	{
		throw Exception("Soldier type '" + _rules->getType() + "' has no rank-1 experience requirement for role " +
			std::to_string(static_cast<int>(newRole)) + ".");
	}

	auto *entry = findRole(newRole);
	if (!entry)
	{
		entry = new SoldierRoleRanks;
		entry->role = newRole;
		_roles.push_back(entry);
	}
	entry->rank = 1;
	entry->experience -= qualificationCost;
	normalizeRoles();
	return true;
}

bool Soldier::canGainExperience() const
{
	// FTA robots are non-sapient hardware. Until intelligent robots receive a
	// separate design, they never gain role or stat experience; transformations
	// remain the only supported upgrade path.
	return !hasRole(ROLE_ROBOT);
}

SoldierRole Soldier::getHackingExperienceRole(SoldierRole context) const
{
	if (context == ROLE_AGENT || context == ROLE_ENGINEER)
	{
		return context;
	}
	return hasRole(ROLE_AGENT) && (!hasRole(ROLE_ENGINEER) || getBestRole() == ROLE_AGENT)
		? ROLE_AGENT : ROLE_ENGINEER;
}

bool Soldier::isDeployableCombatUnit() const
{
	return hasRole(ROLE_SOLDIER) || hasRole(ROLE_AGENT) || hasRole(ROLE_ROBOT);
}

void Soldier::addExperience(SoldierRole role, int exp, std::string)
{
	if (exp <= 0 || role <= ROLE_NONE || role > ROLE_ENGINEER || role == ROLE_ROBOT || !canGainExperience())
	{
		return;
	}
	// A personnel type may gain latent experience only in roles for which it
	// defines the rank-1 qualification cost. Unsupported tactical experience is
	// ignored instead of creating an entry that normalizeRoles() cannot validate.
	if (_rules->getRequiredExperience(role, 1) < 0)
	{
		return;
	}

	auto *entry = findRole(role);
	if (!entry)
	{
		entry = new SoldierRoleRanks;
		entry->role = role;
		_roles.push_back(entry);
	}
	entry->experience += exp;
	normalizeRoles();
}

int Soldier::getRoleRank(SoldierRole role) const
{
	const auto *entry = findRole(role);
	return entry ? entry->rank : 0;
}

int Soldier::getRoleRank(const std::vector<int>& roles) const
{
	int rank = 0;
	for (SoldierRoleRanks* i : _roles)
	{
		for (int j : roles)
		{
			if (i->role == SoldierRole(j))
			{
				if (i->rank > rank)
				{
					rank = i->rank;
				}
			}
		}
	}
	return rank;
}

std::pair<SoldierRole, int> Soldier::getBestRoleRank() const
{
	int max = 0;
	SoldierRole role = ROLE_NONE;
	for (auto i : _roles)
	{
		if (i && i->rank > max)
		{
			max = i->rank;
			role = i->role;
		}
	}
	return std::make_pair(role, max);
}

bool Soldier::hasOnlyOneRole(SoldierRole role) const
{
	bool foundGivenRoleWithRank = false;
	for (const auto* r : _roles)
	{
		if (r == nullptr) continue;
		if (r->rank > 0)
		{
			if (r->role == role)
			{
				foundGivenRoleWithRank = true;
			}
			else
			{
				// Another role has rank > 0 -> not the only role
				return false;
			}
		}
	}
	// True only if the passed role had rank > 0 and no other role did
	return foundGivenRoleWithRank;
}

int Soldier::getRoleRankSprite(SoldierRole role) const
{
	const int roleRank = std::max(1, getRoleRank(role));
	int id = 0;
	switch (role)
	{
	case OpenXcom::ROLE_SOLDIER:
		id = this->getRules()->getRankSprite() + roleRank - 1;
		break;
	case OpenXcom::ROLE_PILOT:
		id = this->getRules()->getPilotRankSprite() + roleRank - 1;
		break;
	case OpenXcom::ROLE_AGENT:
		id = this->getRules()->getAgentRankSprite() + roleRank - 1;
		break;
	case OpenXcom::ROLE_SCIENTIST:
		id = this->getRules()->getScientistRankSprite() + roleRank - 1;
		break;
	case OpenXcom::ROLE_ENGINEER:
		id = this->getRules()->getEngineerRankSprite() + roleRank - 1;
		break;
	default:
		id = this->getRules()->getRankSprite() + roleRank - 1;
		break;
	}
	return id;
}

int Soldier::getRoleRankSpriteBattlescape(SoldierRole role) const
{
	const int roleRank = std::max(1, getRoleRank(role));
	int id = 0;
	switch (role)
	{
	case OpenXcom::ROLE_SOLDIER:
		id = this->getRules()->getRankSpriteBattlescape() + roleRank - 1;
		break;
	case OpenXcom::ROLE_PILOT:
		id = this->getRules()->getPilotRankSpriteBattlescape() + roleRank - 1;
		break;
	case OpenXcom::ROLE_AGENT:
		id = this->getRules()->getAgentRankSpriteBattlescape() + roleRank - 1;
		break;
	case OpenXcom::ROLE_SCIENTIST:
		id = this->getRules()->getScientistRankSpriteBattlescape() + roleRank - 1;
		break;
	case OpenXcom::ROLE_ENGINEER:
		id = this->getRules()->getEngineerRankSpriteBattlescape() + roleRank - 1;
		break;
	default:
		id = this->getRules()->getRankSpriteBattlescape() + roleRank - 1;
		break;
	}
	return id;
}

int Soldier::getRoleRankSpriteTiny(SoldierRole role) const
{
	const int roleRank = std::max(1, getRoleRank(role));
	int id = 0;
	switch (role)
	{
	case OpenXcom::ROLE_SOLDIER:
		id = this->getRules()->getRankSpriteTiny() + roleRank - 1;
		break;
	case OpenXcom::ROLE_PILOT:
		id = this->getRules()->getPilotRankSpriteTiny() + roleRank - 1;
		break;
	case OpenXcom::ROLE_AGENT:
		id = this->getRules()->getAgentRankSpriteTiny() + roleRank - 1;
		break;
	case OpenXcom::ROLE_SCIENTIST:
		id = this->getRules()->getScientistRankSpriteTiny() + roleRank - 1;
		break;
	case OpenXcom::ROLE_ENGINEER:
		id = this->getRules()->getEngineerRankSpriteTiny() + roleRank - 1;
		break;
	default:
		id = this->getRules()->getRankSpriteTiny() + roleRank - 1;
		break;
	}
	return id;
}

/**
 * Calculates the stat changes a soldier undergoes from this project
 * @param mod Pointer to the mod
 * @param mode 0 = final, 1 = min, 2 = max
 * @return The stat changes
 */
UnitStats Soldier::calculateStatChanges(const Mod *mod, RuleSoldierTransformation *transformationRule, Soldier *sourceSoldier, int mode, const RuleSoldier *sourceSoldierType)
{
	UnitStats statChange;

	UnitStats initialStats = *sourceSoldier->getInitStats();
	UnitStats currentStats = *sourceSoldier->getCurrentStats();
	UnitStats gainedStats = currentStats - initialStats;

	// Flat stat changes
	statChange += transformationRule->getFlatOverallStatChange();
	UnitStats rnd0;
	if (mode == 2)
		rnd0 = UnitStats::max(transformationRule->getFlatMin(), transformationRule->getFlatMax());
	else if (mode == 1)
		rnd0 = UnitStats::min(transformationRule->getFlatMin(), transformationRule->getFlatMax());
	else
		rnd0 = UnitStats::random(transformationRule->getFlatMin(), transformationRule->getFlatMax());
	statChange += rnd0;

	// Stat changes based on current stats
	statChange += UnitStats::percent(currentStats, transformationRule->getPercentOverallStatChange());
	UnitStats rnd1;
	if (mode == 2)
		rnd1 = UnitStats::max(transformationRule->getPercentMin(), transformationRule->getPercentMax());
	else if (mode == 1)
		rnd1 = UnitStats::min(transformationRule->getPercentMin(), transformationRule->getPercentMax());
	else
		rnd1 = UnitStats::random(transformationRule->getPercentMin(), transformationRule->getPercentMax());
	statChange += UnitStats::percent(currentStats, rnd1);

	// Stat changes based on gained stats
	statChange += UnitStats::percent(gainedStats, transformationRule->getPercentGainedStatChange());
	UnitStats rnd2;
	if (mode == 2)
		rnd2 = UnitStats::max(transformationRule->getPercentGainedMin(), transformationRule->getPercentGainedMax());
	else if (mode == 1)
		rnd2 = UnitStats::min(transformationRule->getPercentGainedMin(), transformationRule->getPercentGainedMax());
	else
		rnd2 = UnitStats::random(transformationRule->getPercentGainedMin(), transformationRule->getPercentGainedMax());
	statChange += UnitStats::percent(gainedStats, rnd2);

	// round (mathematically) to whole tens
	int sign = statChange.bravery < 0 ? -1 : 1;
	statChange.bravery = ((statChange.bravery + (sign * 5)) / 10) * 10;

	const RuleSoldier *transformationSoldierType = _rules;
	if (!Mod::isEmptyRuleName(transformationRule->getProducedSoldierType()))
	{
		transformationSoldierType = mod->getSoldier(transformationRule->getProducedSoldierType());
	}

	if (transformationRule->hasLowerBoundAtMinStats())
	{
		UnitStats lowerBound = transformationSoldierType->getMinStats();
		UnitStats cappedChange = lowerBound - currentStats;

		statChange = UnitStats::max(statChange, cappedChange);
	}

	if (transformationRule->hasUpperBoundAtMaxStats() || transformationRule->hasUpperBoundAtStatCaps())
	{
		UnitStats upperBound = transformationRule->hasUpperBoundAtMaxStats()
			? transformationSoldierType->getMaxStats()
			: transformationSoldierType->getStatCaps();
		UnitStats cappedChange = upperBound - currentStats;

		bool isSameSoldierType = (transformationSoldierType == sourceSoldierType);
		bool softLimit = transformationRule->isSoftLimit(isSameSoldierType);
		if (softLimit)
		{
			// soft limit
			statChange = UnitStats::softLimit(statChange, currentStats, upperBound);
		}
		else
		{
			// hard limit
			statChange = UnitStats::min(statChange, cappedChange);
		}
	}

	return statChange;
}

/**
 * Checks whether the soldier has a given bonus.
 * Disclaimer: DOES NOT REFRESH THE BONUS CACHE!
 */
bool Soldier::hasBonus(const RuleSoldierBonus* bonus) const
{
	for (auto* sb : _bonusCache)
	{
		if (sb == bonus)
		{
			return true;
		}
	}
	return false;
}

/**
 * Gets all the soldier bonuses
 * @return The map of soldier bonuses
 */
const std::vector<const RuleSoldierBonus*> *Soldier::getBonuses(const Mod *mod)
{
	if (mod)
	{
		_bonusCache.clear();
		auto addSorted = [&](const RuleSoldierBonus* b)
		{
			if (!b)
			{
				return;
			}

			auto sort = [](const RuleSoldierBonus* l, const RuleSoldierBonus* r){ return l->getListOrder() < r->getListOrder(); };

			auto p = std::lower_bound(_bonusCache.begin(), _bonusCache.end(), b, sort);
			if (p == _bonusCache.end() || *p != b)
			{
				_bonusCache.insert(p, b);
			}
		};

		for (const auto& bonusName : _transformationBonuses)
		{
			auto* bonusRule = mod->getSoldierBonus(bonusName.first, false);

			addSorted(bonusRule);
		}
		for (auto* commendation : *_diary->getSoldierCommendations())
		{
			auto* bonusRule = commendation->getRule()->getSoldierBonus(commendation->getDecorationLevelInt());

			addSorted(bonusRule);
		}
	}

	return &_bonusCache;
}

/**
 * Get pointer to current stats with soldier bonuses, but without armor bonuses.
 */
const UnitStats *Soldier::getStatsWithSoldierBonusesOnly() const
{
	return &_tmpStatsWithSoldierBonuses;
}

/**
 * Get pointer to current stats with armor and soldier bonuses.
 */
const UnitStats *Soldier::getStatsWithAllBonuses() const
{
	return &_tmpStatsWithAllBonuses;
}

/**
 * Pre-calculates soldier stats with various bonuses.
 */
bool Soldier::prepareStatsWithBonuses(const Mod *mod)
{
	bool hasSoldierBonus = false;

	// 1. current stats
	UnitStats tmp = _currentStats;
	auto basePsiSkill = _currentStats.psiSkill;

	// 2. refresh soldier bonuses
	auto* bonuses = getBonuses(mod); // this is the only place where bonus cache is rebuilt

	// 3. apply soldier bonuses
	for (const auto* bonusRule : *bonuses)
	{
		hasSoldierBonus = true;
		tmp += *(bonusRule->getStats());
	}

	// 4. stats with soldier bonuses, but without armor bonuses
	_tmpStatsWithSoldierBonuses = UnitStats::obeyFixedMinimum(tmp);

	// if the psi skill has not been "unlocked" yet by training, do not allow soldier bonuses to unlock it
	if (basePsiSkill <= 0 && _tmpStatsWithSoldierBonuses.psiSkill > 0)
	{
		_tmpStatsWithSoldierBonuses.psiSkill = basePsiSkill;
	}

	// 5. apply armor bonus
	tmp += *_armor->getStats();

	// 6. stats with all bonuses
	_tmpStatsWithAllBonuses = UnitStats::obeyFixedMinimum(tmp);

	// 7. pilot armors count as soldier bonuses
	if (_armor->isPilotArmor())
	{
		_tmpStatsWithSoldierBonuses = _tmpStatsWithAllBonuses;
	}

	return hasSoldierBonus;
}

UnitStats* Soldier::getMonthlyExperienceCache()
{
	return &_monthlyExperienceCache;
}

void Soldier::resetMonthlyExperienceCache()
{
	_monthlyExperienceCache = UnitStats::scalar(0);
}

void Soldier::improvePrimaryStats(UnitStats* exp, SoldierRole role)
{
	if (!canGainExperience())
	{
		// Some subsystems accumulate experience directly in their deferred
		// cache. Consume it defensively even if a robot reached one of them.
		*exp = UnitStats::scalar(0);
		return;
	}
	UnitStats *stats = getCurrentStatsEditable();
	const UnitStats caps = getRules()->getStatCaps();
	UnitStats origStats = *getCurrentStats();
	int rate = 0;
	// soldier primary stats
	{
		if (exp->bravery && stats->bravery < caps.bravery)
		{
			stats->bravery += improveStat(exp->bravery, rate, true);
			if (role == ROLE_SOLDIER || role == ROLE_AGENT || role == ROLE_PILOT)
				addExperience(role, rate, "bravery stat improvement");
			else
				addExperience(ROLE_SOLDIER, rate, "bravery stat improvement");
		}
		if (exp->reactions && stats->reactions < caps.reactions)
		{
			stats->reactions += improveStat(exp->reactions, rate);
			if (role == ROLE_SOLDIER || role == ROLE_AGENT)
				addExperience(role, rate, "reactions stat improvement");
			else
				addExperience(ROLE_SOLDIER, rate, "reactions stat improvement");
		}
		if (exp->firing && stats->firing < caps.firing)
		{
			stats->firing += improveStat(exp->firing, rate);
			if (role == ROLE_SOLDIER || role == ROLE_AGENT)
				addExperience(role, rate, "firing stat improvement");
			else
				addExperience(ROLE_SOLDIER, rate, "firing stat improvement");
		}
		if (exp->melee && stats->melee < caps.melee)
		{
			stats->melee += improveStat(exp->melee, rate);
			if (role == ROLE_SOLDIER || role == ROLE_AGENT)
				addExperience(role, rate, "melee stat improvement");
			else
				addExperience(ROLE_SOLDIER, rate, "melee stat improvement");
		}
		if (exp->throwing && stats->throwing < caps.throwing)
		{
			stats->throwing += improveStat(exp->throwing, rate);
			if (role == ROLE_SOLDIER || role == ROLE_AGENT)
				addExperience(role, rate, "throwing stat improvement");
			else
				addExperience(ROLE_SOLDIER, rate, "throwing stat improvement");
		}
		if (exp->psiSkill && stats->psiSkill < caps.psiSkill)
		{
			stats->psiSkill += improveStat(exp->psiSkill, rate);
			if (role == ROLE_SOLDIER || role == ROLE_AGENT)
				addExperience(role, rate, "psiSkill stat improvement");
			else
				addExperience(ROLE_SOLDIER, rate, "psiSkill stat improvement");
		}
		if (exp->psiStrength && stats->psiStrength < caps.psiStrength)
		{
			stats->psiStrength += improveStat(exp->psiStrength, rate);
			if (role == ROLE_SOLDIER || role == ROLE_AGENT)
				addExperience(role, rate, "psiStrength stat improvement");
			else
				addExperience(ROLE_SOLDIER, rate, "psiStrength stat improvement");
		}
		if (exp->mana && stats->mana < caps.mana)
		{
			stats->mana += improveStat(exp->mana, rate);
			if (role == ROLE_SOLDIER || role == ROLE_AGENT)
				addExperience(role, rate, "mana stat improvement");
			else
				addExperience(ROLE_SOLDIER, rate, "mana stat improvement");
		}
	}

	//pilot stats
	{
		if (exp->maneuvering && stats->maneuvering < caps.maneuvering)
		{
			stats->maneuvering += improveStat(exp->maneuvering, rate);
			addExperience(ROLE_PILOT, rate, "maneuvering stat improvement");
		}
		if (exp->missiles && stats->missiles < caps.missiles)
		{
			stats->missiles += improveStat(exp->missiles, rate);
			addExperience(ROLE_PILOT, rate, "missiles stat improvement");
		}
		if (exp->dogfight && stats->dogfight < caps.dogfight)
		{
			stats->dogfight += improveStat(exp->dogfight, rate);
			addExperience(ROLE_PILOT, rate, "dogfight stat improvement");
		}
		if (exp->tracking && stats->tracking < caps.tracking)
		{
			stats->tracking += improveStat(exp->tracking, rate);
			int reducedRate = RNG::generate(0, rate); // non-combat skill
			addExperience(ROLE_PILOT, reducedRate, "tracking stat improvement");
		}
		if (exp->cooperation && stats->cooperation < caps.cooperation)
		{
			stats->cooperation += improveStat(exp->cooperation, rate);
			addExperience(ROLE_PILOT, rate, "cooperation stat improvement");
		}
		if (exp->beams && stats->beams < caps.beams)
		{
			stats->beams += improveStat(exp->beams, rate);
			addExperience(ROLE_PILOT, rate, "beams stat improvement");
		}
		if (exp->synaptic && stats->synaptic < caps.synaptic)
		{
			stats->synaptic += improveStat(exp->synaptic, rate);
			addExperience(ROLE_PILOT, rate, "synaptic stat improvement");
		}
		if (exp->gravity && stats->gravity < caps.gravity)
		{
			stats->gravity += improveStat(exp->gravity, rate);
			addExperience(ROLE_PILOT, rate, "gravity stat improvement");
		}
	}

	//science stats
	{
		if (exp->physics && stats->physics < caps.physics)
		{
			stats->physics += improveStat(exp->physics, rate);
			addExperience(ROLE_SCIENTIST, rate, "physics stat improvement");
		}
		if (exp->chemistry && stats->chemistry < caps.chemistry)
		{
			stats->chemistry += improveStat(exp->chemistry, rate);
			addExperience(ROLE_SCIENTIST, rate, "chemistry stat improvement");
		}
		if (exp->biology && stats->biology < caps.biology)
		{
			stats->biology += improveStat(exp->biology, rate);
			addExperience(ROLE_SCIENTIST, rate, "biology stat improvement");
		}
		if (exp->insight && stats->insight < caps.insight)
		{
			stats->insight += improveStat(exp->insight, rate);
			addExperience(ROLE_SCIENTIST, rate, "insight stat improvement");
		}
		if (exp->data && stats->data < caps.data)
		{
			stats->data += improveStat(exp->data, rate);
			SoldierRole experienceRole = role;
			if (experienceRole != ROLE_SCIENTIST && experienceRole != ROLE_AGENT)
			{
				experienceRole = hasRole(ROLE_AGENT) && (!hasRole(ROLE_SCIENTIST) || getBestRole() == ROLE_AGENT)
					? ROLE_AGENT : ROLE_SCIENTIST;
			}
			addExperience(experienceRole, rate, "data stat improvement");
		}
		if (exp->computers && stats->computers < caps.computers)
		{
			stats->computers += improveStat(exp->computers, rate);
			addExperience(role == ROLE_AGENT ? ROLE_AGENT : ROLE_SCIENTIST, rate, "computers stat improvement");
		}
		if (exp->tactics && stats->tactics < caps.tactics)
		{
			stats->tactics += improveStat(exp->tactics, rate);
			addExperience(ROLE_SCIENTIST, rate, "tactics stat improvement");
		}
		if (exp->materials && stats->materials < caps.materials)
		{
			stats->materials += improveStat(exp->materials, rate);
			addExperience(ROLE_SCIENTIST, rate, "materials stat improvement");
		}
		if (exp->designing && stats->designing < caps.designing)
		{
			stats->designing += improveStat(exp->designing, rate);
			addExperience(ROLE_SCIENTIST, rate, "designing stat improvement");
		}
		if (exp->psionics && stats->psionics < caps.psionics)
		{
			stats->psionics += improveStat(exp->psionics, rate);
			addExperience(ROLE_SCIENTIST, rate, "psionics stat improvement");
		}
		if (exp->xenolinguistics && stats->xenolinguistics < caps.xenolinguistics)
		{
			stats->xenolinguistics += improveStat(exp->xenolinguistics, rate);
			addExperience(role == ROLE_AGENT ? ROLE_AGENT : ROLE_SCIENTIST, rate, "xenolinguistics stat improvement");
		}
		if (exp->alienTech && stats->alienTech < caps.alienTech && (role == ROLE_SCIENTIST || role == ROLE_AGENT))
		{
			stats->alienTech += improveStat(exp->alienTech, rate);
			addExperience(role, rate, "alienTech stat improvement");
		}
	}

	//engineer stats
	{
		if (exp->weaponry && stats->weaponry < caps.weaponry)
		{
			stats->weaponry += improveStat(exp->weaponry, rate);
			addExperience(ROLE_ENGINEER, rate, "weaponry stat improvement");
		}
		if (exp->explosives && stats->explosives < caps.explosives)
		{
			stats->explosives += improveStat(exp->explosives, rate);
			addExperience(ROLE_ENGINEER, rate, "explosives stat improvement");
		}
		if (exp->efficiency && stats->efficiency < caps.efficiency)
		{
			stats->efficiency += improveStat(exp->efficiency, rate);
			addExperience(ROLE_ENGINEER, rate, "efficiency stat improvement");
		}
		if (exp->microelectronics && stats->microelectronics < caps.microelectronics)
		{
			stats->microelectronics += improveStat(exp->microelectronics, rate);
			addExperience(ROLE_ENGINEER, rate, "microelectronics stat improvement");
		}
		if (exp->metallurgy && stats->metallurgy < caps.metallurgy)
		{
			stats->metallurgy += improveStat(exp->metallurgy, rate);
			addExperience(ROLE_ENGINEER, rate, "metallurgy stat improvement");
		}
		if (exp->processing && stats->processing < caps.processing)
		{
			stats->processing += improveStat(exp->processing, rate);
			addExperience(role == ROLE_SCIENTIST ? ROLE_SCIENTIST : ROLE_ENGINEER,
				rate, "processing stat improvement");
		}
		if (exp->hacking && stats->hacking < caps.hacking)
		{
			stats->hacking += improveStat(exp->hacking, rate);
			addExperience(getHackingExperienceRole(role), rate, "hacking stat improvement");
		}
		if (exp->robotics && stats->robotics < caps.robotics)
		{
			stats->robotics += improveStat(exp->robotics, rate);
			addExperience(ROLE_ENGINEER, rate, "robotics stat improvement");
		}
		if (exp->diligence && stats->diligence < caps.diligence)
		{
			stats->diligence += improveStat(exp->diligence, rate);
			addExperience(ROLE_ENGINEER, rate, "diligence stat improvement");
		}
		if (exp->alienTech && stats->alienTech < caps.alienTech && role != ROLE_SCIENTIST && role != ROLE_AGENT)
		{
			stats->alienTech += improveStat(exp->alienTech, rate);
			addExperience(ROLE_ENGINEER, rate, "alienTech stat improvement");
		}
		if (exp->reverseEngineering && stats->reverseEngineering < caps.reverseEngineering)
		{
			stats->reverseEngineering += improveStat(exp->reverseEngineering, rate);
			addExperience(ROLE_ENGINEER, rate, "reverseEngineering stat improvement");
		}
	}

	//agent stats
	{
		if (exp->stealth && stats->stealth < caps.stealth)
		{
			stats->stealth += improveStat(exp->stealth, rate);
			addExperience(ROLE_AGENT, rate, "stealth stat improvement");
		}
		if (exp->perception && stats->perception < caps.perception)
		{
			stats->perception += improveStat(exp->perception, rate);
			addExperience(ROLE_AGENT, rate, "perception stat improvement");
		}
		if (exp->charisma && stats->charisma < caps.charisma)
		{
			stats->charisma += improveStat(exp->charisma, rate);
			addExperience(ROLE_AGENT, rate, "charisma stat improvement");
		}
		if (exp->investigation && stats->investigation < caps.investigation)
		{
			stats->investigation += improveStat(exp->investigation, rate);
			addExperience(ROLE_AGENT, rate, "investigation stat improvement");
		}
		if (exp->deception && stats->deception < caps.deception)
		{
			stats->deception += improveStat(exp->deception, rate);
			addExperience(ROLE_AGENT, rate, "deception stat improvement");
		}
		if (exp->interrogation && stats->interrogation < caps.interrogation)
		{
			stats->interrogation += improveStat(exp->interrogation, rate);
			addExperience(ROLE_AGENT, rate, "interrogation stat improvement");
		}
	}

	_monthlyExperienceCache += *getCurrentStats() - origStats;
}

bool Soldier::rolePromoteSoldier(SoldierRole promotionRole, RolePromotion* promotion)
{
	if (!canGainExperience() || promotionRole <= ROLE_NONE || promotionRole == ROLE_ROBOT)
	{
		return false;
	}

	auto *role = findRole(promotionRole);
	if (!role || role->rank < 1)
	{
		return false;
	}

	for (const auto *roleReq : getRules()->getRoleExpRequirements())
	{
		if (!roleReq || roleReq->role != promotionRole || roleReq->requirements.empty())
		{
			continue;
		}
		const auto requirement = roleReq->requirements.find(role->rank);
		if (requirement == roleReq->requirements.end() || role->experience < requirement->second)
		{
			return false;
		}
		const int previousRank = role->rank;
		role->rank++;
		role->experience -= requirement->second;
		if (promotion)
		{
			promotion->soldierId = _id;
			promotion->role = promotionRole;
			promotion->previousRank = previousRank;
			promotion->newRank = role->rank;
		}
		return true;
	}
	return false;
}

/**
 * Check if the soldier has all the required soldier bonuses for the given soldier skill.
 * @param skillRules Skill rules.
 */
bool Soldier::hasAllRequiredBonusesForSkill(const RuleSkill* skillRules)
{
	for (auto* requiredBonusRule : skillRules->getRequiredBonuses())
	{
		bool found = false;
		for (auto* bonusRule : *getBonuses(nullptr))
		{
			if (bonusRule == requiredBonusRule)
			{
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	return true;
}

/**
 * Check if the soldier has an active pilot role and meets all requirements for piloting the (current or new) craft.
 */
bool Soldier::hasAllPilotingRequirements(const Craft* newCraft) const
{
	if (!_rules->getAllowPiloting() || getRoleRank(ROLE_PILOT) <= 0)
		return false;

	const Craft* craft = newCraft ? newCraft : _craft;

	if (!craft)
		return false;

	// Does this soldier meet the minimum stat requirements for piloting the current craft?
	const UnitStats& currentStats = _tmpStatsWithAllBonuses; // all bonuses count
	const UnitStats& minStats = craft->getRules()->getPilotMinStatsRequired();
	if (currentStats.tu < minStats.tu ||
		currentStats.stamina < minStats.stamina ||
		currentStats.health < minStats.health ||
		currentStats.bravery < minStats.bravery ||
		currentStats.reactions < minStats.reactions ||
		currentStats.firing < minStats.firing ||
		currentStats.throwing < minStats.throwing ||
		currentStats.melee < minStats.melee ||
		currentStats.mana < minStats.mana ||
		currentStats.strength < minStats.strength ||
		currentStats.psiStrength < minStats.psiStrength ||
		(currentStats.psiSkill < minStats.psiSkill && minStats.psiSkill != 0) || // Initial psi training sets skill to a negative value.
		currentStats.maneuvering < minStats.maneuvering ||
		currentStats.missiles < minStats.missiles ||
		currentStats.dogfight < minStats.dogfight ||
		currentStats.tracking < minStats.tracking ||
		currentStats.cooperation < minStats.cooperation ||
		currentStats.beams < minStats.beams ||
		currentStats.synaptic < minStats.synaptic ||
		currentStats.gravity < minStats.gravity ||
		currentStats.physics < minStats.physics ||
		currentStats.chemistry < minStats.chemistry ||
		currentStats.biology < minStats.biology ||
		currentStats.insight < minStats.insight ||
		currentStats.data < minStats.data ||
		currentStats.computers < minStats.computers ||
		currentStats.tactics < minStats.tactics ||
		currentStats.materials < minStats.materials ||
		currentStats.designing < minStats.designing ||
		currentStats.psionics < minStats.psionics ||
		currentStats.xenolinguistics < minStats.xenolinguistics ||
		currentStats.weaponry < minStats.weaponry ||
		currentStats.explosives < minStats.explosives ||
		currentStats.efficiency < minStats.efficiency ||
		currentStats.microelectronics < minStats.microelectronics ||
		currentStats.metallurgy < minStats.metallurgy ||
		currentStats.processing < minStats.processing ||
		currentStats.hacking < minStats.hacking ||
		currentStats.robotics < minStats.robotics ||
		currentStats.diligence < minStats.diligence ||
		currentStats.alienTech < minStats.alienTech ||
		currentStats.reverseEngineering < minStats.reverseEngineering ||
		currentStats.stealth < minStats.stealth ||
		currentStats.perception < minStats.perception ||
		currentStats.charisma < minStats.charisma ||
		currentStats.investigation < minStats.investigation ||
		currentStats.deception < minStats.deception ||
		currentStats.interrogation < minStats.interrogation)
	{
		return false;
	}

	// Does this soldier have all required soldier bonuses for piloting the current craft?
	for (auto* requiredBonusRule : craft->getRules()->getPilotSoldierBonusesRequired())
	{
		bool found = false;
		for (auto* bonusRule : _bonusCache) // *getBonuses(nullptr)
		{
			if (bonusRule == requiredBonusRule)
			{
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}

	return true;
}


////////////////////////////////////////////////////////////
//					Script binding
////////////////////////////////////////////////////////////

namespace
{

void getGenderScript(const Soldier *so, int &ret)
{
	if (so)
	{
		ret = so->getGender();
		return;
	}
	ret = 0;
}
void getRoleRankScript(const Soldier *so, int &ret, int role)
{
	if (so && role >= ROLE_NONE && role <= ROLE_ENGINEER)
	{
		ret = so->getRoleRank(static_cast<SoldierRole>(role));
		return;
	}
	ret = 0;
}
void getBestRoleScript(const Soldier *so, int &ret)
{
	ret = so ? static_cast<int>(so->getBestRole()) : static_cast<int>(ROLE_NONE);
}
void getLookScript(const Soldier *so, int &ret)
{
	if (so)
	{
		ret = so->getLook();
		return;
	}
	ret = 0;
}
void getLookVariantScript(const Soldier *so, int &ret)
{
	if (so)
	{
		ret = so->getLookVariant();
		return;
	}
	ret = 0;
}
struct getRuleSoldierScript
{
	static RetEnum func(const Soldier *so, const RuleSoldier* &ret)
	{
		if (so)
		{
			ret = so->getRules();
		}
		else
		{
			ret = nullptr;
		}
		return RetContinue;
	}
};

std::string debugDisplayScript(const Soldier* so)
{
	if (so)
	{
		std::string s;
		s += Soldier::ScriptName;
		s += "(type: \"";
		s += so->getRules()->getType();
		s += "\" id: ";
		s += std::to_string(so->getId());
		s += " name: \"";
		s += so->getName(false, 0);
		s += "\")";
		return s;
	}
	else
	{
		return "null";
	}
}

} // namespace

/**
 * Register Soldier in script parser.
 * @param parser Script parser.
 */
void Soldier::ScriptRegister(ScriptParserBase* parser)
{
	parser->registerPointerType<RuleSoldier>();

	Bind<Soldier> so = { parser };


	so.addField<&Soldier::_id>("getId");
	so.add<&getRoleRankScript>("getRoleRank", "first argument returns the rank, second argument is a SoldierRole value");
	so.add<&getBestRoleScript>("getBestRole");
	so.add<&getGenderScript>("getGender");
	so.add<&getLookScript>("getLook");
	so.add<&getLookVariantScript>("getLookVariant");


	UnitStats::addGetStatsScript<&Soldier::_currentStats>(so, "Stats.");
	UnitStats::addSetStatsScript<&Soldier::_currentStats>(so, "Stats.");


	so.addFunc<getRuleSoldierScript>("getRuleSoldier");
	so.add<&Soldier::getWoundRecoveryInt>("getWoundRecovery");
	so.add<&Soldier::setWoundRecovery>("setWoundRecovery");
	so.add<&Soldier::getManaMissing>("getManaMissing");
	so.add<&Soldier::setManaMissing>("setManaMissing");
	so.add<&Soldier::getHealthMissing>("getHealthMissing");
	so.add<&Soldier::setHealthMissing>("setHealthMissing");


	so.addScriptValue<BindBase::OnlyGet, &Soldier::_rules, &RuleSoldier::getScriptValuesRaw>();
	so.addScriptValue<&Soldier::_scriptValues>();
	so.addDebugDisplay<&debugDisplayScript>();
	so.addCustomConst("ROLE_NONE", SoldierRole::ROLE_NONE);
	so.addCustomConst("ROLE_SOLDIER", SoldierRole::ROLE_SOLDIER);
	so.addCustomConst("ROLE_ROBOT", SoldierRole::ROLE_ROBOT);
	so.addCustomConst("ROLE_PILOT", SoldierRole::ROLE_PILOT);
	so.addCustomConst("ROLE_AGENT", SoldierRole::ROLE_AGENT);
	so.addCustomConst("ROLE_SCIENTIST", SoldierRole::ROLE_SCIENTIST);
	so.addCustomConst("ROLE_ENGINEER", SoldierRole::ROLE_ENGINEER);
}

} // namespace OpenXcom
