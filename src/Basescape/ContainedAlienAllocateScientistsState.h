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
class ContainedAlienInfoState;
class Soldier;
class Text;
class TextButton;
class TextList;
class Window;
struct SortFunctor;

/** Selects scientists for a contained alien using standard allocation controls. */
class ContainedAlienAllocateScientistsState : public State
{
private:
	Base* _base;
	ContainedAlienInfoState* _detail;
	Window* _window;
	TextButton* _btnOk;
	Text* _txtTitle;
	Text* _txtCapacity;
	Text* _txtName;
	Text* _txtAssignment;
	ComboBox* _cbxSortBy;
	TextList* _lstScientists;
	std::vector<Soldier*> _originalOrder;
	std::vector<Soldier*> _rows;
	std::vector<SortFunctor*> _sortFunctors;
	Uint8 _busyColor;

	void fillList(size_t scroll);
	int maximumForCurrentAlien() const;

public:
	ContainedAlienAllocateScientistsState(Base* base, ContainedAlienInfoState* detail);
	~ContainedAlienAllocateScientistsState() override;
	void init() override;
	void btnOkClick(Action* action);
	void cbxSortByChange(Action* action);
	void lstScientistsClick(Action* action);
};

}
