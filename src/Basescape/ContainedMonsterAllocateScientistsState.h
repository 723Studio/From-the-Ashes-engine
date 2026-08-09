#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "../Engine/State.h"
#include "SoldierSortUtil.h"
#include <vector>

namespace OpenXcom
{

class Base;
class ComboBox;
class ContainedMonsterInfoState;
class Soldier;
class Text;
class TextButton;
class TextList;
class Window;
struct SortFunctor;

/** Selects free scientists for one contained monster specimen. */
class ContainedMonsterAllocateScientistsState : public State
{
private:
	Base* _base;
	ContainedMonsterInfoState* _detail;
	Window* _window;
	TextButton* _btnOk;
	Text* _txtTitle;
	Text* _txtCount;
	Text* _txtName;
	Text* _txtAssignment;
	ComboBox* _cbxSortBy;
	TextList* _lstScientists;
	std::vector<Soldier*> _originalOrder;
	std::vector<Soldier*> _rows;
	std::vector<SortFunctor*> _sortFunctors;

	void fillList(size_t scroll);

public:
	ContainedMonsterAllocateScientistsState(Base* base,
		ContainedMonsterInfoState* detail);
	~ContainedMonsterAllocateScientistsState() override;
	void init() override;
	void btnOkClick(Action* action);
	void cbxSortByChange(Action* action);
	void lstScientistsClick(Action* action);
};

}
