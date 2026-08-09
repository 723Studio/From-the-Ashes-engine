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
#include "ContainedAlienStudy.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include "../Engine/Exception.h"
#include "../Engine/RNG.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleSoldier.h"
#include "../Mod/RuleXenoSpecies.h"
#include "Craft.h"
#include "Soldier.h"
#include "XenoSpecies.h"

namespace OpenXcom
{

namespace
{

constexpr ContainedAlienProtocolModifiers PROTOCOLS[] =
{
	{55, 30, 50, 70, true},
	{100, 100, 100, 100, false},
	{165, 200, 150, 135, false}
};
constexpr double DIMINISHING_RETURNS_LOG_PERCENT = 19.0;
constexpr double INSIGHT_EFFORT_FACTOR = 0.25;
constexpr double FAMILIARITY_DISCOUNT_PER_UNIT = 0.15;
constexpr double MAX_FAMILIARITY_DISCOUNT = 0.60;
constexpr double MIN_EFFECTIVE_COST_FACTOR = 0.40;
constexpr int MAX_FORECAST_HOURS = 24 * 365 * 10;

std::vector<UnitStats::Ptr> getCategoryStats(ContainedAlienStudyCategory category)
{
	switch (category)
	{
	case CONTAINED_STUDY_LIVE_BIOLOGY:
		return {&UnitStats::biology};
	case CONTAINED_STUDY_COMMUNICATION:
		return {&UnitStats::xenolinguistics};
	case CONTAINED_STUDY_TACTICAL_INTELLIGENCE:
		return {&UnitStats::xenolinguistics, &UnitStats::tactics};
	case CONTAINED_STUDY_TECHNICAL_INTELLIGENCE:
		return {&UnitStats::xenolinguistics, &UnitStats::alienTech};
	case CONTAINED_STUDY_STRATEGIC_INTELLIGENCE:
		return {&UnitStats::xenolinguistics, &UnitStats::data, &UnitStats::alienTech};
	default:
		throw Exception("Unsupported contained-alien study category " +
			std::to_string(static_cast<int>(category)));
	}
}

bool isContributingScientist(const Soldier* scientist)
{
	return scientist && scientist->getRoleRank(ROLE_SCIENTIST) > 0 &&
		(!scientist->getCraft() || scientist->getCraft()->getStatus() != "STR_OUT");
}

bool allMilestoneStatesComplete(const std::vector<std::pair<double, double>>& states)
{
	return !states.empty() && std::all_of(states.begin(), states.end(),
		[](const auto& state) { return state.first >= state.second; });
}

double calculateEffectiveCostCore(double baseCost, int repeatStudyCostPercent,
	bool repeatStudyCostPercentAuthored, bool milestonesComplete,
	double familiarityDiscount)
{
	if (repeatStudyCostPercentAuthored && milestonesComplete)
	{
		return std::max(1.0, std::ceil(baseCost *
			static_cast<double>(repeatStudyCostPercent) / 100.0));
	}
	const double minimum = std::ceil(baseCost * MIN_EFFECTIVE_COST_FACTOR);
	return std::max(minimum, std::ceil(baseCost * (1.0 - familiarityDiscount)));
}

}

const ContainedAlienProtocolModifiers& getContainedAlienProtocolModifiers(
	ContainedAlienStudyProtocol protocol)
{
	if (protocol < CONTAINED_PROTOCOL_PASSIVE || protocol > CONTAINED_PROTOCOL_AGGRESSIVE)
	{
		throw Exception("Invalid contained-alien study protocol " +
			std::to_string(static_cast<int>(protocol)));
	}
	return PROTOCOLS[static_cast<int>(protocol)];
}

bool canUseContainedAlienProtocol(ContainedAlienStudyProtocol protocol,
	bool hasCurrentStudyNode, bool studyBlocked)
{
	getContainedAlienProtocolModifiers(protocol);
	return protocol == CONTAINED_PROTOCOL_PASSIVE ||
		(hasCurrentStudyNode && !studyBlocked);
}

ContainedAlienStudyProtocol normalizeContainedAlienProtocol(
	ContainedAlienStudyProtocol protocol, bool hasCurrentStudyNode, bool studyBlocked)
{
	return canUseContainedAlienProtocol(protocol, hasCurrentStudyNode, studyBlocked)
		? protocol : CONTAINED_PROTOCOL_PASSIVE;
}

double getContainedAlienTeamEffort(const std::vector<Soldier*>& scientists,
	ContainedAlienStudyCategory category, const Mod* mod)
{
	const auto fields = getCategoryStats(category);
	double effort = 0.0;
	size_t contributors = 0;
	for (const auto* scientist : scientists)
	{
		if (!isContributingScientist(scientist))
		{
			continue;
		}
		const UnitStats* stats = scientist->getCurrentStats();
		double individual = 0.0;
		for (UnitStats::Ptr field : fields)
		{
			individual += stats->*field;
		}
		individual /= static_cast<double>(fields.size());
		individual += static_cast<double>(stats->insight) * INSIGHT_EFFORT_FACTOR;
		effort += individual;
		++contributors;
	}
	if (contributors > 1)
	{
		effort *= std::max(0.0,
			(100.0 - DIMINISHING_RETURNS_LOG_PERCENT * std::log(static_cast<double>(contributors))) / 100.0);
	}
	return effort * static_cast<double>(mod->getResearchSpeedFactor()) / 100.0;
}

bool hasContainedAlienContributors(const std::vector<Soldier*>& scientists)
{
	return std::any_of(scientists.begin(), scientists.end(), isContributingScientist);
}

void trainContainedAlienScientists(const std::vector<Soldier*>& scientists,
	ContainedAlienStudyCategory category, const Mod* mod)
{
	const auto fields = getCategoryStats(category);
	const int trainingFactor = mod->getResearchTrainingFactor();
	for (auto* scientist : scientists)
	{
		if (!isContributingScientist(scientist) || !scientist->canGainExperience())
		{
			continue;
		}
		const UnitStats* stats = scientist->getCurrentStats();
		const UnitStats caps = scientist->getRules()->getStatCaps();
		for (UnitStats::Ptr field : fields)
		{
			const int current = stats->*field;
			const int cap = caps.*field;
			if (current < cap && RNG::generate(0, cap) > current && RNG::percent(trainingFactor))
			{
				scientist->getResearchExperience()->*field += 1;
			}
		}
	}
}

ContainedAlienHourlyOutput processContainedAlienHour(const ContainedAlienHourlyInput& input)
{
	// Validate the persisted/preview protocol even though a blocked node always
	// uses Passive physical handling.
	getContainedAlienProtocolModifiers(input.protocol);
	const auto& modifiers = getContainedAlienProtocolModifiers(input.studyBlocked
		? CONTAINED_PROTOCOL_PASSIVE : input.protocol);
	ContainedAlienHourlyOutput output;
	output.health = input.health;
	output.stability = input.stability;
	output.progress = input.progress;

	if (!input.studyBlocked && input.hasContributors &&
		input.effectiveCost > 0.0 && input.baseCost > 0.0 &&
		input.progress < input.effectiveCost)
	{
		output.progressGain = input.teamEffort *
			(static_cast<double>(modifiers.progressPercent) / 100.0) / 24.0;
		output.progressGain = std::max(0.0,
			std::min(output.progressGain, input.effectiveCost - input.progress));
		output.progress += output.progressGain;
	}

	const double studyStrain = input.baseCost > 0.0
		? input.stabilityCost * output.progressGain / input.baseCost *
			(static_cast<double>(modifiers.strainPercent) / 100.0)
		: 0.0;
	double passiveRecovery = 0.0;
	if (modifiers.passiveRecovery && input.hasContributors && input.passiveRecoveryCap > 0.0)
	{
		passiveRecovery = input.passiveRecoveryPerDay *
			std::max(0.0, input.passiveRecoveryCap - input.stability) /
			input.passiveRecoveryCap / 24.0;
	}
	output.stability = std::clamp(
		input.stability - input.stabilityDecayPerDay *
			(static_cast<double>(modifiers.stabilityPercent) / 100.0) / 24.0 -
			studyStrain + passiveRecovery,
		0.0, 100.0);
	const double instability = 1.0 - output.stability / 100.0;
	const double healthLoss = (input.healthDecayPerDay +
		input.unstableHealthDamagePerDay * instability * instability) *
		(static_cast<double>(modifiers.healthPercent) / 100.0) / 24.0;
	output.health = input.health - healthLoss;
	return output;
}

bool shouldApplyContainedAlienScientificProgress(
	const ContainedAlienHourlyInput& input, const ContainedAlienHourlyOutput& output)
{
	return !input.studyBlocked && output.progressGain > 0.0;
}

double calculateContainedAlienEffectiveCost(const RuleContainedAlienStudyNode* node,
	const XenoSpecies* species)
{
	if (!node || !species)
	{
		throw Exception("Cannot calculate contained-alien effective cost without node and species state");
	}
	double discount = 0.0;
	std::vector<std::pair<double, double>> milestoneStates;
	milestoneStates.reserve(node->getSpeciesProgress().size());
	if (!node->getSpeciesProgress().empty())
	{
		discount = MAX_FAMILIARITY_DISCOUNT;
		for (const auto& progress : node->getSpeciesProgress())
		{
			const auto* milestone = species->getRules()->getMilestone(progress.first);
			const double currentProgress = species->getMilestoneProgress(progress.first);
			milestoneStates.emplace_back(currentProgress, milestone->getCost());
			const double familiarityUnits = currentProgress / milestone->getCost();
			const double milestoneDiscount = std::min(MAX_FAMILIARITY_DISCOUNT,
				FAMILIARITY_DISCOUNT_PER_UNIT * familiarityUnits);
			discount = std::min(discount, milestoneDiscount);
		}
	}
	return calculateEffectiveCostCore(node->getCost(), node->getRepeatStudyCostPercent(),
		node->hasExplicitRepeatStudyCostPercent(),
		allMilestoneStatesComplete(milestoneStates), discount);
}

double calculateContainedAlienCaptureStability(double initialStability, int fatalWounds,
	int fatalWoundStabilityPenalty)
{
	return std::clamp(initialStability -
		static_cast<double>(std::max(0, fatalWounds)) * fatalWoundStabilityPenalty,
		0.0, 100.0);
}

ContainedAlienStudyForecast forecastContainedAlienStudy(const ContainedAlienHourlyInput& input)
{
	ContainedAlienStudyForecast forecast;
	forecast.blocked = input.studyBlocked;
	forecast.canProgress = !input.studyBlocked &&
		input.hasContributors && input.teamEffort > 0.0 &&
		input.effectiveCost > input.progress && input.baseCost > 0.0;

	// Completion and physical collapse are independent forecast timelines.  A
	// specimen can have a theoretical completion time even when it will die
	// first, while physical strain must continue after the current node would
	// finish so that completion does not artificially extend its life.
	ContainedAlienHourlyInput study = input;
	for (int hour = 1; forecast.canProgress && hour <= MAX_FORECAST_HOURS; ++hour)
	{
		const auto output = processContainedAlienHour(study);
		study.progress = output.progress;
		if (study.progress >= study.effectiveCost)
		{
			forecast.hoursToComplete = hour;
			break;
		}
	}

	ContainedAlienHourlyInput physical = input;
	if (forecast.canProgress)
	{
		// Keep applying the current team's full protocol load after the projected
		// node boundary; the forecast cannot know the next node, but must not drop
		// study strain merely because this one completed.
		physical.effectiveCost = std::numeric_limits<double>::max();
	}
	for (int hour = 1; hour <= MAX_FORECAST_HOURS; ++hour)
	{
		const auto output = processContainedAlienHour(physical);
		physical.health = output.health;
		physical.stability = output.stability;
		physical.progress = output.progress;
		if (physical.health <= 0.0)
		{
			forecast.hoursToDeath = hour;
			break;
		}
	}

	// Runtime resolves death before study completion for the same hour.
	forecast.completesBeforeDeath = forecast.hoursToComplete >= 0 &&
		(forecast.hoursToDeath < 0 || forecast.hoursToComplete < forecast.hoursToDeath);
	if (!forecast.canProgress)
	{
		forecast.outlook = CONTAINED_OUTLOOK_NONE;
		return forecast;
	}
	if (input.progress / input.effectiveCost < 0.25)
	{
		forecast.outlook = CONTAINED_OUTLOOK_UNKNOWN;
		return forecast;
	}
	if (forecast.hoursToComplete < 0)
	{
		forecast.outlook = CONTAINED_OUTLOOK_POOR;
		return forecast;
	}
	if (forecast.hoursToDeath < 0)
	{
		forecast.outlook = CONTAINED_OUTLOOK_EXCELLENT;
		return forecast;
	}
	const double ratio = static_cast<double>(forecast.hoursToDeath) / forecast.hoursToComplete;
	if (ratio < 0.65) forecast.outlook = CONTAINED_OUTLOOK_POOR;
	else if (!forecast.completesBeforeDeath) forecast.outlook = CONTAINED_OUTLOOK_AVERAGE;
	else if (ratio < 1.5) forecast.outlook = CONTAINED_OUTLOOK_GOOD;
	else forecast.outlook = CONTAINED_OUTLOOK_EXCELLENT;
	return forecast;
}

ContainedAlienStrategicImpact calculateContainedAlienStrategicImpact(
	const RuleContainedAlienStudyNode* node, double currentProgress, double effectiveCost,
	const XenoSpecies* species)
{
	if (!node || !species || node->getSpeciesProgress().empty())
	{
		return CONTAINED_IMPACT_ROUTINE;
	}
	bool advancesIncomplete = false;
	for (const auto& progress : node->getSpeciesProgress())
	{
		const auto* milestone = species->getRules()->getMilestone(progress.first);
		const double current = species->getMilestoneProgress(progress.first);
		if (current >= milestone->getCost())
		{
			continue;
		}
		advancesIncomplete = true;
		const double remainingFraction = effectiveCost > 0.0
			? std::clamp((effectiveCost - currentProgress) / effectiveCost, 0.0, 1.0)
			: 0.0;
		if (current + progress.second * remainingFraction >= milestone->getCost())
		{
			return CONTAINED_IMPACT_BREAKTHROUGH;
		}
	}
	return advancesIncomplete ? CONTAINED_IMPACT_SPECIES_PROGRESS : CONTAINED_IMPACT_ROUTINE;
}

void runContainedAlienStudyDebugSelfTests()
{
#ifndef NDEBUG
	static const bool tested = []
	{
		const auto closeEnough = [](double a, double b)
		{
			return std::abs(a - b) < 0.000001;
		};

		assert(!allMilestoneStatesComplete({}));
		assert(!allMilestoneStatesComplete({{100.0, 100.0}, {99.0, 100.0}}));
		assert(allMilestoneStatesComplete({{100.0, 100.0}, {300.0, 200.0}}));
		// An omitted repeat percentage preserves the legacy familiarity path,
		// even after every referenced milestone is complete.
		assert(closeEnough(calculateEffectiveCostCore(
			100.0, 100, false, true, 0.60), 40.0));
		// Explicit 100 is a real repeat override and deliberately restores base cost.
		assert(closeEnough(calculateEffectiveCostCore(
			100.0, 100, true, true, 0.60), 100.0));
		assert(closeEnough(calculateEffectiveCostCore(
			100.0, 15, true, true, 0.60), 15.0));
		assert(closeEnough(calculateEffectiveCostCore(
			101.0, 15, true, true, 0.60), 16.0));
		assert(closeEnough(calculateEffectiveCostCore(
			0.1, 1, true, true, 0.60), 1.0));
		assert(closeEnough(calculateEffectiveCostCore(
			100.0, 15, true, false, 0.15), 85.0));
		assert(closeEnough(calculateEffectiveCostCore(
			100.0, 15, false, false, 0.60), 40.0));
		const double fixedAtNodeEntry =
			calculateEffectiveCostCore(100.0, 15, true, false, 0.15);
		const double laterSpecimenCost =
			calculateEffectiveCostCore(100.0, 15, true, true, 0.60);
		assert(closeEnough(fixedAtNodeEntry, 85.0));
		assert(closeEnough(laterSpecimenCost, 15.0));
		assert(!closeEnough(fixedAtNodeEntry, laterSpecimenCost));

		assert(canUseContainedAlienProtocol(CONTAINED_PROTOCOL_PASSIVE, true, true));
		assert(!canUseContainedAlienProtocol(CONTAINED_PROTOCOL_ACTIVE, true, true));
		assert(!canUseContainedAlienProtocol(CONTAINED_PROTOCOL_AGGRESSIVE, true, true));
		auto normalizedProtocol = normalizeContainedAlienProtocol(
			CONTAINED_PROTOCOL_ACTIVE, true, true);
		assert(normalizedProtocol == CONTAINED_PROTOCOL_PASSIVE);
		// Unlocking the node does not restore the protocol that was rejected.
		normalizedProtocol = normalizeContainedAlienProtocol(
			normalizedProtocol, true, false);
		assert(normalizedProtocol == CONTAINED_PROTOCOL_PASSIVE);
		assert(normalizeContainedAlienProtocol(CONTAINED_PROTOCOL_AGGRESSIVE, true, true) ==
			CONTAINED_PROTOCOL_PASSIVE);
		assert(canUseContainedAlienProtocol(CONTAINED_PROTOCOL_ACTIVE, true, false));
		assert(canUseContainedAlienProtocol(CONTAINED_PROTOCOL_AGGRESSIVE, true, false));
		assert(normalizeContainedAlienProtocol(CONTAINED_PROTOCOL_PASSIVE, true, false) ==
			CONTAINED_PROTOCOL_PASSIVE);
		// A completed chain has no current node and is Passive-only.
		assert(canUseContainedAlienProtocol(CONTAINED_PROTOCOL_PASSIVE, false, false));
		assert(!canUseContainedAlienProtocol(CONTAINED_PROTOCOL_ACTIVE, false, false));
		assert(!canUseContainedAlienProtocol(CONTAINED_PROTOCOL_AGGRESSIVE, false, false));
		assert(normalizeContainedAlienProtocol(CONTAINED_PROTOCOL_ACTIVE, false, false) ==
			CONTAINED_PROTOCOL_PASSIVE);
		assert(normalizeContainedAlienProtocol(CONTAINED_PROTOCOL_AGGRESSIVE, false, false) ==
			CONTAINED_PROTOCOL_PASSIVE);

		ContainedAlienHourlyInput completed;
		completed.health = 100.0;
		completed.stability = 40.0;
		completed.healthDecayPerDay = 24.0;
		completed.stabilityDecayPerDay = 6.0;
		completed.passiveRecoveryPerDay = 24.0;
		completed.passiveRecoveryCap = 100.0;
		completed.protocol = CONTAINED_PROTOCOL_PASSIVE;
		completed.hasContributors = true;
		const auto completedOutput = processContainedAlienHour(completed);
		assert(closeEnough(completedOutput.progressGain, 0.0));
		assert(!shouldApplyContainedAlienScientificProgress(completed, completedOutput));
		assert(completedOutput.stability > completed.stability);
		assert(completedOutput.health < completed.health);

		assert(closeEnough(calculateContainedAlienCaptureStability(80.0, 0, 20), 80.0));
		assert(closeEnough(calculateContainedAlienCaptureStability(80.0, 3, 0), 80.0));
		assert(closeEnough(calculateContainedAlienCaptureStability(80.0, 1, 20), 60.0));
		assert(closeEnough(calculateContainedAlienCaptureStability(80.0, 3, 20), 20.0));
		assert(closeEnough(calculateContainedAlienCaptureStability(80.0, 5, 20), 0.0));

		ContainedAlienHourlyInput blocked;
		blocked.health = 24.0;
		blocked.stability = 50.0;
		blocked.progress = 10.0;
		blocked.effectiveCost = 100.0;
		blocked.baseCost = 100.0;
		blocked.stabilityCost = 10.0;
		blocked.teamEffort = 24.0;
		blocked.healthDecayPerDay = 24.0;
		blocked.stabilityDecayPerDay = 24.0;
		blocked.passiveRecoveryPerDay = 24.0;
		blocked.passiveRecoveryCap = 100.0;
		blocked.protocol = CONTAINED_PROTOCOL_AGGRESSIVE;
		blocked.hasContributors = true;
		blocked.studyBlocked = true;
		const auto blockedOutput = processContainedAlienHour(blocked);
		assert(closeEnough(blockedOutput.progress, blocked.progress));
		assert(closeEnough(blockedOutput.progressGain, 0.0));
		assert(!shouldApplyContainedAlienScientificProgress(blocked, blockedOutput));
		assert(blockedOutput.health < blocked.health);
		assert(closeEnough(blockedOutput.stability, blocked.stability));
		blocked.passiveRecoveryPerDay = 0.0;
		assert(processContainedAlienHour(blocked).stability < blocked.stability);

		blocked.health = 0.1;
		const auto blockedForecast = forecastContainedAlienStudy(blocked);
		assert(blockedForecast.blocked);
		assert(!blockedForecast.canProgress);
		assert(blockedForecast.hoursToComplete < 0);
		assert(blockedForecast.hoursToDeath == 1);

		ContainedAlienHourlyInput ready;
		ready.health = 100.0;
		ready.stability = 100.0;
		ready.effectiveCost = 1.0;
		ready.baseCost = 1.0;
		ready.teamEffort = 24.0;
		ready.protocol = CONTAINED_PROTOCOL_ACTIVE;
		ready.hasContributors = true;
		const auto readyOutput = processContainedAlienHour(ready);
		assert(shouldApplyContainedAlienScientificProgress(ready, readyOutput));
		const auto readyForecast = forecastContainedAlienStudy(ready);
		assert(!readyForecast.blocked);
		assert(readyForecast.canProgress);
		assert(readyForecast.hoursToComplete == 1);
		return true;
	}();
	(void)tested;
#endif
}

}
