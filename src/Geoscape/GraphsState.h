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
#include <cstddef>
#include <vector>
#include "../Engine/State.h"

namespace OpenXcom
{

class Surface;
class InteractiveSurface;
class Text;
class TextButton;
class ToggleTextButton;
class TextList;
class Region;
class DiplomacyFaction;
struct GraphButInfo;

enum class GraphPage
{
	UfoRegions,
	XcomRegions,
	FactionStrength,
	FactionReputation,
	FactionFunds,
	Finance
};

/**
 * Graphs screen for displaying graphs of various
 * monthly game data like activity and finances.
 */
class GraphsState : public State
{
private:
	InteractiveSurface *_bg;
	InteractiveSurface *_btnGeoscape;
	InteractiveSurface *_btnXcomRegion, *_btnUfoRegion;
	InteractiveSurface *_btnFactionStrength, *_btnFactionReputation, *_btnFactionFunds;
	InteractiveSurface *_btnFinance;
	Text *_txtTitle, *_txtFactor;
	TextList *_txtMonths, *_txtYears;
	std::vector<Text *> _txtScale;
	std::vector<ToggleTextButton *> _btnRegions, _btnFactions, _btnFinances;
	std::vector<GraphButInfo *> _regionToggles, _factionToggles;
	std::vector<DiplomacyFaction *> _graphFactions;
	std::vector<bool> _financeToggles;
	ToggleTextButton *_btnRegionTotal;
	std::vector<Surface *> _alienRegionLines;
	std::vector<Surface *> _xcomRegionLines;
	std::vector<Surface *> _factionLines, _factionSecondaryLines;
	std::vector<Surface *> _financeLines;
	GraphPage _page;
	inline static constexpr size_t GRAPH_MAX_BUTTONS = 16;
	//will be only between 0 and size()
	size_t _butRegionsOffset;
	//scroll and repaint buttons functions
	void scrollButtons(std::vector<GraphButInfo *> &toggles, std::vector<ToggleTextButton *> &buttons, size_t &offset, int step);
	void updateButton(GraphButInfo *from,ToggleTextButton *to);
public:
	/// Creates the Graphs state.
	GraphsState();
	/// Cleans up the Graphs state.
	~GraphsState();
	/// Handler for clicking the Geoscape icon.
	void btnGeoscapeClick(Action *action);
	/// Handler for clicking the ufo region icon.
	void btnUfoRegionClick(Action *action);
	/// Handler for clicking the xcom region icon.
	void btnXcomRegionClick(Action *action);
	/// Handler for clicking the faction power and vigilance icon.
	void btnFactionStrengthClick(Action *action);
	/// Handler for clicking the faction reputation icon.
	void btnFactionReputationClick(Action *action);
	/// Handler for clicking the faction funds icon.
	void btnFactionFundsClick(Action *action);
	/// Handler for clicking the finance icon.
	void btnFinanceClick(Action *action);
	/// Handler for clicking on a region button.
	void btnRegionListClick(Action *action);
	/// Handler for clicking on a faction button.
	void btnFactionListClick(Action *action);
	/// Handler for clicking  on a finances button.
	void btnFinanceListClick(Action *action);
	/// Mouse wheel handler for shifting up/down the buttons
	void shiftButtons(Action *action);
	/// Reset all the elements on screen.
	void resetScreen();
	/// Update the scale
	void updateScale(double lowerLimit, double upperLimit);
	/// Decide which lines to draw
	void drawLines();
	/// Draw Region Lines.
	void drawRegionLines();
	/// Draw Faction Lines.
	void drawFactionLines();
	/// Draw Finances Lines.
	void drawFinanceLines();
	/// Scroll button lists
};

}
