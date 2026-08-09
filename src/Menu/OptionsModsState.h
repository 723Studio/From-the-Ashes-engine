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
#include "OptionsBaseState.h"
#include "../Engine/ModInfo.h"
#include <string>
#include <utility>
#include <vector>

namespace OpenXcom
{

class ModConfirmExtendedState;
class Text;
class TextList;

/**
 * Options screen for managing submods compatible with the active FTA master.
 */
class OptionsModsState : public OptionsBaseState
{
	friend class ModConfirmExtendedState;
private:
	Text *_txtModName, *_txtLoadOrder, *_txtEnabled;
	TextList *_lstMods;
	std::string _curMasterId;
	std::vector<std::pair<std::string, bool> > _mods;

	std::string makeTooltip(const ModInfo &modInfo);
	void lstModsRefresh(size_t scrollLoc);
	void toggleMod();
	void moveModUp(Action *action, unsigned int row, bool max = false);
	void moveModDown(Action *action, unsigned int row, bool max = false);
public:
	/// Creates the Mods Options state.
	OptionsModsState(OptionsOrigin origin);
	/// Cleans up the Mods Options state.
	~OptionsModsState();
	/// Updates the tooltip for the hovered mod.
	void lstModsHover(Action *action);
	/// Toggles the selected mod.
	void lstModsClick(Action *action);
	/// Moves the selected mod up.
	void lstModsLeftArrowClick(Action *action);
	/// Moves the selected mod down.
	void lstModsRightArrowClick(Action *action);
	/// Handles mouse-wheel reordering over the load-order column.
	void lstModsMousePress(Action *action);
};

}
