#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#include <vector>
#include "../Mod/RuleSoldier.h"

namespace OpenXcom
{

/**
 * Immutable description of one role promotion caused by a specific game event.
 * UI code owns batches of these records instead of scanning global soldier flags.
 */
struct RolePromotion
{
	int soldierId = 0;
	SoldierRole role = ROLE_NONE;
	int previousRank = 0;
	int newRank = 0;
};

using PromotionBatch = std::vector<RolePromotion>;

}
