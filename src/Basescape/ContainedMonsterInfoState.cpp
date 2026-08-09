/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "ContainedMonsterInfoState.h"

#include <algorithm>
#include <sstream>
#include "ContainedMonsterAllocateScientistsState.h"
#include "../Geoscape/ContainedMonsterReportState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Mod/RuleContainedMonster.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseContainedMonster.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Soldier.h"

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

std::string selectedLabel(bool selected, const std::string& label)
{
	return selected ? "[ " + label + " ]" : label;
}

}

ContainedMonsterInfoState::ContainedMonsterInfoState(Base* base,
	BaseContainedMonster* monster, bool confirmTerminate) :
	_base(base), _monster(monster), _previewAction(monster->getAction()),
	_previewScientists(monster->getAssignedScientists()),
	_confirmingTerminate(confirmTerminate)
{
	_screen = false;
	_window = new Window(this, 320, 200, 0, 0);
	_txtTitle = new Text(198, 17, 8, 7);
	_btnTerminate = new TextButton(106, 16, 206, 7);
	_txtCurrentHP = new Text(148, 9, 8, 28);
	_txtMaxHP = new Text(148, 9, 164, 28);
	_txtAction = new Text(148, 9, 8, 39);
	_txtProgress = new Text(148, 9, 164, 39);
	_txtScientists = new Text(304, 9, 8, 50);
	_btnNone = new TextButton(58, 16, 8, 63);
	_btnStudy = new TextButton(58, 16, 69, 63);
	_btnQuarantine = new TextButton(58, 16, 130, 63);
	_btnVivisection = new TextButton(58, 16, 191, 63);
	_btnTaming = new TextButton(60, 16, 252, 63);
	_btnAllocate = new TextButton(126, 16, 8, 86);
	_txtAvailability = new Text(168, 80, 144, 86);
	_lstScientists = new TextList(126, 64, 8, 107);
	_btnCancel = new TextButton(148, 16, 8, 176);
	_btnOk = new TextButton(148, 16, 164, 176);

	setInterface("prisonerInfo");
	add(_window, "window", "prisonerInfo");
	add(_btnOk, "button1", "prisonerInfo");
	add(_btnCancel, "button1", "prisonerInfo");
	add(_btnAllocate, "button1", "prisonerInfo");
	add(_btnTerminate, "button1", "prisonerInfo");
	add(_btnNone, "button3", "prisonerInfo");
	add(_btnStudy, "button3", "prisonerInfo");
	add(_btnQuarantine, "button3", "prisonerInfo");
	add(_btnVivisection, "button3", "prisonerInfo");
	add(_btnTaming, "button3", "prisonerInfo");
	add(_txtTitle, "text", "prisonerInfo");
	add(_txtCurrentHP, "text", "prisonerInfo");
	add(_txtMaxHP, "text", "prisonerInfo");
	add(_txtAction, "text", "prisonerInfo");
	add(_txtProgress, "text", "prisonerInfo");
	add(_txtScientists, "text", "prisonerInfo");
	add(_txtAvailability, "text", "prisonerInfo");
	add(_lstScientists, "list", "prisonerInfo");
	centerAllSurfaces();
	setWindowBackground(_window, "prisonerInfo");

	_txtTitle->setBig();
	_txtTitle->setText(_monster->getNameAndDesignation(_game->getLanguage()));
	_txtTitle->setWordWrap(true);
	_btnTerminate->setText(tr("STR_TERMINATE_CONTAINED_MONSTER"));
	_btnTerminate->onMouseClick((ActionHandler)&ContainedMonsterInfoState::btnTerminateClick);
	_btnAllocate->setText(tr("STR_ALLOCATE_SCIENTISTS"));
	_btnAllocate->onMouseClick((ActionHandler)&ContainedMonsterInfoState::btnAllocateClick);
	_btnNone->onMouseClick((ActionHandler)&ContainedMonsterInfoState::btnNoneClick);
	_btnStudy->onMouseClick((ActionHandler)&ContainedMonsterInfoState::btnStudyClick);
	_btnQuarantine->onMouseClick(
		(ActionHandler)&ContainedMonsterInfoState::btnQuarantineClick);
	_btnVivisection->onMouseClick(
		(ActionHandler)&ContainedMonsterInfoState::btnVivisectionClick);
	_btnTaming->onMouseClick((ActionHandler)&ContainedMonsterInfoState::btnTamingClick);
	_btnCancel->onMouseClick((ActionHandler)&ContainedMonsterInfoState::btnCancelClick);
	_btnCancel->onKeyboardPress(
		(ActionHandler)&ContainedMonsterInfoState::btnCancelClick, Options::keyCancel);
	_btnOk->onMouseClick((ActionHandler)&ContainedMonsterInfoState::btnOkClick);
	_btnOk->onKeyboardPress(
		(ActionHandler)&ContainedMonsterInfoState::btnOkClick, Options::keyOk);
	_lstScientists->setColumns(1, 126);
	_lstScientists->setBackground(_window);
	_lstScientists->setWordWrap(true);
	_txtAvailability->setWordWrap(true);
}

void ContainedMonsterInfoState::init()
{
	State::init();
	refresh();
}

bool ContainedMonsterInfoState::isActionAvailable(ContainedMonsterAction action) const
{
	const SavedGame& save = *_game->getSavedGame();
	switch (action)
	{
	case CONTAINED_MONSTER_ACTION_STUDY:
		return _monster->isStudyAvailable(save);
	case CONTAINED_MONSTER_ACTION_QUARANTINE:
		return _monster->isQuarantineAvailable(save);
	case CONTAINED_MONSTER_ACTION_VIVISECTION:
		return _monster->isVivisectionAvailable(save);
	case CONTAINED_MONSTER_ACTION_TAMING:
		return _monster->isTamingAvailable(save);
	case CONTAINED_MONSTER_ACTION_NONE:
	default:
		return true;
	}
}

int ContainedMonsterInfoState::progressForAction(ContainedMonsterAction action) const
{
	switch (action)
	{
	case CONTAINED_MONSTER_ACTION_STUDY:
		return _monster->getStudyProgress();
	case CONTAINED_MONSTER_ACTION_VIVISECTION:
		return _monster->getVivisectionProgress();
	case CONTAINED_MONSTER_ACTION_TAMING:
		return _monster->getTamingProgress();
	default:
		return 0;
	}
}

int ContainedMonsterInfoState::costForAction(ContainedMonsterAction action) const
{
	const auto* rules = _monster->getRules();
	switch (action)
	{
	case CONTAINED_MONSTER_ACTION_STUDY:
		return rules->getStudy().getCost();
	case CONTAINED_MONSTER_ACTION_VIVISECTION:
		return rules->getVivisection() ? rules->getVivisection()->getCost() : 0;
	case CONTAINED_MONSTER_ACTION_TAMING:
		return rules->getTaming() ? rules->getTaming()->getCost() : 0;
	default:
		return 0;
	}
}

void ContainedMonsterInfoState::refresh()
{
	if (_confirmingTerminate)
	{
		_txtTitle->setText(tr("STR_CONFIRM_TERMINATE_CONTAINED_MONSTER"));
		_txtAvailability->setText(_monster->getNameAndDesignation(_game->getLanguage()));
		_btnCancel->setText(tr("STR_NO"));
		_btnOk->setText(tr("STR_YES"));
		_btnTerminate->setVisible(false);
		_btnAllocate->setVisible(false);
		_btnNone->setVisible(false);
		_btnStudy->setVisible(false);
		_btnQuarantine->setVisible(false);
		_btnVivisection->setVisible(false);
		_btnTaming->setVisible(false);
		_txtCurrentHP->setVisible(false);
		_txtMaxHP->setVisible(false);
		_txtAction->setVisible(false);
		_txtProgress->setVisible(false);
		_txtScientists->setVisible(false);
		_lstScientists->setVisible(false);
		return;
	}

	_txtTitle->setText(_monster->getNameAndDesignation(_game->getLanguage()));
	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnOk->setText(tr("STR_OK"));
	_btnTerminate->setVisible(true);
	_txtCurrentHP->setVisible(true);
	_txtMaxHP->setVisible(true);
	_txtAction->setVisible(true);
	_txtProgress->setVisible(true);
	_txtScientists->setVisible(true);
	_txtAvailability->setVisible(true);
	_lstScientists->setVisible(true);

	const bool studyAvailable =
		isActionAvailable(CONTAINED_MONSTER_ACTION_STUDY);
	const bool quarantineAvailable =
		isActionAvailable(CONTAINED_MONSTER_ACTION_QUARANTINE);
	const bool vivisectionAvailable =
		isActionAvailable(CONTAINED_MONSTER_ACTION_VIVISECTION);
	const bool tamingAvailable =
		isActionAvailable(CONTAINED_MONSTER_ACTION_TAMING);
	_btnNone->setVisible(true);
	_btnStudy->setVisible(studyAvailable);
	_btnQuarantine->setVisible(quarantineAvailable);
	_btnVivisection->setVisible(vivisectionAvailable);
	_btnTaming->setVisible(tamingAvailable);
	_btnAllocate->setVisible(_previewAction != CONTAINED_MONSTER_ACTION_NONE);

	_btnNone->setText(selectedLabel(
		_previewAction == CONTAINED_MONSTER_ACTION_NONE,
		tr("STR_CONTAINED_MONSTER_ACTION_NONE")));
	_btnStudy->setText(selectedLabel(
		_previewAction == CONTAINED_MONSTER_ACTION_STUDY,
		tr("STR_CONTAINED_MONSTER_ACTION_STUDY")));
	_btnQuarantine->setText(selectedLabel(
		_previewAction == CONTAINED_MONSTER_ACTION_QUARANTINE,
		tr("STR_CONTAINED_MONSTER_ACTION_QUARANTINE")));
	_btnVivisection->setText(selectedLabel(
		_previewAction == CONTAINED_MONSTER_ACTION_VIVISECTION,
		tr("STR_CONTAINED_MONSTER_ACTION_VIVISECTION")));
	_btnTaming->setText(selectedLabel(
		_previewAction == CONTAINED_MONSTER_ACTION_TAMING,
		tr("STR_CONTAINED_MONSTER_ACTION_TAMING")));

	_txtCurrentHP->setText(tr("STR_CONTAINED_MONSTER_CURRENT_HP")
		.arg(_monster->getCurrentHP()));
	_txtMaxHP->setText(tr("STR_CONTAINED_MONSTER_MAX_HP")
		.arg(_monster->getMaxHP()));
	std::ostringstream action;
	action << tr("STR_CONTAINED_MONSTER_ACTION") << ": "
		<< tr(actionKey(_previewAction));
	_txtAction->setText(action.str());
	const int cost = costForAction(_previewAction);
	if (cost > 0)
	{
		std::ostringstream progress;
		progress << progressForAction(_previewAction) << "/" << cost;
		_txtProgress->setText(tr("STR_CONTAINED_MONSTER_PROGRESS").arg(progress.str()));
	}
	else
	{
		_txtProgress->setText(tr("STR_CONTAINED_MONSTER_PROGRESS").arg("-"));
	}
	_txtScientists->setText(tr("STR_CONTAINED_MONSTER_SCIENTISTS")
		.arg(_previewScientists.size()));

	_lstScientists->clearList();
	for (auto* scientist : _previewScientists)
	{
		_lstScientists->addRow(1, scientist->getName(true, 20).c_str());
	}

	std::ostringstream unavailable;
	auto appendUnavailable = [&](const char* key)
		{
			if (unavailable.tellp() > 0)
			{
				unavailable << "\n";
			}
			unavailable << tr(key);
		};
	if (!studyAvailable)
	{
		appendUnavailable("STR_CONTAINED_MONSTER_STUDY_UNAVAILABLE");
	}
	if (!quarantineAvailable)
	{
		appendUnavailable("STR_CONTAINED_MONSTER_QUARANTINE_UNAVAILABLE");
	}
	if (!vivisectionAvailable)
	{
		appendUnavailable("STR_CONTAINED_MONSTER_VIVISECTION_UNAVAILABLE");
	}
	if (!tamingAvailable)
	{
		appendUnavailable("STR_CONTAINED_MONSTER_TAMING_UNAVAILABLE");
	}
	_txtAvailability->setText(unavailable.str());
}

void ContainedMonsterInfoState::selectAction(ContainedMonsterAction action)
{
	if (isActionAvailable(action))
	{
		_previewAction = action;
		refresh();
	}
}

void ContainedMonsterInfoState::btnOkClick(Action*)
{
	if (_confirmingTerminate)
	{
		terminateConfirmed();
		return;
	}
	if (!_monster->setAction(_previewAction, *_game->getSavedGame()))
	{
		refresh();
		return;
	}
	_monster->clearAssignments();
	if (_previewAction != CONTAINED_MONSTER_ACTION_NONE)
	{
		for (auto* scientist : _previewScientists)
		{
			scientist->clearBaseDuty();
			scientist->setActiveContainedMonster(_monster);
		}
	}
	_game->popState();
}

void ContainedMonsterInfoState::btnCancelClick(Action*)
{
	if (_confirmingTerminate)
	{
		_confirmingTerminate = false;
		refresh();
		return;
	}
	_game->popState();
}

void ContainedMonsterInfoState::btnAllocateClick(Action*)
{
	if (_previewAction != CONTAINED_MONSTER_ACTION_NONE)
	{
		_game->pushState(new ContainedMonsterAllocateScientistsState(_base, this));
	}
}

void ContainedMonsterInfoState::btnTerminateClick(Action*)
{
	_confirmingTerminate = true;
	refresh();
}

void ContainedMonsterInfoState::terminateConfirmed()
{
	const std::string specimenName =
		_monster->getNameAndDesignation(_game->getLanguage());
	ContainedMonsterProcessResult result = _monster->terminate();
	delete _monster;
	_monster = nullptr;
	Game* game = _game;
	Base* base = _base;
	game->popState();
	game->pushState(new ContainedMonsterReportState(base, specimenName, result));
}

void ContainedMonsterInfoState::btnNoneClick(Action*)
{
	selectAction(CONTAINED_MONSTER_ACTION_NONE);
}

void ContainedMonsterInfoState::btnStudyClick(Action*)
{
	selectAction(CONTAINED_MONSTER_ACTION_STUDY);
}

void ContainedMonsterInfoState::btnQuarantineClick(Action*)
{
	selectAction(CONTAINED_MONSTER_ACTION_QUARANTINE);
}

void ContainedMonsterInfoState::btnVivisectionClick(Action*)
{
	selectAction(CONTAINED_MONSTER_ACTION_VIVISECTION);
}

void ContainedMonsterInfoState::btnTamingClick(Action*)
{
	selectAction(CONTAINED_MONSTER_ACTION_TAMING);
}

void ContainedMonsterInfoState::addPreviewScientist(Soldier* scientist)
{
	if (std::find(_previewScientists.begin(), _previewScientists.end(), scientist) ==
		_previewScientists.end())
	{
		_previewScientists.push_back(scientist);
	}
}

void ContainedMonsterInfoState::removePreviewScientist(Soldier* scientist)
{
	_previewScientists.erase(
		std::remove(_previewScientists.begin(), _previewScientists.end(), scientist),
		_previewScientists.end());
}

}
