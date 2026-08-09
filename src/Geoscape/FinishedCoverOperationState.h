#pragma once
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
#include "../Engine/State.h"
#include "../Savegame/Promotion.h"

namespace OpenXcom
{

	class TextButton;
	class Window;
	class Text;
	class CovertOperation;
	class RuleCovertOperation;
	class FinishedCoverOperationDetailsState;

	/**
	 * Displays info about complete Covert Operations.
	 */
	class FinishedCoverOperationState : public State
	{
	private:
		Window* _window;
		Text* _txtTitle, *_txtResult, *_txtBase, *_txtMessage;
		TextButton* _btnOk, *_btnDetails, *_btnReport, *_btnPromotions;

		std::string _music;
		std::vector<std::string> _researchArticleIds;
		CovertOperation* _operation;
		const RuleCovertOperation* _rule;
		PromotionBatch _promotions;
		std::vector<std::string> _newResearchNotifications;
		bool _containedAlienStudiesUnblocked = false;

	public:
		/// Creates the FinishedCoverOperationState.
		FinishedCoverOperationState(CovertOperation* operation, bool result);
		/// Cleans up the FinishedCoverOperationState.
		~FinishedCoverOperationState();
		/// Initializes the state.
		void init() override;
		/// Handler for clicking the OK button.
		void btnOkClick(Action* action);
		/// Handler for clicking the DETAILS button.
		void btnDetailsClick(Action* action);
		/// Handler for clicking the VIEW REPORT button.
		void btnReportClick(Action* action);
		/// Handler for clicking the PROMOTIONS button.
		void btnPromotionsClick(Action* action);
	};

}
