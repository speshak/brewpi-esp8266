/*
 * Copyright 2012-2013 BrewPi/Elco Jacobs.
 * Copyright 2013 Matthew McGowan.
 *
 * This file is part of BrewPi.
 * 
 * BrewPi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * BrewPi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with BrewPi.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Brewpi.h"
#include "TempSensor.h"
#include "TempControl.h"

/**
 * \brief A mock implementation of BasicTempSensor
 *
 * This is a fake temp sensor that shifts it's value on each read.
 */
class MockTempSensor : public BasicTempSensor
{
public:	
	MockTempSensor(temperature initial, temperature delta) : _temperature(initial), _delta(delta), _connected(true) { }
	
	void setConnected(bool connected)
	{
		_connected = connected;
	}
	
	bool isConnected() { return _connected; }

	bool init() {
		return read()!=TEMP_SENSOR_DISCONNECTED;
	}
	
  /**
   * Return the current temperature value.
   *
   * Shifts the temp value up/down (depending on tempcontrol mode) by `_delta` on each read.
   */
	temperature read()
	{
		if (!isConnected())
			return TEMP_SENSOR_DISCONNECTED;

		switch (tempControl.getState()) {
			case ControlState::COOLING:
				_temperature -= _delta;
				break;
			case ControlState::HEATING:
				_temperature += _delta;
				break;
      default:
        ;
        //no-op.  This is here to silence a gcc warning when a switch doesn't
        //have a case for every possible enum value
		}
		
		return _temperature;
	}
	
	private:
  /**
   * Current temperature value
   */
	temperature _temperature;	

  /**
   * Delta to shift per read
   */
	temperature _delta;	
	bool _connected;
};

