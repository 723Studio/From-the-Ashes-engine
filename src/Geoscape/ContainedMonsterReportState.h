#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "../Engine/State.h"
#include "../Savegame/BaseContainedMonster.h"
#include <string>
#include <vector>

namespace OpenXcom
{

class Base;
class Text;
class TextButton;
class Window;

/** Report for a contained monster completion, death or termination. */
class ContainedMonsterReportState : public State
{
private:
	Base* _base;
	std::string _specimenName;
	ContainedMonsterProcessResult _result;
	std::vector<std::string> _articleIds;
	std::vector<std::string> _newResearchNotifications;
	bool _containedAlienStudiesUnblocked = false;
	Window* _window;
	Text* _txtTitle;
	Text* _txtDescription;
	TextButton* _btnOk;
	TextButton* _btnReport;
	TextButton* _btnPromotions;

public:
	ContainedMonsterReportState(Base* base, std::string specimenName,
		const ContainedMonsterProcessResult& result);
	void btnOkClick(Action* action);
	void btnReportClick(Action* action);
	void btnPromotionsClick(Action* action);
};

}
