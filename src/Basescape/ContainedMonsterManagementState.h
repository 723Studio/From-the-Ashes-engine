#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "../Engine/State.h"
#include <vector>

namespace OpenXcom
{

class Base;
class BaseContainedMonster;
class Text;
class TextButton;
class TextList;
class Window;

/** Lists living monster specimens and opens their management details. */
class ContainedMonsterManagementState : public State
{
private:
	Base* _base;
	bool _forced;
	Window* _window;
	TextButton* _btnOk;
	TextButton* _btnTerminate;
	Text* _txtTitle;
	Text* _txtSpaceAvailable;
	Text* _txtSpaceUsed;
	Text* _txtSpaceFree;
	Text* _txtSpecimen;
	Text* _txtHealth;
	Text* _txtAction;
	Text* _txtProgress;
	Text* _txtScientists;
	TextList* _lstMonsters;
	std::vector<BaseContainedMonster*> _rows;

	void fillList(size_t scroll);

public:
	ContainedMonsterManagementState(Base* base, bool forced = false);
	void init() override;
	void btnOkClick(Action* action);
	void btnTerminateClick(Action* action);
	void lstMonstersClick(Action* action);
};

}
