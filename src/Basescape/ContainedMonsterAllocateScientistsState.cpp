/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "ContainedMonsterAllocateScientistsState.h"

#include <algorithm>
#include "ContainedMonsterInfoState.h"
#include "SoldierInfoStateFtA.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/ComboBox.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseContainedMonster.h"
#include "../Savegame/Soldier.h"

namespace OpenXcom
{

ContainedMonsterAllocateScientistsState::ContainedMonsterAllocateScientistsState(
	Base* base, ContainedMonsterInfoState* detail) :
	_base(base), _detail(detail), _originalOrder(*base->getSoldiers())
{
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(148, 16, 164, 176);
	_txtTitle = new Text(247, 17, 16, 7);
	_txtCount = new Text(247, 9, 16, 24);
	_txtName = new Text(114, 9, 16, 32);
	_txtAssignment = new Text(84, 9, 122, 32);
	_cbxSortBy = new ComboBox(this, 148, 16, 8, 176, true);
	_lstScientists = new TextList(288, 128, 8, 40);

	setInterface("researchAllocateScientists");
	add(_window, "window", "researchAllocateScientists");
	add(_btnOk, "button", "researchAllocateScientists");
	add(_txtTitle, "text", "researchAllocateScientists");
	add(_txtCount, "text", "researchAllocateScientists");
	add(_txtName, "text", "researchAllocateScientists");
	add(_txtAssignment, "text", "researchAllocateScientists");
	add(_cbxSortBy, "button", "researchAllocateScientists");
	add(_lstScientists, "list", "researchAllocateScientists");
	centerAllSurfaces();
	setWindowBackground(_window, "researchAllocateScientists");

	_txtTitle->setBig();
	_txtTitle->setText(
		_detail->getMonster()->getNameAndDesignation(_game->getLanguage()));
	_txtTitle->setWordWrap(true);
	_txtTitle->setVerticalAlign(ALIGN_MIDDLE);
	_txtName->setText(tr("STR_NAME_UC"));
	_txtAssignment->setText(tr("STR_ASSIGNMENT"));
	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick(
		(ActionHandler)&ContainedMonsterAllocateScientistsState::btnOkClick);
	_btnOk->onKeyboardPress(
		(ActionHandler)&ContainedMonsterAllocateScientistsState::btnOkClick,
		Options::keyCancel);

	std::vector<std::string> sortOptions;
	sortOptions.push_back(tr("STR_ORIGINAL_ORDER"));
	_sortFunctors.push_back(nullptr);
#define PUSH_IN(strId, functor) \
	sortOptions.push_back(tr(strId)); \
	_sortFunctors.push_back(new SortFunctor(_game, functor));
	PUSH_IN("STR_ID", idStat);
	PUSH_IN("STR_NAME_UC", nameStat);
	PUSH_IN("STR_BIOLOGY_UC", biologyStat);
	PUSH_IN("STR_INSIGHT_UC", insightStat);
#undef PUSH_IN
	_cbxSortBy->setOptions(sortOptions);
	_cbxSortBy->setSelected(0);
	_cbxSortBy->setText(tr("STR_SORT_BY"));
	_cbxSortBy->onChange(
		(ActionHandler)&ContainedMonsterAllocateScientistsState::cbxSortByChange);

	_lstScientists->setColumns(2, 106, 174);
	_lstScientists->setSelectable(true);
	_lstScientists->setBackground(_window);
	_lstScientists->setMargin(8);
	_lstScientists->onMouseClick(
		(ActionHandler)&ContainedMonsterAllocateScientistsState::lstScientistsClick, 0);
}

ContainedMonsterAllocateScientistsState::~ContainedMonsterAllocateScientistsState()
{
	for (auto* sortFunctor : _sortFunctors)
	{
		delete sortFunctor;
	}
}

void ContainedMonsterAllocateScientistsState::init()
{
	State::init();
	_base->prepareSoldierStatsWithBonuses();
	fillList(_lstScientists->getScroll());
}

void ContainedMonsterAllocateScientistsState::cbxSortByChange(Action*)
{
	const size_t selected = _cbxSortBy->getSelected();
	if (selected == static_cast<size_t>(-1))
	{
		return;
	}
	SortFunctor* sorter = _sortFunctors[selected];
	if (sorter)
	{
		if (selected == 2)
		{
			std::stable_sort(_base->getSoldiers()->begin(), _base->getSoldiers()->end(),
				[](const Soldier* a, const Soldier* b)
				{ return Unicode::naturalCompare(a->getName(), b->getName()); });
		}
		else
		{
			std::stable_sort(
				_base->getSoldiers()->begin(), _base->getSoldiers()->end(), *sorter);
		}
		if (_game->isShiftPressed())
		{
			std::reverse(_base->getSoldiers()->begin(), _base->getSoldiers()->end());
		}
	}
	else
	{
		for (auto* original : _originalOrder)
		{
			auto found = std::find(
				_base->getSoldiers()->begin(), _base->getSoldiers()->end(), original);
			if (found != _base->getSoldiers()->end())
			{
				_base->getSoldiers()->erase(found);
				_base->getSoldiers()->push_back(original);
			}
		}
	}
	fillList(_lstScientists->getScroll());
}

void ContainedMonsterAllocateScientistsState::fillList(size_t scroll)
{
	_rows.clear();
	_lstScientists->clearList();
	const auto& selected = _detail->getPreviewScientists();

	for (auto* scientist : *_base->getSoldiers())
	{
		if (scientist->getRoleRank(ROLE_SCIENTIST) < 1)
		{
			continue;
		}
		const bool isSelected =
			std::find(selected.begin(), selected.end(), scientist) != selected.end();
		const bool assignedHere =
			scientist->getActiveContainedMonster() == _detail->getMonster();
		if (!isSelected && !assignedHere && !scientist->isAvailableForBaseDuty())
		{
			continue;
		}

		_rows.push_back(scientist);
		bool isBusy = false;
		bool isFree = false;
		std::string duty = scientist->getCurrentDuty(
			_game->getLanguage(), _base->getSumRecoveryPerDay(), isBusy, isFree, LAB);
		if (isSelected)
		{
			duty = tr("STR_ASSIGNED_UC");
		}
		_lstScientists->addRow(
			2, scientist->getName(true, 19).c_str(), duty.c_str());
		if (isSelected)
		{
			_lstScientists->setRowColor(
				_rows.size() - 1, _lstScientists->getSecondaryColor());
		}
	}
	_txtCount->setText(tr("STR_CONTAINED_MONSTER_SCIENTISTS").arg(selected.size()));
	if (scroll)
	{
		_lstScientists->scrollTo(scroll);
	}
	_lstScientists->draw();
}

void ContainedMonsterAllocateScientistsState::btnOkClick(Action*)
{
	_game->popState();
}

void ContainedMonsterAllocateScientistsState::lstScientistsClick(Action* action)
{
	const size_t row = _lstScientists->getSelectedRow();
	if (row >= _rows.size())
	{
		return;
	}
	Soldier* scientist = _rows[row];
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		auto found = std::find(
			_base->getSoldiers()->begin(), _base->getSoldiers()->end(), scientist);
		if (found != _base->getSoldiers()->end())
		{
			_game->pushState(new SoldierInfoStateFtA(_base,
				static_cast<size_t>(std::distance(
					_base->getSoldiers()->begin(), found))));
		}
		return;
	}
	if (action->getDetails()->button.button != SDL_BUTTON_LEFT)
	{
		return;
	}
	const auto& selected = _detail->getPreviewScientists();
	if (std::find(selected.begin(), selected.end(), scientist) != selected.end())
	{
		_detail->removePreviewScientist(scientist);
	}
	else if (scientist->isAvailableForBaseDuty() ||
		scientist->getActiveContainedMonster() == _detail->getMonster())
	{
		_detail->addPreviewScientist(scientist);
	}
	fillList(_lstScientists->getScroll());
}

}
