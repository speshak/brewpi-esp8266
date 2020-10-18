/*
 * File:   ArduinoActuator.h
 * Author: mat
 *
 * Created on 19 August 2013, 20:32
 */

#pragma once

#include "Actuator.h"

template <uint8_t pin, bool invert> class DigitalConstantPinActuator : public Actuator {
private:
  bool active;

public:
  DigitalConstantPinActuator() : active(false) {
    setActive(false);
    fastPinMode(pin, OUTPUT);
  }

  inline virtual void setActive(const bool active) {
    this->active = active;
    fastDigitalWrite(pin, active ^ invert ? HIGH : LOW);
  }

  bool isActive() const { return active; }
};

/**
 * \brief Actuator for a digitial pin output
 */
class DigitalPinActuator : public Actuator {
private:
  /**
   * \brief Flag to indicate that the control signal should be inverted.
   *
   * If true, when the Actuator is active, the output pin will be brought low.
   */
  bool invert;

  /**
   * \brief Arduino pin number to control.
   */
  uint8_t pin;

  /**
   * \brief Actuator state.
   */
  bool active;

public:
  /**
   * \brief Consructor
   *
   * \param pin - Pin physical pin that is being connected
   * \param invert - If the control signal should be inverted (eg: Off is high)
   */
  DigitalPinActuator(const uint8_t pin, const bool invert) {
    this->invert = invert;
    this->pin = pin;
    setActive(false);
    pinMode(pin, OUTPUT);
  }

  inline virtual void setActive(const bool active) {
    this->active = active;
    digitalWrite(pin, active ^ invert ? HIGH : LOW);
  }

  bool isActive() const { return active; }
};
