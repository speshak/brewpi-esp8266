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

#include "SettingLoader.h"
#include "Display.h"
#include "PiLink.h"
#include "TempControl.h"

/**
 * \brief Process a single setting key/value pair
 *
 * \param kv - The parsed JsonPair of the setting
 */
void SettingLoader::processSettingKeypair(JsonPair kv) {
  if (kv.key() == "mode") {
    tempControl.setMode(kv.value().as<char *>()[0]);
  }

  else if (kv.key() == "beerSet") {
    setBeerSetting(kv.value().as<char *>());
  }

  else if (kv.key() == "fridgeSet") {
    setFridgeSetting(kv.value().as<char *>());
  }

  else if (kv.key() == "heatEst") {
    tempControl.cs.heatEstimator = stringToFixedPoint(kv.value().as<char *>());
  }

  else if (kv.key() == "coolEst") {
    tempControl.cs.coolEstimator = stringToFixedPoint(kv.value().as<char *>());
  }

  else if (kv.key() == "tempFormat") {
    tempControl.cc.tempFormat = kv.value().as<char *>()[0];
    display.printStationaryText(); // reprint stationary text to update to right
                                   // degree unit
  }

  else if (kv.key() == "tempSetMin") {
    tempControl.cc.tempSettingMin = stringToTemp(kv.value().as<char *>());
  }

  else if (kv.key() == "tempSetMax") {
    tempControl.cc.tempSettingMax = stringToTemp(kv.value().as<char *>());
  }

  else if (kv.key() == "pidMax") {
    tempControl.cc.pidMax = stringToTempDiff(kv.value().as<char *>());
  }

  else if (kv.key() == "Kp") {
    tempControl.cc.Kp = stringToFixedPoint(kv.value().as<char *>());
  }

  else if (kv.key() == "Ki") {
    tempControl.cc.Ki = stringToFixedPoint(kv.value().as<char *>());
  }

  else if (kv.key() == "Kd") {
    tempControl.cc.Kd = stringToFixedPoint(kv.value().as<char *>());
  }

  else if (kv.key() == "iMaxErr") {
    tempControl.cc.iMaxError = stringToTempDiff(kv.value().as<char *>());
  }

  else if (kv.key() == "idleRangeH") {
    tempControl.cc.idleRangeHigh = stringToTempDiff(kv.value().as<char *>());
  }

  else if (kv.key() == "idleRangeL") {
    tempControl.cc.idleRangeLow = stringToTempDiff(kv.value().as<char *>());
  }

  else if (kv.key() == "heatTargetH") {
    tempControl.cc.heatingTargetUpper =
        stringToTempDiff(kv.value().as<char *>());
  }

  else if (kv.key() == "heatTargetL") {
    tempControl.cc.heatingTargetLower =
        stringToTempDiff(kv.value().as<char *>());
  }

  else if (kv.key() == "coolTargetH") {
    tempControl.cc.coolingTargetUpper =
        stringToTempDiff(kv.value().as<char *>());
  }

  else if (kv.key() == "coolTargetL") {
    tempControl.cc.coolingTargetLower =
        stringToTempDiff(kv.value().as<char *>());
  }

  else if (kv.key() == "maxHeatTimeForEst") {
    tempControl.cc.maxHeatTimeForEstimate = kv.value().as<uint16_t>();
  }

  else if (kv.key() == "maxCoolTimeForEst") {
    tempControl.cc.maxCoolTimeForEstimate = kv.value().as<uint16_t>();
  }

  else if (kv.key() == "fridgeFastFilt") {
    tempControl.fridgeSensor->setFastFilterCoefficients(
        kv.value().as<uint8_t>());
  }

  else if (kv.key() == "fridgeSlowFilt") {
    tempControl.fridgeSensor->setSlowFilterCoefficients(
        kv.value().as<uint8_t>());
  }

  else if (kv.key() == "fridgeSlopeFilt") {
    tempControl.fridgeSensor->setSlopeFilterCoefficients(
        kv.value().as<uint8_t>());
  }

  else if (kv.key() == "beerFastFilt") {
    tempControl.beerSensor->setFastFilterCoefficients(kv.value().as<uint8_t>());
  }

  else if (kv.key() == "beerSlowFilt") {
    tempControl.beerSensor->setSlowFilterCoefficients(kv.value().as<uint8_t>());
  }

  else if (kv.key() == "beerSlopeFilt") {
    tempControl.beerSensor->setSlopeFilterCoefficients(
        kv.value().as<uint8_t>());
  }

  else if (kv.key() == "lah") {
    tempControl.cc.lightAsHeater = kv.value().as<bool>();
  }

  else if (kv.key() == "hs") {
    tempControl.cc.rotaryHalfSteps = kv.value().as<bool>();
  }
}

/**
 * Set the target beer temperature
 * @param val - New temp value
 */
void SettingLoader::setBeerSetting(const char *val) {
  String annotation = "Beer temp set to ";
  annotation += val;

  temperature newTemp = stringToTemp(val);

  if (tempControl.cs.mode == 'p') {
    // this excludes gradual updates under 0.2 degrees
    if (abs(newTemp - tempControl.cs.beerSetting) > 100) {
      annotation += " by temperature profile";
    }
  } else {
    annotation += " in web interface";
  }

  if (annotation.length() > 0)
    piLink.sendStateNotification(annotation.c_str());

  tempControl.setBeerTemp(newTemp);
}

/**
 * Set the target fridge temperature
 * @param val - New temp value
 */
void SettingLoader::setFridgeSetting(const char *val) {
  temperature newTemp = stringToTemp(val);
  if (tempControl.cs.mode == 'f') {
    String annotation = "Fridge temp set to ";
    annotation += val;
    annotation += " in web interface";

    piLink.sendStateNotification(nullptr, annotation.c_str());
  }

  tempControl.setFridgeTemp(newTemp);
}
