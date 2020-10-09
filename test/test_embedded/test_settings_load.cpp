#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "SettingLoader.h"
#include "TempControl.h"
#include "TemperatureFormats.h"

#include "temp_test.h"

// Test document strings
const String pidDoc("{Kp: 0.98, Ki: 3.42, Kd: 5.01}");

/**
 * Process a JSON string through the settings loader
 */
void process_doc(const String str) {
  StaticJsonDocument<256> doc;
  deserializeJson(doc, str);

  JsonObject root = doc.as<JsonObject>();
  for (JsonPair kv : root) {
    SettingLoader::processSettingKeypair(kv);
  }
}

void test_pid_settings() {
  process_doc(pidDoc);
  TEST_ASSERT_EQUAL_TEMPERATURE(stringToTemp("0.98"), tempControl.cc.Kp);
  TEST_ASSERT_EQUAL_TEMPERATURE(stringToTemp("3.42"), tempControl.cc.Ki);
  TEST_ASSERT_EQUAL_TEMPERATURE(stringToTemp("5.01"), tempControl.cc.Kd);
}

void test_lah() {
  process_doc(String("{lah: 0}"));
  TEST_ASSERT_FALSE(tempControl.cc.lightAsHeater);

  process_doc(String("{lah: 1}"));
  TEST_ASSERT_TRUE(tempControl.cc.lightAsHeater);
}

void test_hs() {
  process_doc(String("{hs: 0}"));
  TEST_ASSERT_FALSE(tempControl.cc.rotaryHalfSteps);

  process_doc(String("{hs: 1}"));
  TEST_ASSERT_TRUE(tempControl.cc.rotaryHalfSteps);
}

void test_mode() {
  process_doc(String("{mode: \"f\"}"));
  TEST_ASSERT_EQUAL(ControlMode::fridgeConstant, tempControl.cs.mode);

  process_doc(String("{mode: \"F\"}"));
  TEST_ASSERT_EQUAL(ControlMode::fridgeProfile, tempControl.cs.mode);

  process_doc(String("{mode: \"b\"}"));
  TEST_ASSERT_EQUAL(ControlMode::beerConstant, tempControl.cs.mode);

  process_doc(String("{mode: \"p\"}"));
  TEST_ASSERT_EQUAL(ControlMode::beerProfile, tempControl.cs.mode);

  process_doc(String("{mode: \"o\"}"));
  TEST_ASSERT_EQUAL(ControlMode::off, tempControl.cs.mode);

  process_doc(String("{mode: \"t\"}"));
  TEST_ASSERT_EQUAL(ControlMode::test, tempControl.cs.mode);
}

void test_misc_values() {
  process_doc(String("{maxHeatTimeForEst: 200, maxCoolTimeForEst: 300}"));

  TEST_ASSERT_EQUAL(200, tempControl.cc.maxHeatTimeForEstimate);
  TEST_ASSERT_EQUAL(300, tempControl.cc.maxCoolTimeForEstimate);
}

void test_filter_coefficents() {
  // The filter pointers need to be initialized
  TempControl::init();

  process_doc(String("{fridgeFastFilt: 200, fridgeSlowFilt: 300, fridgeSlopeFilt: 500}"));

  // There isn't an accessor to get into these parameters.  Need to think about
  // if adding them is worth the effort just to test.  I'm leaving the test as
  // is, because at the very least this means that we're exercising that code
  // path, even if we can't then check the result.
}

/* Run the test suite */
void test_settings_load() {
  RUN_TEST(test_pid_settings);
  RUN_TEST(test_mode);
  RUN_TEST(test_lah);
  RUN_TEST(test_hs);
  RUN_TEST(test_misc_values);
  RUN_TEST(test_filter_coefficents);
}
