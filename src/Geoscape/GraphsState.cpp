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
#include "GraphsState.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/InteractiveSurface.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Engine/Palette.h"
#include "../Engine/Surface.h"
#include "../Engine/Unicode.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/ToggleTextButton.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleDiplomacyFaction.h"
#include "../Mod/RuleInterface.h"
#include "../Mod/RuleRegion.h"
#include "../Savegame/DiplomacyFaction.h"
#include "../Savegame/GameTime.h"
#include "../Savegame/Region.h"
#include "../Savegame/SavedGame.h"

namespace OpenXcom
{

namespace
{

const size_t GRAPH_MONTHS = 12;
const int GRAPH_GRID_HEIGHT = 126;
const int GRAPH_ZERO_Y = 175;
const Uint8 FACTION_COLORS[] = {24, 40, 80};
const Uint8 FINANCE_BUTTON_COLORS[] = {13, 21, 29, 37, 45, 104};
const int FINANCE_BUTTON_Y[] = {0, 11, 22, 33, 55, 44};

struct GraphScale
{
	int64_t lower;
	int64_t upper;
};

int64_t roundDownToStep(int64_t value, int64_t step)
{
	if (value >= 0)
		return 0;
	return -(((-value + step - 1) / step) * step);
}

GraphScale makeScale(int64_t minimum, int64_t maximum, int64_t initialStep)
{
	minimum = std::min<int64_t>(minimum, 0);
	maximum = std::max<int64_t>(maximum, 0);
	int64_t step = initialStep;
	while (true)
	{
		int64_t lower = roundDownToStep(minimum, step);
		int64_t upper = lower + step * 9;
		if (upper >= maximum)
			return {lower, upper};
		step *= 2;
	}
}

int graphY(int64_t value, const GraphScale& scale)
{
	double units = static_cast<double>(scale.upper - scale.lower) / GRAPH_GRID_HEIGHT;
	int y = GRAPH_ZERO_Y - static_cast<int>((value - scale.lower) / units);
	return std::max(45, std::min(180, y));
}

void drawDashedLine(Surface* surface, int x1, int y1, int x2, int y2, Uint8 color)
{
	int dx = std::abs(x2 - x1);
	int sx = x1 < x2 ? 1 : -1;
	int dy = -std::abs(y2 - y1);
	int sy = y1 < y2 ? 1 : -1;
	int error = dx + dy;
	int step = 0;
	while (true)
	{
		if (step % 4 < 2)
			surface->setPixel(x1, y1, color);
		if (x1 == x2 && y1 == y2)
			break;
		int error2 = error * 2;
		if (error2 >= dy)
		{
			error += dy;
			x1 += sx;
		}
		if (error2 <= dx)
		{
			error += dx;
			y1 += sy;
		}
		++step;
	}
}

Uint8 financeLineColor(size_t index)
{
	if (index == 5)
		return FINANCE_BUTTON_COLORS[index] + 4;
	int offset = index % 2 ? 8 : 0;
	return Palette::blockOffset(static_cast<int>(index / 2) + 1) + offset;
}

}

struct GraphButInfo
{
	LocalizedText _name;
	int _color;
	bool _pushed;
	size_t _dataIndex;
	GraphButInfo(const LocalizedText& name, Uint8 color, size_t dataIndex) :
		_name(name), _color(color), _pushed(false), _dataIndex(dataIndex) {}
};

/**
 * Initializes all the elements in the Graphs screen.
 */
GraphsState::GraphsState() : _page(GraphPage::UfoRegions), _butRegionsOffset(0)
{
	_bg = new InteractiveSurface(320, 200, 0, 0);
	_bg->onMousePress((ActionHandler)&GraphsState::shiftButtons, SDL_BUTTON_WHEELUP);
	_bg->onMousePress((ActionHandler)&GraphsState::shiftButtons, SDL_BUTTON_WHEELDOWN);
	_btnUfoRegion = new InteractiveSurface(32, 24, 96, 0);
	_btnXcomRegion = new InteractiveSurface(32, 24, 128, 0);
	_btnFactionStrength = new InteractiveSurface(32, 24, 160, 0);
	_btnFactionReputation = new InteractiveSurface(32, 24, 192, 0);
	_btnFactionFunds = new InteractiveSurface(32, 24, 224, 0);
	_btnFinance = new InteractiveSurface(32, 24, 256, 0);
	_btnGeoscape = new InteractiveSurface(32, 24, 288, 0);
	_txtTitle = new Text(230, 16, 90, 28);
	_txtFactor = new Text(38, 11, 96, 28);
	_txtMonths = new TextList(205, 8, 115, 183);
	_txtYears = new TextList(200, 8, 121, 191);

	setInterface("graphs");

	add(_bg);
	add(_btnUfoRegion);
	add(_btnXcomRegion);
	add(_btnFactionStrength);
	add(_btnFactionReputation);
	add(_btnFactionFunds);
	add(_btnFinance);
	add(_btnGeoscape);
	add(_txtMonths, "scale", "graphs");
	add(_txtYears, "scale", "graphs");
	add(_txtTitle, "text", "graphs");
	add(_txtFactor, "text", "graphs");
	for (int scaleText = 0; scaleText != 10; ++scaleText)
	{
		_txtScale.push_back(new Text(42, 16, 80, 171 - scaleText * 14));
		add(_txtScale.back(), "scale", "graphs");
	}

	SavedGame* save = _game->getSavedGame();
	Uint8 regionTotalColor = _game->getMod()->getInterface("graphs")->getElement("regionTotal")->color;
	size_t regionIndex = 0;
	for (auto* region : *save->getRegions())
	{
		Uint8 color = 13 + 8 * (regionIndex % GRAPH_MAX_BUTTONS);
		_regionToggles.push_back(new GraphButInfo(tr(region->getRules()->getType()), color, regionIndex));
		if (regionIndex < GRAPH_MAX_BUTTONS)
		{
			auto* button = new ToggleTextButton(88, 11, 0, regionIndex * 11);
			button->setText(tr(region->getRules()->getType()));
			button->setInvertColor(color);
			button->onMousePress((ActionHandler)&GraphsState::btnRegionListClick);
			_btnRegions.push_back(button);
			add(button, "button", "graphs");
		}
		_alienRegionLines.push_back(new Surface(320, 200, 0, 0));
		_xcomRegionLines.push_back(new Surface(320, 200, 0, 0));
		add(_alienRegionLines.back());
		add(_xcomRegionLines.back());
		++regionIndex;
	}

	size_t totalY = std::min(_regionToggles.size(), GRAPH_MAX_BUTTONS) * 11;
	_btnRegionTotal = new ToggleTextButton(88, 11, 0, totalY);
	_regionToggles.push_back(new GraphButInfo(tr("STR_TOTAL_UC"), regionTotalColor, regionIndex));
	_btnRegionTotal->onMousePress((ActionHandler)&GraphsState::btnRegionListClick);
	_btnRegionTotal->setInvertColor(regionTotalColor);
	_btnRegionTotal->setText(tr("STR_TOTAL_UC"));
	_alienRegionLines.push_back(new Surface(320, 200, 0, 0));
	_xcomRegionLines.push_back(new Surface(320, 200, 0, 0));
	add(_alienRegionLines.back());
	add(_xcomRegionLines.back());
	add(_btnRegionTotal, "button", "graphs");

	std::string graphFactionToggles = save->getGraphFactionToggles();
	while (graphFactionToggles.size() < save->getDiplomacyFactions().size())
		graphFactionToggles.push_back('0');
	for (size_t factionIndex = 0; factionIndex < save->getDiplomacyFactions().size(); ++factionIndex)
	{
		DiplomacyFaction* faction = save->getDiplomacyFactions()[factionIndex];
		if (!faction->isDiscovered())
			continue;
		Uint8 color = factionIndex < sizeof(FACTION_COLORS) / sizeof(FACTION_COLORS[0]) ?
			FACTION_COLORS[factionIndex] : 13 + 8 * (factionIndex % GRAPH_MAX_BUTTONS);
		auto* toggle = new GraphButInfo(tr(faction->getRules()->getName()), color, factionIndex);
		toggle->_pushed = graphFactionToggles[factionIndex] != '0';
		_factionToggles.push_back(toggle);
		_graphFactions.push_back(faction);

		auto* button = new ToggleTextButton(88, 11, 0, (_btnFactions.size()) * 11);
		button->setText(toggle->_name);
		button->setInvertColor(color);
		button->setPressed(toggle->_pushed);
		button->onMousePress((ActionHandler)&GraphsState::btnFactionListClick);
		_btnFactions.push_back(button);
		add(button, "button", "graphs");

		_factionLines.push_back(new Surface(320, 200, 0, 0));
		_factionSecondaryLines.push_back(new Surface(320, 200, 0, 0));
		add(_factionLines.back());
		add(_factionSecondaryLines.back());
	}

	for (size_t index = 0; index < sizeof(FINANCE_BUTTON_COLORS) / sizeof(FINANCE_BUTTON_COLORS[0]); ++index)
	{
		auto* button = new ToggleTextButton(88, 11, 0, FINANCE_BUTTON_Y[index]);
		button->setInvertColor(FINANCE_BUTTON_COLORS[index]);
		button->onMousePress((ActionHandler)&GraphsState::btnFinanceListClick);
		_btnFinances.push_back(button);
		_financeToggles.push_back(false);
		add(button, "button", "graphs");
		_financeLines.push_back(new Surface(320, 200, 0, 0));
		add(_financeLines.back());
	}

	_btnFinances[0]->setText(tr("STR_INCOME"));
	_btnFinances[1]->setText(tr("STR_EXPENDITURE"));
	_btnFinances[2]->setText(tr("STR_MAINTENANCE"));
	_btnFinances[3]->setText(tr("STR_BALANCE"));
	_btnFinances[4]->setText(tr("STR_SCORE"));
	_btnFinances[5]->setText(tr("STR_LOYALTY_UC"));
	const bool loyaltyUnlocked = save->isResearched("STR_LOYALTY");
	_btnFinances[5]->setVisible(loyaltyUnlocked);

	std::string graphRegionToggles = save->getGraphRegionToggles();
	std::string graphFinanceToggles = save->getGraphFinanceToggles();
	while (graphRegionToggles.size() < _regionToggles.size())
		graphRegionToggles.push_back('0');
	while (graphFinanceToggles.size() < _financeToggles.size())
		graphFinanceToggles.push_back('0');
	for (size_t index = 0; index < _regionToggles.size(); ++index)
	{
		_regionToggles[index]->_pushed = graphRegionToggles[index] != '0';
		if (index == _regionToggles.size() - 1)
			_btnRegionTotal->setPressed(_regionToggles[index]->_pushed);
		else if (index < GRAPH_MAX_BUTTONS)
			_btnRegions[index]->setPressed(_regionToggles[index]->_pushed);
	}
	for (size_t index = 0; index < _financeToggles.size(); ++index)
	{
		_financeToggles[index] = graphFinanceToggles[index] != '0' && (index != 5 || loyaltyUnlocked);
		_btnFinances[index]->setPressed(_financeToggles[index]);
	}

	Uint8 gridColor = _game->getMod()->getInterface("graphs")->getElement("graph")->color;
	_bg->drawRect(125, 49, 188, 127, gridColor);
	for (int grid = 0; grid != 5; ++grid)
	{
		for (int y = 50 + grid; y <= 163 + grid; y += 14)
		{
			for (int x = 126 + grid; x <= 297 + grid; x += 17)
			{
				Uint8 color = grid == 4 ? 0 : gridColor + grid + 1;
				_bg->drawRect(x, y, 16 - grid * 2, 13 - grid * 2, color);
			}
		}
	}

	std::string months[] = {"STR_JAN", "STR_FEB", "STR_MAR", "STR_APR", "STR_MAY", "STR_JUN", "STR_JUL", "STR_AUG", "STR_SEP", "STR_OCT", "STR_NOV", "STR_DEC"};
	int month = save->getTime()->getMonth();
	_txtMonths->setColumns(12, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17);
	_txtMonths->addRow(12, " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ", " ");
	_txtYears->setColumns(6, 34, 34, 34, 34, 34, 34);
	_txtYears->addRow(6, " ", " ", " ", " ", " ", " ");
	for (int index = 0; index != 12; ++index)
	{
		if (month > 11)
		{
			month = 0;
			std::ostringstream currentYear;
			currentYear << save->getTime()->getYear();
			_txtYears->setCellText(0, index / 2, currentYear.str());
			if (index > 2)
			{
				std::ostringstream previousYear;
				previousYear << save->getTime()->getYear() - 1;
				_txtYears->setCellText(0, 0, previousYear.str());
			}
		}
		_txtMonths->setCellText(0, index, tr(months[month]));
		++month;
	}

	for (auto* scaleText : _txtScale)
		scaleText->setAlign(ALIGN_RIGHT);
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtFactor->setText(tr("STR_FINANCE_THOUSANDS"));

	if (_game->getMod()->getSurface("GRAPH.BDY", false))
		_game->getMod()->getSurface("GRAPH.BDY")->blitNShade(_bg, 0, 0);
	else
		_game->getMod()->getSurface("GRAPHS.SPK")->blitNShade(_bg, 0, 0);

	_btnUfoRegion->onMousePress((ActionHandler)&GraphsState::btnUfoRegionClick);
	_btnXcomRegion->onMousePress((ActionHandler)&GraphsState::btnXcomRegionClick);
	_btnFactionStrength->onMousePress((ActionHandler)&GraphsState::btnFactionStrengthClick);
	_btnFactionReputation->onMousePress((ActionHandler)&GraphsState::btnFactionReputationClick);
	_btnFactionFunds->onMousePress((ActionHandler)&GraphsState::btnFactionFundsClick);
	_btnFinance->onMousePress((ActionHandler)&GraphsState::btnFinanceClick);
	_btnGeoscape->onMousePress((ActionHandler)&GraphsState::btnGeoscapeClick);
	_btnGeoscape->onKeyboardPress((ActionHandler)&GraphsState::btnGeoscapeClick, Options::keyCancel);

	btnUfoRegionClick(nullptr);
	centerAllSurfaces();
}

GraphsState::~GraphsState()
{
	std::string graphRegionToggles;
	std::string graphFinanceToggles;
	for (auto* toggle : _regionToggles)
	{
		graphRegionToggles.push_back(toggle->_pushed ? '1' : '0');
		delete toggle;
	}
	for (bool toggle : _financeToggles)
		graphFinanceToggles.push_back(toggle ? '1' : '0');

	SavedGame* save = _game->getSavedGame();
	std::string graphFactionToggles = save->getGraphFactionToggles();
	graphFactionToggles.resize(save->getDiplomacyFactions().size(), '0');
	for (auto* toggle : _factionToggles)
	{
		graphFactionToggles[toggle->_dataIndex] = toggle->_pushed ? '1' : '0';
		delete toggle;
	}
	save->setGraphRegionToggles(graphRegionToggles);
	save->setGraphFinanceToggles(graphFinanceToggles);
	save->setGraphFactionToggles(graphFactionToggles);
}

void GraphsState::btnGeoscapeClick(Action *)
{
	_game->popState();
}

void GraphsState::btnUfoRegionClick(Action *)
{
	_page = GraphPage::UfoRegions;
	resetScreen();
	for (auto* button : _btnRegions)
		button->setVisible(true);
	_btnRegionTotal->setVisible(true);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_UFO_ACTIVITY_IN_AREAS"));
	drawLines();
}

void GraphsState::btnXcomRegionClick(Action *)
{
	_page = GraphPage::XcomRegions;
	resetScreen();
	for (auto* button : _btnRegions)
		button->setVisible(true);
	_btnRegionTotal->setVisible(true);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_XCOM_ACTIVITY_IN_AREAS"));
	drawLines();
}

void GraphsState::btnFactionStrengthClick(Action *)
{
	_page = GraphPage::FactionStrength;
	resetScreen();
	for (auto* button : _btnFactions)
		button->setVisible(true);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_FACTION_POWER_AND_VIGILANCE"));
	drawLines();
}

void GraphsState::btnFactionReputationClick(Action *)
{
	_page = GraphPage::FactionReputation;
	resetScreen();
	for (auto* button : _btnFactions)
		button->setVisible(true);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_FACTION_REPUTATION"));
	drawLines();
}

void GraphsState::btnFactionFundsClick(Action *)
{
	_page = GraphPage::FactionFunds;
	resetScreen();
	for (auto* button : _btnFactions)
		button->setVisible(true);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_FACTION_FUNDS_GRAPH"));
	drawLines();
}

void GraphsState::btnFinanceClick(Action *)
{
	_page = GraphPage::Finance;
	resetScreen();
	for (auto* button : _btnFinances)
		button->setVisible(true);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_FINANCE"));
	drawLines();
}

void GraphsState::btnRegionListClick(Action *action)
{
	size_t number = 0;
	auto* button = dynamic_cast<ToggleTextButton*>(action->getSender());
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		for (auto* visibleButton : _btnRegions)
		{
			if (button != visibleButton)
				visibleButton->setPressed(button->getPressed());
		}
		_btnRegionTotal->setPressed(button->getPressed());
		for (auto* toggle : _regionToggles)
			toggle->_pushed = button->getPressed();
	}
	else
	{
		if (button == _btnRegionTotal)
			number = _regionToggles.size() - 1;
		else
		{
			for (size_t index = 0; index < _btnRegions.size(); ++index)
			{
				if (button == _btnRegions[index])
				{
					number = index + _butRegionsOffset;
					break;
				}
			}
		}
		_regionToggles.at(number)->_pushed = button->getPressed();
	}
	drawLines();
}

void GraphsState::btnFactionListClick(Action *action)
{
	auto* button = dynamic_cast<ToggleTextButton*>(action->getSender());
	if (action->getDetails()->button.button == SDL_BUTTON_RIGHT)
	{
		for (size_t index = 0; index < _btnFactions.size(); ++index)
		{
			_btnFactions[index]->setPressed(button->getPressed());
			_factionToggles[index]->_pushed = button->getPressed();
		}
	}
	else
	{
		for (size_t index = 0; index < _btnFactions.size(); ++index)
		{
			if (button == _btnFactions[index])
			{
				_factionToggles[index]->_pushed = button->getPressed();
				break;
			}
		}
	}
	drawLines();
}

void GraphsState::btnFinanceListClick(Action *action)
{
	auto* button = dynamic_cast<ToggleTextButton*>(action->getSender());
	for (size_t index = 0; index < _btnFinances.size(); ++index)
	{
		if (button == _btnFinances[index])
		{
			_financeToggles[index] = button->getPressed();
			break;
		}
	}
	drawLines();
}

void GraphsState::resetScreen()
{
	for (auto* surface : _alienRegionLines)
		surface->setVisible(false);
	for (auto* surface : _xcomRegionLines)
		surface->setVisible(false);
	for (auto* surface : _factionLines)
		surface->setVisible(false);
	for (auto* surface : _factionSecondaryLines)
		surface->setVisible(false);
	for (auto* surface : _financeLines)
		surface->setVisible(false);
	for (auto* button : _btnRegions)
		button->setVisible(false);
	for (auto* button : _btnFactions)
		button->setVisible(false);
	for (auto* button : _btnFinances)
		button->setVisible(false);
	_btnRegionTotal->setVisible(false);
	_txtFactor->setVisible(false);
}

void GraphsState::updateScale(double lowerLimit, double upperLimit)
{
	double increment = (upperLimit - lowerLimit) / 9;
	double text = lowerLimit;
	for (int index = 0; index < 10; ++index)
	{
		_txtScale[index]->setText(Unicode::formatNumber(static_cast<int>(text)));
		text += increment;
	}
}

void GraphsState::drawLines()
{
	switch (_page)
	{
	case GraphPage::UfoRegions:
	case GraphPage::XcomRegions:
		drawRegionLines();
		break;
	case GraphPage::FactionStrength:
	case GraphPage::FactionReputation:
	case GraphPage::FactionFunds:
		drawFactionLines();
		break;
	case GraphPage::Finance:
		drawFinanceLines();
		break;
	}
}

void GraphsState::drawRegionLines()
{
	bool alien = _page == GraphPage::UfoRegions;
	int64_t minimum = 0;
	int64_t maximum = 0;
	std::vector<int64_t> totals(GRAPH_MONTHS, 0);
	std::vector<bool> totalKnown(GRAPH_MONTHS, false);
	auto* regions = _game->getSavedGame()->getRegions();
	for (size_t regionIndex = 0; regionIndex < regions->size(); ++regionIndex)
	{
		std::vector<int>& values = alien ? regions->at(regionIndex)->getActivityAlien() : regions->at(regionIndex)->getActivityXcom();
		size_t count = std::min(GRAPH_MONTHS, values.size());
		for (size_t point = 0; point < count; ++point)
		{
			int value = values[values.size() - 1 - point];
			totals[point] += value;
			totalKnown[point] = true;
			if (_regionToggles[regionIndex]->_pushed)
			{
				minimum = std::min<int64_t>(minimum, value);
				maximum = std::max<int64_t>(maximum, value);
			}
		}
	}
	if (_regionToggles.back()->_pushed)
	{
		for (size_t point = 0; point < GRAPH_MONTHS; ++point)
		{
			if (totalKnown[point])
			{
				minimum = std::min(minimum, totals[point]);
				maximum = std::max(maximum, totals[point]);
			}
		}
	}
	GraphScale scale = makeScale(minimum, maximum, 10);

	for (size_t regionIndex = 0; regionIndex < regions->size(); ++regionIndex)
	{
		Surface* alienSurface = _alienRegionLines[regionIndex];
		Surface* xcomSurface = _xcomRegionLines[regionIndex];
		alienSurface->clear();
		xcomSurface->clear();
		Surface* surface = alien ? alienSurface : xcomSurface;
		std::vector<int>& values = alien ? regions->at(regionIndex)->getActivityAlien() : regions->at(regionIndex)->getActivityXcom();
		size_t count = std::min(GRAPH_MONTHS, values.size());
		bool previousValid = false;
		int previousX = 0;
		int previousY = 0;
		Uint8 color = _regionToggles[regionIndex]->_color + 4;
		for (size_t point = 0; point < count; ++point)
		{
			int x = 312 - point * 17;
			int y = graphY(values[values.size() - 1 - point], scale);
			surface->setPixel(x, y, color);
			if (previousValid)
				surface->drawLine(x, y, previousX, previousY, color);
			previousValid = true;
			previousX = x;
			previousY = y;
		}
		alienSurface->setVisible(alien && _regionToggles[regionIndex]->_pushed);
		xcomSurface->setVisible(!alien && _regionToggles[regionIndex]->_pushed);
	}

	Surface* alienTotalSurface = _alienRegionLines.back();
	Surface* xcomTotalSurface = _xcomRegionLines.back();
	alienTotalSurface->clear();
	xcomTotalSurface->clear();
	Surface* totalSurface = alien ? alienTotalSurface : xcomTotalSurface;
	Uint8 totalColor = _game->getMod()->getInterface("graphs")->getElement("regionTotal")->color2;
	bool previousValid = false;
	int previousX = 0;
	int previousY = 0;
	for (size_t point = 0; point < GRAPH_MONTHS; ++point)
	{
		if (!totalKnown[point])
		{
			previousValid = false;
			continue;
		}
		int x = 312 - point * 17;
		int y = graphY(totals[point], scale);
		totalSurface->setPixel(x, y, totalColor);
		if (previousValid)
			totalSurface->drawLine(x, y, previousX, previousY, totalColor);
		previousValid = true;
		previousX = x;
		previousY = y;
	}
	alienTotalSurface->setVisible(alien && _regionToggles.back()->_pushed);
	xcomTotalSurface->setVisible(!alien && _regionToggles.back()->_pushed);
	updateScale(scale.lower, scale.upper);
	_txtFactor->setVisible(false);
}

void GraphsState::drawFactionLines()
{
	bool strengthPage = _page == GraphPage::FactionStrength;
	auto valueForPage = [this](const FactionHistoryEntry& entry, bool secondary) -> int64_t
	{
		switch (_page)
		{
		case GraphPage::FactionStrength:
			return secondary ? entry.vigilance : entry.power;
		case GraphPage::FactionReputation:
			return entry.reputation;
		case GraphPage::FactionFunds:
			return entry.funds / 1000;
		default:
			return 0;
		}
	};

	int64_t minimum = 0;
	int64_t maximum = 0;
	for (size_t factionIndex = 0; factionIndex < _graphFactions.size(); ++factionIndex)
	{
		if (!_factionToggles[factionIndex]->_pushed)
			continue;
		auto includeEntry = [&](const FactionHistoryEntry& entry)
		{
			if (!entry.known)
				return;
			int64_t primary = valueForPage(entry, false);
			minimum = std::min(minimum, primary);
			maximum = std::max(maximum, primary);
			if (strengthPage)
			{
				int64_t secondary = valueForPage(entry, true);
				minimum = std::min(minimum, secondary);
				maximum = std::max(maximum, secondary);
			}
		};
		includeEntry(_graphFactions[factionIndex]->getCurrentHistoryEntry());
		const auto& history = _graphFactions[factionIndex]->getHistory();
		for (size_t point = 1; point < GRAPH_MONTHS && point <= history.size(); ++point)
			includeEntry(history[history.size() - point]);
	}
	GraphScale scale = makeScale(minimum, maximum, _page == GraphPage::FactionFunds ? 250 : 10);

	for (size_t factionIndex = 0; factionIndex < _graphFactions.size(); ++factionIndex)
	{
		Surface* primarySurface = _factionLines[factionIndex];
		Surface* secondarySurface = _factionSecondaryLines[factionIndex];
		primarySurface->clear();
		secondarySurface->clear();
		bool visible = _factionToggles[factionIndex]->_pushed;
		primarySurface->setVisible(visible);
		secondarySurface->setVisible(visible && strengthPage);
		if (!visible)
			continue;

		const auto& history = _graphFactions[factionIndex]->getHistory();
		bool previousValid = false;
		int previousX = 0;
		int previousPrimaryY = 0;
		int previousSecondaryY = 0;
		Uint8 color = _factionToggles[factionIndex]->_color + 4;
		for (size_t point = 0; point < GRAPH_MONTHS; ++point)
		{
			FactionHistoryEntry entry;
			if (point == 0)
				entry = _graphFactions[factionIndex]->getCurrentHistoryEntry();
			else if (point <= history.size())
				entry = history[history.size() - point];
			else
				break;
			if (!entry.known)
			{
				previousValid = false;
				continue;
			}

			int x = 312 - point * 17;
			int primaryY = graphY(valueForPage(entry, false), scale);
			primarySurface->setPixel(x, primaryY, color);
			if (previousValid)
				primarySurface->drawLine(x, primaryY, previousX, previousPrimaryY, color);
			int secondaryY = 0;
			if (strengthPage)
			{
				secondaryY = graphY(valueForPage(entry, true), scale);
				secondarySurface->setPixel(x, secondaryY, color);
				if (previousValid)
					drawDashedLine(secondarySurface, x, secondaryY, previousX, previousSecondaryY, color);
			}
			previousValid = true;
			previousX = x;
			previousPrimaryY = primaryY;
			previousSecondaryY = secondaryY;
		}
	}
	updateScale(scale.lower, scale.upper);
	_txtFactor->setVisible(_page == GraphPage::FactionFunds);
}

void GraphsState::drawFinanceLines()
{
	const size_t financeSeries = 6;
	std::vector<std::vector<int64_t> > values(financeSeries, std::vector<int64_t>(GRAPH_MONTHS, 0));
	std::vector<size_t> counts(financeSeries, 0);
	SavedGame* save = _game->getSavedGame();

	size_t fundsCount = std::min(GRAPH_MONTHS, save->getFundsList().size());
	counts[2] = counts[3] = counts[4] = fundsCount;
	if (fundsCount > 0)
		values[2][0] = save->getBaseMaintenance() / 1000;
	for (size_t point = 0; point < fundsCount; ++point)
	{
		size_t source = save->getFundsList().size() - 1 - point;
		if (source < save->getMaintenances().size())
			values[2][point] += save->getMaintenances()[source] / 1000;
		values[3][point] = save->getFundsList()[source] / 1000;
		if (source < save->getResearchScores().size())
			values[4][point] = save->getResearchScores()[source];
		for (auto* region : *save->getRegions())
		{
			if (source < region->getActivityXcom().size() && source < region->getActivityAlien().size())
				values[4][point] += region->getActivityXcom()[source] - region->getActivityAlien()[source];
		}
	}

	counts[0] = std::min(GRAPH_MONTHS, save->getIncomes().size());
	for (size_t point = 0; point < counts[0]; ++point)
		values[0][point] = save->getIncomes()[save->getIncomes().size() - 1 - point] / 1000;
	counts[1] = std::min(GRAPH_MONTHS, save->getExpenditures().size());
	for (size_t point = 0; point < counts[1]; ++point)
		values[1][point] = save->getExpenditures()[save->getExpenditures().size() - 1 - point] / 1000;

	if (save->isResearched("STR_LOYALTY"))
	{
		values[5][0] = save->getLoyalty();
		counts[5] = 1;
		const auto& loyaltyHistory = save->getLoyaltyHistory();
		for (size_t point = 1; point < GRAPH_MONTHS && point <= loyaltyHistory.size(); ++point)
		{
			values[5][point] = loyaltyHistory[loyaltyHistory.size() - point];
			++counts[5];
		}
	}

	int64_t minimum = 0;
	int64_t maximum = 0;
	bool moneySelected = false;
	for (size_t series = 0; series < financeSeries; ++series)
	{
		if (!_financeToggles[series])
			continue;
		if (series < 4)
			moneySelected = true;
		for (size_t point = 0; point < counts[series]; ++point)
		{
			minimum = std::min(minimum, values[series][point]);
			maximum = std::max(maximum, values[series][point]);
		}
	}
	GraphScale scale = makeScale(minimum, maximum, moneySelected ? 250 : 10);

	for (size_t series = 0; series < financeSeries; ++series)
	{
		Surface* surface = _financeLines[series];
		surface->clear();
		surface->setVisible(_financeToggles[series]);
		if (!_financeToggles[series])
			continue;
		bool previousValid = false;
		int previousX = 0;
		int previousY = 0;
		Uint8 color = financeLineColor(series);
		for (size_t point = 0; point < counts[series]; ++point)
		{
			int x = 312 - point * 17;
			int y = graphY(values[series][point], scale);
			surface->setPixel(x, y, color);
			if (previousValid)
				surface->drawLine(x, y, previousX, previousY, color);
			previousValid = true;
			previousX = x;
			previousY = y;
		}
	}
	updateScale(scale.lower, scale.upper);
	_txtFactor->setVisible(true);
}

void GraphsState::shiftButtons(Action *action)
{
	if ((_page != GraphPage::UfoRegions && _page != GraphPage::XcomRegions) || _regionToggles.size() <= GRAPH_MAX_BUTTONS)
		return;
	if (action->getDetails()->button.button == SDL_BUTTON_WHEELUP)
		scrollButtons(_regionToggles, _btnRegions, _butRegionsOffset, -1);
	else if (action->getDetails()->button.button == SDL_BUTTON_WHEELDOWN)
		scrollButtons(_regionToggles, _btnRegions, _butRegionsOffset, 1);
}

void GraphsState::scrollButtons(std::vector<GraphButInfo *>& toggles, std::vector<ToggleTextButton *>& buttons, size_t& offset, int step)
{
	int nextOffset = static_cast<int>(offset) + step;
	if (nextOffset < 0 || static_cast<size_t>(nextOffset) + GRAPH_MAX_BUTTONS >= toggles.size())
		return;
	offset = static_cast<size_t>(nextOffset);
	size_t index = 0;
	auto button = buttons.begin();
	for (auto toggle = toggles.begin() + offset; toggle != toggles.end() && index < GRAPH_MAX_BUTTONS; ++toggle, ++button, ++index)
		updateButton(*toggle, *button);
}

void GraphsState::updateButton(GraphButInfo *from, ToggleTextButton *to)
{
	to->setText(from->_name);
	to->setInvertColor(from->_color);
	to->setPressed(from->_pushed);
}

}
