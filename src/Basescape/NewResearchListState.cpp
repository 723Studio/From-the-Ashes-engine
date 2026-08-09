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
#include <algorithm>
#include <sstream>
#include <vector>
#include "NewResearchListState.h"
#include "../Engine/Game.h"
#include "../Mod/Mod.h"
#include "../Engine/LocalizedText.h"
#include "../Engine/Options.h"
#include "../Interface/ComboBox.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextList.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Base.h"
#include "../Mod/RuleResearch.h"
#include "ResearchInfoStateFtA.h"

namespace OpenXcom
{
/**
 * Initializes all the elements in the Research list screen.
 * @param game Pointer to the core game.
 * @param base Pointer to the base to get info from.
 * @param sortByCost Should the list be sorted by cost or listOrder?
 */
NewResearchListState::NewResearchListState(Base *base, bool sortByCost) : _base(base), _sortByCost(sortByCost), _lstScroll(0)
{
	if (Options::isPasswordCorrect())
	{
		_sortByCost = !_sortByCost;
	}

	_screen = false;
	_isSortingEnabled = _game->getMod()->getEnableNewResearchSorting();

	_window = new Window(this, 320, _isSortingEnabled ? 166 : 146, 0, _isSortingEnabled ? 17 : 27, POPUP_BOTH);
	_btnOK = new TextButton(216, 16, 52, _isSortingEnabled ? 169 : 149);
	_cbxSort = new ComboBox(this, 108, 16, 22, 149, true);
	_txtTitle = new Text(300, 16, 10, 35);
	_txtName = new Text(138, 9, 10, 51);
	_txtCategory = new Text(89, 9, 182, 51);
	_lstResearch = new TextList(288, 80, 10, 62);

	touchComponentsCreate(_txtTitle, true, -45, +30);

	// Set palette
	setInterface("selectNewResearch");

	add(_window, "window", "selectNewResearch");
	add(_btnOK, "button", "selectNewResearch");
	add(_txtTitle, "text", "selectNewResearch");
	add(_txtName, "text", "selectNewResearch");
	add(_txtCategory, "text", "selectNewResearch");
	add(_lstResearch, "list", "selectNewResearch");
	add(_cbxSort, "button", "selectNewResearch");

	touchComponentsAdd("button2", "selectNewResearch", _window);

	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, "selectNewResearch");

	touchComponentsConfigure();

	_btnOK->setText(tr("STR_OK"));
	_btnOK->onMouseClick((ActionHandler)&NewResearchListState::btnOKClick);
	_btnOK->onKeyboardPress((ActionHandler)&NewResearchListState::btnOKClick, Options::keyCancel);

	if (_isSortingEnabled)
	{
		std::vector<std::string> sortOptions;
		sortOptions.push_back("STR_SORT_DEFAULT");
		sortOptions.push_back("STR_SORT_BY_COST");
		sortOptions.push_back("STR_SORT_BY_NAME");
		_cbxSort->setOptions(sortOptions, true);
		if (_sortByCost)
		{
			_cbxSort->setSelected(1);
		}
		_cbxSort->onChange((ActionHandler)&NewResearchListState::cbxSortChange);
	}
	else
	{
		_cbxSort->setVisible(false);
	}

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr("STR_NEW_RESEARCH_PROJECTS"));
	_txtTitle->setBig();

	_txtName->setText(tr("STR_PROJECT_NAME"));

	_txtCategory->setText(tr("STR_PROJECT_CATEGORY"));

	_lstResearch->setColumns(2, 172, 116);
	_lstResearch->setWordWrap(true);
	_lstResearch->setMargin(0);
	_lstResearch->setSelectable(true);
	_lstResearch->setBackground(_window);

	_lstResearch->onMouseClick((ActionHandler)&NewResearchListState::onSelectProject, SDL_BUTTON_LEFT);

}

/**
 * Initializes the screen (fills the list).
 */
void NewResearchListState::init()
{
	State::init();
	fillProjectList();

	touchComponentsRefresh();
}

/**
 * Selects the RuleResearch to work on.
 * @param action Pointer to an action.
 */
void NewResearchListState::onSelectProject(Action *)
{
	_lstScroll = _lstResearch->getScroll();
	_game->pushState(new ResearchInfoStateFtA(_base, _projects[_lstResearch->getSelectedRow()]));
}

/**
 * Returns to the previous screen.
 * @param action Pointer to an action.
 */
void NewResearchListState::btnOKClick(Action *)
{
	_game->popState();
}

/**
 * Updates the research list based on the selected option.
 */
void NewResearchListState::cbxSortChange(Action *)
{
	fillProjectList();
}

/**
 * Fills the list with possible ResearchProjects.
 */
void NewResearchListState::fillProjectList()
{
	_projects.clear();
	_lstResearch->clearList();
	// Note: this is the *only* place where this method is called with considerDebugMode = true
	_game->getSavedGame()->getAvailableResearchProjects(_projects, _game->getMod() , _base, true);
	size_t selectedSort = _cbxSort->getSelected();
	if (selectedSort == 1)
	{
		std::sort(_projects.begin(), _projects.end(), [&](RuleResearch* a, RuleResearch* b) { return a->getCost() < b->getCost(); });
	}
	else if (selectedSort == 2)
	{
		std::sort(_projects.begin(), _projects.end(), [&](RuleResearch* a, RuleResearch* b) { return Unicode::naturalCompare(tr(a->getName()), tr(b->getName())); });
	}
	else
	{
		// sort by list order
		std::sort(_projects.begin(), _projects.end(), [&](RuleResearch* a, RuleResearch* b) { return a->getListOrder() < b->getListOrder(); });
	}
	auto researchRuleIt = _projects.begin();
	RuleResearch* rule = nullptr;
	while (researchRuleIt != _projects.end())
	{
		rule = (*researchRuleIt);

		// EXPLANATION
		// -----------
		// Projects with "requires" can only be discovered/researched indirectly
		//  - this is because we can't reliably determine if they are unlocked or not
		// Example:
		//  - Alien Origins + Alien Leader => ALIEN_LEADER_PLUS is discovered
		//  - Alien Leader + Alien Origins => ALIEN_LEADER_PLUS is NOT discovered (you need to research another alien leader/commander)
		// If we wanted to allow also direct research of projects with "requires",
		// we would need to implement a slightly more complicated unlocking algorithm
		// and more importantly, we would need to remember the list of unlocked topics
		// in the save file (currently this is not done, the list is calculated on-the-fly).
		// Summary:
		//  - it would be possible to remove this condition, but more refactoring would be needed
		//  - for now, handling "requires" via zero-cost helpers (e.g. STR_LEADER_PLUS)... is enough
		if (rule->getRequirements().empty())
		{
			_lstResearch->addRow(2, tr(rule->getName()).c_str(), getProjectCategory(rule).c_str());
			++researchRuleIt;
		}
		else
		{
			researchRuleIt = _projects.erase(researchRuleIt);
		}
	}

	if (_lstScroll > 0)
	{
		_lstResearch->scrollTo(_lstScroll);
		_lstScroll = 0;
	}
}

std::string NewResearchListState::getProjectCategory(RuleResearch *project)
{
	std::ostringstream ss;
	auto stats = project->getStats();
	std::vector<std::pair<int, std::string> > statList;

	if (stats.physics > 0)
		statList.push_back(std::make_pair(stats.physics, tr(UnitStats::getStatString(&UnitStats::physics, UnitStats::STATSTR_LC))));
	if (stats.chemistry > 0)
		statList.push_back(std::make_pair(stats.chemistry, tr(UnitStats::getStatString(&UnitStats::chemistry, UnitStats::STATSTR_LC))));
	if (stats.biology > 0)
		statList.push_back(std::make_pair(stats.biology, tr(UnitStats::getStatString(&UnitStats::biology, UnitStats::STATSTR_LC))));
	if (stats.data > 0)
		statList.push_back(std::make_pair(stats.data, tr(UnitStats::getStatString(&UnitStats::data, UnitStats::STATSTR_LC))));
	if (stats.computers > 0)
		statList.push_back(std::make_pair(stats.computers, tr(UnitStats::getStatString(&UnitStats::computers, UnitStats::STATSTR_LC))));
	if (stats.tactics > 0)
		statList.push_back(std::make_pair(stats.tactics, tr(UnitStats::getStatString(&UnitStats::tactics, UnitStats::STATSTR_LC))));
	if (stats.materials > 0)
		statList.push_back(std::make_pair(stats.materials, tr(UnitStats::getStatString(&UnitStats::materials, UnitStats::STATSTR_LC))));
	if (stats.designing > 0)
		statList.push_back(std::make_pair(stats.designing, tr(UnitStats::getStatString(&UnitStats::designing, UnitStats::STATSTR_LC))));
	if (stats.alienTech > 0)
		statList.push_back(std::make_pair(stats.alienTech, tr(UnitStats::getStatString(&UnitStats::alienTech, UnitStats::STATSTR_LC))));
	if (stats.psionics > 0)
		statList.push_back(std::make_pair(stats.psionics, tr(UnitStats::getStatString(&UnitStats::psionics, UnitStats::STATSTR_LC))));
	if (stats.xenolinguistics > 0)
		statList.push_back(std::make_pair(stats.xenolinguistics, tr(UnitStats::getStatString(&UnitStats::xenolinguistics, UnitStats::STATSTR_LC))));

	std::stable_sort(statList.begin(), statList.end(), [](const auto& a, const auto& b)
	{
		return a.first < b.first;
	});

	for (std::vector<std::pair<int, std::string> >::const_iterator it = statList.begin(); it != statList.end(); ++it)
	{
		if (it != statList.begin())
		{
			ss << ", ";
		}
		ss << it->second;
	}

	return ss.str();
}

}
