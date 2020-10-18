#pragma once

#ifdef ARDUINO
#include "DallasTemperature.h"	// for DeviceAddress
#endif

#include <ArduinoJson.h>


/*
 * \addtogroup tempcontrol
 * @{
 */


/**
 * \brief Data that can be persisted as JSON
 */
class JSONSaveable {
protected:
    static void writeJsonToFile(const char *filename, const JsonDocument& json_doc);
    static DynamicJsonDocument readJsonFromFile(const char*filename);

};


/**
 * \brief PID Control constants
 */
class ControlConstants : public JSONSaveable {
public:
    ControlConstants();

    temperature tempSettingMin; //<! Minimum valid control temperature
    temperature tempSettingMax; //<! Maximum valid control temperature
    temperature Kp;
    temperature Ki;
    temperature Kd;
    temperature iMaxError;
    temperature idleRangeHigh;
    temperature idleRangeLow;
    temperature heatingTargetUpper;
    temperature heatingTargetLower;
    temperature coolingTargetUpper;
    temperature coolingTargetLower;
    uint16_t maxHeatTimeForEstimate; //!< max time for heat estimate in seconds
    uint16_t maxCoolTimeForEstimate; //!< max time for heat estimate in seconds
    // for the filter coefficients the b value is stored. a is calculated from b.
    uint8_t fridgeFastFilter;	//!< for display, logging and on-off control
    uint8_t fridgeSlowFilter;	//!< for peak detection
    uint8_t fridgeSlopeFilter;	//!< not used in current control algorithm
    uint8_t beerFastFilter;	//!< for display and logging
    uint8_t beerSlowFilter;	//!< for on/off control algorithm
    uint8_t beerSlopeFilter;	//!< for PID calculation
    uint8_t lightAsHeater;		//!< Use the light to heat rather than the configured heater device
    uint8_t rotaryHalfSteps; //!< Define whether to use full or half steps for the rotary encoder
    temperature pidMax;
    char tempFormat; //!< Temperature format (F/C)

    DynamicJsonDocument toJson();
    void storeToSpiffs();
    void loadFromSpiffs();
    void setDefaults();

    /**
     * \brief Filename used when reading/writing data to flash
     */
    static constexpr auto filename = "/controlConstants.json";
private:
};

/** @} */


/**
 * \enum ControlMode
 * \brief Modes of operation
 * \ingroup tempcontrol
 */
enum class ControlMode : char {
  fridgeConstant = 'f', //!< Hold fridge at temperature
  fridgeProfile = 'F', //!< Hold fridge at temperature according to profile
  beerConstant = 'b', //!< Hold beer at temperature
  beerProfile = 'p', //!< Hold beer at temperature according to profile
  off = 'o', //!< Disable temp control
  test = 't', //!< Enable test mode
};


struct ControlSettings : public JSONSaveable {
public:
    ControlSettings();

    temperature beerSetting;
    temperature fridgeSetting;
    temperature heatEstimator; // updated automatically by self learning algorithm
    temperature coolEstimator; // updated automatically by self learning algorithm
    ControlMode mode;

    DynamicJsonDocument toJson();
    void storeToSpiffs();
    void loadFromSpiffs();
    void setDefaults();

    /**
     * \brief Filename used when reading/writing data to flash
     */
    static constexpr auto filename = "/controlSettings.json";
};

/*
 * \addtogroup hardware
 * @{
 */



/**
 * \brief Describes the logical function of a device.
 */
enum class DeviceFunction : int8_t {
	none = 0, //!< Used as a sentry to mark end of list
	// chamber devices
	chamberDoor = 1,	//!< Chamber door switch sensor
	chamberHeat = 2,  //!< Chamber heater actuator
	chamberCool = 3,  //!< Chamber cooler actuator
	chamberLight = 4,	//!< Chamber light actuator
	chamberTemp = 5,  //!< Chamber temp sensor
	chamberRoomTemp = 6,	//!< Ambient room temp sensor
	chamberFan = 7,			//!< A fan in the chamber
	chamberReserved1 = 8,	//!< Reserved for future use

	// carboy devices
	beerFirst = 9,                //!< First beer temp sensor
	beerTemp = DeviceFunction::beerFirst,	//!< Primary beer temp sensor
	beerTemp2 = 10,								//!< Secondary beer temp sensor
	beerHeat = 11,                //!< Individual beer heater actuator
  beerCool = 12,				        //!< Individual beer cooler actuator
	beerSG = 13,									//!< Beer SG sensor
	beerReserved1 = 14, //!< Reserved for future use
  beerReserved2 = 15,	//!< Reserved for future use

	max = 16
};



/**
 * \brief The concrete type of the device.
 */
enum DeviceHardware {
	DEVICE_HARDWARE_NONE = 0,
	DEVICE_HARDWARE_PIN = 1, //!< A digital pin, either input or output
	DEVICE_HARDWARE_ONEWIRE_TEMP = 2,	//<! A onewire temperature sensor
#if BREWPI_DS2413
	DEVICE_HARDWARE_ONEWIRE_2413 = 3	//<! A onewire 2-channel PIO input or output.
#endif
};


/**
 * A union of all device types.
 */
struct DeviceConfig {
	uint8_t chamber;		//!< Chamber assignment. 0 means no chamber. 1 is the first chamber.
	uint8_t beer;				//!< Beer assignment.  0 means no beer, 1 is the first beer

	DeviceFunction deviceFunction;				// The function of the device to configure
	DeviceHardware deviceHardware;				// flag to indicate the runtime type of device
	struct Hardware {
		uint8_t pinNr;							// the arduino pin nr this device is connected to
		bool invert;							// for actuators/sensors, controls if the signal value is inverted.
		bool deactivate;							// disable this device - the device will not be installed.
		DeviceAddress address;					// for onewire devices, if address[0]==0 then use the first matching device type, otherwise use the device with the specific address

												/* The pio and sensor calibration are never needed at the same time so they are a union.
												* To ensure the eeprom format is stable when including/excluding DS2413 support, ensure all fields are the same size.
												*/
		union {
#if BREWPI_DS2413
			uint8_t pio;						// for ds2413 (deviceHardware==3) : the pio number (0,1)
#endif			
			int8_t /* fixed4_4 */ calibration;	// for temp sensors (deviceHardware==2), calibration adjustment to add to sensor readings
												// this is intentionally chosen to match the raw value precision returned by the ds18b20 sensors
		};
		bool reserved;								// extra space so that additional fields can be added without breaking layout
	} hw;
	bool reserved2;
};
/** @} */
