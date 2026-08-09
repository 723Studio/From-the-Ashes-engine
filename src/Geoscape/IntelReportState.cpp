/*
 * Copyright 2010-2022 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "IntelReportState.h"

#include "GeoscapeState.h"
#include "../Basescape/IntelState.h"
#include "../Battlescape/PromotionsState.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Savegame/Base.h"
#include "../Savegame/IntelProject.h"
#include "../Savegame/Soldier.h"
#include "../Ufopaedia/Ufopaedia.h"

namespace OpenXcom
{

IntelReportState::IntelReportState(IntelProject* project, GeoscapeState* state, Base* base,
	IntelReportMode mode, const std::vector<std::string>& articleIds,
	const PromotionBatch& promotions, Base* researchBase,
	std::vector<std::string> newResearchNotifications,
	bool containedAlienStudiesUnblocked) :
	_project(project), _state(state), _base(base), _researchBase(researchBase), _mode(mode),
	_articleIds(articleIds), _promotions(promotions),
	_newResearchNotifications(std::move(newResearchNotifications)),
	_containedAlienStudiesUnblocked(containedAlienStudiesUnblocked)
{
	_screen = false;
	const bool showReport = !_articleIds.empty();
	const bool showAllocate = _base && (_mode == INTEL_REPORT_BLOCKED ||
		_mode == INTEL_REPORT_UNBLOCKED || (_mode == INTEL_REPORT_COMPLETED && !showReport));
	const bool showPromotions = !_promotions.empty();
	const bool useTwoRowActionLayout = showReport && showAllocate;

	_window = new Window(this, 230, 140, 45, 30, POPUP_BOTH);
	_btnOk = new TextButton(useTwoRowActionLayout ? 104 : 80, 16,
		useTwoRowActionLayout ? 52 : 64, 146);
	_btnReport = new TextButton(useTwoRowActionLayout ? 104 : 80, 16,
		useTwoRowActionLayout ? 164 : 176, 146);
	_btnAllocate = new TextButton(useTwoRowActionLayout ? 104 : 80, 16,
		useTwoRowActionLayout ? 52 : 176, useTwoRowActionLayout ? 125 : 146);
	_btnPromotions = new TextButton(useTwoRowActionLayout ? 104 : 118, 16,
		useTwoRowActionLayout ? 164 : 162, 125);
	_txtTitle = new Text(230, 17, 45,
		_containedAlienStudiesUnblocked ? 47 : 58);
	_txtProject = new Text(230, 28, 45,
		_containedAlienStudiesUnblocked ? 68 : 82);
	_txtDescription = new Text(210,
		_containedAlienStudiesUnblocked ? 25 : (useTwoRowActionLayout ? 9 : 18),
		55, _containedAlienStudiesUnblocked ? 98 : 111);

	setInterface("geoIntelReport");
	add(_window, "window", "geoIntelReport");
	add(_btnOk, "button", "geoIntelReport");
	add(_btnReport, "button", "geoIntelReport");
	add(_btnAllocate, "button", "geoIntelReport");
	add(_btnPromotions, "button", "geoIntelReport");
	add(_txtTitle, "text1", "geoIntelReport");
	add(_txtProject, "text2", "geoIntelReport");
	add(_txtDescription, "text1", "geoIntelReport");
	centerAllSurfaces();
	setWindowBackground(_window, "geoIntelReport");

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&IntelReportState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&IntelReportState::btnOkClick, Options::keyCancel);

	_btnReport->setText(tr("STR_VIEW_REPORT"));
	_btnReport->onMouseClick((ActionHandler)&IntelReportState::btnReportClick);

	_btnAllocate->setText(tr("STR_ALLOCATE_AGENTS"));
	_btnAllocate->onMouseClick((ActionHandler)&IntelReportState::btnAllocateClick);
	if (showReport)
	{
		_btnReport->onKeyboardPress((ActionHandler)&IntelReportState::btnReportClick, Options::keyOk);
	}
	else if (showAllocate)
	{
		_btnAllocate->onKeyboardPress((ActionHandler)&IntelReportState::btnAllocateClick, Options::keyOk);
	}

	_btnPromotions->setText(tr("STR_VIEW_PROMOTIONS"));
	_btnPromotions->onMouseClick((ActionHandler)&IntelReportState::btnPromotionsClick);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	switch (_mode)
	{
	case INTEL_REPORT_BREAKTHROUGH:
		_txtTitle->setText(tr("STR_INTEL_BREAKTHROUGH"));
		break;
	case INTEL_REPORT_BLOCKED:
		_txtTitle->setText(tr("STR_INVESTIGATION_STALLED"));
		break;
	case INTEL_REPORT_UNBLOCKED:
		_txtTitle->setText(tr("STR_NEW_LEADS"));
		break;
	case INTEL_REPORT_COMPLETED:
		_txtTitle->setText(tr("STR_INTEL_PROJECT_COMPLETED"));
		break;
	}

	_txtProject->setAlign(ALIGN_CENTER);
	_txtProject->setBig();
	_txtProject->setWordWrap(true);
	if (_project)
	{
		_txtProject->setText(tr(_project->getName()));
	}

	_txtDescription->setAlign(ALIGN_CENTER);
	_txtDescription->setWordWrap(true);
	_txtDescription->setVisible(_mode == INTEL_REPORT_BLOCKED ||
		_containedAlienStudiesUnblocked);
	if (_mode == INTEL_REPORT_BLOCKED)
	{
		std::string description = tr("STR_MORE_EVIDENCE_REQUIRED");
		if (_containedAlienStudiesUnblocked)
		{
			description += "\n";
			description += tr("STR_CONTAINED_ALIEN_STUDIES_CAN_CONTINUE");
		}
		_txtDescription->setText(description);
	}
	else if (_containedAlienStudiesUnblocked)
	{
		_txtDescription->setText(tr("STR_CONTAINED_ALIEN_STUDIES_CAN_CONTINUE"));
	}

	_btnReport->setVisible(showReport);
	_btnAllocate->setVisible(showAllocate);
	_btnPromotions->setVisible(showPromotions);
}

IntelReportState::~IntelReportState()
{
}

void IntelReportState::btnOkClick(Action*)
{
	Game* game = _game;
	GeoscapeState* state = _state;
	Base* researchBase = _researchBase;
	const auto researchIds = _newResearchNotifications;
	game->popState();
	if (state)
	{
		state->showNewResearchNotifications(researchBase, researchIds);
	}
}

void IntelReportState::btnReportClick(Action*)
{
	for (auto article = _articleIds.rbegin(); article != _articleIds.rend(); ++article)
	{
		Ufopaedia::openArticle(_game, *article);
	}
}

void IntelReportState::btnAllocateClick(Action*)
{
	_state->timerReset();
	if (_base)
	{
		_game->pushState(new IntelState(_base));
	}
}

void IntelReportState::btnPromotionsClick(Action*)
{
	_game->pushState(new PromotionsState(_promotions));
}

}
