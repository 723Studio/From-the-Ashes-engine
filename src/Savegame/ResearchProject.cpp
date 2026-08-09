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
#include "ResearchProject.h"
#include "../Mod/RuleResearch.h"
#include "../Mod/Mod.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/Craft.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace OpenXcom
{
const float PROGRESS_LIMIT_UNKNOWN = 0.333f;

ResearchProject::ResearchProject(const RuleResearch * p, int c) : _project(p), _spent(0), _cost(c)
{
}

/**
 * Called every day (every hour in FtA) to compute time spent on this ResearchProject
 * @return true if the ResearchProject is finished
 */
bool ResearchProject::step(int progress)
{
	_spent += progress;
	return isFinished();
}

int getScientistProjectProgress(const UnitStats& projectStats,
	const std::map<Soldier*, int>& assignedScientists, Mod* mod, int rating,
	bool requirePositiveProgressForExperience)
{
	double effort = 0;
	int trainingFactor = mod->getResearchTrainingFactor();
	double speedFactor = (double)mod->getResearchSpeedFactor() / 100;
	std::size_t contributingScientists = 0;
	std::map<Soldier*, UnitStats> pendingExperience;
	for (const auto& assignment : assignedScientists)
	{
		Soldier* scientist = assignment.first;
		if (scientist->getCraft() != nullptr && scientist->getCraft()->getStatus() == "STR_OUT")
		{
			continue;
		}
		++contributingScientists;
		const bool canTrain = scientist->canGainExperience();

		auto stats = scientist->getCurrentStats();
		auto caps = scientist->getRules()->getStatCaps();
		unsigned int statsN = 0;
		double soldierEffort = 0, statEffort = 0;
		auto processStat = [&](UnitStats::Ptr field, bool allowMinimumTraining)
		{
			const int requirement = projectStats.*field;
			if (requirement <= 0)
			{
				return;
			}
			const int current = stats->*field;
			const int cap = caps.*field;
			statEffort = current;
			soldierEffort += statEffort / requirement;
			bool trained = canTrain && current < cap &&
				RNG::generate(0, cap) > current &&
				RNG::percent(trainingFactor) && RNG::percent(assignment.second);
			if (!trained && allowMinimumTraining)
			{
				const int minimum = scientist->getRules()->getMinStats().*field;
				trained = canTrain && current < minimum &&
					RNG::percent(100 - current) && RNG::percent(trainingFactor / 2) &&
					RNG::percent(assignment.second);
			}
			if (trained)
			{
				pendingExperience[scientist].*field += 1;
			}
			++statsN;
		};

		processStat(&UnitStats::physics, false);
		processStat(&UnitStats::chemistry, false);
		processStat(&UnitStats::biology, false);
		processStat(&UnitStats::data, false);
		processStat(&UnitStats::computers, false);
		processStat(&UnitStats::tactics, false);
		processStat(&UnitStats::materials, false);
		processStat(&UnitStats::designing, false);
		processStat(&UnitStats::alienTech, true);
		processStat(&UnitStats::psionics, true);
		processStat(&UnitStats::xenolinguistics, true);

		double insightBonus = RNG::generate(0, stats->insight);
		insightBonus /= 100;
		soldierEffort += insightBonus;

		soldierEffort /= statsN + 1;

		effort += soldierEffort;
	}

	// If one woman can carry a baby in nine months, nine women can't do it in a month...
	if (contributingScientists > 1)
		effort *= std::max(0.0, (100.0 - 19.0 * log(contributingScientists)) / 100.0);

	effort *= rating; //not normalizing by 100 to fit small hourly values into integer later
	effort *= speedFactor;

	const int progress = std::max(0, static_cast<int>(effort));
	if (!requirePositiveProgressForExperience || progress > 0)
	{
		for (const auto& experience : pendingExperience)
		{
			*experience.first->getResearchExperience() += experience.second;
		}
	}

	return progress;
}

int ResearchProject::getStepProgress(std::map<Soldier*, int>& assignedScientists, Mod* mod, int rating)
{
	return getScientistProjectProgress(_project->getStats(), assignedScientists, mod, rating);
}

/**
 * gets state of project.
 */
bool ResearchProject::isFinished()
{
	return _spent >= getCost();
}

const RuleResearch * ResearchProject::getRules() const
{
	return _project;
}


/**
 * Returns the time already spent on this project
 * @return the time already spent on this ResearchProject(in man/day)
 */
int ResearchProject::getSpent() const
{
	return _spent;
}

/**
 * Changes the cost of the ResearchProject
 * @param spent new project cost(in man/day)
 */
void ResearchProject::setSpent (int spent)
{
	_spent = spent;
}

/**
 * Returns the cost of the ResearchProject
 * @return the cost of the ResearchProject(in man/day)
 */
int ResearchProject::getCost() const
{
	return _cost;
}

/**
 * Changes the cost of the ResearchProject
 * @param f new project cost(in man/day)
 */
void ResearchProject::setCost(int f)
{
	_cost = f;
}

/**
 * Loads the research project from a YAML file.
 * @param node YAML node.
 */
void ResearchProject::load(const YAML::YamlNodeReader& reader)
{
	setSpent(reader["spent"].readVal(getSpent()));
	setCost(reader["cost"].readVal(getCost()));
}

/**
 * Saves the research project to a YAML file.
 * @return YAML node.
 */
void ResearchProject::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("project", getRules()->getName());
	writer.write("spent", getSpent());
	writer.write("cost", getCost());
}

/**
 * Return a string describing Research progress.
 * @return a string describing Research progress.
 */
std::string ResearchProject::getResearchProgress() const
{
	float progress = (float)getSpent() / (float)getCost();
	if (progress <= 0.0f)
	{
		return "STR_NONE";
	}
	else if (progress <= PROGRESS_LIMIT_UNKNOWN)
	{
		return "STR_UNKNOWN";
	}
	else if (progress <= 0.5f)
	{
		return "STR_POOR";
	}
	else if (progress <= 0.75f)
	{
		return "STR_AVERAGE";
	}
	else if (progress <= 0.9f)
	{
		return "STR_GOOD";
	}
	else
	{
		return "STR_EXCELLENT";
	}
}

}
