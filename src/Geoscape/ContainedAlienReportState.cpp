/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "ContainedAlienReportState.h"

#include <sstream>
#include "GeoscapeState.h"
#include "../Battlescape/PromotionsState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Savegame/Base.h"
#include "../Ufopaedia/Ufopaedia.h"

namespace OpenXcom
{

namespace
{

const char* impactKey(ContainedAlienStrategicImpact impact)
{
	switch (impact)
	{
	case CONTAINED_IMPACT_BREAKTHROUGH: return "STR_CONTAINED_IMPACT_BREAKTHROUGH";
	case CONTAINED_IMPACT_SPECIES_PROGRESS: return "STR_CONTAINED_IMPACT_SPECIES_PROGRESS";
	default: return "STR_CONTAINED_IMPACT_ROUTINE";
	}
}

}

ContainedAlienReportState::ContainedAlienReportState(Base* base, std::string specimenName,
	const ContainedAlienProcessResult& result, bool death) :
	_base(base), _death(death), _specimenName(std::move(specimenName)), _result(result),
	_articleIds(result.articleIds), _newResearchNotifications(result.researchNotifications)
{
	_screen = false;
	_window = new Window(this, 270, 150, 25, 25, POPUP_BOTH);
	_txtTitle = new Text(250, 17, 35, 36);
	_txtDescription = new Text(250, 70, 35, 58);
	_btnOk = new TextButton(78, 16, 35, 151);
	_btnReport = new TextButton(78, 16, 121, 151);
	_btnPromotions = new TextButton(78, 16, 207, 151);
	const std::string interface = death ? "geoPrisonerReportDeath" : "geoPrisonerReportNormal";
	setInterface(interface);
	add(_window, "window", interface);
	add(_txtTitle, "text1", interface);
	add(_txtDescription, "text2", interface);
	add(_btnOk, "button", interface);
	add(_btnReport, "button", interface);
	add(_btnPromotions, "button", interface);
	centerAllSurfaces();
	setWindowBackground(_window, interface);
	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr(result.terminated ? "STR_CONTAINED_ALIEN_TERMINATION_REPORT" :
		(death ? "STR_CONTAINED_ALIEN_DEATH_REPORT" : "STR_CONTAINED_ALIEN_STUDY_REPORT")));
	_txtDescription->setAlign(ALIGN_CENTER);
	_txtDescription->setWordWrap(true);
	std::ostringstream description;
	if (death)
	{
		description << _specimenName << " " << tr(result.terminated
			? "STR_CONTAINED_ALIEN_TERMINATED" : "STR_CONTAINED_ALIEN_DIED");
		if (!_result.completedMilestones.empty())
		{
			description << "\n" << tr("STR_SPECIES_MILESTONES_COMPLETED").arg(_result.completedMilestones.size());
		}
		description << "\n" << tr(impactKey(_result.strategicImpact));
	}
	else
	{
		description << _specimenName;
		if (!_result.completedNode.empty())
		{
			description << "\n" << tr(_result.completedNode);
		}
		description << "\n" << tr(impactKey(_result.strategicImpact));
		if (!_result.completedMilestones.empty())
		{
			description << "\n" << tr("STR_SPECIES_MILESTONES_COMPLETED").arg(_result.completedMilestones.size());
		}
		if (_result.chainCompleted) description << "\n" << tr("STR_STUDY_CHAIN_COMPLETE");
	}
	if (_result.containedAlienStudiesUnblocked)
	{
		description << "\n" << tr("STR_CONTAINED_ALIEN_STUDIES_CAN_CONTINUE");
	}
	_txtDescription->setText(description.str());
	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ContainedAlienReportState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ContainedAlienReportState::btnOkClick, Options::keyCancel);
	_btnReport->setText(tr("STR_VIEW_REPORT"));
	_btnReport->onMouseClick((ActionHandler)&ContainedAlienReportState::btnReportClick);
	_btnReport->setVisible(!_articleIds.empty());
	if (_articleIds.empty())
	{
		_btnOk->onKeyboardPress((ActionHandler)&ContainedAlienReportState::btnOkClick, Options::keyOk);
	}
	else
	{
		_btnReport->onKeyboardPress((ActionHandler)&ContainedAlienReportState::btnReportClick, Options::keyOk);
	}
	_btnPromotions->setText(tr("STR_VIEW_PROMOTIONS"));
	_btnPromotions->onMouseClick((ActionHandler)&ContainedAlienReportState::btnPromotionsClick);
	_btnPromotions->setVisible(!_result.promotions.empty());
}

void ContainedAlienReportState::btnOkClick(Action*)
{
	Game* game = _game;
	Base* base = _base;
	const auto researchIds = _newResearchNotifications;
	game->popState();
	if (GeoscapeState* geoscape = game->getGeoscapeState())
	{
		geoscape->showNewResearchNotifications(base, researchIds);
	}
}

void ContainedAlienReportState::btnReportClick(Action*)
{
	for (auto it = _articleIds.rbegin(); it != _articleIds.rend(); ++it)
	{
		Ufopaedia::openArticle(_game, *it);
	}
}

void ContainedAlienReportState::btnPromotionsClick(Action*)
{
	_game->pushState(new PromotionsState(_result.promotions));
}

}
