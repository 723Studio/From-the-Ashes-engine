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
class BaseContainedAlien;
class Text;
class TextButton;
class TextList;
class Window;

/** Lists living alien specimens and opens their study details. */
class ContainedAlienManagementState : public State
{
private:
	Base* _base;
	bool _forced;
	Window* _window;
	TextButton* _btnOk;
	Text* _txtTitle;
	Text* _txtScientistsAvailable;
	Text* _txtScientistsAllocated;
	Text* _txtContainmentSpace;
	Text* _txtStudySpace;
	Text* _txtSpecimen;
	Text* _txtScientists;
	Text* _txtStatus;
	TextList* _lstAliens;
	std::vector<BaseContainedAlien*> _rows;

	void fillList(size_t scroll);

public:
	ContainedAlienManagementState(Base* base, bool forced = false);
	void init() override;
	void btnOkClick(Action* action);
	void lstAliensClick(Action* action);
};

}
