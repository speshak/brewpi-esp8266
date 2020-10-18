/*
 * Copyright 2013 Matthew McGowan
 * Copyright 2013 BrewPi/Elco Jacobs.
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
#include "FastDigitalPin.h"

/**
 * \brief An actuator simply turns something on or off.
 */
class Actuator {
public:
  /**
   * \brief Set the state of the actuator
   *
   * @param active - New state
   */
  virtual void setActive(const bool active) = 0;

  /**
   * \brief Check if current actuator state is active
   */
  virtual bool isActive() const = 0;
  virtual ~Actuator() {}
};

/**
 * \brief An actuator that simply remembers the set value.
 *
 * This is primary used for testing.
 */
class ValueActuator : public Actuator {
public:
  ValueActuator() : state(false) {}
  ValueActuator(const bool initial) : state(initial) {}

  virtual void setActive(const bool active) { state = active; }
  virtual bool isActive() const { return state; }

private:
  bool state;
};
