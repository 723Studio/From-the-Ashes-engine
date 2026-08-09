/*
 * Copyright 2010-2026 OpenXcom Developers.
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
#include "DevelopmentToolsState.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "MapEditorMenuState.h"
#include "NewBattleState.h"

namespace OpenXcom
{

DevelopmentToolsState::DevelopmentToolsState()
{
	_screen = false;

	_window = new Window(this, 87, 160, 32, 20, POPUP_BOTH);
	_btnNewBattle = new TextButton(66, 20, 43, 55);
	_btnMapEditor = new TextButton(66, 20, 43, 28);
	_btnClose = new TextButton(66, 20, 43, 152);

	setInterface("mainMenu", false);

	add(_window, "window", "mainMenu");
	add(_btnNewBattle, "button", "mainMenu");
	add(_btnMapEditor, "button", "mainMenu");
	add(_btnClose, "button", "mainMenu");

	centerAllSurfaces();
	setWindowBackground(_window, "mainMenu");

	_btnNewBattle->setText(tr("STR_NEW_BATTLE"));
	_btnNewBattle->onMouseClick((ActionHandler)&DevelopmentToolsState::btnNewBattleClick);

	_btnMapEditor->setText(tr("STR_MAP_EDITOR"));
	_btnMapEditor->onMouseClick((ActionHandler)&DevelopmentToolsState::btnMapEditorClick);

	_btnClose->setText(tr("STR_CLOSE"));
	_btnClose->onMouseClick((ActionHandler)&DevelopmentToolsState::btnCloseClick);
	_btnClose->onKeyboardPress((ActionHandler)&DevelopmentToolsState::btnCloseClick, Options::keyCancel);
}

DevelopmentToolsState::~DevelopmentToolsState() {}

void DevelopmentToolsState::btnNewBattleClick(Action *)
{
	_game->popState();
	_game->pushState(new NewBattleState);
}

void DevelopmentToolsState::btnMapEditorClick(Action *)
{
	_game->popState();
	_game->pushState(new MapEditorMenuState);
}

void DevelopmentToolsState::btnCloseClick(Action *)
{
	_game->popState();
}

}
