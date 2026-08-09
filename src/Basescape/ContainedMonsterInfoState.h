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
class Soldier;
class Text;
class TextButton;
class TextList;
class Window;
enum ContainedMonsterAction : int;

/** Preview-and-commit editor for one contained monster specimen. */
class ContainedMonsterInfoState : public State
{
private:
	Base* _base;
	BaseContainedMonster* _monster;
	ContainedMonsterAction _previewAction;
	std::vector<Soldier*> _previewScientists;
	bool _confirmingTerminate;
	Window* _window;
	TextButton* _btnOk;
	TextButton* _btnCancel;
	TextButton* _btnAllocate;
	TextButton* _btnTerminate;
	TextButton* _btnNone;
	TextButton* _btnStudy;
	TextButton* _btnQuarantine;
	TextButton* _btnVivisection;
	TextButton* _btnTaming;
	Text* _txtTitle;
	Text* _txtCurrentHP;
	Text* _txtMaxHP;
	Text* _txtAction;
	Text* _txtProgress;
	Text* _txtScientists;
	Text* _txtAvailability;
	TextList* _lstScientists;

	void refresh();
	void selectAction(ContainedMonsterAction action);
	bool isActionAvailable(ContainedMonsterAction action) const;
	int progressForAction(ContainedMonsterAction action) const;
	int costForAction(ContainedMonsterAction action) const;
	void terminateConfirmed();

public:
	ContainedMonsterInfoState(Base* base, BaseContainedMonster* monster,
		bool confirmTerminate = false);
	void init() override;
	void btnOkClick(Action* action);
	void btnCancelClick(Action* action);
	void btnAllocateClick(Action* action);
	void btnTerminateClick(Action* action);
	void btnNoneClick(Action* action);
	void btnStudyClick(Action* action);
	void btnQuarantineClick(Action* action);
	void btnVivisectionClick(Action* action);
	void btnTamingClick(Action* action);

	BaseContainedMonster* getMonster() const { return _monster; }
	const std::vector<Soldier*>& getPreviewScientists() const
	{
		return _previewScientists;
	}
	void addPreviewScientist(Soldier* scientist);
	void removePreviewScientist(Soldier* scientist);
};

}
