#pragma once
/*
 * Copyright 2010-2019 OpenXcom Developers.
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
	class OptionsModsState;
	class ModInfo;
	class TextButton;
	class Window;
	class Text;

	/**
	 * Confirmation window when enabling a submod that requires a newer master package.
	 */
	class ModConfirmExtendedState : public State
	{
	private:
		OptionsModsState *_state;
		TextButton *_btnYes, *_btnNo;
		Window *_window;
		Text *_txtTitle;
	public:
		/// Creates the master-version confirmation state.
		ModConfirmExtendedState(OptionsModsState *state, const ModInfo *modInfo, const ModInfo *masterInfo = nullptr);
		/// Cleans up the Confirm OXCE state.
		~ModConfirmExtendedState();
		/// Handler for clicking the Yes button.
		void btnYesClick(Action *action);
		/// Handler for clicking the No button.
		void btnNoClick(Action *action);

		/// Check if mod is not valid.
		static bool isModNotValid(const ModInfo *modInfo, const ModInfo *masterInfo);

		/// Show confirmation when a submod requires a newer master version.
		static bool tryShowModNotValidConfirmationState(OptionsModsState *state, const ModInfo *modInfo, const ModInfo *masterInfo);
	};

}
