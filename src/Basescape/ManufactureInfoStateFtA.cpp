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
#include "ManufactureInfoStateFtA.h"
#include <algorithm>
#include "../Interface/Window.h"
#include "../Interface/TextButton.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Interface/ArrowButton.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleManufacture.h"
#include "../Savegame/Base.h"
#include "../Savegame/Production.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/ItemContainer.h"
#include "../Savegame/Soldier.h"
#include "../Engine/Timer.h"
#include "../Menu/ErrorMessageState.h"
#include "../Mod/RuleInterface.h"
#include "../Basescape/ManufactureAllocateEngineersState.h"

#include <climits>

namespace OpenXcom
{

/**
 * Initializes all elements in the Production settings screen (new Production).
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param item The RuleManufacture to produce.
 */
ManufactureInfoStateFtA::ManufactureInfoStateFtA(Base *base, RuleManufacture *item) : _base(base), _item(item), _production(0)
{
	_newProject = true;
	_unitsToProduce = 1;
	_producedItems = 0;
	_infiniteProduction = false;
	buildUi();
}

/**
 * Initializes all elements in the Production settings screen (modifying Production).
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param production The Production to modify.
 */
ManufactureInfoStateFtA::ManufactureInfoStateFtA(Base *base, Production *production) : _base(base), _item(0), _production(production)
{
	_newProject = false;
	_unitsToProduce = _production->getAmountTotal();
	_producedItems = _production->getAmountProduced();
	_infiniteProduction = _production->getInfiniteAmount();
	for (auto* soldier : *_base->getSoldiers())
	{
		if (soldier->getProductionProject() == _production)
		{
			_engineers.insert(soldier);
		}
	}
	buildUi();
}

/**
 * Builds screen User Interface.
 */
void ManufactureInfoStateFtA::buildUi()
{
	_screen = false;

	_window = new Window(this, 320, 160, 0, 20, POPUP_BOTH);
	_txtTitle = new Text(302, 17, 9, 30);
	_btnOk = new TextButton(136, 16, 168, 155);
	_btnStop = new TextButton(136, 16, 16, 155);
	_txtAvailableEngineer = new Text(160, 9, 16, 49);
	_txtAvailableSpace = new Text(160, 9, 16, 59);
	_txtAllocatedEngineers = new Text(150, 9, 16, 69);
	_txtUnitCost = new Text(143, 9, 168, 69);
	_txtUnitToProduce = new Text(112, 34, 168, 78);
	_txtUnitUp = new Text(90, 9, 192, 118);
	_txtUnitDown = new Text(90, 9, 192, 138);
	_btnUnitUp = new ArrowButton(ARROW_BIG_UP, 13, 14, 284, 114);
	_btnUnitDown = new ArrowButton(ARROW_BIG_DOWN, 13, 14, 284, 136);
	_txtTodo = new Text(40, 16, 280, 88);
	_txtAvgEfficiency = new Text(143, 9, 168, 50);
	_txtAvgDiligence = new Text(143, 9, 168, 59);
	_btnAllocateEngineers = new TextButton(110, 16, 16, 79);
	_lstEngineers = new TextList(116, 56, 16, 97);

	_surfaceUnits = new InteractiveSurface(160, 150, 160, 25);
	_surfaceUnits->onMouseClick((ActionHandler)&ManufactureInfoStateFtA::handleWheelUnit, 0);

	// Set palette
	setInterface("manufactureInfo");

	add(_surfaceUnits);
	add(_window, "window", "manufactureInfo");
	add(_txtTitle, "text", "manufactureInfo");
	add(_txtAvailableEngineer, "text", "manufactureInfo");
	add(_txtAvailableSpace, "text", "manufactureInfo");
	add(_txtAllocatedEngineers, "text", "manufactureInfo");
	add(_txtUnitCost, "text", "manufactureInfo");
	add(_txtUnitToProduce, "text", "manufactureInfo");
	add(_txtTodo, "text", "manufactureInfo");
	add(_txtUnitUp, "text", "manufactureInfo");
	add(_txtUnitDown, "text", "manufactureInfo");
	add(_btnUnitUp, "button1", "manufactureInfo");
	add(_btnUnitDown, "button1", "manufactureInfo");
	add(_btnOk, "button2", "manufactureInfo");
	add(_btnStop, "button2", "manufactureInfo");
	add(_txtAvgEfficiency, "text", "manufactureInfo");
	add(_txtAvgDiligence, "text", "manufactureInfo");
	add(_btnAllocateEngineers, "button2", "manufactureInfo");
	add(_lstEngineers, "list", "manufactureInfo");

	centerAllSurfaces();

	setWindowBackground(_window, "manufactureInfo");

	_txtTitle->setText(tr(getManufactureRules()->getName()));
	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);

	_txtTodo->setBig();

	_txtUnitToProduce->setText(tr("STR_UNITS_TO_PRODUCE"));
	_txtUnitToProduce->setBig();
	_txtUnitToProduce->setWordWrap(true);
	_txtUnitToProduce->setVerticalAlign(ALIGN_BOTTOM);

	_btnUnitUp->onMousePress((ActionHandler)&ManufactureInfoStateFtA::moreUnitPress);
	_btnUnitUp->onMouseRelease((ActionHandler)&ManufactureInfoStateFtA::moreUnitRelease);
	_btnUnitUp->onMouseClick((ActionHandler)&ManufactureInfoStateFtA::moreUnitClick, 0);

	_btnUnitDown->onMousePress((ActionHandler)&ManufactureInfoStateFtA::lessUnitPress);
	_btnUnitDown->onMouseRelease((ActionHandler)&ManufactureInfoStateFtA::lessUnitRelease);
	_btnUnitDown->onMouseClick((ActionHandler)&ManufactureInfoStateFtA::lessUnitClick, 0);

	_txtUnitUp->setText(tr("STR_INCREASE_UC"));

	_txtUnitDown->setText(tr("STR_DECREASE_UC"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ManufactureInfoStateFtA::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ManufactureInfoStateFtA::btnOkClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&ManufactureInfoStateFtA::btnCancelClick, Options::keyCancel);

	if (!_newProject)
	{
		if (getManufactureRules()->getRefund())
		{
			_btnStop->setText(tr("STR_REFUND_PRODUCTION"));
		}
		else
		{
			_btnStop->setText(tr("STR_STOP_PRODUCTION"));
		}
	}
	else
	{
		_btnStop->setText(tr("STR_CANCEL_UC"));
	}
	_btnStop->onMouseClick((ActionHandler)&ManufactureInfoStateFtA::btnStopClick);

	refreshAssignedEngineers();

	_btnAllocateEngineers->setText(tr("STR_ALLOCATE_ENGINEERS"));
	_btnAllocateEngineers->onMouseClick((ActionHandler)&ManufactureInfoStateFtA::btnAllocateClick, 0);

	_lstEngineers->setColumns(2, 116, 32);
	_lstEngineers->setAlign(ALIGN_LEFT, 0);
	_lstEngineers->setBackground(_window);
	//_lstEngineers->setMargin(2);
	_lstEngineers->setWordWrap(true);

	_timerMoreUnit = new Timer(250);
	_timerLessUnit = new Timer(250);

	_timerMoreUnit->onTimer((StateHandler)&ManufactureInfoStateFtA::onMoreUnit);
	_timerLessUnit->onTimer((StateHandler)&ManufactureInfoStateFtA::onLessUnit);

	if (getManufactureRules()->getProducedCraft())
	{
		_unitsToProduce = 1;
		_infiniteProduction = false;
		_btnUnitUp->setVisible(false);
		_btnUnitDown->setVisible(false);
		_txtUnitUp->setVisible(false);
		_txtUnitDown->setVisible(false);
		_surfaceUnits->setVisible(false);
	}
}

/**
 * Frees up memory that's not automatically cleaned on exit
 */
ManufactureInfoStateFtA::~ManufactureInfoStateFtA()
{
	delete _timerMoreUnit;
	delete _timerLessUnit;
}

void ManufactureInfoStateFtA::init()
{
	State::init();
	refreshAssignedEngineers();
	fillEngineersList(0);
}

void ManufactureInfoStateFtA::fillEngineersList(size_t scrl)
{
	_lstEngineers->clearList();
	for (auto e : _engineers)
	{
		std::ostringstream ss;
		ss << e->getRoleRank(ROLE_ENGINEER);
		_lstEngineers->addRow(2, e->getName().c_str(), ss.str().c_str());
		//_lstEngineers->addRow(1, e->getName().c_str());
	}
}

const RuleManufacture* ManufactureInfoStateFtA::getManufactureRules()
{
	if (_item != nullptr)
	{
		return _item;
	}
	else
	{
		return _production->getRules();
	}
}

/**
 * Stops this Production. Returns to the previous screen.
 * @param action A pointer to an Action.
 */
void ManufactureInfoStateFtA::btnStopClick(Action *)
{
	if (!_newProject)
	{
		if (getManufactureRules()->getRefund())
		{
			_production->refundItem(_base, _game->getSavedGame(), _game->getMod());
		}
		_base->removeProduction(_production);
	}
	exitState();
}

void ManufactureInfoStateFtA::btnCancelClick(Action *)
{
	// Engineer selection and quantity changes are staged until OK.
	exitState();
}

/**
 * Starts this Production (if new). Returns to the previous screen.
 * @param action A pointer to an Action.
 */
void ManufactureInfoStateFtA::btnOkClick(Action *)
{
	_base->prepareSoldierStatsWithBonuses();
	int efficiency = 0;
	for (auto* engineer : _engineers)
	{
		efficiency += engineer->getStatsWithAllBonuses()->efficiency;
	}
	if (!_engineers.empty())
	{
		efficiency /= static_cast<int>(_engineers.size());
	}

	Production* target = _production;
	if (_newProject)
	{
		target = new Production(_item, 1);
		target->setEfficiency(efficiency);
		target->setAmountTotal(getManufactureRules()->getProducedCraft() ? 1 : _unitsToProduce);
		target->setInfiniteAmount(getManufactureRules()->getProducedCraft() ? false : _infiniteProduction);
		_base->addProduction(target);

		const productionProgress_e status = target->startItem(_base, _game->getSavedGame(), _game->getMod());
		if (status != PROGRESS_NOT_COMPLETE)
		{
			std::string message;
			switch (status)
			{
			case PROGRESS_NOT_ENOUGH_MONEY:
				message = tr("STR_NOT_ENOUGH_MONEY_TO_PRODUCE_ITEM_AT_BASE").arg(tr(getManufactureRules()->getName())).arg(_base->getName());
				break;
			case PROGRESS_NOT_ENOUGH_LIVING_SPACE:
				message = tr("STR_NOT_ENOUGH_LIVING_SPACE_AT_BASE").arg(tr(getManufactureRules()->getName())).arg(_base->getName());
				break;
			case PROGRESS_NOT_ENOUGH_BASE_FUNCTIONS:
				message = tr("STR_REQUIRED_BASE_FUNCTIONS_UNAVAILABLE_AT_BASE").arg(tr(getManufactureRules()->getName())).arg(_base->getName());
				break;
			default:
				message = tr("STR_NOT_ENOUGH_SPECIAL_MATERIALS_TO_PRODUCE_ITEM_AT_BASE").arg(tr(getManufactureRules()->getName())).arg(_base->getName());
				break;
			}
			_base->removeProduction(target);
			_game->pushState(new ErrorMessageState(message, _palette, _game->getMod()->getInterface("basescape")->getElement("errorMessage")->color, "BACK17.SCR", _game->getMod()->getInterface("basescape")->getElement("errorPalette")->color));
			return;
		}
	}
	else
	{
		target->setEfficiency(efficiency);
		target->setAmountTotal(getManufactureRules()->getProducedCraft() ? 1 : _unitsToProduce);
		target->setInfiniteAmount(getManufactureRules()->getProducedCraft() ? false : _infiniteProduction);
	}

	for (auto* soldier : *_base->getSoldiers())
	{
		if (soldier->getProductionProject() == target && _engineers.find(soldier) == _engineers.end())
		{
			soldier->setProductionProject(nullptr);
		}
	}
	for (auto* engineer : _engineers)
	{
		if (engineer->getProductionProject() != target)
		{
			engineer->clearBaseDuty();
			engineer->setProductionProject(target);
		}
	}
	_production = target;
	exitState();
}

void ManufactureInfoStateFtA::btnAllocateClick(Action* action)
{
	_game->pushState(new ManufactureAllocateEngineersState(_base, this));
}

/**
 * Returns to the previous screen.
 */
void ManufactureInfoStateFtA::exitState()
{
	_game->popState();
	if (_item)
	{
		_game->popState();
	}
}

int ManufactureInfoStateFtA::calcAvgStat(bool check)
{
	double result = 0;
	if (!_engineers.empty())
	{
		
		for (auto s : _engineers)
		{
			if (check) //efficiency; im so sorry if you can see this =)
			{
				result += s->getStatsWithAllBonuses()->efficiency;
			}
			else
			{
				result += s->getStatsWithAllBonuses()->diligence;
			}
		}

		result /= _engineers.size();
	}
	return static_cast<int>(result);
}

/**
 * Updates display of assigned/available engineer/workshop space.
 */
void ManufactureInfoStateFtA::refreshAssignedEngineers()
{
	_base->prepareSoldierStatsWithBonuses();
	size_t freeEngineers = 0;
	auto recovery = _base->getSumRecoveryPerDay();
	for (auto* soldier : _base->getPersonnel(ROLE_ENGINEER))
	{
		bool isBusy = false;
		bool isFree = false;
		soldier->getCurrentDuty(_game->getLanguage(), recovery, isBusy, isFree);
		if (!isBusy && isFree && _engineers.find(soldier) == _engineers.end())
		{
			++freeEngineers;
		}
	}
	_txtAvailableEngineer->setText(tr("STR_ENGINEERS_AVAILABLE_UC").arg(freeEngineers));

	int additionalEngineerSpace = 0;
	for (auto* engineer : _engineers)
	{
		Production* current = engineer->getProductionProject();
		if (!current || current == _production)
		{
			++additionalEngineerSpace;
		}
	}
	_workSpace = _base->getFreeWorkshops(_production)
		- getManufactureRules()->getRequiredSpace()
		- additionalEngineerSpace;
	_txtAvailableSpace->setText(tr("STR_WORKSHOP_SPACE_AVAILABLE_UC").arg(_workSpace));

	std::ostringstream s4;
	s4 << ">" << Unicode::TOK_COLOR_FLIP;
	if (_infiniteProduction)
	{
		s4 << "∞";
	}
	else
	{
		s4 << _unitsToProduce;
	}
	_txtTodo->setText(s4.str());

	const int averageDiligence = calcAvgStat(false);
	const int averageEfficiency = calcAvgStat(true);
	_txtAllocatedEngineers->setText(tr("STR_ENGINEERS_ALLOCATED_UC").arg(_engineers.size()));
	_txtAvgDiligence->setText(tr("STR_AVERAGE_DILIGENCE_UC").arg(averageDiligence));
	_txtAvgEfficiency->setText(tr("STR_AVERAGE_EFFICIENCY_UC").arg(averageEfficiency));
	_txtAvgDiligence->setVisible(!_engineers.empty());
	_txtAvgEfficiency->setVisible(!_engineers.empty());

	const RuleManufacture* rules = getManufactureRules();
	if (rules->getFundsGain() > 0)
	{
		_txtUnitCost->setText(tr("STR_FUNDS_GAIN").arg(Unicode::formatFunding(rules->getFundsGain())));
	}
	else
	{
		const int64_t effectiveCost = Production::calculateUnitCost(rules->getManufactureCost(), averageEfficiency);
		_txtUnitCost->setText(tr("STR_COST_PER_UNIT_").arg(Unicode::formatFunding(effectiveCost)));
	}
}


/**
 * Adds given number of units to produce to the project if possible.
 * @param change How much we want to add.
 */
void ManufactureInfoStateFtA::moreUnit(int change)
{
	if (change <= 0 || getManufactureRules()->getProducedCraft())
	{
		return;
	}

	int units = _unitsToProduce;
	if (units == 1 && change > 1)
	{
		--change; // e.g. jump from 1 to 10, not to 11
	}
	change = std::min(INT_MAX - units, change);
	_unitsToProduce = units + change;
	refreshAssignedEngineers();
}

/**
 * Starts the timerMoreUnit.
 * @param action A pointer to an Action.
 */
void ManufactureInfoStateFtA::moreUnitPress(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT && _unitsToProduce < INT_MAX)
		_timerMoreUnit->start();
}

/**
 * Stops the timerMoreUnit.
 * @param action A pointer to an Action.
 */
void ManufactureInfoStateFtA::moreUnitRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerMoreUnit->setInterval(250);
		_timerMoreUnit->stop();
	}
}

/**
 * Increases the "units to produce", in the case of a right-click, to infinite, and 1 on left-click.
 * @param action A pointer to an Action.
 */
void ManufactureInfoStateFtA::moreUnitClick(Action *action)
{
	if (_infiniteProduction || getManufactureRules()->getProducedCraft())
	{
		return; // We can't increase over infinite :)
	}

	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		_infiniteProduction = true;
		refreshAssignedEngineers();
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		moreUnit(1);
	}
}

/**
 * Removes the given number of units to produce from the project if possible.
 * @param change How much we want to subtract.
 */
void ManufactureInfoStateFtA::lessUnit(int change)
{
	if (change <= 0 || getManufactureRules()->getProducedCraft())
	{
		return;
	}

	int units = _unitsToProduce;
	change = std::min(units - (_producedItems + 1), change);
	_unitsToProduce = units - change;
	refreshAssignedEngineers();
}

/**
 * Starts the timerLessUnit.
 * @param action A pointer to an Action.
 */
void ManufactureInfoStateFtA::lessUnitPress(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT) _timerLessUnit->start();
}

/**
 * Stops the timerLessUnit.
 * @param action A pointer to an Action.
 */
void ManufactureInfoStateFtA::lessUnitRelease(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		_timerLessUnit->setInterval(250);
		_timerLessUnit->stop();
	}
}

/**
 * Decreases the units to produce.
 * @param action A pointer to an Action.
 */
void ManufactureInfoStateFtA::lessUnitClick(Action *action)
{
	if (getManufactureRules()->getProducedCraft())
	{
		return;
	}
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT
	||  action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		bool wasInfinite = _infiniteProduction;
		_infiniteProduction = false;
		if (_unitsToProduce <= _producedItems)
		{ // So the produced item number is increased over the planned
			_unitsToProduce += 1;
			refreshAssignedEngineers();
			return;
		}
		if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
		{
			_unitsToProduce = _producedItems + 1;
			refreshAssignedEngineers();
			return;
		}
		if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
		{
			if (wasInfinite)
			{
				// when infinite amount is decreased by 1, set the amount to maximum possible considering current funds and store supplies
				_base->prepareSoldierStatsWithBonuses();
				int productionPossible = INT_MAX;
				const int64_t unitCost = Production::calculateUnitCost(
					getManufactureRules()->getManufactureCost(), calcAvgStat(true));
				if (unitCost > 0)
				{
					int64_t byFunds = _game->getSavedGame()->getFunds() / unitCost;
					productionPossible = static_cast<int>(std::min<int64_t>(productionPossible, byFunds));
				}
				for (auto &item : getManufactureRules()->getRequiredItems())
				{
					productionPossible = std::min(productionPossible, _base->getStorageItems()->getItem(item.first) / item.second);
				}
				for (auto &craft : getManufactureRules()->getRequiredCrafts())
				{
					productionPossible = std::min(productionPossible, _base->getCraftCountForProduction(craft.first) / craft.second);
				}
				productionPossible = std::max(0, productionPossible);

				const int currentItem = !_newProject && _production->hasCurrentItemStarted() ? 1 : 0;
				const int64_t newTotal = static_cast<int64_t>(_producedItems) + currentItem + productionPossible;
				_unitsToProduce = static_cast<int>(std::min<int64_t>(INT_MAX,
					std::max<int64_t>(_producedItems + 1, newTotal)));
				refreshAssignedEngineers();
				return;
			}
			lessUnit(1);
		}
	}
}

/**
 * Builds one more unit.
 */
void ManufactureInfoStateFtA::onMoreUnit()
{
	_timerMoreUnit->setInterval(50);
	moreUnit(1);
}

/**
 * Builds one less unit( if possible).
 */
void ManufactureInfoStateFtA::onLessUnit()
{
	_timerLessUnit->setInterval(50);
	lessUnit(1);
}

/**
 * Increases or decreases the Units to produce according the mouse-wheel used.
 * @param action A pointer to an Action.
 */
void ManufactureInfoStateFtA::handleWheelUnit(Action *action)
{
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP)
		moreUnit(Options::changeValueByMouseWheel);
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
		lessUnit(Options::changeValueByMouseWheel);
}

/**
 * Runs state functionality every cycle (used to update the timer).
 */
void ManufactureInfoStateFtA::think()
{
	State::think();
	_timerMoreUnit->think(this, 0);
	_timerLessUnit->think(this, 0);
}

}
