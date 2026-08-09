#pragma once
/*
 * Copyright 2010-2022 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
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

class Mod;
class RuleAlienMission;
class RuleEventScript;
class RuleResearch;

enum IntelProjectEffectType : int
{
	INTEL_EFFECT_NONE = 0,
	INTEL_EFFECT_UFO_TRACKING = 1,
	INTEL_EFFECT_COVERT_OPERATIONS = 2,
	INTEL_EFFECT_DEPLOYMENT_HINTS = 3
};

class RuleIntelMilestone
{
private:
	std::string _id;
	std::string _previousMilestone;
	int _cost = 0;
	std::vector<std::string> _requiredResearchNames;
	std::vector<const RuleResearch*> _requiredResearch;
	std::vector<std::string> _eventScriptNames;
	std::vector<const RuleEventScript*> _eventScripts;
	std::string _spawnMissionName;
	const RuleAlienMission* _spawnMission = nullptr;
	std::vector<std::string> _unlockResearchNames;
	std::vector<const RuleResearch*> _unlockResearches;
	bool _completesProject = false;

public:
	void load(const YAML::YamlNodeReader& reader, const std::string& projectName);
	void afterLoad(const Mod* mod, const std::string& projectName);

	const std::string& getId() const { return _id; }
	const std::string& getPreviousMilestone() const { return _previousMilestone; }
	int getCost() const { return _cost; }
	const std::vector<const RuleResearch*>& getRequiredResearch() const { return _requiredResearch; }
	const std::vector<std::string>& getEventScripts() const { return _eventScriptNames; }
	const RuleAlienMission* getSpawnMission() const { return _spawnMission; }
	const std::vector<const RuleResearch*>& getUnlockResearches() const { return _unlockResearches; }
	bool completesProject() const { return _completesProject; }
};

class RuleIntelEventOutput
{
private:
	std::string _eventScriptName;
	const RuleEventScript* _eventScript = nullptr;
	std::string _spawnMissionName;
	const RuleAlienMission* _spawnMission = nullptr;
	int _weight = 100;
	int64_t _minCycles = 0;
	std::vector<std::string> _requiredResearchNames;
	std::vector<const RuleResearch*> _requiredResearch;
	std::vector<std::string> _disabledByResearchNames;
	std::vector<const RuleResearch*> _disabledByResearch;

public:
	void load(const YAML::YamlNodeReader& reader, const std::string& projectName);
	void afterLoad(const Mod* mod, const std::string& projectName);

	const std::string& getEventScript() const { return _eventScriptName; }
	const std::string& getSpawnMissionName() const { return _spawnMissionName; }
	const RuleAlienMission* getSpawnMission() const { return _spawnMission; }
	int getWeight() const { return _weight; }
	int64_t getMinCycles() const { return _minCycles; }
	const std::vector<const RuleResearch*>& getRequiredResearch() const { return _requiredResearch; }
	const std::vector<const RuleResearch*>& getDisabledByResearch() const { return _disabledByResearch; }
};

class RuleIntelEventCycle
{
private:
	int _cost = 0;
	std::vector<RuleIntelEventOutput*> _outputs;

public:
	~RuleIntelEventCycle();
	void load(const YAML::YamlNodeReader& reader, const std::string& projectName);
	void afterLoad(const Mod* mod, const std::string& projectName);

	int getCost() const { return _cost; }
	const std::vector<RuleIntelEventOutput*>& getOutputs() const { return _outputs; }
};

/**
 * Rules for one global intelligence project.
 */
class RuleIntelProject
{
private:
	std::string _name;
	std::string _description;
	std::vector<std::string> _requiredResearchNames;
	std::vector<const RuleResearch*> _requiredResearch;
	IntelProjectEffectType _effectType = INTEL_EFFECT_NONE;
	UnitStats _stats;
	std::vector<RuleIntelMilestone*> _milestones;
	std::map<std::string, RuleIntelMilestone*> _milestonesById;
	std::map<std::string, const RuleIntelMilestone*> _nextMilestones;
	const RuleIntelMilestone* _firstMilestone = nullptr;
	RuleIntelEventCycle* _eventCycle = nullptr;
	int _listOrder;

	void validateStats() const;
	void buildMilestoneChain();

public:
	RuleIntelProject(const std::string& name, int listOrder);
	~RuleIntelProject();

	void load(const YAML::YamlNodeReader& reader, Mod* mod);
	void afterLoad(const Mod* mod);

	const std::string& getName() const { return _name; }
	const std::string& getDescription() const { return _description; }
	const std::vector<const RuleResearch*>& getRequiredResearch() const { return _requiredResearch; }
	IntelProjectEffectType getEffectType() const { return _effectType; }
	UnitStats getStats() const { return _stats; }
	int getListOrder() const { return _listOrder; }
	const std::vector<RuleIntelMilestone*>& getMilestones() const { return _milestones; }
	const RuleIntelMilestone* getFirstMilestone() const { return _firstMilestone; }
	const RuleIntelMilestone* getMilestone(const std::string& id) const;
	const RuleIntelMilestone* getNextMilestone(const std::string& id) const;
	const RuleIntelEventCycle* getEventCycle() const { return _eventCycle; }
};

}
