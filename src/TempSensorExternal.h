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

/**
 * \brief A temp sensor whose value is not read from the device, but set in code.
 * This is used by the simulator.
 */
class ExternalTempSensor : public BasicTempSensor {
public:
  /**
   * \brief Constructor
   *
   * \param connected - Sensor connection state
   */
  ExternalTempSensor(const bool connected = false) : _temperature(0), _connected(false) { setConnected(connected); }

  /**
   * \brief Set the sensor connection state
   *
   * \param connected - Sensor connection state
   */
  void setConnected(const bool connected) { this->_connected = connected; }


  /**
   * \brief Get sensor connection state
   */
  bool isConnected() const { return _connected; }

  /**
   * \brief Initialize sensor
   * \returns true if the sensor is connected
   */
  bool init() { return read() != TEMP_SENSOR_DISCONNECTED; }

  /**
   * \brief Read the sensor value
   */
  temperature read() {
    if (!isConnected())
      return TEMP_SENSOR_DISCONNECTED;
    return _temperature;
  }

  /**
   * \brief Set the sensor value
   * \param newTemp - Temperature to set
   */
  void setValue(const temperature newTemp) { _temperature = newTemp; }

private:
  temperature _temperature; //!< Sensor temperature value
  bool _connected; //!< Sensor connection status
};
