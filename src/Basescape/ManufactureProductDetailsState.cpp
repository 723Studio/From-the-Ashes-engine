/*
 * Copyright 2010-2022 OpenXcom Developers.
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
#include "ManufactureProductDetailsState.h"
#include <algorithm>
#include <sstream>
#include "../Interface/Window.h"
#include "../Interface/TextButton.h"
#include "../Interface/Text.h"
#include "../Engine/Game.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Unicode.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleManufacture.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the productions start screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param item The RuleManufacture to produce.
 */
ManufactureProductDetailsState::ManufactureProductDetailsState(Base *base, const RuleManufacture *item) :  _base(base), _item(item)
{
	_screen = false;

	_window = new Window(this, 168, 200, 152, 0);
	_btnOk = new TextButton(146, 16, 163, 175);
	_txtTitle = new Text(154, 17, 159, 7);
	_txtManHour = new Text(154, 9, 159, 27);
	_txtCost = new Text(154, 9, 159, 37);
	_txtWorkSpace = new Text(154, 9, 159, 47);
	_txtReqStatsHeader = new Text(154, 9, 159, 59);
	_txtReqStats = new Text(154, 19, 159, 69);

	// Set palette
	setInterface("allocateManufacture");

	add(_window, "window", "allocateManufacture");
	add(_txtTitle, "text", "allocateManufacture");
	add(_txtManHour, "text", "allocateManufacture");
	add(_txtCost, "text", "allocateManufacture");
	add(_txtWorkSpace, "text", "allocateManufacture");
	add(_txtReqStatsHeader, "text", "allocateManufacture");
	add(_txtReqStats, "text", "allocateManufacture");
	add(_btnOk, "button", "allocateManufacture");

	centerAllSurfaces();

	setWindowBackground(_window, "allocateManufacture");

	_txtTitle->setText(tr("STR_PRODUCTION_STATE_INFO"));
	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);

	_txtManHour->setText(tr("STR_BASE_LABOR_COSTS").arg(_item->getManufactureTime() / 100));
	_txtReqStatsHeader->setText(tr("STR_REQUIRED_STATS"));

	_txtReqStats->setText(generateStatsList());
	_txtReqStats->setWordWrap(true);

	if (_item->getFundsGain() > 0)
	{
		_txtCost->setText(tr("STR_FUNDS_GAIN").arg(Unicode::formatFunding(_item->getFundsGain())));
	}
	else
	{
		_txtCost->setText(tr("STR_COST_PER_UNIT_").arg(Unicode::formatFunding(_item->getManufactureCost())));
	}

	_txtWorkSpace->setText(tr("STR_WORK_SPACE_REQUIRED").arg(_item->getRequiredSpace()));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ManufactureProductDetailsState::btnOKClick);
	_btnOk->onKeyboardPress((ActionHandler)&ManufactureProductDetailsState::btnOKClick, Options::keyOk);
	_btnOk->onKeyboardPress((ActionHandler)&ManufactureProductDetailsState::btnOKClick, Options::keyCancel);
}

/**
 * Returns to previous screen.
 * @param action A pointer to an Action.
 */
void ManufactureProductDetailsState::btnOKClick(Action *)
{
	_game->popState();
}


std::string ManufactureProductDetailsState::generateStatsList()
{
	std::ostringstream ss;
	const UnitStats stats = _item->getStats();
	std::vector<std::pair<int, std::string>> statList;
	auto addStat = [&](int value, const std::string& name)
	{
		if (value > 0)
		{
			statList.push_back(std::make_pair(value, name));
		}
	};

	addStat(stats.weaponry, tr("STR_WEAPONRY_LC"));
	addStat(stats.explosives, tr("STR_EXPLOSIVES_LC"));
	addStat(stats.microelectronics, tr("STR_MICROELECTRONICS_LC"));
	addStat(stats.metallurgy, tr("STR_METALLURGY_LC"));
	addStat(stats.processing, tr("STR_PROCESSING_LC"));
	addStat(stats.hacking, tr("STR_HACKING_LC"));
	addStat(stats.robotics, tr("STR_ROBOTICS_LC"));
	addStat(stats.alienTech, tr("STR_ALIEN_TECH_LC"));
	addStat(stats.reverseEngineering, tr("STR_REVERSE_ENGINEERING_LC"));

	std::stable_sort(statList.begin(), statList.end(),
		[](const auto& left, const auto& right)
		{
			return left.first > right.first;
		});
	for (size_t i = 0; i < statList.size(); ++i)
	{
		if (i > 0)
		{
			ss << ", ";
		}
		ss << statList[i].second;
	}
	if (!statList.empty())
	{
		ss << ".";
	}
	return ss.str();
}

}
