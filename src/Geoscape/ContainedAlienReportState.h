#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "../Engine/State.h"
#include "../Savegame/BaseContainedAlien.h"
#include <string>
#include <vector>

namespace OpenXcom
{

class Base;
class Text;
class TextButton;
class Window;

/** Aggregated report for a study-node completion, death or manual termination. */
class ContainedAlienReportState : public State
{
private:
	Base* _base;
	bool _death;
	std::string _specimenName;
	ContainedAlienProcessResult _result;
	std::vector<std::string> _articleIds;
	std::vector<std::string> _newResearchNotifications;
	Window* _window;
	Text* _txtTitle;
	Text* _txtDescription;
	TextButton* _btnOk;
	TextButton* _btnReport;
	TextButton* _btnPromotions;

public:
	ContainedAlienReportState(Base* base, std::string specimenName,
		const ContainedAlienProcessResult& result, bool death = false);
	void btnOkClick(Action* action);
	void btnReportClick(Action* action);
	void btnPromotionsClick(Action* action);
};

}
