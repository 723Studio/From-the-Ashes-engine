#pragma once
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
#include "../Engine/State.h"

namespace OpenXcom
{

class TextButton;
class Window;

/**
 * Development tools window available from the main menu in debug mode.
 */
class DevelopmentToolsState : public State
{
private:
	Window *_window;
	TextButton *_btnNewBattle, *_btnMapEditor, *_btnClose;
public:
	/// Creates the Development Tools state.
	DevelopmentToolsState();
	/// Cleans up the Development Tools state.
	~DevelopmentToolsState();
	/// Opens the New Battle screen.
	void btnNewBattleClick(Action *action);
	/// Opens the Map Editor.
	void btnMapEditorClick(Action *action);
	/// Closes the window.
	void btnCloseClick(Action *action);
};

}
