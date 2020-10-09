#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <unity.h>

#include "test_devicenamemanager.h"
#include "test_enum_helpers.h"
#include "test_logger.h"
#include "test_settings_load.h"

void setup() {
  // NOTE!!! Wait for >2 secs
  // if board doesn't support software reset via Serial.DTR/RTS
  delay(2000);

  SPIFFSConfig cfg;
  cfg.setAutoFormat(true);
  SPIFFS.setConfig(cfg);
  SPIFFS.begin();
  SPIFFS.format();

  UNITY_BEGIN();
  test_enum_helpers();
  test_settings_load();
  test_devicenamemanager();
  test_logger();
  UNITY_END();
}

void loop() {}
