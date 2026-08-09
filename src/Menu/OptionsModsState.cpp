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
#include "OptionsModsState.h"
#include "ModConfirmExtendedState.h"
#include <SDL.h>
#include "../Engine/Action.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"

namespace OpenXcom
{

OptionsModsState::OptionsModsState(OptionsOrigin origin) : OptionsBaseState(origin), _curMasterId(Options::getActiveMaster())
{
	setCategory(_btnMods);

	_txtModName = new Text(93, 8, 94, 8);
	_txtLoadOrder = new Text(59, 8, 197, 8);
	_txtEnabled = new Text(55, 8, 257, 8);
	_lstMods = new TextList(200, 128, 94, 17);

	add(_txtModName, "text", "optionsMenu");
	add(_txtLoadOrder, "text", "optionsMenu");
	add(_txtEnabled, "text", "optionsMenu");
	add(_lstMods, "optionLists", "optionsMenu");

	centerAllSurfaces();

	_txtModName->setText(tr("STR_MOD_NAME"));
	_txtLoadOrder->setText(tr("STR_LOAD_ORDER"));
	_txtLoadOrder->setAlign(ALIGN_RIGHT);
	_txtEnabled->setText(tr("STR_ENABLED_QUESTION"));

	_lstMods->setArrowColumn(139, ARROW_VERTICAL);
	_lstMods->setColumns(3, 138, 25, 37);
	_lstMods->setAlign(ALIGN_RIGHT, 1);
	_lstMods->setSelectable(true);
	_lstMods->setBackground(_window);
	_lstMods->setWordWrap(true);
	_lstMods->onMouseClick((ActionHandler)&OptionsModsState::lstModsClick);
	_lstMods->onLeftArrowClick((ActionHandler)&OptionsModsState::lstModsLeftArrowClick);
	_lstMods->onRightArrowClick((ActionHandler)&OptionsModsState::lstModsRightArrowClick);
	_lstMods->onMousePress((ActionHandler)&OptionsModsState::lstModsMousePress);
	_lstMods->onMouseIn((ActionHandler)&OptionsModsState::txtTooltipIn);
	_lstMods->onMouseOut((ActionHandler)&OptionsModsState::txtTooltipOut);
	_lstMods->onMouseOver((ActionHandler)&OptionsModsState::lstModsHover);

	lstModsRefresh(0);
}

OptionsModsState::~OptionsModsState() {}

std::string OptionsModsState::makeTooltip(const ModInfo &modInfo)
{
	return tr("STR_MODS_TOOLTIP").arg(modInfo.getVersionDisplay()).arg(modInfo.getAuthor()).arg(modInfo.getDescription());
}

void OptionsModsState::lstModsRefresh(size_t scrollLoc)
{
	_lstMods->clearList();
	_mods.clear();

	for (const auto &pair : Options::mods)
	{
		auto search = Options::getModInfos().find(pair.first);
		if (search == Options::getModInfos().end())
		{
			continue;
		}

		const ModInfo &modInfo = search->second;
		if (modInfo.isMaster() || !modInfo.canActivate(_curMasterId))
		{
			continue;
		}

		_lstMods->addRow(3, modInfo.getName().c_str(), "", (pair.second ? tr("STR_YES").c_str() : tr("STR_NO").c_str()));
		_mods.push_back(pair);
	}

	_lstMods->scrollTo(scrollLoc);
}

void OptionsModsState::lstModsHover(Action *)
{
	size_t selectedRow = _lstMods->getSelectedRow();
	if (selectedRow != (size_t)-1 && selectedRow < _mods.size())
	{
		_txtTooltip->setText(makeTooltip(Options::getModInfos().at(_mods[selectedRow].first)));
	}
}

void OptionsModsState::lstModsClick(Action *action)
{
	if (action->getAbsoluteXMouse() >= _lstMods->getArrowsLeftEdge() &&
		action->getAbsoluteXMouse() <= _lstMods->getArrowsRightEdge())
	{
		return;
	}

	size_t selectedRow = _lstMods->getSelectedRow();
	if (selectedRow >= _mods.size())
	{
		return;
	}

	std::pair<std::string, bool> &mod = _mods.at(selectedRow);
	if (!mod.second)
	{
		const ModInfo *modInfo = &Options::getModInfos().at(mod.first);
		if (ModConfirmExtendedState::tryShowModNotValidConfirmationState(this, modInfo, Options::getActiveMasterInfo()))
		{
			return;
		}
	}

	toggleMod();
}

void OptionsModsState::toggleMod()
{
	std::pair<std::string, bool> &mod = _mods.at(_lstMods->getSelectedRow());

	for (auto &pair : Options::mods)
	{
		if (mod.first == pair.first)
		{
			mod.second = !mod.second;
			pair.second = mod.second;
			_lstMods->setCellText(_lstMods->getSelectedRow(), 2, (mod.second ? tr("STR_YES") : tr("STR_NO")));
			break;
		}
	}

	Options::reload = true;
}

static void moveAbove(const std::pair<std::string, bool> &source, const std::pair<std::string, bool> &destination)
{
	for (auto i = Options::mods.begin(); i != Options::mods.end(); ++i)
	{
		if (destination.first == i->first)
		{
			Options::mods.insert(i, source);
			break;
		}
	}

	for (auto i = Options::mods.rbegin(); i != Options::mods.rend(); ++i)
	{
		if (source.first == i->first)
		{
			Options::mods.erase(i.base() - 1);
			break;
		}
	}
}

void OptionsModsState::lstModsLeftArrowClick(Action *action)
{
	unsigned int row = _lstMods->getSelectedRow();
	if (row == 0 || row >= _mods.size())
	{
		return;
	}

	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		moveModUp(action, row);
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		moveModUp(action, row, true);
	}
}

void OptionsModsState::moveModUp(Action *action, unsigned int row, bool max)
{
	if (max)
	{
		moveAbove(_mods.at(row), _mods.front());
		lstModsRefresh(_lstMods->getScroll());
	}
	else
	{
		int currentScroll = _lstMods->getScroll();
		int targetScroll = 0;
		for (size_t i = 0; i < row - 1; ++i)
		{
			targetScroll += _lstMods->getNumTextLines(i);
		}

		if (currentScroll < targetScroll)
		{
			int yDifference = _lstMods->getTextHeight(row - 1);
			SDL_WarpMouse(action->getLeftBlackBand() + action->getXMouse(),
				action->getTopBlackBand() + action->getYMouse() - static_cast<Uint16>(yDifference * action->getYScale()));
		}
		else
		{
			int yDifference = _lstMods->getRowY(row) - _lstMods->getY();
			SDL_WarpMouse(action->getLeftBlackBand() + action->getXMouse(),
				action->getTopBlackBand() + action->getYMouse() - static_cast<Uint16>(yDifference * action->getYScale()));
			_lstMods->scrollTo(targetScroll);
		}

		moveAbove(_mods.at(row), _mods.at(row - 1));
		lstModsRefresh(_lstMods->getScroll());
	}

	Options::reload = true;
}

static void moveBelow(const std::pair<std::string, bool> &source, const std::pair<std::string, bool> &destination)
{
	for (auto i = Options::mods.rbegin(); i != Options::mods.rend(); ++i)
	{
		if (destination.first == i->first)
		{
			Options::mods.insert(i.base(), source);
			break;
		}
	}

	for (auto i = Options::mods.begin(); i != Options::mods.end(); ++i)
	{
		if (source.first == i->first)
		{
			Options::mods.erase(i);
			break;
		}
	}
}

void OptionsModsState::lstModsRightArrowClick(Action *action)
{
	unsigned int row = _lstMods->getSelectedRow();
	if (_mods.empty() || row >= _mods.size() - 1)
	{
		return;
	}

	if (action->getDetails()->button.button == SDL_BUTTON_LEFT)
	{
		moveModDown(action, row);
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		moveModDown(action, row, true);
	}
}

void OptionsModsState::moveModDown(Action *action, unsigned int row, bool max)
{
	if (max)
	{
		moveBelow(_mods.at(row), _mods.back());
		lstModsRefresh(_lstMods->getScroll());
	}
	else
	{
		size_t currentScroll = _lstMods->getScroll();
		size_t targetScroll = 0;
		for (size_t i = 0; i <= row + 1; ++i)
		{
			if (i != row)
			{
				targetScroll += _lstMods->getNumTextLines(i);
			}
		}

		if (currentScroll + (int)_lstMods->getVisibleRows() > targetScroll)
		{
			int yDifference = _lstMods->getTextHeight(row + 1);
			SDL_WarpMouse(action->getLeftBlackBand() + action->getXMouse(),
				action->getTopBlackBand() + action->getYMouse() + static_cast<Uint16>(yDifference * action->getYScale()));
		}
		else
		{
			int yDifference = _lstMods->getY() + _lstMods->getHeight() - (_lstMods->getRowY(row) + _lstMods->getTextHeight(row));
			SDL_WarpMouse(action->getLeftBlackBand() + action->getXMouse(),
				action->getTopBlackBand() + action->getYMouse() + static_cast<Uint16>(yDifference * action->getYScale()));
			_lstMods->scrollTo(targetScroll - _lstMods->getVisibleRows() + 1);
		}

		moveBelow(_mods.at(row), _mods.at(row + 1));
		lstModsRefresh(_lstMods->getScroll());
	}

	Options::reload = true;
}

void OptionsModsState::lstModsMousePress(Action *action)
{
	if (Options::changeValueByMouseWheel == 0)
	{
		return;
	}

	unsigned int row = _lstMods->getSelectedRow();
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP && row > 0 && row < _mods.size())
	{
		if (action->getAbsoluteXMouse() >= _lstMods->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstMods->getArrowsRightEdge())
		{
			moveModUp(action, row);
		}
	}
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN &&
		!_mods.empty() && row < _mods.size() - 1)
	{
		if (action->getAbsoluteXMouse() >= _lstMods->getArrowsLeftEdge() &&
			action->getAbsoluteXMouse() <= _lstMods->getArrowsRightEdge())
		{
			moveModDown(action, row);
		}
	}
}

}
