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
#include "MovingTarget.h"
#include "../fmath.h"
#include "SerializationHelper.h"

namespace OpenXcom
{

/**
 * Initializes a moving target with blank coordinates.
 */
MovingTarget::MovingTarget() : Target(), _dest(0), _speedLon(0.0), _speedLat(0.0), _speedRadian(0.0), _targetLon(0.0), _targetLat(0.0), _speed(0)
{
}

/**
 * Make sure to cleanup the target's destination followers.
 */
MovingTarget::~MovingTarget()
{
	setDestination(0);
}

/**
 * Loads the moving target from a YAML file.
 * @param node YAML node.
 */
void MovingTarget::load(const YAML::YamlNodeReader& reader)
{
	Target::load(reader);
	reader.tryRead("speedLon", _speedLon);
	reader.tryRead("speedLat", _speedLat);
	reader.tryRead("speedRadian", _speedRadian);
	reader.tryRead("speed", _speed);
}

/**
 * Saves the moving target to a YAML file.
 * @return YAML node.
 */
void MovingTarget::save(YAML::YamlNodeWriter writer) const
{
	writer.setAsMap();
	Target::save(writer);
	if (_dest)
		_dest->saveId(writer["dest"]);
	writer.write("speedLon", serializeDouble(_speedLon));
	writer.write("speedLat", serializeDouble(_speedLat));
	writer.write("speedRadian", serializeDouble(_speedRadian));
	writer.write("speed", _speed);
}

/**
 * Returns the destination the moving target is heading to.
 * @return Pointer to destination.
 */
Target *MovingTarget::getDestination() const
{
	return _dest;
}

/**
 * Changes the destination the moving target is heading to.
 * @param dest Pointer to destination.
 */
void MovingTarget::setDestination(Target *dest)
{
	// Remove moving target from old destination's followers
	if (_dest != 0)
	{
		for (auto iter = _dest->getFollowers()->begin(); iter != _dest->getFollowers()->end(); ++iter)
		{
			if ((*iter) == this)
			{
				_dest->getFollowers()->erase(iter);
				break;
			}
		}
	}
	_dest = dest;
	// Add moving target to new destination's followers
	if (_dest != 0)
	{
		_dest->getFollowers()->push_back(this);
	}
	calculateSpeed();
}

/**
 * Returns the speed of the moving target.
 * @return Speed in knots.
 */
int MovingTarget::getSpeed() const
{
	return _speed;
}

/**
 * Returns the radial speed of the moving target.
 * @return Speed in 1 / 5 sec.
 */
double MovingTarget::getSpeedRadian() const
{
	return _speedRadian;
}

/**
 * Converts a speed in degrees to a speed in radians.
 * Each nautical mile is 1/60th of a degree.
 * Each hour contains 720 5-seconds.
 * @param speed Speed in degrees.
 * @return Speed in radians.
 */
double MovingTarget::calculateRadianSpeed(int speed)
{
	return Nautical(speed) / 720.0;
}

/**
 * Changes the speed of the moving target
 * and converts it from standard knots (nautical miles per hour)
 * into radians per 5 in-game seconds.
 * @param speed Speed in knots.
 */
void MovingTarget::setSpeed(int speed)
{
	_speed = speed;
	_speedRadian = calculateRadianSpeed(_speed);
	calculateSpeed();
}

/**
 * Calculates the speed vector based on the
 * great circle distance to destination and
 * current raw speed.
 */
void MovingTarget::calculateSpeed()
{
	updateTargetCoordinates();
	if (_dest != 0)
	{
		double dLon, dLat, length;
		dLon = sin(_targetLon - _lon) * cos(_targetLat);
		dLat = cos(_lat) * sin(_targetLat) - sin(_lat) * cos(_targetLat) * cos(_targetLon - _lon);
		length = sqrt(dLon * dLon + dLat * dLat);
		_speedLat = dLat / length * _speedRadian;
		_speedLon = dLon / length * _speedRadian / cos(_lat + _speedLat);

		// Check for invalid speeds when a division by zero occurs due to near-zero values
		if (!(_speedLon == _speedLon) || !(_speedLat == _speedLat))
		{
			_speedLon = 0;
			_speedLat = 0;
		}
	}
	else
	{
		_speedLon = 0;
		_speedLat = 0;
	}
}

/**
 * Checks if the moving target has reached its destination.
 * @return True if it has, False otherwise.
 */
bool MovingTarget::reachedDestination() const
{
	if (_dest == 0)
	{
		return false;
	}
	return ( AreSame(_dest->getLongitude(), _lon) && AreSame(_dest->getLatitude(), _lat) );
}

/**
 * Executes a movement cycle for the moving target.
 */
void MovingTarget::move()
{
	calculateSpeed();
	if (_dest != 0)
	{
		if (getDistance(_targetLon, _targetLat) > _speedRadian)
		{
			setLongitude(_lon + _speedLon);
			setLatitude(_lat + _speedLat);
		}
		else
		{
			if (getDistance(_dest) > _speedRadian)
			{
				setLongitude(_targetLon);
				setLatitude(_targetLat);
			}
			else
			{
				setLongitude(_dest->getLongitude());
				setLatitude(_dest->getLatitude());
			}
		}
	}
}

/**
 * Updates the coordinates used for movement toward the current destination.
 */
void MovingTarget::updateTargetCoordinates()
{
	if (_dest != 0)
	{
		_targetLat = _dest->getLatitude();
		_targetLon = _dest->getLongitude();
	}
	else
	{
		_targetLat = _lat;
		_targetLon = _lon;
	}
}

}
