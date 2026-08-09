#pragma once
/*
 * Copyright 2010-2022 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include "../Engine/State.h"
#include "../Savegame/Promotion.h"
#include <vector>

namespace OpenXcom
{

class Base;
class GeoscapeState;
class IntelProject;
class RuleResearch;
class Text;
class TextButton;
class Window;

enum IntelReportMode : int
{
	INTEL_REPORT_BREAKTHROUGH = 0,
	INTEL_REPORT_BLOCKED = 1,
	INTEL_REPORT_UNBLOCKED = 2,
	INTEL_REPORT_COMPLETED = 3
};

/**
 * Unified intelligence notification for discoveries, blocked/unblocked
 * investigations and completed projects.
 */
class IntelReportState : public State
{
private:
	Window* _window;
	Text* _txtTitle;
	Text* _txtProject;
	Text* _txtDescription;
	TextButton* _btnOk;
	TextButton* _btnReport;
	TextButton* _btnAllocate;
	TextButton* _btnPromotions;
	IntelProject* _project;
	GeoscapeState* _state;
	Base* _base;
	Base* _researchBase;
	IntelReportMode _mode;
	std::vector<std::string> _articleIds;
	PromotionBatch _promotions;
	std::vector<std::string> _newResearchNotifications;
	bool _containedAlienStudiesUnblocked;

public:
	IntelReportState(IntelProject* project, GeoscapeState* state, Base* base,
		IntelReportMode mode, const std::vector<std::string>& articleIds = {},
		const PromotionBatch& promotions = {}, Base* researchBase = nullptr,
		std::vector<std::string> newResearchNotifications = {},
		bool containedAlienStudiesUnblocked = false);
	~IntelReportState();

	void btnOkClick(Action* action);
	void btnReportClick(Action* action);
	void btnAllocateClick(Action* action);
	void btnPromotionsClick(Action* action);
};

}
