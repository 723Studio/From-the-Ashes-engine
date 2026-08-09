/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "ContainedAlienInfoState.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include "ContainedAlienAllocateScientistsState.h"
#include "../Geoscape/ContainedAlienReportState.h"
#include "../Engine/Action.h"
#include "../Engine/Game.h"
#include "../Engine/Options.h"
#include "../Interface/Text.h"
#include "../Interface/TextButton.h"
#include "../Interface/TextList.h"
#include "../Interface/Window.h"
#include "../Mod/RuleContainedAlien.h"
#include "../Mod/RuleResearch.h"
#include "../Savegame/Base.h"
#include "../Savegame/BaseContainedAlien.h"
#include "../Savegame/Soldier.h"

namespace OpenXcom
{

namespace
{

const char* outlookKey(ContainedAlienStudyOutlook outlook)
{
	switch (outlook)
	{
	case CONTAINED_OUTLOOK_NONE: return "STR_CONTAINED_OUTLOOK_NONE_SHORT";
	case CONTAINED_OUTLOOK_POOR: return "STR_CONTAINED_OUTLOOK_POOR_SHORT";
	case CONTAINED_OUTLOOK_AVERAGE: return "STR_CONTAINED_OUTLOOK_AVERAGE_SHORT";
	case CONTAINED_OUTLOOK_GOOD: return "STR_CONTAINED_OUTLOOK_GOOD_SHORT";
	case CONTAINED_OUTLOOK_EXCELLENT: return "STR_CONTAINED_OUTLOOK_EXCELLENT_SHORT";
	default: return "STR_CONTAINED_OUTLOOK_UNKNOWN_SHORT";
	}
}

const char* impactKey(ContainedAlienStrategicImpact impact)
{
	switch (impact)
	{
	case CONTAINED_IMPACT_BREAKTHROUGH: return "STR_CONTAINED_IMPACT_BREAKTHROUGH_SHORT";
	case CONTAINED_IMPACT_SPECIES_PROGRESS: return "STR_CONTAINED_IMPACT_SPECIES_PROGRESS_SHORT";
	default: return "STR_CONTAINED_IMPACT_ROUTINE_SHORT";
	}
}

}

ContainedAlienInfoState::ContainedAlienInfoState(Base* base, BaseContainedAlien* alien) :
	_base(base), _alien(alien), _previewProtocol(alien->getProtocol()),
	_previewScientists(alien->getAssignedScientists())
{
	_screen = false;
	_window = new Window(this, 320, 184, 0, 8);
	_txtTitle = new Text(190, 17, 8, 16);
	_btnTerminate = new TextButton(110, 16, 202, 15);
	_btnAllocate = new TextButton(126, 16, 8, 38);
	_txtHealth = new Text(168, 9, 144, 40);
	_txtStability = new Text(168, 9, 144, 50);
	_txtNode = new Text(168, 18, 144, 60);
	_txtOutlook = new Text(168, 18, 144, 80);
	_txtFurtherStudy = new Text(168, 27, 144, 100);
	_lstScientists = new TextList(126, 103, 8, 58);
	_btnPassive = new TextButton(80, 16, 144, 128);
	_btnActive = new TextButton(80, 16, 232, 128);
	_btnAggressive = new TextButton(168, 16, 144, 147);
	_btnCancel = new TextButton(148, 16, 8, 168);
	_btnOk = new TextButton(148, 16, 164, 168);

	setInterface("prisonerInfo");
	add(_window, "window", "prisonerInfo");
	add(_btnOk, "button1", "prisonerInfo");
	add(_btnCancel, "button1", "prisonerInfo");
	add(_btnAllocate, "button1", "prisonerInfo");
	add(_btnTerminate, "button1", "prisonerInfo");
	add(_btnPassive, "button3", "prisonerInfo");
	add(_btnActive, "button3", "prisonerInfo");
	add(_btnAggressive, "button3", "prisonerInfo");
	add(_txtTitle, "text", "prisonerInfo");
	add(_txtHealth, "text", "prisonerInfo");
	add(_txtStability, "text", "prisonerInfo");
	add(_txtNode, "text", "prisonerInfo");
	add(_txtOutlook, "text", "prisonerInfo");
	add(_txtFurtherStudy, "text", "prisonerInfo");
	add(_lstScientists, "list", "prisonerInfo");
	centerAllSurfaces();
	setWindowBackground(_window, "prisonerInfo");

	_txtTitle->setBig();
	_txtTitle->setText(_alien->getNameAndDesignation(_game->getLanguage()));
	_txtTitle->setWordWrap(true);
	_btnAllocate->setText(tr("STR_ALLOCATE_SCIENTISTS"));
	_btnAllocate->onMouseClick((ActionHandler)&ContainedAlienInfoState::btnAllocateClick);
	_btnTerminate->setText(tr("STR_TERMINATE"));
	_btnTerminate->onMouseClick((ActionHandler)&ContainedAlienInfoState::btnTerminateClick);
	_btnPassive->onMouseClick((ActionHandler)&ContainedAlienInfoState::btnPassiveClick);
	_btnActive->onMouseClick((ActionHandler)&ContainedAlienInfoState::btnActiveClick);
	_btnAggressive->onMouseClick((ActionHandler)&ContainedAlienInfoState::btnAggressiveClick);
	_btnCancel->setText(tr("STR_CANCEL_UC"));
	_btnCancel->onMouseClick((ActionHandler)&ContainedAlienInfoState::btnCancelClick);
	_btnCancel->onKeyboardPress((ActionHandler)&ContainedAlienInfoState::btnCancelClick, Options::keyCancel);
	_btnOk->setText(tr("STR_OK"));
	_btnOk->onMouseClick((ActionHandler)&ContainedAlienInfoState::btnOkClick);
	_btnOk->onKeyboardPress((ActionHandler)&ContainedAlienInfoState::btnOkClick, Options::keyOk);
	_lstScientists->setColumns(1, 126);
	_lstScientists->setBackground(_window);
	_lstScientists->setWordWrap(true);
	_txtFurtherStudy->setWordWrap(true);
}

void ContainedAlienInfoState::init()
{
	State::init();
	refresh();
}

void ContainedAlienInfoState::refresh()
{
	if (!_alien->canUseProtocol(_previewProtocol))
	{
		_previewProtocol = CONTAINED_PROTOCOL_PASSIVE;
	}
	_lstScientists->clearList();
	for (auto* scientist : _previewScientists)
	{
		_lstScientists->addRow(1, scientist->getName(true, 20).c_str());
	}
	std::ostringstream health, stability;
	health << tr("STR_HEALTH") << ": " << static_cast<int>(std::round(_alien->getHealth()))
		<< "/" << static_cast<int>(std::round(_alien->getMaxHealth()));
	stability << tr("STR_STABILITY") << ": " << static_cast<int>(std::round(_alien->getStability()));
	_txtHealth->setText(health.str());
	_txtStability->setText(stability.str());
	if (const auto* node = _alien->getCurrentStudyNode())
	{
		_txtNode->setText(tr("STR_CURRENT_STUDY_NODE").arg(tr(node->getType())));
	}
	else
	{
		_txtNode->setText(tr("STR_COMPLETE"));
	}
	auto forecast = _alien->getForecast(_previewProtocol, &_previewScientists);
	std::ostringstream outlook;
	outlook << tr(outlookKey(forecast.outlook)) << " / "
		<< tr(impactKey(_alien->getStrategicImpact()));
	_txtOutlook->setText(outlook.str());
	const auto unmetResearch = _alien->getUnmetStudyResearch();
	if (!unmetResearch.empty())
	{
		std::ostringstream researchNames;
		for (size_t i = 0; i < unmetResearch.size(); ++i)
		{
			if (i != 0) researchNames << ", ";
			researchNames << tr(unmetResearch[i]->getName());
		}
		std::ostringstream requirements;
		requirements << tr("STR_CONTAINED_ALIEN_STUDY_BLOCKED") << ": "
			<< tr("STR_CONTAINED_ALIEN_REQUIRES_RESEARCH").arg(researchNames.str());
		_txtFurtherStudy->setText(requirements.str());
	}
	else if (_alien->getCurrentStudyNode())
	{
		_txtFurtherStudy->setText(tr("STR_CONTAINED_ALIEN_STUDY_READY"));
	}
	else _txtFurtherStudy->setText(std::string());
	std::ostringstream passive, active, aggressive;
	passive << (_previewProtocol == CONTAINED_PROTOCOL_PASSIVE ? "[ " : "")
		<< tr("STR_CONTAINED_PROTOCOL_PASSIVE")
		<< (_previewProtocol == CONTAINED_PROTOCOL_PASSIVE ? " ]" : "");
	active << (_previewProtocol == CONTAINED_PROTOCOL_ACTIVE ? "[ " : "")
		<< tr("STR_CONTAINED_PROTOCOL_ACTIVE")
		<< (_previewProtocol == CONTAINED_PROTOCOL_ACTIVE ? " ]" : "");
	aggressive << (_previewProtocol == CONTAINED_PROTOCOL_AGGRESSIVE ? "[ " : "")
		<< tr("STR_CONTAINED_PROTOCOL_AGGRESSIVE")
		<< (_previewProtocol == CONTAINED_PROTOCOL_AGGRESSIVE ? " ]" : "");
	_btnPassive->setText(passive.str());
	_btnActive->setText(active.str());
	_btnAggressive->setText(aggressive.str());
	_btnActive->setDisabled(
		!_alien->canUseProtocol(CONTAINED_PROTOCOL_ACTIVE));
	_btnAggressive->setDisabled(
		!_alien->canUseProtocol(CONTAINED_PROTOCOL_AGGRESSIVE));
}

void ContainedAlienInfoState::selectProtocol(ContainedAlienStudyProtocol protocol)
{
	if (!_alien->canUseProtocol(protocol))
	{
		return;
	}
	_previewProtocol = protocol;
	refresh();
}

void ContainedAlienInfoState::btnOkClick(Action*)
{
	if (!_alien->setProtocol(_previewProtocol))
	{
		_previewProtocol = CONTAINED_PROTOCOL_PASSIVE;
		_alien->setProtocol(_previewProtocol);
	}
	_alien->clearAssignments();
	for (auto* scientist : _previewScientists)
	{
		scientist->clearBaseDuty();
		scientist->setActiveContainedAlien(_alien);
	}
	_game->popState();
}

void ContainedAlienInfoState::btnCancelClick(Action*)
{
	_game->popState();
}

void ContainedAlienInfoState::btnAllocateClick(Action*)
{
	_game->pushState(new ContainedAlienAllocateScientistsState(_base, this));
}

void ContainedAlienInfoState::btnTerminateClick(Action*)
{
	const std::string specimenName = _alien->getNameAndDesignation(_game->getLanguage());
	ContainedAlienProcessResult result = _alien->terminate();
	result.containedAlienStudiesUnblocked =
		_alien->takePendingStudyUnblockNotification();
	delete _alien;
	_alien = nullptr;
	_game->popState();
	_game->pushState(new ContainedAlienReportState(_base, specimenName, result, true));
}

void ContainedAlienInfoState::btnPassiveClick(Action*) { selectProtocol(CONTAINED_PROTOCOL_PASSIVE); }
void ContainedAlienInfoState::btnActiveClick(Action*) { selectProtocol(CONTAINED_PROTOCOL_ACTIVE); }
void ContainedAlienInfoState::btnAggressiveClick(Action*) { selectProtocol(CONTAINED_PROTOCOL_AGGRESSIVE); }

void ContainedAlienInfoState::addPreviewScientist(Soldier* scientist)
{
	if (std::find(_previewScientists.begin(), _previewScientists.end(), scientist) == _previewScientists.end())
	{
		_previewScientists.push_back(scientist);
	}
}

void ContainedAlienInfoState::removePreviewScientist(Soldier* scientist)
{
	_previewScientists.erase(std::remove(_previewScientists.begin(), _previewScientists.end(), scientist),
		_previewScientists.end());
}

}
