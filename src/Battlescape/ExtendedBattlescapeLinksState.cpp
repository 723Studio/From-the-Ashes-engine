/*
 * Copyright 2010-2021 OpenXcom Developers.
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

#include "ExtendedBattlescapeLinksState.h"
#include "BattlescapeState.h"
#include "BriefingState.h"
#include "InfoboxState.h"
#include "Map.h"
#include "TurnDiaryState.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/Options.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Battlescape/ExperienceOverviewState.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the ExtendedBattlescapeLinksState screen.
 */
ExtendedBattlescapeLinksState::ExtendedBattlescapeLinksState(BattlescapeState* parent, SavedBattleGame* save) : _parent(parent), _save(save)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 256, 180, 32, 10, POPUP_BOTH);
	_txtTitle = new Text(220, 17, 50, 33);
	_btnTouch = new TextButton(220, 16, 50, 50);
	_btnNightVision = new TextButton(220, 16, 50, 69);
	_btnTurnDiary = new TextButton(220, 16, 50, 88);
	_btnBriefing = new TextButton(220, 16, 50, 107);
	_btnExperienceOverview = new TextButton(220, 16, 50, 126);
	_btnMultilevel = new TextButton(220, 16, 50, 145);
	_btnOk = new TextButton(220, 16, 50, 164);
	// Set palette
	setInterface("oxceLinks", false, _save);

	add(_window, "window", "oxceLinks");
	add(_txtTitle, "text", "oxceLinks");
	add(_btnOk, "button", "oxceLinks");

	add(_btnTouch, "button", "oxceLinks");
	add(_btnNightVision, "button", "oxceLinks");
	add(_btnTurnDiary, "button", "oxceLinks");
	add(_btnBriefing, "button", "oxceLinks");
	add(_btnExperienceOverview, "button", "oxceLinks");
	add(_btnMultilevel, "button", "oxceLinks");

	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, "oxceLinks");

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_EXTENDED_LINKS"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ExtendedBattlescapeLinksState::btnOkClick, Options::keyCancel);

	_btnTouch->setText(tr("STR_TOGGLE_TOUCH_BUTTONS"));
	_btnTouch->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnTouchClick);

	_btnNightVision->setText(tr("STR_TOGGLE_NIGHT_VISION"));
	_btnNightVision->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnNightVisionClick);

	_btnTurnDiary->setText(tr("STR_HIT_LOG"));
	_btnTurnDiary->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnTurnDiaryClick);

	_btnBriefing->setText(tr("STR_BRIEFING"));
	_btnBriefing->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnBriefingClick);

	_btnExperienceOverview->setText(tr("STR_EXPERIENCE_OVERVIEW"));
	_btnExperienceOverview->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnExperienceOverviewClick);

	_btnMultilevel->setText(tr("STR_MULTI_LEVEL_VIEW"));
	_btnMultilevel->onMouseClick((ActionHandler)&ExtendedBattlescapeLinksState::btnMultilevelClick);

	applyBattlescapeTheme("oxceLinks");
}

void ExtendedBattlescapeLinksState::btnTouchClick(Action *)
{
	_game->popState();
	_parent->toggleTouchButtons(false, false);
}

void ExtendedBattlescapeLinksState::btnNightVisionClick(Action *)
{
	_game->popState();
	_parent->btnNightVisionClick(nullptr);
}


void ExtendedBattlescapeLinksState::btnTurnDiaryClick(Action *)
{
	_game->popState();
	if (Options::oxceDisableHitLog)
	{
		_game->pushState(new InfoboxState(tr("STR_THIS_FEATURE_IS_DISABLED_4")));
	}
	else
	{
		// turn diary
		_game->pushState(new TurnDiaryState(_save->getHitLog()));
	}
}

void ExtendedBattlescapeLinksState::btnBriefingClick(Action *)
{
	_game->popState();
	_game->pushState(new BriefingState(0, 0, true));
}

void ExtendedBattlescapeLinksState::btnExperienceOverviewClick(Action *)
{
	_game->popState();
	_game->pushState(new ExperienceOverviewState(_parent));
}

void ExtendedBattlescapeLinksState::btnMultilevelClick(Action*)
{
	_game->popState();
	_parent->btnShowLayersClickOrig(nullptr);
	return;
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ExtendedBattlescapeLinksState::btnOkClick(Action *)
{
	_game->popState();
}

}
