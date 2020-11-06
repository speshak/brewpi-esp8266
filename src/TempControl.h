/*
 * Copyright 2012-2013 BrewPi/Elco Jacobs.
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
#include "Pins.h"
#include "TemperatureFormats.h"
#include "Actuator.h"
#include "Sensor.h"
#include "EepromManager.h"
#include "ActuatorAutoOff.h"
#include "EepromStructs.h"
#include <ArduinoJson.h>


/**
 * \defgroup tempcontrol Temperature PID Control
 * \brief Software implementation of a PID controller.
 *
 * \addtogroup tempcontrol
 * @{
 */

//! Minimum cooler off time, in seconds. To prevent short cycling the compressor
const uint16_t MIN_COOL_OFF_TIME = 300;
//! Minimum heater off time, in seconds. To heat in cycles, not lots of short bursts
const uint16_t MIN_HEAT_OFF_TIME = 300;
//! Minimum on time for the cooler.
const uint16_t MIN_COOL_ON_TIME = 180;
//! Minimum on time for the heater.
const uint16_t MIN_HEAT_ON_TIME = 180;

/**
 * \brief Minimum cooler off time, in seconds.
 *
 * Used when the controller is in Fridge Constant mode.  Larger than
 * MIN_COOL_OFF_TIME. No need for very fast cycling.
 */
const uint16_t MIN_COOL_OFF_TIME_FRIDGE_CONSTANT = 600;
//! Minimum off time between switching between heating and cooling
const uint16_t MIN_SWITCH_TIME = 600;
//! Time allowed for cooling peak detection
const uint16_t COOL_PEAK_DETECT_TIME = 1800;
//! Time allowed for heating peak detection
const uint16_t HEAT_PEAK_DETECT_TIME = 900;

/**
 * \brief Variables used for temp control
 *
 * These values are stored in & loaded from EEPROM.
 *
 * @see ControlSettings
 * @see ControlConstants
 */
struct ControlVariables {
	temperature beerDiff;
	long_temperature diffIntegral; // also uses 9 fraction bits, but more integer bits to prevent overflow
	temperature beerSlope;
	temperature p;
	temperature i;
	temperature d;
	temperature estimatedPeak;
	temperature negPeakEstimate; //!< Last estimate
	temperature posPeakEstimate;
	temperature negPeak; //!< Last detected peak
	temperature posPeak;
};


/**
 * \enum ControlState
 * \brief Temperature control states
 * \ingroup tempcontrol
 */
enum class ControlState : uint8_t {
	IDLE,           //!< Neither heating, nor cooling
	OFF,            //!< Disabled
	DOOR_OPEN,			//!< Fridge door open. Used by the Display only
	HEATING,				//!< Calling for heat
	COOLING,				//!< Calling for cool
	WAITING_TO_COOL,//!< Waiting to cool. (Compressor delay)
	WAITING_TO_HEAT,//!< Waiting to heat. (Compressor delay)
	WAITING_FOR_PEAK_DETECT,	//!< Waiting for peak detection
	COOLING_MIN_TIME,			//!< Waiting for the minimum cooling time to elapse, before returning to Idle
	HEATING_MIN_TIME,			//!< Waiting for the minimum heating time to elapse, before returning to Idle
  UNKNOWN  = UINT8_MAX, //!< An unknown state.
};


#if TEMP_CONTROL_STATIC
#define TEMP_CONTROL_METHOD static
#define TEMP_CONTROL_FIELD static
#else
#define TEMP_CONTROL_METHOD
#define TEMP_CONTROL_FIELD
#endif
/**
 * \def TEMP_CONTROL_METHOD
 * \brief Compile-time control of making TempControl methods static
 * \see TEMP_CONTROL_STATIC
 */

/**
 * \def TEMP_CONTROL_FIELD
 * \brief Compile-time control of making TempControl fields static
 * \see TEMP_CONTROL_STATIC
 */

/**
 * \def TEMP_CONTROL_STATIC
 * \brief Compile-time control of making TempControl static
 *
 * To support multi-chamber, I could have made TempControl non-static, and had
 * a reference to the current instance. But this means each lookup of a field
 * must be done indirectly, which adds to the code size.  Instead, we swap
 * in/out the sensors and control data so that the bulk of the code can work
 * against compile-time resolvable memory references. While the design goes
 * against the grain of typical OO practices, the reduction in code size make
 * it worth it.
 */


/**
 * \brief Temperature control PID implementation
 *
 * This is the heart of the brewpi system.  It handles turning on and off heat
 * & cool to track a target temperature.
 *
 * Temp Control tracking can be done using several different modes
 *
 * - Beer: Heat & Cool are applied to keep a probe in the fermenting beer at a target.
 * - Fridge: Heat & Cool are applied to keep a probe in the chamber surrounding the beer at a target.
 */
class TempControl{
public:

	TempControl(){};
	~TempControl(){};

	TEMP_CONTROL_METHOD void init();
	TEMP_CONTROL_METHOD void reset();

	TEMP_CONTROL_METHOD void updateTemperatures();
	TEMP_CONTROL_METHOD void updatePID();
	TEMP_CONTROL_METHOD void updateState();
	TEMP_CONTROL_METHOD void updateOutputs();
	TEMP_CONTROL_METHOD void detectPeaks();

	TEMP_CONTROL_METHOD void loadSettings();
	TEMP_CONTROL_METHOD void storeSettings();
	TEMP_CONTROL_METHOD void loadDefaultSettings();

	TEMP_CONTROL_METHOD void loadConstants();
	TEMP_CONTROL_METHOD void storeConstants();
	TEMP_CONTROL_METHOD void loadDefaultConstants();

	TEMP_CONTROL_METHOD ticks_seconds_t timeSinceCooling();
 	TEMP_CONTROL_METHOD ticks_seconds_t timeSinceHeating();
 	TEMP_CONTROL_METHOD ticks_seconds_t timeSinceIdle();

	TEMP_CONTROL_METHOD temperature getBeerTemp();
	TEMP_CONTROL_METHOD temperature getBeerSetting();
	TEMP_CONTROL_METHOD void setBeerTemp(const temperature newTemp);

	TEMP_CONTROL_METHOD temperature getFridgeTemp();
	TEMP_CONTROL_METHOD temperature getFridgeSetting();
	TEMP_CONTROL_METHOD void setFridgeTemp(const temperature newTemp);

  /**
   * \brief Get the current temperature of the room probe.
   */
	TEMP_CONTROL_METHOD temperature getRoomTemp() {
		return ambientSensor->read();
	}

	TEMP_CONTROL_METHOD void setMode(const ControlMode newMode, bool force=false);

  /**
   * \brief Get current temp control mode
   */
	TEMP_CONTROL_METHOD ControlMode getMode() {
		return cs.mode;
	}

  /**
   * \brief Get the current state of the control system.
   */
	TEMP_CONTROL_METHOD ControlState getState(){
		return state;
	}

  /**
   * \brief Get the current value of the elapsed wait time counter.
   */
	TEMP_CONTROL_METHOD ticks_seconds_t getWaitTime(){
		return waitTime;
	}

  /**
   * \brief Reset the elapsed wait time counter back to 0.
   */
	TEMP_CONTROL_METHOD void resetWaitTime(){
		waitTime = 0;
	}

	TEMP_CONTROL_METHOD void updateWaitTime(const ticks_seconds_t newTimeLimit, const ticks_seconds_t newTimeSince);


	TEMP_CONTROL_METHOD bool stateIsCooling();
	TEMP_CONTROL_METHOD bool stateIsHeating();

	TEMP_CONTROL_METHOD bool modeIsBeer();

	TEMP_CONTROL_METHOD void initFilters();

  /**
   * \brief Check if the door is currently open
   */
	TEMP_CONTROL_METHOD bool isDoorOpen() { return doorOpen; }

	TEMP_CONTROL_METHOD ControlState getDisplayState();

  TEMP_CONTROL_METHOD void getControlVariablesDoc(JsonDocument& doc);
  TEMP_CONTROL_METHOD void getControlConstantsDoc(JsonDocument& doc);
  TEMP_CONTROL_METHOD void getControlSettingsDoc(JsonDocument& doc);

private:
	TEMP_CONTROL_METHOD void increaseEstimator(temperature * estimator, const temperature error);
	TEMP_CONTROL_METHOD void decreaseEstimator(temperature * estimator, const temperature error);
	TEMP_CONTROL_METHOD void updateEstimatedPeak(const ticks_seconds_t estimate, temperature estimator, const ticks_seconds_t sinceIdle);

  TEMP_CONTROL_METHOD bool sensorsAreValid();
  TEMP_CONTROL_METHOD void alertDoorStateChange();
  TEMP_CONTROL_METHOD void updateStateWhileCooling();
  TEMP_CONTROL_METHOD void updateStateWhileHeating();
  TEMP_CONTROL_METHOD void updateStateWhileIdle();


public:
	TEMP_CONTROL_FIELD TempSensor* beerSensor; //!< Temp sensor monitoring beer
	TEMP_CONTROL_FIELD TempSensor* fridgeSensor; //!< Temp sensor monitoring fridge
	TEMP_CONTROL_FIELD BasicTempSensor* ambientSensor; //!< Ambient room temp sensor
	TEMP_CONTROL_FIELD Actuator* heater; //!< Actuator used to call for heat
	TEMP_CONTROL_FIELD Actuator* cooler; //!< Actuator used to call for cool
	TEMP_CONTROL_FIELD Actuator* light; //!< Actuator to control chamber light
	TEMP_CONTROL_FIELD Actuator* fan; //!< Actuator to control chamber fan
	TEMP_CONTROL_FIELD AutoOffActuator cameraLight;
	TEMP_CONTROL_FIELD Sensor<bool>* door; //!< Chamber door sensor

	// Control parameters
	TEMP_CONTROL_FIELD ControlConstants cc; //!< PID control constants
	TEMP_CONTROL_FIELD ControlSettings cs; //!< Control settings
	TEMP_CONTROL_FIELD ControlVariables cv; //!< PID control variables

	/**
   * \brief Defaults for control constants.
   * Defined in cpp file, copied with memcpy_p
   */
	static const ControlConstants ccDefaults;

private:
	TEMP_CONTROL_FIELD temperature storedBeerSetting; //!< Beer setting stored in EEPROM
	TEMP_CONTROL_FIELD temperature storedFridgeSetting; //!< Fridge setting stored in EEPROM

	// Timers
	TEMP_CONTROL_FIELD ticks_seconds_t lastIdleTime; //!< Last time the controller was idle
	TEMP_CONTROL_FIELD ticks_seconds_t lastHeatTime; //!< Last time that the controller was heating
	TEMP_CONTROL_FIELD ticks_seconds_t lastCoolTime; //!< Last time that the controller was cooling
	TEMP_CONTROL_FIELD ticks_seconds_t waitTime; //!< Amount of time to continue waiting, when in a wait state


	// State variables
	TEMP_CONTROL_FIELD ControlState state; //!< Current controller state
	TEMP_CONTROL_FIELD bool doPosPeakDetect; //!< True if the controller is doing positive peak detection
	TEMP_CONTROL_FIELD bool doNegPeakDetect; //!< True if the controller is doing negative peak detection
	TEMP_CONTROL_FIELD bool doorOpen; //!< True if the chamber door is open
};

extern TempControl tempControl;

/** @} */
