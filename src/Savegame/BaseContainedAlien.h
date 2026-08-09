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
#include <cstdint>
#include <string>
#include <vector>
#include "../Engine/Yaml.h"
#include "ContainedAlienStudy.h"
#include "Promotion.h"

namespace OpenXcom
{

class Base;
class Game;
class Language;
class Mod;
class RuleContainedAlien;
class RuleContainedAlienStudyNode;
class RuleResearch;
class SavedGame;
class Soldier;

struct ContainedAlienProcessResult
{
	bool died = false;
	bool terminated = false;
	bool nodeCompleted = false;
	bool chainCompleted = false;
	std::string completedNode;
	ContainedAlienStrategicImpact strategicImpact = CONTAINED_IMPACT_ROUTINE;
	std::vector<std::string> completedMilestones;
	std::vector<std::string> researchRewards;
	std::vector<std::string> articleIds;
	std::vector<std::string> researchNotifications;
	std::vector<std::string> newlyDiscoveredResearchIds;
	bool containedAlienStudiesUnblocked = false;
	PromotionBatch promotions;
};

/**
 * One persistent living alien specimen held at a base.
 */
class BaseContainedAlien
{
private:
	static constexpr int64_t FIXED_SCALE = 1000000;

	int _id;
	std::string _designation;
	const RuleContainedAlien* _rule;
	Base* _base;
	SavedGame* _save = nullptr;
	int64_t _healthFixed = FIXED_SCALE;
	int64_t _maxHealthFixed = FIXED_SCALE;
	int64_t _stabilityFixed = 0;
	ContainedAlienStudyProtocol _protocol = CONTAINED_PROTOCOL_ACTIVE;
	std::string _currentStudyNode;
	std::vector<std::string> _completedStudyNodes;
	int64_t _studyProgressFixed = 0;
	int64_t _effectiveStudyCostFixed = 0;
	int64_t _containedHours = 0;
	std::vector<std::string> _pendingCompletedMilestones;
	std::vector<std::string> _pendingResearchRewards;
	std::vector<std::string> _pendingArticleIds;
	std::vector<std::string> _pendingResearchNotifications;
	bool _pendingSpeciesProgress = false;
	bool _pendingStudyUnblockNotification = false;

	static int64_t toFixed(double value, const std::string& context);
	static double fromFixed(int64_t value) { return static_cast<double>(value) / FIXED_SCALE; }
	void validateStudyChain() const;
	void normalizeProtocolForCurrentNode();
	PromotionBatch finalizeScientistExperience(bool checkPromotions);
	ContainedAlienProcessResult finishTerminal(bool manualTermination);
	ContainedAlienHourlyInput makeHourlyInput(ContainedAlienStudyProtocol protocol,
		const std::vector<Soldier*>& scientists) const;

public:
	BaseContainedAlien(const RuleContainedAlien* rule, Base* base, int id,
		std::string designation);

	void initialize(double health, double maxHealth, int fatalWounds, SavedGame* save);
	void load(const YAML::YamlNodeReader& reader, const Mod* mod, SavedGame* save);
	void save(YAML::YamlNodeWriter writer) const;
	ContainedAlienProcessResult processHour(Game& engine);
	ContainedAlienProcessResult terminate();
	void clearAssignments();

	int getId() const { return _id; }
	const std::string& getDesignation() const { return _designation; }
	const RuleContainedAlien* getRules() const { return _rule; }
	void setBase(Base* base) { _base = base; }
	Base* getBase() const { return _base; }
	double getHealth() const { return fromFixed(_healthFixed); }
	double getMaxHealth() const { return fromFixed(_maxHealthFixed); }
	double getStability() const { return fromFixed(_stabilityFixed); }
	ContainedAlienStudyProtocol getProtocol() const { return _protocol; }
	bool setProtocol(ContainedAlienStudyProtocol protocol);
	const std::string& getCurrentStudyNodeId() const { return _currentStudyNode; }
	const RuleContainedAlienStudyNode* getCurrentStudyNode() const;
	bool isCurrentStudyNodeBlocked() const;
	std::vector<const RuleResearch*> getUnmetStudyResearch() const;
	bool canUseProtocol(ContainedAlienStudyProtocol protocol) const;
	const std::vector<std::string>& getCompletedStudyNodes() const { return _completedStudyNodes; }
	double getStudyProgress() const { return fromFixed(_studyProgressFixed); }
	double getEffectiveStudyCost() const { return fromFixed(_effectiveStudyCostFixed); }
	int64_t getContainedHours() const { return _containedHours; }
	bool isStudyComplete() const { return _currentStudyNode.empty(); }
	std::vector<Soldier*> getAssignedScientists() const;
	std::string getNameAndDesignation(Language* lang) const;
	ContainedAlienStudyForecast getForecast(ContainedAlienStudyProtocol protocol,
		const std::vector<Soldier*>* scientists = nullptr) const;
	ContainedAlienStrategicImpact getStrategicImpact() const;
	void appendPendingArticleIds(const std::vector<std::string>& ids);
	std::vector<std::string> takePendingArticleIds();
	void appendPendingResearchNotifications(const std::vector<std::string>& ids);
	std::vector<std::string> takePendingResearchNotifications();
	void markPendingStudyUnblockNotification()
		{ _pendingStudyUnblockNotification = true; }
	bool takePendingStudyUnblockNotification();
};

}
