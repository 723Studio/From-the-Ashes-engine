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
 */
#include <vector>
#include "../Engine/State.h"
#include "../Savegame/Promotion.h"

namespace OpenXcom
{

class Base;
class Craft;
class GeoscapeState;
class Text;
class TextButton;
class TextList;
class Window;

/**
 * Reports one or more craft returning to the same base after dogfights.
 */
class CraftReturnReportState : public State
{
private:
	Base *_base;
	GeoscapeState *_state;
	PromotionBatch _promotions;
	Window *_window;
	TextButton *_btnOk, *_btnGotoBase, *_btnPromotions;
	Text *_txtTitle, *_txtCraft, *_txtRepair, *_txtRearm, *_txtRefuel, *_txtTotal;
	TextList *_lstReturnedCrafts;

public:
	/// Creates a report for craft that returned to one base in the same processing pass.
	CraftReturnReportState(Base *base, GeoscapeState *state, const std::vector<Craft*>& crafts, const PromotionBatch& promotions);
	/// Cleans up the state.
	~CraftReturnReportState() = default;
	/// Closes the report.
	void btnOkClick(Action *action);
	/// Opens the base that received the craft.
	void btnGotoBaseClick(Action *action);
	/// Opens the exact promotion batch produced by these returns.
	void btnPromotionsClick(Action *action);
};

}
