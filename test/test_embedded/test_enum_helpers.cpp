#include <Arduino.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "test_enum_helpers.h"

void test_enum_value(const char *jsonStr, const DummyEnum expect) {
  StaticJsonDocument<256> doc;
  deserializeJson(doc, jsonStr);

  DummyEnum enumVal;

  JsonObject root = doc.as<JsonObject>();
  for (JsonPair kv : root) {
    EnumHelpers::readEnumValue(kv, enumVal);
    TEST_ASSERT_EQUAL(expect, enumVal);
  }
}

void test_string_value() { test_enum_value("{a: \"b\"}", DummyEnum::blue); }

void test_int_value() { test_enum_value("{a: \"1\"}", DummyEnum::one); }

void test_enum_helpers() {
  RUN_TEST(test_string_value);
  RUN_TEST(test_int_value);
}
