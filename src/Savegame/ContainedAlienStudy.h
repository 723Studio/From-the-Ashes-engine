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
#include <vector>
#include "../Mod/RuleContainedAlien.h"

namespace OpenXcom
{

class Mod;
class RuleContainedAlien;
class RuleContainedAlienStudyNode;
class Soldier;
class XenoSpecies;

enum ContainedAlienStudyProtocol : int
{
	CONTAINED_PROTOCOL_PASSIVE = 0,
	CONTAINED_PROTOCOL_ACTIVE = 1,
	CONTAINED_PROTOCOL_AGGRESSIVE = 2
};

enum ContainedAlienStudyOutlook : int
{
	CONTAINED_OUTLOOK_NONE = 0,
	CONTAINED_OUTLOOK_UNKNOWN = 1,
	CONTAINED_OUTLOOK_POOR = 2,
	CONTAINED_OUTLOOK_AVERAGE = 3,
	CONTAINED_OUTLOOK_GOOD = 4,
	CONTAINED_OUTLOOK_EXCELLENT = 5
};

enum ContainedAlienStrategicImpact : int
{
	CONTAINED_IMPACT_ROUTINE = 0,
	CONTAINED_IMPACT_SPECIES_PROGRESS = 1,
	CONTAINED_IMPACT_BREAKTHROUGH = 2
};

struct ContainedAlienProtocolModifiers
{
	int progressPercent;
	int strainPercent;
	int stabilityPercent;
	int healthPercent;
	bool passiveRecovery;
};

struct ContainedAlienHourlyInput
{
	double health = 0.0;
	double stability = 0.0;
	double progress = 0.0;
	double effectiveCost = 0.0;
	double baseCost = 0.0;
	double stabilityCost = 0.0;
	double teamEffort = 0.0;
	double healthDecayPerDay = 0.0;
	double stabilityDecayPerDay = 0.0;
	double unstableHealthDamagePerDay = 0.0;
	double passiveRecoveryPerDay = 0.0;
	double passiveRecoveryCap = 0.0;
	ContainedAlienStudyProtocol protocol = CONTAINED_PROTOCOL_ACTIVE;
	bool hasContributors = false;
	bool studyBlocked = false;
};

struct ContainedAlienHourlyOutput
{
	double health = 0.0;
	double stability = 0.0;
	double progress = 0.0;
	double progressGain = 0.0;
};

struct ContainedAlienStudyForecast
{
	bool canProgress = false;
	bool blocked = false;
	bool completesBeforeDeath = false;
	int hoursToComplete = -1;
	int hoursToDeath = -1;
	ContainedAlienStudyOutlook outlook = CONTAINED_OUTLOOK_NONE;
};

const ContainedAlienProtocolModifiers& getContainedAlienProtocolModifiers(
	ContainedAlienStudyProtocol protocol);
bool canUseContainedAlienProtocol(ContainedAlienStudyProtocol protocol,
	bool hasCurrentStudyNode, bool studyBlocked);
ContainedAlienStudyProtocol normalizeContainedAlienProtocol(
	ContainedAlienStudyProtocol protocol, bool hasCurrentStudyNode, bool studyBlocked);
double getContainedAlienTeamEffort(const std::vector<Soldier*>& scientists,
	ContainedAlienStudyCategory category, const Mod* mod);
bool hasContainedAlienContributors(const std::vector<Soldier*>& scientists);
void trainContainedAlienScientists(const std::vector<Soldier*>& scientists,
	ContainedAlienStudyCategory category, const Mod* mod);
ContainedAlienHourlyOutput processContainedAlienHour(const ContainedAlienHourlyInput& input);
bool shouldApplyContainedAlienScientificProgress(
	const ContainedAlienHourlyInput& input, const ContainedAlienHourlyOutput& output);
double calculateContainedAlienEffectiveCost(const RuleContainedAlienStudyNode* node,
	const XenoSpecies* species);
double calculateContainedAlienCaptureStability(double initialStability, int fatalWounds,
	int fatalWoundStabilityPenalty);
ContainedAlienStudyForecast forecastContainedAlienStudy(const ContainedAlienHourlyInput& input);
ContainedAlienStrategicImpact calculateContainedAlienStrategicImpact(
	const RuleContainedAlienStudyNode* node, double currentProgress, double effectiveCost,
	const XenoSpecies* species);
void runContainedAlienStudyDebugSelfTests();

}
