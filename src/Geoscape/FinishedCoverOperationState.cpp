/*
 * Copyright 2010-2020 OpenXcom Developers.
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
#include "FinishedCoverOperationState.h"
#include "GeoscapeState.h"
#include "FinishedCoverOperationDetailsState.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleCovertOperation.h"
#include "../Savegame/CovertOperation.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedGame.h"
#include "../Battlescape/PromotionsState.h"
#include "../Ufopaedia/Ufopaedia.h"


namespace OpenXcom
{

	/**
	 * Initializes all the elements in the FinishedCoverOperationState window.
	 * @param operation Pointer to the operation.
	 * @param result - comes true if success operation, false if it was failed.
	 */
	FinishedCoverOperationState::FinishedCoverOperationState(CovertOperation* operation, bool result) :
		_operation(operation), _rule(operation->getRules()),
		_promotions(operation->getResults() ? operation->getResults()->getPromotions() : PromotionBatch{})
	{
		if (operation->getResults())
		{
			_newResearchNotifications = operation->getResults()->getNewResearchNotifications();
			_researchArticleIds = operation->getResults()->getResearchArticleIds();
			_containedAlienStudiesUnblocked =
				operation->getResults()->getContainedAlienStudiesUnblocked();
		}
		_screen = false;
		// Load result rules and stats
		std::string ui;
		std::string description;
		std::string backgrd;
		std::string resultTxt;
		const bool cancelled = operation->isCancelled();
		if (cancelled)
		{
			// Reuse the failure palette, but cancellation is not a failed result
			// and deliberately has no failure music or side effects.
			ui = "covertOperationFinishFailure";
			resultTxt = tr("STR_OPERATION_CANCELLED");
			description = _rule->getCancelledDescription();
		}
		else if (result)
		{
			ui = "covertOperationFinishSuccess";
			resultTxt = tr("STR_SUCCESS_US");
			description = _rule->getSuccessDescription();
			backgrd = _rule->getSuccessBackground();
			_music = _rule->getSuccessMusic();
		}
		else
		{
			ui = "covertOperationFinishFailure";
			resultTxt = tr("STR_FAILURE_US");
			description = _rule->getFailureDescription();
			backgrd = _rule->getFailureBackground();
			_music = _rule->getFailureMusic();
		}
		// Create objects
		_window = new Window(this, 256, 200, 0, 0, POPUP_BOTH);
		_txtTitle = new Text(240, 17, 8, 10);
		_txtResult = new Text(240, 9, 8, 32);
		_txtBase = new Text(240, 9, 8, 44);
		_txtMessage = new Text(240,
			(_promotions.empty() && _researchArticleIds.empty()) ? 113 : 88, 8, 58);
		_btnOk = new TextButton(cancelled ? 240 : 115, 18, cancelled ? 8 : 133, 174);
		_btnDetails = new TextButton(115, 18, 8, 174);
		_btnReport = new TextButton(115, 18, 8, 151);
		_btnPromotions = new TextButton(115, 18, 133, 151);

		// Set palette
		setInterface(ui);

		add(_window, "window", ui);
		add(_txtTitle, "text1", ui);
		add(_txtResult, "text2", ui);
		add(_txtBase, "text2", ui);
		add(_txtMessage, "text2", ui);
		add(_btnOk, "button", ui);
		add(_btnDetails, "button", ui);
		add(_btnReport, "button", ui);
		add(_btnPromotions, "button", ui);

		centerAllSurfaces();

		// Set up objects
		
		if (!backgrd.empty())
		{
			_window->setBackground(_game->getMod()->getSurface(backgrd));
		}
		else
		{
			setWindowBackground(_window, ui);
		}

		_txtTitle->setAlign(ALIGN_CENTER);
		_txtTitle->setBig();
		_txtTitle->setWordWrap(true);
		_txtTitle->setText(tr(operation->getOperationName()));

		std::ostringstream ss1;
		ss1 << tr("STR_OPERATION_RESULT") << ">" << Unicode::TOK_COLOR_FLIP << resultTxt;
		_txtResult->setText(ss1.str());

		std::ostringstream ss2;
		ss2 << tr("STR_OPERATION_BASE") << ">" << operation->getBase()->getName();
		_txtBase->setText(ss2.str());

		_txtMessage->setVerticalAlign(ALIGN_TOP);
		_txtMessage->setWordWrap(true);
		std::ostringstream reportText;
		reportText << tr(description);
		if (_containedAlienStudiesUnblocked)
		{
			reportText << "\n" << tr("STR_CONTAINED_ALIEN_STUDIES_CAN_CONTINUE");
		}
		_txtMessage->setText(reportText.str());

		_btnOk->setText(tr("STR_OK"));
		_btnOk->onMouseClick((ActionHandler)&FinishedCoverOperationState::btnOkClick);
		_btnOk->onKeyboardPress((ActionHandler)&FinishedCoverOperationState::btnOkClick, Options::keyOk);

		_btnDetails->setText(tr("STR_DETAILS_BUTTON"));
		_btnDetails->onMouseClick((ActionHandler)&FinishedCoverOperationState::btnDetailsClick);
		_btnDetails->setVisible(!cancelled);

		_btnReport->setText(tr("STR_VIEW_REPORT"));
		_btnReport->onMouseClick((ActionHandler)&FinishedCoverOperationState::btnReportClick);
		_btnReport->setVisible(!_researchArticleIds.empty());

		_btnPromotions->setText(tr("STR_VIEW_PROMOTIONS"));
		_btnPromotions->onMouseClick((ActionHandler)&FinishedCoverOperationState::btnPromotionsClick);
		_btnPromotions->setVisible(!_promotions.empty());
	}

	/**
	 *
	 */
	FinishedCoverOperationState::~FinishedCoverOperationState()
	{
		// Empty by design
	}

	/**
	 * Initializes the state.
	 */
	void FinishedCoverOperationState::init()
	{
		State::init();
		if (!_music.empty())
		{
			_game->getMod()->playMusic(_music);
		}
	}

	/**
	 * Closes the window and shows a pedia article if needed.
	 * @param action Pointer to an action.
	 */
	void FinishedCoverOperationState::btnOkClick(Action*)
	{
		Game* game = _game;
		CovertOperation* operation = _operation;
		Base* operationBase = operation->getBase();
		const auto newResearch = _newResearchNotifications;
		game->popState();
		operation->finishOperation();
		operationBase->removeCovertOperation(operation);
		if (GeoscapeState* geoscape = game->getGeoscapeState())
		{
			geoscape->showNewResearchNotifications(operationBase, newResearch);
		}
	}
	/**
	 * Shows Finished Covert Opration details.
	 * @param action Pointer to an action.
	 */
	void FinishedCoverOperationState::btnDetailsClick(Action* action)
	{
		_game->pushState(new FinishedCoverOperationDetailsState(_operation));
	}

	void FinishedCoverOperationState::btnReportClick(Action*)
	{
		for (auto article = _researchArticleIds.rbegin(); article != _researchArticleIds.rend(); ++article)
		{
			Ufopaedia::openArticle(_game, *article);
		}
	}

	void FinishedCoverOperationState::btnPromotionsClick(Action*)
	{
		_game->pushState(new PromotionsState(_promotions));
	}

}
