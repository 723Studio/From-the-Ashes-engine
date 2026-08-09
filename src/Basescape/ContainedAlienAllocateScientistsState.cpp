/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "ContainedAlienAllocateScientistsState.h"

#include <algorithm>
#include "ContainedAlienInfoState.h"
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
#include "../Mod/Mod.h"
#include "../Mod/RuleContainedAlien.h"
#include "../Mod/RuleInterface.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseContainedAlien.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Soldier.h"

namespace OpenXcom
{

ContainedAlienAllocateScientistsState::ContainedAlienAllocateScientistsState(
	Base* base, ContainedAlienInfoState* detail) :
	_base(base), _detail(detail), _originalOrder(*base->getSoldiers()),
	_busyColor(0)
{
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(148, 16, 164, 176);
	_txtTitle = new Text(247, 17, 16, 7);
	_txtCapacity = new Text(150, 9, 16, 24);
	_txtName = new Text(114, 9, 16, 32);
	_txtAssignment = new Text(84, 9, 122, 32);
	_cbxSortBy = new ComboBox(this, 148, 16, 8, 176, true);
	_lstScientists = new TextList(288, 128, 8, 40);

	setInterface("researchAllocateScientists");
	add(_window, "window", "researchAllocateScientists");
	add(_btnOk, "button", "researchAllocateScientists");
	add(_txtTitle, "text", "researchAllocateScientists");
	add(_txtCapacity, "text", "researchAllocateScientists");
	add(_txtName, "text", "researchAllocateScientists");
	add(_txtAssignment, "text", "researchAllocateScientists");
	add(_cbxSortBy, "button", "researchAllocateScientists");
	add(_lstScientists, "list", "researchAllocateScientists");
	centerAllSurfaces();
	setWindowBackground(_window, "researchAllocateScientists");
	_busyColor = _game->getMod()->getInterface("researchAllocateScientists")->getElement("otherCraft")->color;

	_txtTitle->setBig();
	_txtTitle->setText(_detail->getAlien()->getNameAndDesignation(_game->getLanguage()));
	_txtTitle->setWordWrap(true);
	_txtTitle->setVerticalAlign(ALIGN_MIDDLE);
	_txtName->setText(tr("STR_NAME_UC"));
	_txtAssignment->setText(tr("STR_ASSIGNMENT"));
	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ContainedAlienAllocateScientistsState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ContainedAlienAllocateScientistsState::btnOkClick, Options::keyCancel);

	std::vector<std::string> sortOptions;
	sortOptions.push_back(tr("STR_ORIGINAL_ORDER"));
	_sortFunctors.push_back(nullptr);
#define PUSH_IN(strId, functor) \
	sortOptions.push_back(tr(strId)); \
	_sortFunctors.push_back(new SortFunctor(_game, functor));
	PUSH_IN("STR_ID", idStat);
	PUSH_IN("STR_NAME_UC", nameStat);
	const auto* node = _detail->getAlien()->getCurrentStudyNode();
	const auto category = node ? node->getCategory() : CONTAINED_STUDY_LIVE_BIOLOGY;
	const bool xenolinguisticsUnlocked = _game->getSavedGame()->isResearched(
		_game->getMod()->getXenolinguisticsUnlockResearch());
	const bool alienTechUnlocked = _game->getSavedGame()->isResearched(
		_game->getMod()->getAlienTechUnlockResearch());
	switch (category)
	{
	case CONTAINED_STUDY_LIVE_BIOLOGY:
		PUSH_IN("STR_BIOLOGY_UC", biologyStat);
		break;
	case CONTAINED_STUDY_COMMUNICATION:
		if (xenolinguisticsUnlocked)
		{
			PUSH_IN("STR_XENOLINGUISTICS_UC", xenolinguisticsStat);
		}
		break;
	case CONTAINED_STUDY_TACTICAL_INTELLIGENCE:
		if (xenolinguisticsUnlocked)
		{
			PUSH_IN("STR_XENOLINGUISTICS_UC", xenolinguisticsStat);
		}
		PUSH_IN("STR_TACTICS_UC", tacticsStat);
		break;
	case CONTAINED_STUDY_TECHNICAL_INTELLIGENCE:
		if (xenolinguisticsUnlocked)
		{
			PUSH_IN("STR_XENOLINGUISTICS_UC", xenolinguisticsStat);
		}
		if (alienTechUnlocked)
		{
			PUSH_IN("STR_ALIEN_TECH_UC", alienTechStat);
		}
		break;
	case CONTAINED_STUDY_STRATEGIC_INTELLIGENCE:
		if (xenolinguisticsUnlocked)
		{
			PUSH_IN("STR_XENOLINGUISTICS_UC", xenolinguisticsStat);
		}
		PUSH_IN("STR_DATA_ANALYSIS_UC", dataStat);
		if (alienTechUnlocked)
		{
			PUSH_IN("STR_ALIEN_TECH_UC", alienTechStat);
		}
		break;
	}
	PUSH_IN("STR_INSIGHT_UC", insightStat);
#undef PUSH_IN
	_cbxSortBy->setOptions(sortOptions);
	_cbxSortBy->setSelected(0);
	_cbxSortBy->setText(tr("STR_SORT_BY"));
	_cbxSortBy->onChange((ActionHandler)&ContainedAlienAllocateScientistsState::cbxSortByChange);

	_lstScientists->setColumns(2, 106, 174);
	_lstScientists->setSelectable(true);
	_lstScientists->setBackground(_window);
	_lstScientists->setMargin(8);
	_lstScientists->onMouseClick((ActionHandler)&ContainedAlienAllocateScientistsState::lstScientistsClick, 0);
}

ContainedAlienAllocateScientistsState::~ContainedAlienAllocateScientistsState()
{
	for (auto* sortFunctor : _sortFunctors)
	{
		delete sortFunctor;
	}
}

int ContainedAlienAllocateScientistsState::maximumForCurrentAlien() const
{
	int occupiedByOthers = 0;
	for (const auto* scientist : *_base->getSoldiers())
	{
		if (scientist->getActiveContainedAlien() &&
			scientist->getActiveContainedAlien() != _detail->getAlien())
		{
			++occupiedByOthers;
		}
	}
	return std::max(0, _base->getAvailableContainedAlienResearchSpace() - occupiedByOthers);
}

void ContainedAlienAllocateScientistsState::init()
{
	State::init();
	_base->prepareSoldierStatsWithBonuses();
	fillList(_lstScientists->getScroll());
}

void ContainedAlienAllocateScientistsState::cbxSortByChange(Action*)
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
			std::stable_sort(_base->getSoldiers()->begin(), _base->getSoldiers()->end(), *sorter);
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
			auto found = std::find(_base->getSoldiers()->begin(), _base->getSoldiers()->end(), original);
			if (found != _base->getSoldiers()->end())
			{
				_base->getSoldiers()->erase(found);
				_base->getSoldiers()->push_back(original);
			}
		}
	}
	fillList(_lstScientists->getScroll());
}

void ContainedAlienAllocateScientistsState::fillList(size_t scroll)
{
	_rows.clear();
	_lstScientists->clearList();
	const auto& selected = _detail->getPreviewScientists();
	_lstScientists->setColumns(2, 106, 174);

	int row = 0;
	for (auto* scientist : *_base->getSoldiers())
	{
		if (scientist->getRoleRank(ROLE_SCIENTIST) < 1)
		{
			continue;
		}
		_rows.push_back(scientist);
		bool isBusy = false;
		bool isFree = false;
		std::string duty = scientist->getCurrentDuty(
			_game->getLanguage(), _base->getSumRecoveryPerDay(), isBusy, isFree, LAB);
		const bool isSelected = std::find(selected.begin(), selected.end(), scientist) != selected.end();
		if (isSelected)
		{
			duty = tr("STR_ASSIGNED_UC");
		}
		_lstScientists->addRow(2, scientist->getName(true, 19).c_str(), duty.c_str());
		const bool available = scientist->isAvailableForBaseDuty() ||
			scientist->getActiveContainedAlien() == _detail->getAlien() || isSelected;
		_lstScientists->setRowColor(row,
			isSelected ? _lstScientists->getSecondaryColor() :
			(available ? _lstScientists->getColor() : _busyColor));
		++row;
	}
	_txtCapacity->setText(tr("STR_ALIEN_STUDY_ALLOCATION")
		.arg(selected.size()).arg(maximumForCurrentAlien()));
	if (scroll)
	{
		_lstScientists->scrollTo(scroll);
	}
	_lstScientists->draw();
}

void ContainedAlienAllocateScientistsState::btnOkClick(Action*)
{
	_game->popState();
}

void ContainedAlienAllocateScientistsState::lstScientistsClick(Action* action)
{
	const size_t row = _lstScientists->getSelectedRow();
	if (row >= _rows.size())
	{
		return;
	}
	Soldier* scientist = _rows[row];
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		auto found = std::find(_base->getSoldiers()->begin(), _base->getSoldiers()->end(), scientist);
		if (found != _base->getSoldiers()->end())
		{
			_game->pushState(new SoldierInfoStateFtA(_base,
				static_cast<size_t>(std::distance(_base->getSoldiers()->begin(), found))));
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
	else if ((scientist->isAvailableForBaseDuty() ||
		scientist->getActiveContainedAlien() == _detail->getAlien()) &&
		static_cast<int>(selected.size()) < maximumForCurrentAlien())
	{
		_detail->addPreviewScientist(scientist);
	}
	fillList(_lstScientists->getScroll());
}

}
