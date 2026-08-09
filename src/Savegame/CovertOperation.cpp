/*
 * Copyright 2010-2019 OpenXcom Developers.
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
#include "CovertOperation.h"
#include <algorithm>
#include "SerializationHelper.h"
#include "../fmath.h"
#include "../Engine/Language.h"
#include "../Engine/Game.h"
#include "../Engine/RNG.h"
#include "../Engine/Logger.h"
#include "../Geoscape/Globe.h"
#include "../Battlescape/BattlescapeGenerator.h"
#include "../Battlescape/BriefingState.h"
#include "../Battlescape/DebriefingState.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Savegame/Region.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/Soldier.h"
#include "../Savegame/DiplomacyFaction.h"
#include "../Savegame/SavedBattleGame.h"
#include "../Savegame/AlienBase.h"
#include "../Savegame/AlienMission.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCovertOperation.h"
#include "../Mod/RuleSoldier.h"
#include "../Mod/RuleDiplomacyFaction.h"
#include "../Mod/RuleAlienMission.h"
#include "../Mod/AlienDeployment.h"
#include "../Mod/AlienRace.h"
#include "../Mod/Unit.h"
#include "../Geoscape/GeoscapeState.h"
#include "../Ufopaedia/Ufopaedia.h"

namespace OpenXcom
{
CovertOperation::CovertOperation(const RuleCovertOperation* rule, const Mod* mod, Base* base, int cost, int chances) :
	_rule(rule), _mod(mod), _base(base), _spent(0), _cost(cost), _successChance(chances),
	_results(0), _inBattlescape(false), _hasBattlescapeResolve(false), _over(false), _hasPsi(false), _progressEventSpawned(false), _cancelled(false),
	_resultNotificationClaimed(false)
{
	_items = new ItemContainer();
	if (base != 0)
	{
		setBase(base);
	}
}

CovertOperation::~CovertOperation()
{
	delete _items;
	delete _results;
}

/**
* Loads the event from YAML.
* @param node The YAML node containing the data.
*/
void CovertOperation::load(const YAML::YamlNodeReader& reader)
{
	reader.tryRead("spent", _spent);
	reader.tryRead("cost", _cost);
	reader.tryRead("successChance", _successChance);
	reader.tryRead("inBattlescape", _inBattlescape);
	reader.tryRead("hasBattlescapeResolve", _hasBattlescapeResolve);
	reader.tryRead("hasPsi", _hasPsi);
	reader.tryRead("over", _over);
	reader.tryRead("progressEventSpawned", _progressEventSpawned);
	reader.tryRead("cancelled", _cancelled);
	reader.tryRead("finishedResult", _finishedResult);
	reader.tryRead("researchName", _researchName);
	_items->load(reader["items"], _mod);
	if (const auto& resultsReader = reader["results"])
	{
		_results = new CovertOperationResults(getOperationName(), _finishedResult, "0");
		_results->load(resultsReader);
	}
}

/**
 	* Saves the Operation to YAML.
 	* @return YAML node.
 	*/
void CovertOperation::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("name", getRules()->getName());
	writer.write("spent", _spent);
	writer.write("cost", _cost);
	writer.write("successChance", _successChance);
	writer.write("hasPsi", _hasPsi);
	writer.write("inBattlescape", _inBattlescape);
	writer.write("hasBattlescapeResolve", _hasBattlescapeResolve);
	writer.write("progressEventSpawned", _progressEventSpawned);
	writer.write("finishedResult", _finishedResult);
	if (!_researchName.empty())
	{
		writer.write("researchName", _researchName);
	}
	if (_cancelled)
	{
		writer.write("cancelled", _cancelled);
	}
	if (_over)
	{
		writer.write("over", _over);
	}
	_items->save(writer["items"]);
	if (_results)
	{
		_results->save(writer["results"]);
	}
}


void CovertOperation::setBase(Base* base)
{
	_base = base;
}

/**
 * Loads a Covert Operation name from rules.
 * @return operation name.
 */
std::string CovertOperation::getOperationName()
{
	return _rule->getName();
}


/**
 * Return a vector of pointers to a Soldier realizations that assigned to this operation.
 * @return a vector of pointers to a Soldier class.
 */
std::vector<Soldier*> CovertOperation::getSoldiers()
{
	std::vector<Soldier*> soldiers;
	for (auto& soldier : *_base->getSoldiers())
	{
		if (soldier->getCovertOperation() == this)
		{
			soldiers.push_back(soldier);
		}
	}
	return soldiers;
}

bool CovertOperation::claimResultNotification()
{
	if (_resultNotificationClaimed || _over || _inBattlescape)
	{
		return false;
	}
	_resultNotificationClaimed = true;
	return true;
}

void CovertOperationResults::load(const YAML::YamlNodeReader& reader)
{
	reader.tryRead("operationName", _operationName);
	reader.tryRead("result", _result);
	reader.tryRead("finishDate", _finishDate);
	reader.tryRead("score", _score);
	reader.tryRead("funds", _funds);
	reader.tryRead("items", _bountyItems);
	reader.tryRead("reputation", _reputation);
	reader.tryRead("soldierDamage", _soldierDamage);
	reader.tryRead("specialMessage", _specialMessage);
	reader.tryRead("newResearchNotifications", _newResearchNotifications);
	reader.tryRead("containedAlienStudiesUnblocked", _containedAlienStudiesUnblocked);
	reader.tryRead("researchArticleIds", _researchArticleIds);
	for (const auto& promotionReader : reader["promotions"].children())
	{
		RolePromotion promotion;
		int role = static_cast<int>(ROLE_NONE);
		promotionReader.tryRead("soldierId", promotion.soldierId);
		promotionReader.tryRead("role", role);
		promotionReader.tryRead("previousRank", promotion.previousRank);
		promotionReader.tryRead("newRank", promotion.newRank);
		promotion.role = static_cast<SoldierRole>(role);
		_promotions.push_back(promotion);
	}
	for (const auto& statsReader : reader["soldierStats"].children())
	{
		std::string soldierName;
		UnitStats stats;
		statsReader.tryRead("name", soldierName);
		statsReader.tryRead("stats", stats);
		_soldierStats.emplace_back(soldierName, stats);
	}
}

void CovertOperationResults::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	writer.write("operationName", _operationName);
	writer.write("result", _result);
	writer.write("finishDate", _finishDate);
	writer.write("score", _score);
	writer.write("funds", _funds);
	if (!_bountyItems.empty())
		writer.write("items", _bountyItems);
	if (!_reputation.empty())
		writer.write("reputation", _reputation);
	if (!_soldierDamage.empty())
		writer.write("soldierDamage", _soldierDamage);
	if (!_specialMessage.empty())
		writer.write("specialMessage", _specialMessage);
	if (!_newResearchNotifications.empty())
		writer.write("newResearchNotifications", _newResearchNotifications);
	if (_containedAlienStudiesUnblocked)
		writer.write("containedAlienStudiesUnblocked", _containedAlienStudiesUnblocked);
	if (!_researchArticleIds.empty())
		writer.write("researchArticleIds", _researchArticleIds);
	if (!_promotions.empty())
	{
		auto promotionsWriter = writer["promotions"];
		promotionsWriter.setAsSeq();
		for (const auto& promotion : _promotions)
		{
			auto entryWriter = promotionsWriter.write();
			entryWriter.setAsMap();
			entryWriter.write("soldierId", promotion.soldierId);
			entryWriter.write("role", static_cast<int>(promotion.role));
			entryWriter.write("previousRank", promotion.previousRank);
			entryWriter.write("newRank", promotion.newRank);
		}
	}
	if (!_soldierStats.empty())
	{
		auto statsWriter = writer["soldierStats"];
		statsWriter.setAsSeq();
		for (const auto& soldierStats : _soldierStats)
		{
			auto entryWriter = statsWriter.write();
			entryWriter.setAsMap();
			entryWriter.write("name", soldierStats.first);
			entryWriter.write("stats", soldierStats.second);
		}
	}
}

void CovertOperationResults::addPromotion(const RolePromotion& promotion)
{
	const auto existing = std::find_if(_promotions.begin(), _promotions.end(),
		[&promotion](const RolePromotion& candidate)
		{
			return candidate.soldierId == promotion.soldierId && candidate.role == promotion.role;
		});
	if (existing == _promotions.end())
	{
		_promotions.push_back(promotion);
	}
}

/**
 * Return a string describing CovertOperation success odds.
 * @par chance is input to calculate chance name.
 * @return a string describing CovertOperation success odds.
 */
std::string CovertOperation::getOddsName()
{
	if (_successChance > 100)
		return ("STR_GREAT");
	else if (_successChance > 70)
		return ("STR_GOOD");
	else if (_successChance > 50)
		return ("STR_AVERAGE");
	else if (_successChance > 25)
		return ("STR_POOR");
	else if (_successChance > 0)
		return ("STR_VERY_LOW");
	else
		return ("STR_NONE");

}

/**
 * Return a string describing approximate time before operation results.
 * @return a string describing time left.
 */
std::string CovertOperation::getTimeLeftName()
{
	int time = _cost - _spent;
	if (time > 45 * 24)
		return ("STR_SEVERAL_MONTHS");
	else if (time > 20 * 24)
		return ("STR_A_MONTH");
	else if (time > 10 * 24)
		return ("STR_SEVERAL_WEEKS");
	else if (time > 6 * 24)
		return ("STR_WEEK");
	else
		return ("STR_SEVERAL_DAYS");
}

/**
* Handle Covert Operation daily logic.
* @param engine game engine.
* @param globe
*/
bool CovertOperation::think(Game& engine, const Globe& globe)
{
	const Mod& mod = *engine.getMod();
	SavedGame& save = *engine.getSavedGame();
	if (_over)
	{
		return false;
	}
	// Results are persisted before any optional battlescape is created. Never
	// roll them a second time after load: while combat is active there is nothing
	// to report, and afterwards the stored strategic result must be shown as-is.
	if (_results || _hasBattlescapeResolve)
	{
		return !_inBattlescape;
	}
	if (_cancelled)
	{
		return true;
	}
	for (const auto& research : _rule->getDisabledByResearch())
	{
		if (save.isResearched(research, false))
		{
			// Cancellation is a distinct terminal outcome: do not advance time,
			// roll success, apply side effects, or erase from the active vector here.
			_cancelled = true;
			return true;
		}
	}

	// are we there yet?
	if (_spent < _cost)
	{
		++_spent;
	}
	if (_spent < _cost)
	{
		//should we spawn ongoing event?
		std::string progressEvent = _rule->chooseProgressEvent();
		if (!progressEvent.empty())
		{
			if (!_rule->getRepeatProgressEvent() && _progressEventSpawned)
				return false;
			else
			{
				if (RNG::percent(_rule->getProgressEventChance()))
				{
					_progressEventSpawned = save.spawnEvent(mod.getEvent(progressEvent));
				}
			}
		}
		return false;
	}

	// ok, the time has come to resolve covert operation
	GameDifficulty diff = save.getDifficulty(); //first, we understand values based on game difficulty
	int critFailCoef = 45;
	int woundOdds = 20;
	int deathOdds = 10;
	switch (diff)
	{
	case OpenXcom::DIFF_BEGINNER:
		critFailCoef += 5;
		woundOdds -= 5;
		deathOdds -= 8;
		break;
	case OpenXcom::DIFF_EXPERIENCED:
		critFailCoef += 2;
		woundOdds -= 2;
		deathOdds -= 4;
		break;
	case OpenXcom::DIFF_VETERAN:
		break;
	case OpenXcom::DIFF_GENIUS:
		critFailCoef -= 2;
		woundOdds += 5;
		deathOdds += 2;
		break;
	case OpenXcom::DIFF_SUPERHUMAN:
		critFailCoef -= 5;
		woundOdds += 10;
		deathOdds += 10;
		break;
	}
	//now, we roll for operation results and init some vars
	int roll = RNG::generate(0, 99);
	_finishedResult = _successChance > roll;
	bool criticalFail = roll > (_successChance + critFailCoef);
	int score = 0;
	int loyalty = 0;
	int funds = 0;
	std::string eventName;
	std::vector<std::string> researchList;
	std::map<const RuleItem*, int> resultItems;
	const RuleAlienMission* missionRules = nullptr;
	std::string deploymentName;
	std::map<std::string, int> reputationScore;

	_results = new CovertOperationResults(this->getOperationName(), _finishedResult, "0"); //#FINNIKTODO date
	if (_finishedResult) //define success operation results
	{
		score = _rule->getSuccessScore();
		loyalty = _rule->getSuccessLoyalty();
		funds = _rule->getSuccessFunds();
		researchList = _rule->getSuccessResearchList();
		eventName = _rule->getSuccessEvent();
		reputationScore = _rule->getSuccessReputationScoreList();
		missionRules = _rule->chooseGenSuccessMission();
		deploymentName = _rule->chooseGenInstantSuccessDeploymentType();
		if (!_rule->getSuccessItems().empty())
		{
			int totalWeight = 0;
			for (const auto& itemSet : _rule->getSuccessItems())
			{
				totalWeight += itemSet.first;
			}

			int runningTotal = 0;
			int itemSetRoll = RNG::generate(1, totalWeight);
			for (const auto& itemSet : _rule->getSuccessItems())
			{
				runningTotal += itemSet.first;
				if (runningTotal >= itemSetRoll)
				{
					for (const auto& i : itemSet.second)
					{
						resultItems[i.first] += i.second;
					}
					break; // break outer loop
				}
			}
		}
	}
	else //define failure operation results
	{
		score = _rule->getFailureScore();
		if (criticalFail) score = score - 300;
		loyalty = _rule->getFailureLoyalty();
		funds = _rule->getFailureFunds();
		researchList = _rule->getFailureResearchList();
		eventName = _rule->getFailureEvent();
		reputationScore = _rule->getFailureReputationScoreList();
		missionRules = _rule->chooseGenFailureMission();
		deploymentName = _rule->chooseGenInstantTrapDeploymentType();
		if (!_rule->getFailureItems().empty())
		{
			int totalWeight = 0;
			for (const auto& itemSet : _rule->getFailureItems())
			{
				totalWeight += itemSet.first;
			}
			int runningTotal = 0;
			int itemSetRoll = RNG::generate(1, totalWeight);
			for (const auto& itemSet : _rule->getFailureItems())
			{
				runningTotal += itemSet.first;
				if (runningTotal >= itemSetRoll)
				{
					for (const auto& i : itemSet.second)
					{
						resultItems[i.first] += i.second;
					}
					break; // break outer loop
				}
			}
		}
	}
	// lets process operation results
	if (score != 0)
	{
		save.addResearchScore(score);
		_results->addScore(score);
		engine.getGeoscapeState()->updateLoyalty(score, XCOM_GEOSCAPE);
	}

	if (loyalty != 0)
	{
		engine.getGeoscapeState()->updateLoyalty(loyalty, ABSOLUTE_COEF);
	}

	if (funds != 0)
	{
		save.setFunds(save.getFunds() + funds);
		_results->addFunds(funds);
	}

	if (!resultItems.empty())
	{
		for (auto& addItems : resultItems)
		{
			this->getItems()->addItem(addItems.first, addItems.second);
			_results->addItem(addItems.first->getType(), addItems.second);
		}
	}

	if (!eventName.empty())
	{
		save.spawnEvent(mod.getEvent(eventName));
	}

	if (!researchList.empty())
	{
		std::vector<const RuleResearch*> possibilities;

		for (const auto& rName : researchList)
		{
			const RuleResearch* rRule = mod.getResearch(rName, true);
			if (!save.isResearched(rRule, false))
			{
				possibilities.push_back(rRule);
			}
		}

		if (!possibilities.empty())
		{
			const RuleResearch* eventResearch = possibilities.at(0);
			std::vector<RuleResearch*> researchBefore;
			save.getAvailableResearchProjects(researchBefore, &mod, _base);
			ResearchDiscoveryResult discovery = save.completeResearchDiscovery(
				eventResearch, &mod, _base, true, true, false);
			std::vector<std::string> newlyDiscoveredResearchIds;
			for (const auto* discovered : discovery.newlyDiscovered)
			{
				newlyDiscoveredResearchIds.push_back(discovered->getName());
			}
			if (GeoscapeState* geoscape = engine.getGeoscapeState())
			{
				_results->setContainedAlienStudiesUnblocked(
					geoscape->hasNewlyUnblockedContainedAlienStudies(
						newlyDiscoveredResearchIds));
			}
			const auto articleIds = Ufopaedia::getResearchArticleIds(
				&save, &mod, discovery);
			_results->setResearchArticleIds(articleIds);
			_results->setNewResearchNotifications(
				save.getNewlyAvailableResearchIds(researchBefore, &mod, _base));
			_researchName = articleIds.empty() ? std::string() : articleIds.front();
			_results->setSpecialMessage("STR_NEW_DATA_ACQUIRED");
		}
	}

	// Custom counters are updated at strategic resolution, before any optional
	// tactical deployment. Increases are applied first, matching research side
	// effect ordering when both fields target the same counter.
	for (const auto& counter : _rule->getIncreaseCounter())
	{
		save.increaseCustomCounter(counter, _rule->getCounterValue());
	}
	for (const auto& counter : _rule->getDecreaseCounter())
	{
		save.decreaseCustomCounter(counter, _rule->getCounterValue());
	}

	if (!reputationScore.empty())
	{
		for (const auto& i : reputationScore)
		{
			for (auto& j : save.getDiplomacyFactions())
			{
				std::string factionName = j->getRules()->getName();
				std::string lookingName = i.first;
				if (factionName == lookingName)
				{
					j->updateReputationScore(i.second);
					_results->addReputation(factionName, i.second);
					break;
				}
			}
		}
	}

	if (!_rule->getRequiredItemList().empty())
	{
		bool removeItems = false;

		if (_finishedResult)
		{
			removeItems = _rule->getRemoveRequiredItemsOnSuccess();
		}
		else
		{
			removeItems = _rule->getRemoveRequiredItemsOnFailure();
		}

		if (removeItems)
		{
			auto items = _rule->getRequiredItemList();
			for (auto &item : items)
			{
				_items->removeItem(item.first, item.second);
			}
		}
	}

	auto addSoldiers = _rule->getSoldiersToAdd();
	for (auto &kv : addSoldiers)
	{
		for (int i = 0; i < kv.second; ++i)
		{
			Soldier* soldier = mod.genSoldier(&save, kv.first);
			_base->getSoldiers()->push_back(soldier);
			soldier->setCovertOperation(this);
			soldier->setJustSaved(true);
		}
	}

	if (missionRules)
	{
		engine.getGeoscapeState()->spawnAlienMission(missionRules, globe, _base);
	}

	if (!_rule->getSpecialRule().empty())//processing of yet hardcoded fta story arc
	{
		std::string specRule = _rule->getSpecialRule();
		if (specRule == "STR_REGIONAL_HQ_DISCOVERY")
		{
			for (auto& i : *save.getAlienBases())
			{
				auto baseName = i->getDeployment()->getType();
				if (baseName == "STR_INITIAL_REGIONAL_HQ")
				{
					i->setDiscovered(true);

					_results->setSpecialMessage("STR_REGIONAL_HQ_FOUND");
				}
			}
		}
	}

	//simulating operation
	if (this->getRules()->getDanger() > 0)
	{
		backgroundSimulation(engine, _finishedResult, criticalFail, woundOdds, deathOdds);
	}

	if (!deploymentName.empty())
	{
		bool process = true;
		if (!_finishedResult) //if operation failed lets see if we get into a trap!
		{
			int trapRoll = _rule->getTrapChance();
			if (criticalFail && trapRoll > 0) trapRoll = trapRoll + 35;
			process = RNG::generate(0, 99) < trapRoll;
		}
		if (process && !_inBattlescape)
		{//oh, boy, we are going to generate battlescape to resolve our covert operation!
			AlienDeployment* deployment = mod.getDeployment(deploymentName);
			if (deployment != 0)
			{
				SavedBattleGame* bgame = new SavedBattleGame(engine.getMod(), engine.getLanguage());
				bgame->setMissionType(deploymentName);
				save.setBattleGame(bgame);
				BattlescapeGenerator bgen(&engine);
				bgen.setCovertOperation(this);
				_hasBattlescapeResolve = true;
				_inBattlescape = true;
				bgen.run();
				engine.pushState(new BriefingState());
			}
			else
			{
				throw Exception("No deployment defined for operation: " + this->getOperationName() +
					" ! It is reffering to alienDeployment named: " + deploymentName);
			}
		}
	}
	if (!_inBattlescape) // no tactical resolution was started, so return the operation inventory to its base
	{
		returnItemsToBase();
	}

	// A tactical operation is reported after returning to Geoscape. This keeps
	// the notification reproducible after saving and reloading the battle.
	return !_inBattlescape;
}

/**
* Handle background simulation for Covert Operation.
* Award exp to soldiers, simulate critical fail repercussion.
* @param engine game engine.
* @param operationResult and criticalFail operation results.
* @param woundOdds and deathOdds stats from game difficulty.
*/
void CovertOperation::backgroundSimulation(Game& engine, bool operationResult, bool criticalFail, int woundOdds, int deathOdds)
{
	SavedGame& save = *engine.getSavedGame();

	int danger = this->getRules()->getDanger(); //only dangerous operations train battle stats

	//first, we calculate how much experience we can award for the operation (expRolls)
	int effCost = (int)ceil(this->getRules()->getCosts() / 15.0);
	int expRolls = (effCost * engine.getMod()->getCovertOpsExpFactor() / 100) + 1;
	expRolls += RNG::generate(-2, 2); //add more random
	if (save.getDifficulty() == DIFF_SUPERHUMAN)
		expRolls -= RNG::generate(0, 5);

	//processing soldiers change before returning home
	std::vector<Soldier*> soldiersToKill;
	int operationSoldierN = 0;
	std::vector<Soldier*> soldiers = getSoldiers();
	bool engagement = false;
	int avgStealth = 0;
	if (danger > 0 && !soldiers.empty())
	{
		for (auto s : soldiers)
		{
			avgStealth += s->getCurrentStats()->stealth;
		}

		avgStealth /= soldiers.size();
		int engChance = danger * 5 - avgStealth;
		if (!operationResult)
		{
			engChance *= 2;
		}
		engagement = RNG::percent(engChance);
	}

	for (auto& soldier : soldiers)
	{
		bool dead = false;
		int wound = 0;
		int soldierExpRolls = expRolls;
		++operationSoldierN;
		UnitStats* exp = new UnitStats();

		if (danger > 0)
		{
			int damage = 0;
			int damageRolls = danger * (ceil(save.getDifficultyCoefficient() / 2.0) + 1);
			if (criticalFail)
			{
				damageRolls *= RNG::generate(1, 3);
			}

			for (size_t j = 0; j < damageRolls; j++)
			{
				if (RNG::generate(0, 99) < woundOdds && RNG::generate(0, 99) > soldier->getStatsWithAllBonuses()->psiSkill)
				{
					++wound;
				}
			}
			if (wound > 0)
			{
				damage = RNG::generate(wound, wound * 6);
			}
			if (damage < soldier->getCurrentStats()->health)
			{
				soldier->setWoundRecovery(damage);
				_results->addSoldierDamage(soldier->getName(), damage);
			}
			else
			{
				dead = true; //ouch, too much damage rolled!
			}

			if (!dead && criticalFail)
			{ //OMG, Finger of Death for soldier on critical failed operation!!!
				dead = RNG::generate(0, 99) < deathOdds + ceil(danger / 3.0);
			}
			if (dead)
			{
				//Check for divine protection
				int protection = soldier->getRoleRank(ROLE_AGENT) - 2;
				//lets add save if we have psi. Btw, there is a place for additional perks
				if (_hasPsi)
					protection += 3;
				int requiredProtection = RNG::generate(1, 7 + save.getDifficultyCoefficient());
				if (criticalFail)
					requiredProtection += 3;
				if (requiredProtection > protection)
				{ //RIP...
					soldiersToKill.push_back(soldier);
				}
				else
				{
					dead = false;
					if (soldier->getStatsWithAllBonuses()->bravery <= 20 || RNG::percent(5))
						exp->bravery++;
				}
			}
		}

		UnitStats origStat = *soldier->getCurrentStats();
		//soldiers can improve stats based on virtual experience they take
		if (!dead && soldierExpRolls > 0 && soldier->canGainExperience())
		{
			const UnitStats caps = soldier->getRules()->getStatCaps();
			// at first, we process combat stats
			if (engagement)
			{
				//TU and Energy
				if (origStat.tu < caps.tu)
					exp->tu += RNG::generate(-3, soldierExpRolls);
				if (origStat.stamina < caps.stamina)
					exp->stamina = RNG::generate(-3, soldierExpRolls);

				//other stats would be rolled to be improved
				int statID = 0;
				int expGain = 0;
				for (size_t j = 0; j < (size_t)soldierExpRolls; j++)
				{
					statID = RNG::generate(0, 6);  //choose non-psionic combat stat
					expGain = RNG::generate(1, 4); //choose how many experience it would be
					if (expGain == 4)
						expGain = 1;
					switch (statID)
					{
					case 0:
						if (origStat.health < caps.health)
							exp->health += expGain;
						break;
					case 1:
						if (origStat.bravery < caps.bravery && !exp->bravery)
						{
							int braveryRoll = 1;
							if (wound > 0)
							{
								braveryRoll += 1;
							}
							if (RNG::generate(0, 14) > braveryRoll)
								exp->bravery += expGain;
						}
						break;
					case 2:
						if (origStat.reactions < caps.reactions)
							exp->reactions += expGain;
						break;
					case 3:
						if (origStat.firing < caps.firing)
							exp->firing += expGain;
						break;
					case 4:
						if (origStat.throwing < caps.throwing)
							exp->throwing += expGain;
						break;
					case 5:
						if (origStat.melee < caps.melee)
							exp->melee += expGain;
						break;
					case 6:
						if (origStat.strength < caps.strength)
							exp->strength += expGain;
						break;
					default:
						break;
					}
				}
				soldierExpRolls /= 2;
			}
			else if (danger > 0 && !engagement)
			{
				exp->stealth += RNG::generate(0, 4);
				exp->perception += RNG::generate(0, 2);
			}

			const auto& cats = _rule->getCategories();
			int categoryCount = static_cast<int>(cats.size());
			int categoryBaseRolls = categoryCount > 0 ? soldierExpRolls / categoryCount : 0;
			int categoryRemainder = categoryCount > 0 ? soldierExpRolls % categoryCount : 0;
			int extraRollStart = categoryRemainder > 0 ? RNG::generate(0, categoryCount - 1) : 0;
			for (int categoryIndex = 0; categoryIndex < categoryCount; ++categoryIndex)
			{
				int categoryExpRolls = categoryBaseRolls;
				int relativeIndex = (categoryIndex - extraRollStart + categoryCount) % categoryCount;
				if (relativeIndex < categoryRemainder)
					++categoryExpRolls;
				if (categoryExpRolls <= 0)
					continue;

				const auto& category = cats[categoryIndex];
				if (category == "STR_INVESTIGATION")
				{
					exp->investigation += RNG::generate(1, std::max(2, categoryExpRolls));
					exp->perception += RNG::generate(0, std::max(1, categoryExpRolls / 2));
					exp->interrogation += RNG::generate(0, std::max(1, categoryExpRolls / 2));
				}
				else if (category == "STR_INFILTRATION")
				{
					exp->stealth += RNG::generate(1, std::max(2, categoryExpRolls));
					exp->perception += RNG::generate(0, std::max(1, categoryExpRolls / 2));
					exp->interrogation += RNG::generate(0, std::max(1, categoryExpRolls / 2));
				}
				else if (category == "STR_NEGOTIATION")
				{
					exp->charisma += RNG::generate(1, std::max(3, static_cast<int>(categoryExpRolls * 1.5)));
				}
				else if (category == "STR_DECEPTION")
				{
					exp->deception += RNG::generate(1, std::max(3, static_cast<int>(categoryExpRolls * 1.5)));
				}
			}
		}

		if (soldier->canGainExperience() && soldier->isRookieAgent())
		{
			soldier->addExperience(ROLE_AGENT, RNG::generate(5, 10), getOperationName()); // extra role experience for rookie agents
			soldier->setRookieAgent(false);
		}

		// Ordinary covert operations must not train psionic stats or mana.
		// Dedicated psionics mechanics should handle these stats separately.
		exp->psiStrength = 0;
		exp->psiSkill = 0;
		exp->mana = 0;

		if (soldier->canGainExperience())
		{
			soldier->improvePrimaryStats(exp, ROLE_AGENT);
			// Also improve secondary stats.
			int rate = 0;
			soldier->getCurrentStatsEditable()->tu += Soldier::improveStat(exp->tu, rate, false);
			soldier->getCurrentStatsEditable()->stamina += Soldier::improveStat(exp->stamina, rate, false);
		}

		UnitStats improvement = *soldier->getCurrentStats() - origStat;
		_results->addSoldierImprovement(soldier->getName(), improvement);

		if (!dead && wound != 0)
		{
			soldier->setReturnToTrainingWhenOperationOver(NONE);
		}
		RolePromotion promotion;
		if (!dead && soldier->rolePromoteSoldier(ROLE_AGENT, &promotion))
		{
			_results->addPromotion(promotion);
		}
		delete exp;
	}

	//if needed kill soldiers from doomed list
	if (!soldiersToKill.empty())
	{
		int it = 0;
		int killN = soldiersToKill.size();
		bool loneSaved = false;
		int chosenID = 0;
		if (killN == operationSoldierN) //we want to keep at least one soldier alive, that would say operation failure results
		{
			loneSaved = true;
			chosenID = RNG::generate(0, killN - 1);
		}
		for (auto& j : soldiersToKill)
		{
			if (loneSaved && chosenID == it)
			{
				int health = j->getCurrentStats()->health;
				int genDamage = (int)RNG::generate(health * 0.5, health * 0.9);
				j->setWoundRecovery(genDamage);
				_results->addSoldierDamage(j->getName(), genDamage);
			}
			else
			{
				_results->addSoldierDamage(j->getName(), -10);
				save.killSoldier(true, j); //RIP
			}
			++it;
		}
	}
}

/**
 * Returns all operation equipment to its base exactly once.
 */
void CovertOperation::returnItemsToBase()
{
	for (auto& item : *_items->getContents())
	{
		_base->getStorageItems()->addItem(item.first, item.second);
	}
	_items->clear();
}

/**
 * Makes sure we over operation
 */
void CovertOperation::finishOperation()
{
	if (_over)
	{
		return;
	}
	if (_cancelled)
	{
		returnItemsToBase();
	}
	auto soldiers = getSoldiers();
	for (auto& soldier : soldiers)
	{
		//remove soldier from operation
		soldier->setCovertOperation(0);
		soldier->setJustSaved(false);

		//if soldier was not hurt we return him or her to training, if settings allows it
		if (soldier->getHealthMissing() == 0)
		{
			ReturnToTrainings trainings = soldier->getReturnToTrainingsWhenOperationOver();
			if (trainings == MARTIAL_TRAINING || trainings == BOTH_TRAININGS)
			{
				if (_base->getUsedTraining() < _base->getAvailableTraining())
				{
					soldier->setTraining(true);
				}
			}
			if (trainings == PSI_TRAINING || trainings == BOTH_TRAININGS)
			{
				if (_base->getUsedPsiLabs() < _base->getAvailablePsiLabs())
				{
					soldier->setPsiTraining(true);
				}
			}
		}
		soldier->setReturnToTrainingWhenOperationOver(NONE);
	}
	_over = true;
}

}
