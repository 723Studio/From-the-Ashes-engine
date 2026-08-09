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

#include <locale>
#include "Ufopaedia.h"
#include "UfopaediaSelectState.h"
#include "../Mod/ArticleDefinition.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/ComboBox.h"
#include "../Interface/TextList.h"

namespace OpenXcom
{
	UfopaediaSelectState::UfopaediaSelectState(const std::string &section, int heightOffset, int windowOffset) : _section(section), _lstScroll(0)
	{
		_isCommendationsSection = (_section == UFOPAEDIA_COMMENDATIONS);
		_screen = false;

		// set background window
		_window = new Window(this, 256, 180 + heightOffset, 32, 10 - windowOffset, POPUP_NONE);
		_window->setInnerColor(239); // almost black = darkest index from backpals.dat

		// set title
		_txtTitle = new Text(224, 17, 48, 26 - windowOffset);

		// set buttons
		_btnOk = new TextButton(108, 16, _isCommendationsSection ? 164 : 106, 166 - windowOffset + heightOffset);
		_cbxFilter = new ComboBox(this, 108, 16, 48, 166 - windowOffset + heightOffset, true);
		int listHeightOffset = (heightOffset / 8) * 8; // multiple of 8
		_lstSelection = new TextList(224, 104 + listHeightOffset, 40, 50 - windowOffset);

		// Set palette
		setInterface("ufopaedia");

		add(_window, "window", "ufopaedia");
		add(_txtTitle, "text", "ufopaedia");
		add(_btnOk, "button2", "ufopaedia");
		add(_lstSelection, "list", "ufopaedia");
		add(_cbxFilter, "button2", "ufopaedia");

		centerAllSurfaces();

		setWindowBackground(_window, "ufopaedia");

		_txtTitle->setBig();
		_txtTitle->setAlign(ALIGN_CENTER);
		_txtTitle->setText(tr("STR_SELECT_ITEM"));

		_btnOk->setText(tr("STR_OK"));
		_btnOk->onMouseClick((ActionHandler)&UfopaediaSelectState::btnOkClick);
		_btnOk->onKeyboardPress((ActionHandler)&UfopaediaSelectState::btnOkClick,Options::keyCancel);

		_lstSelection->setColumns(1, 206);
		_lstSelection->setSelectable(true);
		_lstSelection->setBackground(_window);
		_lstSelection->setMargin(18);
		_lstSelection->setAlign(ALIGN_CENTER);
		_lstSelection->onMouseClick((ActionHandler)&UfopaediaSelectState::lstSelectionClick, SDL_BUTTON_LEFT);

		if (_isCommendationsSection)
		{
			std::vector<std::string> filterOptions;
			filterOptions.push_back("STR_FILTER_DEFAULT");
			filterOptions.push_back("STR_NOT_AWARDED_YET");
			_cbxFilter->setOptions(filterOptions, true);
			_cbxFilter->onChange((ActionHandler)&UfopaediaSelectState::cbxFilterChange);
		}
		else
		{
			_cbxFilter->setVisible(false);
		}

	}

	UfopaediaSelectState::~UfopaediaSelectState()
	{}

	/**
	 * Initializes the state.
	 */
	void UfopaediaSelectState::init()
	{
		State::init();
		loadSelectionList();
	}

	/**
	 * Returns to the previous screen.
	 * @param action Pointer to an action.
	 */
	void UfopaediaSelectState::btnOkClick(Action *)
	{
		_game->popState();
	}

	/**
	 *
	 * @param action Pointer to an action.
	 */
	void UfopaediaSelectState::lstSelectionClick(Action *)
	{
		_lstScroll = _lstSelection->getScroll();
		Ufopaedia::openArticle(_game, _filtered_article_list[_lstSelection->getSelectedRow()]);
	}

	/**
	* Updates the list to match the filter.
	* @param action Pointer to an action.
	*/
	void UfopaediaSelectState::cbxFilterChange(Action *)
	{
		loadSelectionList();
	}

	void UfopaediaSelectState::loadSelectionList()
	{
		_lstSelection->clearList();
		_article_list.clear();
		Ufopaedia::list(_game->getSavedGame(), _game->getMod(), _section, _article_list);
		_filtered_article_list.clear();
		size_t selectedFilter = _isCommendationsSection ? _cbxFilter->getSelected() : 0;
		for (auto* articleDef : _article_list)
		{
			if (selectedFilter == 1)
			{
				if (Ufopaedia::isAwardedCommendation(_game->getSavedGame(), articleDef))
				{
					continue;
				}
			}

			_filtered_article_list.push_back(articleDef);
			_lstSelection->addRow(1, tr(articleDef->getMainTitle()).c_str());

		}
		if (_lstScroll > 0)
		{
			_lstSelection->scrollTo(_lstScroll);
			_lstScroll = 0;
		}
	}

}
