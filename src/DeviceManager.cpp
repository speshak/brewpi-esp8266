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

#include "Brewpi.h"
#include "BrewpiStrings.h"
#include "DeviceManager.h"
#include "TempControl.h"
#include "Actuator.h"
#include "Sensor.h"
#include "TempSensorDisconnected.h"
#include "TempSensorExternal.h"
#include "PiLink.h"
#include "EepromFormat.h"
#include "DeviceNameManager.h"
#include <ArduinoJson.h>
#include "JsonKeys.h"
#include "NumberFormats.h"
#include <type_traits>
#include "SmartAssignment.h"
#include "EnumHelpers.h"


#ifdef ARDUINO
#include "OneWireTempSensor.h"

#include "OneWireActuator.h"
#include "DS2413.h"
#include <OneWire.h>
#include "DallasTemperature.h"
#include "ActuatorArduinoPin.h"
#include "SensorArduinoPin.h"
#endif

constexpr auto calibrationOffsetPrecision = 4;

/*
 * Defaults for sensors, actuators and temperature sensors when not defined in the eeprom.
 */

ValueSensor<bool> defaultSensor(false);			// off
ValueActuator defaultActuator;
DisconnectedTempSensor defaultTempSensor;

#if !BREWPI_SIMULATE
#ifdef oneWirePin
OneWire DeviceManager::primaryOneWireBus(oneWirePin);
#else
OneWire DeviceManager::beerSensorBus(beerSensorPin);
OneWire DeviceManager::fridgeSensorBus(fridgeSensorPin);
#endif
#endif

/**
 * \brief Memory required for a DeviceDefinition JSON document
 */
constexpr auto deviceDefinitionJsonSize = 256;

OneWire* DeviceManager::oneWireBus(const uint8_t pin) {
#if !BREWPI_SIMULATE
#ifdef oneWirePin
	if (pin == oneWirePin)
		return &primaryOneWireBus;
#else
	if (pin==beerSensorPin)
		return &beerSensorBus;
	if (pin==fridgeSensorPin)
		return &fridgeSensorBus;
#endif
#endif
	return nullptr;
}


/**
 * Check if a given BasicTempSensor is the default temp sensor
 *
 * @param sensor - Pointer to the sensor to check
 * @returns true if the provided sensor is the default, otherwise false.
 */
bool DeviceManager::isDefaultTempSensor(BasicTempSensor* sensor) {
	return sensor==&defaultTempSensor;
}

/**
 * Sets devices to their unconfigured states. Each device is initialized to a static no-op instance.
 * This method is idempotent, and is called each time the eeprom is reset.
 */
void DeviceManager::setupUnconfiguredDevices()
{
	// right now, uninstall doesn't care about chamber/beer distinction.
	// but this will need to match beer/function when multiferment is available
	DeviceConfig cfg;
	cfg.chamber = 1;
  cfg.beer = 1;

	for (uint8_t i=0; i< EnumHelpers::underlyingEnumValue(DeviceFunction::max); i++) {
		cfg.deviceFunction = DeviceFunction(i);
		uninstallDevice(cfg);
	}
}


/**
 * Creates a new device for the given config.
 */
void* DeviceManager::createDevice(DeviceConfig& config, DeviceType dt)
{
	switch (config.deviceHardware) {
		case DEVICE_HARDWARE_NONE:
			break;
		case DEVICE_HARDWARE_PIN:
			if (dt==DEVICETYPE_SWITCH_SENSOR)
			#if BREWPI_SIMULATE
				return new ValueSensor<bool>(false);
			#else
				return new DigitalPinSensor(config.hw.pinNr, config.hw.invert);
			#endif
			else
#if BREWPI_SIMULATE
				return new ValueActuator();
#else
				// use hardware actuators even for simulator
				return new DigitalPinActuator(config.hw.pinNr, config.hw.invert);
#endif
		case DEVICE_HARDWARE_ONEWIRE_TEMP:
		#if BREWPI_SIMULATE
			return new ExternalTempSensor(false);// initially disconnected, so init doesn't populate the filters with the default value of 0.0
		#else
			return new OneWireTempSensor(oneWireBus(config.hw.pinNr), config.hw.address, config.hw.calibration);
		#endif

#if BREWPI_DS2413
		case DEVICE_HARDWARE_ONEWIRE_2413:
		#if BREWPI_SIMULATE
		if (dt==DEVICETYPE_SWITCH_SENSOR)
			return new ValueSensor<bool>(false);
		else
			return new ValueActuator();
		#else
			return new OneWireActuator(oneWireBus(config.hw.pinNr), config.hw.address, config.hw.pio, config.hw.invert);
		#endif
#endif
	}
	return nullptr;
}

/**
 * Returns the pointer to where the device pointer resides.
 *
 * This can be used to delete the current device and install a new one.  For
 * Temperature sensors, the returned pointer points to a TempSensor*. The basic
 * device can be fetched by calling TempSensor::getSensor().
 *
 * @param config
 */
inline void** deviceTarget(DeviceConfig& config)
{
	// for multichamber, will write directly to the multi-chamber managed storage.
	// later...
	if (config.chamber>1 || config.beer>1)
		return nullptr;

	void** ppv;
	switch (config.deviceFunction) {
    case DeviceFunction::chamberRoomTemp:
      ppv = (void**)&tempControl.ambientSensor;
      break;
    case DeviceFunction::chamberDoor:
      ppv = (void**)&tempControl.door;
      break;
    case DeviceFunction::chamberLight:
      ppv = (void**)&tempControl.light;
      break;
    case DeviceFunction::chamberHeat:
      ppv = (void**)&tempControl.heater;
      break;
    case DeviceFunction::chamberCool:
      ppv = (void**)&tempControl.cooler;
      break;
    case DeviceFunction::chamberTemp:
      ppv = (void**)&tempControl.fridgeSensor;
      break;
    case DeviceFunction::chamberFan:
      ppv = (void**)&tempControl.fan;
      break;

    case DeviceFunction::beerTemp:
      ppv = (void**)&tempControl.beerSensor;
      break;
    default:
      ppv = nullptr;
	}
	return ppv;
}

// A pointer to a "temp sensor" may be a TempSensor* or a BasicTempSensor* .
// These functions allow uniform treatment.
inline bool isBasicSensor(const DeviceFunction function) {
	// currently only ambient sensor is basic. The others are wrapped in a TempSensor.
	return function == DeviceFunction::chamberRoomTemp;
}

inline BasicTempSensor& unwrapSensor(DeviceFunction f, void* pv) {
	return isBasicSensor(f) ? *(BasicTempSensor*)pv : ((TempSensor*)pv)->sensor();
}

inline void setSensor(DeviceFunction f, void** ppv, BasicTempSensor* sensor) {
	if (isBasicSensor(f))
		*ppv = sensor;
	else
		((TempSensor*)*ppv)->setSensor(sensor);
}


/**
 * Removes an installed device.
 *
 * @param config - The device to remove. The fields that are used are
 *		chamber, beer, hardware and function.
 */
void DeviceManager::uninstallDevice(DeviceConfig& config)
{
	DeviceType dt = deviceType(config.deviceFunction);
	void** ppv = deviceTarget(config);
	if (ppv==nullptr)
		return;

	BasicTempSensor* s;
	switch(dt) {
		case DEVICETYPE_NONE:
			break;
		case DEVICETYPE_TEMP_SENSOR:
			// sensor may be wrapped in a TempSensor class, or may stand alone.
			s = &unwrapSensor(config.deviceFunction, *ppv);
			if (s!=&defaultTempSensor) {
				setSensor(config.deviceFunction, ppv, &defaultTempSensor);
//				DEBUG_ONLY(logInfoInt(INFO_UNINSTALL_TEMP_SENSOR, config.deviceFunction));
				delete s;
			}
			break;
		case DEVICETYPE_SWITCH_ACTUATOR:
			if (*ppv!=&defaultActuator) {
//				DEBUG_ONLY(logInfoInt(INFO_UNINSTALL_ACTUATOR, config.deviceFunction));
				delete (Actuator*)*ppv;
				*ppv = &defaultActuator;
			}
			break;
		case DEVICETYPE_SWITCH_SENSOR:
			if (*ppv!=&defaultSensor) {
//				DEBUG_ONLY(logInfoInt(INFO_UNINSTALL_SWITCH_SENSOR, config.deviceFunction));
				delete (SwitchSensor*)*ppv;
				*ppv = &defaultSensor;
			}
			break;
	}
}


/**
 * Creates and installs a device in the current chamber.
 *
 * @param config
 * @return true if a device was installed. false if the config is not complete.
 */
void DeviceManager::installDevice(DeviceConfig& config)
{
	DeviceType dt = deviceType(config.deviceFunction);
	void** ppv = deviceTarget(config);
	if (ppv==nullptr || config.hw.deactivate)
		return;

	BasicTempSensor* s;
	TempSensor* ts;
	switch(dt) {
		case DEVICETYPE_NONE:
			break;
		case DEVICETYPE_TEMP_SENSOR:
			DEBUG_ONLY(logInfoInt(INFO_INSTALL_TEMP_SENSOR, config.deviceFunction));
			// sensor may be wrapped in a TempSensor class, or may stand alone.
			s = (BasicTempSensor*)createDevice(config, dt);
			if (*ppv==nullptr){
				logErrorInt(ERROR_OUT_OF_MEMORY_FOR_DEVICE, (int)config.deviceFunction);
			}
			if (isBasicSensor(config.deviceFunction)) {
				s->init();
				*ppv = s;
			}
			else {
				ts = ((TempSensor*)*ppv);
				ts->setSensor(s);
				ts->init();
			}
#if BREWPI_SIMULATE
			((ExternalTempSensor*)s)->setConnected(true);	// now connect the sensor after init is called
#endif
			break;
		case DEVICETYPE_SWITCH_ACTUATOR:
		case DEVICETYPE_SWITCH_SENSOR:
			DEBUG_ONLY(logInfoInt(INFO_INSTALL_DEVICE, (int)config.deviceFunction));
			*ppv = createDevice(config, dt);
#if (BREWPI_DEBUG > 0)
			if (*ppv==nullptr)
				logErrorInt(ERROR_OUT_OF_MEMORY_FOR_DEVICE, (int)config.deviceFunction);
#endif
			break;
	}
}



/**
 * \brief Read incoming JSON and populate a DeviceDefinition
 *
 * \param dev - DeviceDefinition to populate
 */
void DeviceManager::readJsonIntoDeviceDef(DeviceDefinition& dev) {
  StaticJsonDocument<deviceDefinitionJsonSize> doc;
  piLink.receiveJsonMessage(doc);

  const char* address = doc[DeviceDefinitionKeys::address];
  if(address)
    parseBytes(dev.address, address, 8);

  JsonVariant calibration = doc[DeviceDefinitionKeys::calibrateadjust];
  if(calibration) {
    dev.calibrationAdjust = fixed4_4(stringToTempDiff(calibration.as<char *>()) >> (TEMP_FIXED_POINT_BITS - calibrationOffsetPrecision));
  }

  JsonVariant id = doc[DeviceDefinitionKeys::index];
  if(!id.isNull())
    dev.id = id.as<uint8_t>();

  JsonVariant chamber = doc[DeviceDefinitionKeys::chamber];
  if(!chamber.isNull())
    dev.chamber = chamber.as<uint8_t>();

  JsonVariant beer = doc[DeviceDefinitionKeys::beer];
  if(!beer.isNull())
    dev.beer = beer.as<uint8_t>();

  JsonVariant function = doc[DeviceDefinitionKeys::function];
  if(!function.isNull())
    dev.deviceFunction = EnumHelpers::readEnumValue<DeviceFunction>(function);

  JsonVariant hardware = doc[DeviceDefinitionKeys::hardware];
  if(!hardware.isNull())
    dev.deviceHardware = EnumHelpers::readEnumValue<DeviceHardware>(hardware);

  JsonVariant pin = doc[DeviceDefinitionKeys::pin];
  if(!pin.isNull())
    dev.pinNr = pin.as<uint8_t>();

  JsonVariant invert = doc[DeviceDefinitionKeys::invert];
  if(!invert.isNull())
    dev.invert = pin.as<uint8_t>();
}

/**
 * \brief Check if value is within a range
 * \param val - Value to check
 * \param min - Lower bound
 * \param max - Upper bound
 */
bool inRangeUInt8(const uint8_t val, const uint8_t min, const int8_t max) {
	return min<=val && val<=max;
}

/**
 * \brief Check if value is within a range
 * \param val - Value to check
 * \param min - Lower bound
 * \param max - Upper bound
 */
bool inRangeInt8(const int8_t val, const int8_t min, const int8_t max) {
	return min<=val && val<=max;
}



/**
 * \brief Safely updates the device definition.
 *
 * Only changes that result in a valid device, with no conflicts with other
 * devices are allowed.
 */
void DeviceManager::parseDeviceDefinition()
{
	static DeviceDefinition dev;
	fill((int8_t*)&dev, sizeof(dev));

  readJsonIntoDeviceDef(dev);

	if (!inRangeInt8(dev.id, 0, MAX_DEVICE_SLOT))	{
    // no device id given, or it's out of range, can't do anything else.
    piLink.print_fmt("Out of range: %d", dev.id);
    piLink.printNewLine();
		return;
  }

  if(Config::forceDeviceDefaults) {
    // Overwrite the chamber/beer number to prevent user error.
    dev.chamber = 1;

    // Check if device function is beer specific
    if (dev.deviceFunction >= DeviceFunction::beerFirst && dev.deviceFunction < DeviceFunction::max)
      dev.beer = 1;
    else
      dev.beer = 0;
  }

	// save the original device so we can revert
	DeviceConfig target;
	DeviceConfig original;

	// todo - should ideally check if the eeprom is correctly initialized.
	eepromManager.fetchDevice(original, dev.id);
	memcpy(&target, &original, sizeof(target));


	assignIfSet(dev.chamber, &target.chamber);
	assignIfSet(dev.beer, &target.beer);
	assignIfSet(dev.deviceFunction, &target.deviceFunction);
	assignIfSet(dev.deviceHardware, &target.deviceHardware);
	assignIfSet(dev.pinNr, &target.hw.pinNr);


#if BREWPI_DS2413
	assignIfSet(dev.pio, &target.hw.pio);
#endif

	if (dev.calibrationAdjust!=-1)		// since this is a union, it also handles pio for 2413 sensors
		target.hw.calibration = dev.calibrationAdjust;

	assignIfSet(dev.invert, &target.hw.invert);

	if (dev.address[0] != 0xFF) {// first byte is family identifier. I don't have a complete list, but so far 0xFF is not used.
		memcpy(target.hw.address, dev.address, 8);
	}
	assignIfSet(dev.deactivate, &target.hw.deactivate);

	// setting function to none clears all other fields.
	if (target.deviceFunction == DeviceFunction::none) {
		piLink.print("Function set to NONE\r\n");
		clear((uint8_t*)&target, sizeof(target));
	}

	const bool valid = isDeviceValid(target, original, dev.id);
	DeviceConfig* print = &original;
	if (valid) {
		print = &target;
		// remove the device associated with the previous function
		uninstallDevice(original);
		// also remove any existing device for the new function, since install overwrites any existing definition.
		uninstallDevice(target);
		installDevice(target);
		eepromManager.storeDevice(target, dev.id);
	}
	else {
		logError(ERROR_DEVICE_DEFINITION_UPDATE_SPEC_INVALID);
	}

  StaticJsonDocument<deviceDefinitionJsonSize> doc;
  serializeJsonDevice(doc, dev.id, *print, nullptr);
  piLink.sendSingleItemJsonMessage('U', doc);
}

/**
 * Determines if a given device definition is valid.
 *
 * Validity is defiend by:
 * - Chamber & beer must be within bounds
 * - Device function must match the chamber/beer spec, and must not already be defined for the same chamber/beer combination - Not Implemented
 * - Device hardware type must be applicable with the device function
 * - pinNr must be unique for digital pin devices - Not Implemented
 * - pinNr must be a valid OneWire bus for one wire devices.
 * - For OneWire temp devices, address must be unique. - Not Implemented
 * - For OneWire DS2413 devices, address+pio must be unique. - Not Implemented
 */
bool DeviceManager::isDeviceValid(DeviceConfig& config, DeviceConfig& original, int8_t deviceIndex)
{
	/* Implemented checks to ensure the system will not crash when supplied with invalid data.
	   More refined checks that may cause confusing results are not yet implemented. See todo below. */

	/* chamber and beer within range.*/
	if (!inRangeUInt8(config.chamber, 0, EepromFormat::MAX_CHAMBERS))
	{
		logErrorInt(ERROR_INVALID_CHAMBER, config.chamber);
		return false;
	}

	/* 0 is allowed - represents a chamber device not assigned to a specific beer */
	if (!inRangeUInt8(config.beer, 0, ChamberBlock::MAX_BEERS))
	{
		logErrorInt(ERROR_INVALID_BEER, config.beer);
		return false;
	}

	if (!inRangeUInt8((int)config.deviceFunction, 0, (int)DeviceFunction::max-1))
	{
		logErrorInt(ERROR_INVALID_DEVICE_FUNCTION, (int)config.deviceFunction);
		return false;
	}

	DeviceOwner owner = deviceOwner(config.deviceFunction);
	if (!((owner==DeviceOwner::beer && config.beer) || (owner==DeviceOwner::chamber && config.chamber)
		|| (owner==DeviceOwner::none && !config.beer && !config.chamber)))
	{
		logErrorIntIntInt(ERROR_INVALID_DEVICE_CONFIG_OWNER, (int)owner, config.beer, config.chamber);
		return false;
	}

	// todo - find device at another index with the same chamber/beer/function spec.
	// with duplicate function defined for the same beer, that they will both try to create/delete the device in the target location.
	// The highest id will win.
	DeviceType dt = deviceType(config.deviceFunction);
	if (!isAssignable(dt, config.deviceHardware)) {
		logErrorIntInt(ERROR_CANNOT_ASSIGN_TO_HARDWARE, dt, config.deviceHardware);
		return false;
	}

	// todo - check pinNr uniqueness for direct digital I/O devices?

	/* pinNr for a onewire device must be a valid bus. While this won't cause a crash, it's a good idea to validate this. */
	if (isOneWire(config.deviceHardware)) {
		if (!oneWireBus(config.hw.pinNr)) {
			logErrorInt(ERROR_NOT_ONEWIRE_BUS, config.hw.pinNr);
			return false;
		}
	}
	else {		// regular pin device
		// todo - could verify that the pin nr corresponds to enumActuatorPins/enumSensorPins
	}

	// todo - for onewire temp, ensure address is unique
	// todo - for onewire 2413 check address+pio nr is unique
	return true;
}

void printAttrib(Print& p, char c, int8_t val, bool first=false)
{
	if (!first)
        	p.print(',');

	char tempString[32]; // resulting string limited to 128 chars
	sprintf_P(tempString, PSTR("\"%c\":%d"), c, val);
	p.print(tempString);
}

// I really want to buffer stuff rather than printing directly to the serial stream
void appendAttrib(String& str, char c, int8_t val, bool first = false)
{
	if (!first)
		str += ",";

	char tempString[32]; // resulting string limited to 128 chars
	sprintf_P(tempString, PSTR("\"%c\":%d"), c, val);
	str += tempString;
}


/**
 * Check if DeviceHardware definition is for a device which is "invertable"
 *
 * @param hw - DeviceHardware definition
 */
inline bool hasInvert(DeviceHardware hw)
{
	return hw==DEVICE_HARDWARE_PIN
#if BREWPI_DS2413
	|| hw==DEVICE_HARDWARE_ONEWIRE_2413
#endif
	;
}


/**
 * \brief Check if DeviceHardware definition is for a OneWire device
 *
 * \param hw - DeviceHardware definition
 */
inline bool hasOnewire(DeviceHardware hw)
{
	return
#if BREWPI_DS2413
	hw==DEVICE_HARDWARE_ONEWIRE_2413 ||
#endif
	hw==DEVICE_HARDWARE_ONEWIRE_TEMP;
}


/**
 * \brief Add device information to a JsonDocument
 *
 * Used for outputting device information
 */
void DeviceManager::serializeJsonDevice(JsonDocument& doc, device_slot_t slot, DeviceConfig& config, const char* value) {
  JsonObject deviceObj = doc.createNestedObject();

  deviceObj[DeviceDefinitionKeys::index] = slot;

	DeviceType dt = deviceType(config.deviceFunction);
  deviceObj[DeviceDefinitionKeys::type] = dt;

  deviceObj[DeviceDefinitionKeys::chamber] = config.chamber;
  deviceObj[DeviceDefinitionKeys::beer] = config.beer;
  deviceObj[DeviceDefinitionKeys::function] = EnumHelpers::underlyingEnumValue(config.deviceFunction);
  deviceObj[DeviceDefinitionKeys::hardware] = config.deviceHardware;
  deviceObj[DeviceDefinitionKeys::deactivated] = config.hw.deactivate;
  deviceObj[DeviceDefinitionKeys::pin] = config.hw.pinNr;


	if (value && *value) {
    deviceObj[DeviceDefinitionKeys::value] = String(value);
	}

	if (hasInvert(config.deviceHardware))
    deviceObj[DeviceDefinitionKeys::invert] = config.hw.invert;

	char buf[17];
	if (hasOnewire(config.deviceHardware)) {
		printBytes(config.hw.address, 8, buf);
    deviceObj[DeviceDefinitionKeys::address] = buf;
	}

#if BREWPI_DS2413
	if (config.deviceHardware==DEVICE_HARDWARE_ONEWIRE_2413) {
    deviceObj[DeviceDefinitionKeys::pio] = config.hw.pio;
	}
#endif

	if (config.deviceHardware==DEVICE_HARDWARE_ONEWIRE_TEMP) {
		tempDiffToString(buf, temperature(config.hw.calibration)<<(TEMP_FIXED_POINT_BITS - calibrationOffsetPrecision), 3, 8);
    deviceObj[DeviceDefinitionKeys::calibrateadjust] = buf;
	}
}


/**
 * Iterate over the defined devices.
 *
 * Caller first calls with deviceIndex 0. If the return value is true, config
 * is filled out with the config for the device. The caller can then increment
 * deviceIndex and try again.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * for (device_slot_t idx=0; deviceManager.allDevices(dc, idx); idx++) {
 *  // The "current" device info is in dc, use it somehow
 * }
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * @param config - A reference to a DeviceConfig.  This is populated with the
 * current device info.
 * @param deviceIndex - The index of the current device.
 *
 * @returns - true if the deviceIndex is valid, otherwise false.
 */
bool DeviceManager::allDevices(DeviceConfig& config, uint8_t deviceIndex)
{
  return eepromManager.fetchDevice(config, deviceIndex);
}


/**
 * \brief EnumDevicesCallback function that adds the device to a JsonDocument
 *
 * \see serializeJsonDevice
 */
void DeviceManager::outputEnumeratedDevices(DeviceConfig* config, void* pv, JsonDocument* doc)
{
	DeviceOutput* out = (DeviceOutput*)pv;
  serializeJsonDevice(*doc, out->slot, *config, out->value);
}

bool DeviceManager::enumDevice(DeviceDisplay& dd, DeviceConfig& dc, uint8_t idx)
{
	if (dd.id==-1)
		return (dd.empty || EnumHelpers::underlyingEnumValue(dc.deviceFunction));	// if enumerating all devices, honor the unused request param
	else
		return (dd.id==idx);						// enumerate only the specific device requested
}


/**
 * \brief Compare device addresses
 */
inline bool matchAddress(uint8_t* detected, uint8_t* configured, uint8_t count) {
	if (!configured[0])
		return true;
	while (count-->0) {
		if (detected[count]!=configured[count])
			return false;
	}
	return true;
}

/**
 * \brief Find a device based on it's location.
 * A device's location is:
 *   - pinNr  for simple digital pin devices
 *   - pinNr+address for one-wire devices
 *   - pinNr+address+pio for 2413
 */
device_slot_t findHardwareDevice(DeviceConfig& find)
{
	DeviceConfig config;
	for (device_slot_t slot= 0; deviceManager.allDevices(config, slot); slot++) {
		if (find.deviceHardware==config.deviceHardware) {
			bool match = true;
			switch (find.deviceHardware) {
#if BREWPI_DS2413
				case DEVICE_HARDWARE_ONEWIRE_2413:
					match &= find.hw.pio==config.hw.pio;
					// fall through
#endif
				case DEVICE_HARDWARE_ONEWIRE_TEMP:
					match &= matchAddress(find.hw.address, config.hw.address, 8);
					// fall through
				case DEVICE_HARDWARE_PIN:
					match &= find.hw.pinNr==config.hw.pinNr;
				default:	// this should not happen - if it does the device will be returned as matching.
					break;
			}
			if (match)
				return slot;
		}
	}
	return INVALID_SLOT;
}


/**
 * \brief Read a temp sensor device and convert the value into a string.
 *
 * **Warning:** the read value does not include any calibration offset.
 */
inline void DeviceManager::formatTempSensorValue(const DeviceConfig::Hardware hw, char* out)
{
#if !BREWPI_SIMULATE
  const temperature temp = readTempSensorValue(hw);
	tempToString(out, temp, 3, 9);
#else
	strcpy_P(out, PSTR("0.00"));
#endif
}

/**
 * \brief Read a temp sensor device.
 *
 * **Warning:** the read value does not include any calibration offset.
 */
temperature DeviceManager::readTempSensorValue(const DeviceConfig::Hardware hw)
{
#if !BREWPI_SIMULATE
	OneWire* bus = oneWireBus(hw.pinNr);
	OneWireTempSensor sensor(bus, hw.address, 0);		// NB: this value is uncalibrated, since we don't have the calibration offset until the device is configured
	temperature temp = INVALID_TEMP;

	if (sensor.init())
		temp = sensor.read();

  return temp;
#else
	return 0;
#endif
}


/**
 * \brief Process a found hardware device
 *
 * Used from the various enumerate* methods.
 */
void DeviceManager::handleEnumeratedDevice(DeviceConfig& config, EnumerateHardware& h, EnumDevicesCallback callback, DeviceOutput& out, JsonDocument* doc)
{
	if (h.function && !isAssignable(deviceType(DeviceFunction(h.function)), config.deviceHardware))
		return; // device not applicable for required function

	out.slot = findHardwareDevice(config);
	DEBUG_ONLY(logInfoInt(INFO_MATCHING_DEVICE, out.slot));

	if (isDefinedSlot(out.slot)) {
		if (h.unused)	// only list unused devices, and this one is already used
			return;
		// display the actual matched value
		deviceManager.allDevices(config, out.slot);
	}

	out.value[0] = 0;
	if (h.values) {
		switch (config.deviceHardware) {
			case DEVICE_HARDWARE_ONEWIRE_TEMP:
				formatTempSensorValue(config.hw, out.value);
				break;
      // unassigned pins could be input or output so we can't determine any
      // other details from here.  values can be read once the pin has been
      // assigned a function
			default:
				break;
		}
	}

	callback(&config, &out, doc);
}


/**
 * \brief Enumerate the "pin" devices.
 *
 * Pin devices are those that are attached directly to a pin, not on a bus like OneWire
 */
void DeviceManager::enumeratePinDevices(EnumerateHardware& h, EnumDevicesCallback callback, DeviceOutput& output, JsonDocument* doc)
{
	DeviceConfig config;
	clear((uint8_t*)&config, sizeof(config));
	config.deviceHardware = DEVICE_HARDWARE_PIN;
	config.chamber = 1; // chamber 1 is default

	int8_t pin;
	for (uint8_t count=0; (pin=deviceManager.enumerateActuatorPins(count))>=0; count++) {
		if (h.pin!=-1 && h.pin!=pin)
			continue;
		config.hw.pinNr = pin;
		config.hw.invert = true; // make inverted default, because shiels have transistor on them
		handleEnumeratedDevice(config, h, callback, output, doc);
	}

	for (uint8_t count=0; (pin=deviceManager.enumerateSensorPins(count))>=0; count++) {
		if (h.pin!=-1 && h.pin!=pin)
			continue;
		config.hw.pinNr = pin;
		handleEnumeratedDevice(config, h, callback, output, doc);
	}
}


/**
 * \brief Enumerate all OneWire devices
 *
 * \param h - Hardware spec, used to filter sensors
 * \param callback - Callback function, called for every found hardware device
 * \param output -
 * \param doc - JsonDocument to populate
 */
void DeviceManager::enumerateOneWireDevices(EnumerateHardware& h, EnumDevicesCallback callback, DeviceOutput& output, JsonDocument* doc)
{
#if !BREWPI_SIMULATE
	int8_t pin;
	for (uint8_t count=0; (pin=deviceManager.enumOneWirePins(count))>=0; count++) {
		DeviceConfig config;
		clear((uint8_t*)&config, sizeof(config));
		if (h.pin!=-1 && h.pin!=pin)
			continue;

		config.hw.pinNr = pin;
		config.chamber = 1; // chamber 1 is default
		OneWire* wire = oneWireBus(pin);

		if (wire != nullptr) {
			wire->reset_search();
			while (wire->search(config.hw.address)) {
				// hardware device type from OneWire family ID
				switch (config.hw.address[0]) {
		#if BREWPI_DS2413
					case DS2413_FAMILY_ID:
						config.deviceHardware = DEVICE_HARDWARE_ONEWIRE_2413;
						break;
		#endif
					case DS18B20MODEL:
						config.deviceHardware = DEVICE_HARDWARE_ONEWIRE_TEMP;
						break;
					default:
						config.deviceHardware = DEVICE_HARDWARE_NONE;
				}

				switch (config.deviceHardware) {
		#if BREWPI_DS2413
					// for 2408 this will require iterating 0..7
					case DEVICE_HARDWARE_ONEWIRE_2413:
						// enumerate each pin separately
						for (uint8_t i=0; i<2; i++) {
							config.hw.pio = i;
							handleEnumeratedDevice(config, h, callback, output, doc);
						}
						break;
		#endif
					case DEVICE_HARDWARE_ONEWIRE_TEMP:
		#if !ONEWIRE_PARASITE_SUPPORT
						{	// check that device is not parasite powered
							DallasTemperature sensor(wire);
							if(sensor.initConnection(config.hw.address)){
								handleEnumeratedDevice(config, h, callback, output, doc);
							}
						}
		#else
						handleEnumeratedDevice(config, h, callback, output, doc);
		#endif
						break;
					default:
						handleEnumeratedDevice(config, h, callback, output, doc);
				}
			}
		}
	}
#endif
}


/**
 * \brief Read hardware spec from stream and output matching devices
 */
void DeviceManager::enumerateHardware(JsonDocument& doc)
{
	EnumerateHardware spec;
	// set up defaults
	spec.unused = 0;			// list all devices
	spec.values = 0;			// don't list values
	spec.pin = -1;				// any pin
	spec.hardware = -1;		// any hardware
	spec.function = 0;		// no function restriction

  readJsonIntoHardwareSpec(spec);
	DeviceOutput out;

  // Initialize the document as an array
  doc.to<JsonArray>();

	if (spec.hardware==-1 || isOneWire(DeviceHardware(spec.hardware))) {
		enumerateOneWireDevices(spec, outputEnumeratedDevices, out, &doc);
	}
	if (spec.hardware==-1 || isDigitalPin(DeviceHardware(spec.hardware))) {
		enumeratePinDevices(spec, outputEnumeratedDevices, out, &doc);
	}
}

/**
 * \brief Parse JSON into a DeviceDisplay struct
 */
void DeviceManager::readJsonIntoDeviceDisplay(DeviceDisplay& dev) {
  StaticJsonDocument<128> doc;
  piLink.receiveJsonMessage(doc);

  JsonVariant id = doc[DeviceDisplayKeys::index];
  if(!id.isNull())
    dev.id = id.as<int8_t>();

  JsonVariant value = doc[DeviceDisplayKeys::value];
  if(!value.isNull())
    dev.value = value.as<int8_t>();

  JsonVariant write = doc[DeviceDisplayKeys::write];
  if(!write.isNull())
    dev.write = write.as<int8_t>();

  JsonVariant empty = doc[DeviceDisplayKeys::empty];
  if(!empty.isNull())
    dev.empty = empty.as<int8_t>();
}


/**
 * \brief Parse JSON into an EnumerateHardware struct
 */
void DeviceManager::readJsonIntoHardwareSpec(EnumerateHardware& hw) {
  StaticJsonDocument<128> doc;
  piLink.receiveJsonMessage(doc);

  JsonVariant hardware = doc[EnumerateHardwareKeys::hardware];
  if(!hardware.isNull())
    hw.hardware = hardware.as<int8_t>();

  JsonVariant pin = doc[EnumerateHardwareKeys::pin];
  if(!pin.isNull())
    hw.pin = pin.as<int8_t>();

  JsonVariant values = doc[EnumerateHardwareKeys::values];
  if(!values.isNull())
    hw.values = values.as<int8_t>();

  JsonVariant unused = doc[EnumerateHardwareKeys::unused];
  if(!unused.isNull())
    hw.unused = unused.as<int8_t>();

  JsonVariant function = doc[EnumerateHardwareKeys::function];
  if(!function.isNull())
    hw.function = function.as<int8_t>();
}


void UpdateDeviceState(DeviceDisplay& dd, DeviceConfig& dc, char* val)
{
	DeviceType dt = deviceType(dc.deviceFunction);
	if (dt==DEVICETYPE_NONE)
		return;

	void** ppv = deviceTarget(dc);
	if (ppv==NULL)
		return;

	if (dd.write>=0 && dt==DEVICETYPE_SWITCH_ACTUATOR) {
		// write value to a specific device. For now, only actuators are relevant targets
		DEBUG_ONLY(logInfoInt(INFO_SETTING_ACTIVATOR_STATE, dd.write!=0));
		((Actuator*)*ppv)->setActive(dd.write!=0);
	}
	else if (dd.value==1) {		// read values
		if (dt==DEVICETYPE_SWITCH_SENSOR) {
			sprintf_P(val, STR_FMT_U, (unsigned int) ((SwitchSensor*)*ppv)->sense()!=0); // cheaper than itoa, because it overlaps with vsnprintf
		}
		else if (dt==DEVICETYPE_TEMP_SENSOR) {
			BasicTempSensor& s = unwrapSensor(dc.deviceFunction, *ppv);
			temperature temp = s.read();
			tempToString(val, temp, 3, 9);
		}
		else if (dt==DEVICETYPE_SWITCH_ACTUATOR) {
			sprintf_P(val, STR_FMT_U, (unsigned int) ((Actuator*)*ppv)->isActive()!=0);
		}
	}
}

/**
 * \brief Print list of hardware devices
 * \param doc - JsonDocument to add results to
 */
void DeviceManager::listDevices(JsonDocument& doc) {
	DeviceConfig dc;
	DeviceDisplay dd;
	fill((int8_t*)&dd, sizeof(dd));
	dd.empty = 0;

  readJsonIntoDeviceDisplay(dd);

	if (dd.id==-2) {
		if (dd.write>=0)
			tempControl.cameraLight.setActive(dd.write!=0);

		return;
	}

	for (device_slot_t idx=0; deviceManager.allDevices(dc, idx); idx++) {
		if (deviceManager.enumDevice(dd, dc, idx))
		{
			char val[10];
			val[0] = 0;
			UpdateDeviceState(dd, dc, val);
			serializeJsonDevice(doc, idx, dc, val);
		}
	}
}


/**
 * \brief Print the raw temp readings from all temp sensors.
 *
 * Allows logging temps that aren't part of the control logic.
 * \param doc - JsonDocument to add results to
 */
void DeviceManager::rawDeviceValues(JsonDocument& doc) {
	EnumerateHardware spec;
	// set up defaults
	spec.unused = 0;			// list all devices
	spec.values = 0;			// don't list values
	spec.pin = -1;				// any pin
	spec.hardware = -1;		// any hardware
	spec.function = 0;		// no function restriction

	DeviceOutput out;

  enumerateOneWireDevices(spec, outputRawDeviceValue, out, &doc);
}


/**
 * \brief Print the sensor's information & current reading.
 */
void DeviceManager::outputRawDeviceValue(DeviceConfig* config, void* pv, JsonDocument* doc)
{
  if(config->deviceHardware == DeviceHardware::DEVICE_HARDWARE_ONEWIRE_TEMP) {
    // Read the temp
    const temperature temp = DeviceManager::readTempSensorValue(config->hw);

    // Pretty-print the address
    char devName[17];
    printBytes(config->hw.address, 8, devName);

    String humanName = DeviceNameManager::getDeviceName(devName);

    JsonObject deviceObj = doc->createNestedObject();
    deviceObj["device"] = devName;
    deviceObj["value"] = tempToDouble(temp, Config::TempFormat::fixedPointDecimals);
    deviceObj["name"] = humanName;
  }
}

/**
 * \brief Determines where devices belongs, based on its function.
 *
 * @param id - Device Function
 */
DeviceOwner deviceOwner(const DeviceFunction id) {
  if(id == DeviceFunction::none)
    return DeviceOwner::none;

  if(id >= DeviceFunction::beerFirst)
    return DeviceOwner::beer;

  return DeviceOwner::chamber;
}

/**
 * Determines the class of device for the given DeviceID.
 */
DeviceType deviceType(const DeviceFunction id) {
	switch (id) {
	case DeviceFunction::chamberDoor:
		return DEVICETYPE_SWITCH_SENSOR;

	case DeviceFunction::chamberHeat:
	case DeviceFunction::chamberCool:
	case DeviceFunction::chamberLight:
	case DeviceFunction::chamberFan:
	case DeviceFunction::beerHeat:
	case DeviceFunction::beerCool:
		return DEVICETYPE_SWITCH_ACTUATOR;

	case DeviceFunction::chamberTemp:
	case DeviceFunction::chamberRoomTemp:
	case DeviceFunction::beerTemp:
	case DeviceFunction::beerTemp2:
		return DEVICETYPE_TEMP_SENSOR;

	default:
		return DEVICETYPE_NONE;
	}
}

DeviceManager deviceManager;
