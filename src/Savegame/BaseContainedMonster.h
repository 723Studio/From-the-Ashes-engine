#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include <map>
#include <string>
#include <vector>
#include "../Engine/Yaml.h"
#include "Promotion.h"

namespace OpenXcom
{

class Base;
class Game;
class Language;
class Mod;
class RuleContainedMonster;
class RuleItem;
class RuleResearch;
class SavedGame;
class Soldier;

enum ContainedMonsterAction : int
{
	CONTAINED_MONSTER_ACTION_NONE = 0,
	CONTAINED_MONSTER_ACTION_STUDY,
	CONTAINED_MONSTER_ACTION_QUARANTINE,
	CONTAINED_MONSTER_ACTION_VIVISECTION,
	CONTAINED_MONSTER_ACTION_TAMING
};

struct ContainedMonsterProcessResult
{
	bool died = false;
	bool terminated = false;
	bool studyCompleted = false;
	bool vivisectionCompleted = false;
	bool tamingCompleted = false;
	bool actionCancelled = false;
	bool corpseProduced = false;
	int healthRestored = 0;
	int healthLost = 0;
	Soldier* tamedSoldier = nullptr;
	std::map<const RuleItem*, int> producedItems;
	std::vector<std::string> newlyDiscoveredResearchIds;
	std::vector<std::string> researchNotifications;
	PromotionBatch promotions;

	bool removesSpecimen() const
	{
		return died || terminated || vivisectionCompleted || tamingCompleted;
	}
};

/**
 * One persistent living monster specimen held at a base.
 */
class BaseContainedMonster
{
private:
	int _id = 0;
	std::string _designation;
	const RuleContainedMonster* _rule = nullptr;
	Base* _base = nullptr;
	int _currentHP = 1;
	int _maxHP = 1;
	ContainedMonsterAction _action = CONTAINED_MONSTER_ACTION_NONE;
	int _studyProgress = 0;
	int _vivisectionProgress = 0;
	int _tamingProgress = 0;
	bool _studied = false;

	bool isUsefulResearch(const RuleResearch* research, const SavedGame& save) const;
	std::vector<Soldier*> getContributingScientists() const;
	std::map<Soldier*, int> getScientistAssignments() const;
	PromotionBatch finalizeScientistExperience(bool checkPromotions);
	ContainedMonsterProcessResult finishWithCorpse(bool terminated);
	void removeFromBase();

public:
	BaseContainedMonster(const RuleContainedMonster* rule, Base* base, int id,
		std::string designation);

	void initialize(int currentHP, int maxHP);
	void load(const YAML::YamlNodeReader& reader, const Mod* mod, SavedGame* save);
	void save(YAML::YamlNodeWriter writer) const;

	ContainedMonsterProcessResult processHour(Game& engine);
	ContainedMonsterProcessResult processDay(Game& engine);
	/// Applies a transit day without scientist mitigation or base-side removal.
	ContainedMonsterProcessResult processTransferDay(Game& engine);
	ContainedMonsterProcessResult terminate();

	bool setAction(ContainedMonsterAction action, const SavedGame& save);
	void cancelAction();
	bool normalizeActionAvailability(const SavedGame& save);
	void clearAssignments();

	bool isStudyAvailable(const SavedGame& save) const;
	bool isVivisectionAvailable(const SavedGame& save) const;
	bool isTamingAvailable(const SavedGame& save) const;
	bool isQuarantineAvailable(const SavedGame& save) const;
	bool isTamingBlockedByVivisection() const { return _vivisectionProgress > 0; }

	int getId() const { return _id; }
	const std::string& getDesignation() const { return _designation; }
	const RuleContainedMonster* getRules() const { return _rule; }
	void setBase(Base* base) { _base = base; }
	Base* getBase() const { return _base; }
	int getCurrentHP() const { return _currentHP; }
	int getMaxHP() const { return _maxHP; }
	void setCurrentHP(int health);
	ContainedMonsterAction getAction() const { return _action; }
	std::string getActionId() const;
	int getStudyProgress() const { return _studyProgress; }
	int getVivisectionProgress() const { return _vivisectionProgress; }
	int getTamingProgress() const { return _tamingProgress; }
	bool isStudied() const { return _studied; }
	int getCurrentProgress() const;
	int getCurrentCost() const;
	std::vector<Soldier*> getAssignedScientists() const;
	std::string getNameAndDesignation(Language* lang) const;

	static int calculateDailyHealthDegradation(int maxHP, double degradationPercent,
		const std::vector<double>& biologyRatios);
};

}
