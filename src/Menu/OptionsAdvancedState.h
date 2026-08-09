#pragma once
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
#include "OptionsBaseState.h"
#include "../Engine/OptionInfo.h"
#include <vector>

namespace OpenXcom
{

class TextList;

/**
 * Options window that displays the
 * advanced game settings.
 */
class OptionsAdvancedState : public OptionsBaseState
{
private:
	TextList *_lstOptions;
	Uint8 _colorGroup, _greyedOutColor;
	std::vector<OptionInfo> _settingsGeneral;
	std::vector<OptionInfo> _settingsGeo;
	std::vector<OptionInfo> _settingsBase;
	std::vector<OptionInfo> _settingsBattle;
	std::vector<OptionInfo> _settingsAI;
	int _offsetGeneralMin = -1;
	int _offsetGeneralMax = -1;
	int _offsetGeoMin = -1;
	int _offsetGeoMax = -1;
	int _offsetBaseMin = -1;
	int _offsetBaseMax = -1;
	int _offsetBattleMin = -1;
	int _offsetBattleMax = -1;
	int _offsetAIMin = -1;
	int _offsetAIMax = -1;

	void addSettings(const std::vector<OptionInfo> &settings);
	OptionInfo *getSetting(size_t sel);
public:
	/// Creates the Advanced state.
	OptionsAdvancedState(OptionsOrigin origin);
	/// Cleans up the Advanced state.
	~OptionsAdvancedState();
	/// Refreshes the UI.
	void init() override;
	/// Fills settings list.
	void updateList();
	/// Handler for clicking a setting on the list.
	void lstOptionsClick(Action *action);
	/// Handler for moving the mouse over a setting.
	void lstOptionsMouseOver(Action *action);
	/// Handler for moving the mouse outside the settings.
	void lstOptionsMouseOut(Action *action);
};

}
