#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 */
#include "../Engine/State.h"
#include "../Savegame/ContainedAlienStudy.h"
#include <vector>

namespace OpenXcom
{

class Base;
class BaseContainedAlien;
class Soldier;
class Text;
class TextButton;
class TextList;
class Window;

/** Preview-and-commit editor for one contained alien study assignment. */
class ContainedAlienInfoState : public State
{
private:
	Base* _base;
	BaseContainedAlien* _alien;
	ContainedAlienStudyProtocol _previewProtocol;
	std::vector<Soldier*> _previewScientists;
	Window* _window;
	TextButton* _btnOk;
	TextButton* _btnCancel;
	TextButton* _btnAllocate;
	TextButton* _btnTerminate;
	TextButton* _btnPassive;
	TextButton* _btnActive;
	TextButton* _btnAggressive;
	Text* _txtTitle;
	Text* _txtHealth;
	Text* _txtStability;
	Text* _txtNode;
	Text* _txtOutlook;
	Text* _txtFurtherStudy;
	TextList* _lstScientists;

	void refresh();
	void selectProtocol(ContainedAlienStudyProtocol protocol);

public:
	ContainedAlienInfoState(Base* base, BaseContainedAlien* alien);
	void init() override;
	void btnOkClick(Action* action);
	void btnCancelClick(Action* action);
	void btnAllocateClick(Action* action);
	void btnTerminateClick(Action* action);
	void btnPassiveClick(Action* action);
	void btnActiveClick(Action* action);
	void btnAggressiveClick(Action* action);

	BaseContainedAlien* getAlien() const { return _alien; }
	const std::vector<Soldier*>& getPreviewScientists() const { return _previewScientists; }
	void addPreviewScientist(Soldier* scientist);
	void removePreviewScientist(Soldier* scientist);
};

}
