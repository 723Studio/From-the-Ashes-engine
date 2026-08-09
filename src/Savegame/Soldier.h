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
#include <string>
#include "../Engine/Yaml.h"
#include "../Mod/Unit.h"
#include "../Mod/StatString.h"
#include "../Mod/RuleSoldier.h"
#include "../Engine/Script.h"
#include "Promotion.h"

namespace OpenXcom
{

enum SoldierGender : char { GENDER_MALE, GENDER_FEMALE };
enum SoldierLook : char { LOOK_BLONDE, LOOK_BROWNHAIR, LOOK_ORIENTAL, LOOK_AFRICAN };
enum ReturnToTrainings : char {NONE, MARTIAL_TRAINING, PSI_TRAINING, BOTH_TRAININGS};
enum DutyMode: char { CRAFT, LAB, WORK, INTEL, ASSIGN, INFO};

class Craft;
class CovertOperation;
class ResearchProject;
class Production;
class IntelProject;
class BasePrisoner;
class BaseContainedAlien;
class BaseContainedMonster;
class SoldierNamePool;
class Mod;
class RuleSoldier;
class BattleUnit;
class Armor;
class Language;
class EquipmentLayoutItem;
class SoldierDeath;
class SoldierDiary;
class SavedGame;
class RuleSoldierTransformation;
class RuleSoldierBonus;
class RuleSkill;
class Base;
struct BaseSumDailyRecovery;

struct SoldierRoleRanks
{
	SoldierRole role = ROLE_NONE;
	int rank = 0;
	int experience = 0;

	/// Loads stats from YAML.
	void load(const YAML::YamlNodeReader& reader)
	{
		reader.tryRead("role", role);
		reader.tryRead("rank", rank);
		reader.tryRead("experience", experience);
	}
	/// Saves stats to YAML.
	void save(YAML::YamlNodeWriter writer) const
	{
		writer.setAsMap();
		writer.write("role", (int)role);
		writer.write("rank", rank);
		writer.write("experience", experience);
	}
};

/**
 * Represents a soldier hired by the player.
 * Soldiers have a wide variety of stats that affect
 * their performance during battles.
 */
class Soldier
{
public:

	/// Name of class used in script.
	static constexpr const char *ScriptName = "GeoscapeSoldier";
	/// Register all useful function used by script.
	static void ScriptRegister(ScriptParserBase* parser);

private:
	std::string _name;
	std::string _callsign;
	int _id, _nationality, _improvement;
	std::vector<SoldierRoleRanks*> _roles;
	const RuleSoldier *_rules;
	UnitStats _initialStats, _currentStats, _tmpStatsWithSoldierBonuses, _tmpStatsWithAllBonuses;
	UnitStats _monthlyExperienceCache;
	UnitStats _dogfightExperience, _researchExperience, _engineerExperience, _intelExperience;
	Craft *_craft;
	CovertOperation *_covertOperation;
	ResearchProject *_researchProject;
	Production *_production;
	IntelProject* _intelProject;
	BasePrisoner* _prisoner;
	BaseContainedAlien* _containedAlien;
	BaseContainedMonster* _containedMonster;
	SoldierGender _gender;
	SoldierLook _look;
	int _lookVariant;
	int _missions, _kills, _stuns;
	int _healthMissing = 0; // amount of health missing until full health recovery, this is less serious than wound recovery.
	int _manaMissing = 0;   // amount of mana missing until full mana recovery
	float _recovery = 0.0;  // amount of hospital attention soldier needs... used to calculate recovery time
	bool _psiTraining, _training, _returnToTrainingWhenHealed, _justSaved;
	ReturnToTrainings _returnToTrainingsWhenOperationOver;
	Armor *_armor;
	Armor *_replacedArmor;
	Armor *_transformedArmor;
	std::vector<EquipmentLayoutItem*> _equipmentLayout;           // last used equipment layout, managed by the game
	SoldierDeath *_death;
	SoldierDiary *_diary;
	std::string _statString;
	bool _corpseRecovered;
	bool _isRookieSoldier, _isRookieAgent, _isRookiePilot;
	std::map<std::string, int> _previousTransformations, _transformationBonuses, _pendingTransformations;
	std::vector<const RuleSoldierBonus*> _bonusCache;
	ScriptValues<Soldier> _scriptValues;

	int generateNormalStat(int min, int max);
	int generateScienceStat(int min, int max);
	int selectNamePoolIndex() const;
	SoldierRoleRanks *findRole(SoldierRole role) const;
	void initializeRole(SoldierRole role, int rank = 1, int experience = 0);
	void clearRoles();
	void normalizeRoles(bool strict = false);

  public:
	/// Creates a new soldier.
	Soldier(const RuleSoldier *rules, Armor *armor, int id = 0, int nationality = -1);
	/// Cleans up the soldier.
	~Soldier();
	/// Loads the soldier from YAML.
	void load(const YAML::YamlNodeReader& reader, const Mod *mod, SavedGame *save, const ScriptGlobal *shared, bool soldierTemplate = false);
	/// Saves the soldier to YAML.
	void save(YAML::YamlNodeWriter writer, const ScriptGlobal *shared) const;
	/// Gets the soldier's name.
	std::string getName(bool statstring = false, unsigned int maxLength = 20) const;
	/// Sets the soldier's name.
	void setName(const std::string &name);
	/// Generates a new name based on nationality.
	void genName();
	/// Gets the soldier's callsign.
	std::string getCallsign(unsigned int maxLength = 20) const;
	/// Sets the soldier's callsign.
	void setCallsign(const std::string &callsign);
	/// Check for callsign assignment.
	bool hasCallsign() const;
	/// Gets the soldier's nationality.
	int getNationality() const;
	/// Sets the soldier's nationality.
	void setNationality(int nationality);
	/// Gets the soldier's craft.
	Craft *getCraft() const;
	/// Sets the soldier's craft.
	void setCraft(Craft *craft, bool resetCustomDeployment = false);
	/// Gets the soldier's Covert Operation.
	CovertOperation* getCovertOperation() const { return _covertOperation; }
	/// Sets the soldier's Covert Operation.
	void setCovertOperation(CovertOperation* covertOperation) { _covertOperation = covertOperation; }
	/// Gets the soldier's Intel Project.
	IntelProject* getIntelProject() const { return _intelProject; }
	/// Sets the soldier's Intel Project.
	void setIntelProject(IntelProject* project) { _intelProject = project; }
	/// Gets the soldier's Research Project.
	ResearchProject *getResearchProject() const { return _researchProject; }
	/// Sets the soldier's Research Project.
	void setResearchProject(ResearchProject *researchProject) { _researchProject = researchProject; }
	/// Gets the soldier's Production Project.
	Production* getProductionProject() const { return _production; }
	/// Sets the soldier's Production Project.
	void setProductionProject(Production* production) { _production = production; }
	/// Sets the soldier's BasePrisoner.
	void setActivePrisoner(BasePrisoner* prisoner) { _prisoner = prisoner; }
	/// Gets the soldier's BasePrisoner.
	BasePrisoner* getActivePrisoner() const { return _prisoner; }
	void setActiveContainedAlien(BaseContainedAlien* alien) { _containedAlien = alien; }
	BaseContainedAlien* getActiveContainedAlien() const { return _containedAlien; }
	void setActiveContainedMonster(BaseContainedMonster* monster) { _containedMonster = monster; }
	BaseContainedMonster* getActiveContainedMonster() const { return _containedMonster; }
	/// Gets the soldier's craft string.
	std::string getCurrentDuty(Language *lang, const BaseSumDailyRecovery &recovery, bool &isBusy, bool &isFree, DutyMode mode = CRAFT) const;
	/// Returns whether this person has no assignment that must be cleared before taking a base duty.
	bool isAvailableForBaseDuty() const;
	void clearBaseDuty();
	/// Gets a string version of the soldier's rank.
	const std::string getRankString() const;
	/// Gets a localizable rank string for an exact role and rank snapshot.
	const std::string getRoleRankString(SoldierRole role, int rank) const;
	/// Gets the soldier's missions.
	int getMissions() const;
	/// Gets the soldier's kills.
	int getKills() const;
	/// Gets the soldier's stuns.
	int getStuns() const;
	/// Gets the soldier's gender.
	SoldierGender getGender() const;
	/// Sets the soldier's gender.
	void setGender(SoldierGender gender);
	/// Gets the soldier's look.
	SoldierLook getLook() const;
	/// Sets the soldier's look.
	void setLook(SoldierLook look);
	/// Gets the soldier's look sub type.
	int getLookVariant() const;
	/// Sets the soldier's look sub type.
	void setLookVariant(int lookVariant);
	/// Gets soldier rules.
	const RuleSoldier *getRules() const;
	/// Gets the soldier's unique ID.
	int getId() const;
	/// Add a mission to the counter.
	void addMissionCount();
	/// Add a kill to the counter.
	void addKillCount(int count);
	/// Add a stun to the counter.
	void addStunCount(int count);
	/// Gets the soldier's hire value.
	int getHireValue() const;
	/// Get pointer to initial stats.
	const UnitStats* getInitStats() const;
	/// Get pointer to current stats.
	UnitStats *getCurrentStatsEditable();
	const UnitStats* getCurrentStats() const;
	/// Set initial and current stats.
	void setBothStats(UnitStats *stats);
	/// Gets the soldier armor.
	Armor *getArmor() const;
	/// Sets the soldier armor.
	void setArmor(Armor *armor, bool resetCustomDeployment = false);
	/// Gets the armor layers (sprite names).
	const std::vector<std::string>& getArmorLayers(Armor *customArmor = nullptr) const;
	/// Gets the soldier's original armor (before replacement).
	Armor *getReplacedArmor() const;
	/// Backs up the soldier's original armor (before replacement).
	void setReplacedArmor(Armor *armor);
	/// Gets the soldier's original armor (before transformation).
	Armor *getTransformedArmor() const;
	/// Backs up the soldier's original armor (before transformation).
	void setTransformedArmor(Armor *armor);


	/// Is the soldier wounded or not?.
	bool isWounded() const;
	/// Is the soldier wounded or not?.
	bool hasFullHealth() const;
	/// Is the soldier capable of defending a base?.
	bool canDefendBase() const;

	/// Gets the amount of missing mana.
	int getManaMissing() const;
	/// Sets the amount of missing mana.
	void setManaMissing(int manaMissing);
	/// Gets the soldier's mana recovery time.
	int getManaRecovery(int manaRecoveryPerDay) const;

	/// Gets the amount of missing health.
	int getHealthMissing() const;
	/// Sets the amount of missing health.
	void setHealthMissing(int healthMissing);
	/// Gets the soldier's health recovery time.
	int getHealthRecovery(int healthRecoveryPerDay) const;

	/// Gets the soldier's wound recovery time.
	int getWoundRecoveryInt() const;
	/// Sets the soldier's wound recovery time.
	void setWoundRecovery(int recovery);
	/// Gets ther soldier's wound recovery.
	int getWoundRecovery(float absBonus, float relBonus) const;

	/// Heals wound recoveries.
	void healWound(float absBonus, float relBonus);
	/// Replenishes mana.
	void replenishMana(int manaRecoveryPerDay);
	/// Replenishes health.
	void replenishHealth(int healthRecoveryPerDay);

	/// Daily stat replenish and healing of the soldier based on the facilities available in the base.
	void replenishStats(const BaseSumDailyRecovery& recovery);
	/// Gets number of days until the soldier is ready for action again.
	int getNeededRecoveryTime(const BaseSumDailyRecovery& recovery) const;


	/// Gets the soldier's equipment-layout.
	std::vector<EquipmentLayoutItem*> *getEquipmentLayout();

	/// Trains a soldier's psionic abilities.
	void trainPsi1Day();
	/// Is the soldier already fully psi-trained?
	bool isFullyPsiTrained();
	/// Returns whether the unit is in psi training or not
	bool isInPsiTraining() const;
	/// set the psi training status
	void setPsiTraining(bool psi);
	/// returns this soldier's psionic skill improvement score for this month.
	int getImprovement() const;
	/// Gets the soldier death info.
	SoldierDeath *getDeath() const;
	/// Kills the soldier.
	void die(SoldierDeath *death);
	/// Clears the equipment layout.
	void clearEquipmentLayout();
	/// Gets the soldier's diary.
	SoldierDiary *getDiary();
	const SoldierDiary* getDiary() const;
	/// Resets the soldier's diary.
	void resetDiary();
	/// Calculate statString.
	void calcStatString(const std::vector<StatString *> &statStrings);
	/// Trains a soldier's physical stats
	void trainPhys(int customTrainingFactor);
	/// Is the soldier already fully trained?
	bool isFullyTrained() const;
	/// Returns whether the unit is in training or not
	bool isInTraining() const;
	/// Set the training status
	void setTraining(bool training);
	/// Returns whether the soldier was just saved
	bool isJustSaved() const { return _justSaved; }
	/// Set the "Just saved" status - we recover this soldier on battlescape, but not deliverid him/her to the base yet
	void setJustSaved(bool saved) { _justSaved = saved; }
	/// Is the soldier a rookie (first mission)?
	bool isRookieSoldier() const { return _isRookieSoldier; }
	/// Sets whether the soldier is a rookie (first mission)?
	void setRookieSoldier(bool rookie) { _isRookieSoldier = rookie; }
	/// Is the soldier a newbie agent?
	bool isRookieAgent() const { return _isRookieAgent; }
	/// Sets whether the soldier is a newbie agent?
	void setRookieAgent(bool rookie) { _isRookieAgent = rookie; }
	/// Is the soldier a newbie pilot?
	bool isRookiePilot() const { return _isRookiePilot; }
	/// Sets whether the soldier is a newbie pilot?
	void setRookiePilot(bool rookie) { _isRookiePilot = rookie; }
	/// Should the soldier return to martial training automatically when fully healed?
	bool getReturnToTrainingWhenHealed() const;
	/// Sets whether the soldier should return to martial training automatically when fully healed.
	void setReturnToTrainingWhenHealed(bool returnToTrainingWhenHealed);
	/// Should the soldier return to martial training automatically when return from covert operation?
	ReturnToTrainings getReturnToTrainingsWhenOperationOver() const { return _returnToTrainingsWhenOperationOver; }
	/// Sets whether the soldier should return to martial training automatically when fully healed.
	void setReturnToTrainingWhenOperationOver(ReturnToTrainings returnToTrainingWhenOperationOver) { _returnToTrainingsWhenOperationOver = canGainExperience() ? returnToTrainingWhenOperationOver : NONE; }
	/// Sets whether the soldier's body was recovered from a battle
	void setCorpseRecovered(bool corpseRecovered);
	/// Gets the previous transformations performed on this soldier
	std::map<std::string, int> &getPreviousTransformations();
	/// Returns whether the unit is eligible for a certain transformation
	bool isEligibleForTransformation(const RuleSoldierTransformation *transformationRule) const;
	/// Returns whether all roles can survive the transformation's destination type and optional qualification.
	bool hasCompatibleRolesForTransformation(const RuleSoldierTransformation *transformationRule) const;
	/// Performs a transformation on this soldier
	void transform(const Mod *mod, RuleSoldierTransformation *transformationRule, Soldier *sourceSoldier, Base *base);
	/// Create pending transformation on this soldier
	void postponeTransformation(RuleSoldierTransformation* transformationRule);
	/// Handles pending transformation - reducing timer, performing transformation once finished
	bool handlePendingTransformation();
	/// Gets if this soldier has pending transformation
	bool hasPendingTransformation() const { return !_pendingTransformations.empty() ;}
	/// Gets pending transformation name
	const std::string &getPendingTransformation() const { return _pendingTransformations.begin()->first; }

	/// Gets possible stat inprovement
	static int improveStat(int exp, int& rate, bool bravery = false);
	/// Gets soldier roles with ranks
	const std::vector<SoldierRoleRanks*>& getRoles() const { return _roles; }
	/// Qualifies the soldier for a new role at rank 1.
	bool addRole(SoldierRole newRole);
	/// Adds experience to a role without qualifying or promoting it.
	void addExperience(SoldierRole role, int exp = 1, std::string name = "");
	/// Gets whether the soldier is qualified for a role.
	bool hasRole(SoldierRole role) const { return getRoleRank(role) >= 1; }
	/// Is this personnel trained for combat-oriented spawn placement?
	bool isDeployableCombatUnit() const;
	/// Gets whether this personnel type may improve through experience or training.
	bool canGainExperience() const;
	/// Resolves whether hacking experience belongs to the agent or engineer career.
	SoldierRole getHackingExperienceRole(SoldierRole context = ROLE_NONE) const;
	/// Gets rank of role.
	int getRoleRank(SoldierRole role) const;
	int getRoleRank(const std::vector<int>& roles) const;
	/// Gets the role with highest rank and rank value.
	std::pair<SoldierRole, int> getBestRoleRank() const;
	/// Gets the role with highest rank.
	SoldierRole getBestRole() const { return getBestRoleRank().first; }
	/// Gets if soldier has ranks only in selected role.
	bool hasOnlyOneRole(SoldierRole role) const;
	/// Gets a pointer to the dogfight experience values (FtA mechanic).
	UnitStats* getDogfightExperience() { return &_dogfightExperience; }
	/// Clears dogfight experience values (FtA mechanic).
	void clearDogfightExperience() { _dogfightExperience = UnitStats::scalar(0); }
	/// Gets a pointer to the research experience values (FtA mechanic).
	UnitStats* getResearchExperience() { return &_researchExperience; }
	/// Clears research experience values (FtA mechanic).
	void clearResearchExperience() { _researchExperience = UnitStats::scalar(0); }
	/// Gets a pointer to the research experience values (FtA mechanic).
	UnitStats* getEngineerExperience() { return &_engineerExperience; }
	/// Clears engineer experience values (FtA mechanic).
	void clearEngineerExperience() { _engineerExperience = UnitStats::scalar(0); }
	/// Gets a pointer to the intel experience values (FtA mechanic).
	UnitStats* getIntelExperience() { return &_intelExperience; }
	/// Clears intel experience values (FtA mechanic).
	void clearIntelExperience() { _intelExperience = UnitStats::scalar(0); }

	/// Calculate soldier stats improvement.
	void improvePrimaryStats(UnitStats* exp, SoldierRole role);
	/// Process role ranks promotions for a soldier.
	bool rolePromoteSoldier(SoldierRole role, RolePromotion* promotion = nullptr);
	/// Gets a sprite version of the soldier for specific role. Used for BASEBITS.PCK.
	int getRoleRankSprite(SoldierRole role) const;
	/// Gets a sprite version of the soldier for specific role. Used for SMOKE.PCK.
	int getRoleRankSpriteBattlescape(SoldierRole role) const;
	/// Gets a sprite version of the soldier for specific role. Used for TinyRanks.
	int getRoleRankSpriteTiny(SoldierRole role) const;

	/// Calculates how this project changes the soldier's stats
	UnitStats calculateStatChanges(const Mod *mod, RuleSoldierTransformation *transformationRule, Soldier *sourceSoldier, int mode, const RuleSoldier *sourceSoldierType);
	/// Checks whether the soldier has a given bonus. Disclaimer: DOES NOT REFRESH THE BONUS CACHE!
	bool hasBonus(const RuleSoldierBonus* bonus) const;
	/// Gets all the soldier bonuses
	const std::vector<const RuleSoldierBonus*> *getBonuses(const Mod *mod);
	/// Get pointer to current stats with soldier bonuses, but without armor bonuses.
	const UnitStats *getStatsWithSoldierBonusesOnly() const;
	/// Get pointer to current stats with armor and soldier bonuses.
	const UnitStats *getStatsWithAllBonuses() const;
	/// Pre-calculates soldier stats with various bonuses.
	bool prepareStatsWithBonuses(const Mod *mod);
	/// Gets a pointer to the monthly soldier experience cache.
	UnitStats* getMonthlyExperienceCache();
	/// Resets the monthly soldier experience cache.
	void resetMonthlyExperienceCache();
	/// Check if the soldier has all the required soldier bonuses for the given soldier skill.
	bool hasAllRequiredBonusesForSkill(const RuleSkill* skillRules);
	/// Check if the soldier has an active pilot role and meets all requirements for piloting the (current or new) craft.
	bool hasAllPilotingRequirements(const Craft* newCraft = nullptr) const;

private:
	std::string generateCallsign(const std::vector<SoldierNamePool*> &names);

};

}
