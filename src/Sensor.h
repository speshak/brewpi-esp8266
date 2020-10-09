/*
 * Copyright 2013 BrewPi/Elco Jacobs.
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

/**
 * \brief A sensor
 *
 * Base class for all devices that sense something.
 *
 * \tparam T - The data type that the sensor produces when read.
 */
template <class T> class Sensor {
public:
  /**
   * \brief Read the sensor
   *
   * \returns The sensed value from the sensor.
   */
  virtual T sense() = 0;

  virtual ~Sensor() {}
};

/**
 * \brief A Sensor that stores a value
 *
 * \tparam T - The data type that the sensor produces when read.
 */
template <class T> class ValueSensor : public Sensor<T> {
public:
  /**
   * \brief Constructor
   *
   * \param initial - Initial sensor value
   */
  ValueSensor(const T initial) : value(initial) {}

  /**
   * \brief Read the sensor
   *
   * \returns The sensed value from the sensor.
   */
  virtual T sense() { return (T)0; }

  /**
   * \brief Set the sensor value
   * \param _value - Value to set
   */
  void setValue(const T _value) { value = _value; }

private:
  T value; //!< Stored sensor value
};

typedef Sensor<bool> SwitchSensor;
