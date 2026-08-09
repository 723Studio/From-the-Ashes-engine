/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "ContainedMonsterManagementState.h"

#include <sstream>
#include "ContainedMonsterInfoState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseContainedMonster.h"

namespace OpenXcom
{

namespace
{

const char* actionKey(ContainedMonsterAction action)
{
	switch (action)
	{
	case CONTAINED_MONSTER_ACTION_STUDY:
		return "STR_CONTAINED_MONSTER_ACTION_STUDY";
	case CONTAINED_MONSTER_ACTION_QUARANTINE:
		return "STR_CONTAINED_MONSTER_ACTION_QUARANTINE";
	case CONTAINED_MONSTER_ACTION_VIVISECTION:
		return "STR_CONTAINED_MONSTER_ACTION_VIVISECTION";
	case CONTAINED_MONSTER_ACTION_TAMING:
		return "STR_CONTAINED_MONSTER_ACTION_TAMING";
	default:
		return "STR_CONTAINED_MONSTER_ACTION_NONE";
	}
}

}

ContainedMonsterManagementState::ContainedMonsterManagementState(Base* base, bool forced) :
	_base(base), _forced(forced)
{
	_window = new Window(this, 320, 200, 0, 0);
	_btnTerminate = new TextButton(148, 16, 8, 176);
	_btnOk = new TextButton(148, 16, 164, 176);
	_txtTitle = new Text(304, 17, 8, 7);
	_txtSpaceAvailable = new Text(98, 18, 8, 25);
	_txtSpaceUsed = new Text(98, 18, 111, 25);
	_txtSpaceFree = new Text(98, 18, 214, 25);
	_txtSpecimen = new Text(104, 9, 10, 45);
	_txtHealth = new Text(42, 9, 114, 45);
	_txtAction = new Text(64, 9, 156, 45);
	_txtProgress = new Text(48, 9, 220, 45);
	_txtScientists = new Text(44, 9, 268, 45);
	_lstMonsters = new TextList(304, 116, 8, 56);

	setInterface("prisonManagement");
	add(_window, "window", "prisonManagement");
	add(_btnTerminate, "button", "prisonManagement");
	add(_btnOk, "button", "prisonManagement");
	add(_txtTitle, "text1", "prisonManagement");
	add(_txtSpaceAvailable, "text1", "prisonManagement");
	add(_txtSpaceUsed, "text1", "prisonManagement");
	add(_txtSpaceFree, "text1", "prisonManagement");
	add(_txtSpecimen, "text2", "prisonManagement");
	add(_txtHealth, "text2", "prisonManagement");
	add(_txtAction, "text2", "prisonManagement");
	add(_txtProgress, "text2", "prisonManagement");
	add(_txtScientists, "text2", "prisonManagement");
	add(_lstMonsters, "list", "prisonManagement");
	centerAllSurfaces();
	setWindowBackground(_window, "prisonManagement");

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ContainedMonsterManagementState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ContainedMonsterManagementState::btnOkClick,
		Options::keyCancel);
	_btnTerminate->setText(tr("STR_TERMINATE_CONTAINED_MONSTER"));
	_btnTerminate->onMouseClick(
		(ActionHandler)&ContainedMonsterManagementState::btnTerminateClick);
	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_CONTAINED_MONSTER_MANAGEMENT"));
	_txtSpaceAvailable->setWordWrap(true);
	_txtSpaceUsed->setWordWrap(true);
	_txtSpaceFree->setWordWrap(true);
	_txtSpecimen->setText(tr("STR_CONTAINED_MONSTERS"));
	_txtHealth->setAlign(ALIGN_CENTER);
	_txtHealth->setText(tr("STR_HEALTH"));
	_txtAction->setAlign(ALIGN_CENTER);
	_txtAction->setText(tr("STR_CONTAINED_MONSTER_ACTION"));
	_txtProgress->setAlign(ALIGN_CENTER);
	_txtProgress->setText(tr("STR_PROGRESS"));
	_txtScientists->setAlign(ALIGN_CENTER);
	_txtScientists->setText(tr("STR_SCIENTISTS"));
	_lstMonsters->setColumns(5, 104, 42, 64, 48, 46);
	_lstMonsters->setSelectable(true);
	_lstMonsters->setBackground(_window);
	_lstMonsters->setMargin(2);
	_lstMonsters->setWordWrap(true);
	_lstMonsters->onMouseClick(
		(ActionHandler)&ContainedMonsterManagementState::lstMonstersClick,
		SDL_BUTTON_LEFT);
}

void ContainedMonsterManagementState::init()
{
	State::init();
	fillList(_lstMonsters->getScroll());
}

void ContainedMonsterManagementState::fillList(size_t scroll)
{
	_lstMonsters->clearList();
	_rows.clear();
	for (auto* monster : _base->getContainedMonsters())
	{
		_rows.push_back(monster);
		std::ostringstream health;
		health << monster->getCurrentHP() << "/" << monster->getMaxHP();
		std::ostringstream progress;
		if (monster->getCurrentCost() > 0)
		{
			progress << monster->getCurrentProgress() << "/" << monster->getCurrentCost();
		}
		else
		{
			progress << "-";
		}
		std::ostringstream scientists;
		scientists << monster->getAssignedScientists().size();
		_lstMonsters->addRow(5,
			monster->getNameAndDesignation(_game->getLanguage()).c_str(),
			health.str().c_str(),
			tr(actionKey(monster->getAction())).c_str(),
			progress.str().c_str(),
			scientists.str().c_str());
	}

	_txtSpaceAvailable->setText(tr("STR_CONTAINED_MONSTER_SPACE_AVAILABLE")
		.arg(_base->getAvailableContainedMonsterSpace()));
	_txtSpaceUsed->setText(tr("STR_CONTAINED_MONSTER_SPACE_USED")
		.arg(_base->getUsedContainedMonsterSpace()));
	_txtSpaceFree->setText(tr("STR_CONTAINED_MONSTER_SPACE_FREE")
		.arg(_base->getFreeContainedMonsterSpace()));
	_btnOk->setVisible(!_forced || _base->getFreeContainedMonsterSpace() >= 0);
	_btnTerminate->setVisible(!_rows.empty());
	if (scroll)
	{
		_lstMonsters->scrollTo(scroll);
	}
}

void ContainedMonsterManagementState::btnOkClick(Action*)
{
	if (!_forced || _base->getFreeContainedMonsterSpace() >= 0)
	{
		_game->popState();
	}
}

void ContainedMonsterManagementState::btnTerminateClick(Action*)
{
	const size_t row = _lstMonsters->getSelectedRow();
	auto* monster = row < _rows.size() ? _rows[row] : nullptr;
	if (monster)
	{
		_game->pushState(new ContainedMonsterInfoState(_base, monster, true));
	}
}

void ContainedMonsterManagementState::lstMonstersClick(Action*)
{
	const size_t row = _lstMonsters->getSelectedRow();
	auto* monster = row < _rows.size() ? _rows[row] : nullptr;
	if (monster)
	{
		_game->pushState(new ContainedMonsterInfoState(_base, monster));
	}
}

}
