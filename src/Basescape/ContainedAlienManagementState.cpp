/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "ContainedAlienManagementState.h"

#include <algorithm>
#include <sstream>
#include "ContainedAlienInfoState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseContainedAlien.h"
#include "../Savegame/Soldier.h"

namespace OpenXcom
{

namespace
{

int dangerRank(const ContainedAlienStudyForecast& forecast)
{
	if (forecast.hoursToDeath >= 0 && forecast.hoursToDeath <= 24) return 3;
	if ((forecast.hoursToDeath >= 0 && forecast.hoursToDeath <= 72) ||
		(forecast.canProgress && !forecast.completesBeforeDeath)) return 2;
	if (forecast.hoursToDeath >= 0 && forecast.hoursToDeath <= 24 * 7) return 1;
	return 0;
}

const char* outlookKey(ContainedAlienStudyOutlook outlook)
{
	switch (outlook)
	{
	case CONTAINED_OUTLOOK_NONE: return "STR_CONTAINED_OUTLOOK_NONE_SHORT";
	case CONTAINED_OUTLOOK_POOR: return "STR_CONTAINED_OUTLOOK_POOR_SHORT";
	case CONTAINED_OUTLOOK_AVERAGE: return "STR_CONTAINED_OUTLOOK_AVERAGE_SHORT";
	case CONTAINED_OUTLOOK_GOOD: return "STR_CONTAINED_OUTLOOK_GOOD_SHORT";
	case CONTAINED_OUTLOOK_EXCELLENT: return "STR_CONTAINED_OUTLOOK_EXCELLENT_SHORT";
	default: return "STR_CONTAINED_OUTLOOK_UNKNOWN_SHORT";
	}
}

const char* protocolKey(ContainedAlienStudyProtocol protocol)
{
	switch (protocol)
	{
	case CONTAINED_PROTOCOL_PASSIVE: return "STR_CONTAINED_PROTOCOL_PASSIVE_SHORT";
	case CONTAINED_PROTOCOL_AGGRESSIVE: return "STR_CONTAINED_PROTOCOL_AGGRESSIVE_SHORT";
	default: return "STR_CONTAINED_PROTOCOL_ACTIVE_SHORT";
	}
}

}

ContainedAlienManagementState::ContainedAlienManagementState(Base* base, bool forced) :
	_base(base), _forced(forced)
{
	_window = new Window(this, 320, 200, 0, 0);
	_btnOk = new TextButton(304, 16, 8, 176);
	_txtTitle = new Text(304, 17, 8, 7);
	_txtScientistsAvailable = new Text(149, 9, 8, 25);
	_txtScientistsAllocated = new Text(149, 9, 163, 25);
	_txtContainmentSpace = new Text(149, 9, 8, 35);
	_txtStudySpace = new Text(149, 9, 163, 35);
	_txtSpecimen = new Text(149, 17, 10, 46);
	_txtScientists = new Text(49, 17, 159, 46);
	_txtStatus = new Text(88, 17, 208, 46);
	_lstAliens = new TextList(288, 110, 8, 64);

	setInterface("prisonManagement");
	add(_window, "window", "prisonManagement");
	add(_btnOk, "button", "prisonManagement");
	add(_txtTitle, "text1", "prisonManagement");
	add(_txtScientistsAvailable, "text1", "prisonManagement");
	add(_txtScientistsAllocated, "text1", "prisonManagement");
	add(_txtContainmentSpace, "text1", "prisonManagement");
	add(_txtStudySpace, "text1", "prisonManagement");
	add(_txtSpecimen, "text2", "prisonManagement");
	add(_txtScientists, "text2", "prisonManagement");
	add(_txtStatus, "text2", "prisonManagement");
	add(_lstAliens, "list", "prisonManagement");
	centerAllSurfaces();
	setWindowBackground(_window, "prisonManagement");

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ContainedAlienManagementState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ContainedAlienManagementState::btnOkClick, Options::keyCancel);
	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_CONTAINED_ALIEN_MANAGEMENT"));
	_txtSpecimen->setText(tr("STR_SPECIMEN_ID"));
	_txtScientists->setAlign(ALIGN_CENTER);
	_txtScientists->setText(tr("STR_SCIENTISTS"));
	_txtStatus->setText(tr("STR_PROTOCOL_STATUS"));
	_txtStatus->setWordWrap(true);
	_lstAliens->setColumns(3, 149, 49, 88);
	_lstAliens->setSelectable(true);
	_lstAliens->setBackground(_window);
	_lstAliens->setMargin(2);
	_lstAliens->setWordWrap(true);
	_lstAliens->onMouseClick((ActionHandler)&ContainedAlienManagementState::lstAliensClick, SDL_BUTTON_LEFT);
}

void ContainedAlienManagementState::init()
{
	State::init();
	fillList(_lstAliens->getScroll());
}

void ContainedAlienManagementState::fillList(size_t scroll)
{
	struct Row { BaseContainedAlien* alien; ContainedAlienStudyForecast forecast; int danger; };
	std::vector<Row> rows;
	for (auto* alien : _base->getContainedAliens())
	{
		auto forecast = alien->getForecast(alien->getProtocol());
		rows.push_back({alien, forecast, dangerRank(forecast)});
	}
	std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b)
	{
		return a.danger > b.danger;
	});
	_lstAliens->clearList();
	_rows.clear();
	for (const auto& row : rows)
	{
		_rows.push_back(row.alien);
		std::ostringstream assigned;
		assigned << row.alien->getAssignedScientists().size();
		std::string condition;
		if (row.danger == 3) condition = tr("STR_CONTAINED_CONDITION_DYING_SHORT");
		else if (row.danger == 2) condition = tr("STR_CONTAINED_CONDITION_CRITICAL_SHORT");
		else if (row.danger == 1) condition = tr("STR_CONTAINED_CONDITION_UNSTABLE_SHORT");
		else condition = tr(outlookKey(row.forecast.outlook));
		std::string studyStatus;
		if (row.alien->isStudyComplete()) studyStatus = tr("STR_COMPLETE");
		else if (row.forecast.blocked) studyStatus = tr("STR_CONTAINED_ALIEN_STUDY_BLOCKED");
		else studyStatus = tr("STR_CONTAINED_ALIEN_STUDY_READY");
		std::ostringstream status;
		status << tr(protocolKey(row.alien->getProtocol())) << ": " << studyStatus;
		if (!row.alien->isStudyComplete())
		{
			status << " / " << condition;
		}
		_lstAliens->addRow(3,
			row.alien->getNameAndDesignation(_game->getLanguage()).c_str(),
			assigned.str().c_str(), status.str().c_str());
	}

	int freeScientists = 0;
	for (auto* scientist : _base->getPersonnel(ROLE_SCIENTIST))
	{
		if (scientist->isAvailableForBaseDuty()) ++freeScientists;
	}
	_txtScientistsAvailable->setText(tr("STR_SCIENTISTS_AVAILABLE").arg(freeScientists));
	_txtScientistsAllocated->setText(tr("STR_SCIENTISTS_ALLOCATED").arg(
		_base->getUsedContainedAlienResearchSpace()));
	_txtContainmentSpace->setText(tr("STR_CONTAINMENT_SPACE_AVAILABLE").arg(
		_base->getFreeContainedAlienSpace()));
	_txtStudySpace->setText(tr("STR_ALIEN_STUDY_SPACE_AVAILABLE").arg(
		_base->getFreeContainedAlienResearchSpace()));
	_btnOk->setVisible(!_forced || _base->getFreeContainedAlienSpace() >= 0);
	if (scroll) _lstAliens->scrollTo(scroll);
}

void ContainedAlienManagementState::btnOkClick(Action*)
{
	if (!_forced || _base->getFreeContainedAlienSpace() >= 0)
	{
		_game->popState();
	}
}

void ContainedAlienManagementState::lstAliensClick(Action*)
{
	const size_t row = _lstAliens->getSelectedRow();
	auto* alien = row < _rows.size() ? _rows[row] : nullptr;
	if (alien)
	{
		_game->pushState(new ContainedAlienInfoState(_base, alien));
	}
}

}
