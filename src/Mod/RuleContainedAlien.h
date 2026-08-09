#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
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

namespace OpenXcom
{

class Mod;
class RuleItem;
class RuleResearch;
class RuleXenoSpecies;

enum ContainedAlienStudyCategory : int
{
	CONTAINED_STUDY_LIVE_BIOLOGY = 0,
	CONTAINED_STUDY_COMMUNICATION = 1,
	CONTAINED_STUDY_TACTICAL_INTELLIGENCE = 2,
	CONTAINED_STUDY_TECHNICAL_INTELLIGENCE = 3,
	CONTAINED_STUDY_STRATEGIC_INTELLIGENCE = 4
};

class RuleContainedAlienStudyNode
{
private:
	std::string _type;
	ContainedAlienStudyCategory _category = CONTAINED_STUDY_LIVE_BIOLOGY;
	double _cost = 0.0;
	double _stabilityCost = 0.0;
	std::map<std::string, double> _speciesProgress;
	std::vector<std::string> _requiredResearchNames;
	std::vector<const RuleResearch*> _requiredResearch;
	int _repeatStudyCostPercent = 100;
	bool _repeatStudyCostPercentAuthored = false;

public:
	void load(const YAML::YamlNodeReader& reader, const std::string& containedAlienType);
	void afterLoad(const Mod* mod, const std::string& containedAlienType);

	const std::string& getType() const { return _type; }
	ContainedAlienStudyCategory getCategory() const { return _category; }
	double getCost() const { return _cost; }
	double getStabilityCost() const { return _stabilityCost; }
	const std::map<std::string, double>& getSpeciesProgress() const { return _speciesProgress; }
	const std::vector<const RuleResearch*>& getRequiredResearch() const { return _requiredResearch; }
	int getRepeatStudyCostPercent() const { return _repeatStudyCostPercent; }
	bool hasExplicitRepeatStudyCostPercent() const { return _repeatStudyCostPercentAuthored; }
};

/**
 * Ruleset capture profile for one kind of living alien specimen.
 */
class RuleContainedAlien
{
private:
	std::string _type;
	std::string _xenoSpeciesName;
	const RuleXenoSpecies* _xenoSpecies = nullptr;
	std::string _corpseItemName;
	const RuleItem* _corpseItem = nullptr;
	double _initialStability = 100.0;
	double _healthDecayPerDay = 0.0;
	double _stabilityDecayPerDay = 0.0;
	double _unstableHealthDamagePerDay = 0.0;
	double _passiveRecoveryPerDay = 0.0;
	double _passiveRecoveryCap = 100.0;
	int _fatalWoundStabilityPenalty = 0;
	std::vector<RuleContainedAlienStudyNode*> _studyNodes;
	std::map<std::string, RuleContainedAlienStudyNode*> _studyNodesByType;

public:
	explicit RuleContainedAlien(const std::string& type);
	~RuleContainedAlien();

	void load(const YAML::YamlNodeReader& reader, Mod* mod);
	void afterLoad(const Mod* mod);

	const std::string& getType() const { return _type; }
	const RuleXenoSpecies* getXenoSpecies() const { return _xenoSpecies; }
	const RuleItem* getCorpseItem() const { return _corpseItem; }
	double getInitialStability() const { return _initialStability; }
	double getHealthDecayPerDay() const { return _healthDecayPerDay; }
	double getStabilityDecayPerDay() const { return _stabilityDecayPerDay; }
	double getUnstableHealthDamagePerDay() const { return _unstableHealthDamagePerDay; }
	double getPassiveRecoveryPerDay() const { return _passiveRecoveryPerDay; }
	double getPassiveRecoveryCap() const { return _passiveRecoveryCap; }
	int getFatalWoundStabilityPenalty() const { return _fatalWoundStabilityPenalty; }
	const std::vector<RuleContainedAlienStudyNode*>& getStudyNodes() const { return _studyNodes; }
	const RuleContainedAlienStudyNode* getStudyNode(const std::string& type) const;
	const RuleContainedAlienStudyNode* getNextStudyNode(const std::string& type) const;
};

}
