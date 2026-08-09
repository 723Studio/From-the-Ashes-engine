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

#include "ExtendedInventoryLinksState.h"
#include "InventoryState.h"
#include "../Engine/Game.h"
#include "../Engine/Action.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the ExtendedInventoryLinksState screen.
 */
ExtendedInventoryLinksState::ExtendedInventoryLinksState(InventoryState* parent, SavedBattleGame* save, bool inBase, bool beforeMission) : _parent(parent), _save(save)
{
	_screen = false;

	// Create objects
	_window = new Window(this, 256, inBase ? 180 : 180-14, 32, inBase ? 10 : 10+14, POPUP_BOTH);
	_txtTitle = new Text(220, 17, 50, inBase ? 33 : 33+23);

	const int firstButtonY = inBase ? 50 : 76;
	_btnEquipmentSave = new TextButton(220, 12, 50, firstButtonY);
	_btnEquipmentLoad = new TextButton(220, 12, 50, firstButtonY + 13);
	_btnUfopedia = new TextButton(220, 12, 50, firstButtonY + 26);
	_btnOk = new TextButton(220, 12, 50, firstButtonY + 39);


	// Set palette
	setInterface("oxceLinks", false, _save);

	add(_window, "window", "oxceLinks");
	add(_txtTitle, "text", "oxceLinks");
	add(_btnOk, "button", "oxceLinks");

	add(_btnEquipmentSave, "button", "oxceLinks");
	add(_btnEquipmentLoad, "button", "oxceLinks");
	add(_btnUfopedia, "button", "oxceLinks");

	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, "oxceLinks");

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_EXTENDED_LINKS"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ExtendedInventoryLinksState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ExtendedInventoryLinksState::btnOkClick, Options::keyCancel);

	_btnEquipmentSave->setText(tr("STR_SAVE_EQUIPMENT_TEMPLATE"));
	_btnEquipmentSave->onMouseClick((ActionHandler)&ExtendedInventoryLinksState::btnEquipmentSaveClick);
	_btnEquipmentSave->setVisible(beforeMission);

	_btnEquipmentLoad->setText(tr("STR_LOAD_EQUIPMENT_TEMPLATE"));
	_btnEquipmentLoad->onMouseClick((ActionHandler)&ExtendedInventoryLinksState::btnEquipmentLoadClick);
	_btnEquipmentLoad->setVisible(beforeMission);

	_btnUfopedia->setText(tr("STR_UFOPAEDIA"));
	_btnUfopedia->onMouseClick((ActionHandler)&ExtendedInventoryLinksState::btnUfopediaClick);

	applyBattlescapeTheme("oxceLinks");
}

void ExtendedInventoryLinksState::btnEquipmentSaveClick(Action *)
{
	_game->popState();
	_parent->btnInventorySaveClick(nullptr);
}

void ExtendedInventoryLinksState::btnEquipmentLoadClick(Action *)
{
	_game->popState();
	_parent->btnInventoryLoadClick(nullptr);
}

void ExtendedInventoryLinksState::btnUfopediaClick(Action *)
{
	_game->popState();
	_parent->btnUfopaediaClick(nullptr);
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void ExtendedInventoryLinksState::btnOkClick(Action *)
{
	_game->popState();
}

}
