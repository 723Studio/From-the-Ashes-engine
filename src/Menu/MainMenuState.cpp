#include "MainMenuState.h"
#include <sstream>
#include "../version.h"
#include "../Engine/Game.h"
#include "../Engine/Logger.h"
#include "../Engine/Screen.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Interface/Text.h"
#include "DevelopmentToolsState.h"
#include "NewGameState.h"
#include "ListLoadState.h"
#include "OptionsVideoState.h"
#include "../Engine/Options.h"

namespace OpenXcom
{

GoToMainMenuState::GoToMainMenuState() {}
GoToMainMenuState::~GoToMainMenuState() {}

void GoToMainMenuState::init()
{
	Screen::updateScale(Options::geoscapeScale, Options::baseXGeoscape, Options::baseYGeoscape, true);
	_game->getScreen()->resetDisplay(false);
	_game->setState(new MainMenuState());
}

MainMenuState::MainMenuState()
{
	_window = new Window(this, 256, 160, 32, 20, POPUP_BOTH);
	_btnNewGame = new TextButton(92, 20, 64, 102);
	_btnLoad = new TextButton(92, 20, 164, 102);
	_btnOptions = new TextButton(92, 20, 64, 135);
	_btnQuit = new TextButton(92, 20, 164, 135);
	_btnDevelopment = new TextButton(66, 20, 215, 27);
	_txtUpdateInfo = new Text(320, 17, 0, 11);
	_txtTitle = new Text(246, 32, 37, 52);
	_txtVersion = new Text(240, 10, 40, 164);

	setInterface("mainMenu");

	add(_window, "window", "mainMenu");
	add(_btnNewGame, "button", "mainMenu");
	add(_btnLoad, "button", "mainMenu");
	add(_btnOptions, "button", "mainMenu");
	add(_btnQuit, "button", "mainMenu");
	add(_btnDevelopment, "button", "mainMenu");
	add(_txtUpdateInfo, "text", "mainMenu");
	add(_txtTitle, "text", "mainMenu");
	add(_txtVersion, "text", "mainMenu");

	centerAllSurfaces();
	setWindowBackground(_window, "mainMenu");

	_btnNewGame->setText(tr("STR_NEW_GAME"));
	_btnNewGame->onMouseClick((ActionHandler)&MainMenuState::btnNewGameClick);
	_btnLoad->setText(tr("STR_LOAD_SAVED_GAME"));
	_btnLoad->onMouseClick((ActionHandler)&MainMenuState::btnLoadClick);
	_btnOptions->setText(tr("STR_OPTIONS"));
	_btnOptions->onMouseClick((ActionHandler)&MainMenuState::btnOptionsClick);
	_btnQuit->setText(tr("STR_QUIT"));
	_btnQuit->onMouseClick((ActionHandler)&MainMenuState::btnQuitClick);
	_btnDevelopment->setText(tr("STR_DEVELOPMENT"));
	_btnDevelopment->onMouseClick((ActionHandler)&MainMenuState::btnDevelopmentClick);
	_btnDevelopment->setVisible(Options::debug);

	_txtUpdateInfo->setAlign(ALIGN_CENTER);
	_txtUpdateInfo->setWordWrap(true);
	_txtUpdateInfo->setVisible(false);
	if (Options::isModifiedContentLoaded())
	{
		_txtUpdateInfo->setText(tr("STR_FTA_MODIFIED_CONTENT_LOADED"));
		_txtUpdateInfo->setColor(81);
		_txtUpdateInfo->setVisible(true);
	}

	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setBig();
	_txtTitle->setText(tr("STR_OPENXCOM"));

	_txtVersion->setAlign(ALIGN_RIGHT);
	std::ostringstream version;
	version << FTA_PRODUCT_VERSION_DISPLAY;
	_txtVersion->setText(version.str());
}

void MainMenuState::init()
{
	State::init();
	_game->setModifiedContentLoaded(Options::isModifiedContentLoaded());
	if (Options::getLoadLastSave() && !Options::getLoadThisSave().empty())
	{
		Log(LOG_INFO) << "Loading saved game passed as parameter";
		btnLoadClick(NULL);
	}
	else if (Options::getLoadLastSave() && _game->getSavedGame()->getList(_game->getLanguage(), true).size() > 0)
	{
		Log(LOG_INFO) << "Loading last saved game";
		btnLoadClick(NULL);
	}
}

MainMenuState::~MainMenuState() {}

void MainMenuState::btnNewGameClick(Action *) { _game->pushState(new NewGameState); }
void MainMenuState::btnLoadClick(Action *) { _game->pushState(new ListLoadState(OPT_MENU)); }
void MainMenuState::btnOptionsClick(Action *)
{
	Options::backupDisplay();
	_game->pushState(new OptionsVideoState(OPT_MENU));
}
void MainMenuState::btnDevelopmentClick(Action *) { _game->pushState(new DevelopmentToolsState); }
void MainMenuState::btnQuitClick(Action *) { _game->quit(); }

void MainMenuState::resize(int &dX, int &dY)
{
	dX = Options::baseXResolution;
	dY = Options::baseYResolution;
	Screen::updateScale(Options::geoscapeScale, Options::baseXGeoscape, Options::baseYGeoscape, true);
	dX = Options::baseXResolution - dX;
	dY = Options::baseYResolution - dY;
	State::resize(dX, dY);
}

}
