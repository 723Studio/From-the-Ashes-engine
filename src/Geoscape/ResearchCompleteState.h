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
#include "../Engine/State.h"
#include "../Savegame/Promotion.h"
#include <string>
#include <vector>

namespace OpenXcom
{

class Game;
class Window;
class TextButton;
class Text;
class Base;
class RuleResearch;

/**
 * Window which inform the player that a research project is finished.
 * Allow him to view information about the project (Ufopaedia).
 */
class ResearchCompleteState : public State
{
	Window *_window;
	Text *_txtTitle, *_txtResearch, *_txtBase;
	TextButton *_btnReport, *_btnOk, *_btnPromotions;
	PromotionBatch _promotions;
	Base* _base;
	std::vector<std::string> _articleIds;
	std::vector<std::string> _newResearchNotifications;
	bool _containedAlienStudiesUnblocked;
public:
	/// Creates the EndResearch state.
	ResearchCompleteState(const RuleResearch* research, Base* base, const PromotionBatch& promotions,
		std::vector<std::string> articleIds,
		std::vector<std::string> newResearchNotifications = {},
		bool containedAlienStudiesUnblocked = false);
	/// Handler for clicking the OK button.
	void btnOkClick(Action *action);
	/// Handler for clicking the Report button.
	void btnReportClick(Action *action);
	/// Handler for clicking the Promotions button.
	void btnPromotionsClick(Action* action);
};

}
