/*
 * Copyright 2010-2024 OpenXcom Developers.
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
#include "MapEditorSave.h"

namespace OpenXcom
{

/**
 * Initializes the runtime holder for MapEditor map file information.
 */
MapEditorSave::MapEditorSave()
{
}

/**
 * Deletes runtime MapEditor map file information.
 */
MapEditorSave::~MapEditorSave()
{
}

/**
 * Gets the data for the map file we want to load.
 * @return pointer to the map info
 */
MapFileInfo *MapEditorSave::getMapFileToLoad()
{
    return &_mapFileToLoad;
}

/**
 * Clears the MapFileInfo for the file we want to load.
 */
void MapEditorSave::clearMapFileToLoad()
{
    MapFileInfo fileInfo;
    _mapFileToLoad = fileInfo;
}

/**
 * Gets the data for the current map being edited.
 * @return pointer to the current map info
 */
MapFileInfo *MapEditorSave::getCurrentMapFile()
{
    return &_currentMapFile;
}

}