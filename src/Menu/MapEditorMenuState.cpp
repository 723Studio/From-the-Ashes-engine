/*
 * Copyright 2010-2019 OpenXcom Developers.
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
#include "MapEditorMenuState.h"
#include "MapEditorSetSizeState.h"
#include <algorithm>
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Engine/Screen.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleTerrain.h"
#include "../Mod/MapBlock.h"
#include "../Mod/MapDataSet.h"
#include "../Mod/RuleCraft.h"
#include "../Mod/RuleUfo.h"
#include "../Engine/Action.h"
#include "../Engine/LocalizedText.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Frame.h"
#include "../Battlescape/MapEditor.h"
#include "../Battlescape/MapEditorState.h"
#include "../Battlescape/BattlescapeGenerator.h"
#include "../Savegame/MapEditorSave.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/SavedBattleGame.h"

namespace OpenXcom
{

/**
 * Initializes all the elements in the Map Editor Menu window.
 * @param game Pointer to the core game.
 */
MapEditorMenuState::MapEditorMenuState() : _selectedMap(-1), _pickTerrainMode(false), _newMapName(""), _newMapX(10), _newMapY(10), _newMapZ(10)
{
	if (_game->getSavedGame() && _game->getSavedGame()->getSavedBattle() && Options::maximizeInfoScreens)
	{
		Options::baseXResolution = Screen::ORIGINAL_WIDTH;
		Options::baseYResolution = Screen::ORIGINAL_HEIGHT;
		_game->getScreen()->resetDisplay(false);
        _window = new Window(this, 320, 200, 0, 0, POPUP_NONE);
	}
    else
    {
	    _window = new Window(this, 320, 200, 0, 0, POPUP_BOTH);
    }

	// Create objects
    // TODO: interfaces ruleset for colors
	_txtTitle = new Text(320, 17, 0, 9);

	_btnOk = new TextButton(100, 16, 8, 176);
	_btnCancel = new TextButton(100, 16, 110, 176);
    _btnNew = new TextButton(100, 16, 212, 176);

    _filterTerrain = new TextButton(48, 16, 8, 28);
    _filterCraft = new TextButton(48, 16, 58, 28);
    _filterUFOs = new TextButton(48, 16, 108, 28);
    _currentFilter = _filterTerrain;

    _txtPickTerrainMode = new Text(148, 16, 164, 28);

    _filterTerrain->setGroup(&_currentFilter);
    _filterCraft->setGroup(&_currentFilter);
    _filterUFOs->setGroup(&_currentFilter);

    _txtSearch = new Text(48, 10, 8, 46);

    _lstMaps = new TextList(119, 104, 14, 64);

    _txtSelectedEntry = new Text(135, 116, 170, 52);

    _frameLeft = new Frame(148, 116, 8, 58);
    _frameRight = new Frame(148, 128, 164, 46);

	// Set palette
	setInterface("mainMenu");

	add(_window, "window", "mainMenu");
	add(_txtTitle, "text", "mainMenu");
	add(_btnOk, "button", "mainMenu");
	add(_btnCancel, "button", "mainMenu");
    add(_btnNew, "button", "mainMenu");
    add(_filterTerrain, "button", "mainMenu");
    add(_filterCraft, "button", "mainMenu");
    add(_filterUFOs, "button", "mainMenu");
    add(_txtPickTerrainMode, "text", "mainMenu");
    add(_txtSearch, "text", "mainMenu");
    add(_lstMaps, "list", "saveMenus");
    add(_txtSelectedEntry, "text", "mainMenu");
    add(_frameLeft, "frames", "newBattleMenu");
    add(_frameRight, "frames", "newBattleMenu");

	centerAllSurfaces();

	// Set up objects
	setWindowBackground(_window, "mainMenu");

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_MAP_EDITOR"));

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&MapEditorMenuState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&MapEditorMenuState::btnOkClick, Options::keyOk);
    _btnOk->setVisible(false);

	_btnCancel->setText(tr("STR_CANCEL"));
	_btnCancel->onMouseClick((ActionHandler)&MapEditorMenuState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&MapEditorMenuState::btnCancelClick, Options::keyCancel);

    _btnNew->setText(tr("STR_NEW_MAP"));
    _btnNew->onMouseClick((ActionHandler)&MapEditorMenuState::btnNewMapClick);

    _filterTerrain->setText(tr("STR_TERRAIN"));
    _filterTerrain->onMouseClick((ActionHandler)&MapEditorMenuState::btnMapFilterClick);

    _filterCraft->setText(tr("STR_CRAFT"));
    _filterCraft->onMouseClick((ActionHandler)&MapEditorMenuState::btnMapFilterClick);

    _filterUFOs->setText(tr("STR_UFOS"));
    _filterUFOs->onMouseClick((ActionHandler)&MapEditorMenuState::btnMapFilterClick);

    _txtPickTerrainMode->setText(tr("STR_PICKING_TERRAIN").arg(tr("STR_NEW_MAP")));
    _txtPickTerrainMode->setVisible(false);

	_txtSearch->setText(tr("STR_MAP_EDITOR_MENU_SEARCH"));


    _lstMaps->setColumns(1, 120);
    _lstMaps->setAlign(ALIGN_LEFT);
    _lstMaps->setSelectable(true);
	_lstMaps->setBackground(_window);
    _lstMaps->onMouseClick((ActionHandler)&MapEditorMenuState::lstMapsClick);

    _txtSelectedEntry->setWordWrap(true);

    _editor = _game->getMapEditor();
    if (!_editor)
    {
        // we haven't passed it a save yet, so this is **ONLY** to get access to the MapEditorSave data
        // we'll make the save and pass it to the editor when starting it
        _editor = new MapEditor(0);
        _game->setMapEditor(_editor);
    }
    _editor->getMapEditorSave()->clearMapFileToLoad();

    _buttonTerrainFilters[_filterTerrain] = FILTER_NORMAL;
    _buttonTerrainFilters[_filterCraft] = FILTER_CRAFT;
    _buttonTerrainFilters[_filterUFOs] = FILTER_UFO;
}

MapEditorMenuState::~MapEditorMenuState()
{
	if (_game->getSavedGame() && _game->getSavedGame()->getSavedBattle() && Options::maximizeInfoScreens)
	{
        Screen::updateScale(Options::battlescapeScale, Options::baseXBattlescape, Options::baseYBattlescape, true);
        _game->getScreen()->resetDisplay(false);
	}
}

void MapEditorMenuState::init()
{
	State::init();

    populateMapsList();
}

/**
 * Fills the list with available map names
 */
void MapEditorMenuState::populateMapsList()
{
    _mapsList.clear();
    _lstMaps->clearList();

    _selectedMap = -1;
    _txtSelectedEntry->setText("");
    _btnOk->setVisible(false);

    // If we're creating a new map, the list should have terrains instead of map names
    if (_pickTerrainMode)
    {
        populateTerrainsList();
        return;
    }

    std::vector<std::string> availableTerrains;
    switch (_buttonTerrainFilters[_currentFilter])
    {
    case FILTER_CRAFT:
        availableTerrains = _game->getMod()->getCraftsList();
        break;
    
    case FILTER_UFO:
        availableTerrains = _game->getMod()->getUfosList();
        break;

    default: // FILTER_NORMAL
        availableTerrains = _game->getMod()->getTerrainList();
        break;
    }

    for (auto &i : availableTerrains)
    {
        RuleTerrain *terrain;
        MapEditorEntry entry;
        
        switch (_buttonTerrainFilters[_currentFilter])
        {
        case FILTER_CRAFT:
            terrain = _game->getMod()->getCraft(i)->getBattlescapeTerrainData();
            entry.craftOrUFOName = i;
            break;
        
        case FILTER_UFO:
            terrain = _game->getMod()->getUfo(i)->getBattlescapeTerrainData();
            entry.craftOrUFOName = i;
            break;

        default: // FILTER_NORMAL
            terrain = _game->getMod()->getTerrain(i);
            break;
        }

        if (!terrain)
        {
            continue;
        }

        entry.desc = terrain->getName();
        entry.filter = _buttonTerrainFilters[_currentFilter];

        for (auto j : *terrain->getMapBlocks())
        {
            entry.name = j->getName();
            _lstMaps->addRow(1, entry.name.c_str());
            _mapsList.push_back(entry);
        }

    }

    if (_mapsList.size() == 1)
    {
        lstMapsClick(0);
    }
}

/**
 * Fills the list with available terrain names
 */
void MapEditorMenuState::populateTerrainsList()
{
    std::vector<MapEditorEntry> availableTerrains;

    switch (_buttonTerrainFilters[_currentFilter])
    {
    case FILTER_CRAFT:
        for (auto &i : _game->getMod()->getCraftsList())
        {
            if (_game->getMod()->getCraft(i)->getBattlescapeTerrainData())
            {
                MapEditorEntry entry;
                entry.name = i;
                entry.craftOrUFOName = i;
                entry.desc = _game->getMod()->getCraft(i)->getBattlescapeTerrainData()->getName();
                entry.filter = FILTER_CRAFT;
                availableTerrains.push_back(entry);
            }
        }
        break;
    
    case FILTER_UFO:
        for (auto &i : _game->getMod()->getUfosList())
        {
            if (_game->getMod()->getUfo(i)->getBattlescapeTerrainData())
            {
                MapEditorEntry entry;
                entry.name = i;
                entry.craftOrUFOName = i;
                entry.desc = _game->getMod()->getUfo(i)->getBattlescapeTerrainData()->getName();
                entry.filter = FILTER_UFO;
                availableTerrains.push_back(entry);
            }
        }
        break;

    default: // FILTER_NORMAL
        for (auto &i : _game->getMod()->getTerrainList())
        {
            MapEditorEntry entry;
            entry.name = i;
            entry.desc = i;
            availableTerrains.push_back(entry);
        }
        break;
    }

    for (auto i : availableTerrains)
    {
        // Multiple craft and ufos can use the same terrain, so only add them once to the list.
        auto jt = std::find_if(_mapsList.begin(), _mapsList.end(), [&](const MapEditorEntry& entry) { return entry.desc == i.desc; });
        if (jt == _mapsList.end())
        {
            _lstMaps->addRow(1, i.desc.c_str());
            _mapsList.push_back(i);
        }
    }

    if (_mapsList.size() == 1)
    {
        lstMapsClick(0);
    }
}

/**
 * Handles clicking on the filter buttons for the type of map to display
 * @param action Pointer to an action.
 */
void MapEditorMenuState::btnMapFilterClick(Action *action)
{
	SDL_Event ev;
	ev.type = SDL_MOUSEBUTTONDOWN;
	ev.button.button = SDL_BUTTON_LEFT;
	Action a = Action(&ev, 0.0, 0.0, 0, 0);
	action->getSender()->mousePress(&a, this);

    if (action->getSender() == _filterTerrain)
    {
        _currentFilter = _filterTerrain;
    }
    else if (action->getSender() == _filterCraft)
    {
        _currentFilter = _filterCraft;
    }
    else if (action->getSender() == _filterUFOs)
    {
        _currentFilter = _filterUFOs;
    }

    populateMapsList();

    // consume the action to keep the selected filter button pressed
    action->getDetails()->type = SDL_NOEVENT;
}

/**
 * Handles clicking on the list of maps.
 * If there's only one map matched, set it as selected
 * @param action Pointer to an action.
 */
void MapEditorMenuState::lstMapsClick(Action *)
{
    _selectedMap = _mapsList.size() == 1 ? 0 : _lstMaps->getSelectedRow();
    if (_selectedMap > -1 && _selectedMap < (int)_mapsList.size())
    {
        MapEditorEntry entry = _mapsList.at(_selectedMap);

        std::string name;
        if (!_pickTerrainMode)
        {
            name = entry.name;
            _editor->setMapFileToLoadName(name);
        }
        else
        {
            name = tr("STR_NEW_MAP");
        }

        std::string terrainName = entry.desc;
        RuleTerrain *terrain = _game->getMod()->getTerrain(terrainName);

        switch (entry.filter)
        {
        case FILTER_CRAFT:
            _editor->setMapFileToLoadTerrain(entry.craftOrUFOName);
            terrainName = terrainName + "\n" + (std::string)tr("STR_CRAFT") + ": " + entry.craftOrUFOName;
            terrain = _game->getMod()->getCraft(entry.craftOrUFOName)->getBattlescapeTerrainData();
            break;

        case FILTER_UFO:
            _editor->setMapFileToLoadTerrain(entry.craftOrUFOName);
            terrainName = terrainName + "\n" + (std::string)tr("STR_UFO") + ": " + entry.craftOrUFOName;
            terrain = _game->getMod()->getUfo(entry.craftOrUFOName)->getBattlescapeTerrainData();
            break;

        default: // FILTER_NORMAL
            _editor->setMapFileToLoadTerrain(entry.desc);
            break;
        }

        std::string mcds = "";
        if (terrain)
        {
            for (auto mcd : *terrain->getMapDataSets())
            {
                mcds = mcds + "\n  - " + mcd->getName();
            }
        }

        _txtSelectedEntry->setText(tr("STR_SELECTED_MAP_EDITOR_ENTRY").arg(name).arg(terrainName).arg(mcds));
        _btnOk->setVisible(true);
    }
    else
    {
        _selectedMap = -1;
        _txtSelectedEntry->setText("");
        _btnOk->setVisible(false);
    }
}

/**
 * Handles clicking the OK button
 * @param action Pointer to an action.
 */
void MapEditorMenuState::btnOkClick(Action *)
{
    if (_pickTerrainMode)
    {
        MapEditorSetSizeState *mapEditorSetSizeState = new MapEditorSetSizeState(this);
        _game->pushState(mapEditorSetSizeState);
    }
    else
    {
        startEditor();
    }
}

/**
 * Starts a new instance of the Map Editor.
 */
void MapEditorMenuState::startEditor()
{
    SavedGame *save = new SavedGame();
    _game->setSavedGame(save);

    SavedBattleGame *savedBattleGame = new SavedBattleGame(_game->getMod(), _game->getLanguage());
    save->setBattleGame(savedBattleGame);

    _editor->setSave(savedBattleGame);
    _editor->init();

	BattlescapeGenerator battlescapeGenerator = BattlescapeGenerator(_game);

    // get the name of and a pointer to the terrain we're loading
    std::string terrainName = _editor->getMapFileToLoadTerrain();
    RuleTerrain *terrain = 0;

    switch (_buttonTerrainFilters[_currentFilter])
    {
    case FILTER_CRAFT:
        terrain = _game->getMod()->getCraft(terrainName)->getBattlescapeTerrainData();
        break;
    
    case FILTER_UFO:
        terrain = _game->getMod()->getUfo(terrainName)->getBattlescapeTerrainData();
        break;

    default: // FILTER_NORMAL
        terrain = _game->getMod()->getTerrain(terrainName);
        break;
    }

	battlescapeGenerator.setTerrain(terrain);
	battlescapeGenerator.setWorldShade(0);

    std::string mapName = _editor->getMapFileToLoadName();
    if (mapName.empty() && _pickTerrainMode)
    {
        _game->getSavedGame()->getSavedBattle()->initMap(_newMapX, _newMapY, _newMapZ, true);
        _editor->setMapFileToLoadName(_newMapName);
        battlescapeGenerator.loadEmptyMap();
    }
    else
    {
        if (mapName.empty())
        {
            _editor->setMapFileToLoadName(_mapsList.at(_selectedMap).name);
        }

        battlescapeGenerator.loadMapForEditing();
    }

	Options::baseXResolution = Options::baseXBattlescape;
	Options::baseYResolution = Options::baseYBattlescape;
	_game->getScreen()->resetDisplay(false);

    _editor->updateMapFileInfo();

	MapEditorState *mapEditorState = new MapEditorState(_editor);
	_game->setState(mapEditorState);
	_game->getSavedGame()->getSavedBattle()->setMapEditorState(mapEditorState);
}

/**
 * Returns to the main menu.
 * @param action Pointer to an action.
 */
void MapEditorMenuState::btnCancelClick(Action *)
{
	_game->popState();
}

/**
 * Switches between new and existing map modes
 * @param action Pointer to an action.
 */
void MapEditorMenuState::btnNewMapClick(Action *action)
{
    _pickTerrainMode = !_pickTerrainMode;

    if (_pickTerrainMode)
    {
        // Press the button down
        _btnNew->setGroup(&_btnNew);
        _btnNew->setText(tr("STR_CANCEL_TERRAIN_PICKING"));
        _txtPickTerrainMode->setText(tr("STR_PICKING_TERRAIN").arg(tr("STR_NEW_MAP")));

        // Consume the action to keep the button held down
        action->getDetails()->type = SDL_NOEVENT;
    }
    else
    {
        // Release the button so it pops back up
        _btnNew->setGroup(0);
        action->getDetails()->type = SDL_MOUSEBUTTONUP;
        _btnNew->mouseRelease(action, this);
        _btnNew->setText(tr("STR_NEW_MAP"));
        _btnNew->draw();
    }

    _txtPickTerrainMode->setVisible(_pickTerrainMode);

    // We have populateMapsList() call populateTerrainsList() instead if necessary
    // This is so it can also handle changing the terrain/crafts/ufos filter
    populateMapsList();

    _editor->getMapEditorSave()->clearMapFileToLoad();
}

/**
 * Sets the information necessary for a new map
 * @param newMapName String name for the map
 * @param newMapX Width for the map
 * @param newMapY Length for the map
 * @param newMapZ Height for the map
 */
void MapEditorMenuState::setNewMapInformation(std::string newMapName, int newMapX, int newMapY, int newMapZ)
{
    _newMapName = newMapName;
    _newMapX = newMapX;
    _newMapY = newMapY;
    _newMapZ = newMapZ;
}

}
