#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "Target.h"

namespace OpenXcom
{

/**
 * Base class for moving targets on the globe
 * with a certain speed and destination.
 */
class MovingTarget : public Target
{
protected:
	static const double GLOBE_RADIUS;

	Target *_dest;
	double _speedLon, _speedLat, _speedRadian;
	double _targetLon, _targetLat;
	int _speed;

	/// Calculates a new speed vector to the destination.
	virtual void calculateSpeed();
	/// Updates the current destination coordinates.
	void updateTargetCoordinates();
	/// Converts a speed to radians.
	static double calculateRadianSpeed(int speed);
	/// Creates a moving target.
	MovingTarget();
public:
	/// Cleans up the moving target.
	virtual ~MovingTarget();
	/// Loads the moving target from YAML.
	virtual void load(const YAML::YamlNodeReader& reader) override;
	/// Saves the moving target to YAML.
	virtual void save(YAML::YamlNodeWriter writer) const override;
	/// Gets the moving target's destination.
	Target *getDestination() const;
	/// Sets the moving target's destination.
	virtual void setDestination(Target *dest);
	/// Gets the moving target's speed.
	int getSpeed() const;
	/// Gets the moving target's radial speed.
	double getSpeedRadian() const;
	/// Sets the moving target's speed.
	void setSpeed(int speed);
	/// Has the moving target reached its destination?
	bool reachedDestination() const;
	/// Move towards the destination.
	void move();
};

}
