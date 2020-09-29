/*
 * Copyright 2012-2013 BrewPi/Elco Jacobs.
 * Copyright 2013 Matthew McGowan.
 * Copyright 2020 Scott Peshak
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

#include "CommandProcessor.h"
#include "Brewpi.h"
#include "Display.h"
#include "PiLink.h"
#include "SettingLoader.h"
#include "SettingsManager.h"
#include "TempControl.h"
#include "Version.h"
#include <ArduinoJson.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

extern void handleReset();

/**
 * \brief Receive incoming commands
 *
 * Continuously reads data from PiStream and processes the command strings.
 */
void CommandProcessor::receiveCommand() {
  while (piLink.available() > 0) {
    char inByte = piLink.read();

    // Clamp the command (roughly) to the printable ASCII range
    // This cuts down the number of cases needed in the switch and silence
    // noise caused by telnet control characters.  Command values within the
    // range will cause a message to be returned (see the default case of the
    // switch)
    if (inByte < 65 || inByte > 122) {
      continue;
    }

    switch (inByte) {
#if BREWPI_SIMULATE == 1
    case 'y':
      parseJson(HandleSimulatorConfig);
      break;
    case 'Y':
      printSimulatorSettings();
      break;
#endif
    case 'A': // alarm on
      setAlarmState(true);
      break;
    case 'a': // alarm off
      setAlarmState(false);
      break;

    case 't': // temperatures requested
      printTemperatures();
      break;
    case 'T': // All temps
      printRawTemperatures();
      break;
    case 'o': // Configure probe names
      setDeviceNames();
      break;
    case 'p': // Get configured probe names
      printDeviceNames();
      break;
    case 'C': // Set default constants
      setDefaultConstants();
      break;
    case 'S': // Set default settings
      setDefaultSettings();
      break;
    case 's': // Control settings requested
      sendControlSettings();
      break;
    case 'c': // Control constants requested
      sendControlConstants();
      break;
    case 'v': // Control variables requested
      sendControlVariables();
      break;
    case 'n':
      versionInfo();
      break;
    case 'l': // Display content requested
      getLcdContent();
      break;
    case 'j': // Receive settings as json
      processSettingsJson();
      break;

    case 'E': // initialize eeprom
      initEeprom();
      break;

    case 'd': // list devices in eeprom order
      listDevices();
      break;

    case 'U': // update device
      deviceManager.parseDeviceDefinition();
      break;

    case 'h': // hardware query
      listHardware();
      break;

#if defined(ESP8266) || defined(ESP32)
    case 'w': // Reset WiFi settings
      resetWiFi();
      break;

    case 'b': // Toggle Backlight (since we don't have a rotary encoder)
      toggleBacklight();
      break;
#endif

#if (BREWPI_DEBUG > 0)
    case 'Z': // zap eeprom
      zapEeprom();
      break;
#endif

    case 'R': // reset
      handleReset();
      break;

    default:
      invalidCommand(inByte);
    }
  }
}

/**
 * \brief Display a warning about unknown commands
 * \param inByte - The recieved command
 */
void CommandProcessor::invalidCommand(char inByte) {
  piLink.print_P(PSTR("Invalid command received \"%c\" (0x%02X)"), inByte, inByte);
  piLink.printNewLine();
}

/**
 * \brief Version information
 *
 * Sends a string with software & hardware version information.
 * \ingroup commands
 */
void CommandProcessor::versionInfo() {
  StaticJsonDocument<256> doc;
  // v version
  // s shield type
  // y: simulator
  // b: board

  doc["v"] = Config::Version::release;
  doc["n"] = Config::Version::git_rev;
  doc["c"] = Config::Version::git_tag;
  doc["s"] = BREWPI_STATIC_CONFIG;
  doc["y"] = BREWPI_SIMULATE;
  doc["b"] = String(BREWPI_BOARD);
  doc["l"] = BREWPI_LOG_MESSAGES_VERSION;
  doc["f"] = Config::Feature::flagString;

  piLink.sendJsonMessage('N', doc);
}

/**
 * \brief Reset control constants to their default values.
 *
 * \ingroup commands
 */
void CommandProcessor::setDefaultConstants() {
  tempControl.loadDefaultConstants();
  display.printStationaryText(); // reprint stationary text to update to right
                                 // degree unit
  sendControlConstants();        // update script with new settings
  logInfo(INFO_DEFAULT_CONSTANTS_LOADED);
}

/**
 * \brief Reset settings to their default values.
 *
 * \ingroup commands
 */
void CommandProcessor::setDefaultSettings() {
  tempControl.loadDefaultSettings();
  sendControlSettings(); // update script with new settings
  logInfo(INFO_DEFAULT_SETTINGS_LOADED);
}

/**
 * \brief Enable/disable the alarm buzzer.
 *
 * \ingroup commands
 */
void CommandProcessor::setAlarmState(bool enabled) { alarm_actuator.setActive(enabled); }

/**
 * \brief List devices that have been installed.
 *
 * Installed devices are devices that have been mapped to a control function.
 * \ingroup commands
 */
void CommandProcessor::listDevices() {
  piLink.openListResponse('d');
  deviceManager.listDevices();
  piLink.closeListResponse();
}

/**
 * \brief List all hardware devices.
 *
 * \ingroup commands
 */
void CommandProcessor::listHardware() {
  piLink.openListResponse('h');
  deviceManager.enumerateHardware();
  piLink.closeListResponse();
}

/**
 * \brief Reset the WiFi configuration
 *
 * \ingroup commands
 */
void CommandProcessor::resetWiFi() { WiFi.disconnect(true); }

/**
 * \brief Toggle the state of the LCD backlight
 *
 * \ingroup commands
 */
void CommandProcessor::toggleBacklight() { ::toggleBacklight = !::toggleBacklight; }

/**
 * \brief Get what is currently displayed on the LCD
 *
 * \ingroup commands
 */
void CommandProcessor::getLcdContent() {
  StaticJsonDocument<256> doc;
  JsonArray rootArray = doc.to<JsonArray>();
  char stringBuffer[Config::Lcd::columns + 1];

  for (uint8_t i = 0; i < Config::Lcd::lines; i++) {
    display.getLine(i, stringBuffer);
    rootArray.add(stringBuffer);
  }
  piLink.sendJsonMessage('L', doc);
}

/**
 * \brief Send the current temperatures
 *
 * \ingroup commands
 */
void CommandProcessor::printTemperatures() {
  // print all temperatures with empty annotations
  piLink.printTemperatures(0, 0);
}

/**
 * \brief Send the current temperatures
 *
 * \ingroup commands
 */
void CommandProcessor::printRawTemperatures() { deviceManager.rawDeviceValues(); }

/**
 * \brief Erase EEPROM data
 *
 * \see EepromManager::zapEeprom()
 */
void CommandProcessor::zapEeprom() {
  eepromManager.zapEeprom();
  logInfo(INFO_EEPROM_ZAPPED);
}

/**
 * \brief Initialize EEPROM data
 *
 * \see EepromManager::initializeEeprom()
 */
void CommandProcessor::initEeprom() {
  eepromManager.initializeEeprom();
  logInfo(INFO_EEPROM_INITIALIZED);
  settingsManager.loadSettings();
}

/**
 * \brief Print out the configured device names
 */
void CommandProcessor::printDeviceNames() {
  StaticJsonDocument<256> doc;
  DeviceNameManager::enumerateDeviceNames(doc);
  piLink.sendJsonMessage('N', doc);
}

/**
 * \brief Process incoming settings
 */
void CommandProcessor::processSettingsJson() {
  StaticJsonDocument<256> doc;
  piLink.receiveJsonMessage(doc);
  // Echo the settings back, for testing
  piLink.sendJsonMessage('D', doc);

  // Process
  JsonObject root = doc.as<JsonObject>();
  for (JsonPair kv : root) {
    SettingLoader::processSettingKeypair(kv);
  }

  // Save the settings
  eepromManager.storeTempConstantsAndSettings();

  // Inform the other end of the new state of affairs
  sendControlSettings();
  sendControlConstants();
}

/**
 * \brief Set device names
 *
 * @see DeviceNameManager::setDeviceName
 */
void CommandProcessor::setDeviceNames() {
  StaticJsonDocument<256> doc;
  piLink.receiveJsonMessage(doc);

  JsonObject root = doc.as<JsonObject>();
  for (JsonPair kv : root) {
    DeviceNameManager::setDeviceName(kv.key().c_str(), kv.value().as<char *>());
  }
}

/**
 * \brief Send control settings as JSON string
 */
void CommandProcessor::sendControlSettings() {
  StaticJsonDocument<256> doc;
  tempControl.getControlSettingsDoc(doc);
  piLink.sendJsonMessage('S', doc);
}

/**
 * \brief Send control constants as JSON string.
 * Might contain spaces between minus sign and number. Python will have to strip
 * these
 */
void CommandProcessor::sendControlConstants() {
  StaticJsonDocument<256> doc;
  tempControl.getControlConstantsDoc(doc);
  piLink.sendJsonMessage('C', doc);
}

/**
 * \brief Send all control variables.
 *
 * Useful for debugging and choosing parameters
 */
void CommandProcessor::sendControlVariables() {
  StaticJsonDocument<256> doc;
  tempControl.getControlVariablesDoc(doc);
  piLink.sendJsonMessage('V', doc);
}
