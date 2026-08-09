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
#include "CraftReturnReportState.h"
#include "GeoscapeState.h"
#include "../Basescape/BasescapeState.h"
#include "../Battlescape/PromotionsState.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Savegame/Base.h"
#include "../Savegame/Craft.h"

namespace OpenXcom
{

/**
 * Initializes the craft return report.
 */
CraftReturnReportState::CraftReturnReportState(Base *base, GeoscapeState *state, const std::vector<Craft*>& crafts, const PromotionBatch& promotions) :
	_base(base), _state(state), _promotions(promotions)
{
	_screen = false;

	_window = new Window(this, 230, 140, 45, 30, POPUP_BOTH);
	_txtTitle = new Text(161, 18, 80, 38);
	_txtCraft = new Text(43, 9, 54, 60);
	_txtRepair = new Text(36, 9, 118, 60);
	_txtRearm = new Text(36, 9, 155, 60);
	_txtRefuel = new Text(36, 9, 192, 60);
	_txtTotal = new Text(36, 9, 229, 60);
	_lstReturnedCrafts = new TextList(202, 50, 54, 70);
	_btnPromotions = new TextButton(80, 16, 176, 125);
	_btnOk = new TextButton(80, 16, 64, 146);
	_btnGotoBase = new TextButton(80, 16, 176, 146);

	setInterface("craftReturnReport");

	add(_window, "window", "craftReturnReport");
	add(_txtTitle, "text1", "craftReturnReport");
	add(_txtCraft, "text2", "craftReturnReport");
	add(_txtRepair, "text2", "craftReturnReport");
	add(_txtRearm, "text2", "craftReturnReport");
	add(_txtRefuel, "text2", "craftReturnReport");
	add(_txtTotal, "text2", "craftReturnReport");
	add(_lstReturnedCrafts, "list", "craftReturnReport");
	add(_btnPromotions, "button", "craftReturnReport");
	add(_btnOk, "button", "craftReturnReport");
	add(_btnGotoBase, "button", "craftReturnReport");

	centerAllSurfaces();
	setWindowBackground(_window, "craftReturnReport");

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_CRAFT_RETURNS_TO_BASE"));

	_txtCraft->setText(tr("STR_CRAFT"));
	_txtRepair->setAlign(ALIGN_CENTER);
	_txtRepair->setText(tr("STR_REPAIR"));
	_txtRearm->setAlign(ALIGN_CENTER);
	_txtRearm->setText(tr("STR_REARM"));
	_txtRefuel->setAlign(ALIGN_CENTER);
	_txtRefuel->setText(tr("STR_REFUEL"));
	_txtTotal->setAlign(ALIGN_CENTER);
	_txtTotal->setText(tr("STR_TOTAL"));

	_lstReturnedCrafts->setColumns(5, 64, 37, 37, 37, 27);
	_lstReturnedCrafts->setAlign(ALIGN_RIGHT);
	_lstReturnedCrafts->setAlign(ALIGN_LEFT, 0);
	_lstReturnedCrafts->setDot(true);
	_lstReturnedCrafts->setBackground(_window);
	_lstReturnedCrafts->setScrolling(true, 3);

	for (auto *craft : crafts)
	{
		const unsigned int repair = craft->calcRepairTime();
		const unsigned int rearm = craft->calcRearmTime();
		const unsigned int refuel = craft->calcRefuelTime();
		const unsigned int total = repair + rearm + refuel;
		_lstReturnedCrafts->addRow(5,
			craft->getName(_game->getLanguage()).c_str(),
			tr("STR_HOUR_SHORT").arg(repair).c_str(),
			tr("STR_HOUR_SHORT").arg(rearm).c_str(),
			tr("STR_HOUR_SHORT").arg(refuel).c_str(),
			tr("STR_HOUR_SHORT").arg(total).c_str());
	}

	_btnPromotions->setText(tr("STR_VIEW_PROMOTIONS"));
	_btnPromotions->onMouseClick((ActionHandler)&CraftReturnReportState::btnPromotionsClick);
	_btnPromotions->setVisible(!_promotions.empty());

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&CraftReturnReportState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&CraftReturnReportState::btnOkClick, Options::keyCancel);

	_btnGotoBase->setText(tr("STR_GO_TO_BASE"));
	_btnGotoBase->onMouseClick((ActionHandler)&CraftReturnReportState::btnGotoBaseClick);
	_btnGotoBase->onKeyboardPress((ActionHandler)&CraftReturnReportState::btnGotoBaseClick, Options::keyOk);
}

void CraftReturnReportState::btnOkClick(Action *)
{
	_game->popState();
}

void CraftReturnReportState::btnGotoBaseClick(Action *)
{
	_state->timerReset();
	_game->popState();
	_game->pushState(new BasescapeState(_base, _state->getGlobe()));
}

void CraftReturnReportState::btnPromotionsClick(Action *)
{
	_game->pushState(new PromotionsState(_promotions));
}

}
