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
#include "PrisonReportState.h"
#include "GeoscapeState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Mod/RuleResearch.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Basescape/PrisonManagementState.h"
#include "../Basescape/SoldiersState.h"
#include "../Engine/Options.h"
#include "../Savegame/BasePrisoner.h"
#include "../Battlescape/PromotionsState.h"

namespace OpenXcom
{
/**
 * Creates the PrisonReportState for case of prisoner's death.
 * @param prisoner - current prisoner
 * @param base - current base
 */
PrisonReportState::PrisonReportState(BasePrisoner* prisoner, Base* base) :
		_research(nullptr), _bonus(nullptr), _prisoner(prisoner), _base(base), _soldier(nullptr), _reportCase(PRISONER_REPORT_CASE_DEATH)
{
	initUI();
}

/**
 * Creates the PrisonReportState for case of finished interrogation.
 * @param research - main interrogation research rule
 * @param bonus - bonus interrogation research rule
 * @param prisoner - current prisoner
 * @param base - current base
 */
PrisonReportState::PrisonReportState(const RuleResearch* research, const RuleResearch* bonus,
	BasePrisoner* prisoner, Base* base, const PromotionBatch& promotions,
	std::vector<std::string> articleIds, std::vector<std::string> newResearchNotifications,
	bool containedAlienStudiesUnblocked) :
		_research(research), _bonus(bonus), _prisoner(prisoner), _base(base), _soldier(0),
		_reportCase(PRISONER_REPORT_CASE_INTERROGATION), _promotions(promotions),
		_articleIds(std::move(articleIds)),
		_newResearchNotifications(std::move(newResearchNotifications)),
		_containedAlienStudiesUnblocked(containedAlienStudiesUnblocked)
{
	initUI();
}

PrisonReportState::PrisonReportState(Soldier* soldier, BasePrisoner* prisoner, Base* base) :
	_research(nullptr), _bonus(nullptr), _prisoner(prisoner), _base(base), _soldier(soldier), _reportCase(PRISONER_REPORT_RECRUITING)
{
	initUI();
}

/**
 * Initializes all the elements in the PrisonReportState screen.
 * @param reportCase - PrisonerReportCase for desired display.
 */
void PrisonReportState::initUI()
{
	_screen = false;

	// Create objects
	_window = new Window(this, 230, 140, 45, 30, POPUP_BOTH);
	_btnOk = new TextButton(80, 16, 64, 146);
	_btnDetails = new TextButton(80, 16, 176, 146);
	_btnPromotions = new TextButton(80, 16, 176, 125);
	_txtTitle = new Text(230, 17, 45,
		_containedAlienStudiesUnblocked ? 58 : 70);
	_txtDescription = new Text(230,
		_containedAlienStudiesUnblocked ? 44 : 32, 45,
		_containedAlienStudiesUnblocked ? 78 : 96);

	// Set palette
	std::string interface = "geoPrisonerReportNormal";
	if (_reportCase == PRISONER_REPORT_CASE_DEATH)
	{
		interface = "geoPrisonerReportDeath";
	}
	setInterface(interface);
	add(_window, "window", interface);
	add(_btnOk, "button", interface);
	add(_btnDetails, "button", interface);
	add(_btnPromotions, "button", interface);
	add(_txtTitle, "text1", interface);
	add(_txtDescription, "text2", interface);

	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, interface);

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&PrisonReportState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&PrisonReportState::btnOkClick, Options::keyCancel);

	_btnPromotions->setText(tr("STR_VIEW_PROMOTIONS"));
	_btnPromotions->onMouseClick((ActionHandler)&PrisonReportState::btnPromotionsClick);
	_btnPromotions->setVisible(_reportCase == PRISONER_REPORT_CASE_INTERROGATION && !_promotions.empty());

	std::string btnName = "NONE", desription = "NONE";
	bool detailsAvailable = true;
	std::ostringstream ss;
	switch (_reportCase) {
	case PRISONER_REPORT_CASE_DEATH:
		btnName = "STR_GO_TO_BASE";
		ss << tr("STR_PRISONER_DIED_PT1");
		ss << " ";
		ss << _prisoner->getNameAndDesignation(_game->getLanguage());
		ss << " ";
		ss << tr("STR_PRISONER_DIED_PT2");
		desription = ss.str();
		break;
	case PRISONER_REPORT_CASE_INTERROGATION:
		btnName = "STR_VIEW_REPORT";
		detailsAvailable = !_articleIds.empty();
		ss << tr("STR_PRISONER_INTERROGATED_PT1");
		ss << " ";
		ss << _prisoner->getNameAndDesignation(_game->getLanguage());
		ss << " ";
		if (_research != nullptr || _bonus != nullptr)
		{
			if (!_prisoner->isInterrogationDone())
				ss << tr("STR_PRISONER_INTERROGATED_PT2");
			else
				ss << tr("STR_PRISONER_INTERROGATED_PT2_1");
		}
		else
			ss << tr("STR_PRISONER_INTERROGATED_PT3");
		desription = ss.str();
		break;
	case PRISONER_REPORT_RECRUITING:
		if (_soldier != 0)
		{
			btnName = "STR_VIEW_SOLDIER";
			ss << tr("STR_PRISONER_RECRUITING_PT1");
			ss << " ";
			ss << _prisoner->getNameAndDesignation(_game->getLanguage());
			ss << " ";
			ss << tr("STR_PRISONER_RECRUITING_PT2");
			ss << " ";
			ss << _soldier->getName();
			ss << " ";
			ss << tr("STR_PRISONER_RECRUITING_PT3");
			desription = ss.str();
		}
		else
		{
			ss << tr("STR_PRISONER_RECRUITING_PT1");
			ss << " ";
			ss << _prisoner->getNameAndDesignation(_game->getLanguage());
			ss << " ";
			ss << tr("STR_PRISONER_RECRUITING_PT2");
			ss << " ";
			ss << tr("STR_PRISONER_RECRUITING_PT4");
			desription = ss.str();
		}

		break;
	}
	if (_containedAlienStudiesUnblocked)
	{
		ss << "\n" << tr("STR_CONTAINED_ALIEN_STUDIES_CAN_CONTINUE");
		desription = ss.str();
	}
	_btnDetails->setText(tr(btnName));
	_btnDetails->onMouseClick((ActionHandler)&PrisonReportState::btnDetailsClick);
	_btnDetails->setVisible(detailsAvailable);
	if (detailsAvailable)
	{
		_btnDetails->onKeyboardPress((ActionHandler)&PrisonReportState::btnDetailsClick, Options::keyOk);
	}
	else
	{
		_btnOk->onKeyboardPress((ActionHandler)&PrisonReportState::btnOkClick, Options::keyOk);
	}

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_PRISON_REPORT"));

	_txtDescription->setAlign(ALIGN_CENTER);
	if (!_containedAlienStudiesUnblocked)
	{
		_txtDescription->setBig();
	}
	_txtDescription->setWordWrap(true);
	_txtDescription->setText(desription);
}

/**
 * return to the previous screen
 * @param action Pointer to an action.
 */
void PrisonReportState::btnOkClick(Action *)
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

/**
 * open the Ufopaedia to the entry about the Research.
 * @param action Pointer to an action.
 */
void PrisonReportState::btnDetailsClick(Action *)
{
	if (_reportCase == PRISONER_REPORT_CASE_DEATH)
	{
		Game* game = _game;
		Base* base = _base;
		game->popState();
		game->pushState(new PrisonManagementState(base));
	}
	else if (_reportCase == PRISONER_REPORT_CASE_INTERROGATION)
	{
		for (auto article = _articleIds.rbegin(); article != _articleIds.rend(); ++article)
		{
			Ufopaedia::openArticle(_game, *article);
		}
	}
	else if (_reportCase == PRISONER_REPORT_RECRUITING)
	{
		Game* game = _game;
		Base* base = _base;
		game->popState();
		game->pushState(new SoldiersState(base));
	}
}

void PrisonReportState::btnPromotionsClick(Action *)
{
	_game->pushState(new PromotionsState(_promotions));
}

}
