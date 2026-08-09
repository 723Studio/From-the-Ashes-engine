/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "ContainedMonsterReportState.h"

#include <algorithm>
#include <sstream>
#include "GeoscapeState.h"
#include "../Battlescape/PromotionsState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/Window.h"
#include "../Mod/Mod.h"
#include "../Mod/RuleItem.h"
#include "../Mod/RuleResearch.h"
#include "../Savegame/Base.h"
#include "../Savegame/SavedGame.h"
#include "../Savegame/Soldier.h"
#include "../Ufopaedia/Ufopaedia.h"

namespace OpenXcom
{

namespace
{

const char* titleKey(const ContainedMonsterProcessResult& result)
{
	if (result.studyCompleted)
	{
		return "STR_CONTAINED_MONSTER_STUDY_COMPLETE";
	}
	if (result.vivisectionCompleted)
	{
		return "STR_CONTAINED_MONSTER_VIVISECTION_COMPLETE";
	}
	if (result.tamingCompleted)
	{
		return "STR_CONTAINED_MONSTER_TAMING_COMPLETE";
	}
	if (result.died)
	{
		return "STR_CONTAINED_MONSTER_DIED";
	}
	if (result.terminated)
	{
		return "STR_TERMINATE_CONTAINED_MONSTER";
	}
	return "STR_CONTAINED_MONSTER_MANAGEMENT";
}

}

ContainedMonsterReportState::ContainedMonsterReportState(Base* base,
	std::string specimenName, const ContainedMonsterProcessResult& result) :
	_base(base), _specimenName(std::move(specimenName)), _result(result),
	_newResearchNotifications(result.researchNotifications)
{
	std::vector<const RuleResearch*> discoveredResearch;
	for (const auto& researchId : result.newlyDiscoveredResearchIds)
	{
		if (const auto* research = _game->getMod()->getResearch(researchId, false))
		{
			if (std::find(discoveredResearch.begin(), discoveredResearch.end(), research) ==
				discoveredResearch.end())
			{
				discoveredResearch.push_back(research);
			}
		}
	}
	_articleIds = Ufopaedia::getResearchArticleIds(
		_game->getSavedGame(), _game->getMod(), discoveredResearch);
	if (GeoscapeState* geoscape = _game->getGeoscapeState())
	{
		_containedAlienStudiesUnblocked =
			geoscape->hasNewlyUnblockedContainedAlienStudies(
				result.newlyDiscoveredResearchIds);
	}

	_screen = false;
	_window = new Window(this, 270, 150, 25, 25, POPUP_BOTH);
	_txtTitle = new Text(250, 17, 35, 36);
	_txtDescription = new Text(250, 70, 35, 58);
	_btnOk = new TextButton(78, 16, 35, 151);
	_btnReport = new TextButton(78, 16, 121, 151);
	_btnPromotions = new TextButton(78, 16, 207, 151);
	const std::string interface =
		(result.died || result.terminated) ?
		"geoPrisonerReportDeath" : "geoPrisonerReportNormal";
	setInterface(interface);
	add(_window, "window", interface);
	add(_txtTitle, "text1", interface);
	add(_txtDescription, "text2", interface);
	add(_btnOk, "button", interface);
	add(_btnReport, "button", interface);
	add(_btnPromotions, "button", interface);
	centerAllSurfaces();
	setWindowBackground(_window, interface);

	_txtTitle->setBig();
	_txtTitle->setAlign(ALIGN_CENTER);
	_txtTitle->setText(tr(titleKey(result)));
	_txtDescription->setAlign(ALIGN_CENTER);
	_txtDescription->setWordWrap(true);
	std::ostringstream description;
	description << _specimenName;
	for (const auto& produced : result.producedItems)
	{
		description << "\n" << tr(produced.first->getType()) << " x" << produced.second;
	}
	if (result.tamedSoldier)
	{
		description << "\n" << result.tamedSoldier->getName(true);
	}
	if (_containedAlienStudiesUnblocked)
	{
		description << "\n" << tr("STR_CONTAINED_ALIEN_STUDIES_CAN_CONTINUE");
	}
	_txtDescription->setText(description.str());

	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ContainedMonsterReportState::btnOkClick);
	_btnOk->onKeyboardPress(
		(ActionHandler)&ContainedMonsterReportState::btnOkClick, Options::keyCancel);
	_btnReport->setText(tr("STR_VIEW_REPORT"));
	_btnReport->onMouseClick(
		(ActionHandler)&ContainedMonsterReportState::btnReportClick);
	_btnReport->setVisible(!_articleIds.empty());
	if (_articleIds.empty())
	{
		_btnOk->onKeyboardPress(
			(ActionHandler)&ContainedMonsterReportState::btnOkClick, Options::keyOk);
	}
	else
	{
		_btnReport->onKeyboardPress(
			(ActionHandler)&ContainedMonsterReportState::btnReportClick, Options::keyOk);
	}
	_btnPromotions->setText(tr("STR_VIEW_PROMOTIONS"));
	_btnPromotions->onMouseClick(
		(ActionHandler)&ContainedMonsterReportState::btnPromotionsClick);
	_btnPromotions->setVisible(!_result.promotions.empty());
}

void ContainedMonsterReportState::btnOkClick(Action*)
{
	Game* game = _game;
	Base* base = _base;
	const auto researchIds = _newResearchNotifications;
	game->popState();
	if (GeoscapeState* geoscape = game->getGeoscapeState())
	{
		geoscape->showNewResearchNotifications(base, researchIds);
	}
}

void ContainedMonsterReportState::btnReportClick(Action*)
{
	for (auto it = _articleIds.rbegin(); it != _articleIds.rend(); ++it)
	{
		Ufopaedia::openArticle(_game, *it);
	}
}

void ContainedMonsterReportState::btnPromotionsClick(Action*)
{
	_game->pushState(new PromotionsState(_result.promotions));
}

}
