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
#include <vector>
#include <string>
#include "Soldier.h"
#include "BattleUnit.h"

namespace OpenXcom
{
class Game;
class Mod;
class RulePrisoner;
class RuleResearch;
class Armor;
class Soldier;
class Base;
class BattleUnit;
class Language;
class SavedGame;
enum UnitFaction : int;
enum PrisonerState: int {
	PRISONER_STATE_NONE = 0,
	PRISONER_STATE_CONTAINING = 1,
	PRISONER_STATE_INTERROGATION = 2,
	PRISONER_STATE_TORTURE = 3,
	PRISONER_STATE_REQRUITING = 4 };

enum PrisonerProcessEvent : int
{
	PRISONER_PROCESS_NONE = 0,
	PRISONER_PROCESS_DEATH,
	PRISONER_PROCESS_INTERROGATION,
	PRISONER_PROCESS_RECRUITING
};

struct PrisonerProcessResult
{
	PrisonerProcessEvent event = PRISONER_PROCESS_NONE;
	const RuleResearch* research = nullptr;
	const RuleResearch* bonus = nullptr;
	Soldier* recruitedSoldier = nullptr;
	std::vector<const RuleResearch*> newlyDiscovered;
	std::vector<std::string> researchNotifications;
	PromotionBatch promotions;
};

class BasePrisoner
{
private:
	const RulePrisoner* _rule;
	int _id;
	std::string _designation, _type;
	PrisonerState _state;
	std::vector<SoldierRole> _roles;
	UnitFaction _faction = FACTION_HOSTILE;
	std::string _name;
	UnitStats _stats;
	int _health, _intelligence, _aggression, _morale, _cooperation;
	int _interrogationProgress, _recruitingProgress;
	bool _spawnedTortureEvent = false;
	bool _interrogationDone = false;
	Armor *_armor = nullptr;
	std::vector<Soldier*> _agents;
	Base* _base;

	void loadRoles(const std::vector<int>& r);
	/// Applies and clears accumulated agent stat experience; optionally checks role promotion.
	PromotionBatch finalizeAgentExperience(bool checkPromotions);
public:
	/// Creates a BasePrisoner.
	BasePrisoner(const RulePrisoner* rule, Base* base, const std::string& type,
		int id, std::string designation);
	/// Loads the unit from YAML.
	void load(const YAML::YamlNodeReader& reader, const Mod *mod);
	/// Saves the unit to YAML.
	void save(YAML::YamlNodeWriter writer) const;
	/// Geoscape logic
	PrisonerProcessResult think(Game &engine);
	/// Applies the signed daily containment upkeep or income.
	void applyDailyFunds(SavedGame& save) const;
	void die();

	//getters and setters
	int getId() const { return _id; }
	const std::string& getDesignation() const { return _designation; }
	void setName(const std::string& name) { _name = name; }
	std::string getName() const { return _name; }
	std::string getNameAndDesignation(Language* lang) const;

	void setPrisonerState(PrisonerState state) { _state = state; }
	PrisonerState getPrisonerState() const { return _state; }

	const RulePrisoner* getRules() const { return _rule; }

	void setBase(Base* base) { _base = base; }

	void setArmor(Armor* armor) { _armor = armor; }
	const Armor* getArmor() const { return _armor; }

	void setRoles(const std::vector<SoldierRole>& roles);
	const std::vector<SoldierRole>& getRoles() const { return _roles; }

	void setFaction(UnitFaction faction) { _faction = faction; }
	UnitFaction getFaction() const { return _faction; }

	void setStats(const UnitStats* stats) { _stats = *stats; }
	UnitStats* getStats() { return &_stats; }

	void setHealth(int health) { _health = health; }
	int getHealth() const { return _health; }

	void setIntelligence(int intel) { _intelligence = intel; }
	int getIntelligence() const { return _intelligence; }

	void setAggression(int aggro) { _aggression = aggro; }
	int getAggression() const { return _aggression; }
	
	void setMorale(int morale);
	int getMorale() const { return _morale; }

	void setCooperation(int cooperation);
	int getCooperation() const { return _cooperation; }

	bool isInterrogationDone() const { return _interrogationDone; }
	

};

}
