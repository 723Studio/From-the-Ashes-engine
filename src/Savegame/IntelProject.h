#pragma once
/*
 * Copyright 2010-2022 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include <map>
#include <string>
#include <vector>
#include "../Engine/Yaml.h"
#include "../Mod/Unit.h"

namespace OpenXcom
{

class RuleIntelProject;
class RuleIntelMilestone;
class RuleIntelEventOutput;
class RuleAlienMission;
class Soldier;
class SavedGame;
class Mod;

struct IntelProjectDailyResult
{
	std::vector<std::string> eventScripts;
	std::vector<const RuleAlienMission*> spawnMissions;
	const RuleIntelMilestone* completedMilestone = nullptr;
	bool completed = false;
};

/**
 * Global state of an intelligence project. Agents assigned at every base
 * contribute to this single object.
 */
class IntelProject
{
	friend class SavedGame;

private:
	const RuleIntelProject* _rules;
	bool _completed = false;
	std::string _currentMilestone;
	std::string _blockedMilestone;
	int64_t _milestoneProgress = 0;
	int64_t _eventProgress = 0;
	int64_t _eventCycles = 0;

	std::vector<const RuleIntelEventOutput*> getAvailableEventOutputs(const SavedGame* save) const;
	std::string getState(int progress, int cost) const;
	/// Marks the project completed and clears all active progress.
	void complete();

public:
	explicit IntelProject(const RuleIntelProject* rule);

	int getStepProgress(std::map<Soldier*, int>& assignedAgents, const Mod* mod, int rating,
		std::string& description, bool estimate = false,
		std::map<Soldier*, UnitStats>* dailyExperience = nullptr,
		std::vector<Soldier*>* contributors = nullptr);
	IntelProjectDailyResult processDay(SavedGame* save, int progress);
	bool isCurrentMilestoneBlocked(const SavedGame* save) const;
	std::string getProgressState(int dailyProgress, const SavedGame* save) const;

	const std::string& getName() const;
	const RuleIntelProject* getRules() const { return _rules; }
	bool isCompleted() const { return _completed; }
	const std::string& getCurrentMilestone() const { return _currentMilestone; }
	const std::string& getBlockedMilestone() const { return _blockedMilestone; }
	void markCurrentMilestoneBlocked();
	void clearBlockedMilestone() { _blockedMilestone.clear(); }
	int64_t getMilestoneProgress() const { return _milestoneProgress; }
	int64_t getEventProgress() const { return _eventProgress; }
	int64_t getEventCycles() const { return _eventCycles; }
	void load(const YAML::YamlNodeReader& reader);
	void save(YAML::YamlNodeWriter writer) const;
};

}
