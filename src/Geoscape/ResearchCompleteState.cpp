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
#include "ResearchCompleteState.h"
#include <sstream>
#include "GeoscapeState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Mod/RuleResearch.h"
#include "../Ufopaedia/Ufopaedia.h"
#include "../Engine/Options.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedGame.h"
#include "../Battlescape/PromotionsState.h"

namespace OpenXcom
{
/**
 * Initializes all the elements in the EndResearch screen.
 * @param research Pointer to the research project.
 */
ResearchCompleteState::ResearchCompleteState(const RuleResearch* research, Base* base,
	const PromotionBatch& promotions, std::vector<std::string> articleIds,
	std::vector<std::string> newResearchNotifications,
	bool containedAlienStudiesUnblocked) :
	_promotions(promotions), _base(base), _articleIds(std::move(articleIds)),
	_newResearchNotifications(std::move(newResearchNotifications)),
	_containedAlienStudiesUnblocked(containedAlienStudiesUnblocked)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 230, 140, 45, 30, POPUP_BOTH);
	_btnOk = new TextButton(80, 16, 64, 146);
	_btnReport = new TextButton(80, 16, 176, 146);
	_btnPromotions = new TextButton(80, 16, 176, 125);
	_txtBase = new Text(230, 9, 45, 40);
	_txtTitle = new Text(230, 17, 45,
		_containedAlienStudiesUnblocked ? 58 : 70);
	_txtResearch = new Text(230,
		_containedAlienStudiesUnblocked ? 44 : 32, 45,
		_containedAlienStudiesUnblocked ? 78 : 96);

	// Set palette
	setInterface("geoResearchComplete");

	add(_window, "window", "geoResearchComplete");
	add(_btnOk, "button", "geoResearchComplete");
	add(_btnReport, "button", "geoResearchComplete");
	add(_btnPromotions, "button", "geoResearchComplete");
	add(_txtBase, "text1", "geoResearchComplete");
	add(_txtTitle, "text1", "geoResearchComplete");
	add(_txtResearch, "text2", "geoResearchComplete");

	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, "geoResearchComplete");

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ResearchCompleteState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ResearchCompleteState::btnOkClick, Options::keyCancel);

	_btnReport->setText(tr("STR_VIEW_REPORTS"));
	_btnReport->onMouseClick((ActionHandler)&ResearchCompleteState::btnReportClick);
	if (!_articleIds.empty())
	{
		_btnReport->onKeyboardPress((ActionHandler)&ResearchCompleteState::btnReportClick, Options::keyOk);
	}
	else
	{
		_btnReport->setVisible(false);
		_btnOk->onKeyboardPress((ActionHandler)&ResearchCompleteState::btnOkClick, Options::keyOk);
	}

	_btnPromotions->setText(tr("STR_VIEW_PROMOTIONS"));
	_btnPromotions->onMouseClick((ActionHandler)&ResearchCompleteState::btnPromotionsClick);
	_btnPromotions->setVisible(!_promotions.empty());

	_txtBase->setAlign(ALIGN_CENTER);
	_txtBase->setText(base->getName());

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_RESEARCH_COMPLETED"));

	_txtResearch->setAlign(ALIGN_CENTER);
	if (!_containedAlienStudiesUnblocked)
	{
		_txtResearch->setBig();
	}
	_txtResearch->setWordWrap(true);
	std::ostringstream researchText;
	if (research)
	{
		researchText << tr(research->getName());
	}
	if (_containedAlienStudiesUnblocked)
	{
		if (research) researchText << "\n";
		researchText << tr("STR_CONTAINED_ALIEN_STUDIES_CAN_CONTINUE");
	}
	_txtResearch->setText(researchText.str());
}

/**
 * return to the previous screen
 * @param action Pointer to an action.
 */
void ResearchCompleteState::btnOkClick(Action *)
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
void ResearchCompleteState::btnReportClick(Action *)
{
	for (auto article = _articleIds.rbegin(); article != _articleIds.rend(); ++article)
	{
		Ufopaedia::openArticle(_game, *article);
	}
}

void ResearchCompleteState::btnPromotionsClick(Action* action)
{
	_game->pushState(new PromotionsState(_promotions));
}

}
